// =============================================================================
// Demo4_Materials.cpp  — Demo 3 : systeme de materiaux NkMaterial
//
// 5 spheres, chacune avec un materiau different. Les parametres des materiaux
// sont modifiables en temps reel via le clavier.
//
// Controles :
//   1-5     : selectionner le materiau actif (sphere mise en evidence)
//   +/=     : augmenter roughness (PBR 1-2) ou threshold (Toon/Anime 3-4)
//   -       : diminuer roughness ou threshold
//   M       : toggle metallic 0<->1 (PBR 1-2 uniquement)
//   C       : changer couleur albedo (cycle dans une palette)
//   O       : changer largeur outline (Toon/Anime 3-4 uniquement)
//   V       : toggle VSync
// =============================================================================
#include "DemoCommon.h"
#include "NKRenderer/Materials/NkMaterial.h"
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
struct Demo4MatState {
    NkMaterial*  mats[5]     = {};
    const char*  matNames[5] = {"PBR Metal", "PBR Plastic", "Toon", "Anime", "Unlit"};
    NkMeshHandle meshSphere;
    NkMeshHandle meshPlane;
    float32      angle       = 0.f;
    int          activeMat   = 0;
    MatParams    params[5];
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

// ── Init ──────────────────────────────────────────────────────────────────────
bool Demo4_Materials_Init(DemoCtx& ctx) {
    auto* st = new Demo4MatState();
    ctx.userData = st;

    // Meshes
    auto* meshSys = ctx.renderer->GetMeshSystem();
    if (!meshSys) {
        logger.Errorf("[Demo4] Pas de NkMeshSystem\n");
        delete st; ctx.userData = nullptr; return false;
    }
    st->meshSphere = meshSys->GetIcosphere();
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
        logger.Errorf("[Demo4] Pas de NkMaterialSystem\n");
        delete st; ctx.userData = nullptr; return false;
    }

    static const NkMaterialType kTypes[5] = {
        NkMaterialType::NK_PBR_METALLIC,
        NkMaterialType::NK_PBR_METALLIC,
        NkMaterialType::NK_TOON,
        NkMaterialType::NK_ANIME,
        NkMaterialType::NK_UNLIT,
    };

    for (int i = 0; i < 5; i++) {
        st->mats[i] = NkMaterial::Create(matSys, kTypes[i]);
        if (!st->mats[i] || !st->mats[i]->IsValid()) {
            logger.Warnf("[Demo4] Materiau [{0}] ({1}) invalide — fallback tint seul\n",
                         i, st->matNames[i]);
        } else {
            ApplyParams(st->mats[i], i, st->params[i]);
        }
    }

    // Controles clavier : modifications temps reel
    NkEvents().AddEventCallback<NkKeyPressEvent>([st](NkKeyPressEvent* e) {
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
            // VSync toggle
            case NkKey::NK_V: {
                static bool vsync = true;
                vsync = !vsync;
                // renderer n'est pas accessible ici — on log seulement
                logger.Info("[Demo4] VSync toggle (relancer avec --vsync=0 pour desactiver)\n");
                break;
            }
            default: break;
        }
    });

    logger.Info("[Demo4] Init OK — 5 materiaux crees\n");
    logger.Info("[Demo4] 1-5:selectionner  +/-:roughness/threshold  M:metallic  C:couleur  O:outline\n");
    return true;
}

// ── Frame ─────────────────────────────────────────────────────────────────────
void Demo4_Materials_Frame(DemoCtx& ctx, float32 dt) {
    auto* st = (Demo4MatState*)ctx.userData;
    st->angle += dt * 0.35f;

    if (!ctx.renderer->BeginFrame()) return;

    auto* r3d = ctx.renderer->GetRender3D();
    if (!r3d) {
        ctx.renderer->Present();
        ctx.renderer->EndFrame();
        return;
    }

    // ── Camera orbite autour de l'origine ────────────────────────────────────
    const float32 camDist = 9.f;
    NkCamera3DData camData;
    camData.position  = {cosf(st->angle) * camDist, 3.0f, sinf(st->angle) * camDist};
    camData.target    = {0.f, 0.5f, 0.f};
    camData.up        = {0.f, 1.f, 0.f};
    camData.fovY      = 55.f;
    camData.aspect    = (float32)ctx.width / (float32)ctx.height;
    camData.nearPlane = 0.1f;
    camData.farPlane  = 100.f;
    NkCamera3D cam(camData);

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

    r3d->BeginScene(sctx);

    // ── Sol ──────────────────────────────────────────────────────────────────
    {
        NkDrawCall3D dc;
        dc.mesh       = st->meshPlane;
        dc.transform  = NkMat4f::Scale({14.f, 1.f, 14.f});
        dc.aabb       = {{-7.f, -0.01f, -7.f}, {7.f, 0.01f, 7.f}};
        dc.castShadow = false;
        r3d->Submit(dc);
    }

    // ── 5 spheres avec leur materiau ──────────────────────────────────────────
    for (int i = 0; i < 5; i++) {
        const float32 x = (float32)(i - 2) * 2.4f;

        NkDrawCall3D dc;
        dc.mesh      = st->meshSphere;
        dc.transform = NkMat4f::Translate({x, 0.6f, 0.f}) *
                       NkMat4f::Scale({0.55f, 0.55f, 0.55f});
        dc.aabb      = {{x - 0.35f, 0.2f, -0.35f}, {x + 0.35f, 1.0f, 0.35f}};

        // Branche materiau systeme si disponible
        if (st->mats[i] && st->mats[i]->IsValid())
            dc.material = st->mats[i]->GetInstHandle();

        // Tint + override PBR fallback si materiau invalide
        dc.tint      = kPalette[st->params[i].colorIdx % kPaletteSize];
        dc.metallic  = st->params[i].metallic;
        dc.roughness = st->params[i].roughness;

        r3d->Submit(dc);
    }

    // ── Debug gizmos ─────────────────────────────────────────────────────────
    r3d->DrawDebugAxes(NkMat4f::Identity(), 0.5f);

    // Sphere fil-de-fer verte autour de la sphere actuellement selectionnee
    {
        const float32 x = (float32)(st->activeMat - 2) * 2.4f;
        r3d->DrawDebugSphere({x, 0.6f, 0.f}, 0.72f, {0.1f, 1.f, 0.2f, 1.f});
    }

    // ── Overlay texte ─────────────────────────────────────────────────────────
    if (auto* overlay = ctx.renderer->GetOverlay()) {
        overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
        overlay->DrawStats(ctx.renderer->GetStats());

        const MatParams& p  = st->params[st->activeMat];
        float32 paramVal = (st->activeMat <= 1) ? p.roughness : p.threshold;
        const char* paramName = (st->activeMat <= 1) ? "roughness" : "threshold";

        overlay->DrawText({20.f, 35.f},
            "Demo Materials  |  actif : %d (%s)",
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
void Demo4_Materials_Shutdown(DemoCtx& ctx) {
    auto* st = (Demo4MatState*)ctx.userData;
    if (st) {
        for (int i = 0; i < 5; i++) NkMaterial::Destroy(st->mats[i]);
        delete st;
    }
    ctx.userData = nullptr;
    logger.Info("[Demo4] Shutdown\n");
}

}} // namespace nkentseu::demo
