// =============================================================================
// Demo6_HierarchicalMaterials.cpp  — M.4 : Hierarchical Material Instances
//
// Demontre le mecanisme parent/enfants des materiaux (style UE5 MID + Parent) :
//   - 1 sphere PARENT au centre (PBR Metal, couleur cyclable au clavier).
//   - 3 spheres ENFANTS autour du parent :
//       * child A (gauche)  : herite TOUT  → suit le parent en live
//       * child B (droite)  : override albedo (toujours bleu, ignore parent)
//       * child C (avant)   : override metallic+roughness (mat plastique fixe)
//                             mais suit la couleur du parent (heritage partiel)
//   - 1 petit-enfant (small ball au-dessus du child A) qui herite de child A
//     (= profondeur 2 dans la hierarchie).
//
// Controles :
//   1-7  : cycle couleur albedo du parent (propage a child A et C, pas B)
//   M    : toggle parent->metallic (0/1) (propage a A et B, pas C)
//   R    : reset override albedo sur child B (re-link au parent)
//   Y    : reset override metallic+roughness sur child C
//   V    : VSync log only
//
// Camera : LEFT-drag rotate | wheel zoom | WASD/fleches pan | T:auto-orbit
// =============================================================================
#include "DemoCommon.h"
#include "DemoCamera.h"
#include "NKRenderer/Materials/NkMaterial.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include <cmath>

namespace nkentseu { namespace demo {

// ── Palette de couleurs pour le cycle parent ──────────────────────────────────
static const NkVec3f kPalette6[] = {
    {0.90f, 0.10f, 0.10f},  // rouge
    {1.00f, 0.78f, 0.20f},  // or
    {0.10f, 0.80f, 0.30f},  // vert
    {0.65f, 0.10f, 0.85f},  // violet
    {1.00f, 0.45f, 0.00f},  // orange
    {0.90f, 0.90f, 0.90f},  // blanc
    {0.95f, 0.40f, 0.70f},  // rose
};
static constexpr int kPaletteSize6 = (int)(sizeof(kPalette6) / sizeof(kPalette6[0]));

// ── Etat de la demo ──────────────────────────────────────────────────────────
struct Demo6HierState {
    NkMaterial*  parent  = nullptr;
    NkMaterial*  childA  = nullptr;   // herite tout
    NkMaterial*  childB  = nullptr;   // override albedo
    NkMaterial*  childC  = nullptr;   // override metallic + roughness
    NkMaterial*  grand   = nullptr;   // petit-enfant de childA

    NkMeshHandle meshSphere;
    NkMeshHandle meshPlane;
    DemoCamera   camera;

    int     colorIdx       = 0;       // index dans kPalette6 pour le parent
    float32 parentMetallic = 0.8f;
};

// ── Init ──────────────────────────────────────────────────────────────────────
bool Demo6_HierarchicalMaterials_Init(DemoCtx& ctx) {
    auto* st = new Demo6HierState();
    ctx.userData = st;

    auto* meshSys = ctx.renderer->GetMeshSystem();
    auto* matSys  = ctx.renderer->GetMaterials();
    if (!meshSys || !matSys) {
        logger.Errorf("[Demo6] MeshSystem/MaterialSystem manquant\n");
        delete st; ctx.userData = nullptr; return false;
    }
    st->meshSphere = meshSys->GetIcosphere();
    st->meshPlane  = meshSys->GetPlane();

    // ── Materiau PARENT ────────────────────────────────────────────────────
    st->parent = NkMaterial::Create(matSys, NkMaterialType::NK_PBR_METALLIC);
    if (!st->parent || !st->parent->IsValid()) {
        logger.Errorf("[Demo6] Parent invalide\n");
        delete st; ctx.userData = nullptr; return false;
    }
    st->parent->SetAlbedo(kPalette6[0])
              ->SetMetallic(st->parentMetallic)
              ->SetRoughness(0.35f);

    // ── Enfants ───────────────────────────────────────────────────────────
    st->childA = NkMaterial::CreateChild(st->parent);    // herite tout

    st->childB = NkMaterial::CreateChild(st->parent);    // override albedo
    if (st->childB) {
        st->childB->SetAlbedo({0.10f, 0.35f, 0.90f});    // bleu fixe
    }

    st->childC = NkMaterial::CreateChild(st->parent);    // override mat+rough
    if (st->childC) {
        st->childC->SetMetallic(0.f)                     // plastique
                  ->SetRoughness(0.85f);                 // tres rugueux
    }

    // Petit-enfant : enfant de childA. Tout suit la chaine parent->childA->grand.
    if (st->childA)
        st->grand = NkMaterial::CreateChild(st->childA);

    // ── Inputs clavier ────────────────────────────────────────────────────
    auto* state = st;   // capture pointer stable
    NkEvents().AddEventCallback<NkKeyPressEvent>([state](NkKeyPressEvent* e) {
        if (!e) return;
        switch (e->GetKey()) {
            // 1-7 : cycle couleur parent
            case NkKey::NK_NUM1: case NkKey::NK_NUM2: case NkKey::NK_NUM3:
            case NkKey::NK_NUM4: case NkKey::NK_NUM5: case NkKey::NK_NUM6:
            case NkKey::NK_NUM7: {
                const int idx = (int)e->GetKey() - (int)NkKey::NK_NUM1;
                if (idx >= 0 && idx < kPaletteSize6) {
                    state->colorIdx = idx;
                    if (state->parent) state->parent->SetAlbedo(kPalette6[idx]);
                }
                break;
            }
            // M : toggle metallic du parent (propage a A et au grand-enfant via A)
            case NkKey::NK_M: {
                state->parentMetallic = (state->parentMetallic > 0.5f) ? 0.f : 1.f;
                if (state->parent) state->parent->SetMetallic(state->parentMetallic);
                break;
            }
            // R : reset override albedo sur childB (re-link au parent)
            case NkKey::NK_R: {
                if (state->childB) state->childB->ResetPBROverride(NK_PBR_O_ALBEDO);
                logger.Info("[Demo6] childB reset albedo -> follow parent\n");
                break;
            }
            // Y : reset override metallic+roughness sur childC
            // (T est deja utilise par DemoCamera pour toggle auto-orbit)
            case NkKey::NK_Y: {
                if (state->childC) {
                    state->childC->ResetPBROverride(NK_PBR_O_METALLIC);
                    state->childC->ResetPBROverride(NK_PBR_O_ROUGHNESS);
                }
                logger.Info("[Demo6] childC reset metallic+roughness -> follow parent\n");
                break;
            }
            default: break;
        }
    });

    // ── Camera orbit ──────────────────────────────────────────────────────
    st->camera.Controller().SetCenter({0.f, 0.6f, 0.f}, 7.5f, 0.f, -0.25f);
    st->camera.Controller().SetAutoOrbit(false);
    st->camera.InstallEvents();

    logger.Info("[Demo6] === M.4 Hierarchical Material Instances ===\n");
    logger.Info("[Demo6] Parent (centre) | child A (gauche, herite tout) |\n");
    logger.Info("[Demo6]                   child B (droite, override albedo) |\n");
    logger.Info("[Demo6]                   child C (avant, override metal+rough) |\n");
    logger.Info("[Demo6]                   grand (au-dessus A, enfant de A)\n");
    logger.Info("[Demo6] 1-7:couleur parent  M:metal  R:reset B  Y:reset C\n");
    return true;
}

// ── Frame ─────────────────────────────────────────────────────────────────────
void Demo6_HierarchicalMaterials_Frame(DemoCtx& ctx, float32 dt) {
    auto* st = (Demo6HierState*)ctx.userData;
    st->camera.Update(dt);

    if (!ctx.renderer->BeginFrame()) return;

    auto* r3d = ctx.renderer->GetRender3D();
    if (!r3d) {
        ctx.renderer->Present();
        ctx.renderer->EndFrame();
        return;
    }

    // Camera
    NkCamera3DData camData;
    camData.up        = {0.f, 1.f, 0.f};
    camData.fovY      = 55.f;
    camData.aspect    = (float32)ctx.width / (float32)ctx.height;
    camData.nearPlane = 0.1f;
    camData.farPlane  = 100.f;
    NkCamera3D cam(camData);
    st->camera.Controller().Apply(cam);

    NkSceneContext sctx;
    sctx.camera = cam;
    sctx.time   = ctx.totalTime;

    // Lumieres
    NkLightDesc sun;
    sun.type       = NkLightType::NK_DIRECTIONAL;
    sun.direction  = {-0.5f, -1.f, -0.4f};
    sun.color      = {1.f, 0.95f, 0.9f};
    sun.intensity  = 3.5f;
    sun.castShadow = true;
    sctx.lights.PushBack(sun);

    NkLightDesc fill;
    fill.type      = NkLightType::NK_POINT;
    fill.position  = {0.f, 4.f, 4.f};
    fill.color     = {0.4f, 0.5f, 0.9f};
    fill.intensity = 2.5f;
    fill.range     = 20.f;
    sctx.lights.PushBack(fill);

    sctx.ambientIntensity = 0.12f;

    r3d->BeginScene(sctx);

    // ── Sol gris simple ──────────────────────────────────────────────────
    {
        NkDrawCall3D dc;
        dc.mesh       = st->meshPlane;
        dc.transform  = NkMat4f::Scale({14.f, 1.f, 14.f});
        dc.aabb       = {{-7.f, -0.01f, -7.f}, {7.f, 0.01f, 7.f}};
        dc.castShadow = false;
        dc.tint       = {0.55f, 0.55f, 0.55f};
        r3d->Submit(dc);
    }

    // Helper local pour submit une sphere a un (x, y, z) avec un material.
    // Lit l'albedo/metallic/roughness LIVE depuis le mat instance et les
    // copie sur le drawcall — important pour M.4 : le shader PBR utilise
    // dc.tint/metallic/roughness comme override per-object, donc sans ce
    // pont les changements de parent->SetAlbedo() ne se voient pas.
    auto* matSys = ctx.renderer->GetMaterials();
    auto SubmitSphere = [&](NkMaterial* mat, NkVec3f pos, float32 scale) {
        if (!mat || !mat->IsValid()) return;
        NkDrawCall3D dc;
        dc.mesh      = st->meshSphere;
        dc.transform = NkMat4f::Translate(pos) *
                       NkMat4f::Scale({scale, scale, scale});
        const float32 r = scale * 1.05f;
        dc.aabb      = {{pos.x - r, pos.y - r, pos.z - r},
                        {pos.x + r, pos.y + r, pos.z + r}};
        dc.material  = mat->GetInstHandle();
        // Pont mat->drawcall pour rendre les setters visibles
        if (auto* inst = matSys->GetInstance(mat->GetInstHandle())) {
            const auto& pbr = inst->GetPBR();
            dc.tint       = {pbr.albedo.x, pbr.albedo.y, pbr.albedo.z};
            dc.alpha      = pbr.albedo.w;
            dc.metallic   = pbr.metallic;
            dc.roughness  = pbr.roughness;
        }
        r3d->Submit(dc);
    };

    // Parent au centre, enfants autour (gauche / droite / avant) :
    SubmitSphere(st->parent, {0.f,  1.0f,  0.f}, 0.65f);   // parent (gros)
    SubmitSphere(st->childA, {-2.2f, 1.0f, 0.f}, 0.50f);   // A : gauche
    SubmitSphere(st->childB, { 2.2f, 1.0f, 0.f}, 0.50f);   // B : droite
    SubmitSphere(st->childC, {0.f,   1.0f, 2.2f}, 0.50f);  // C : avant
    SubmitSphere(st->grand,  {-2.2f, 1.9f, 0.f}, 0.30f);   // grand : sur A

    // Repere
    r3d->DrawDebugAxes(NkMat4f::Identity(), 0.5f);

    // Overlay
    if (auto* overlay = ctx.renderer->GetOverlay()) {
        overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
        overlay->DrawStats(ctx.renderer->GetStats());

        const NkVec3f col = kPalette6[st->colorIdx % kPaletteSize6];

        overlay->DrawText({20.f, 35.f},
            "Demo6 M.4 Hierarchical  |  API : %s",
            NkGraphicsApiName(ctx.api));
        overlay->DrawText({20.f, 55.f},
            "parent : albedo=#%d (%.2f, %.2f, %.2f)  metal=%.0f  rough=0.35",
            st->colorIdx, col.x, col.y, col.z, st->parentMetallic);
        overlay->DrawText({20.f, 75.f},
            "A (gauche)=herite tout  |  B (droite)=override albedo (bleu)");
        overlay->DrawText({20.f, 95.f},
            "C (avant)=override metal+rough (plastique)  |  grand=enfant de A");
        overlay->DrawText({20.f, 115.f},
            "1-7:couleur parent  M:metal  R:reset B  Y:reset C");
        overlay->DrawText({20.f, 135.f},
            "FPS : %.0f", dt > 1e-5f ? 1.f / dt : 0.f);

        overlay->EndOverlay();
    }

    ctx.renderer->Present();
    ctx.renderer->EndFrame();
}

// ── Shutdown ──────────────────────────────────────────────────────────────────
void Demo6_HierarchicalMaterials_Shutdown(DemoCtx& ctx) {
    auto* st = (Demo6HierState*)ctx.userData;
    if (!st) return;
    // Detruire dans l'ordre enfants -> parent pour eviter dangling refs.
    NkMaterial::Destroy(st->grand);
    NkMaterial::Destroy(st->childA);
    NkMaterial::Destroy(st->childB);
    NkMaterial::Destroy(st->childC);
    NkMaterial::Destroy(st->parent);
    delete st;
    ctx.userData = nullptr;
    logger.Info("[Demo6] Shutdown\n");
}

}} // namespace nkentseu::demo
