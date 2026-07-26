#version 450
// Test fixture: pulls vec2 positions through a buffer device address handed in via
// push constants — the v0.1 geometry path (§9.14: no vertex input state; §9.8:
// resources arrive by device address). Vulkan 1.2 bufferDeviceAddress feature.
// Compiled into VertexPullByDeviceAddress.vert.spv with:
//   glslc --target-env=vulkan1.3 -O VertexPullByDeviceAddress.vert -o VertexPullByDeviceAddress.vert.spv
#extension GL_EXT_buffer_reference : require

layout(buffer_reference, std430, buffer_reference_align = 8) readonly buffer VertexPositions {
    vec2 positions[];
};

layout(push_constant) uniform PushData {
    VertexPositions vertexPositions;
} pushData;

void main() {
    gl_Position = vec4(pushData.vertexPositions.positions[gl_VertexIndex], 0.0, 1.0);
}
