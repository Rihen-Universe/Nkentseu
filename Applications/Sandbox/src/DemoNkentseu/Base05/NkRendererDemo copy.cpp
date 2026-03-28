// =============================================================================
// NkRendererDemo.cpp
// Exemple d'utilisation complet du NKRenderer.
//
// Démontre :
//   • Initialisation du renderer
//   • Chargement de modèles 3D (OBJ, GLB)
//   • Matériaux PBR, Toon, Verre, Peau, Eau, Foliage, Émissif
//   • Système de scène ECS (entités + composants)
//   • Éclairage dynamique (directionnel, point, spot)
//   • IBL (Image Based Lighting) depuis HDRI
//   • Rendu 2D (formes, sprites, texte)
//   • Texte 3D billboard et extrudé
//   • Particules / VFX (feu, fumée, magie, pluie)
//   • Trails
//   • Post-processing (ACES tonemapping, bloom, SSAO, DOF, vignette)
//   • Mode wireframe / vertex / normal (comme Blender)
//   • Shadow mapping
//   • Picking (sélection d'objet par clic)
//   • Debug gizmos (axes, AABB, frustum)
//   • Capture de frame (screenshot)
// =============================================================================
#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKLogger/NkLog.h"

// NKRenderer — le cœur
#include "NKRenderer/NkRenderer.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"

using namespace nkentseu;
using namespace nkentseu::renderer;

// =============================================================================
// Application état
// =============================================================================
struct AppState {
    bool    running      = true;
    float   time         = 0.f;
    float   dt           = 0.f;
    bool    keys[512]    = {};
    bool    mouseLeft    = false;
    float   mouseX       = 0.f;
    float   mouseY       = 0.f;
    float   camYaw       = 0.f;
    float   camPitch     = 20.f;
    float   camDist      = 5.f;
    int     renderMode   = 0;    // 0=Solid, 1=Wireframe, 2=Points, 3=Normals
    bool    showGizmos   = true;
    bool    showStats    = true;
    bool    showGrid     = true;
    NkEntityID selectedEntity = NK_INVALID_ENTITY;
};

static NkGraphicsApi ParseBackend(const NkVector<NkString>& args, bool& explicitBackend) {
    explicitBackend = false;
    for (usize i = 1; i < args.Size(); ++i) {
        const NkString& a = args[i];
        if (a == "--backend=opengl" || a == "--api=opengl" || a == "-bgl") {
            explicitBackend = true;
            return NkGraphicsApi::NK_API_OPENGL;
        }
        if (a == "--backend=vulkan" || a == "--api=vulkan" || a == "-bvk") {
            explicitBackend = true;
            return NkGraphicsApi::NK_API_VULKAN;
        }
        if (a == "--backend=dx11" || a == "--api=dx11" || a == "-bdx11") {
            explicitBackend = true;
            return NkGraphicsApi::NK_API_DIRECTX11;
        }
        if (a == "--backend=dx12" || a == "--api=dx12" || a == "-bdx12") {
            explicitBackend = true;
            return NkGraphicsApi::NK_API_DIRECTX12;
        }
        if (a == "--backend=metal" || a == "--api=metal" || a == "-bmtl") {
            explicitBackend = true;
            return NkGraphicsApi::NK_API_METAL;
        }
        if (a == "--backend=sw" || a == "--api=sw" || a == "-bsw") {
            explicitBackend = true;
            return NkGraphicsApi::NK_API_SOFTWARE;
        }
        if (a == "--backend=auto" || a == "--api=auto" || a == "-bauto") {
            explicitBackend = false;
            return NkGraphicsApi::NK_API_NONE;
        }
    }
    return NkGraphicsApi::NK_API_NONE;
}

static NkIDevice* CreateDeviceWithBackendFallback(const NkDeviceInitInfo& info,
                                                  NkGraphicsApi requestedApi,
                                                  bool explicitBackend) {
    if (explicitBackend && requestedApi != NkGraphicsApi::NK_API_NONE) {
        NkIDevice* explicitDev = NkDeviceFactory::CreateForApi(requestedApi, info);
        if (explicitDev && explicitDev->IsValid()) {
            return explicitDev;
        }
        if (explicitDev) {
            NkDeviceFactory::Destroy(explicitDev);
        }
        logger.Warn("[Demo] Backend demande indisponible: {0} - fallback auto",
                    NkGraphicsApiName(requestedApi));
    }

#if defined(NKENTSEU_PLATFORM_WINDOWS)
    return NkDeviceFactory::CreateWithFallback(info, {
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_DIRECTX12,
        NkGraphicsApi::NK_API_DIRECTX11,
        NkGraphicsApi::NK_API_VULKAN,
        NkGraphicsApi::NK_API_SOFTWARE
    });
#elif defined(NKENTSEU_PLATFORM_MACOS)
    return NkDeviceFactory::CreateWithFallback(info, {
        NkGraphicsApi::NK_API_METAL,
        NkGraphicsApi::NK_API_VULKAN,
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_SOFTWARE
    });
#else
    return NkDeviceFactory::CreateWithFallback(info, {
        NkGraphicsApi::NK_API_VULKAN,
        NkGraphicsApi::NK_API_OPENGL,
        NkGraphicsApi::NK_API_SOFTWARE
    });
#endif
}

// =============================================================================
// nkmain — point d'entrée
// =============================================================================
int nkmain(const NkEntryState& state) {

    // ── Fenêtre ───────────────────────────────────────────────────────────────
    NkWindowConfig winCfg;
    winCfg.title     = "NKRenderer Demo — PBR Multi-Material";
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;
    NkWindow window;
    if (!window.Create(winCfg)) { logger.Error("Window failed"); return 1; }

    // ── Device RHI ────────────────────────────────────────────────────────────
    bool explicitBackend = false;
    const NkGraphicsApi requestedApi = ParseBackend(state.GetArgs(), explicitBackend);

    NkDeviceInitInfo devInfo;
    devInfo.api         = requestedApi;
    devInfo.surface     = window.GetSurfaceDesc();
    devInfo.width       = window.GetSize().width;
    devInfo.height      = window.GetSize().height;
    NkIDevice* device   = CreateDeviceWithBackendFallback(devInfo, requestedApi, explicitBackend);
    if (!device || !device->IsValid()) { logger.Error("Device failed"); return 1; }
    logger.Info("[Demo] Backend RHI actif: {0}", NkGraphicsApiName(device->GetApi()));

    // ── NKRenderer ────────────────────────────────────────────────────────────
    NkRendererConfig rendCfg;
    rendCfg.width           = devInfo.width;
    rendCfg.height          = devInfo.height;
    rendCfg.enableShadows   = true;
    rendCfg.enableSSAO      = true;
    rendCfg.enableBloom     = true;
    rendCfg.enableFXAA      = true;
    rendCfg.enableIBL       = true;
    rendCfg.enableVFX       = true;
    rendCfg.fontSearchPath  = "Resources/Fonts";
    rendCfg.defaultHDRI     = "Resources/HDRI/studio.hdr"; // optionnel

    NkRenderer renderer;
    if (!renderer.Init(device, rendCfg)) { logger.Error("Renderer failed"); return 1; }

    // ── Post-processing ───────────────────────────────────────────────────────
    NkPostProcessSettings pp;
    pp.tonemap.mode       = 1;     // ACES
    pp.tonemap.exposure   = 1.f;
    pp.tonemap.gamma      = 2.2f;
    pp.bloom.enabled      = true;
    pp.bloom.threshold    = 1.f;
    pp.bloom.intensity    = 0.4f;
    pp.ssao.enabled       = true;
    pp.ssao.radius        = 0.4f;
    pp.ssao.power         = 2.f;
    pp.vignette.enabled   = true;
    pp.vignette.intensity = 0.3f;
    pp.fxaa               = true;
    renderer.SetPostProcess(pp);

    // ── Charger une police ────────────────────────────────────────────────────
    NkFont* font24 = renderer.Fonts().Load("Resources/Fonts/Roboto-Regular.ttf", 24);
    NkFont* font48 = renderer.Fonts().Load("Resources/Fonts/Roboto-Regular.ttf", 48);
    if (!font24) font24 = renderer.Fonts().GetDefault(24);

    // ── Charger des textures ──────────────────────────────────────────────────
    auto& texMgr = renderer.Textures();
    NkTexture2D* texBrickAlbedo  = texMgr.LoadTexture2D("Resources/Textures/PBR/gold/albedo.png",  NkTextureParams::Albedo());
    NkTexture2D* texBrickNormal  = texMgr.LoadTexture2D("Resources/Textures/PBR/gold/normal.png",  NkTextureParams::Normal());
    NkTexture2D* texBrickORM     = texMgr.LoadTexture2D("Resources/Textures/PBR/gold/ao.png",     NkTextureParams::Linear());
    NkTexture2D* texWoodAlbedo   = texMgr.LoadTexture2D("Resources/Textures/PBR/wall/albedo.png",   NkTextureParams::Albedo());
    NkTexture2D* texWoodNormal   = texMgr.LoadTexture2D("Resources/Textures/PBR/wall/normal.png",   NkTextureParams::Normal());
    NkTexture2D* texGrassAlbedo  = texMgr.LoadTexture2D("Resources/Textures/PBR/grass/albedo.png",  NkTextureParams::Albedo());
    NkTexture2D* texLeafAlbedo   = texMgr.LoadTexture2D("Resources/Textures/PBR/grass/albedo.png",   NkTextureParams::Albedo());
    NkTexture2D* texSkinAlbedo   = texMgr.LoadTexture2D("Resources/Textures/PBR/platic/albedo.png",   NkTextureParams::Albedo());
    NkTexture2D* texSkinNormal   = texMgr.LoadTexture2D("Resources/Textures/PBR/gold/normal.png",   NkTextureParams::Normal());
    NkTexture2D* texWaterNormal  = texMgr.LoadTexture2D("Resources/Textures/PBR/gold/normal.png",  NkTextureParams::Normal());
    NkTexture2D* texSprite       = texMgr.LoadTexture2D("Resources/Spritesheet/sp0.jpeg",  NkTextureParams::UISprite());

    // ── Créer les matériaux ───────────────────────────────────────────────────

    // PBR brique (matériau complet avec ORM)
    NkMaterial* matBrick = renderer.CreatePBRMaterial("Brick", {1,1,1,1}, 0.f, 0.5f, texBrickAlbedo, texBrickNormal, texBrickORM);
    matBrick->SetUVTiling(3.f, 3.f);

    // PBR bois (roughness élevée)
    NkMaterial* matWood = renderer.CreatePBRMaterial("Wood", {0.8f,0.6f,0.3f,1}, 0.f, 0.7f, texWoodAlbedo, texWoodNormal);
    matWood->SetRoughness(0.75f);

    // Métal poli (chrome)
    NkMaterial* matChrome = renderer.CreatePBRMaterial("Chrome", {0.9f,0.9f,0.95f,1}, 1.f, 0.05f);

    // Métal rouillé (rough + low metallic)
    NkMaterial* matRustedMetal = renderer.CreatePBRMaterial("RustedMetal", {0.6f,0.3f,0.2f,1}, 0.7f, 0.85f);

    // Plastique rouge
    NkMaterial* matPlastic = renderer.CreatePBRMaterial("Plastic", {0.9f,0.1f,0.1f,1}, 0.f, 0.3f);
    matPlastic->SetSpecular(0.5f);

    // Verre (transmission + IOR)
    NkMaterial* matGlass = renderer.CreateGlassMaterial("Glass", 1.52f,
        {0.9f,0.95f,1.f,0.1f});

    // Émissif bleu (panneaux LED, etc.)
    NkMaterial* matEmissive = renderer.CreateEmissiveMaterial("GlowBlue",
        {0.2f,0.5f,1.f,1.f}, 5.f);

    // Toon cartoon rouge
    NkMaterial* matToon = renderer.CreateToonMaterial("ToonRed",
        {0.9f,0.2f,0.1f,1.f}, 4, true);

    // Peau (SSS)
    NkMaterial* matSkin = renderer.CreateSkinMaterial("Skin",
        texSkinAlbedo, texSkinNormal, {1.0f,0.4f,0.3f,1.f});

    // Eau
    NkMaterial* matWater = renderer.CreateWaterMaterial("Water",
        texWaterNormal, {0.1f,0.4f,0.7f,0.6f});

    // Cheveux
    NkMaterial* matHair = renderer.CreateHairMaterial("Hair",
        {0.2f,0.1f,0.05f,1.f});

    // Voiture (clear coat)
    NkMaterial* matCar = renderer.CreateVehicleMaterial("CarPaint",
        {0.8f,0.1f,0.1f,1.f}, nullptr, 1.f);

    // Feuillage
    NkMaterial* matFoliage = renderer.CreateFoliageMaterial("Leaves",
        texLeafAlbedo, nullptr, 0.4f);

    // Sol herbe (Principled)
    NkMaterial* matGrass = renderer.CreatePBRMaterial("Grass",
        {0.3f,0.6f,0.2f,1}, 0.f, 0.9f, texGrassAlbedo);
    matGrass->SetSubsurfaceColor({0.2f,0.8f,0.1f,1.f});
    matGrass->SetSubsurfaceRadius(0.1f);
    matGrass->SetUVTiling(5.f, 5.f);

    // Wireframe debug
    NkMaterial* matWire = renderer.Materials().GetDefaultWireframe();

    // ── Charger des meshes ────────────────────────────────────────────────────
    auto& meshLib = renderer.Meshes();

    // Modèles externes (si disponibles)
    NkStaticMesh* meshSuzanne = meshLib.Load("Resources/Models/nanosuit/nanosuit.obj");
    NkStaticMesh* meshCar     = meshLib.Load("Resources/Models/LowPolyCars.obj");
    NkStaticMesh* meshTree    = meshLib.Load("Resources/Models/rock/rock.obj");

    // Primitives built-in
    NkStaticMesh* meshCube     = meshLib.GetCube();
    NkStaticMesh* meshSphere   = meshLib.GetSphere();
    NkStaticMesh* meshPlane    = meshLib.GetPlane();
    NkStaticMesh* meshCapsule  = meshLib.GetCapsule();
    NkStaticMesh* meshCylinder = meshLib.GetCylinder();
    NkStaticMesh* meshTorus    = meshLib.GetTorus();

    // Construire le wireframe pour overlay
    if (meshCube)   meshCube->BuildWireframe(device);
    if (meshSphere) meshSphere->BuildWireframe(device);

    // ── Créer la scène ────────────────────────────────────────────────────────
    NkScene scene;
    scene.Create(device, "DemoScene");

    // Caméra principale
    auto camEntity = scene.CreateCamera("MainCamera", {0, 2, 5});
    auto* camComp  = camEntity.GetCamera();
    camComp->isMainCamera = true;
    camComp->camera.fovDeg = 60.f;
    camComp->renderMode    = NkRenderMode::NK_SOLID;

    // ── Lumières ──────────────────────────────────────────────────────────────

    // Soleil (directionnel principal)
    auto sunEntity = scene.CreateLight("Sun", NkLightType::NK_DIRECTIONAL, {0,10,-5});
    auto* sunLight = sunEntity.GetLight();
    sunLight->SetType(NkLightType::NK_DIRECTIONAL);
    sunLight->SetColor({1.0f,0.95f,0.85f,1.f});
    sunLight->SetIntensity(3.f);
    sunLight->CastShadow(true);
    sunLight->light.direction = {0.3f,-1.f,0.5f};

    // Point light rouge (accentuation)
    auto redLight = scene.CreateLight("RedPoint", NkLightType::NK_POINT, {-3, 1.5f, 0});
    redLight.GetLight()->SetColor({1,0.2f,0.1f,1});
    redLight.GetLight()->SetIntensity(5.f);
    redLight.GetLight()->SetRange(8.f);
    redLight.GetLight()->CastShadow(false);

    // Spot bleu (ambiance)
    auto blueSpot = scene.CreateLight("BlueSpot", NkLightType::NK_SPOT, {3, 3, 2});
    blueSpot.GetLight()->SetType(NkLightType::NK_SPOT);
    blueSpot.GetLight()->SetColor({0.3f,0.5f,1.f,1});
    blueSpot.GetLight()->SetIntensity(8.f);
    blueSpot.GetLight()->SetRange(10.f);
    blueSpot.GetLight()->SetConeAngles(20.f, 35.f);
    blueSpot.GetLight()->light.direction = {-0.5f,-1.f,-0.3f};

    // ── Entités de la scène ───────────────────────────────────────────────────

    // Sol
    auto floor = scene.CreateMesh("Floor", meshPlane, matGrass, {0,-1,0});
    floor.GetTransform()->SetScale({10,1,10});

    // Cube brique (rotation)
    auto cube = scene.CreateMesh("BrickCube", meshCube, matBrick, {0, 0, 0});
    cube.GetTransform()->SetRotation({0, 45, 0});

    // Sphère chrome
    auto chromeSphere = scene.CreateMesh("ChromeSphere", meshSphere, matChrome, {2.5f, 0, 0});

    // Sphère verre
    auto glassSphere = scene.CreateMesh("GlassSphere", meshSphere, matGlass, {-2.5f, 0, 0});
    glassSphere.GetMesh()->castShadow = false;

    // Tore en bois
    auto torus = scene.CreateMesh("WoodTorus", meshTorus, matWood, {0, 0, -3});
    torus.GetTransform()->SetScale({1.5f,1.5f,1.5f});

    // Capsule peau (personnage stylisé)
    auto capsule = scene.CreateMesh("Character", meshCapsule, matSkin, {-4, 0, 0});

    // Cylindre toon
    auto cylinder = scene.CreateMesh("ToonCyl", meshCylinder, matToon, {4, 0, -2});

    // Voiture (si le modèle existe)
    if (meshCar) {
        auto car = scene.CreateMesh("Car", meshCar, matCar, {0, -1, -6});
        car.GetTransform()->SetScale({1,1,1});
    }

    // Arbre (si disponible)
    if (meshTree) {
        auto tree = scene.CreateMesh("Tree", meshTree, matFoliage, {5, -1, -4});
    }

    // Suzanne (si disponible)
    if (meshSuzanne) {
        auto suzy = scene.CreateMesh("Suzanne", meshSuzanne, matChrome, {-3, 0.5f, -3});
        suzy.GetMesh()->drawOutline = false;
    }

    // Panneau émissif
    auto glow = scene.CreateMesh("GlowPanel", meshCube, matEmissive, {0, 3, -5});
    glow.GetTransform()->SetScale({3, 0.1f, 0.5f});

    // Plan eau (transparent)
    auto water = scene.CreateMesh("Water", meshPlane, matWater, {3, -0.9f, -4});
    water.GetTransform()->SetScale({3,1,3});
    water.GetMesh()->receiveShadow = false;

    // Texte 3D billboard au-dessus des objets
    auto labelCube = scene.CreateText3D("LabelCube", "PBR BRICK", font48, {0,1.5f,0});
    auto labelChrome = scene.CreateText3D("LabelChrome", "CHROME", font48, {2.5f,1.2f,0});
    labelChrome.GetComponent<NkText3DComponent>()->style.color = {0.8f,0.9f,1.f,1.f};

    // Texte 3D extrudé (titre de la scène)
    auto* titleMesh = renderer.TextRenderer().CreateExtruded3DText(
        device, font48, "NKRenderer", matEmissive, {0.05f, 0.005f, 1});
    if (titleMesh) {
        auto titleEnt = scene.CreateMesh("Title3D", titleMesh, matEmissive, {-2, 4, -8});
        titleEnt.GetTransform()->SetScale({0.3f,0.3f,0.3f});
    }

    // Skybox
    scene.CreateSkybox(nullptr); // utilise la couleur par défaut

    // ── Particules / VFX ──────────────────────────────────────────────────────
    auto& vfx = renderer.VFX();

    // Feu sous le cube
    auto* fireSys = vfx.CreateFire({0, -0.5f, 0}, 0.5f);
    auto fireEnt  = scene.CreateParticles("Fire", fireSys, {0,-0.5f,0});

    // Magie bleue autour de la sphère de verre
    auto* magicSys = vfx.CreateMagic({-2.5f, 0, 0}, {0.3f,0.6f,1.f,1.f}, 0.8f);
    scene.CreateParticles("Magic", magicSys, {-2.5f, 0, 0});

    // Trail derrière un objet animé
    NkTrailDesc trailDesc;
    trailDesc.startWidth = 0.1f;
    trailDesc.endWidth   = 0.f;
    trailDesc.lifetime   = 0.5f;
    trailDesc.colorOverLife = NkColorGradient::WhiteToTransparent();
    auto* trailSys = new NkTrail();
    trailSys->Create(device, trailDesc);

    // ── Sprite 2D ─────────────────────────────────────────────────────────────
    NkSprite uiSprite;
    uiSprite.texture  = texSprite;
    uiSprite.position = {50, 50};
    uiSprite.size     = {64, 64};
    uiSprite.color    = {1,1,1,1};

    // ── État application ──────────────────────────────────────────────────────
    AppState app;
    NkClock clock;
    auto& events = NkEvents();
    uint32 W = devInfo.width;
    uint32 H = devInfo.height;

    // Modes de rendu cyclés
    const NkRenderMode renderModes[] = {
        NkRenderMode::NK_SOLID,
        NkRenderMode::NK_WIREFRAME,
        NkRenderMode::NK_POINTS,
        NkRenderMode::NK_NORMALS,
        NkRenderMode::NK_UV,
        NkRenderMode::NK_ALBEDO,
        NkRenderMode::NK_ROUGHNESS,
        NkRenderMode::NK_METALLIC,
        NkRenderMode::NK_AMBIENT_OCCLUSION,
    };
    const char* modeNames[] = {
        "Solid PBR", "Wireframe", "Points", "Normals",
        "UV", "Albedo", "Roughness", "Metallic", "AO"
    };
    int currentMode = 0;

    // ── Events ────────────────────────────────────────────────────────────────
    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) {
        app.running = false;
    });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        auto k = (uint32)e->GetKey();
        if (k < 512) app.keys[k] = true;
        if (e->GetKey() == NkKey::NK_ESCAPE)  app.running = false;
        if (e->GetKey() == NkKey::NK_TAB) {
            currentMode = (currentMode + 1) % 9;
            renderer.SetRenderMode(renderModes[currentMode]);
            logger.Info("[Demo] Mode: {0}", modeNames[currentMode]);
        }
        if (e->GetKey() == NkKey::NK_G)   app.showGizmos = !app.showGizmos;
        if (e->GetKey() == NkKey::NK_F5)  renderer.CaptureFrame("capture.png");
        if (e->GetKey() == NkKey::NK_F1)  renderer.SetupGameScene(scene);
        if (e->GetKey() == NkKey::NK_F2)  renderer.SetupModelingScene(scene);
        if (e->GetKey() == NkKey::NK_F3)  renderer.SetupFilmScene(scene);
        if (e->GetKey() == NkKey::NK_F4)  renderer.SetupArchScene(scene);
        if (e->GetKey() == NkKey::NK_F11) {
            // Toggle bloom
            pp.bloom.enabled = !pp.bloom.enabled;
            renderer.SetPostProcess(pp);
        }
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        auto k = (uint32)e->GetKey();
        if (k < 512) app.keys[k] = false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W = (uint32)e->GetWidth();
        H = (uint32)e->GetHeight();
        renderer.Resize(W, H);
    });
    events.AddEventCallback<NkMouseButtonPressEvent>([&](NkMouseButtonPressEvent* e) {
        if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
            // Picking — sélectionner l'objet sous le curseur
            NkRenderScene rs;
            scene.CollectRenderScene(rs, camComp->camera);
            uint32 id = renderer.Pick(rs, (uint32)app.mouseX, (uint32)app.mouseY);
            app.selectedEntity = (NkEntityID)id;
            if (id != 0)
                logger.Info("[Demo] Picked entity ID: {0}", id);
        }
    });
    events.AddEventCallback<NkMouseMoveEvent>([&](NkMouseMoveEvent* e) {
        app.mouseX = (float)e->GetX();
        app.mouseY = (float)e->GetY();
    });

    // ── Boucle de rendu ───────────────────────────────────────────────────────
    logger.Info("[Demo] Boucle principale — TAB=mode rendu G=gizmos F5=screenshot");
    logger.Info("[Demo] WASD=caméra, F1-F4=preset scène, F11=toggle bloom");

    while (app.running) {
        events.PollEvents();
        if (!app.running) break;
        if (W == 0 || H == 0) continue;
        if (W != device->GetSwapchainWidth() || H != device->GetSwapchainHeight())
            device->OnResize(W, H);

        // Delta time
        app.dt   = clock.Tick().delta;
        app.time += app.dt;
        if (app.dt > 0.1f) app.dt = 1.f/60.f;

        // ── Contrôles caméra ──────────────────────────────────────────────────
        const float camSpd = 60.f, zoomSpd = 3.f;
        if (app.keys[(uint32)NkKey::NK_A] || app.keys[(uint32)NkKey::NK_LEFT])
            app.camYaw -= camSpd * app.dt;
        if (app.keys[(uint32)NkKey::NK_D] || app.keys[(uint32)NkKey::NK_RIGHT])
            app.camYaw += camSpd * app.dt;
        if (app.keys[(uint32)NkKey::NK_W] || app.keys[(uint32)NkKey::NK_UP])
            app.camPitch += camSpd * app.dt;
        if (app.keys[(uint32)NkKey::NK_S] || app.keys[(uint32)NkKey::NK_DOWN])
            app.camPitch -= camSpd * app.dt;
        if (app.keys[(uint32)NkKey::NK_Q]) app.camDist -= zoomSpd * app.dt;
        if (app.keys[(uint32)NkKey::NK_E]) app.camDist += zoomSpd * app.dt;
        app.camPitch = NkClamp(app.camPitch, -85.f, 85.f);
        app.camDist  = NkClamp(app.camDist,  0.5f,  50.f);

        // Mettre à jour la caméra
        float cx = app.camDist * NkCos(NkToRadians(app.camPitch)) * NkSin(NkToRadians(app.camYaw));
        float cy = app.camDist * NkSin(NkToRadians(app.camPitch));
        float cz = app.camDist * NkCos(NkToRadians(app.camPitch)) * NkCos(NkToRadians(app.camYaw));
        camComp->camera.position = {cx, cy, cz};
        camComp->camera.target   = {0, 0, 0};

        // ── Animations ────────────────────────────────────────────────────────
        // Rotation du cube
        cube.GetTransform()->SetRotation({0, app.time * 45.f, 0});

        // Orbite de la lumière rouge
        float lx = 3.f * NkCos(app.time * 0.5f);
        float lz = 3.f * NkSin(app.time * 0.5f);
        redLight.GetTransform()->SetPosition({lx, 1.5f, lz});

        // Oscillation du tore
        torus.GetTransform()->SetRotation({0, app.time * 30.f, app.time * 15.f});

        // Mise à jour du trail (suit le cylinder)
        NkVec3f cylPos = {4.f + NkCos(app.time)*2.f, 0, -2.f + NkSin(app.time)*2.f};
        cylinder.GetTransform()->SetPosition(cylPos);
        trailSys->Update(app.dt, cylPos);

        // ── Mise à jour de la scène ───────────────────────────────────────────
        scene.Update(app.dt);
        vfx.Update(app.dt);
        // NkScene::Update resynchronise les cameras depuis leurs transforms.
        // On reapplique donc la camera orbitale juste apres pour le rendu de cette frame.
        camComp->camera.position = {cx, cy, cz};
        camComp->camera.target   = {0, 0, 0};

        // ── Rendu ─────────────────────────────────────────────────────────────
        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;
        W = device->GetSwapchainWidth();
        H = device->GetSwapchainHeight();
        if (W == 0 || H == 0) { device->EndFrame(frame); continue; }

        NkICommandBuffer* cmd = device->CreateCommandBuffer();
        cmd->Begin();

        // ── Passe 3D ──────────────────────────────────────────────────────────
        // NkRenderScene rs;
        // rs.SetCamera(camComp->camera);
        // scene.CollectRenderScene(rs, camComp->camera, renderModes[currentMode]);

        // // Overlay outline sur l'entité sélectionnée
        // if (app.selectedEntity != NK_INVALID_ENTITY) {
        //     NkEntity selEnt = scene.Find(app.selectedEntity);
        //     if (selEnt.IsValid() && selEnt.HasComponent<NkMeshComponent>()) {
        //         selEnt.GetMesh()->drawOutline = true;
        //     }
        // }

        // renderer.Render3D(cmd, rs);

        // // Trail
        // if (true && trailSys->HasGeometry()) {
        //     vfx.Render(cmd, camComp->camera,
        //                camComp->camera.GetView(),
        //                camComp->camera.GetProjection((float)W/(float)H));
        // }

        // // Gizmos debug (axes, grille)
        // if (true && app.showGizmos) {
        //     renderer.DrawGrid(cmd, camComp->camera, 20.f, 20);
        //     renderer.DrawAxis(cmd, NkMat4f::Identity(), 1.f);

        //     // AABB de l'entité sélectionnée
        //     if (app.selectedEntity != NK_INVALID_ENTITY) {
        //         NkEntity selEnt = scene.Find(app.selectedEntity);
        //         if (selEnt.IsValid() && selEnt.HasComponent<NkMeshComponent>()) {
        //             auto* mc = selEnt.GetMesh();
        //             if (mc->mesh) {
        //                 auto* t = selEnt.GetTransform();
        //                 renderer.DrawAABB(cmd, mc->mesh->GetAABB(),
        //                                    t ? t->worldMatrix : NkMat4f::Identity(),
        //                                    {1,0.8f,0,1});
        //             }
        //         }
        //     }

        //     // Gizmo lumières
        //     for (auto* lc : scene.GetLights()) {
        //         if (lc->visualize) {
        //             auto* tc = scene.GetComponent<NkTransformComponent>(lc->owner);
        //             NkVec3f lpos = tc ? tc->GetWorldPosition() : lc->light.position;
        //             renderer.DrawLightGizmo(cmd, lc->light, lpos, lc->light.color);
        //         }
        //     }
        // }

        // // ── Passe 2D (overlay) ────────────────────────────────────────────────
        // renderer.Render2D(cmd, [&](NkRenderer2D& r2d) {
        //     // Fond du panneau stats
        //     if (app.showStats) {
        //         r2d.DrawRoundedRect(5, 5, 280, 130, 8,
        //             NkShapeStyle{{0,0,0,0.6f}, {}, 0, true, false, 0.f});
        //     }

        //     // Barre de progression FPS
        //     float fps = app.dt > 0 ? 1.f/app.dt : 60.f;
        //     float fpsBarW = NkClamp(fps/120.f, 0.f, 1.f) * 260.f;
        //     // r2d.DrawRect(10, 70, fpsBarW, 8, {0.2f,0.8f,0.3f,0.8f}, 0.f);
        //     // r2d.DrawRect(10, 70, 260, 8, NkColor4f{0,0,0,0}); // border

        //     NkShapeStyle barStyle;
        //     barStyle.fillColor = {0.2f, 0.8f, 0.3f, 0.8f};
        //     barStyle.filled = true;
        //     barStyle.stroked = false;
        //     r2d.DrawRect(10, 70, fpsBarW, 8, barStyle);

        //     NkShapeStyle borderStyle;
        //     borderStyle.fillColor = {0,0,0,0};
        //     borderStyle.strokeColor = {0,0,0,0.5f};
        //     borderStyle.strokeWidth = 1.f;
        //     borderStyle.filled = false;
        //     borderStyle.stroked = true;
        //     r2d.DrawRect(10, 70, 260, 8, borderStyle);

        //     // Logo NKRenderer
        //     r2d.DrawCircle(W-40, 40, 25, NkShapeStyle{
        //         {0.1f,0.3f,0.9f,0.8f}, {1,1,1,1}, 2.f, true, true
        //     });

        //     // Indicateur mode de rendu (coin haut droit)
        //     r2d.DrawRoundedRect((float)W-200, 5, 195, 40, 6,
        //         NkShapeStyle{{0,0,0.5f,0.7f}, {}, 0, true, false});

        //     // Grille UV debug si mode UV
        //     if (renderModes[currentMode] == NkRenderMode::NK_UV) {
        //         const int G = 8;
        //         for (int i=0; i<=G; i++) {
        //             float x = 200.f + (float)i * 200.f/G;
        //             float y = (float)H - 210.f + (float)i * 200.f/G;
        //             r2d.DrawLine(200, (float)H-210, 400, (float)H-210+(float)i*200/G,
        //                          {0.5f,0.5f,1,0.3f}, 1.f);
        //         }
        //     }

        //     // Sprite UI (si texture disponible)
        //     if (texSprite) r2d.DrawSprite(uiSprite);

        //     // Cercles décoratifs animés
        //     for (int i=0; i<5; i++) {
        //         float t = app.time + i * 0.4f;
        //         float x = (float)W - 80.f + NkCos(t * 2.f) * 30.f;
        //         float y = (float)H - 80.f + NkSin(t * 2.f) * 30.f;
        //         r2d.DrawCircle(x, y, 5.f + i*2.f,
        //             NkShapeStyle{{(float)i/5.f, 0.3f, 1-i/5.f, 0.6f}});
        //     }
        // });

        // // ── Texte 2D ──────────────────────────────────────────────────────────
        // renderer.RenderText(cmd, [&](NkTextRenderer& tr) {
        //     if (!font24) return;

        //     NkTextStyle white  = {};
        //     NkTextStyle yellow;  yellow.color  = {1,1,0,1};
        //     NkTextStyle green;   green.color   = {0.3f,1,0.3f,1};
        //     NkTextStyle shadow;  shadow.shadow = true; shadow.shadowOffset = {2,2};
        //     NkTextStyle outline; outline.outlineWidth = 2.f; outline.outlineColor = {0,0,0,1};

        //     if (app.showStats) {
        //         float fps = app.dt > 0 ? 1.f/app.dt : 60.f;
        //         char buf[128];

        //         // FPS
        //         snprintf(buf, sizeof(buf), "FPS: %.0f  (%.2f ms)", fps, app.dt*1000.f);
        //         tr.DrawText2D(font24, buf, 15, 15, yellow);

        //         // Mode de rendu
        //         snprintf(buf, sizeof(buf), "Mode: %s", modeNames[currentMode]);
        //         tr.DrawText2D(font24, buf, 15, 40, green);

        //         // Stats de rendu
        //         auto stats = renderer.GetStats();
        //         snprintf(buf, sizeof(buf), "Draw: %u  Tris: %u  Inst: %u",
        //                  stats.drawCalls, stats.triangles, stats.instances);
        //         tr.DrawText2D(font24, buf, 15, 65, white);

        //         // Aide clavier
        //         // tr.DrawText2D(font24, "TAB:Mode  G:Gizmos  F5:Screenshot  Q/E:Zoom", 15, 95, {0.7f,0.7f,0.7f,1});
        //         // tr.DrawText2D(font24, "WASD:Cam  F1-F4:Presets  F11:Bloom  Click:Pick", 15, 115, {0.7f,0.7f,0.7f,1});
        //         NkTextStyle grayStyle;
        //         grayStyle.color = {0.7f, 0.7f, 0.7f, 1.f};
        //         // tr.DrawText2D(font24, buf, 15, 95, grayStyle);
        //         // tr.DrawText2D(font24, buf, 15, 115, grayStyle);
        //         tr.DrawText2D(font24, "TAB:Mode  G:Gizmos  F5:Screenshot  Q/E:Zoom", 15, 95, grayStyle);
        //         tr.DrawText2D(font24, "WASD:Cam  F1-F4:Presets  F11:Bloom  Click:Pick", 15, 115, grayStyle);
        //     }

        //     // Titre en haut à droite avec ombre
        //     NkTextStyle titleStyle;
        //     titleStyle.shadow = true;
        //     titleStyle.shadowOffset = {3,3};
        //     titleStyle.shadowColor  = {0,0,0,0.5f};
        //     titleStyle.gradient = true;
        //     titleStyle.gradientTop = {1,1,1,1};
        //     titleStyle.gradientBot = {0.7f,0.8f,1,1};
        //     tr.DrawText2D(font24, "NKRenderer", (float)W-195, 10,
        //                   titleStyle, NkTextAlign::NK_LEFT);

        //     // Mode courant en haut à droite
        //     tr.DrawText2D(font24, modeNames[currentMode], (float)W-195, 32,
        //                   outline, NkTextAlign::NK_LEFT);

        //     // Texte 3D billboard (synchronisé avec la caméra)
        //     auto matView = camComp->camera.GetView();
        //     auto matProj = camComp->camera.GetProjection((float)W/(float)H);

        //     if (font48) {
        //         NkTextStyle lblStyle;
        //         lblStyle.shadow = true; lblStyle.shadowColor = {0,0,0,0.5f};

        //         tr.DrawText3DBillboard(font48, "CUBE",
        //             {0, 1.4f, 0}, matView, matProj, 0.6f, lblStyle);
        //         tr.DrawText3DBillboard(font48, "CHROME",
        //             {2.5f, 1.2f, 0}, matView, matProj, 0.5f, lblStyle);
        //         tr.DrawText3DBillboard(font48, "GLASS",
        //             {-2.5f, 1.2f, 0}, matView, matProj, 0.5f, lblStyle);
        //         tr.DrawText3DBillboard(font48, "TOON",
        //             {4.f, 1.4f, -2}, matView, matProj, 0.5f, lblStyle);

        //         // Texte 3D ancré (reste à une position world)
        //         tr.DrawText3DAnchored(font24, "NKRenderer Demo",
        //             {0, 5, -10}, matView * matProj, 0.8f, yellow, NkTextAlign::NK_CENTER);
        //     }
        // });

        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);
    }

    // ── Nettoyage ─────────────────────────────────────────────────────────────
    device->WaitIdle();

    delete trailSys;
    scene.Destroy();
    renderer.Shutdown();
    NkDeviceFactory::Destroy(device);
    window.Close();

    logger.Info("[Demo] Terminé proprement.");
    return 0;
}
