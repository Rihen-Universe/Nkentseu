#pragma once
// =============================================================================
// NkRenderer.h
// Façade principale du NKRenderer — point d'entrée unique.
//
// Utilisation typique :
//
//   NkRenderer renderer;
//   renderer.Init(device, width, height);
//
//   // Charger des ressources
//   auto* mat  = renderer.Materials().GetDefaultPBR();
//   auto* mesh = renderer.Meshes().Load("Resources/Models/suzanne.obj");
//   auto* tex  = renderer.Textures().LoadTexture2D("Resources/Textures/brick.png");
//   mat->SetAlbedoMap(tex)->SetRoughness(0.7f)->SetMetallic(0.0f);
//
//   // Créer une scène
//   NkScene scene;
//   scene.Create(device, "MyScene");
//   auto entity = scene.CreateMesh("Suzanne", mesh, mat, {0,0,0});
//   auto light  = scene.CreateLight("Sun", NkLightType::NK_DIRECTIONAL, {0,10,0});
//   auto cam    = scene.CreateCamera("MainCam", {0,2,5});
//
//   // Boucle de rendu
//   while (running) {
//       scene.Update(dt);
//
//       NkRenderScene rs;
//       scene.CollectRenderScene(rs, cam.GetCamera()->camera);
//
//       NkICommandBuffer* cmd = device->CreateCommandBuffer();
//       cmd->Begin();
//       renderer.Render3D(cmd, rs);
//       renderer.Render2D(cmd, [&](NkRenderer2D& r2d) {
//           r2d.DrawText(font, "Hello World", 10, 10);
//       });
//       cmd->End();
//       device->SubmitAndPresent(cmd);
//   }
//
//   renderer.Shutdown();
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKRenderer/Core/NkShaderAsset.h"
#include "NKRenderer/Material/NkMaterial.h"
#include "NKRenderer/Mesh/NkMesh.h"
#include "NKRenderer/Renderer/NkRenderer3D.h"
#include "NKRenderer/Renderer/NkRenderer2D.h"
#include "NKRenderer/Text/NkTextRenderer.h"
#include "NKRenderer/VFX/NkVFXSystem.h"
#include "NKRenderer/Scene/NkScene.h"
#include "NKRenderer/Loader/NkModelLoader.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Configuration d'initialisation
        // =============================================================================
        struct NkRendererConfig {
            uint32  width             = 1280;
            uint32  height            = 720;
            bool    enableShadows     = true;
            bool    enableSSAO        = true;
            bool    enableBloom       = true;
            bool    enableFXAA        = true;
            bool    enableIBL         = true;
            bool    enableVFX         = true;
            bool    enableTextRenderer= true;
            bool    deferred          = true;   // false = forward rendering
            uint32  shadowMapSize     = 2048;
            uint32  maxParticles      = 100000;
            uint32  maxLights         = 256;
            NkRenderMode defaultMode  = NkRenderMode::NK_SOLID;
            const char* defaultHDRI   = nullptr;
            const char* fontSearchPath= "Resources/Fonts";
            const char* textureSearchPath = "Resources/Textures";
            const char* modelSearchPath   = "Resources/Models";
            const char* shaderCachePath   = "Build/ShaderCache";
        };

        // =============================================================================
        // NkRenderer — façade principale
        // =============================================================================
        class NkRenderer {
            public:
                NkRenderer()  = default;
                ~NkRenderer() { Shutdown(); }
                NkRenderer(const NkRenderer&) = delete;
                NkRenderer& operator=(const NkRenderer&) = delete;

                // ── Cycle de vie ──────────────────────────────────────────────────────────
                bool Init(NkIDevice* device, const NkRendererConfig& cfg = {});
                void Shutdown();
                void Resize(uint32 width, uint32 height);
                bool IsValid() const { return mIsValid; }

                // ── Accès aux sous-systèmes ────────────────────────────────────────────────
                NkTextureManager&   Textures()   { return NkTextureManager::Get(); }
                NkMaterialLibrary&  Materials()  { return NkMaterialLibrary::Get(); }
                NkMeshLibrary&      Meshes()     { return NkMeshLibrary::Get(); }
                NkFontLibrary&      Fonts()      { return NkFontLibrary::Get(); }
                NkRenderer3D&       Renderer3D() { return mRenderer3D; }
                NkRenderer2D&       Renderer2D() { return mRenderer2D; }
                NkTextRenderer&     TextRenderer() { return mTextRenderer; }
                NkVFXRenderer&      VFX()        { return mVFXRenderer; }

                // ── Rendu ─────────────────────────────────────────────────────────────────

                // Rendre une scène complète (3D + 2D overlay + text + VFX)
                void RenderFrame(NkICommandBuffer* cmd, NkScene& scene, float dt = 0.f);

                // Rendu 3D standalone
                void Render3D(NkICommandBuffer* cmd, NkRenderScene& scene);

                // Rendu 2D avec callback (batch automatique)
                // Usage : renderer.Render2D(cmd, [&](NkRenderer2D& r2d) { r2d.DrawRect(...); });
                template<typename Fn>
                void Render2D(NkICommandBuffer* cmd, Fn&& drawFn);

                // Rendu texte avec callback
                template<typename Fn>
                void RenderText(NkICommandBuffer* cmd, Fn&& drawFn);

                // ── Configuration du mode de rendu ────────────────────────────────────────
                void SetRenderMode(NkRenderMode mode);
                NkRenderMode GetRenderMode() const { return mConfig.defaultMode; }

                // ── Post-processing ───────────────────────────────────────────────────────
                void SetPostProcess(const NkPostProcessSettings& settings);
                NkPostProcessSettings& GetPostProcess() { return mPostProcess; }

                // ── Environnement ─────────────────────────────────────────────────────────
                void SetEnvironment(const char* hdriPath);
                void SetEnvironment(NkTextureCube* envMap);
                void SetSkyColor(const NkColor4f& top, const NkColor4f& bottom);
                void SetAmbientIntensity(float v);

                // ── Statistiques ──────────────────────────────────────────────────────────
                NkRendererStats GetStats() const;
                void            DisplayStats(float x = 10.f, float y = 10.f); // overlay debug

                // ── Debug / Gizmos ────────────────────────────────────────────────────────
                void DrawGrid    (NkICommandBuffer* cmd, const NkCamera& cam, float size=10.f,
                                int divisions=10, NkColor4f color={0.3f,0.3f,0.3f,1.f});
                void DrawAxis    (NkICommandBuffer* cmd, const NkMat4f& transform, float size=1.f);
                void DrawAABB    (NkICommandBuffer* cmd, const NkAABB& aabb, const NkMat4f& world,
                                NkColor4f color={1,1,0,1});
                void DrawSphere  (NkICommandBuffer* cmd, const NkVec3f& center, float radius,
                                NkColor4f color={1,1,0,1});
                void DrawFrustum (NkICommandBuffer* cmd, const NkCamera& cam, float aspect,
                                NkColor4f color={0,1,1,1});
                void DrawLightGizmo(NkICommandBuffer* cmd, const NkLight& light,
                                    const NkVec3f& pos, NkColor4f color={1,1,0,1});

                // ── Picking ───────────────────────────────────────────────────────────────
                // Obtenir l'entité sous un pixel (nécessite une passe ID)
                uint32 Pick(NkRenderScene& scene, uint32 pixelX, uint32 pixelY);

                // ── Capture / Screenshot ──────────────────────────────────────────────────
                bool CaptureFrame(const char* outputPath, const char* format = "png");

                // ── Accesseurs ────────────────────────────────────────────────────────────
                NkIDevice*         GetDevice()  const { return mDevice; }
                uint32             GetWidth()   const { return mWidth; }
                uint32             GetHeight()  const { return mHeight; }
                float              GetAspect()  const { return mHeight > 0 ? (float)mWidth/(float)mHeight : 1.f; }
                const NkRendererConfig& GetConfig() const { return mConfig; }

                // ── Import de modèles 3D ──────────────────────────────────────────────────
                NkStaticMesh* ImportMesh(const char* path, NkMaterial* mat = nullptr,
                                        const NkModelImportOptions& opts = {});

                // Import complet avec hiérarchie
                NkModelImporter::ImportResult ImportModel(const char* path,
                                                        const NkModelImportOptions& opts = {});

                // Créer un mesh procédural
                NkStaticMesh* CreateMesh(const NkMeshData& data, const char* name = nullptr);

                // ── Helpers de création rapide ────────────────────────────────────────────
                NkMaterial* CreatePBRMaterial(const char* name,
                                            const NkColor4f& albedo   = {1,1,1,1},
                                            float metallic = 0.f, float roughness = 0.5f,
                                            NkTexture2D* albedoTex = nullptr,
                                            NkTexture2D* normalTex = nullptr,
                                            NkTexture2D* ormTex    = nullptr);

                NkMaterial* CreateUnlitMaterial(const char* name,
                                                const NkColor4f& color = {1,1,1,1},
                                                NkTexture2D* tex = nullptr);

                NkMaterial* CreateGlassMaterial(const char* name,
                                                float ior = 1.52f,
                                                const NkColor4f& tint = {0.9f,0.95f,1.f,0.1f});

                NkMaterial* CreateEmissiveMaterial(const char* name,
                                                    const NkColor4f& color,
                                                    float intensity = 3.f);

                NkMaterial* CreateToonMaterial(const char* name,
                                                const NkColor4f& color,
                                                int bands = 4,
                                                bool outline = true);

                NkMaterial* CreateSkinMaterial(const char* name,
                                                NkTexture2D* albedo,
                                                NkTexture2D* normal = nullptr,
                                                const NkColor4f& sssColor = {1,0.3f,0.2f,1});

                NkMaterial* CreateWaterMaterial(const char* name,
                                                NkTexture2D* normalMap = nullptr,
                                                const NkColor4f& tint = {0.1f,0.4f,0.7f,0.8f});

                NkMaterial* CreateHairMaterial(const char* name,
                                                const NkColor4f& color,
                                                NkTexture2D* alphaMap = nullptr);

                NkMaterial* CreateVehicleMaterial(const char* name,
                                                const NkColor4f& bodyColor,
                                                NkTexture2D* normalMap = nullptr,
                                                float clearCoat = 1.f);

                NkMaterial* CreateFoliageMaterial(const char* name,
                                                NkTexture2D* albedo,
                                                NkTexture2D* normal = nullptr,
                                                float subsurface = 0.3f);

                // ── Presets de scène ──────────────────────────────────────────────────────
                // Configurer une scène typique jeu vidéo (PBR + ombres + SSAO + bloom)
                void SetupGameScene(NkScene& scene, NkPostProcessSettings* pp = nullptr);

                // Configurer pour application de modélisation (pas de post, wireframe possible)
                void SetupModelingScene(NkScene& scene);

                // Configurer pour rendu film (IBL haute qualité + DOF + motion blur)
                void SetupFilmScene(NkScene& scene, NkPostProcessSettings* pp = nullptr);

                // Configurer pour visualisation architecturale
                void SetupArchScene(NkScene& scene, NkPostProcessSettings* pp = nullptr);

                // Configurer pour VR (stéréo, reprojection)
                void SetupVRScene(NkScene& scene);

            private:
                NkIDevice*            mDevice     = nullptr;
                uint32                mWidth      = 0;
                uint32                mHeight     = 0;
                bool                  mIsValid    = false;
                NkRendererConfig      mConfig;
                NkPostProcessSettings mPostProcess;

                NkRenderer3D   mRenderer3D;
                NkRenderer2D   mRenderer2D;
                NkTextRenderer mTextRenderer;
                NkVFXRenderer  mVFXRenderer;

                // Gizmo resources
                NkStaticMesh*  mGridMesh    = nullptr;
                NkStaticMesh*  mAxisMesh    = nullptr;
                NkMaterial*    mGizmoMat    = nullptr;
                NkStaticMesh*  mSphereMesh  = nullptr;

                bool InitGizmos();
                void ShutdownGizmos();
        };

        // =============================================================================
        // Template implementations
        // =============================================================================
        template<typename Fn>
        void NkRenderer::Render2D(NkICommandBuffer* cmd, Fn&& drawFn) {
            mRenderer2D.Begin(cmd);
            drawFn(mRenderer2D);
            mRenderer2D.End();
        }

        template<typename Fn>
        void NkRenderer::RenderText(NkICommandBuffer* cmd, Fn&& drawFn) {
            mTextRenderer.Begin(cmd);
            drawFn(mTextRenderer);
            mTextRenderer.End();
        }

        // =============================================================================
        // Helpers de création rapide inline
        // =============================================================================
        inline NkMaterial* NkRenderer::CreatePBRMaterial(
            const char* name, const NkColor4f& albedo,
            float metallic, float roughness,
            NkTexture2D* albedoTex, NkTexture2D* normalTex, NkTexture2D* ormTex) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_DEFAULT_LIT, NkBlendMode::NK_OPAQUE);
            mat->SetName(name);
            mat->SetAlbedo(albedo);
            mat->SetMetallic(metallic);
            mat->SetRoughness(roughness);
            if (albedoTex) mat->SetAlbedoMap(albedoTex);
            if (normalTex) mat->SetNormalMap(normalTex);
            if (ormTex)    mat->SetORMMap(ormTex);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateUnlitMaterial(
            const char* name, const NkColor4f& color, NkTexture2D* tex) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_UNLIT, NkBlendMode::NK_OPAQUE);
            mat->SetName(name);
            mat->SetAlbedo(color);
            if (tex) mat->SetAlbedoMap(tex);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateGlassMaterial(
            const char* name, float ior, const NkColor4f& tint) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_GLASS_BSDF, NkBlendMode::NK_TRANSLUCENT);
            mat->SetName(name);
            mat->SetAlbedo(tint);
            mat->SetTransmission(1.f, ior, 0.f);
            mat->SetRoughness(0.01f);
            mat->SetMetallic(0.f);
            mat->SetTwoSided(true);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateEmissiveMaterial(
            const char* name, const NkColor4f& color, float intensity) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_UNLIT, NkBlendMode::NK_ADDITIVE);
            mat->SetName(name);
            mat->SetAlbedo({0,0,0,1});
            mat->SetEmissive(color, intensity);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateToonMaterial(
            const char* name, const NkColor4f& color, int bands, bool outline) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_TOON, NkBlendMode::NK_OPAQUE);
            mat->SetName(name);
            mat->SetAlbedo(color);
            mat->SetToonBands(bands, 0.02f);
            if (outline) mat->SetToonOutline({0,0,0,1}, 0.003f);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateSkinMaterial(
            const char* name, NkTexture2D* albedo, NkTexture2D* normal,
            const NkColor4f& sssColor) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_SUBSURFACE, NkBlendMode::NK_OPAQUE);
            mat->SetName(name);
            mat->SetMetallic(0.f);
            mat->SetRoughness(0.6f);
            if (albedo) mat->SetAlbedoMap(albedo);
            if (normal) mat->SetNormalMap(normal);
            mat->SetSubsurfaceColor(sssColor);
            mat->SetSubsurfaceRadius(0.5f);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateWaterMaterial(
            const char* name, NkTexture2D* normalMap, const NkColor4f& tint) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_SINGLE_LAYER_WATER, NkBlendMode::NK_TRANSLUCENT);
            mat->SetName(name);
            mat->SetAlbedo(tint);
            mat->SetMetallic(0.f);
            mat->SetRoughness(0.05f);
            mat->SetTransmission(0.9f, 1.33f, 0.f);
            mat->SetTwoSided(false);
            if (normalMap) mat->SetNormalMap(normalMap);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateHairMaterial(
            const char* name, const NkColor4f& color, NkTexture2D* alphaMap) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_HAIR, NkBlendMode::NK_MASKED);
            mat->SetName(name);
            mat->SetAlbedo(color);
            mat->SetRoughness(0.4f);
            mat->SetMetallic(0.f);
            mat->SetTwoSided(true);
            mat->SetAlphaCutoff(0.5f);
            if (alphaMap) mat->SetTexture(NkTexSlot::NK_ALBEDO, alphaMap);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateVehicleMaterial(
            const char* name, const NkColor4f& bodyColor,
            NkTexture2D* normalMap, float clearCoat) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_CLEAR_COAT, NkBlendMode::NK_OPAQUE);
            mat->SetName(name);
            mat->SetAlbedo(bodyColor);
            mat->SetMetallic(0.f);
            mat->SetRoughness(0.1f);
            mat->SetClearCoat(clearCoat, 0.05f);
            mat->SetSpecular(0.5f, 0.f);
            if (normalMap) mat->SetNormalMap(normalMap);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

        inline NkMaterial* NkRenderer::CreateFoliageMaterial(
            const char* name, NkTexture2D* albedo, NkTexture2D* normal, float subsurface) {

            auto* mat = new NkMaterial();
            mat->Create(mDevice, NkShadingModel::NK_TWO_SIDED_FOLIAGE, NkBlendMode::NK_MASKED);
            mat->SetName(name);
            mat->SetMetallic(0.f);
            mat->SetRoughness(0.85f);
            mat->SetAlphaCutoff(0.3f);
            mat->SetTwoSided(true);
            mat->SetSubsurfaceColor({0.3f,0.8f,0.1f,1.f});
            mat->SetSubsurfaceRadius(subsurface);
            if (albedo) mat->SetAlbedoMap(albedo);
            if (normal) mat->SetNormalMap(normal);
            mat->FlushToGPU();
            NkMaterialLibrary::Get().Register(mat);
            return mat;
        }

    } // namespace renderer
} // namespace nkentseu
