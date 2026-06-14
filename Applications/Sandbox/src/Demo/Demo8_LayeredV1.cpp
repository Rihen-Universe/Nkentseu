// =============================================================================
// Demo8_LayeredV1.cpp  — M.1 v1 : Material Layering N=8 layers
//
// 1 sphere avec un material LayeredV1 empilant jusqu'a 8 couches PBR
// simplifiees (albedo + metallic + roughness). Chaque couche est blendee
// au-dessus de la pile via un masque parametrique :
//   - vColor.r/g/b/a, vUV.x/y : varient sur la surface
//   - constant                : valeur fixe (utile pour mix uniforme)
//   - layer.albedo.a          : auto-mask via l'alpha de la couche
//
// Setup initial (8 couches) :
//   layer 0 = base sombre (toujours visible)
//   layer 1 = rouge,   mask = vUV.x        (degrade horizontal)
//   layer 2 = vert,    mask = vUV.y        (degrade vertical)
//   layer 3 = bleu,    mask = vColor.r     (depend du sommet)
//   layer 4 = jaune,   mask = constant 0.3 (overlay uniforme 30%)
//   layer 5 = magenta, mask = vUV.x  k=0.5 (autre degrade)
//   layer 6 = cyan,    mask = vUV.y  k=0.5
//   layer 7 = blanc,   mask = layer.alpha (depend de albedo.a)
//
// Touches :
//   1-8 : numLayers = 1..8 (revele les couches progressivement)
//   R   : reset config initiale
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

struct Demo8State {
    NkMaterial*       mat = nullptr;
    NkMeshHandle      meshSphere;
    NkMeshHandle      meshPlane;
    DemoCamera        camera;
    int               numLayers = 8;
    NkLayeredV1Params initial;   // pour reset
};

// Construit la config initiale des 8 couches (cf. entete fichier).
static NkLayeredV1Params BuildInitialLayers() {
    NkLayeredV1Params p;
    p.numLayers = 8;

    auto MakeLayer = [](NkVec4f albedo, float met, float rough) {
        NkPBRLayer L;
        L.albedo    = albedo;
        L.metallic  = met;
        L.roughness = rough;
        return L;
    };

    // IMPORTANT : si albedo.a est non nul ET que le mask source = LAYER_ALPHA,
    // le layer ecrase tout. On utilise albedo.a < 1 pour les layers qui le
    // permettent, ou bien on utilise un autre mask (UV / vColor / constant).
    p.layers[0] = MakeLayer({0.10f, 0.10f, 0.12f, 1.f}, 0.0f, 0.7f);   // base sombre
    p.layers[1] = MakeLayer({0.90f, 0.15f, 0.10f, 1.f}, 0.0f, 0.5f);   // rouge
    p.layers[2] = MakeLayer({0.15f, 0.85f, 0.20f, 1.f}, 0.0f, 0.5f);   // vert
    p.layers[3] = MakeLayer({0.20f, 0.30f, 0.95f, 1.f}, 0.0f, 0.5f);   // bleu
    p.layers[4] = MakeLayer({0.95f, 0.85f, 0.10f, 1.f}, 0.6f, 0.3f);   // jaune metal
    p.layers[5] = MakeLayer({0.95f, 0.15f, 0.85f, 1.f}, 0.0f, 0.6f);   // magenta
    p.layers[6] = MakeLayer({0.10f, 0.85f, 0.90f, 1.f}, 0.4f, 0.4f);   // cyan
    // Layer 7 = highlight blanc 25% (constant mask faible pour eviter ecrase)
    p.layers[7] = MakeLayer({1.00f, 1.00f, 1.00f, 1.f}, 0.0f, 0.2f);   // blanc

    // maskSources : layer 0 ignore (base), 1..7 cf. entete
    p.maskSources0[1] = (int32)NK_LAYER_MASK_UV_X;       // layer 1 rouge
    p.maskSources0[2] = (int32)NK_LAYER_MASK_UV_Y;       // layer 2 vert
    p.maskSources0[3] = (int32)NK_LAYER_MASK_VCOLOR_R;   // layer 3 bleu
    p.maskSources1[0] = (int32)NK_LAYER_MASK_CONSTANT;   // layer 4 jaune (idx 0 dans masks1)
    p.maskSources1[1] = (int32)NK_LAYER_MASK_UV_X;       // layer 5 magenta
    p.maskSources1[2] = (int32)NK_LAYER_MASK_UV_Y;       // layer 6 cyan
    p.maskSources1[3] = (int32)NK_LAYER_MASK_CONSTANT;   // layer 7 blanc highlight

    // maskConstants : useful quand source == CONSTANT
    p.maskConstants1[0] = 0.3f;    // layer 4 jaune : overlay 30%
    p.maskConstants1[3] = 0.25f;   // layer 7 blanc : overlay 25%

    return p;
}

bool Demo8_LayeredV1_Init(DemoCtx& ctx) {
    auto* st = new Demo8State();
    ctx.userData = st;

    auto* meshSys = ctx.renderer->GetMeshSystem();
    auto* matSys  = ctx.renderer->GetMaterials();
    if (!meshSys || !matSys) {
        logger.Errorf("[Demo8] MeshSystem/MaterialSystem manquant\n");
        delete st; ctx.userData = nullptr; return false;
    }
    st->meshSphere = meshSys->GetSphere();
    st->meshPlane  = meshSys->GetPlane();

    // Material : utilise le template builtin "Default_LayeredV1".
    st->mat = NkMaterial::Create(matSys, NkMaterialType::NK_LAYERED_V1);
    if (!st->mat || !st->mat->IsValid()) {
        logger.Errorf("[Demo8] LayeredV1 invalide\n");
        delete st; ctx.userData = nullptr; return false;
    }

    st->initial = BuildInitialLayers();
    // Pousse les params via l'inst (NkMaterial n'a pas de SetLayeredV1 publique).
    if (auto* inst = matSys->GetInstance(st->mat->GetInstHandle())) {
        inst->SetLayeredV1(st->initial);
    }

    // Inputs clavier : 1-8 -> numLayers, R -> reset
    auto* state = st;
    auto* matSysCap = matSys;
    NkEvents().AddEventCallback<NkKeyPressEvent>([state, matSysCap](NkKeyPressEvent* e) {
        if (!e || !state->mat) return;
        auto* inst = matSysCap->GetInstance(state->mat->GetInstHandle());
        if (!inst) return;
        const NkKey k = e->GetKey();
        if (k >= NkKey::NK_NUM1 && k <= NkKey::NK_NUM8) {
            int n = (int)k - (int)NkKey::NK_NUM1 + 1;
            state->numLayers = n;
            inst->SetLayerV1Count(n);
            logger.Info("[Demo8] numLayers = {0}\n", n);
        } else if (k == NkKey::NK_R) {
            inst->SetLayeredV1(state->initial);
            state->numLayers = state->initial.numLayers;
            logger.Info("[Demo8] reset config initiale\n");
        }
    });

    // Camera orbit : pitch POSITIF = camera plus haute (y = dist * sin(pitch)).
    // +0.45 rad (~+26°) -> camera a y = 4.5 * sin(0.45) ~ 1.95 au-dessus du
    // target (Y=0.9), donc bien au-dessus du sol Y=0. Regarde vers le bas/
    // vers le target.
    st->camera.Controller().SetCenter({0.f, 0.9f, 0.f}, 4.5f, 0.f, 0.45f);
    st->camera.Controller().SetAutoOrbit(false);
    st->camera.InstallEvents();

    logger.Info("[Demo8] === M.1 v1 Material Layering N=8 ===\n");
    logger.Info("[Demo8] 1-8 : numLayers (1=base seule, 8=toutes les couches)\n");
    logger.Info("[Demo8] R   : reset config initiale\n");
    return true;
}

void Demo8_LayeredV1_Frame(DemoCtx& ctx, float32 dt) {
    auto* st = (Demo8State*)ctx.userData;
    st->camera.Update(dt);

    if (!ctx.renderer->BeginFrame()) return;
    auto* r3d = ctx.renderer->GetRender3D();
    if (!r3d) { ctx.renderer->Present(); ctx.renderer->EndFrame(); return; }

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

    NkLightDesc sun;
    sun.type       = NkLightType::NK_DIRECTIONAL;
    sun.direction  = {-0.3f, -1.f, -0.4f};
    sun.color      = {1.f, 0.95f, 0.9f};
    sun.intensity  = 2.5f;
    sun.castShadow = false;
    sctx.lights.PushBack(sun);
    sctx.ambientIntensity = 0.15f;

    r3d->BeginScene(sctx);

    // Sol gris simple
    {
        NkDrawCall3D dc;
        dc.mesh       = st->meshPlane;
        dc.transform  = NkMat4f::Scale({10.f, 1.f, 10.f});
        dc.aabb       = {{-5.f, -0.01f, -5.f}, {5.f, 0.01f, 5.f}};
        dc.castShadow = false;
        dc.tint       = {0.55f, 0.55f, 0.55f};
        r3d->Submit(dc);
    }

    // Sphere LayeredV1
    {
        NkDrawCall3D dc;
        dc.mesh       = st->meshSphere;
        dc.transform  = NkMat4f::Translate({0.f, 0.9f, 0.f}) *
                        NkMat4f::Scale({0.85f, 0.85f, 0.85f});
        dc.aabb       = {{-0.9f, 0.0f, -0.9f}, {0.9f, 1.8f, 0.9f}};
        dc.tint       = {1.f, 1.f, 1.f};
        if (st->mat && st->mat->IsValid())
            dc.material = st->mat->GetInstHandle();
        r3d->Submit(dc);
    }

    r3d->DrawDebugAxes(NkMat4f::Identity(), 0.5f);

    if (auto* overlay = ctx.renderer->GetOverlay()) {
        overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
        overlay->DrawStats(ctx.renderer->GetStats());
        overlay->DrawText({20.f, 35.f},
            "Demo8 M.1 v1 Layered N=%d  |  API : %s",
            st->numLayers, NkGraphicsApiName(ctx.api));
        overlay->DrawText({20.f, 55.f},
            "Layers : 0=base 1=red(UV.x) 2=green(UV.y) 3=blue(vColor.r) 4=yellow(const 0.3)");
        overlay->DrawText({20.f, 75.f},
            "         5=magenta(UV.x) 6=cyan(UV.y) 7=white(const 0.25)");
        overlay->DrawText({20.f, 95.f},
            "1-8:numLayers  R:reset");
        overlay->DrawText({20.f, 115.f},
            "FPS : %.0f", dt > 1e-5f ? 1.f / dt : 0.f);
        overlay->EndOverlay();
    }

    ctx.renderer->Present();
    ctx.renderer->EndFrame();
}

void Demo8_LayeredV1_Shutdown(DemoCtx& ctx) {
    auto* st = (Demo8State*)ctx.userData;
    if (!st) return;
    NkMaterial::Destroy(st->mat);
    delete st;
    ctx.userData = nullptr;
    logger.Info("[Demo8] Shutdown\n");
}

}} // namespace nkentseu::demo
