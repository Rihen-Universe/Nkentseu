// =============================================================================
// main.cpp  — Renderdemo entry point (NkRenderer v5.0)
//
// Usage :
//   renderdemo                       # demo par defaut (Demo 0 — Subsystems)
//   renderdemo --demo=N              # selectionne la demo (0=Subsystems, 1=2D, 2=3D, 3=Materials)
//   renderdemo --backend=opengl      # (par defaut)
//   renderdemo --backend=vulkan|dx11|dx12|metal|sw
//
// Liste des demos :
//   0 — Subsystems  : enable/disable runtime des sous-systemes
//   1 — 2D          : sprites + formes + texte (config For2D)
//   2 — 3D          : sphere grid + lights + ombres (config ForGame)
//   3 — Materials   : 5 spheres NkMaterial (PBR/Toon/Anime/Unlit), edition temps reel
//   12 — GLTF       : charge + affiche un modele glTF (rubber_duck, PBR + baseColor)
// =============================================================================
#include "DemoCommon.h"
#include <cstdlib>   // getenv (diag opt-in NK_VK_VALIDATION)

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/NKMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu { struct NkEntryState; }
using namespace nkentseu;
using namespace nkentseu::demo;

namespace nkentseu { namespace demo {

    // Forward declarations des demos
    bool DemoSubsystems_Init        (DemoCtx&); void DemoSubsystems_Frame    (DemoCtx&, float32); void DemoSubsystems_Shutdown    (DemoCtx&);
    bool Demo2D_Init                (DemoCtx&); void Demo2D_Frame            (DemoCtx&, float32); void Demo2D_Shutdown            (DemoCtx&);
    bool Demo3D_Init                (DemoCtx&); void Demo3D_Frame            (DemoCtx&, float32); void Demo3D_Shutdown            (DemoCtx&);
    bool Demo4_Materials_Init       (DemoCtx&); void Demo4_Materials_Frame   (DemoCtx&, float32); void Demo4_Materials_Shutdown   (DemoCtx&);
    bool Demo5_Materials_Init       (DemoCtx&); void Demo5_Materials_Frame   (DemoCtx&, float32); void Demo5_Materials_Shutdown   (DemoCtx&);
    bool Demo6_HierarchicalMaterials_Init(DemoCtx&); void Demo6_HierarchicalMaterials_Frame(DemoCtx&, float32); void Demo6_HierarchicalMaterials_Shutdown(DemoCtx&);
    bool Demo7_MaterialFunctions_Init    (DemoCtx&); void Demo7_MaterialFunctions_Frame    (DemoCtx&, float32); void Demo7_MaterialFunctions_Shutdown    (DemoCtx&);
    bool Demo9_Glow2D_Init               (DemoCtx&); void Demo9_Glow2D_Frame               (DemoCtx&, float32); void Demo9_Glow2D_Shutdown               (DemoCtx&);
    bool Demo8_LayeredV1_Init            (DemoCtx&); void Demo8_LayeredV1_Frame            (DemoCtx&, float32); void Demo8_LayeredV1_Shutdown            (DemoCtx&);
    bool Demo11_FPSArena_Init            (DemoCtx&); void Demo11_FPSArena_Frame            (DemoCtx&, float32); void Demo11_FPSArena_Shutdown            (DemoCtx&);
    bool DemoGLTF_Init                   (DemoCtx&); void DemoGLTF_Frame                   (DemoCtx&, float32); void DemoGLTF_Shutdown                   (DemoCtx&);
    bool DemoSkin_Init                   (DemoCtx&); void DemoSkin_Frame                   (DemoCtx&, float32); void DemoSkin_Shutdown                   (DemoCtx&);
    bool DemoIK_Init                     (DemoCtx&); void DemoIK_Frame                     (DemoCtx&, float32); void DemoIK_Shutdown                     (DemoCtx&);
    bool DemoIKChar_Init                 (DemoCtx&); void DemoIKChar_Frame                 (DemoCtx&, float32); void DemoIKChar_Shutdown                 (DemoCtx&);
    bool DemoAnim_Init                   (DemoCtx&); void DemoAnim_Frame                   (DemoCtx&, float32); void DemoAnim_Shutdown                   (DemoCtx&);
    bool DemoAnimIK_Init                 (DemoCtx&); void DemoAnimIK_Frame                 (DemoCtx&, float32); void DemoAnimIK_Shutdown                 (DemoCtx&);

    static const DemoEntry kDemos[] = {
        { "Subsystems", "Runtime enable/disable des sous-systemes",
            DemoSubsystems_Init,      DemoSubsystems_Frame,    DemoSubsystems_Shutdown },
        { "2D",         "Render2D : sprites + shapes + texte",
            Demo2D_Init,              Demo2D_Frame,            Demo2D_Shutdown },
        { "3D",         "Render3D : grid PBR + lights + ombres",
            Demo3D_Init,              Demo3D_Frame,            Demo3D_Shutdown },
        { "Materials",  "NkMaterial : 5 spheres multi-materiau, modifications temps reel",
            Demo4_Materials_Init,     Demo4_Materials_Frame,   Demo4_Materials_Shutdown },
        { "Materials5", "NkMaterial v2 : evolutions M.2+ (MPC, blend vcolor, hierarchies, etc.)",
            Demo5_Materials_Init,     Demo5_Materials_Frame,   Demo5_Materials_Shutdown },
        { "Materials6", "M.4 Hierarchical Material Instances (parent/enfants avec override + propagation)",
            Demo6_HierarchicalMaterials_Init, Demo6_HierarchicalMaterials_Frame, Demo6_HierarchicalMaterials_Shutdown },
        { "Materials7", "M.5 Material Functions (shader #include + .glsli builtins)",
            Demo7_MaterialFunctions_Init, Demo7_MaterialFunctions_Frame, Demo7_MaterialFunctions_Shutdown },
        { "Materials8", "M.1 v1 Material Layering N=8 layers (masks vColor/vUV/const)",
            Demo8_LayeredV1_Init, Demo8_LayeredV1_Frame, Demo8_LayeredV1_Shutdown },
        { "Materials9", "Phase E Materials 2D v0 (Glow2D sprite, future unifie NkMaterial)",
            Demo9_Glow2D_Init, Demo9_Glow2D_Frame, Demo9_Glow2D_Shutdown },
        // Phase N v1 : meme scene 5 spheres que Demo4 mais avec newport_loft.hdr
        // (contraste fort interieur-loft <-> fenetres lumineuses) + iblStrength
        // boost pour valider visuellement le cubemap HDR brut + tonemap ACES.
        // Reutilise les callbacks Demo4_Materials_* (config differe en BuildConfig).
        { "PhaseNv1",   "Phase N v1 : skybox HDR brut + ACES tonemap (newport_loft, contraste fort)",
            Demo4_Materials_Init, Demo4_Materials_Frame, Demo4_Materials_Shutdown },
        // Demo11 FPS Arena : scene cubes (sol+murs) + spheres + cylindres,
        // textures procedural REPEAT, camera FPS souris+WASD via NkInputQuery.
        { "FPSArena",   "Demo11 : arene FPS style x.jpg (sol+murs cubes texturees, FPS cam)",
            Demo11_FPSArena_Init, Demo11_FPSArena_Frame, Demo11_FPSArena_Shutdown },
        // DemoGLTF : chargement glTF 2.0 (geometrie + materiaux PBR + textures)
        // via NkGLTFLoader + NkGLTFMaterialBridge, affichage camera orbitale.
        { "GLTF",       "DemoGLTF : charge + affiche un modele glTF (rubber_duck, PBR + baseColor)",
            DemoGLTF_Init, DemoGLTF_Frame, DemoGLTF_Shutdown },
        // DemoSkin : skinning GPU (glTF skinne SimpleSkin, pose animee par frame
        // via EvaluateGLTFPose + SubmitSkinned, vertex shader skin LBS).
        { "Skin",       "DemoSkin : skinning GPU glTF (SimpleSkin se plie, anime)",
            DemoSkin_Init, DemoSkin_Frame, DemoSkin_Shutdown },
        // DemoIK : NkAnima M0 — IK FABRIK temps reel (chaine d'os suit une cible animee).
        { "IK",         "DemoIK : NkAnima M0 — IK FABRIK (chaine suit une cible, squelette debug)",
            DemoIK_Init, DemoIK_Frame, DemoIK_Shutdown },
        // DemoIKChar : NkAnima M0 (d) — IK FABRIK sur un VRAI squelette glTF
        // (CesiumMan) : un membre suit une cible, squelette rendu en debug-lines.
        { "IKChar",     "DemoIKChar : NkAnima M0 — IK FABRIK sur squelette glTF reel (CesiumMan)",
            DemoIKChar_Init, DemoIKChar_Frame, DemoIKChar_Shutdown },
        // DemoAnim : NkAnima M1 — pipeline clip + .nkanim binaire + player.
        // glTF -> bake clip -> save .nkanim -> reload -> play -> skinning.
        { "Anim",       "DemoAnim : NkAnima M1 — clip + .nkanim binaire + player (CesiumMan rejoue)",
            DemoAnim_Init, DemoAnim_Frame, DemoAnim_Shutdown },
        // DemoAnimIK : NkAnima M1+M0 — IK PAR-DESSUS l'anim (le corps marche, le bras
        // atteint une cible). Signature Cascadeur : animation + IK ensemble.
        { "AnimIK",     "DemoAnimIK : NkAnima M1+M0 — IK sur anim (marche + bras qui atteint une cible)",
            DemoAnimIK_Init, DemoAnimIK_Frame, DemoAnimIK_Shutdown },
    };
    static constexpr uint32 kDemoCount = (uint32)(sizeof(kDemos) / sizeof(kDemos[0]));

    static NkRendererConfig BuildConfig(int demoIdx, NkGraphicsApi api,
                                         uint32 w, uint32 h) {
        switch (demoIdx) {
            case 0: {
                NkRendererConfig c;
                c.api        = api;
                c.width      = w;
                c.height     = h;
                c.subsystems = NK_SS_RENDER2D | NK_SS_TEXT | NK_SS_OVERLAY;
                c.hdr        = false;
                return c;
            }
            case 1: return NkRendererConfig::For2D(api, w, h);
            case 2: {
                auto c = NkRendererConfig::ForGame(api, w, h);
                // Demo3D scene tient dans ~7 unites -> 1 cascade large suffit
                // et evite les transitions de cascade qui font scintiller les
                // ombres quand la camera orbite. CSM 4-cascades reste actif
                // dans le code et utilisable pour des scenes plus ouvertes.
                c.shadow.cascadeCount = 1;
                return c;
            }
            case 3: {
                // Demo4 : meme config que Demo3D — 5 spheres, scene ~12 unites.
                // PCSS active (contact-hardening) : sphere ↔ sol -> ombres
                // nettes au contact, plus floues en s'eloignant.
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = true;
                c.voxelAOEnabled      = true;   // Demo4 enregistre des occluders
                // Phase N v0 : utilise un vrai HDR equirect pour l'IBL au lieu
                // du gradient sky procedural. Visuel beaucoup plus realiste.
                // studio.hdr (256x128) etait trop low-res / peu contraste pour
                // qu'on voie la difference — piazza_bologni_1k.hdr (1024x512)
                // a un ciel bleu + architecture qui apparait visiblement sur
                // les spheres metalliques.
                c.ibl.useHDR     = true;
                c.ibl.hdrPath    = "Resources/NKRenderer/Textures/Vracs/HDR/piazza_bologni_1k.hdr";
                // Booste iblStrength pour mieux voir l'apport HDR (default 0.3).
                c.ibl.iblStrength = 1.0f;
                // Phase N v0.5 : affiche le HDR comme background skybox visible.
                c.ibl.drawSkybox  = true;
                return c;
            }
            case 4: {
                // Demo5 : demo de MATERIAUX (spheres metalliques rough=0.15).
                // Un metal n'a pas de diffus : il ne montre QUE des reflets de
                // l'environnement. Sans environnement visible (drawSkybox=false +
                // IBL faible), les spheres metalliques reflechissent du noir et
                // paraissent sombres ("demo5 sombre"). On lui donne donc un vrai
                // HDR + skybox (comme Demo4) pour que les materiaux soient visibles.
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = false;
                c.ibl.useHDR      = true;
                c.ibl.hdrPath     = "Resources/NKRenderer/Textures/Vracs/HDR/piazza_bologni_1k.hdr";
                c.ibl.iblStrength = 1.0f;
                c.ibl.drawSkybox  = true;
                return c;
            }
            case 5: {
                // Demo6 M.4 : scene legere (4 spheres + 1 petit-enfant + sol).
                // Meme config Game que Demo4/5, 1 cascade suffit, pas de PCSS
                // (les ombres ne sont pas le focus de M.4).
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = false;
                return c;
            }
            case 6: {
                // Demo7 M.5 : 1 sphere custom shader + sol simple. Pas de
                // shadow ni PCSS — la demo est purement procedurale.
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = false;
                return c;
            }
            case 7: {
                // Demo8 M.1 v1 : sphere LayeredV1 + sol. Idem leger.
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = false;
                return c;
            }
            case 8: {
                // Demo9 Phase E Materials 2D : 2D only, pas de shadows ni 3D.
                // Config For2D allege la scene (pas de PBR/IBL/shadow).
                return NkRendererConfig::For2D(api, w, h);
            }
            case 9: {
                // Demo10 Phase N v1 : meme scene 5 spheres que Demo4 (reutilise
                // callbacks Demo4_Materials_*) mais avec un HDR a fort contraste
                // (newport_loft : loft sombre + fenetres tres lumineuses) +
                // iblStrength boost. Objectif : valider visuellement le cubemap
                // skybox dedie HDR brut (RGBA32F, sans Reinhard) + tonemap ACES
                // applique dans le shader skybox. Le contraste rend les bright
                // spots du sun/fenetres bien visibles (vs Reinhard CPU qui les
                // fait disparaitre en gris fade).
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = true;
                c.ibl.useHDR     = true;
                c.ibl.hdrPath    = "Resources/NKRenderer/Textures/Vracs/HDR/newport_loft.hdr";
                c.ibl.iblStrength = 1.5f;   // boost pour amplifier bright spots
                c.ibl.drawSkybox  = true;
                return c;
            }
            case 10: {
                // Demo11 FPS Arena : scene 30x30m clos. UNE seule cascade fixe
                // centree origine couvrant toute l'arene. Le mode N=1 dans
                // NkShadowSystem utilise une sphere fixe au lieu de fit-camera
                // -> shadow texels stables, pas de swim/shimmering quand la
                // camera bouge. sceneRadius reglé dans Demo11_Init via
                // GetShadow()->GetConfig().sceneRadius = 25.
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = true;
                c.ibl.useHDR          = false;   // sky procedural suffit
                c.ibl.drawSkybox      = true;
                c.ibl.iblStrength     = 0.6f;
                return c;
            }
            case 11: {
                // DemoGLTF : modele glTF unique, scene compacte. Config Game
                // (PBR + IBL sky procedural + ombres). 1 cascade suffit pour un
                // seul objet centre ; pas de PCSS (perf + simplicite).
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = false;
                c.ibl.drawSkybox      = true;
                return c;
            }
            case 12: {
                // DemoSkin : modele skinne unique. Config Game minimale, sky
                // procedural visible pour cadrer le mesh, 1 cascade.
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.shadow.pcss         = false;
                c.ibl.drawSkybox      = true;
                // Bloom OFF : ces modeles sont MATS (metallic=0). Le bloom faisait
                // "briller" les textures saturees (glow neon) alors qu'elles ne
                // refletent pas la lumiere. Sans bloom -> eclairage mat normal.
                c.postProcess.bloom   = false;
                c.postProcess.ssr     = false;
                // SSAO OFF : dans l'ombre dense sous le modele, le SSAO pousse le
                // sol vers le noir profond ou le tonemap fait apparaitre un voile
                // magenta ("halo" au sol). Un viewer de modele n'a pas besoin de
                // SSAO -> off. (Bug SSAO/tonemap a corriger globalement par ailleurs.)
                c.postProcess.ssao    = false;
                // Environnement NEUTRE bien eclaire (look "studio" pour viewer) :
                // gradient gris -> ambient doux INCOLORE qui revele tout le modele
                // (cote ombre visible) sans teinte parasite ni effet miroir.
                c.ibl.skyTop      = {0.70f, 0.70f, 0.70f};
                c.ibl.horizon     = {0.62f, 0.62f, 0.62f};
                c.ibl.ground      = {0.45f, 0.45f, 0.45f};
                c.ibl.iblStrength = 0.40f;
                return c;
            }
            case 13: {
                // DemoIK (NkAnima M0) : squelette IK en sphères PBR. Config Game
                // minimale + IBL neutre clair (ambient de remplissage = scène bien
                // éclairée, sinon tout est noir).
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.postProcess.bloom   = false;
                c.postProcess.ssao    = false;
                c.postProcess.ssr     = false;
                c.ibl.drawSkybox      = true;   // fond gradient visible (repère)
                c.ibl.skyTop      = {0.55f, 0.62f, 0.78f};
                c.ibl.horizon     = {0.62f, 0.64f, 0.68f};
                c.ibl.ground      = {0.30f, 0.30f, 0.34f};
                c.ibl.iblStrength = 0.85f;
                return c;
            }
            case 15:   // DemoAnim (NkAnima M1) : rejoue .nkanim. Game standard.
            case 16: { // DemoAnimIK (NkAnima M1+M0) : anim + IK. Game standard.
                return NkRendererConfig::ForGame(api, w, h);
            }
            case 14: {
                // DemoIKChar (NkAnima M0 d) : squelette glTF en debug-lines. Même
                // config neutre que DemoIK (Game minimal + IBL clair de remplissage).
                auto c = NkRendererConfig::ForGame(api, w, h);
                c.shadow.cascadeCount = 1;
                c.postProcess.bloom   = false;
                c.postProcess.ssao    = false;
                c.postProcess.ssr     = false;
                c.ibl.drawSkybox      = true;
                c.ibl.skyTop      = {0.55f, 0.62f, 0.78f};
                c.ibl.horizon     = {0.62f, 0.64f, 0.68f};
                c.ibl.ground      = {0.30f, 0.30f, 0.34f};
                c.ibl.iblStrength = 0.85f;
                return c;
            }
            default: return NkRendererConfig::ForGame(api, w, h);
        }
    }

}} // namespace

// =============================================================================
// nkmain — entry point unifie
// =============================================================================
int nkmain(const NkEntryState& state) {

    // ── Parse args ───────────────────────────────────────────────────────────
    NkGraphicsApi api    = ParseBackend(state.GetArgs());
    int           demoIx = ParseDemo(state.GetArgs(), 0);
    // Alias : --demo=N -> index N-1 pour les demos numerotees (Demo4 -> 3, Demo5 -> 4).
    // Coherence avec le nom de fichier plutot que l'index zero-based.
    if (demoIx == 4) demoIx = 3;
    if (demoIx == 5) demoIx = 4;
    if (demoIx == 6) demoIx = 5;
    if (demoIx == 7) demoIx = 6;
    if (demoIx == 8) demoIx = 7;
    if (demoIx == 9) demoIx = 8;
    if (demoIx == 10) demoIx = 9;   // Demo10 PhaseNv1 -> kDemos[9]
    if (demoIx == 11) demoIx = 10;  // Demo11 FPSArena -> kDemos[10]
    if (demoIx == 12) demoIx = 11;  // DemoGLTF       -> kDemos[11]
    if (demoIx == 13) demoIx = 12;  // DemoSkin       -> kDemos[12]
    if (demoIx == 14) demoIx = 13;  // DemoIK         -> kDemos[13]
    if (demoIx == 15) demoIx = 14;  // DemoIKChar     -> kDemos[14]
    if (demoIx == 16) demoIx = 15;  // DemoAnim       -> kDemos[15]
    if (demoIx == 17) demoIx = 16;  // DemoAnimIK     -> kDemos[16]
    if (demoIx < 0 || (uint32)demoIx >= kDemoCount) demoIx = 0;
    const DemoEntry& demo = kDemos[demoIx];

    logger.Info("=========================================================\n");
    logger.Info(" NkRenderer v5.0 — Demo {0} ({1}) — Backend : {2}\n",
                demoIx, demo.name, NkGraphicsApiName(api));
    logger.Info("=========================================================\n");

    // ── Fenetre ──────────────────────────────────────────────────────────────
    NkWindowConfig wcfg;
    wcfg.title     = NkFormat("NkRenderer demo : {0}", demo.name);
    wcfg.width     = 1280;
    wcfg.height    = 720;
    wcfg.centered  = true;
    wcfg.resizable = true;

    NkWindow window;
    if (!window.Create(wcfg)) {
        logger.Errorf("[main] Window creation failed\n");
        return 1;
    }

    // ── Device RHI ───────────────────────────────────────────────────────────
    NkSurfaceDesc surface = window.GetSurfaceDesc();
    NkDeviceInitInfo devInfo;
    devInfo.api     = api;
    devInfo.surface = surface;
    devInfo.width   = (uint32)window.GetSize().width;
    devInfo.height  = (uint32)window.GetSize().height;
    devInfo.context.vulkan.appName    = "NkRenderer_Demo";
    devInfo.context.vulkan.engineName = "Nkentseu";
    // ── Validation Vulkan (DIAG opt-in, OFF par défaut) ──────────────────────
    // Active VK_LAYER_KHRONOS_validation + debug messenger (route vers NkLogger)
    // UNIQUEMENT si NK_VK_VALIDATION est défini dans l'environnement. Gardé OFF
    // par défaut : la couche peut crasher selon l'env (msvcp140 Huawei DevEco
    // dans le PATH → SIGSEGV vkCreateInstance) et coûte en perf. Sert à diagnostiquer
    // les hazards de synchro (ex. flicker du pipeline skin sur -bvk).
    {
        const char* vkv = getenv("NK_VK_VALIDATION");
        if (vkv && vkv[0] && vkv[0] != '0') {
            devInfo.context.vulkan.validationLayers = true;
            devInfo.context.vulkan.debugMessenger   = true;
            logger.Info("[main] Validation Vulkan ACTIVEE (NK_VK_VALIDATION)\n");
        }
    }
    // Format GLOBAL du swapchain (cross-API : GL/VK/DX). UNORM = couleur affichée telle
    // quelle (comme OpenGL/DX) → rendu identique cross-backend. SRGB = encode gamma auto.
    // Une seule ligne pour tous les backends.
    devInfo.context.swapchainFormat = NkSwapchainFormat::NK_SWAPCHAIN_BGRA8_UNORM;

    NkIDevice* device = NkDeviceFactory::Create(devInfo);
    if (!device || !device->IsValid()) {
        logger.Errorf("[main] NkDeviceFactory::Create failed\n");
        window.Close();
        return 2;
    }

    // ── Renderer ─────────────────────────────────────────────────────────────
    uint32 W = (uint32)window.GetSize().width;
    uint32 H = (uint32)window.GetSize().height;
    NkRendererConfig cfg = BuildConfig(demoIx, api, W, H);

    char flagsBuf[256];
    SubsystemFlagsToString(cfg.subsystems, flagsBuf, sizeof(flagsBuf));
    logger.Info("[main] Config : {0}x{1}, subsystems = {2}\n", W, H, flagsBuf);

    NkRenderer* renderer = NkRenderer::Create(device, cfg);
    if (!renderer) {
        logger.Errorf("[main] NkRenderer::Create failed (last err : {0})\n",
                      NkRGetLastErrorMessage());
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 3;
    }

    // ── Demo init ────────────────────────────────────────────────────────────
    DemoCtx ctx;
    ctx.device   = device;
    ctx.renderer = renderer;
    ctx.window   = &window;
    ctx.api      = api;
    ctx.width    = W;
    ctx.height   = H;
    if (!demo.init(ctx)) {
        logger.Errorf("[main] Demo init failed\n");
        NkRenderer::Destroy(renderer);
        device->WaitIdle();
        NkDeviceFactory::Destroy(device);
        window.Close();
        return 4;
    }

    // ── Boucle ───────────────────────────────────────────────────────────────
    bool running = true;
    NkClock clock;
    NkEventSystem& events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running = false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if (e->GetKey() == NkKey::NK_ESCAPE) running = false;
    });
    // Resize appliqué IMMÉDIATEMENT dans le handler (et pas différé en boucle de jeu) :
    // sous DX12 flip-model, présenter un swapchain à l'ANCIENNE taille dans une fenêtre déjà
    // redimensionnée (le délai qu'introduisait un debounce) provoque un device removed. En
    // resizant tout de suite, aucun present mismatché ne passe. Le no-op même-taille est géré
    // côté device (ResizeSwapchain) → pas de travail redondant si la taille n'a pas changé.
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        uint32 w = (uint32)e->GetWidth(), h = (uint32)e->GetHeight();
        if (w > 0 && h > 0 && (w != ctx.width || h != ctx.height)) {
            ctx.width = w; ctx.height = h;
            renderer->OnResize(w, h);
        }
    });

    // NK_MAXFRAMES=<n> : sortie propre apres n frames (vidange du log async pour
    // diag headless). 0/absent = boucle infinie normale.
    const char* mfEnv = getenv("NK_MAXFRAMES");
    const uint64 maxFrames = mfEnv ? (uint64)atoll(mfEnv) : 0;

    while (running) {
        events.PollEvents();
        if (!running) break;
        if (maxFrames && ctx.frame >= maxFrames) { running = false; break; }

        float32 dt = clock.Tick().delta;
        if (dt <= 0.f || dt > 0.25f) dt = 1.f / 60.f;
        ctx.totalTime += dt;
        ctx.frame++;

        if (ctx.width == 0 || ctx.height == 0) continue;
        demo.frame(ctx, dt);
    }

    // ── Cleanup ──────────────────────────────────────────────────────────────
    demo.shutdown(ctx);
    NkRenderer::Destroy(renderer);
    device->WaitIdle();
    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[main] Bye\n");
    return 0;
}
