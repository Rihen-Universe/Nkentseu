// =============================================================================
// Demo5_Materials.cpp  — Demo 5 : evolutions M.2+ par dessus Demo4
//
// Copie de Demo4 (NkPlanarReflection auto, M.1 v0 Layered, M.8 cube multi-mat).
// Cette demo accumule les phases M.2 a M.7 a venir :
//   - M.2 Material Parameter Collections (pool global de params)
//   - M.3 Blend par vertex color
//   - M.4 Instances hierarchiques
//   - M.5 Material Functions (.glsli)
//   - M.6 Vertex Paint runtime
//   - M.7 Decal Materials (depend deferred)
//
// Demo4 reste stable pour reference ; Demo5 absorbe les nouveautes au fur et a mesure.
//
// Controles :
//   1-5     : selectionner le materiau actif (sphere mise en evidence)
//   +/=     : augmenter roughness (PBR 1-2) ou threshold (Toon/Anime 3-4)
//   -       : diminuer roughness ou threshold
//   M       : toggle metallic 0<->1 (PBR 1-2 uniquement)
//   C       : changer couleur albedo (cycle dans une palette)
//   O       : changer largeur outline (Toon/Anime 3-4 uniquement)
//   R       : toggle planar reflection on/off
//   V       : toggle VSync
//   Camera (cf. DemoCamera.h) :
//     LEFT-drag souris  : rotation yaw/pitch
//     MID/RIGHT-drag    : pan target
//     mouse wheel       : zoom
//     WASD / fleches    : pan target XZ
//     Q/E ou PGUP/PGDN  : monter/descendre target
//     T                 : toggle auto-orbit
//     F                 : toggle FPS mode
//     HOME              : reset camera
// =============================================================================
#include "DemoCommon.h"
#include "DemoCamera.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialLibrary.h"      // Phase G
#include "NKRenderer/Materials/NkMaterialCollection.h"   // Phase M.2
#include "NKRenderer/Tools/Reflection/NkPlanarReflectionSystem.h"
#include "NKRenderer/Core/NkTextureAsset.h"            // Phase H
#include "NKImage/Core/NkImage.h"                     // Phase H smoke test
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include <cmath>

namespace nkentseu { namespace demo {

// ── Palette de couleurs preset ────────────────────────────────────────────────
static const NkVec3f kPalette[] = {
    {1.00f, 0.78f, 0.20f},  // or
    {0.90f, 0.10f, 0.10f},  // rouge
    {0.10f, 0.35f, 0.90f},  // bleu
    {0.65f, 0.10f, 0.85f},  // violet
    {0.90f, 0.90f, 0.90f},  // blanc
    {0.10f, 0.80f, 0.30f},  // vert
    {1.00f, 0.45f, 0.00f},  // orange
};
static constexpr int kPaletteSize = (int)(sizeof(kPalette) / sizeof(kPalette[0]));

// Largeurs d'outline cyclables
static const float32 kOutlineWidths[] = {0.f, 1.f, 2.f, 3.f, 4.f};
static constexpr int kOutlineCount = 5;

// ── Parametres par materiau ────────────────────────────────────────────────────
struct MatParams {
    float32 roughness  = 0.5f;
    float32 metallic   = 0.f;
    float32 threshold  = 0.3f;
    float32 outlineW   = 2.f;
    int     colorIdx   = 0;
};

// ── Etat de la demo ───────────────────────────────────────────────────────────
struct Demo5MatState {
    NkMaterial*  mats[5]     = {};
    const char*  matNames[5] = {"PBR Metal", "PBR Plastic", "Toon", "Anime", "Unlit"};
    NkMeshHandle meshSphere;
    NkMeshHandle meshPlane;
    NkMeshHandle meshCube;        // Phase M.8 : multi-material cube
    NkMeshHandle meshPaintedCube; // Phase M.3 : cube peint (vColors variés par face)
    NkMaterial*  paintedMat = nullptr;  // Layered maskSource=0 dédié à M.3
    // Phase M.6 Dynamic Paint : cache CPU des 24 vertices du painted cube pour
    // permettre l'update VBO partiel a chaque appui P (peint une face random).
    NkVector<NkVertex3D> paintedVertices;
    // Phase M.6 v3 : texture brush. Pattern 32x32 (float, alpha map) genere
    // proceduralement au start. Plus tard : remplaçable par une PNG runtime.
    static constexpr int  kBrushTexSize = 32;
    NkVector<float32>    brushTex;     // size = kBrushTexSize * kBrushTexSize

    // Phase M.6 v4 : texture painting (Blender Image Sequence style).
    // Cube dédié avec UV atlas 2×3 (chaque face occupe un sous-rectangle distinct
    // de la texture). Click → raycast → UV → write region pixels = pixel-perfect.
    static constexpr uint32 kPaintTexSize = 256;
    NkMeshHandle             meshTexCube;
    NkMaterial*              texMat = nullptr;
    NkTexHandle              paintTex;
    NkVector<uint8>          paintTexData;  // 256*256*4 = 256 KiB CPU cache
    // Debug : position monde du dernier "splash" pour DrawDebugSphere overlay.
    NkVec3f             lastPaintPos = {0.f, 0.f, 0.f};
    bool                hasPainted   = false;
    // Brush radius dynamique (touches [ et ]) + transform du painted cube.
    float32             brushRadius     = 0.6f;
    NkMat4f             paintedTransform= NkMat4f::Identity();
    // Cache cam basis vectors pour raycast click-paint (mis a jour chaque frame).
    // Plus fiable qu'invViewProj qui dependant de la convention storage matrix.
    NkVec3f             cachedCamPos      = {0,0,0};
    NkVec3f             cachedCamForward  = {0,0,-1};
    NkVec3f             cachedCamRight    = {1,0,0};
    NkVec3f             cachedCamUp       = {0,1,0};
    float32             cachedCamFovY     = 55.f;   // degres
    float32             cachedViewportW   = 1280.f;
    float32             cachedViewportH   = 720.f;
    DemoCamera   camera;
    int          activeMat    = 0;
    MatParams    params[5];

    // Planar Reflection (auto, gere par NkPlanarReflectionSystem du renderer).
    NkPlanarReflectionHandle reflHandle;
    NkMaterial*              floorMat    = nullptr;
    bool                     reflEnabled = true;

    // Phase G : sphere #1 (or) chargee depuis un .nkasset au lieu du code.
    // Si valide, remplace mats[0]->GetInstHandle() au moment du draw call.
    NkMatInstHandle goldFromAsset;
};

// ── Applique les parametres au materiau ───────────────────────────────────────
static void ApplyParams(NkMaterial* mat, int matIdx, const MatParams& p) {
    if (!mat || !mat->IsValid()) return;
    NkVec3f col = kPalette[p.colorIdx % kPaletteSize];
    switch (matIdx) {
        case 0: // PBR Metal
        case 1: // PBR Plastic
            mat->SetAlbedo(col)->SetRoughness(p.roughness)->SetMetallic(p.metallic);
            break;
        case 2: // Toon
            mat->SetAlbedo(col)
               ->SetToonThreshold(p.threshold)
               ->SetOutline(p.outlineW, {0.f, 0.f, 0.f});
            break;
        case 3: // Anime
            mat->SetAlbedo(col)
               ->SetToonThreshold(p.threshold)
               ->SetOutline(p.outlineW, {0.05f, 0.05f, 0.1f})
               ->SetRim(0.6f, {0.9f, 0.95f, 1.f});
            break;
        case 4: // Unlit
            mat->SetAlbedo(col);
            break;
        default: break;
    }
}

// ── Phase H smoke test : NkImage charge PNG/JPEG/HDR depuis Resources/vracs ──
static void NKImageSmokeTest() {
    logger.Info("[Demo5][NKImage] === Smoke test PNG/JPEG/HDR ===\n");

    struct TestCase { const char* path; const char* label; bool isHDR; };
    static const TestCase kCases[] = {
        { "Resources/NKRenderer/Textures/vracs/Checkerboard.png",        "PNG",       false },
        { "Resources/NKRenderer/Textures/vracs/container.jpg",           "JPEG-JFIF", false },
        { "Resources/NKRenderer/Textures/vracs/bricks2.jpg",             "JPEG-EXIF", false },
        { "Resources/NKRenderer/Textures/vracs/HDR/newport_loft.hdr",    "HDR",       true  },
    };
    int passed = 0, total = (int)(sizeof(kCases) / sizeof(kCases[0]));
    for (int i = 0; i < total; i++) {
        const auto& t = kCases[i];
        NkImage* img = NkImage::Load(t.path, 0);
        if (img && img->IsValid()) {
            logger.Info("[Demo5][NKImage] {0} OK : {1}x{2} ch={3} fmt={4}\n",
                        t.label, img->Width(), img->Height(),
                        (int)img->Channels(), (int)img->Format());
            ++passed;
        } else {
            logger.Warnf("[Demo5][NKImage] %s FAIL : '%s' did not load\n",
                         t.label, t.path);
        }
        // NkImage utilise un allocateur custom (nkMalloc) : delete std est UB.
        if (img) img->Free();
    }
    logger.Info("[Demo5][NKImage] === {0}/{1} passed ===\n", passed, total);
}

// ── Phase M.6 v3 : brush radial avec TEXTURE comme pattern de stamp ──────────
// Mutualise par la touche P (centre random) et le click souris (centre = hit
// point raycast). Texture brush 32x32 procedurale (etoile 5 branches),
// projetee sur un plan local defini par centerNormal. Accumulation = lerp(
// currentColor, newColor, brushStrength * texSample).
static void Demo5_PaintAtLocal(Demo5MatState* st, NkRenderer* renderer,
                                NkVec3f centerLocal,
                                NkVec3f centerNormal,
                                uint8 nr, uint8 ng, uint8 nb) {
    if (!st || st->paintedVertices.Empty() || !st->meshPaintedCube.IsValid()) return;
    const float32 brushRadius   = st->brushRadius;
    const float32 brushStrength = 0.85f;

    // Construit une base orthonormee (right, up) perpendiculaire a centerNormal
    // pour projeter les vertices voisins sur le plan tangent au centre.
    NkVec3f N = centerNormal;
    {
        const float32 nl = sqrtf(N.x*N.x + N.y*N.y + N.z*N.z);
        if (nl < 1e-6f) N = {0, 1, 0};
        else N = {N.x/nl, N.y/nl, N.z/nl};
    }
    // axis arbitraire pour Gram-Schmidt (different de N).
    NkVec3f axisHint = (fabsf(N.y) > 0.9f) ? NkVec3f{1, 0, 0} : NkVec3f{0, 1, 0};
    NkVec3f right{
        axisHint.y * N.z - axisHint.z * N.y,
        axisHint.z * N.x - axisHint.x * N.z,
        axisHint.x * N.y - axisHint.y * N.x,
    };
    {
        const float32 rl = sqrtf(right.x*right.x + right.y*right.y + right.z*right.z);
        if (rl > 1e-6f) right = {right.x/rl, right.y/rl, right.z/rl};
    }
    NkVec3f up{
        N.y * right.z - N.z * right.y,
        N.z * right.x - N.x * right.z,
        N.x * right.y - N.y * right.x,
    };

    const int  TN = Demo5MatState::kBrushTexSize;
    const auto sampleBrush = [&](float32 u, float32 v) -> float32 {
        // u, v in [0, 1] -> texel index nearest.
        if (u < 0.f || u > 1.f || v < 0.f || v > 1.f) return 0.f;
        int ix = (int)(u * (TN - 1));
        int iy = (int)(v * (TN - 1));
        if (ix < 0) ix = 0; if (ix > TN - 1) ix = TN - 1;
        if (iy < 0) iy = 0; if (iy > TN - 1) iy = TN - 1;
        return st->brushTex[(nk_size)(iy * TN + ix)];
    };

    uint32 minIdx = (uint32)st->paintedVertices.Size();
    uint32 maxIdx = 0;
    int touched = 0;
    for (uint32 v = 0; v < (uint32)st->paintedVertices.Size(); v++) {
        auto& vt = st->paintedVertices[v];
        const NkVec3f d = vt.pos - centerLocal;
        const float32 dist = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z);
        if (dist > brushRadius) continue;
        // Projection sur plan tangent (right, up). Mapping [-radius, +radius]
        // -> [0, 1] UV. Sample brush texture comme alpha multiplicateur.
        const float32 dr = d.x * right.x + d.y * right.y + d.z * right.z;
        const float32 du = d.x * up.x    + d.y * up.y    + d.z * up.z;
        const float32 texU = dr / brushRadius * 0.5f + 0.5f;
        const float32 texV = du / brushRadius * 0.5f + 0.5f;
        const float32 falloff = sampleBrush(texU, texV);
        if (falloff <= 0.001f) continue;
        const float32 alpha = brushStrength * falloff;
        const uint32 cur = vt.color;
        const uint8 cr = (cur >>  0) & 0xFF;
        const uint8 cg = (cur >>  8) & 0xFF;
        const uint8 cb = (cur >> 16) & 0xFF;
        const uint8 ca = (cur >> 24) & 0xFF;
        auto lerpb = [](uint8 a, uint8 b, float32 t) -> uint8 {
            const float32 v = (float32)a + ((float32)b - (float32)a) * t;
            return (uint8)(v < 0.f ? 0.f : (v > 255.f ? 255.f : v));
        };
        const uint8 fr = lerpb(cr, nr, alpha);
        const uint8 fg = lerpb(cg, ng, alpha);
        const uint8 fb = lerpb(cb, nb, alpha);
        vt.color = ((uint32)ca << 24) | ((uint32)fb << 16)
                 | ((uint32)fg << 8)  | (uint32)fr;
        if (v < minIdx) minIdx = v;
        if (v > maxIdx) maxIdx = v;
        touched++;
    }
    if (touched > 0 && renderer) {
        if (auto* meshSys = renderer->GetMeshSystem()) {
            meshSys->UpdateVerticesRange(st->meshPaintedCube,
                                         st->paintedVertices.Data() + minIdx,
                                         minIdx,
                                         (maxIdx - minIdx + 1));
        }
    }
    // Position monde pour debug overlay (cube a (4.5, 1.0, -2.0), scale 1).
    st->lastPaintPos = {centerLocal.x + 4.5f, centerLocal.y + 1.0f, centerLocal.z - 2.f};
    st->hasPainted   = true;

    logger.Info("[Demo5][M.6] Paint local=({0:.2},{1:.2},{2:.2}) "
                "color=({3},{4},{5}) touched={6} range=[{7},{8}]\n",
                centerLocal.x, centerLocal.y, centerLocal.z,
                (int)nr, (int)ng, (int)nb, touched,
                (int)minIdx, (int)maxIdx);
}

// ── Phase M.6 v5 : 3D Brush (Displacement Painting facon Blender Sculpt) ──────
// Au lieu de modifier vColor, deplace les vertices selon la normale locale :
//   vt.pos += centerNormal * depth * smoothFalloff(dist/radius)
//   depth > 0 : extrude (bosse), depth < 0 : enfonce (empreinte de pas).
// Recalcul des normales NON fait pour MVP (eclairage approximatif sur la
// deformation), suffisant pour demontrer le concept "pas dans la neige".
static void Demo5_DisplaceAtLocal(Demo5MatState* st, NkRenderer* renderer,
                                   NkVec3f centerLocal, NkVec3f centerNormal,
                                   float32 depth) {
    if (!st || st->paintedVertices.Empty() || !st->meshPaintedCube.IsValid()) return;
    const float32 brushRadius = st->brushRadius;

    NkVec3f N = centerNormal;
    {
        const float32 nl = sqrtf(N.x*N.x + N.y*N.y + N.z*N.z);
        if (nl < 1e-6f) N = {0, 1, 0};
        else N = {N.x/nl, N.y/nl, N.z/nl};
    }

    uint32 minIdx = (uint32)st->paintedVertices.Size();
    uint32 maxIdx = 0;
    int touched = 0;
    for (uint32 v = 0; v < (uint32)st->paintedVertices.Size(); v++) {
        auto& vt = st->paintedVertices[v];
        const NkVec3f d = vt.pos - centerLocal;
        const float32 dist = sqrtf(d.x*d.x + d.y*d.y + d.z*d.z);
        if (dist > brushRadius) continue;
        const float32 t = dist / brushRadius;
        const float32 falloff = 1.f - (3.f * t * t - 2.f * t * t * t);
        // Deplace vt.pos le long de centerNormal de (depth * falloff).
        // depth > 0 : extrude vers l'exterieur ; depth < 0 : enfonce.
        vt.pos.x += N.x * depth * falloff;
        vt.pos.y += N.y * depth * falloff;
        vt.pos.z += N.z * depth * falloff;
        if (v < minIdx) minIdx = v;
        if (v > maxIdx) maxIdx = v;
        touched++;
    }

    if (touched > 0 && renderer) {
        if (auto* meshSys = renderer->GetMeshSystem()) {
            meshSys->UpdateVerticesRange(st->meshPaintedCube,
                                         st->paintedVertices.Data() + minIdx,
                                         minIdx,
                                         (maxIdx - minIdx + 1));
        }
    }
    st->lastPaintPos = {centerLocal.x + 4.5f, centerLocal.y + 1.0f, centerLocal.z - 2.f};
    st->hasPainted   = true;

    logger.Info("[Demo5][M.6 v5] Displace local=({0:.2},{1:.2},{2:.2}) "
                "depth={3:.3} touched={4} range=[{5},{6}]\n",
                centerLocal.x, centerLocal.y, centerLocal.z, depth, touched,
                (int)minIdx, (int)maxIdx);
}

// ── Phase M.6 v4 : peinture pixel-perfect dans paintTex via UV mapping ───────
// hitLocal = position du hit en local-space du tex cube ([-0.5, 0.5]^3).
// Determine la face hit -> coord (u, v) dans la face -> position dans l'atlas
// 2x3 de paintTex -> ecrit un brush radial dans paintTexData + upload region GPU.
static void Demo5_TexPaintAt(Demo5MatState* st, NkRenderer* renderer,
                              NkVec3f hitLocal, int faceIdx,
                              uint8 nr, uint8 ng, uint8 nb) {
    if (!st || !st->paintTex.IsValid() || st->paintTexData.Empty()) return;
    if (faceIdx < 0 || faceIdx >= 6) return;
    const uint32 TS = Demo5MatState::kPaintTexSize;

    // (u_face, v_face) dans [0, 1] selon la face hit (passe par l'appelant
    // qui utilise Demo5_RayAABB_Face pour avoir un test face robuste,
    // independant des cas dégénérés sur les aretes du cube).
    // Mapping calque sur les UVs poses lors de la creation du meshTexCube :
    const int face = faceIdx;
    float32 u_face = 0.f, v_face = 0.f;
    switch (face) {
        case 0: u_face = 0.5f - hitLocal.x; v_face = 0.5f - hitLocal.y; break; // +Z
        case 1: u_face = hitLocal.x + 0.5f; v_face = 0.5f - hitLocal.y; break; // -Z
        case 2: u_face = 0.5f - hitLocal.z; v_face = 0.5f - hitLocal.y; break; // -X
        case 3: u_face = hitLocal.z + 0.5f; v_face = 0.5f - hitLocal.y; break; // +X
        case 4: u_face = 0.5f - hitLocal.z; v_face = 0.5f - hitLocal.x; break; // +Y
        case 5: u_face = hitLocal.x + 0.5f; v_face = hitLocal.z + 0.5f; break; // -Y
    }
    // Clamp pour eviter overflow aux aretes/coins.
    if (u_face < 0.f) u_face = 0.f; if (u_face > 1.f) u_face = 1.f;
    if (v_face < 0.f) v_face = 0.f; if (v_face > 1.f) v_face = 1.f;

    // 2. Atlas rect pour la face (doit matcher la creation du meshTexCube).
    struct AtlasRect { float u0, v0, u1, v1; };
    const AtlasRect atlas[6] = {
        { 0.f,  0.f,    0.5f, 1.f/3.f },
        { 0.5f, 0.f,    1.f,  1.f/3.f },
        { 0.f,  1.f/3.f,0.5f, 2.f/3.f },
        { 0.5f, 1.f/3.f,1.f,  2.f/3.f },
        { 0.f,  2.f/3.f,0.5f, 1.f      },
        { 0.5f, 2.f/3.f,1.f,  1.f      },
    };
    const AtlasRect& a = atlas[face];
    const float32 uvX = a.u0 + (a.u1 - a.u0) * u_face;
    const float32 uvY = a.v0 + (a.v1 - a.v0) * v_face;

    // 3. Brush radial en pixel space. brushRadius (world space) -> pixels.
    // Approximation : la face occupe environ TS * (a.u1-a.u0) pixels en X.
    // brushRadius en world (cube 1x1) -> pixels = brushRadius * face_width_px.
    const float32 faceWpx = TS * (a.u1 - a.u0);
    const int brushPx = (int)NkMax(2.f, st->brushRadius * faceWpx);
    const int cx = (int)(uvX * TS);
    const int cy = (int)(uvY * TS);

    // Clip pixel range a l'atlas rect de cette face (eviter painting sur faces voisines).
    const int rxMin = (int)(a.u0 * TS);
    const int ryMin = (int)(a.v0 * TS);
    const int rxMax = (int)(a.u1 * TS) - 1;
    const int ryMax = (int)(a.v1 * TS) - 1;
    const int x0 = NkMax(cx - brushPx, rxMin);
    const int y0 = NkMax(cy - brushPx, ryMin);
    const int x1 = NkMin(cx + brushPx, rxMax);
    const int y1 = NkMin(cy + brushPx, ryMax);
    if (x1 < x0 || y1 < y0) return;

    // 4. Brush radial CPU : pour chaque pixel dans la zone, falloff + lerp.
    const float32 brushStrength = 0.85f;
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const float32 dx = (float32)(x - cx) / (float32)brushPx;
            const float32 dy = (float32)(y - cy) / (float32)brushPx;
            const float32 d  = sqrtf(dx*dx + dy*dy);
            if (d > 1.f) continue;
            const float32 falloff = 1.f - (3.f * d * d - 2.f * d * d * d);
            const float32 alpha = brushStrength * falloff;
            const nk_size off = ((nk_size)y * TS + (nk_size)x) * 4;
            const uint8 cr = st->paintTexData[off + 0];
            const uint8 cg = st->paintTexData[off + 1];
            const uint8 cb = st->paintTexData[off + 2];
            auto lerpb = [](uint8 a, uint8 b, float32 t) -> uint8 {
                const float32 v = (float32)a + ((float32)b - (float32)a) * t;
                return (uint8)(v < 0.f ? 0.f : (v > 255.f ? 255.f : v));
            };
            st->paintTexData[off + 0] = lerpb(cr, nr, alpha);
            st->paintTexData[off + 1] = lerpb(cg, ng, alpha);
            st->paintTexData[off + 2] = lerpb(cb, nb, alpha);
            st->paintTexData[off + 3] = 255;
        }
    }

    // 5. Upload texture GPU. MVP : Update full (256 KiB par clic, acceptable).
    // Optimisation v4.1 : WriteTextureRegion sur la bande [x0..x1] x [y0..y1]
    // via NkIDevice -> reduit a la taille de la zone peinte.
    if (auto* texLib = renderer->GetTextures()) {
        texLib->Update(st->paintTex, st->paintTexData.Data(), 0, 0, 0);
    }

    logger.Info("[Demo5][M.6 v4] TexPaint hitLocal=({0:.2},{1:.2},{2:.2}) face={3} "
                "uFace={4:.2} vFace={5:.2} uv=({6:.3},{7:.3}) pixel=({8},{9})\n",
                hitLocal.x, hitLocal.y, hitLocal.z,
                face, u_face, v_face, uvX, uvY, cx, cy);
}

// Raycast vs AABB qui retourne AUSSI l'axe et le signe de la face d'entree
// (utile pour determiner sur quelle face le ray a hit, independamment de la
// position exacte du hit — corrige les ambiguites aux aretes).
static float32 Demo5_RayAABB_Face(NkVec3f origin, NkVec3f dir,
                                   NkVec3f boxMin, NkVec3f boxMax,
                                   int& outAxis, int& outSign) {
    float32 tMin = 0.f;
    float32 tMax = 1e9f;
    int entryAxis = -1;
    int entrySign = -1;
    for (int i = 0; i < 3; i++) {
        const float32 o = (&origin.x)[i];
        const float32 d = (&dir.x)[i];
        const float32 lo = (&boxMin.x)[i];
        const float32 hi = (&boxMax.x)[i];
        if (fabsf(d) < 1e-6f) {
            if (o < lo || o > hi) return -1.f;
            continue;
        }
        float32 t1 = (lo - o) / d;
        float32 t2 = (hi - o) / d;
        int sign = -1;
        if (t1 > t2) { float32 tmp = t1; t1 = t2; t2 = tmp; sign = +1; }
        if (t1 > tMin) { tMin = t1; entryAxis = i; entrySign = sign; }
        if (t2 < tMax) tMax = t2;
        if (tMin > tMax) return -1.f;
    }
    outAxis = entryAxis;
    outSign = entrySign;
    return tMin;
}

// Raycast vs AABB. Renvoie t hit (>=0) ou -1 si miss.
// Standard slab method (Kay-Kajiya).
static float32 Demo5_RayAABB(NkVec3f origin, NkVec3f dir,
                              NkVec3f boxMin, NkVec3f boxMax) {
    float32 tMin = 0.f;
    float32 tMax = 1e9f;
    for (int i = 0; i < 3; i++) {
        const float32 o = (&origin.x)[i];
        const float32 d = (&dir.x)[i];
        const float32 lo = (&boxMin.x)[i];
        const float32 hi = (&boxMax.x)[i];
        if (fabsf(d) < 1e-6f) {
            if (o < lo || o > hi) return -1.f;
            continue;
        }
        float32 t1 = (lo - o) / d;
        float32 t2 = (hi - o) / d;
        if (t1 > t2) { float32 tmp = t1; t1 = t2; t2 = tmp; }
        if (t1 > tMin) tMin = t1;
        if (t2 < tMax) tMax = t2;
        if (tMin > tMax) return -1.f;
    }
    return tMin;
}

// ── Init ──────────────────────────────────────────────────────────────────────
bool Demo5_Materials_Init(DemoCtx& ctx) {
    NKImageSmokeTest();   // Phase H : valider que NKImage charge les 3 formats clefs

    auto* st = new Demo5MatState();
    ctx.userData = st;

    // Meshes
    auto* meshSys = ctx.renderer->GetMeshSystem();
    if (!meshSys) {
        logger.Errorf("[Demo5] Pas de NkMeshSystem\n");
        delete st; ctx.userData = nullptr; return false;
    }
    st->meshSphere = meshSys->GetSphere();
    st->meshCube   = meshSys->GetCube();   // Phase M.8 : 6 sous-meshes (1 par face)

    // ── Phase M.3 + M.6 v3 : painted cube subdivise (vColors par face) ───────
    // Chaque face est subdivisee en S x S quads pour avoir assez de resolution
    // de paint (M.3 = mask par face ; M.6 = brush radial assez fin pour qu'on
    // voie le pattern texture). Total = 6 * (S+1)^2 vertices.
    {
        // Subdivision compromise FPS/precision : S=8 -> 9x9 vertices par face
        // = 486 vertices total au lieu de 3750. Resolution suffisante pour
        // M.3 (mask par face) + M.6 v3 (brush radial) sans tuer les FPS dans
        // 4 passes (shadow + 2 RT miroir + main). M.6 v4 texture painting
        // (tex cube separe) reste pixel-perfect grace a la texture writable.
        const int S = 8;
        const int VN_PER_FACE = (S + 1) * (S + 1);
        const int IN_PER_FACE = S * S * 6;

        auto Pack = [](uint8 r, uint8 g, uint8 b) -> uint32 {
            return ((uint32)255 << 24) | ((uint32)b << 16) | ((uint32)g << 8) | r;
        };
        const uint32 faceColors[6] = {
            Pack(255,   0,   0),  // +Z front : R=1 -> peinture
            Pack(  0,   0,   0),  // -Z back  : R=0 -> rouille
            Pack(255, 255,   0),  // -X left  : R=1
            Pack(  0, 255,   0),  // +X right : R=0
            Pack(255,   0, 255),  // +Y top   : R=1
            Pack(  0,   0, 255),  // -Y bot   : R=0
        };
        const NkVec3f n[6] = {{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
        const NkVec3f t[6] = {{1,0,0},{-1,0,0},{0,0,-1},{0,0,1},{1,0,0},{1,0,0}};
        // Origin + 2 axes du quad pour chaque face (origin = coin (-,-), au + ->droite, av + ->haut).
        struct FaceFrame { NkVec3f origin, axU, axV; };
        const FaceFrame F[6] = {
            // +Z front : origin (-0.5,-0.5, 0.5), U=+X, V=+Y
            {{-0.5f,-0.5f, 0.5f}, {1,0,0}, {0,1,0}},
            // -Z back  : origin ( 0.5,-0.5,-0.5), U=-X, V=+Y
            {{ 0.5f,-0.5f,-0.5f}, {-1,0,0}, {0,1,0}},
            // -X left  : origin (-0.5,-0.5,-0.5), U=+Z, V=+Y
            {{-0.5f,-0.5f,-0.5f}, {0,0,1}, {0,1,0}},
            // +X right : origin ( 0.5,-0.5, 0.5), U=-Z, V=+Y
            {{ 0.5f,-0.5f, 0.5f}, {0,0,-1}, {0,1,0}},
            // +Y top   : origin (-0.5, 0.5,-0.5), U=+X, V=+Z
            {{-0.5f, 0.5f,-0.5f}, {1,0,0}, {0,0,1}},
            // -Y bot   : origin (-0.5,-0.5, 0.5), U=+X, V=-Z
            {{-0.5f,-0.5f, 0.5f}, {1,0,0}, {0,0,-1}},
        };

        st->paintedVertices.Resize((nk_size)(6 * VN_PER_FACE));
        NkVector<uint32> idx;
        idx.Resize((nk_size)(6 * IN_PER_FACE));

        for (int f = 0; f < 6; f++) {
            const FaceFrame& ff = F[f];
            const int baseV = f * VN_PER_FACE;
            const int baseI = f * IN_PER_FACE;
            // Vertices : grid (S+1)x(S+1) parameter [0,1] sur (axU, axV).
            for (int j = 0; j <= S; j++) {
                for (int i = 0; i <= S; i++) {
                    const float32 u = (float32)i / (float32)S;
                    const float32 v = (float32)j / (float32)S;
                    auto& vt = st->paintedVertices[(nk_size)(baseV + j * (S+1) + i)];
                    vt.pos = {
                        ff.origin.x + ff.axU.x * u + ff.axV.x * v,
                        ff.origin.y + ff.axU.y * u + ff.axV.y * v,
                        ff.origin.z + ff.axU.z * u + ff.axV.z * v,
                    };
                    vt.normal  = n[f];
                    vt.tangent = t[f];
                    vt.uv      = {u, v};
                    vt.uv2     = {0, 0};
                    vt.color   = faceColors[f];
                }
            }
            // Indices : 2 tris par quad.
            for (int j = 0; j < S; j++) {
                for (int i = 0; i < S; i++) {
                    const uint32 v0 = (uint32)(baseV + j     * (S+1) + i);
                    const uint32 v1 = (uint32)(baseV + j     * (S+1) + i + 1);
                    const uint32 v2 = (uint32)(baseV + (j+1) * (S+1) + i + 1);
                    const uint32 v3 = (uint32)(baseV + (j+1) * (S+1) + i);
                    const int k = baseI + (j * S + i) * 6;
                    idx[(nk_size)(k+0)] = v0;
                    idx[(nk_size)(k+1)] = v1;
                    idx[(nk_size)(k+2)] = v2;
                    idx[(nk_size)(k+3)] = v0;
                    idx[(nk_size)(k+4)] = v2;
                    idx[(nk_size)(k+5)] = v3;
                }
            }
        }

        NkMeshDesc d = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(),
                                          st->paintedVertices.Data(),
                                          (uint32)st->paintedVertices.Size(),
                                          idx.Data(),
                                          (uint32)idx.Size());
        d.debugName = "Demo5_PaintedCube_Subdivided";
        d.bounds    = NkAABB::Unit();
        d.dynamic   = true;
        st->meshPaintedCube = meshSys->Create(d);
        logger.Info("[Demo5][M.3+M.6] Painted cube subdivise: 6 faces * {0}x{0} = {1} vertices, "
                    "{2} indices\n", (S+1), (int)st->paintedVertices.Size(), (int)idx.Size());
    }

    // ── Phase M.6 v3 : texture brush procedurale (etoile 5 branches) ─────────
    // 32x32 alpha map utilisee comme pattern de stamp par Demo5_PaintAtLocal.
    // L'etoile non-symetrique radiale rend visible que le brush utilise une
    // texture (pas juste un falloff smoothstep circulaire).
    {
        const int N = Demo5MatState::kBrushTexSize;
        st->brushTex.Resize((nk_size)(N * N));
        for (int y = 0; y < N; y++) {
            for (int x = 0; x < N; x++) {
                // UV [-1, 1]
                const float fx = (float)x / (N - 1) * 2.f - 1.f;
                const float fy = (float)y / (N - 1) * 2.f - 1.f;
                const float r  = sqrtf(fx * fx + fy * fy);
                const float th = atan2f(fy, fx);
                // Etoile 5 branches : rayon module par |cos(5*theta/2)|.
                const float starR = 0.55f + 0.40f * fabsf(cosf(2.5f * th));
                float v = 0.f;
                if (r < starR) {
                    // Falloff smoothstep dans la branche.
                    const float t = r / starR;
                    v = 1.f - (3.f * t * t - 2.f * t * t * t);
                }
                st->brushTex[y * N + x] = v;
            }
        }
        logger.Info("[Demo5][M.6 v3] Brush texture procedurale generee ({0}x{0})\n", N);
    }

    // ── Phase M.6 v4 : texture writable + cube avec UV atlas 2×3 ─────────────
    // Texture 256×256 grise au start, va etre peinte pixel-par-pixel via
    // raycast → UV → WriteTextureRegion. Le cube a un UV layout atlas 2×3
    // pour que chaque face occupe un sous-rectangle distinct (pas d'overlap).
    {
        const uint32 TS = Demo5MatState::kPaintTexSize;
        st->paintTexData.Resize((nk_size)(TS * TS * 4));
        // Init : gris clair pour distinguer du fond.
        for (uint32 i = 0; i < TS * TS; i++) {
            st->paintTexData[i*4 + 0] = 200;  // R
            st->paintTexData[i*4 + 1] = 200;  // G
            st->paintTexData[i*4 + 2] = 200;  // B
            st->paintTexData[i*4 + 3] = 255;  // A
        }
        // Lignes de sub-grid pour visualiser l'atlas 2×3 (debug visuel).
        for (uint32 v = 0; v < TS; v++) {
            for (uint32 u = 0; u < TS; u++) {
                const bool gridU = (u == TS/2);
                const bool gridV = (v == TS/3 || v == 2*TS/3);
                if (gridU || gridV) {
                    st->paintTexData[(v*TS + u)*4 + 0] = 80;
                    st->paintTexData[(v*TS + u)*4 + 1] = 80;
                    st->paintTexData[(v*TS + u)*4 + 2] = 80;
                }
            }
        }

        NkTextureCreateDesc texDesc;
        texDesc.width     = TS;
        texDesc.height    = TS;
        texDesc.format    = NkGPUFormat::NK_RGBA8_UNORM;
        texDesc.pixels    = st->paintTexData.Data();
        texDesc.srgb      = true;
        texDesc.debugName = "Demo5_PaintTex";
        st->paintTex = ctx.renderer->GetTextures()->Create(texDesc);
        logger.Info("[Demo5][M.6 v4] Texture writable creee ({0}x{0} RGBA8 sRGB)\n", TS);
    }

    // Mesh cube avec UV atlas 2×3 (chaque face occupe son sous-rectangle).
    {
        const int VN_PER_FACE = 4;
        const int IN_PER_FACE = 6;
        // Atlas layout : 2 colonnes × 3 lignes (cellule = 0.5 × 0.333).
        struct AtlasRect { float u0, v0, u1, v1; };
        const AtlasRect atlas[6] = {
            { 0.f,    0.f,    0.5f,   1.f/3.f }, // +Z front
            { 0.5f,   0.f,    1.f,    1.f/3.f }, // -Z back
            { 0.f,    1.f/3.f,0.5f,   2.f/3.f }, // -X left
            { 0.5f,   1.f/3.f,1.f,    2.f/3.f }, // +X right
            { 0.f,    2.f/3.f,0.5f,   1.f      }, // +Y top
            { 0.5f,   2.f/3.f,1.f,    1.f      }, // -Y bot
        };
        const NkVec3f n[6] = {{0,0,1},{0,0,-1},{-1,0,0},{1,0,0},{0,1,0},{0,-1,0}};
        const NkVec3f t[6] = {{1,0,0},{-1,0,0},{0,0,-1},{0,0,1},{1,0,0},{1,0,0}};
        const NkVec3f p[8] = {
            {-0.5f,-0.5f, 0.5f},{0.5f,-0.5f, 0.5f},{0.5f,0.5f, 0.5f},{-0.5f,0.5f, 0.5f},
            {-0.5f,-0.5f,-0.5f},{0.5f,-0.5f,-0.5f},{0.5f,0.5f,-0.5f},{-0.5f,0.5f,-0.5f}
        };
        const int fi[6][4] = {{1,0,3,2},{4,5,6,7},{0,4,7,3},{5,1,2,6},{3,7,6,2},{0,1,5,4}};
        // UVs au sein de chaque cellule atlas : (u0,v1),(u1,v1),(u1,v0),(u0,v0).
        NkVertex3D verts[24]; uint32 idx[36];
        for (int f = 0; f < 6; f++) {
            const AtlasRect& a = atlas[f];
            const NkVec2f uvs[4] = {
                {a.u0, a.v1}, {a.u1, a.v1}, {a.u1, a.v0}, {a.u0, a.v0}
            };
            for (int v = 0; v < 4; v++) {
                auto& vt = verts[f*4+v];
                vt.pos    = p[fi[f][v]];
                vt.normal = n[f];
                vt.tangent= t[f];
                vt.uv     = uvs[v];
                vt.uv2    = {0,0};
                vt.color  = 0xFFFFFFFFu;
            }
            uint32 b = f * 4;
            idx[f*6+0]=b; idx[f*6+1]=b+1; idx[f*6+2]=b+2;
            idx[f*6+3]=b; idx[f*6+4]=b+2; idx[f*6+5]=b+3;
        }
        NkMeshDesc d = NkMeshDesc::Simple(renderer::NkVertexLayout::Default3D(),
                                          verts, 24, idx, 36);
        d.debugName = "Demo5_TexCube";
        d.bounds    = NkAABB::Unit();
        st->meshTexCube = meshSys->Create(d);
    }

    st->meshPlane  = meshSys->GetPlane();

    // Parametres initiaux par materiau
    st->params[0] = {0.15f, 1.0f, 0.3f, 0.f,  0};  // PBR Metal : or, metallic
    st->params[1] = {0.40f, 0.0f, 0.3f, 0.f,  1};  // PBR Plastic : rouge, dielectric
    st->params[2] = {0.5f,  0.0f, 0.3f, 2.0f, 2};  // Toon : bleu
    st->params[3] = {0.5f,  0.0f, 0.25f,1.5f, 3};  // Anime : violet
    st->params[4] = {0.5f,  0.0f, 0.3f, 0.f,  6};  // Unlit : orange (evite le blanc sur fond blanc)

    // Creation des materiaux
    auto* matSys = ctx.renderer->GetMaterials();
    if (!matSys) {
        logger.Errorf("[Demo5] Pas de NkMaterialSystem\n");
        delete st; ctx.userData = nullptr; return false;
    }

    // M.1 v0 : sphere #4 utilise un Layered (2 layers PBR : or en haut,
    // cuivre noirci en bas, blend via UV.y) au lieu de Unlit. Visualise
    // immediatement le mecanisme N-couches.
    static const NkMaterialType kTypes[5] = {
        NkMaterialType::NK_PBR_METALLIC,
        NkMaterialType::NK_PBR_METALLIC,
        NkMaterialType::NK_TOON,
        NkMaterialType::NK_ANIME,
        NkMaterialType::NK_LAYERED,        // remplace NK_UNLIT pour M.1 demo
    };

    for (int i = 0; i < 5; i++) {
        st->mats[i] = NkMaterial::Create(matSys, kTypes[i]);
        if (!st->mats[i] || !st->mats[i]->IsValid()) {
            logger.Warnf("[Demo5] Materiau [{0}] ({1}) invalide — fallback tint seul\n",
                         i, st->matNames[i]);
        } else if (kTypes[i] == NkMaterialType::NK_LAYERED) {
            // Demo5 : rouille (oxydation rugueuse) + peinture (lisse, sature).
            // Cas tres parlant pour M.1 Layered + M.2 gameTime : le mask oscille
            // entre les deux via sin(gameTime) cote shader -> on voit la rouille
            // se former / disparaitre en continu sur la sphere.
            NkPBRParams paint;
            paint.albedo    = {0.18f, 0.42f, 0.85f, 1.f};   // bleu peinture
            paint.metallic  = 0.0f;
            paint.roughness = 0.35f;

            NkPBRParams rust;
            rust.albedo     = {0.42f, 0.12f, 0.05f, 1.f};   // rouille rouge-brun
            rust.metallic   = 0.25f;                         // un peu metallique
            rust.roughness  = 0.90f;                         // tres rugueux

            st->mats[i]->SetLayerBase(rust);              // mask=0 -> rouille
            st->mats[i]->SetLayerTop(paint);              // mask=1 -> peinture
            st->mats[i]->SetLayerMaskSource(4);           // 4 = vUV.y gradient
            st->matNames[i] = "Layered (rouille/peinture)";
        } else {
            ApplyParams(st->mats[i], i, st->params[i]);
        }
    }

    // ── Phase M.3 : material Layered dedie au painted cube (maskSource=0) ────
    // Meme rust/paint que mats[4], mais mask = vColor.r au lieu de vUV.y.
    // Sur le painted cube, vColor.r varie par face -> chaque face est soit
    // 100% peinture (face avec faceColors[f].r=255) soit 100% rouille (r=0).
    {
        st->paintedMat = NkMaterial::Create(matSys, NkMaterialType::NK_LAYERED);
        if (st->paintedMat && st->paintedMat->IsValid()) {
            NkPBRParams paint;
            paint.albedo    = {0.18f, 0.42f, 0.85f, 1.f};
            paint.metallic  = 0.0f;
            paint.roughness = 0.35f;
            NkPBRParams rust;
            rust.albedo     = {0.42f, 0.12f, 0.05f, 1.f};
            rust.metallic   = 0.25f;
            rust.roughness  = 0.90f;
            st->paintedMat->SetLayerBase(rust);          // mask=0 -> rouille
            st->paintedMat->SetLayerTop(paint);          // mask=1 -> peinture
            st->paintedMat->SetLayerMaskSource(0);       // 0 = vColor.r
        }
    }

    // ── Phase M.6 v4 : material PBR avec albedo = texture writable ───────────
    {
        st->texMat = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
        if (st->texMat && st->texMat->IsValid()) {
            st->texMat->SetAlbedo({1.f, 1.f, 1.f}, 1.f)
                       ->SetRoughness(0.55f)
                       ->SetMetallic(0.05f);
            if (st->paintTex.IsValid()) {
                st->texMat->SetAlbedoMap(st->paintTex);
            }
        }
    }

    // ── Planar Reflection AUTO ───────────────────────────────────────────────
    // Phase R.1 : utilise NkPlanarReflectionSystem (NKRenderer). Le renderer
    // s'occupe automatiquement de la passe miroir et de la mise a jour du RT
    // du materiau cible chaque frame -- Demo4 n'a plus rien a faire que de
    // submit ses drawcalls normalement.
    {
        // Materiau du sol : ReflFloor — shader dédié screen-space UV pour le reflet
        // roughness=0.05 → 95% de réflectivité (quasi miroir)
        st->floorMat = NkMaterial::Create(matSys, NkMaterialType::NK_REFL_FLOOR);
        if (st->floorMat && st->floorMat->IsValid()) {
            st->floorMat->SetAlbedo({0.55f, 0.55f, 0.60f})
                         ->SetRoughness(0.05f);
        }

        // Enregistre le plan Y=0 dans le systeme : le renderer fera la passe
        // miroir auto, mettra a jour `floorMat.albedo` avec le RT, et fournira
        // `uCam.mirrorViewProj` au shader ReflFloor pour le sample screen-UV.
        if (auto* refl = ctx.renderer->GetPlanarReflection()) {
            NkPlanarReflectionDesc desc;
            desc.normal   = {0.f, 1.f, 0.f};
            desc.point    = {0.f, 0.f, 0.f};
            desc.rtWidth  = (ctx.width  / 2 > 0) ? ctx.width  / 2 : 512;
            desc.rtHeight = (ctx.height / 2 > 0) ? ctx.height / 2 : 256;
            desc.hdr      = true;
            desc.debugName= NkString("Demo4_FloorReflection");
            // Phase R.2 : bidirectionnel (BOTH) -> 2 RT, sol visible des deux
            // cotes. Sphere du dessus refletees sur la face avant, sphere du
            // dessous refletees sur la face arriere.
            desc.twoSided = true;
            desc.faceMode = NkPlanarFaceMode::BOTH;
            if (st->floorMat && st->floorMat->IsValid())
                desc.targetMaterial = st->floorMat->GetInstHandle();
            st->reflHandle = refl->Register(desc);
        }
    }

    // ── Phase G : round-trip Save → Scan → Load via NkMaterialLibrary ────────
    // Demonstre le pipeline complet : un material code-driven est serialise en
    // .nkasset binaire, puis re-charge via NkAssetRegistry + NkMaterialLibrary.
    // L'instance retournee remplace st->mats[0] (sphere #1 "or") au draw.
    {
        auto* matLib = matSys->GetLibrary();
        if (matLib && matLib->IsValid()) {
            // Construire un NkMaterialAsset code-driven (Gold metallic).
            NkMaterialAsset gold;
            gold.type             = NkMaterialType::NK_PBR_METALLIC;
            gold.name             = NkString("Gold");
            gold.queue            = NkRenderQueue::NK_OPAQUE;
            gold.cullMode         = nkentseu::renderer::NkCullMode::NK_NONE;
            gold.pbr.albedo       = {1.00f, 0.78f, 0.20f, 1.f};
            gold.pbr.metallic     = 1.0f;
            gold.pbr.roughness    = 0.15f;
            gold.pbr.ao           = 1.0f;

            // Save dans Resources/NKRenderer/Materials/Gold.nkasset
            NkString outPath = NkString("Resources/NKRenderer/Materials/Gold.nkasset");
            NkAssetId savedId;
            if (matLib->Save(gold, outPath, &savedId)) {
                logger.Info("[Demo5][PhaseG] Saved '{0}' (id={1})\n",
                            outPath, savedId.ToString());
            } else {
                logger.Warnf("[Demo5][PhaseG] Save failed for %s\n", outPath.CStr());
            }

            // Scanner le dossier pour enregistrer l'asset dans NkAssetRegistry.
            matLib->ScanDirectory(NkString("Resources/NKRenderer/Materials"));

            // Load par chemin logique. Le NkMaterialLibrary construit l'instance
            // via NkMaterialSystem en utilisant les params PBR du .nkasset.
            st->goldFromAsset = matLib->Load(NkString("/Materials/Gold"));
            if (st->goldFromAsset.IsValid()) {
                logger.Info("[Demo5][PhaseG] Loaded '/Materials/Gold' OK -> sphere #1 will use asset-loaded material\n");
            } else {
                logger.Warnf("[Demo5][PhaseG] Load failed for /Materials/Gold\n");
            }

            // Hot-reload : modification du .nkasset a chaud -> patche l'instance.
            matLib->EnableHotReload(true);
        }
    }

    // ── Phase H : Texture asset round-trip ──────────────────────────────────
    // Save un NkTextureAsset qui reference un PNG existant, puis Load pour
    // recuperer un NkTexHandle a appliquer sur la sphere #2 comme albedo map.
    {
        auto* texLib = ctx.renderer->GetTextures();
        if (texLib) {
            NkTextureAsset texAsset;
            texAsset.sourceFilePath = NkString("Resources/NKRenderer/Textures/vracs/awesomeface.png");
            texAsset.targetFormat   = NkGPUFormat::NK_RGBA8_UNORM;
            texAsset.generateMips   = true;
            texAsset.sRGB           = true;

            NkString outAsset    = NkString("Resources/NKRenderer/Materials/AwesomeFace.nkasset");
            NkString logicalPath = NkString("/Textures/AwesomeFace");
            NkAssetId savedId;
            if (NkTextureAssetIO::Save(texAsset, outAsset, logicalPath, &savedId)) {
                logger.Info("[Demo5][PhaseH] Saved texture asset '{0}' (id={1})\n",
                            logicalPath, savedId.ToString());
                NkTexHandle texH = NkTextureAssetIO::LoadById(savedId, texLib);
                if (texH.IsValid()) {
                    // Applique sur sphere #2 (PBR Plastic) comme albedo map.
                    if (st->mats[1] && st->mats[1]->IsValid()) {
                        st->mats[1]->SetAlbedoMap(texH);
                        logger.Info("[Demo5][PhaseH] AwesomeFace albedo applied to sphere #2\n");
                    }
                } else {
                    logger.Warnf("[Demo5][PhaseH] LoadById failed for AwesomeFace\n");
                }
            }
        }
    }

    // Controles clavier : modifications temps reel
    // Capture le renderer par valeur (pointer stable) pour acceder a la
    // Material Parameter Collection depuis le callback (ctx est temporaire).
    auto* renderer = ctx.renderer;
    NkEvents().AddEventCallback<NkKeyPressEvent>([st, renderer](NkKeyPressEvent* e) {
        switch (e->GetKey()) {
            // Selectionner le materiau actif
            case NkKey::NK_NUM1: st->activeMat = 0; break;
            case NkKey::NK_NUM2: st->activeMat = 1; break;
            case NkKey::NK_NUM3: st->activeMat = 2; break;
            case NkKey::NK_NUM4: st->activeMat = 3; break;
            case NkKey::NK_NUM5: st->activeMat = 4; break;

            // Roughness + (PBR) ou threshold + (Toon/Anime)
            case NkKey::NK_EQUALS: {
                int i = st->activeMat;
                if (i <= 1)
                    st->params[i].roughness  = NkMin(1.f, st->params[i].roughness  + 0.05f);
                else
                    st->params[i].threshold  = NkMin(1.f, st->params[i].threshold  + 0.05f);
                ApplyParams(st->mats[i], i, st->params[i]);
                break;
            }
            // Roughness - (PBR) ou threshold - (Toon/Anime)
            case NkKey::NK_MINUS: {
                int i = st->activeMat;
                if (i <= 1)
                    st->params[i].roughness  = NkMax(0.f, st->params[i].roughness  - 0.05f);
                else
                    st->params[i].threshold  = NkMax(0.f, st->params[i].threshold  - 0.05f);
                ApplyParams(st->mats[i], i, st->params[i]);
                break;
            }
            // Toggle metallic (PBR uniquement)
            case NkKey::NK_M: {
                int i = st->activeMat;
                if (i <= 1) {
                    st->params[i].metallic = (st->params[i].metallic > 0.5f) ? 0.f : 1.f;
                    ApplyParams(st->mats[i], i, st->params[i]);
                }
                break;
            }
            // Cycle couleur albedo
            case NkKey::NK_C: {
                int i = st->activeMat;
                st->params[i].colorIdx = (st->params[i].colorIdx + 1) % kPaletteSize;
                ApplyParams(st->mats[i], i, st->params[i]);
                break;
            }
            // Cycle outline width (Toon/Anime uniquement)
            case NkKey::NK_O: {
                int i = st->activeMat;
                if (i == 2 || i == 3) {
                    int cur = 0;
                    for (int j = 0; j < kOutlineCount; j++) {
                        if (st->params[i].outlineW == kOutlineWidths[j]) { cur = j; break; }
                    }
                    cur = (cur + 1) % kOutlineCount;
                    st->params[i].outlineW = kOutlineWidths[cur];
                    ApplyParams(st->mats[i], i, st->params[i]);
                }
                break;
            }
            // Toggle planar reflection
            case NkKey::NK_R: {
                st->reflEnabled = !st->reflEnabled;
                logger.Info("[Demo5] Planar reflection : {0}\n",
                             st->reflEnabled ? "ON" : "OFF");
                break;
            }
            // VSync toggle
            case NkKey::NK_V: {
                static bool vsync = true;
                vsync = !vsync;
                logger.Info("[Demo5] VSync toggle (relancer avec --vsync=0 pour desactiver)\n");
                break;
            }
            // Phase M.2 : cycle globalTint dans le Material Parameter Collection.
            // Le shader Layered (sphere #5) multiplie son resultat par params[0]
            // -> toutes les sphères Layered changent de teinte en un seul Set.
            case NkKey::NK_G: {
                static int tintIdx = 0;
                static const NkVec4f kTints[5] = {
                    {1.f, 1.f, 1.f, 1.f},   // blanc (no-op)
                    {1.f, 0.5f, 0.5f, 1.f}, // rouge
                    {0.5f, 1.f, 0.5f, 1.f}, // vert
                    {0.5f, 0.5f, 1.f, 1.f}, // bleu
                    {1.f, 1.f, 0.4f, 1.f},  // jaune
                };
                tintIdx = (tintIdx + 1) % 5;
                if (auto* mpc = renderer->GetMaterialCollection()) {
                    mpc->SetVec4(NkString("globalTint"), kTints[tintIdx]);
                    logger.Info("[Demo5][MPC] globalTint -> ({0},{1},{2},{3})\n",
                                kTints[tintIdx].x, kTints[tintIdx].y,
                                kTints[tintIdx].z, kTints[tintIdx].w);
                }
                break;
            }
            // Phase M.6 : ajuste brush radius. [ diminue, ] augmente.
            case NkKey::NK_LBRACKET: {
                st->brushRadius = NkMax(0.1f, st->brushRadius - 0.1f);
                logger.Info("[Demo5][M.6] brushRadius = {0:.2}\n", st->brushRadius);
                break;
            }
            case NkKey::NK_RBRACKET: {
                st->brushRadius = NkMin(2.0f, st->brushRadius + 0.1f);
                logger.Info("[Demo5][M.6] brushRadius = {0:.2}\n", st->brushRadius);
                break;
            }
            // Phase M.6 Dynamic Paint v1 : brush radial + accumulation + partial
            // VBO update. Centre du brush = position d'un vertex random dans le
            // mesh ; rayon = 0.6 (en local space cube de taille 1) ; falloff
            // smoothstep. Accumulation : lerp(currentColor, newColor, strength).
            case NkKey::NK_P: {
                if (st->paintedVertices.Empty()) break;
                static uint32 seed = 0xcafebabe;
                seed = seed * 1103515245u + 12345u;
                const uint32 centerIdx = (seed >> 8) % (uint32)st->paintedVertices.Size();
                const NkVec3f centerPos    = st->paintedVertices[centerIdx].pos;
                const NkVec3f centerNormal = st->paintedVertices[centerIdx].normal;
                seed = seed * 1103515245u + 12345u; const uint8 nr = (seed >> 24) & 0xFF;
                seed = seed * 1103515245u + 12345u; const uint8 ng = (seed >> 24) & 0xFF;
                seed = seed * 1103515245u + 12345u; const uint8 nb = (seed >> 24) & 0xFF;
                Demo5_PaintAtLocal(st, renderer, centerPos, centerNormal, nr, ng, nb);
                break;
            }
            // Phase M.6 v5 : 3D Brush Displacement (centre = vertex random).
            // N = "iNfoncer" (empreinte de pas style neige).
            // B = "Bosse" (extrude, bump).
            // Touches choisies hors conflits DemoCamera (WASD/QE).
            case NkKey::NK_N: {
                if (st->paintedVertices.Empty()) break;
                static uint32 seed = 0xfeedface;
                seed = seed * 1103515245u + 12345u;
                const uint32 centerIdx = (seed >> 8) % (uint32)st->paintedVertices.Size();
                const NkVec3f centerPos    = st->paintedVertices[centerIdx].pos;
                const NkVec3f centerNormal = st->paintedVertices[centerIdx].normal;
                Demo5_DisplaceAtLocal(st, renderer, centerPos, centerNormal, -0.05f);
                break;
            }
            case NkKey::NK_B: {
                if (st->paintedVertices.Empty()) break;
                static uint32 seed = 0xbadc0de;
                seed = seed * 1103515245u + 12345u;
                const uint32 centerIdx = (seed >> 8) % (uint32)st->paintedVertices.Size();
                const NkVec3f centerPos    = st->paintedVertices[centerIdx].pos;
                const NkVec3f centerNormal = st->paintedVertices[centerIdx].normal;
                Demo5_DisplaceAtLocal(st, renderer, centerPos, centerNormal, +0.05f);
                break;
            }
            default: break;
        }
    });

    // Phase M.6 v2 : click-paint via raycast. Click GAUCHE souris -> ray world
    // depuis la position pixel -> test AABB painted cube -> hit point local
    // space -> brush. La camera (LEFT-drag rotate) reste prioritaire ; ici on
    // peint quand la cam ne tourne pas (l'orbit controller mute son own drag,
    // donc le click "court" passe par les 2 handlers — accepte pour MVP).
    NkEvents().AddEventCallback<NkMouseButtonPressEvent>(
        [st, renderer](NkMouseButtonPressEvent* e) {
            if (!e->IsLeft()) return;
            if (!st->meshPaintedCube.IsValid() || st->paintedVertices.Empty()) return;

            // Pixel souris -> NDC [-1, 1] avec Y flipped (top-left = +1).
            const float32 mx = (float32)e->GetX();
            const float32 my = (float32)e->GetY();
            const float32 ndcX = 2.f * mx / st->cachedViewportW - 1.f;
            const float32 ndcY = 1.f - 2.f * my / st->cachedViewportH;

            // Approche FOV-based : reconstitue le ray directement avec les
            // basis vectors de la cam (plus fiable qu'invViewProj qui depend
            // de la convention storage matrix).
            const float32 fovY = st->cachedCamFovY * 0.01745329f;  // deg -> rad
            const float32 aspect = st->cachedViewportW / st->cachedViewportH;
            const float32 tanHalfFov = tanf(fovY * 0.5f);
            // x_view = ndcX * tan * aspect, y_view = ndcY * tan, z_view = -1.
            const float32 vx = ndcX * tanHalfFov * aspect;
            const float32 vy = ndcY * tanHalfFov;
            // Compose en world via basis cam : dir = vx*right + vy*up + forward.
            NkVec3f rayDir = {
                vx * st->cachedCamRight.x + vy * st->cachedCamUp.x + st->cachedCamForward.x,
                vx * st->cachedCamRight.y + vy * st->cachedCamUp.y + st->cachedCamForward.y,
                vx * st->cachedCamRight.z + vy * st->cachedCamUp.z + st->cachedCamForward.z,
            };
            const float32 len = sqrtf(rayDir.x*rayDir.x + rayDir.y*rayDir.y + rayDir.z*rayDir.z);
            if (len < 1e-6f) return;
            rayDir = {rayDir.x / len, rayDir.y / len, rayDir.z / len};
            const NkVec3f rayOrig = st->cachedCamPos;

            // Test sequentiel : painted cube (M.3+M.6 vColor) puis tex cube (M.6 v4).
            // Le 1er qui hit prend l'event. La cam est a droite par defaut donc
            // ordre arbitraire — chacun a son AABB distinct, pas de conflit.
            const NkVec3f paintedMin = {4.0f, 0.5f, -2.5f};
            const NkVec3f paintedMax = {5.0f, 1.5f, -1.5f};
            const NkVec3f texMin     = {-5.0f, 0.5f, -2.5f};
            const NkVec3f texMax     = {-4.0f, 1.5f, -1.5f};

            const float32 tPainted = Demo5_RayAABB(rayOrig, rayDir, paintedMin, paintedMax);
            int texFaceAxis = -1, texFaceSign = 0;
            const float32 tTex     = Demo5_RayAABB_Face(rayOrig, rayDir, texMin, texMax,
                                                       texFaceAxis, texFaceSign);

            static uint32 seed = 0xdeadface;
            seed = seed * 1103515245u + 12345u; const uint8 nr = (seed >> 24) & 0xFF;
            seed = seed * 1103515245u + 12345u; const uint8 ng = (seed >> 24) & 0xFF;
            seed = seed * 1103515245u + 12345u; const uint8 nb = (seed >> 24) & 0xFF;

            // Choisis le hit le plus proche (t > 0 minimum).
            const bool hitPainted = (tPainted >= 0.f);
            const bool hitTex     = (tTex     >= 0.f);
            if (!hitPainted && !hitTex) return;

            const bool preferTex = hitTex && (!hitPainted || tTex < tPainted);
            if (preferTex) {
                // Phase M.6 v4 : tex cube touche -> paint dans la texture.
                const NkVec3f hitWorld = {
                    rayOrig.x + tTex * rayDir.x,
                    rayOrig.y + tTex * rayDir.y,
                    rayOrig.z + tTex * rayDir.z,
                };
                const NkVec3f hitLocal = {hitWorld.x + 4.5f, hitWorld.y - 1.0f, hitWorld.z + 2.f};

                // Convert axis+sign -> face index :
                //   axis=2,sign=+1 -> +Z (0), sign=-1 -> -Z (1)
                //   axis=0,sign=-1 -> -X (2), sign=+1 -> +X (3)
                //   axis=1,sign=+1 -> +Y (4), sign=-1 -> -Y (5)
                int face = 0;
                if      (texFaceAxis == 2 && texFaceSign == +1) face = 0;
                else if (texFaceAxis == 2 && texFaceSign == -1) face = 1;
                else if (texFaceAxis == 0 && texFaceSign == -1) face = 2;
                else if (texFaceAxis == 0 && texFaceSign == +1) face = 3;
                else if (texFaceAxis == 1 && texFaceSign == +1) face = 4;
                else if (texFaceAxis == 1 && texFaceSign == -1) face = 5;
                Demo5_TexPaintAt(st, renderer, hitLocal, face, nr, ng, nb);
            } else {
                // Phase M.3/M.6 v3 : painted cube touche -> brush vColor.
                const NkVec3f hitWorld = {
                    rayOrig.x + tPainted * rayDir.x,
                    rayOrig.y + tPainted * rayDir.y,
                    rayOrig.z + tPainted * rayDir.z,
                };
                const NkVec3f hitLocal = {hitWorld.x - 4.5f, hitWorld.y - 1.0f, hitWorld.z + 2.f};
                NkVec3f centerNormal = {0, 1, 0};
                float32 bestDist = 1e9f;
                for (const auto& vt : st->paintedVertices) {
                    const NkVec3f d = vt.pos - hitLocal;
                    const float32 dd = d.x*d.x + d.y*d.y + d.z*d.z;
                    if (dd < bestDist) { bestDist = dd; centerNormal = vt.normal; }
                }
                Demo5_PaintAtLocal(st, renderer, hitLocal, centerNormal, nr, ng, nb);
            }
        });

    // Camera orbit interactive (souris + clavier) via NkOrbitCameraController3D.
    // Defaut : target=(0,0.5,0) au centre de la scene, distance=9, yaw=0, pitch=-0.2.
    // Auto-orbit off : controle full manuel souris + clavier.
    st->camera.Controller().SetCenter({0.f, 0.5f, 0.f}, 9.f, 0.f, -0.2f);
    st->camera.Controller().SetAutoOrbit(false);
    st->camera.InstallEvents();

    logger.Info("[Demo5] Init OK — 5 materiaux crees\n");
    logger.Info("[Demo5] 1-5:mat  +/-:roughness  M:metallic  C:couleur  O:outline  R:reflet\n");
    logger.Info("[Demo5] G : cycle globalTint (M.2 Material Parameter Collection)\n");
    logger.Info("[Demo5] P : Dynamic Paint M.6 -- splash random sur le painted cube\n");
    logger.Info("[Demo5] [ / ] : ajuste brush radius (M.6)\n");
    logger.Info("[Demo5] CLIC GAUCHE : paint au point de la souris (raycast)\n");
    logger.Info("[Demo5] N / B : 3D Brush -- iNfoncer / Bosse (M.6 v5 displacement)\n");
    logger.Info("[Demo5] Camera : LEFT-drag rotate | wheel zoom | WASD/fleches pan | T:auto-orbit | F:FPS | HOME:reset\n");
    return true;
}

// ── Frame ─────────────────────────────────────────────────────────────────────
void Demo5_Materials_Frame(DemoCtx& ctx, float32 dt) {
    auto* st = (Demo5MatState*)ctx.userData;
    st->camera.Update(dt);

    // Phase M.2 : update params chrono dans la Material Parameter Collection.
    if (auto* mpc = ctx.renderer->GetMaterialCollection()) {
        mpc->SetFloat(NkString("gameTime"), ctx.totalTime);
    }

    // Phase G : animation verticale des spheres (monte/descend recursivement).
    // Differentie Demo4 de Demo3 (statique). Amplitude 0.3u, periode 2s, phase
    // decalee par sphere pour effet "vagues".
    // Centre=1.2, amp=0.3 : Y in [0.9, 1.5], avec radius 0.55 le bottom min est
    // 0.35 -> sphere entierement au-dessus du plan Y=0 a tout moment du bob.
    // Important pour la passe miroir auto : sinon la sphere traverse le plan
    // et le clip plane filter (centre AABB) la garde malgre tout, creant des
    // "bribes" de reflet visibles vue de dessous.
    const float32 bobAmp    = 0.3f;
    const float32 bobOmega  = 3.14159f; // 2*pi / 2s
    auto BobY = [&](int i) -> float32 {
        float32 phase = i * 0.6f; // decalage par sphere
        return 1.2f + bobAmp * sinf(ctx.totalTime * bobOmega + phase);
    };

    if (!ctx.renderer->BeginFrame()) return;

    auto* r3d = ctx.renderer->GetRender3D();
    if (!r3d) {
        ctx.renderer->Present();
        ctx.renderer->EndFrame();
        return;
    }

    // ── Camera (controllable via souris + clavier) ───────────────────────────
    // Cf. DemoCamera.h pour les binds. Le mode auto-orbit reproduit l'animation
    // historique (rotation auto) ; toggle T pour figer/relancer.
    NkCamera3DData camData;
    camData.up        = {0.f, 1.f, 0.f};
    camData.fovY      = 55.f;
    camData.aspect    = (float32)ctx.width / (float32)ctx.height;
    camData.nearPlane = 0.1f;
    camData.farPlane  = 100.f;
    NkCamera3D cam(camData);
    st->camera.Controller().Apply(cam);   // ecrit position + target depuis l'orbit state

    // Phase M.6 v2 : cache basis vectors pour raycast click-paint. Approche
    // simple "ray from camera FOV" plus fiable que invViewProj (convention
    // storage matrix differente entre Inverse() et operator*).
    st->cachedCamPos     = cam.GetPosition();
    st->cachedCamForward = cam.GetForward();
    st->cachedCamRight   = cam.GetRight();
    st->cachedCamUp      = cam.GetUp();
    st->cachedCamFovY    = cam.GetFOV();
    st->cachedViewportW  = (float32)ctx.width;
    st->cachedViewportH  = (float32)ctx.height;

    // ── Scene context : lights ────────────────────────────────────────────────
    NkSceneContext sctx;
    sctx.camera = cam;
    sctx.time   = ctx.totalTime;

    // Soleil directionnel
    NkLightDesc sun;
    sun.type       = NkLightType::NK_DIRECTIONAL;
    sun.direction  = {-0.5f, -1.f, -0.4f};
    sun.color      = {1.f, 0.95f, 0.9f};
    sun.intensity  = 3.5f;
    sun.castShadow = true;
    sctx.lights.PushBack(sun);

    // Fill bleue douce
    NkLightDesc fill;
    fill.type      = NkLightType::NK_POINT;
    fill.position  = {0.f, 4.f, 4.f};
    fill.color     = {0.4f, 0.5f, 0.9f};
    fill.intensity = 2.5f;
    fill.range     = 20.f;
    sctx.lights.PushBack(fill);

    sctx.ambientIntensity = 0.12f;

    // ── Plus de passe miroir manuelle ! ─────────────────────────────────────
    // NkPlanarReflectionSystem (declenche dans la passe Geometry de
    // RenderGraph) fait automatiquement :
    //   1. Calcul mirrorMat + mirrorViewProj
    //   2. Re-flush des drawcalls deja submit'd avec mirror_matrix dans le RT
    //   3. Update du materiau cible (st->floorMat) avec le RT comme albedo
    //   4. Injection mirrorViewProj dans uCam pour le sample screen-UV
    // L'utilisateur submit ses drawcalls UNE seule fois ci-dessous.

    // ── Passe principale ──────────────────────────────────────────────────────
    r3d->BeginScene(sctx);

    // ── Sol avec matériau réfléchissant ───────────────────────────────────────
    {
        NkDrawCall3D dc;
        dc.mesh       = st->meshPlane;
        dc.transform  = NkMat4f::Scale({14.f, 1.f, 14.f});
        dc.aabb       = {{-7.f, -0.01f, -7.f}, {7.f, 0.01f, 7.f}};
        // castShadow = true : sans ca, les spheres SOUS le sol (Y<0) recoivent
        // la lumiere directe du soleil (qui est physiquement bloquee par le sol).
        // Le shadow map ne contient l'occlusion du sol que si on le declare
        // shadow caster. Les spheres AU-DESSUS ne sont pas affectees (rien
        // entre elles et le soleil dans la direction lumiere).
        dc.castShadow = true;
        // Utilise le matériau réfléchissant si disponible
        if (st->reflEnabled && st->floorMat && st->floorMat->IsValid())
            dc.material = st->floorMat->GetInstHandle();
        r3d->Submit(dc);
    }

    // ── 5 spheres avec leur materiau ──────────────────────────────────────────
    for (int i = 0; i < 5; i++) {
        const float32 x = (float32)(i - 2) * 2.4f;
        const float32 y = BobY(i);

        NkDrawCall3D dc;
        dc.mesh      = st->meshSphere;
        dc.transform = NkMat4f::Translate({x, y, 0.f}) *
                       NkMat4f::Scale({0.55f, 0.55f, 0.55f});
        // AABB englobante de l'oscillation autour du centre Y=1.2, amp 0.3,
        // radius 0.55 : Y range [0.35, 2.05]. Centre AABB Y = 1.2 > 0 -> le
        // clip plane filter (cote +N) garde bien la sphere dans la passe miroir.
        dc.aabb      = {{x - 0.55f, 0.35f, -0.55f}, {x + 0.55f, 2.05f, 0.55f}};

        // Phase G : sphere #0 utilise l'instance chargee depuis .nkasset.
        if (i == 0 && st->goldFromAsset.IsValid())
            dc.material = st->goldFromAsset;
        else if (st->mats[i] && st->mats[i]->IsValid())
            dc.material = st->mats[i]->GetInstHandle();

        dc.tint      = kPalette[st->params[i].colorIdx % kPaletteSize];
        dc.metallic  = st->params[i].metallic;
        dc.roughness = st->params[i].roughness;

        r3d->Submit(dc);
    }

    // ── 2 spheres EN DESSOUS du plan (pour valider que les reflets en dessous
    // ne montrent que les objets reellement situes sous Y=0) ─────────────────
    // Y < 0, reutilisent les materiaux des sphere #0 et #4 pour distinction
    // visuelle (or + layered). Pas de bobbing -> position statique facile a
    // identifier dans le miroir vue de dessous (cf. roadmap : 2eme passe RT
    // mirror inverse pour reflets en dessous, pas encore implementee).
    {
        const float32 belowY = -1.2f;
        for (int j = 0; j < 2; j++) {
            const float32 x = (j == 0) ? -1.5f : 1.5f;
            const int     matIdx = (j == 0) ? 0 : 4;   // or, puis layered
            NkDrawCall3D dc;
            dc.mesh      = st->meshSphere;
            dc.transform = NkMat4f::Translate({x, belowY, 0.f}) *
                           NkMat4f::Scale({0.55f, 0.55f, 0.55f});
            dc.aabb      = {{x - 0.35f, belowY - 0.55f, -0.35f},
                            {x + 0.35f, belowY + 0.55f,  0.35f}};
            if (st->mats[matIdx] && st->mats[matIdx]->IsValid())
                dc.material = st->mats[matIdx]->GetInstHandle();
            dc.tint      = kPalette[st->params[matIdx].colorIdx % kPaletteSize];
            dc.metallic  = st->params[matIdx].metallic;
            dc.roughness = st->params[matIdx].roughness;
            r3d->Submit(dc);
        }
    }

    // ── Phase M.8 : cube multi-materiaux (1 material par face) ───────────────
    {
        NkDrawCall3D dc;
        dc.mesh       = st->meshCube;
        dc.transform  = NkMat4f::Translate({0.f, 1.0f, -4.f}) *
                        NkMat4f::Scale({1.2f, 1.2f, 1.2f});
        dc.aabb       = {{-0.6f, 0.4f, -4.6f}, {0.6f, 1.6f, -3.4f}};
        dc.castShadow = true;
        for (int s = 0; s < 6; s++) {
            const int matIdx = s % 5;
            if (st->mats[matIdx] && st->mats[matIdx]->IsValid())
                dc.materialSlots.PushBack(st->mats[matIdx]->GetInstHandle());
            else
                dc.materialSlots.PushBack({});
        }
        r3d->Submit(dc);
    }

    // ── Phase M.3 : painted cube — blend layers via vertex color ─────────────
    // Le cube a vColor.r differents par face (3 a 255, 3 a 0). Material Layered
    // avec maskSource=0 -> mix(rust, paint, vColor.r) par fragment. Resultat :
    // 3 faces 100% peinture (bleue) + 3 faces 100% rouille (rouge-brun). Sans
    // toucher au material, juste en peignant le mesh.
    if (st->paintedMat && st->paintedMat->IsValid() && st->meshPaintedCube.IsValid()) {
        NkDrawCall3D dc;
        dc.mesh       = st->meshPaintedCube;
        dc.transform  = NkMat4f::Translate({4.5f, 1.0f, -2.f}) *
                        NkMat4f::Scale({1.0f, 1.0f, 1.0f});
        dc.aabb       = {{4.0f, 0.5f, -2.5f}, {5.0f, 1.5f, -1.5f}};
        dc.castShadow = true;
        dc.material   = st->paintedMat->GetInstHandle();
        r3d->Submit(dc);
    }

    // ── Phase M.6 v4 : texture cube (texture writable + UV atlas 2x3) ────────
    if (st->texMat && st->texMat->IsValid() && st->meshTexCube.IsValid()) {
        NkDrawCall3D dc;
        dc.mesh       = st->meshTexCube;
        dc.transform  = NkMat4f::Translate({-4.5f, 1.0f, -2.f}) *
                        NkMat4f::Scale({1.0f, 1.0f, 1.0f});
        dc.aabb       = {{-5.0f, 0.5f, -2.5f}, {-4.0f, 1.5f, -1.5f}};
        dc.castShadow = true;
        dc.material   = st->texMat->GetInstHandle();
        r3d->Submit(dc);
    }

    // ── Debug gizmos ─────────────────────────────────────────────────────────
    r3d->DrawDebugAxes(NkMat4f::Identity(), 0.5f);

    // Phase M.6 : marqueur visuel du dernier splash de Dynamic Paint.
    // DrawDebugSphere ne dessine qu'un cercle dans XY (souvent invisible),
    // on prefere un AABB cube wireframe qui ressort sous tous les angles.
    if (st->hasPainted) {
        const float32 mark = NkMax(0.15f, st->brushRadius * 0.5f);
        NkAABB box;
        box.min = {st->lastPaintPos.x - mark, st->lastPaintPos.y - mark, st->lastPaintPos.z - mark};
        box.max = {st->lastPaintPos.x + mark, st->lastPaintPos.y + mark, st->lastPaintPos.z + mark};
        r3d->DrawDebugAABB(box, {1.f, 0.2f, 0.8f, 1.f});  // magenta vif
    }

    // Sphere fil-de-fer verte autour de la sphere actuellement selectionnee
    {
        const float32 x = (float32)(st->activeMat - 2) * 2.4f;
        r3d->DrawDebugSphere({x, BobY(st->activeMat), 0.f}, 0.72f, {0.1f, 1.f, 0.2f, 1.f});
    }

    // ── Overlay texte ─────────────────────────────────────────────────────────
    if (auto* overlay = ctx.renderer->GetOverlay()) {
        overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
        overlay->DrawStats(ctx.renderer->GetStats());

        const MatParams& p  = st->params[st->activeMat];
        float32 paramVal = (st->activeMat <= 1) ? p.roughness : p.threshold;
        const char* paramName = (st->activeMat <= 1) ? "roughness" : "threshold";

        overlay->DrawText({20.f, 35.f},
            "Demo Materials5  |  API : %s  |  actif : %d (%s)",
            NkGraphicsApiName(ctx.api),
            st->activeMat + 1, st->matNames[st->activeMat]);
        overlay->DrawText({20.f, 55.f},
            "%s : %.2f   metallic : %.0f   outline : %.1f px",
            paramName, paramVal, p.metallic, p.outlineW);
        overlay->DrawText({20.f, 75.f},
            "couleur #%d   FPS : %.0f",
            p.colorIdx, dt > 1e-5f ? 1.f / dt : 0.f);
        overlay->DrawText({20.f, 95.f},
            "1-5:sel  +/-:%s  M:metal  C:color  O:outline",
            paramName);

        overlay->EndOverlay();
    }

    ctx.renderer->Present();
    ctx.renderer->EndFrame();
}

// ── Shutdown ──────────────────────────────────────────────────────────────────
void Demo5_Materials_Shutdown(DemoCtx& ctx) {
    auto* st = (Demo5MatState*)ctx.userData;
    if (st) {
        for (int i = 0; i < 5; i++) NkMaterial::Destroy(st->mats[i]);
        NkMaterial::Destroy(st->floorMat);
        NkMaterial::Destroy(st->paintedMat);
        NkMaterial::Destroy(st->texMat);
        // Le NkPlanarReflectionSystem est detruit par NkRendererImpl ;
        // pas besoin de Unregister explicitement.
        delete st;
    }
    ctx.userData = nullptr;
    logger.Info("[Demo5] Shutdown\n");
}

}} // namespace nkentseu::demo
