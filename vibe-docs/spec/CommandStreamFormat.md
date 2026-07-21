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
