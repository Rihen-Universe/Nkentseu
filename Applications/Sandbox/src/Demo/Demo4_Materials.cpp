// =============================================================================
// Demo4_Materials.cpp  — Demo 4 : systeme de materiaux + Planar Reflection
//
// 5 spheres, chacune avec un materiau different. Le sol reflete les spheres
// via NkRenderTarget (planar reflection Y=0).
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
#include "NKRenderer/Materials/NkMaterialLibrary.h"   // Phase G
#include "NKRenderer/Tools/Reflection/NkPlanarReflectionSystem.h"
#include "NKRenderer/Tools/VoxelAO/NkVoxelAOSystem.h"   // Phase H.6
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
struct Demo4MatState {
    NkMaterial*  mats[5]     = {};
    const char*  matNames[5] = {"PBR Metal", "PBR Plastic", "Toon", "Anime", "Unlit"};
    NkMeshHandle meshSphere;
    NkMeshHandle meshPlane;
    NkMeshHandle meshCube;        // Phase M.8 : multi-material cube
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
    logger.Info("[Demo4][NKImage] === Smoke test PNG/JPEG/HDR ===\n");

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
        // Load() est une méthode INSTANCE qui charge dans *this. On utilise une
        // image sur la pile : son destructeur libère les pixels (allocateur custom),
        // pas besoin de Free() (réservé aux images allouées via Alloc/Create).
        NkImage img;
        if (img.Load(t.path, 0) && img.IsValid()) {
            logger.Info("[Demo4][NKImage] {0} OK : {1}x{2} ch={3} fmt={4}\n",
                        t.label, img.Width(), img.Height(),
                        (int)img.Channels(), (int)img.Format());
            ++passed;
        } else {
            logger.Warnf("[Demo4][NKImage] %s FAIL : '%s' did not load\n",
                         t.label, t.path);
        }
    }
    logger.Info("[Demo4][NKImage] === {0}/{1} passed ===\n", passed, total);
}

// ── Init ──────────────────────────────────────────────────────────────────────
bool Demo4_Materials_Init(DemoCtx& ctx) {
    NKImageSmokeTest();   // Phase H : valider que NKImage charge les 3 formats clefs

    auto* st = new Demo4MatState();
    ctx.userData = st;

    // Meshes
    auto* meshSys = ctx.renderer->GetMeshSystem();
    if (!meshSys) {
        logger.Errorf("[Demo4] Pas de NkMeshSystem\n");
        delete st; ctx.userData = nullptr; return false;
    }
    st->meshSphere = meshSys->GetSphere();
    st->meshCube   = meshSys->GetCube();   // Phase M.8 : 6 sous-meshes (1 par face)
    st->meshPlane  = meshSys->GetPlane();

    // ── Phase H.6 : enregistre les occluders pour le Voxel AO ─────────────
    // Le sol (plane Y=0, 14x14 unités) est le principal occluder : il cache
    // l'IBL sky pour les sphères placées en dessous (belowY=-1.2).
    // Le voxel grid bake CPU + upload une fois ici ; le PBR shader sample
    // automatiquement chaque frame pour atténuer l'IBL irradiance/specular.
    if (auto* vao = ctx.renderer->GetVoxelAO()) {
        vao->Clear();
        // Sol épais (±0.5 Y = 1 unité d'épaisseur) pour que les cone-traces
        // exponentiels du PBR shader (t = 0.15, 0.225, 0.34, 0.5, 0.76, 1.14...)
        // captent toujours plusieurs voxels au milieu. Un sol trop fin
        // (±0.1) est sauté entre 2 samples successifs.
        vao->RegisterAABB({-7.f, -0.5f, -7.f}, {7.f, 0.5f, 7.f}, 1.0f);
        vao->Build();
    }

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

    // M.1 v0 : sphere #4 utilise un Layered (2 layers PBR : or en haut,
    // cuivre noirci en bas, blend via UV.y) au lieu de Unlit. Visualise
    // immediatement le mecanisme N-couches.
    static const NkMaterialType kTypes[5] = {
        NkMaterialType::NK_PBR_METALLIC,
        NkMaterialType::NK_PBR_METALLIC,
        NkMaterialType::NK_TOON,
        NkMaterialType::NK_ANIME,
        NkMaterialType::NK_UNLIT,          // 5e sphere Unlit (Layered pas encore migre en NkSL -> DX KO)
    };

    for (int i = 0; i < 5; i++) {
        st->mats[i] = NkMaterial::Create(matSys, kTypes[i]);
        if (!st->mats[i] || !st->mats[i]->IsValid()) {
            logger.Warnf("[Demo4] Materiau [{0}] ({1}) invalide — fallback tint seul\n",
                         i, st->matNames[i]);
        } else if (kTypes[i] == NkMaterialType::NK_LAYERED) {
            // Configure le layered : or brillant (haut) + cuivre noirci (bas).
            NkPBRParams gold;
            gold.albedo    = {1.00f, 0.78f, 0.20f, 1.f};
            gold.metallic  = 1.0f;
            gold.roughness = 0.15f;

            NkPBRParams burnedCopper;
            burnedCopper.albedo    = {0.45f, 0.18f, 0.08f, 1.f};
            burnedCopper.metallic  = 0.6f;
            burnedCopper.roughness = 0.75f;

            st->mats[i]->SetLayerBase(burnedCopper);     // mask=0 (bas)
            st->mats[i]->SetLayerTop(gold);              // mask=1 (haut)
            st->mats[i]->SetLayerMaskSource(4);          // 4 = vUV.y gradient
            st->matNames[i] = "Layered (or/cuivre)";
        } else {
            ApplyParams(st->mats[i], i, st->params[i]);
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
                logger.Info("[Demo4][PhaseG] Saved '{0}' (id={1})\n",
                            outPath, savedId.ToString());
            } else {
                logger.Warnf("[Demo4][PhaseG] Save failed for %s\n", outPath.CStr());
            }

            // Scanner le dossier pour enregistrer l'asset dans NkAssetRegistry.
            matLib->ScanDirectory(NkString("Resources/NKRenderer/Materials"));

            // Load par chemin logique. Le NkMaterialLibrary construit l'instance
            // via NkMaterialSystem en utilisant les params PBR du .nkasset.
            st->goldFromAsset = matLib->Load(NkString("/Materials/Gold"));
            if (st->goldFromAsset.IsValid()) {
                logger.Info("[Demo4][PhaseG] Loaded '/Materials/Gold' OK -> sphere #1 will use asset-loaded material\n");
            } else {
                logger.Warnf("[Demo4][PhaseG] Load failed for /Materials/Gold\n");
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
                logger.Info("[Demo4][PhaseH] Saved texture asset '{0}' (id={1})\n",
                            logicalPath, savedId.ToString());
                NkTexHandle texH = NkTextureAssetIO::LoadById(savedId, texLib);
                if (texH.IsValid()) {
                    // Applique sur sphere #2 (PBR Plastic) comme albedo map.
                    if (st->mats[1] && st->mats[1]->IsValid()) {
                        st->mats[1]->SetAlbedoMap(texH);
                        logger.Info("[Demo4][PhaseH] AwesomeFace albedo applied to sphere #2\n");
                    }
                } else {
                    logger.Warnf("[Demo4][PhaseH] LoadById failed for AwesomeFace\n");
                }
            }
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
            // Toggle planar reflection
            case NkKey::NK_R: {
                st->reflEnabled = !st->reflEnabled;
                logger.Info("[Demo4] Planar reflection : {0}\n",
                             st->reflEnabled ? "ON" : "OFF");
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

    // Camera orbit interactive (souris + clavier) via NkOrbitCameraController3D.
    // Defaut : target=(0,0.5,0) au centre de la scene, distance=9, yaw=0, pitch=-0.2.
    // Auto-orbit off : controle full manuel souris + clavier.
    st->camera.Controller().SetCenter({0.f, 0.5f, 0.f}, 9.f, 0.f, -0.2f);
    st->camera.Controller().SetAutoOrbit(false);
    st->camera.InstallEvents();

    logger.Info("[Demo4] Init OK — 5 materiaux crees\n");
    logger.Info("[Demo4] 1-5:mat  +/-:roughness  M:metallic  C:couleur  O:outline  R:reflet\n");
    logger.Info("[Demo4] Camera : LEFT-drag rotate | wheel zoom | WASD/fleches pan | T:auto-orbit | F:FPS | HOME:reset\n");
    return true;
}

// ── Frame ─────────────────────────────────────────────────────────────────────
void Demo4_Materials_Frame(DemoCtx& ctx, float32 dt) {
    auto* st = (Demo4MatState*)ctx.userData;
    // DIAG (gated NK_FIX_CAM) : fige le temps pour une pose 100% déterministe et
    // identique entre backends (compare VK/DX12/DX11 au même angle + même bob sphères).
    static int sFixCam = -1;
    if (sFixCam == -1) { const char* v = getenv("NK_FIX_CAM"); sFixCam = (v && v[0] && v[0] != '0') ? 1 : 0; }
    if (sFixCam) { ctx.totalTime = 1.0f; }
    st->camera.Update(sFixCam ? 0.f : dt);

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
        // castShadow=true : le sol projette son ombre sur les objets en
        // dessous (sphere belowY=-1.2) -> ils recoivent zero lumiere
        // directionnelle de la lumiere du dessus, donc apparaissent sombres.
        // Sans ca, la directional light traverse le sol et eclaire les
        // objets sous le sol (visuellement incorrect).
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

    // ── Debug gizmos ─────────────────────────────────────────────────────────
    r3d->DrawDebugAxes(NkMat4f::Identity(), 0.5f);

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
            "Demo Materials  |  API : %s  |  actif : %d (%s)",
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
void Demo4_Materials_Shutdown(DemoCtx& ctx) {
    auto* st = (Demo4MatState*)ctx.userData;
    if (st) {
        for (int i = 0; i < 5; i++) NkMaterial::Destroy(st->mats[i]);
        NkMaterial::Destroy(st->floorMat);
        // Le NkPlanarReflectionSystem est detruit par NkRendererImpl ;
        // pas besoin de Unregister explicitement.
        delete st;
    }
    ctx.userData = nullptr;
    logger.Info("[Demo4] Shutdown\n");
}

}} // namespace nkentseu::demo
