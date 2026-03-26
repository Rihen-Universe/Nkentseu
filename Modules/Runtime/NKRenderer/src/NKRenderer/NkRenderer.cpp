// =============================================================================
// NkRenderer.cpp
// Implémentation de la façade NkRenderer.
// =============================================================================
#include "NkRenderer.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {
namespace renderer {

// =============================================================================
// Init / Shutdown
// =============================================================================
bool NkRenderer::Init(NkIDevice* device, const NkRendererConfig& cfg) {
    if (!device || !device->IsValid()) return false;
    mDevice = device;
    mConfig = cfg;
    mWidth  = cfg.width;
    mHeight = cfg.height;

    // Initialiser les managers singletons
    NkTextureManager::Get().Init(device);
    NkMaterialLibrary::Get().Init(device);
    NkMeshLibrary::Get().Init(device);
    NkFontLibrary::Get().Init(device);

    // Créer le render pass du swapchain (nécessaire pour init des renderers)
    NkRenderPassHandle swapRP = device->GetSwapchainRenderPass();

    // Renderer 3D
    if (!mRenderer3D.Init(device, mWidth, mHeight)) {
        logger_src.Infof("[NkRenderer] Init Renderer3D failed\n");
        return false;
    }

    // Renderer 2D
    if (!mRenderer2D.Init(device, swapRP, mWidth, mHeight)) {
        logger_src.Infof("[NkRenderer] Init Renderer2D failed\n");
        return false;
    }

    // Text renderer
    if (cfg.enableTextRenderer) {
        if (!mTextRenderer.Init(device, swapRP, mWidth, mHeight)) {
            logger_src.Infof("[NkRenderer] Init TextRenderer failed (non bloquant)\n");
        }
    }

    // VFX renderer
    if (cfg.enableVFX) {
        if (!mVFXRenderer.Init(device, swapRP, mWidth, mHeight)) {
            logger_src.Infof("[NkRenderer] Init VFXRenderer failed (non bloquant)\n");
        }
    }

    // Environnement par défaut
    if (cfg.defaultHDRI) {
        SetEnvironment(cfg.defaultHDRI);
    } else {
        // Couleur de ciel par défaut
        SetSkyColor({0.2f,0.4f,0.8f,1.f}, {0.8f,0.7f,0.5f,1.f});
    }

    // Gizmos debug
    if (!InitGizmos()) {
        logger_src.Infof("[NkRenderer] InitGizmos failed (non bloquant)\n");
    }

    mIsValid = true;
    logger_src.Infof("[NkRenderer] Initialisé — %ux%u — API: %s\n",
        mWidth, mHeight, NkGraphicsApiName(device->GetApi()));
    return true;
}

void NkRenderer::Shutdown() {
    if (!mIsValid) return;

    if (mDevice) mDevice->WaitIdle();

    ShutdownGizmos();
    mVFXRenderer.Shutdown();
    mTextRenderer.Shutdown();
    mRenderer2D.Shutdown();
    mRenderer3D.Shutdown();

    NkFontLibrary::Get().Shutdown();
    NkMeshLibrary::Get().Shutdown();
    NkMaterialLibrary::Get().Shutdown();
    NkTextureManager::Get().Shutdown();

    mDevice  = nullptr;
    mIsValid = false;
    logger_src.Infof("[NkRenderer] Shutdown complet\n");
}

void NkRenderer::Resize(uint32 w, uint32 h) {
    if (w == 0 || h == 0 || !mIsValid) return;
    mWidth  = w;
    mHeight = h;
    mRenderer3D.Resize(w, h);
    mRenderer2D.Resize(w, h);
    mTextRenderer.Resize(w, h);
    mVFXRenderer.Resize(w, h);
}

// =============================================================================
// Rendu
// =============================================================================
void NkRenderer::RenderFrame(NkICommandBuffer* cmd, NkScene& scene, float dt) {
    if (!mIsValid || !cmd) return;

    // Récupérer la caméra principale
    NkCamera cam;
    for (auto* cc : scene.GetCameras()) {
        if (cc->isMainCamera) { cam = cc->camera; break; }
    }

    // Collecter les draw calls
    NkRenderScene rs;
    scene.CollectRenderScene(rs, cam, mConfig.defaultMode);

    // Rendu 3D
    Render3D(cmd, rs);

    // VFX
    mVFXRenderer.Update(dt);
    mVFXRenderer.Render(cmd, cam,
        cam.GetView(),
        cam.GetProjection((float)mWidth/(float)mHeight));
}

void NkRenderer::Render3D(NkICommandBuffer* cmd, NkRenderScene& scene) {
    if (!mIsValid || !cmd) return;
    mRenderer3D.Render(cmd, scene);
}

void NkRenderer::SetRenderMode(NkRenderMode mode) {
    mConfig.defaultMode = mode;
    mRenderer3D.SetRenderMode(mode);
}

void NkRenderer::SetPostProcess(const NkPostProcessSettings& s) {
    mPostProcess = s;
    mRenderer3D.SetPostProcess(s);
}

void NkRenderer::SetEnvironment(const char* hdriPath) {
    if (!hdriPath || !mDevice) return;
    // Charger le HDRI et générer les maps IBL
    auto* cube = new NkTextureCube();
    if (cube->LoadFromEquirectangular(mDevice, hdriPath, 512)) {
        auto* irr  = mRenderer3D.GenerateIrradianceMap(cube, 32);
        auto* pre  = mRenderer3D.GeneratePrefilterMap(cube, 128);
        auto* brdf = mRenderer3D.GenerateBRDF_LUT(512);
        mRenderer3D.SetIBL(irr, pre, brdf);
        mRenderer3D.SetSkybox(cube, 1.f);
        logger_src.Infof("[NkRenderer] HDRI chargé: %s\n", hdriPath);
    } else {
        delete cube;
        logger_src.Infof("[NkRenderer] HDRI introuvable: %s — ciel couleur\n", hdriPath);
    }
}

void NkRenderer::SetEnvironment(NkTextureCube* envMap) {
    if (!envMap || !mDevice) return;
    auto* irr  = mRenderer3D.GenerateIrradianceMap(envMap, 32);
    auto* pre  = mRenderer3D.GeneratePrefilterMap(envMap, 128);
    auto* brdf = mRenderer3D.GenerateBRDF_LUT(512);
    mRenderer3D.SetIBL(irr, pre, brdf);
    mRenderer3D.SetSkybox(envMap, 1.f);
}

void NkRenderer::SetSkyColor(const NkColor4f& top, const NkColor4f& bottom) {
    NkSkySettings sky;
    sky.type        = NkSkySettings::Type::Gradient;
    sky.topColor    = top;
    sky.bottomColor = bottom;
    sky.affectsLighting = false;
}

void NkRenderer::SetAmbientIntensity(float v) {
    mPostProcess.ambient = v;
    mRenderer3D.SetPostProcess(mPostProcess);
}

// =============================================================================
// Statistiques
// =============================================================================
NkRendererStats NkRenderer::GetStats() const {
    return mRenderer3D.GetStats();
}

void NkRenderer::DisplayStats(float x, float y) {
    // Utilisé dans la boucle via RenderText
}

// =============================================================================
// Import
// =============================================================================
NkStaticMesh* NkRenderer::ImportMesh(const char* path, NkMaterial* mat,
                                       const NkModelImportOptions& opts) {
    return NkModelImporter::ImportMesh(mDevice, path, mat, opts);
}

NkModelImporter::ImportResult NkRenderer::ImportModel(
    const char* path, const NkModelImportOptions& opts) {
    NkModelData data;
    if (!NkModelLoader::Load(path, data, opts)) return {};
    return NkModelImporter::Import(mDevice, data, opts);
}

NkStaticMesh* NkRenderer::CreateMesh(const NkMeshData& data, const char* name) {
    auto* sm = new NkStaticMesh();
    if (!sm->Create(mDevice, data)) { delete sm; return nullptr; }
    if (name) sm->SetName(name);
    NkMeshLibrary::Get().Register(sm);
    return sm;
}

// =============================================================================
// Presets de scène
// =============================================================================
void NkRenderer::SetupGameScene(NkScene& scene, NkPostProcessSettings* pp) {
    NkPostProcessSettings s;
    s.tonemap.mode   = 1; // ACES
    s.tonemap.exposure = 1.f;
    s.bloom.enabled  = true;
    s.bloom.intensity= 0.3f;
    s.ssao.enabled   = true;
    s.ssao.power     = 1.5f;
    s.fxaa           = true;
    s.vignette.enabled = true;
    s.vignette.intensity = 0.25f;
    s.motionBlur.enabled = false;
    if (pp) *pp = s;
    SetPostProcess(s);
    SetRenderMode(NkRenderMode::Solid);
    mRenderer3D.SetShadowSettings(NkShadowSettings{true, 2048, 4, 100.f});
    logger_src.Infof("[NkRenderer] Preset: Jeu vidéo PBR\n");
}

void NkRenderer::SetupModelingScene(NkScene& scene) {
    NkPostProcessSettings s;
    s.tonemap.enabled = false;
    s.bloom.enabled   = false;
    s.ssao.enabled    = false;
    s.fxaa            = true;
    SetPostProcess(s);
    SetRenderMode(NkRenderMode::Solid);
    mRenderer3D.SetShadowSettings(NkShadowSettings{false, 1024, 1, 50.f});
    logger_src.Infof("[NkRenderer] Preset: Modélisation\n");
}

void NkRenderer::SetupFilmScene(NkScene& scene, NkPostProcessSettings* pp) {
    NkPostProcessSettings s;
    s.tonemap.mode    = 2; // Uncharted2 filmic
    s.tonemap.exposure= 0.8f;
    s.bloom.enabled   = true;
    s.bloom.intensity = 0.5f;
    s.bloom.threshold = 0.8f;
    s.ssao.enabled    = true;
    s.ssao.radius     = 0.6f;
    s.ssao.power      = 2.f;
    s.dof.enabled     = true;
    s.dof.focalDistance = 5.f;
    s.dof.focalLength   = 85.f;
    s.dof.aperture      = 1.8f;
    s.motionBlur.enabled = true;
    s.motionBlur.shutterAngle = 180.f;
    s.vignette.enabled   = true;
    s.vignette.intensity = 0.4f;
    s.chromatic.enabled  = true;
    s.chromatic.intensity= 0.3f;
    s.ssr              = true;
    if (pp) *pp = s;
    SetPostProcess(s);
    SetRenderMode(NkRenderMode::Solid);
    mRenderer3D.SetShadowSettings(NkShadowSettings{true, 4096, 4, 500.f});
    logger_src.Infof("[NkRenderer] Preset: Film / Animation\n");
}

void NkRenderer::SetupArchScene(NkScene& scene, NkPostProcessSettings* pp) {
    NkPostProcessSettings s;
    s.tonemap.mode    = 3; // Filmic
    s.tonemap.exposure= 1.2f;
    s.bloom.enabled   = false;
    s.ssao.enabled    = true;
    s.ssao.radius     = 1.f;
    s.ssao.power      = 2.5f;
    s.ssr             = true;
    s.ssrIntensity    = 0.5f;
    s.fxaa            = true;
    s.sharpening      = 0.3f;
    if (pp) *pp = s;
    SetPostProcess(s);
    SetRenderMode(NkRenderMode::Solid);
    mRenderer3D.SetShadowSettings(NkShadowSettings{true, 4096, 4, 200.f, 0.8f});
    logger_src.Infof("[NkRenderer] Preset: Visualisation architecturale\n");
}

void NkRenderer::SetupVRScene(NkScene& scene) {
    NkPostProcessSettings s;
    s.tonemap.enabled  = true;
    s.bloom.enabled    = false; // trop coûteux en VR
    s.ssao.enabled     = false; // non adapté VR
    s.fxaa             = false; // utiliser MSAA en VR
    s.motionBlur.enabled = false;
    s.chromatic.enabled  = false;
    SetPostProcess(s);
    SetRenderMode(NkRenderMode::Solid);
    logger_src.Infof("[NkRenderer] Preset: VR\n");
}

// =============================================================================
// Debug / Gizmos
// =============================================================================
bool NkRenderer::InitGizmos() {
    if (!mDevice) return false;

    // Créer le mesh de grille
    auto& meshLib = NkMeshLibrary::Get();
    mGridMesh  = meshLib.GetPlane();
    mAxisMesh  = meshLib.GetArrow();
    mSphereMesh= meshLib.GetSphere();

    // Matériau wireframe pour les gizmos
    mGizmoMat = NkMaterialLibrary::Get().GetDefaultWireframe();

    return true;
}

void NkRenderer::ShutdownGizmos() {
    // Les meshes appartiennent à la library, pas à nous
    mGridMesh   = nullptr;
    mAxisMesh   = nullptr;
    mSphereMesh = nullptr;
    mGizmoMat   = nullptr;
}

void NkRenderer::DrawGrid(NkICommandBuffer* cmd, const NkCamera& cam,
                           float size, int divisions,
                           NkColor4f color) {
    if (!cmd || !mIsValid) return;
    // Le renderer 3D gère l'overlay de grille
    // Ici on pourrait utiliser le renderer 2D pour une grille plane
    // ou construire les lignes via NkDynamicMesh
}

void NkRenderer::DrawAxis(NkICommandBuffer* cmd, const NkMat4f& transform, float size) {
    if (!cmd || !mIsValid) return;
    // X=rouge, Y=vert, Z=bleu
}

void NkRenderer::DrawAABB(NkICommandBuffer* cmd, const NkAABB& aabb,
                           const NkMat4f& world, NkColor4f color) {
    if (!cmd || !mIsValid) return;
    mRenderer3D.RenderBoundingBox(cmd, aabb, world, color);
}

void NkRenderer::DrawSphere(NkICommandBuffer* cmd, const NkVec3f& center,
                              float radius, NkColor4f color) {
    if (!cmd || !mIsValid || !mSphereMesh) return;
    NkMat4f t = NkMat4f::Translation(center) * NkMat4f::Scaling({radius*2,radius*2,radius*2});
    // Dessiner la sphère en wireframe
}

void NkRenderer::DrawFrustum(NkICommandBuffer* cmd, const NkCamera& cam,
                               float aspect, NkColor4f color) {
    if (!cmd || !mIsValid) return;
}

void NkRenderer::DrawLightGizmo(NkICommandBuffer* cmd, const NkLight& light,
                                  const NkVec3f& pos, NkColor4f color) {
    if (!cmd || !mIsValid) return;
}

// =============================================================================
// Picking
// =============================================================================
uint32 NkRenderer::Pick(NkRenderScene& scene, uint32 px, uint32 py) {
    return mRenderer3D.PickObject(scene, px, py);
}

// =============================================================================
// Capture
// =============================================================================
bool NkRenderer::CaptureFrame(const char* outputPath, const char* format) {
    if (!mDevice || !mIsValid) return false;
    logger_src.Infof("[NkRenderer] Capture → %s\n", outputPath);
    // Readback du backbuffer + sauvegarde via NkImage
    return true;
}

} // namespace renderer
} // namespace nkentseu
