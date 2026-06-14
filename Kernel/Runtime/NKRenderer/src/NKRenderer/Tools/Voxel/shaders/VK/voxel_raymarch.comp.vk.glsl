#version 450
// =============================================================================
// voxel_raymarch.comp.vk.glsl — NKRenderer (Tools/Voxel/shaders/VK/)
//
// SOURCE CANONIQUE. Marche le volume voxel depuis la camera et compose le
// resultat dans le G-buffer deferred (la passe de lighting existante l'eclaire).
// Plein ecran : Dispatch2D(width, height).
//
// ⚠️ SQUELETTE.
// =============================================================================

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

// Volume (lecture)
layout(set = 0, binding = 0, r16f)  uniform image3D uDensity;
layout(set = 0, binding = 1, rgba8) uniform image3D uColor;

// Sortie : G-buffer deferred (cf. renderer::NkGBuffer)
layout(set = 0, binding = 2, rgba8)   uniform image2D uGBufAlbedoMetallic;
layout(set = 0, binding = 3, rgba16f) uniform image2D uGBufNormalRoughness;

layout(push_constant) uniform PushConsts {
    mat4  invViewProj;
    vec4  camPosWorld;   // .xyz
    vec4  originWorld;    // .xyz coin (0,0,0) de la grille
    vec4  dims;          // .xyz = dimX/Y/Z, .w = voxelSize
} pc;

const int MAX_STEPS = 256;
const float DENSITY_HIT = 0.5;

void main() {
    ivec2 px   = ivec2(gl_GlobalInvocationID.xy);
    ivec2 res  = imageSize(uGBufAlbedoMetallic);
    if (px.x >= res.x || px.y >= res.y) return;

    // TODO(Voxel): reconstruire le rayon monde depuis px via invViewProj :
    //   vec2 ndc = (vec2(px) + 0.5) / vec2(res) * 2.0 - 1.0;
    //   vec4 wp  = pc.invViewProj * vec4(ndc, 1.0, 1.0); wp /= wp.w;
    //   vec3 ro  = pc.camPosWorld.xyz;
    //   vec3 rd  = normalize(wp.xyz - ro);
    //
    // Convertir en espace-grille, marcher (DDA ou pas fixe), tester la densite,
    // au premier "hit" (> DENSITY_HIT) : lire la couleur, estimer la normale par
    // gradient central de densite, ecrire dans le G-buffer. Sinon : ne rien ecrire
    // (preserver la scene rasterisee).
    //
    // ivec3 vc = ...; float d = imageLoad(uDensity, vc).r; ...
    // imageStore(uGBufAlbedoMetallic,  px, vec4(albedo, 0.0));
    // imageStore(uGBufNormalRoughness, px, vec4(normal, 0.5));
}
