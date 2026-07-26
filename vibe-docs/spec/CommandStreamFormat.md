# §9 BECS 命令流格式

> 本文是 BECS（Barri-Eww Command Stream）模块容器、typed handle tables 与跨语言 ABI
> 的规范性定义。它落实 ADR-0002 的 D1/D2/D5/D6；opcode catalog 与总体执行模型仍以
> `vibe-docs/adr/ADR-0002-CommandStreamFormat.md` 和 ADR-0003 为架构依据。

---

## §9.1 共通 ABI 规则

- 所有表字段固定为 little-endian、自然对齐的定宽整数；表和记录整体保持 8-byte 对齐。
- BECS 中不得携带 native pointer、Vulkan handle 或宿主对象句柄。资源之间只能以 typed
  `u32` slot 间接引用。
- 每张 handle table 都是独立模块 section；不同资源类型不得复用或扩展另一个表的 entry
  layout。
- Java writer 在 bake path 对能独立判断的 authoring errors fail-fast；Native validator
  在 load path 验证完整的单表结构；依赖两张表或具体 backend 的规则在 materialization
  时验证。
- Native materialization 成功后必须把资源保存为 dense slot-indexed native array。回放热
  路径不得重新解析 BECS、创建资源、进行 hash lookup 或 lazy materialization。

---

## §9.2 ImageViewHandleTable v0.1

`ImageViewHandleTable` 的 section type 为 `0x0004`。它描述从一个
`ImageHandleTable` source slot 派生的静态 image view；它不携带 `VkImageView` 或其他
backend handle。

### §9.2.1 Table layout

```text
ImageViewHandleTable:
  +0  entryCount          u32
  +4  reservedFlags       u32  // must be zero

ImageViewHandleTableEntry (32 bytes):
  +0  imageSlot           u32
  +4  imageViewKindValue  u32
  +8  formatValue         u32
  +12 aspectMaskValue     u32
  +16 baseMipLevel        u32
  +20 mipLevelCount       u32
  +24 baseArrayLayer      u32
  +28 arrayLayerCount     u32
```

The exact section size is `8 + entryCount * 32` bytes. The header and every entry are
part of the stable v0.1 ABI.

### §9.2.2 Field rules

- `imageSlot` identifies a slot in the module ImageHandleTable. Its existence and
  bind state are cross-table rules, not self-table validation rules.
- `imageViewKindValue` is one of `1 = OneDimensional`, `2 = TwoDimensional`, or
  `3 = ThreeDimensional`.
- `formatValue` is an assigned neutral image format.
- `aspectMaskValue` is a non-empty subset of Color (`0x1`), Depth (`0x2`) and Stencil
  (`0x4`).
- `baseMipLevel` and `baseArrayLayer` are explicit offsets. No remaining-range sentinel
  is defined or accepted by v0.1.
- `mipLevelCount` is explicit and greater than zero.
- `arrayLayerCount` is explicitly present for future additive ABI growth, but must be
  exactly one in v0.1. v0.1 has no array/cube view kind and must not infer an array
  view type from a layer count.

### §9.2.3 Native load-time materialization

A backend materializer receives only a validated ImageViewHandleTable view and a
materialized source image table. Before creating a native view it must validate:

1. the source image slot is in range and bound;
2. v0.1 format equality and compatible aspect semantics with the source image format;
3. an image-view kind compatible with the source image kind;
4. mip and layer ranges using overflow-safe bounds checks;
5. source usage contains at least one view-compatible usage: Sampled, Storage,
   ColorAttachment or DepthStencilAttachment.

For Vulkan, the materializer maps a validated entry to explicit
`VkImageViewCreateInfo` fields, creates a `VkImageView` on the load path and owns it
through RAII. It must destroy every view before destruction of the source
`VulkanImageTable`. An unbound imported source image is a stable materialization error;
this version does not define imported-image host binding.

### §9.2.4 v0.1 exclusions

The following are deliberately not part of this version: format reinterpretation,
component swizzle, array/cube views, `VK_REMAINING_*` sentinels, graphics command
consumers, rendering/descriptor templates and imported-image host binding. Additions
must preserve existing field values and follow the command-stream minor-version policy.

---

## §9.3 Lane stream 与命令头

A lane stream is the flat, straight-line command tape a compiler emits for one
recording lane (ADR-0002 D3). The version pair is shared across the whole format family
(`versionMajor = 0`, `versionMinor = 1` in v0.1).

```text
LaneStreamHeader (32 bytes):
  +0  magicBytes[4]   u8   // "BECS" = {0x42,0x45,0x43,0x53}
  +4  versionMajor    u16
  +6  versionMinor    u16
  +8  laneIndex       u32
  +12 commandCount    u32
  +16 totalByteSize   u64
  +24 graphHash       u64

CommandHeader (8 bytes, precedes every command payload):
  +0  opcode          u16   // §9.11 catalog
  +2  reservedFlags   u16   // must be zero
  +4  byteSize        u32   // whole command incl. this header; multiple of 8
```

Rules: `totalByteSize` equals the byte range; the command walk visits exactly
`commandCount` commands and ends exactly at `totalByteSize`; each `byteSize` is at least
8 and a multiple of 8; `reservedFlags` is zero; `opcode` is an assigned catalog value.
The stream carries no handles — commands reference resources by slot (§9.11).

---

## §9.4 Module container (BECM)

A module is the bake artifact handed to the native side once per graph: a header, a
section directory, then the section byte ranges (ADR-0002 D2). Handle/barrier/template
tables and per-lane streams are all sections.

```text
ModuleHeader (32 bytes):
  +0  magicBytes[4]    u8   // "BECM" = {0x42,0x45,0x43,0x4D}
  +4  versionMajor     u16
  +6  versionMinor     u16
  +8  sectionCount     u32
  +12 laneStreamCount  u32
  +16 totalByteSize    u64
  +24 graphHash        u64

SectionDirectoryEntry (24 bytes):
  +0  sectionTypeValue u16   // §9.4 section-type table
  +2  reservedFlags    u16   // must be zero
  +4  sectionIndex     u32   // lane index for LaneStream; 0 for singletons
  +8  byteOffset       u64   // from module base; 8-byte aligned
  +16 byteSize         u64
```

Section types: `0x0001 PipelineHandleTable`, `0x0002 BufferHandleTable`,
`0x0003 ImageHandleTable`, `0x0004 ImageViewHandleTable`, `0x0005 SamplerHandleTable`
(reserved), `0x0006 ShaderModuleTable`, `0x0007 GraphicsPipelineTable` (§9.14),
`0x0008 GraphOutputTable` (§9.17), `0x0010 BarrierBatchTable`,
`0x0011 RenderingTemplateTable`, `0x0012 PushDescriptorTemplateTable` (reserved),
`0x0020 LaneStream`.

Rules: sections lie inside the module and outside the directory, are 8-byte aligned, and
do not overlap; no two directory entries share a `(sectionTypeValue, sectionIndex)`
identity; `laneStreamCount` equals the number of `LaneStream` sections and their
`sectionIndex` values are exactly `0..laneStreamCount-1`; each embedded lane stream
independently validates (§9.3) and its `graphHash` equals the module's.

---

## §9.5 BufferHandleTable v0.1

Section type `0x0002`. Header `{entryCount u32, reservedFlags u32}` then fixed 24-byte
entries; exact size `8 + entryCount * 24`.

```text
BufferHandleTableEntry (24 bytes):
  +0  byteSize          u64
  +8  usageFlags        u32   // OR-mask of §9.12 buffer usage bits
  +12 memoryKindValue   u32   // §9.12 buffer memory kind
  +16 importIdentifier  u32   // 0 = created; non-zero = imported
  +20 reservedFlags     u32   // must be zero
```

Rules: a CREATED entry (`importIdentifier == 0`) has a positive `byteSize`, a non-empty
assigned usage mask, and a memory kind other than `None`. An IMPORTED entry
(`importIdentifier != 0`) carries memory kind `None`; the provider owns handle and
memory (§6.3). Imported slots are unbound placeholders in v0.1.

---

## §9.6 ImageHandleTable v0.1

Section type `0x0003`. Header `{entryCount u32, reservedFlags u32}` then fixed 40-byte
entries; exact size `8 + entryCount * 40`.

```text
ImageHandleTableEntry (40 bytes):
  +0  imageKindValue    u32   // §9.12 image kind
  +4  formatValue       u32   // §9.12 image format
  +8  width             u32
  +12 height            u32
  +16 depth             u32
  +20 mipLevelCount     u32
  +24 arrayLayerCount   u32
  +28 sampleCountValue  u32   // 1, 2, 4 or 8
  +32 usageFlags        u32   // OR-mask of §9.12 image usage bits
  +36 importIdentifier  u32   // 0 = created; non-zero = imported
```

Rules: assigned kind/format; sample count in `{1,2,4,8}`; all extents and counts
positive; a 3D image (`imageKindValue == 3`) has exactly one array layer; usage mask
assigned; a CREATED entry has a non-empty usage mask. Descriptive fields are valid for
both shapes so the host can verify a bound imported image against them. CREATED images
are device-local, optimally tiled, initial layout Undefined — the compile-time barrier
pass owns the first transition.

---

## §9.7 ShaderModuleTable v0.1

Section type `0x0006`. Header `{entryCount u32, reservedFlags u32}`, `entryCount`
16-byte directory entries, then the SPIR-V blob region.

```text
ShaderModuleTableEntry (16 bytes):
  +0  blobByteOffset    u64   // from table base; 8-byte aligned
  +8  blobByteSize      u64
```

Rules: each blob lies inside the table and outside the directory, is at least the SPIR-V
minimum (20 bytes), a multiple of 4 bytes, and begins with the SPIR-V magic word
`0x07230203`. Blob regions may be shared between entries (deduplication).

---

## §9.8 PipelineHandleTable v0.1

Section type `0x0001`. Header `{entryCount u32, reservedFlags u32}` then fixed 16-byte
entries; exact size `8 + entryCount * 16`.

```text
PipelineHandleTableEntry (16 bytes):
  +0  pipelineKindValue    u32   // §9.12 pipeline kind (v0.1: Compute only)
  +4  shaderModuleSlot     u32   // slot into the module ShaderModuleTable
  +8  pushConstantByteSize u32   // multiple of 4, at most 128 (0 = no range)
  +12 reservedFlags        u32   // must be zero
```

Rules: assigned kind; `pushConstantByteSize` a multiple of 4 in `[0, 128]`. The entry
point is the fixed convention `"main"`. The v0.1 pipeline layout is push-constants only;
resources reach the shader through buffer device addresses (see PushBufferDeviceAddress,
§9.11) — classic per-pipeline descriptor set layouts are absent. `shaderModuleSlot`
existence is a cross-table materialization check.

Graphics pipelines do NOT live in this table: their fixed state is wider, and resizing
this entry would be a breaking change under §9.13. They get their own additive section,
GraphicsPipelineTable (§9.14).

---

## §9.9 BarrierBatchTable v0.1

Section type `0x0010`. The compile-time barrier pass emits one batch per
ExecuteBarrierBatch site. Header `{batchCount u32, reservedFlags u32}`, `batchCount`
24-byte directory records, then the barrier record region (global records first, buffer
records next, image records last per batch). Barrier masks are synchronization2 stage /
access masks; the v0.1 recorder maps them to legacy `vkCmdPipelineBarrier` and therefore
rejects any mask using bits above bit 31.

```text
BarrierBatchDirectoryRecord (24 bytes):
  +0  globalBarrierCount   u32
  +4  bufferBarrierCount   u32
  +8  imageBarrierCount    u32
  +12 reservedFlags        u32   // must be zero
  +16 barriersByteOffset   u64   // from table base; 8-byte aligned

GlobalBarrierRecord (32 bytes):
  +0  sourceStageMask      u64
  +8  sourceAccessMask     u64
  +16 destinationStageMask u64
  +24 destinationAccessMask u64

BufferBarrierRecord (56 bytes):
  +0  sourceStageMask      u64
  +8  sourceAccessMask     u64
  +16 destinationStageMask u64
  +24 destinationAccessMask u64
  +32 bufferSlot           u32
  +36 reservedFlags        u32   // must be zero
  +40 byteOffset           u64
  +48 byteCount            u64   // 0xFFFFFFFFFFFFFFFF = whole size

ImageBarrierRecord (64 bytes):
  +0  sourceStageMask      u64
  +8  sourceAccessMask     u64
  +16 destinationStageMask u64
  +24 destinationAccessMask u64
  +32 imageSlot            u32
  +36 aspectMaskValue      u32   // §9.12 image aspect bits
  +40 oldLayoutValue       u32   // §9.12 image layout
  +44 newLayoutValue       u32   // §9.12 image layout
  +48 baseMipLevel         u32
  +52 mipLevelCount        u32   // 0xFFFFFFFF = remaining
  +56 baseArrayLayer       u32
  +60 arrayLayerCount      u32   // 0xFFFFFFFF = remaining
```

Rules: every barrier's source and destination stage masks are non-zero; image records
carry assigned layouts, a usable aspect mask, and non-zero mip/layer counts (sentinel
allowed). Barrier regions may be shared between batches. Buffer/image slot ranges and
bind state are record-time checks against the materialized tables.

---

## §9.10 RenderingTemplateTable v0.1

Section type `0x0011`. The compile-time output for one BeginRendering site (dynamic
rendering, no render passes — ADR-0003 / MC 26.2). Header
`{templateCount u32, reservedFlags u32}`, `templateCount` 40-byte directory records,
then per-template attachment regions (color records first, then one depth record when
present).

```text
RenderingTemplateDirectoryRecord (40 bytes):
  +0  colorAttachmentCount    u32
  +4  depthAttachmentPresent  u32   // 0 or 1
  +8  renderAreaOffsetX       i32
  +12 renderAreaOffsetY       i32
  +16 renderAreaWidth         u32
  +20 renderAreaHeight        u32
  +24 layerCount              u32
  +28 viewMask                u32
  +32 attachmentsByteOffset   u64   // from table base; 8-byte aligned

RenderingAttachmentRecord (32 bytes):
  +0  imageViewSlot   u32   // slot into the module ImageViewHandleTable
  +4  imageLayoutValue u32  // §9.12 image layout
  +8  loadOpValue     u32   // §9.12 attachment load op
  +12 storeOpValue    u32   // §9.12 attachment store op
  +16 clearValue[4]   f32   // color RGBA; depth in [0], stencil bits in [1]
```

Rules: `depthAttachmentPresent` is 0 or 1; a template has at least one attachment, a
positive render area and a positive layer count; every attachment record carries an
assigned layout, load op and store op. Attachment regions may be shared between
templates. Image-view slot existence and format/layout compatibility are record-time
checks.

---

## §9.11 Command opcode payloads v0.1

Opcodes belong to the ADR-0002 D4 ranges (`0x0000` core, `0x1000` Vulkan, `0xF000`
controlled-extension). Payload offsets below are relative to the end of the 8-byte
command header; `byteSize` is the whole command including the header. Reserved fields
are zero. Slot fields index the corresponding module table.

```text
Draw                     0x0010  byteSize 24  {vertexCount u32, instanceCount u32, firstVertex u32, firstInstance u32}
Dispatch                 0x0020  byteSize 24  {groupCountX u32, groupCountY u32, groupCountZ u32, reserved u32}
DispatchIndirect         0x0021  byteSize 24  {bufferSlot u32, reserved u32, bufferOffset u64}
BindComputePipeline      0x0002  byteSize 16  {pipelineSlot u32, reserved u32}
PushBufferDeviceAddress  0x0008  byteSize 16  {bufferSlot u32, pushConstantByteOffset u32}
ExecuteBarrierBatch      0x0032  byteSize 16  {barrierBatchSlot u32, reserved u32}
CopyBuffer               0x0040  byteSize 40  {sourceBufferSlot u32, destinationBufferSlot u32,
                                               sourceByteOffset u64, destinationByteOffset u64, copyByteCount u64}
CopyBufferToImage        0x0046  byteSize 64  {bufferSlot u32, imageSlot u32, imageLayoutValue u32,
                                               aspectMaskValue u32, mipLevel u32, baseArrayLayer u32,
                                               arrayLayerCount u32, reserved u32, bufferByteOffset u64,
                                               copyWidth u32, copyHeight u32, copyDepth u32, reserved u32}
ClearColorImage          0x0043  byteSize 56  {imageSlot u32, imageLayoutValue u32, clearColor[4] f32,
                                               aspectMaskValue u32, baseMipLevel u32, mipLevelCount u32,
                                               baseArrayLayer u32, arrayLayerCount u32, reserved u32}
CopyImageToBuffer        0x0047  byteSize 64  {imageSlot u32, bufferSlot u32, imageLayoutValue u32,
                                               aspectMaskValue u32, mipLevel u32, baseArrayLayer u32,
                                               arrayLayerCount u32, reserved u32, bufferByteOffset u64,
                                               copyWidth u32, copyHeight u32, copyDepth u32, reserved u32}
BeginRendering           0x0030  byteSize 16  {renderingTemplateSlot u32, reserved u32}
EndRendering             0x0031  byteSize 8   {}
BindGraphicsPipeline     0x0001  byteSize 16  {graphicsPipelineSlot u32, reserved u32}
SetViewport              0x0006  byteSize 32  {x f32, y f32, width f32, height f32, minDepth f32, maxDepth f32}
SetScissor               0x0007  byteSize 24  {offsetX i32, offsetY i32, width u32, height u32}
DrawIndirect             0x0012  byteSize 32  {bufferSlot u32, drawCount u32, bufferOffset u64,
                                               strideByteCount u32, reserved u32}
```

`graphicsPipelineSlot` indexes the module GraphicsPipelineTable (§9.14). Viewport and
scissor are ALWAYS dynamic pipeline state in v0.1: the recorder rejects a Draw recorded
before both have been set in the command buffer, and rejects Draw / BindGraphicsPipeline
/ SetViewport / SetScissor outside an open rendering scope, plus Draw without a bound
graphics pipeline. When a graphics pipeline is bound inside an open scope, the recorder
also checks the pipeline's declared attachment formats against the scope's rendering
template (count, and per-attachment format resolved through the ImageViewHandleTable) —
a record-time cross-table check per ADR-0002 D5.

DrawIndirect is the GPU-driven draw: its arguments (one 16-byte VkDrawIndirectCommand
per draw) live in the referenced buffer and are typically produced by a compute stage
in the same stream — the CPU prerecords the command without ever knowing the workload
(ADR-0001). It obeys every Draw recording rule above, and additionally requires the
Indirect usage bit on the buffer, a 4-aligned `bufferOffset`, and
`bufferOffset + drawCount * 16` inside the buffer. v0.1 recorders accept only
`drawCount == 1` with `strideByteCount == 16`; multi-draw is a later additive increment
gated on the device's multi-draw capability.

`CopyBufferToImage` is the P24 visible Pure Compute transfer. It mirrors
`CopyImageToBuffer` field order with source/destination slots reversed. Recording requires
buffer TransferSource, bound image TransferDestination, assigned/compatible layout and
aspect, nonzero copy extent, valid mip/layer ranges, and a tightly packed texel range
`bufferByteOffset + width*height*depth*layerCount*texelByteSize` inside the buffer. v0.1
fixes `bufferRowLength=0` and `bufferImageHeight=0`; the compiler emits barriers before and
after the copy. Swapchain usage must advertise `VK_IMAGE_USAGE_TRANSFER_DST_BIT`; absence
is an explicit unsupported-surface failure, never a silent storage-image fallback.

Payload decoding is the load-time point where slot ranges, bind state, usage bits and
copy/subresource ranges are validated against the materialized tables (ADR-0002 D5);
device addresses are resolved and baked at record time. Assigned opcodes that a recorder
does not yet implement fail loudly rather than being skipped. Opcodes present in the
ADR-0002 catalog but absent above are reserved for later increments.

---

## §9.15 Imported image runtime binding v0.1（P22）

BECS imported image entry 继续只携带非零 `importIdentifier` 与 neutral expected description，
不写入 raw Vulkan handle 或 swapchain image index。load/recreation 通过 Version 1 fixed-width
binding records 按 identifier 安装 borrowed image：image handle、format、extent、mips/layers/
samples/usage 与 `swapchainGeneration`。

Rules:

1. identifier 非零且 binding set 内唯一；每个 imported entry 恰好一个 binding，不允许 extra/
   missing/duplicate。
2. Native 校验 format/extent/mips/layers/samples 和 required usage；不兼容则在录制前失败。
3. Native 复制 binding record，不保留 Java temporary segment；slot 标为 bound/borrowed，且没有
   owned device memory。`VulkanImageTable` 永不销毁 borrowed image。
4. binding set 在一个 generation 内 immutable；recreation 先创建新 set 并重录依赖 handle 的
   command buffers，相关旧 work 完成后再退休旧 generation。
5. acquire image index 是 runtime selection，不进入 BECS；presentation runtime 可内聚管理其
   swapchain images，但通用 imported entry 必须遵守相同 metadata/ownership 规则。

---

## §9.16 Presentation runtime 与 BECS 边界

Swapchain mode/format/extent/image index、semaphore/fence rotation 与 present result 只在运行期
可知，属于 ADR-0003 D3 microkernel，不新增每帧 graph/pass 数据。BECS 仍只在 load/recreation
消费；presentation primary 按 `(frameSlot, swapchainImage)` 预录并 O(1) 选择。

图与 presentation 之间只有**一个交接点**：图产出的 output image（§9.17）。swapchain image 与
宿主 GUI/UI 纹理**不进入 BECS**，由 presentation 侧独立持有并发出其固定 barrier 集合
（ADR-0006 D3）。该边界使同一张图可在 standalone、Minecraft 宿主与离屏测试中原样执行。

---

## §9.17 GraphOutputTable v0.1（P27）

Section type `0x0008`。声明图与 presentation 的交接契约：图每帧槽产出一张 output color image，
presentation 侧只读它并自行完成后续合成与呈现。Header 16 字节，其后每帧槽一条 8 字节记录；
精确尺寸 `16 + outputCount * 8`。

```text
GraphOutputTableHeader (16 bytes):
  +0  outputCount       u32   // 每帧槽一条；必须等于 runtime frames-in-flight
  +4  finalLayoutValue  u32   // §9.12 image layout：图执行结束时 output 所处布局
  +8  reservedFlags     u32   // must be zero
  +12 reserved          u32   // must be zero

GraphOutputRecord (8 bytes):
  +0  outputImageSlot   u32   // slot into the module ImageHandleTable
  +4  reservedFlags     u32   // must be zero
```

Rules:

1. `outputCount` 非零；load 期校验其等于 runtime frames-in-flight（ADR-0004 D2 固定为 2），
   不匹配即失败。每帧槽独立 output 是必需的：相邻帧可同时在飞，共用一张 output 会让后一帧的
   图写入与前一帧的合成读取相撞。
2. 每个 `outputImageSlot` 必须存在于 ImageHandleTable，且为 **created** 条目（非 imported）——
   output image 由 RDG 分配并拥有（ADR-0006 D3）。
3. 全部 output 的 format/extent/mips/layers/samples 必须一致；presentation 侧按单一套参数建立
   合成资源。
4. `finalLayoutValue` 为 assigned layout；图的录制必须以该布局收尾，barrier 编译器据此生成尾部
   转换。presentation 侧在 load 期校验该布局与其合成实现所需的读取方式兼容，不兼容则失败。
5. output 必须携带合成实现所需的 usage 位；具体位由 presentation 侧的合成方式决定
   （采样式合成需 Sampled，blit 式合成需 TransferSource），同样在 load 期校验。
6. `outputImageSlot` 互不相同。
7. presentation 侧**只读**该 image：不拥有、不销毁、不改其 owned memory。跨提交的可见性由
   presentation 侧的固定 barrier 集合保证。
8. 无 GraphOutputTable 的模块是合法的（纯离屏/计算模块），此时该模块不可用于 presentation。

---

## §9.12 Neutral encodings v0.1

All resource attributes are backend-neutral; each backend maps them at load time (Vulkan
mappings are the reference). Unassigned values are rejected at load.

```text
BufferUsage (bit flags):   TransferSource 0x1, TransferDestination 0x2, Vertex 0x4,
                           Index 0x8, Uniform 0x10, Storage 0x20, Indirect 0x40,
                           DeviceAddress 0x80
BufferMemoryKind:          None 0, DeviceLocal 1, HostVisiblePersistentMapped 2,
                           HostVisibleReadback 3
ImageKind:                 OneDimensional 1, TwoDimensional 2, ThreeDimensional 3
ImageFormat:               R8G8B8A8Unorm 1, B8G8R8A8Unorm 2, R16G16B16A16Float 3,
                           R32Uint 4, D32Float 5, D24UnormS8Uint 6
ImageUsage (bit flags):    TransferSource 0x1, TransferDestination 0x2, Sampled 0x4,
                           Storage 0x8, ColorAttachment 0x10, DepthStencilAttachment 0x20
ImageLayout:               Undefined 0, General 1, ColorAttachment 2,
                           DepthStencilAttachment 3, ShaderReadOnly 4, TransferSource 5,
                           TransferDestination 6, Present 7
ImageAspect (bit flags):   Color 0x1, Depth 0x2, Stencil 0x4
ImageViewKind:             OneDimensional 1, TwoDimensional 2, ThreeDimensional 3
PipelineKind:              Compute 1
AttachmentLoadOp:          Load 0, Clear 1, DontCare 2
AttachmentStoreOp:         Store 0, DontCare 1
PrimitiveTopology:         PointList 1, LineList 2, LineStrip 3, TriangleList 4,
                           TriangleStrip 5
CompareOperation:          Never 1, Less 2, Equal 3, LessOrEqual 4, Greater 5,
                           NotEqual 6, GreaterOrEqual 7, Always 8
CullMode:                  None 1, Front 2, Back 3, FrontAndBack 4
FrontFace:                 CounterClockwise 1, Clockwise 2
```

---

## §9.13 版本策略

`versionMajor` gates breaking changes; replayers accept exactly the supported major.
`versionMinor` is additive only: new opcodes, section types, neutral encoding values or
appended (never reordered or resized) trailing fields. Every layout, size, offset and
enumeration value above is stable v0.1 ABI. Each cross-language format fixture under
`TestData/CommandStream/` is the byte-exact arbiter that the Java writers and the C++
validators agree (§4 staged integration gate).

---

## §9.14 GraphicsPipelineTable v0.1

Section type `0x0007`. Fixed graphics pipeline state, fully determined at compile time
(T0[1]); the load path materializes each record into one `VkGraphicsPipeline` against
dynamic rendering (`VkPipelineRenderingCreateInfo`, no render passes). Appended after
§9.13 to keep earlier section numbering stable. Header `{pipelineCount u32,
reservedFlags u32}` then fixed 80-byte records; exact size `8 + pipelineCount * 80`.

```text
GraphicsPipelineRecord (80 bytes):
  +0  vertexShaderModuleSlot         u32   // slot into the module ShaderModuleTable
  +4  fragmentShaderModuleSlot       u32   // slot into the module ShaderModuleTable
  +8  pushConstantByteSize           u32   // multiple of 4, at most 128 (0 = no range)
  +12 topologyValue                  u32   // §9.12 primitive topology
  +16 colorAttachmentCount           u32   // at most 8
  +20 depthAttachmentFormatValue     u32   // 0 = no depth attachment; else a §9.12 depth format
  +24 depthTestEnable                u32   // 0 or 1
  +28 depthWriteEnable               u32   // 0 or 1
  +32 depthCompareOperationValue     u32   // §9.12 compare operation; 0 when depthTestEnable = 0
  +36 cullModeValue                  u32   // §9.12 cull mode
  +40 frontFaceValue                 u32   // §9.12 front face
  +44 reservedFlags                  u32   // must be zero
  +48 colorAttachmentFormatValues[8] u32   // §9.12 image formats; indices >= count must be 0
```

Rules: assigned topology, cull mode and front face; `pushConstantByteSize` a multiple of
4 in `[0, 128]`; `colorAttachmentCount <= 8`; at least one attachment
(`colorAttachmentCount > 0` or `depthAttachmentFormatValue != 0`); every used color
format is an assigned color format and every unused array index is zero;
`depthAttachmentFormatValue` is zero or an assigned depth-capable format (`D32Float`,
`D24UnormS8Uint`); `depthTestEnable`/`depthWriteEnable` are 0 or 1; `depthWriteEnable`
requires `depthTestEnable`; depth enables require a depth attachment format; when
`depthTestEnable` is 1 the compare operation is assigned, when 0 it must be 0. Both
shader-module slots' existence is a cross-table materialization check. Entry points are
the fixed convention `"main"`; the push-constant range is visible to the vertex and
fragment stages together; the pipeline layout is push-constants only (resources arrive
via buffer device addresses, §9.8).

Fixed v0.1 pipeline state (not encoded, materialization constants): no vertex input
state (vertex pulling via device addresses — BindVertexBuffers/BindIndexBuffer stay
reserved), fill polygon mode, one-sample multisampling, blending disabled with all color
channels written, no stencil state, viewport and scissor dynamic (§9.11).

### §9.14.1 v0.1 exclusions

Blending, multisampling, stencil, tessellation/geometry stages, specialization
constants, pipeline caches and derivatives, and classic vertex input are all excluded;
each lands as its own additive extension when a consumer exists.
