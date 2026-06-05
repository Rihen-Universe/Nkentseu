#version 450
// =============================================================================
// pixol_resolve.comp.vk.glsl — NKRenderer (Tools/PixolSculpt/shaders/VK/)
//
// SOURCE CANONIQUE (Vulkan GLSL). Composite le canvas pixol -> G-buffer deferred.
// Transpile vers GL/DX11/DX12/MSL par le loader. Plein ecran : Dispatch2D(w,h).
// ⚠️ SQUELETTE.
// =============================================================================

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0, r32f)    uniform image2D uPixolDepth;
layout(set = 0, binding = 1, rgba16f) uniform image2D uPixolNormal;
layout(set = 0, binding = 2, rgba8)   uniform image2D uPixolColor;

layout(set = 0, binding = 3, rgba8)   uniform image2D uGBufAlbedoMetallic;
layout(set = 0, binding = 4, rgba16f) uniform image2D uGBufNormalRoughness;
// Depth du G-buffer = depth-stencil non-storage : reconstruction via passe
// graphique dediee. TODO(PixolSculpt).

void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uPixolDepth);
    if (p.x >= size.x || p.y >= size.y) return;

    // TODO: si pixol vide (depth == sentinelle), ne rien ecrire (preserver scene).
    vec4 col = imageLoad(uPixolColor,  p);
    vec4 nrm = imageLoad(uPixolNormal, p);

    imageStore(uGBufAlbedoMetallic,  p, vec4(col.rgb, 0.0));
    imageStore(uGBufNormalRoughness, p, vec4(normalize(nrm.xyz + 1e-5), 0.5));
}
