#version 450
// Test fixture: writes opaque green so a readback can distinguish rasterized texels
// from the loadOp clear color (a failed draw leaves the clear color behind).
// Compiled into SolidGreen.frag.spv with:
//   glslc --target-env=vulkan1.3 -O SolidGreen.frag -o SolidGreen.frag.spv

layout(location = 0) out vec4 outputColor;

void main() {
    outputColor = vec4(0.0, 1.0, 0.0, 1.0);
}
