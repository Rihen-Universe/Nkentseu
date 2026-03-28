#pragma once
// =============================================================================
// NkRenderer3D.h
// Moteur de rendu 3D complet du NKRenderer.
//
// Pipeline de rendu :
//   1. Shadow Pass   — depth maps pour toutes les lumières avec ombre
//   2. Depth Prepass — profondeur seule (optionnel, économise les overdraw)
//   3. G-Buffer Pass — albedo, normal, ORM, emissive, position (deferred)
//   4. SSAO Pass     — occlusion ambiante (optionnel)
//   5. Lighting Pass — éclairage deferred (IBL + lumières directes)
//   6. Forward Pass  — objets transparents + éléments spéciaux
//   7. Post-Process  — tonemapping, bloom, FXAA, DOF, motion blur, etc.
//   8. Overlay Pass  — wireframe, debug, gizmos, text 3D
//
// Modes de visualisation (comme Blender) :
//   Solid, Wireframe, Points, Material Preview (Matcap), Rendered (PBR complet)
//
// Rendu multi-usage :
//   • Jeux vidéo : Forward+ ou Deferred, PBR, ombres PCF
//   • Architecture : éclairage indirect GI approximé, matériaux réfléchissants
//   • Film/Animation : IBL HDR, subsurface, motion blur, DOF physique
//   • VR : rendu stéréoscopique, reprojection
// =============================================================================
#include "NKRenderer/Core/NkRenderTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Mesh/NkMesh.h"
#include "NKRenderer/Material/NkMaterial.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKRenderer/Core/NkShaderAsset.h"
#include "NKRenderer/Core/NkTexture.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        // forward declarations
        class NkRenderer3D;
        class NkPostProcessStack;

        // =============================================================================
        // DrawCall — commande de rendu à enregistrer dans le frame
        // =============================================================================
        struct NkDrawCall {
            NkStaticMesh*   mesh          = nullptr;
            NkDynamicMesh*  dynMesh       = nullptr;
            NkMaterial*     material      = nullptr;
            NkMat4f         transform     = NkMat4f::Identity();
            uint32          subMeshIndex  = 0;
            bool            castShadow    = true;
            bool            receiveShadow = true;
            bool            visible       = true;
            float           lodBias       = 0.f;
            uint32          instanceCount = 1;
            NkBufferHandle  instanceBuffer;  // buffer de matrices pour le GPU instancing
            uint32          stencilRef    = 0;
            // Outline (sélection, Blender-like)
            bool            drawOutline   = false;
            NkColor4f       outlineColor  = {1,0.5f,0,1};
            float           outlineThickness = 0.002f;
            // Modes debug overlay
            bool            drawWireframe  = false;
            bool            drawVertices   = false;
            bool            drawNormals    = false;
            float           normalLength   = 0.05f;
            // Custom ID pour le picking
            uint32          objectID       = 0;
        };

        // =============================================================================
        // Paramètres des ombres
        // =============================================================================
        struct NkShadowSettings {
            bool    enabled         = true;
            uint32  shadowMapSize   = 2048;
            float   cascadeCount    = 4.f;    // CSM : cascades pour ombres directionnelles
            float   shadowDistance  = 100.f;
            float   splitLambda     = 0.7f;   // distribution des cascades
            float   biasConst       = 1.25f;
            float   biasSlope       = 1.75f;
            bool    softShadows     = true;   // PCF
            int     pcfKernel       = 3;      // 3=3x3, 5=5x5
            bool    vsm             = false;  // Variance Shadow Map
            bool    cascadeDebug    = false;  // afficher les cascades en couleur
        };

        // =============================================================================
        // Paramètres de post-process
        // =============================================================================
        struct NkTonemapParams {
            float exposure    = 1.f;
            float gamma       = 2.2f;
            int   mode        = 1;      // 0=Reinhard, 1=ACES, 2=Uncharted2, 3=Filmic
            bool  enabled     = true;
        };

        struct NkBloomParams {
            bool  enabled     = true;
            float threshold   = 1.f;    // luminance au-dessus de laquelle on bloom
            float knee        = 0.5f;
            float intensity   = 0.3f;
            float radius      = 3.f;
            int   passes      = 5;
        };

        struct NkSSAOParams {
            bool  enabled     = true;
            float radius      = 0.5f;
            float bias        = 0.025f;
            float power       = 1.5f;
            int   samples     = 32;
            int   blurPasses  = 2;
        };

        struct NkDOFParams {
            bool  enabled          = false;
            float focalDistance    = 10.f;
            float focalLength      = 50.f;  // mm
            float aperture         = 1.8f;  // f/
            float nearBlur         = 0.1f;
            float farBlur          = 1.f;
            bool  bokehEnabled     = false;
            float bokehBladeCnt    = 6.f;
        };

        struct NkMotionBlurParams {
            bool  enabled     = false;
            float shutterAngle= 180.f;  // degrés
            int   samples     = 8;
            float intensity   = 1.f;
        };

        struct NkVignetteParams {
            bool      enabled     = true;
            float     intensity   = 0.4f;
            float     smoothness  = 0.45f;
            NkColor4f color       = {0,0,0,1};
        };

        struct NkChromaticAberrationParams {
            bool  enabled   = false;
            float intensity = 0.5f;
        };

        struct NkColorGradeParams {
            bool         enabled     = false;
            NkTexture3D* lut         = nullptr; // LUT 3D
            float        contribution= 1.f;
            NkColor4f    tint        = {1,1,1,1};
            float        brightness  = 0.f;
            float        contrast    = 1.f;
            float        saturation  = 1.f;
            float        temperature = 0.f;  // Kelvin offset
            float        tint2       = 0.f;  // tint magenta/vert
        };

        struct NkPostProcessSettings {
            NkTonemapParams           tonemap;
            NkBloomParams             bloom;
            NkSSAOParams              ssao;
            NkDOFParams               dof;
            NkMotionBlurParams        motionBlur;
            NkVignetteParams          vignette;
            NkChromaticAberrationParams chromatic;
            NkColorGradeParams        colorGrade;
            float                     filmGrain   = 0.f;
            float                     sharpening  = 0.f;
            bool                      fxaa        = true;
            bool                      taa         = false; // Temporal AA
            bool                      ssr         = false; // Screen Space Reflections
            float                     ssrIntensity= 0.3f;
            float                     ambient     = 0.03f; // éclairage ambiant global
        };

        // =============================================================================
        // Paramètres du ciel / skybox
        // =============================================================================
        struct NkSkySettings {
            enum class Type { NK_COLOR, NK_GRADIENT, NK_HDRI, NK_PROCEDURAL } type = Type::NK_COLOR;
            NkColor4f     topColor    = {0.2f,0.4f,0.8f,1.f};
            NkColor4f     bottomColor = {0.8f,0.7f,0.6f,1.f};
            NkTextureCube* hdri        = nullptr;
            float         hdriRotation= 0.f;
            float         hdriExposure= 1.f;
            float         sunSize     = 0.04f;
            float         sunBloom    = 0.2f;
            bool          affectsLighting = true; // IBL depuis ce sky
        };

        // =============================================================================
        // G-Buffer (rendu différé)
        // =============================================================================
        struct NkGBuffer {
            NkRenderTarget* rt            = nullptr;
            // attachments :
            // color[0] = Albedo RGBA8  (rgb=couleur, a=ao)
            // color[1] = Normal RGBA16 (rgb=normal worldspace, a=roughness)
            // color[2] = ORM   RGBA8   (r=metallic, g=roughness_packed, b=flags, a=depth_encoded)
            // color[3] = Emissive RGBA16
            // depth    = D32
            bool IsValid() const { return rt && rt->IsValid(); }
        };

        // =============================================================================
        // Scène de rendu — accumulateur de draw calls pour un frame
        // =============================================================================
        class NkRenderScene {
            public:
                void Clear();

                // Soumettre un mesh à rendre
                void Submit(const NkDrawCall& dc);
                void Submit(NkStaticMesh* mesh, NkMaterial* mat, const NkMat4f& transform,
                            bool castShadow = true);

                // Lumières
                void AddLight(const NkLight& l);
                void SetCamera(const NkCamera& cam);
                void SetSky(const NkSkySettings& sky);

                // Instances GPU (100 000+ objets identiques)
                void SubmitInstanced(NkStaticMesh* mesh, NkMaterial* mat,
                                    const NkMat4f* transforms, uint32 count);

                // ── Accesseurs ────────────────────────────────────────────────────────────
                const NkVector<NkDrawCall>&  GetDrawCalls() const { return mDrawCalls; }
                const NkVector<NkLight>&     GetLights()    const { return mLights; }
                const NkCamera&              GetCamera()    const { return mCamera; }
                const NkSkySettings&         GetSky()       const { return mSky; }
                uint32                       DrawCallCount()const { return (uint32)mDrawCalls.Size(); }

                // Frustum culling
                void FrustumCull(const NkCamera& cam, float aspect);

                // Tri par matériau / distance (front-to-back opaque, back-to-front alpha)
                void Sort();

            private:
                NkVector<NkDrawCall> mDrawCalls;
                NkVector<NkLight>    mLights;
                NkCamera             mCamera;
                NkSkySettings        mSky;
                // Instances GPU pending
                struct InstanceBatch {
                    NkStaticMesh* mesh;
                    NkMaterial*   mat;
                    NkVector<NkMat4f> transforms;
                };
                NkVector<InstanceBatch> mInstanceBatches;
        };

        // =============================================================================
        // NkRenderer3D — moteur de rendu 3D principal
        // =============================================================================
        class NkRenderer3D {
            public:
                NkRenderer3D()  = default;
                ~NkRenderer3D() { Shutdown(); }
                NkRenderer3D(const NkRenderer3D&) = delete;
                NkRenderer3D& operator=(const NkRenderer3D&) = delete;

                // ── Cycle de vie ──────────────────────────────────────────────────────────
                bool Init(NkIDevice* device, uint32 width, uint32 height);
                void Shutdown();
                void Resize(uint32 width, uint32 height);

                // ── Configuration ─────────────────────────────────────────────────────────
                void SetRenderMode(NkRenderMode mode)               { mRenderMode = mode; }
                void SetPostProcess(const NkPostProcessSettings& s) { mPPSettings = s; }
                void SetShadowSettings(const NkShadowSettings& s)   { mShadowSettings = s; }
                void SetSkybox(NkTextureCube* skybox, float exposure = 1.f);
                void SetIBL(NkTextureCube* irradiance, NkTextureCube* prefilter,
                            NkTexture2D* brdfLUT);
                void SetEnvironmentFromHDRI(const char* hdriPath);

                // ── Rendu ─────────────────────────────────────────────────────────────────
                // Rendre une scène vers le swapchain
                void Render(NkICommandBuffer* cmd, NkRenderScene& scene);

                // Rendre une scène vers un render target spécifique
                void RenderTo(NkICommandBuffer* cmd, NkRenderScene& scene, NkRenderTarget* target);

                // Passe ombre seule (utilisée par le CSM)
                void RenderShadowPass(NkICommandBuffer* cmd, NkRenderScene& scene,
                                    const NkLight& light, uint32 cascadeIdx = 0);

                // ── Mode Blender ──────────────────────────────────────────────────────────
                // Rendu overlay (wireframe + vertices + arêtes + normales)
                void RenderEditOverlay(NkICommandBuffer* cmd, NkEditableMesh* mesh,
                                        const NkMat4f& transform,
                                        bool showVerts = true, bool showEdges = true,
                                        bool showFaces = false,
                                        NkColor4f vertColor  = {1,1,1,1},
                                        NkColor4f edgeColor  = {0,0,0,1},
                                        NkColor4f selectColor= {1,0.5f,0,1});

                void RenderNormals(NkICommandBuffer* cmd, NkStaticMesh* mesh,
                                    const NkMat4f& transform, float length = 0.05f,
                                    NkColor4f color = {0,0,1,1});

                void RenderBoundingBox(NkICommandBuffer* cmd, const NkAABB& aabb,
                                        const NkMat4f& transform,
                                        NkColor4f color = {1,1,0,1});

                // ── Picking (sélection d'objet par clic) ──────────────────────────────────
                // Rendu vers un buffer ID — retourne l'objectID sous le pixel (x,y)
                uint32 PickObject(NkRenderScene& scene, uint32 pixelX, uint32 pixelY);

                // ── Accesseurs ────────────────────────────────────────────────────────────
                bool              IsValid()         const { return mIsValid; }
                uint32            GetWidth()        const { return mWidth; }
                uint32            GetHeight()       const { return mHeight; }
                NkRenderMode      GetRenderMode()   const { return mRenderMode; }
                const NkRendererStats& GetStats()   const { return mStats; }
                NkRenderTarget*   GetGBufferRT()    const;
                NkRenderTarget*   GetHDRTarget()    const;
                NkRenderTarget*   GetShadowMap()    const;

                // ── IBL Generation ────────────────────────────────────────────────────────
                NkTextureCube* GenerateIrradianceMap(NkTextureCube* envMap, uint32 size = 32);
                NkTextureCube* GeneratePrefilterMap (NkTextureCube* envMap, uint32 size = 128);
                NkTexture2D*   GenerateBRDF_LUT     (uint32 size = 512);

            private:
                // ── Device ────────────────────────────────────────────────────────────────
                NkIDevice*  mDevice  = nullptr;
                uint32      mWidth   = 0;
                uint32      mHeight  = 0;
                bool        mIsValid = false;
                NkRenderPassHandle mActiveRenderPass;
                NkRenderPassHandle mPipelineRenderPass;

                // ── Mode ──────────────────────────────────────────────────────────────────
                NkRenderMode        mRenderMode  = NkRenderMode::NK_SOLID;
                NkPostProcessSettings mPPSettings;
                NkShadowSettings    mShadowSettings;

                // ── Render Targets ────────────────────────────────────────────────────────
                NkGBuffer        mGBuffer;
                NkRenderTarget   mHDRTarget;       // HDR color + depth
                NkRenderTarget   mPostTarget[2];   // ping-pong post-process
                NkRenderTarget   mShadowMapRT;
                NkRenderTarget   mSSAOTarget;
                NkRenderTarget   mBloomTarget[6];  // downsample + upsample
                NkRenderTarget   mPickingTarget;   // picking ID

                // ── IBL ───────────────────────────────────────────────────────────────────
                NkTextureCube*  mSkyboxTex      = nullptr;
                NkTextureCube*  mIrradianceMap  = nullptr;
                NkTextureCube*  mPrefilterMap   = nullptr;
                NkTexture2D*    mBRDF_LUT       = nullptr;
                float           mSkyExposure    = 1.f;
                bool            mIBLDirty       = true;

                // ── Shaders ───────────────────────────────────────────────────────────────
                NkShaderAsset   mPBRShader;
                NkShaderAsset   mUnlitShader;
                NkShaderAsset   mShadowShader;
                NkShaderAsset   mSkyboxShader;
                NkShaderAsset   mWireframeShader;
                NkShaderAsset   mNormalVisShader;
                NkShaderAsset   mTonemapShader;
                NkShaderAsset   mBloomShader;
                NkShaderAsset   mSSAOShader;
                NkShaderAsset   mFXAAShader;
                NkShaderAsset   mPickingShader;
                NkShaderAsset   mIBLShaders[4];   // equirect→cube, irradiance, prefilter, brdf

                // ── UBOs scène ────────────────────────────────────────────────────────────
                NkBufferHandle  mSceneUBO;
                NkBufferHandle  mLightSSBO;    // toutes les lumières
                NkBufferHandle  mShadowUBO;    // VP matrices des passes ombres
                NkBufferHandle  mSkyboxVBO;
                NkBufferHandle  mFullscreenVBO;// 3 vertices pour triangle plein écran

                // ── Descriptor layouts ────────────────────────────────────────────────────
                NkDescSetHandle mSceneLayout;
                NkDescSetHandle mSceneDescSet;
                NkDescSetHandle mIBLLayout;
                NkDescSetHandle mIBLDescSet;
                NkDescSetHandle mShadowLayout;
                NkDescSetHandle mShadowDescSet;

                // ── Pipelines ─────────────────────────────────────────────────────────────
                NkPipelineHandle mSkyboxPipeline;
                NkPipelineHandle mWireframePipeline;
                NkPipelineHandle mNormalVisPipeline;
                NkPipelineHandle mTonemapPipeline;
                NkPipelineHandle mBloomDownPipeline;
                NkPipelineHandle mBloomUpPipeline;
                NkPipelineHandle mSSAOPipeline;
                NkPipelineHandle mFXAAPipeline;
                NkPipelineHandle mPickingPipeline;
                NkPipelineHandle mVertexPointPipeline;

                // ── Stats ─────────────────────────────────────────────────────────────────
                NkRendererStats mStats;

                // ── Méthodes internes ─────────────────────────────────────────────────────
                bool InitShaders();
                bool InitRenderTargets(uint32 w, uint32 h);
                bool InitBuffers();
                bool InitIBL();
                bool InitDescriptors();
                bool InitPipelines();

                void UpdateSceneUBO(const NkRenderScene& scene);
                void UpdateLightSSBO(const NkRenderScene& scene);

                void PassShadow      (NkICommandBuffer* cmd, NkRenderScene& scene);
                void PassDepthPrepass(NkICommandBuffer* cmd, NkRenderScene& scene);
                void PassGBuffer     (NkICommandBuffer* cmd, NkRenderScene& scene);
                void PassLighting    (NkICommandBuffer* cmd, NkRenderScene& scene);
                void PassForward     (NkICommandBuffer* cmd, NkRenderScene& scene);
                void PassSkybox      (NkICommandBuffer* cmd, const NkSkySettings& sky);
                void PassSSAO        (NkICommandBuffer* cmd);
                void PassBloom       (NkICommandBuffer* cmd);
                void PassPostProcess (NkICommandBuffer* cmd);
                void PassFXAA        (NkICommandBuffer* cmd);
                void PassBlit        (NkICommandBuffer* cmd, NkRenderTarget* from);

                void DrawMesh(NkICommandBuffer* cmd, const NkDrawCall& dc,
                            NkPipelineHandle override = {});
                void DrawFullscreen(NkICommandBuffer* cmd);
                void DrawSkybox(NkICommandBuffer* cmd);

                void ResetStats();
        };

        // =============================================================================
        // NkRenderer3D inline helpers
        // =============================================================================
        inline void NkRenderScene::Clear() {
            mDrawCalls.Clear();
            mLights.Clear();
            mInstanceBatches.Clear();
        }

        inline void NkRenderScene::Submit(const NkDrawCall& dc) {
            if (dc.visible && (dc.mesh || dc.dynMesh))
                mDrawCalls.PushBack(dc);
        }

        inline void NkRenderScene::Submit(NkStaticMesh* mesh, NkMaterial* mat,
                                        const NkMat4f& transform, bool castShadow) {
            NkDrawCall dc;
            dc.mesh = mesh;
            dc.material = mat;
            dc.transform = transform;
            dc.castShadow = castShadow;
            Submit(dc);
        }

        inline void NkRenderScene::AddLight(const NkLight& l) {
            mLights.PushBack(l);
        }

        inline void NkRenderScene::SetCamera(const NkCamera& cam) {
            mCamera = cam;
        }

        inline void NkRenderScene::SetSky(const NkSkySettings& sky) {
            mSky = sky;
        }

    } // namespace renderer
} // namespace nkentseu
