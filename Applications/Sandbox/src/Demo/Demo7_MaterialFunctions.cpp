// =============================================================================
// Demo7_MaterialFunctions.cpp  — M.5 : Material Functions (#include "Include/foo.glsli")
//
// Demontre le preprocesseur d'include shader (NkShaderIncludeResolver) :
//   - 4 .glsli builtins dans Resources/NKRenderer/Shaders/Include/ :
//       NkFresnel.glsli   : Fresnel-Schlick (scalaire + vec3 + roughness)
//       NkToonRamp.glsli  : ramps 2-step / 3-step / N-step
//       NkColor.glsli     : sRGB<->Lin, saturation, HSV<->RGB
//       NkNoise.glsli     : hash + value-noise + FBM
//   - 1 shader custom "M5Demo" qui combine les 4 includes pour produire un
//     effet visible : sphere HSV-cyclante + FBM modulation + Toon ramp + Fresnel halo.
//
// Pas de touche dediee : l'effet est entierement procedural (cycle temporel
// via uCam.time). Confirmer visuellement :
//   - la couleur de la sphere cycle a travers l'arc-en-ciel
//   - on voit des "veines" de bruit FBM
//   - le bord est entoure d'un halo cyan (Fresnel)
//   - les zones d'ombre sont quantisees (Toon ramp 2-step)
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

struct Demo7FnState {
    NkMatHandle   tmpl;             // template "M5Demo" (custom, hint LoadOrCompileVF)
    NkMaterial*   mat = nullptr;    // instance
    NkMeshHandle  meshSphere;
    NkMeshHandle  meshPlane;
    DemoCamera    camera;

    // Params halo modifiables au clavier (mapped vers NkPBRParams emissive)
    NkVec3f       haloColor     = {1.0f, 1.0f, 0.95f};
    float32       haloIntensity = 2.5f;
    float32       rimPower      = 0.25f;   // -> roughness UBO
    int           haloColorIdx  = 0;
    // Tinte materiau (mapped vers albedo.rgb + albedo.a)
    NkVec3f       tintColor     = {1.f, 1.f, 1.f};
    float32       tintMix       = 0.f;     // 0 = cycle HSV pur, 1 = tinte pure
};

// Palette pour cycler la couleur du halo
static const NkVec3f kHaloPalette[] = {
    {1.0f, 1.0f, 0.95f},   // blanc chaud
    {1.0f, 0.4f, 0.2f},    // orange
    {0.4f, 0.85f, 1.0f},   // cyan
    {1.0f, 0.5f, 0.95f},   // magenta
    {0.4f, 1.0f, 0.5f},    // vert
};
static constexpr int kHaloPaletteSize = (int)(sizeof(kHaloPalette) / sizeof(kHaloPalette[0]));

bool Demo7_MaterialFunctions_Init(DemoCtx& ctx) {
    auto* st = new Demo7FnState();
    ctx.userData = st;

    auto* meshSys = ctx.renderer->GetMeshSystem();
    auto* matSys  = ctx.renderer->GetMaterials();
    if (!meshSys || !matSys) {
        logger.Errorf("[Demo7] MeshSystem/MaterialSystem manquant\n");
        delete st; ctx.userData = nullptr; return false;
    }
    st->meshSphere = meshSys->GetSphere();
    st->meshPlane  = meshSys->GetPlane();

    // ── Enregistrement du template M5Demo via "hint" ─────────────────────
    // Convention NkMaterialSystem : vertSrcGL = nom du dossier (= "M5Demo"),
    // fragSrcGL vide → LoadOrCompileVF charge depuis Resources/.../M5Demo/VK/
    // m5demo.{vert,frag}.vk.glsl. Le preprocesseur include est applique
    // dans ReadFile() avant la compilation, donc les #include "Include/*.glsli"
    // sont resolus automatiquement.
    NkMaterialTemplateDesc d;
    d.name       = "M5Demo";
    d.type       = NkMaterialType::NK_CUSTOM;
    d.queue      = NkRenderQueue::NK_OPAQUE;
    d.blendMode  = NkBlendMode::NK_OPAQUE;
    d.cullMode   = renderer::NkCullMode::NK_BACK;   // disambiguer vs nkentseu::NkCullMode du RHI
    d.depthTest  = true;
    d.depthWrite = true;
    d.vertSrcGL  = "M5Demo";   // hint -> LoadOrCompileVF("M5Demo")
    // fragSrcGL volontairement vide pour declencher le mode "hint"
    st->tmpl = matSys->RegisterTemplate(d);

    if (!st->tmpl.IsValid()) {
        logger.Errorf("[Demo7] Template 'M5Demo' invalide\n");
        delete st; ctx.userData = nullptr; return false;
    }

    st->mat = NkMaterial::Create(matSys, "M5Demo");
    if (!st->mat || !st->mat->IsValid()) {
        logger.Errorf("[Demo7] Material 'M5Demo' invalide\n");
        delete st; ctx.userData = nullptr; return false;
    }

    // Applique les params halo initiaux via l'API standard NkMaterial.
    // Mapping shader :
    //   SetEmissive(color, strength) -> uMat.emissive.rgb + uMat.emissiveStrength
    //   SetRoughness(v)              -> uMat.roughness (= rim power factor)
    //   SetAlbedo(tint, a)           -> uMat.albedo (a = mix HSV cycle <-> tinte)
    st->mat->SetEmissive(st->haloColor, st->haloIntensity);
    st->mat->SetRoughness(st->rimPower);
    st->mat->SetAlbedo(st->tintColor, st->tintMix);

    // Inputs clavier : I/K pour intensite, U/J pour rim power, H pour cycler
    // la couleur du halo, T2 (tint mix) en attendant la palette tinte plus tard.
    auto* state = st;
    NkEvents().AddEventCallback<NkKeyPressEvent>([state](NkKeyPressEvent* e) {
        if (!e || !state->mat) return;
        switch (e->GetKey()) {
            case NkKey::NK_I:  // augmente intensite halo
                state->haloIntensity = NkMin(10.f, state->haloIntensity + 0.5f);
                state->mat->SetEmissive(state->haloColor, state->haloIntensity);
                logger.Info("[Demo7] haloIntensity = {0}\n", state->haloIntensity);
                break;
            case NkKey::NK_K:  // diminue intensite halo
                state->haloIntensity = NkMax(0.f, state->haloIntensity - 0.5f);
                state->mat->SetEmissive(state->haloColor, state->haloIntensity);
                logger.Info("[Demo7] haloIntensity = {0}\n", state->haloIntensity);
                break;
            case NkKey::NK_U:  // augmente rim power (halo plus concentre)
                state->rimPower = NkMin(1.f, state->rimPower + 0.05f);
                state->mat->SetRoughness(state->rimPower);
                logger.Info("[Demo7] rimPower = {0}\n", state->rimPower);
                break;
            case NkKey::NK_J:  // diminue rim power (halo plus diffus)
                state->rimPower = NkMax(0.f, state->rimPower - 0.05f);
                state->mat->SetRoughness(state->rimPower);
                logger.Info("[Demo7] rimPower = {0}\n", state->rimPower);
                break;
            case NkKey::NK_H:  // cycle couleur halo
                state->haloColorIdx = (state->haloColorIdx + 1) % kHaloPaletteSize;
                state->haloColor = kHaloPalette[state->haloColorIdx];
                state->mat->SetEmissive(state->haloColor, state->haloIntensity);
                logger.Info("[Demo7] haloColor cycle -> #{0}\n", state->haloColorIdx);
                break;
            case NkKey::NK_NUM1:  // tintMix = 0 (cycle HSV pur)
                state->tintMix = 0.f;
                state->mat->SetAlbedo(state->tintColor, state->tintMix);
                break;
            case NkKey::NK_NUM2:  // tintMix = 1 (tinte fixe pure)
                state->tintMix = 1.f;
                state->mat->SetAlbedo(state->tintColor, state->tintMix);
                break;
            default: break;
        }
    });

    // Camera orbit
    st->camera.Controller().SetCenter({0.f, 0.6f, 0.f}, 4.0f, 0.f, -0.15f);
    st->camera.Controller().SetAutoOrbit(false);
    st->camera.InstallEvents();

    logger.Info("[Demo7] === M.5 Material Functions ===\n");
    logger.Info("[Demo7] Sphere utilisant 4 .glsli builtins : Fresnel + ToonRamp + Color + Noise\n");
    logger.Info("[Demo7] I/K intensite halo | U/J rim power | H cycle couleur | 1/2 cycle HSV / tinte pure\n");
    return true;
}

void Demo7_MaterialFunctions_Frame(DemoCtx& ctx, float32 dt) {
    auto* st = (Demo7FnState*)ctx.userData;
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

    // Lumiere dir simple (le shader M5Demo a sa propre lumiere "fake" pour Toon
    // ramp mais on garde une lumiere globale pour d'eventuels autres objets).
    NkLightDesc sun;
    sun.type       = NkLightType::NK_DIRECTIONAL;
    sun.direction  = {-0.3f, -1.f, -0.4f};
    sun.color      = {1.f, 0.95f, 0.9f};
    sun.intensity  = 2.5f;
    sun.castShadow = false;
    sctx.lights.PushBack(sun);
    sctx.ambientIntensity = 0.15f;

    r3d->BeginScene(sctx);

    // ── Sol gris simple (PBR par defaut, juste pour ancrage visuel) ─────
    {
        NkDrawCall3D dc;
        dc.mesh       = st->meshPlane;
        dc.transform  = NkMat4f::Scale({10.f, 1.f, 10.f});
        dc.aabb       = {{-5.f, -0.01f, -5.f}, {5.f, 0.01f, 5.f}};
        dc.castShadow = false;
        dc.tint       = {0.55f, 0.55f, 0.55f};
        r3d->Submit(dc);
    }

    // ── Sphere M5Demo ────────────────────────────────────────────────────
    {
        NkDrawCall3D dc;
        dc.mesh       = st->meshSphere;
        dc.transform  = NkMat4f::Translate({0.f, 0.9f, 0.f}) *
                        NkMat4f::Scale({0.85f, 0.85f, 0.85f});
        dc.aabb       = {{-0.9f, 0.0f, -0.9f}, {0.9f, 1.8f, 0.9f}};
        dc.tint       = {1.f, 1.f, 1.f};   // pas de teinte (le shader cycle HSV)
        if (st->mat && st->mat->IsValid())
            dc.material = st->mat->GetInstHandle();
        r3d->Submit(dc);
    }

    r3d->DrawDebugAxes(NkMat4f::Identity(), 0.5f);

    if (auto* overlay = ctx.renderer->GetOverlay()) {
        overlay->BeginOverlay(ctx.renderer->GetCmd(), ctx.width, ctx.height);
        overlay->DrawStats(ctx.renderer->GetStats());
        overlay->DrawText({20.f, 35.f},
            "Demo7 M.5 Material Functions  |  API : %s",
            NkGraphicsApiName(ctx.api));
        overlay->DrawText({20.f, 55.f},
            "Shader: m5demo.frag.vk.glsl  +  4 #include depuis Include/");
        overlay->DrawText({20.f, 75.f},
            "Effets : HSV cycle (NkColor) | FBM (NkNoise) | Toon ramp (NkToonRamp) | Fresnel halo (NkFresnel)");
        overlay->DrawText({20.f, 95.f},
            "halo intensity=%.1f  rimPower=%.2f  color#%d  tintMix=%.0f",
            st->haloIntensity, st->rimPower, st->haloColorIdx, st->tintMix);
        overlay->DrawText({20.f, 115.f},
            "I/K:intensity  U/J:rimPower  H:cycle color  1/2:HSV/tint");
        overlay->DrawText({20.f, 135.f},
            "FPS : %.0f", dt > 1e-5f ? 1.f / dt : 0.f);
        overlay->EndOverlay();
    }

    ctx.renderer->Present();
    ctx.renderer->EndFrame();
}

void Demo7_MaterialFunctions_Shutdown(DemoCtx& ctx) {
    auto* st = (Demo7FnState*)ctx.userData;
    if (!st) return;
    NkMaterial::Destroy(st->mat);
    delete st;
    ctx.userData = nullptr;
    logger.Info("[Demo7] Shutdown\n");
}

}} // namespace nkentseu::demo
