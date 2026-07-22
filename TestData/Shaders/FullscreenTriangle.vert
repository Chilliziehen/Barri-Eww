#version 450
// Test fixture: emits one triangle covering the whole viewport from gl_VertexIndex
// alone — no vertex buffers, matching the v0.1 no-vertex-input pipeline state (§9.14).
// Compiled into FullscreenTriangle.vert.spv with:
//   glslc --target-env=vulkan1.3 -O FullscreenTriangle.vert -o FullscreenTriangle.vert.spv

void main() {
    const vec2 positions[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
