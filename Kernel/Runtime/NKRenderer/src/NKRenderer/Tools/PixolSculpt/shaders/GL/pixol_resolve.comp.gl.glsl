#version 430
// =============================================================================
// pixol_resolve.comp.gl.glsl — NKRenderer (Tools/PixolSculpt/shaders/GL/)
// Variante OpenGL (override optionnel). "binding" sans "set". GL 4.3+.
// ⚠️ SQUELETTE.
// =============================================================================

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(binding = 0, r32f)    uniform image2D uPixolDepth;
layout(binding = 1, rgba16f) uniform image2D uPixolNormal;
layout(binding = 2, rgba8)   uniform image2D uPixolColor;

layout(binding = 3, rgba8)   uniform image2D uGBufAlbedoMetallic;
layout(binding = 4, rgba16f) uniform image2D uGBufNormalRoughness;

void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uPixolDepth);
    if (p.x >= size.x || p.y >= size.y) return;

    vec4 col = imageLoad(uPixolColor,  p);
    vec4 nrm = imageLoad(uPixolNormal, p);

    imageStore(uGBufAlbedoMetallic,  p, vec4(col.rgb, 0.0));
    imageStore(uGBufNormalRoughness, p, vec4(normalize(nrm.xyz + 1e-5), 0.5));
}
