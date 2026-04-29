#pragma once
// =============================================================================
// NkRender3D.h
// Outil de rendu 3D : meshes, lumières, ombres, instancing, skinning.
//
// Features :
//   - Soumission de meshes avec matériau + transform
//   - Instanced draw (DrawMeshInstanced)
//   - Skinned meshes (DrawSkinnedMesh)
//   - Lumières : directionnelle, point, spot, area (up to 256)
//   - Frustum culling automatique (AABB)
//   - LOD automatique
//   - Shadow casting + receiving (CSM 4 cascades)
//   - Environment : sky, HDRI, réflexions
//   - Camera 3D (perspective + orthographique)
//   - Debug draw 3D (wireframe, gizmos)
//
// Usage :
//   auto& r3d = *renderer->GetRender3D();
//   r3d.SetCamera(camera);
//   r3d.SetSun({-0.3f,-1.f,-0.5f}, {1,0.95f,0.8f}, 5.f);
//   r3d.DrawMesh(meshId, transform, matId);
//   r3d.DrawMeshInstanced(meshId, transforms, count, matId);
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRenderer/Core/NkMeshCache.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKMath/NKMath.h"
#include "NKMath/NkVec.h"

namespace nkentseu {

    // =============================================================================
    // Camera 3D
    // =============================================================================
    struct NkCamera3D {
        math::NkVec3  position   = {0, 1, 5};
        math::NkVec3  target     = {0, 0, 0};
        math::NkVec3  up         = {0, 1, 0};
        float32       fovY       = 60.f;       // degrés
        float32       nearPlane  = 0.1f;
        float32       farPlane   = 1000.f;
        float32       aspectRatio= 16.f/9.f;
        bool          orthographic= false;
        float32       orthoSize  = 10.f;

        math::NkMat4 GetViewMatrix()       const;
        math::NkMat4 GetProjectionMatrix() const;
        math::NkMat4 GetViewProjection()   const;

        // Frustum (6 plans) pour le culling
        void ExtractFrustumPlanes(math::NkVec4 planes[6]) const;
        bool IsAABBVisible(const NkAABB& aabb, const math::NkMat4& model) const;

        // First-person camera helpers
        math::NkVec3 GetForward() const;
        math::NkVec3 GetRight()   const;
        math::NkVec3 GetUp()      const;
    };

    // =============================================================================
    // Lumières
    // =============================================================================
    enum class NkLightType : uint8 {
        NK_DIRECTIONAL = 0,
        NK_POINT       = 1,
        NK_SPOT        = 2,
        NK_AREA        = 3,   // area light (approximé par rectangle)
    };

    struct NkLight {
        NkLightType   type          = NkLightType::NK_DIRECTIONAL;
        math::NkVec3  position      = {0, 5, 0};
        math::NkVec3  direction     = {0,-1, 0};
        math::NkVec3  color         = {1, 1, 1};
        float32       intensity     = 1.f;
        float32       range         = 10.f;   // point/spot uniquement
        float32       innerAngle    = 30.f;   // spot inner cone (degrés)
        float32       outerAngle    = 45.f;   // spot outer cone (degrés)
        float32       areaWidth     = 1.f;    // area light
        float32       areaHeight    = 1.f;
        bool          castShadows   = false;
        bool          enabled       = true;

        // Constructeurs rapides
        static NkLight Directional(math::NkVec3 dir, math::NkVec3 color = {1,1,1},
                                    float32 intensity = 1.f, bool shadows = true);
        static NkLight Point(math::NkVec3 pos, math::NkVec3 color = {1,1,1},
                              float32 intensity = 1.f, float32 range = 10.f);
        static NkLight Spot(math::NkVec3 pos, math::NkVec3 dir,
                             float32 intensity, float32 innerAngle, float32 outerAngle,
                             math::NkVec3 color = {1,1,1});
        static NkLight Area(math::NkVec3 pos, math::NkVec3 dir,
                             float32 w, float32 h, float32 intensity,
                             math::NkVec3 color = {1,1,1});
    };

    // =============================================================================
    // Transform 3D
    // =============================================================================
    struct NkTransform3D {
        math::NkVec3  position = {0, 0, 0};
        math::NkVec3  rotation = {0, 0, 0};  // Euler XYZ en degrés
        math::NkVec3  scale    = {1, 1, 1};

        math::NkMat4 GetMatrix() const;

        static NkTransform3D Identity() { return {}; }
        static NkTransform3D From(math::NkMat4 mat);
        static NkTransform3D TRS(math::NkVec3 t, math::NkVec3 r, math::NkVec3 s);
    };

    // =============================================================================
    // Données de skinning
    // =============================================================================
    struct NkSkinData {
        const math::NkMat4* boneMatrices = nullptr;
        uint32              boneCount    = 0;
    };

    // =============================================================================
    // Push constants 3D (données par-objet rapides)
    // =============================================================================
    struct alignas(16) NkObjectPushConstants {
        math::NkMat4 modelMatrix;
        math::NkVec4 customColor = {1,1,1,1};
        uint32       materialId  = 0;
        uint32       _pad[3];
    };

    // =============================================================================
    // Frame GPU data (mis à jour chaque frame)
    // =============================================================================
    struct alignas(16) NkFrameUniform {
        math::NkMat4  view;
        math::NkMat4  projection;
        math::NkMat4  viewProjection;
        math::NkMat4  invViewProjection;
        math::NkMat4  prevViewProjection;  // pour TAA / motion vectors
        math::NkVec4  cameraPosition;      // xyz=pos w=nearPlane
        math::NkVec4  cameraDirection;     // xyz=dir w=farPlane
        math::NkVec4  viewportSize;        // xy=size zw=1/size
        math::NkVec4  timeData;            // x=time y=deltaTime z=frame# w=exposure

        // Sun
        math::NkVec4  sunDirection;        // xyz w=shadow bias
        math::NkVec4  sunColor;            // xyz=color w=intensity

        // Cascade shadow matrices (4 cascades)
        math::NkMat4  shadowMatrices[4];
        math::NkVec4  cascadeSplits;       // distances des 4 splits

        // Environment
        float32       envIntensity    = 1.f;
        float32       envRotation     = 0.f;
        uint32        lightCount      = 0;
        uint32        frameIndex      = 0;
    };

    // =============================================================================
    // Draw item (dans la liste de rendu)
    // =============================================================================
    struct NkDrawItem {
        NkMeshId       meshId;
        NkMatId        matId;
        math::NkMat4   transform;
        math::NkVec4   customColor = {1,1,1,1};
        float32        sortKey     = 0.f;   // distance camera pour tri
        uint32         layer       = 0;     // 0=opaque 1=alpha 2=overlay
        bool           castShadow  = true;
    };

    // =============================================================================
    // NkRender3D
    // =============================================================================
    class NkRender3D {
        public:
            explicit NkRender3D(NkIDevice* device, NkMeshCache* meshCache,
                                NkMaterialSystem* matSystem, NkTextureLibrary* texLib);
            ~NkRender3D();

            bool Initialize(const NkShadowConfig& shadowCfg,
                            NkRenderPassHandle opaquePass,
                            NkRenderPassHandle shadowPass,
                            NkSampleCount msaa = NkSampleCount::NK_S1);
            void Shutdown();
            void OnResize(uint32 width, uint32 height);

            // ── Camera ────────────────────────────────────────────────────────────────
            void SetCamera(const NkCamera3D& camera) { mCamera = camera; }
            const NkCamera3D& GetCamera() const      { return mCamera; }

            // ── Lumières ──────────────────────────────────────────────────────────────
            void ClearLights();
            void AddLight(const NkLight& light);
            void SetAmbient(math::NkVec3 color, float32 intensity = 0.1f);
            void SetSun(math::NkVec3 direction, math::NkVec3 color = {1,0.95f,0.8f},
                        float32 intensity = 5.f, bool castShadows = true);
            void SetEnvironmentMap(NkTexId cubemap, float32 intensity = 1.f,
                                    float32 rotation = 0.f);

            // ── Sky ───────────────────────────────────────────────────────────────────
            void SetSky(const NkSkyConfig& sky);

            // ── Dessin de meshes ──────────────────────────────────────────────────────
            // Mesh simple
            void DrawMesh(NkMeshId mesh, const NkTransform3D& transform, NkMatId material, bool castShadow = true, math::NkVec4 color = {1,1,1,1});
            void DrawMesh(NkMeshId mesh, const math::NkMat4& transform, NkMatId material, bool castShadow = true);

            // Instanced (GPU instancing automatique)
            void DrawMeshInstanced(NkMeshId mesh, const NkTransform3D* transforms, uint32 count, NkMatId material);
            void DrawMeshInstanced(NkMeshId mesh, const math::NkMat4* matrices, uint32 count, NkMatId material);

            // Skinned
            void DrawSkinnedMesh(NkMeshId mesh, const NkTransform3D& transform, NkMatId material, const NkSkinData& skin);

            // Primitives rapides (utilisent le NkMeshCache interne)
            void DrawCube    (math::NkVec3 pos, math::NkVec3 scale, NkMatId mat);
            void DrawSphere  (math::NkVec3 pos, float32 radius, NkMatId mat);
            void DrawPlane   (math::NkVec3 pos, math::NkVec2 size, NkMatId mat);
            void DrawCylinder(math::NkVec3 pos, float32 radius, float32 height, NkMatId mat);
            void DrawCapsule (math::NkVec3 pos, float32 radius, float32 height, NkMatId mat);

            // ── Debug draw ────────────────────────────────────────────────────────────
            void DrawWireAABB(const NkAABB& aabb, const math::NkMat4& transform,
                            NkColor32 color = {255,255,0,255});
            void DrawWireSphere(math::NkVec3 center, float32 radius,
                                NkColor32 color = {0,255,255,255}, uint32 segments = 32);
            void DrawLine3D(math::NkVec3 from, math::NkVec3 to,
                            NkColor32 color = {255,255,255,255});
            void DrawArrow3D(math::NkVec3 from, math::NkVec3 to, float32 headSize,
                            NkColor32 color = {255,0,0,255});
            void DrawGrid(uint32 size, float32 cellSize, NkColor32 color = {80,80,80,255});
            void DrawAxes(math::NkVec3 origin, float32 length = 1.f);
            void DrawFrustum(const NkCamera3D& cam, NkColor32 color = {255,128,0,255});
            void DrawBone(math::NkVec3 head, math::NkVec3 tail, NkColor32 color = {255,255,255,255});

            // ── Soumission ────────────────────────────────────────────────────────────
            // ShadowPass : rend le depth-only pour le CSM
            void SubmitShadowPasses(NkICommandBuffer* cmd);
            // GeometryPass : rend opaques + transparents
            void SubmitGeometryPass(NkICommandBuffer* cmd);
            // Vide la liste de draw items pour la frame suivante
            void Reset();

            // ── Frustum culling ───────────────────────────────────────────────────────
            void SetFrustumCulling(bool enabled) { mFrustumCulling = enabled; }
            uint32 GetCulledCount()  const { return mCulledCount; }
            uint32 GetDrawCount()    const { return mDrawCount; }

            // ── Statistiques ──────────────────────────────────────────────────────────
            uint32 GetLightCount() const { return (uint32)mLights.Size(); }

        private:
            NkIDevice*        mDevice;
            NkMeshCache*      mMeshCache;
            NkMaterialSystem* mMatSystem;
            NkTextureLibrary* mTexLib;

            NkCamera3D        mCamera;
            NkVector<NkLight> mLights;
            math::NkVec3      mAmbientColor    = {0.1f,0.1f,0.15f};
            float32           mAmbientIntensity= 1.f;
            NkTexId           mEnvironmentMap  = NK_INVALID_TEX;
            float32           mEnvIntensity    = 1.f;
            float32           mEnvRotation     = 0.f;
            NkSkyConfig       mSky;

            // Listes de rendu (triées, culled)
            NkVector<NkDrawItem>  mOpaqueItems;
            NkVector<NkDrawItem>  mAlphaItems;
            NkVector<NkDrawItem>  mShadowCasters;
            NkVector<NkDrawItem>  mDebugItems;

            // GPU buffers
            NkBufferHandle mFrameUBO;
            NkBufferHandle mLightSSBO;
            NkBufferHandle mInstanceMatSSBO;
            NkBufferHandle mBoneMatSSBO;

            NkDescSetHandle mFrameLayout;
            NkDescSetHandle mFrameDescSet;

            // Shadow maps
            NkShadowConfig  mShadowCfg;
            NkTextureHandle mShadowMap[4];  // 4 cascades
            NkRenderPassHandle mShadowPass;
            NkFramebufferHandle mShadowFBO[4];
            NkPipelineHandle mShadowPipeline;
            NkShaderHandle   mShadowShader;

            // Instancing
            NkVector<math::NkMat4> mInstanceScratch;
            uint32  mMaxInstances;

            // Frustum culling
            bool    mFrustumCulling = true;
            uint32  mCulledCount    = 0;
            uint32  mDrawCount      = 0;

            // Debug draw
            NkVector<NkVertexDebug> mDebugVtx;
            NkVector<uint32>        mDebugIdx;
            NkBufferHandle          mDebugVBO, mDebugIBO;
            NkPipelineHandle        mDebugPipeline;
            NkShaderHandle          mDebugShader;

            void UpdateFrameUBO();
            void UpdateLightSSBO();
            void SortItems();
            void CullItems(math::NkVec4 frustumPlanes[6]);
            void RenderItem(NkICommandBuffer* cmd, const NkDrawItem& item);
            void FlushDebug(NkICommandBuffer* cmd);
            void BuildShadowMatrices();
    };

} // namespace nkentseu