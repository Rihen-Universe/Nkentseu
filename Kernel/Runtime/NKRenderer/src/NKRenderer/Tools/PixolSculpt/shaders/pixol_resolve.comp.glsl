#version 450
// =============================================================================
// pixol_resolve.comp.glsl — NKRenderer (Tools/PixolSculpt/)
//
// Composite le canvas pixol (depth/normal/color en espace-ecran) vers les
// cibles du G-buffer deferred, pour que la passe de lighting existante
// (NkDeferredPass) eclaire le resultat sans rien changer d'autre.
//
// ⚠️ SQUELETTE. Plein ecran : Dispatch2D(width, height, 16, 16).
// =============================================================================

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

// Entrees : canvas pixol
layout(set = 0, binding = 0, r32f)    uniform image2D uPixolDepth;
layout(set = 0, binding = 1, rgba16f) uniform image2D uPixolNormal;
layout(set = 0, binding = 2, rgba8)   uniform image2D uPixolColor;

// Sorties : G-buffer deferred (cf. renderer::NkGBuffer)
layout(set = 0, binding = 3, rgba8)   uniform image2D uGBufAlbedoMetallic;
layout(set = 0, binding = 4, rgba16f) uniform image2D uGBufNormalRoughness;
// La profondeur du G-buffer est en general une depth-stencil (non-storage) :
// la reconstruction du Z se fait souvent via une passe graphique dediee.
// TODO(PixolSculpt): decider du chemin (copie depth vs reprojection).

void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(uPixolDepth);
    if (p.x >= size.x || p.y >= size.y) return;

    // TODO(PixolSculpt):
    //   - lire depth/normal/color du pixol
    //   - si pixol "vide" (depth == sentinel), ne rien ecrire (preserver la scene)
    //   - sinon ecrire albedo + normale dans le G-buffer
    vec4 col  = imageLoad(uPixolColor,  p);
    vec4 nrm  = imageLoad(uPixolNormal, p);

    imageStore(uGBufAlbedoMetallic,  p, vec4(col.rgb, 0.0));
    imageStore(uGBufNormalRoughness, p, vec4(normalize(nrm.xyz + 1e-5), 0.5));
}
