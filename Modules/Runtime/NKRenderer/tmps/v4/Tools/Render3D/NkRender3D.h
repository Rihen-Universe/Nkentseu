#pragma once
// =============================================================================
// NkRender3D.h
// Renderer 3D temps-réel AAA.
//
// Capacités :
//   - PBR Metallic-Roughness + IBL
//   - Ombres directionnelles CSM (Cascaded Shadow Maps)
//   - Éclairage ponctuel/spot multi-lumières
//   - Instancing GPU (glDrawIndexedInstanced / DrawIndexedInstanced)
//   - Culling frustum + occlusion (optional)
//   - Skinning GPU (matrices bones via SSBO)
//   - Terrain multi-couche avec tessellation
//   - Décals projetés
//   - Render modes (solid / wireframe / normals / UV / depth...)
//   - Debug gizmos (axes, AABB, normals, bones)
//
// Pipeline :
//   NkRender3D::BeginScene   — déclare caméra + lumières + env
//   NkRender3D::Submit*      — enregistre des draw calls
//   NkRender3D::EndScene     — flush et exécute via RenderGraph
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        class NkResourceManager;

        // =============================================================================
        // Draw call 3D individuel
        // =============================================================================
        struct NkDrawCall3D {
            NkMeshHandle         mesh;
            NkMaterialInstHandle material;
            NkMat4f              transform;
            NkColorF             tint       = NkColorF::White();
            NkAABB               aabb;           // monde
            bool                 castShadow  = true;
            bool                 visible     = true;
            uint32               subMeshIdx  = 0;
            uint32               sortKey     = 0;  // matériau.SortKey()
        };

        // =============================================================================
        // Draw call instancié
        // =============================================================================
        struct NkDrawCallInstanced {
            NkMeshHandle         mesh;
            NkMaterialHandle     material;    // même matériau pour toutes instances
            NkVector<NkMat4f>    transforms;  // une matrice par instance
        };

        // =============================================================================
        // Descripteur de scène 3D pour un frame
        // =============================================================================
        struct NkSceneContext3D {
            NkCamera3D              camera;
            NkVector<NkLightDesc>   lights;
            NkTextureHandle         envMap;
            float32                 ambientIntensity = 0.2f;
            float32                 time             = 0.f;
            float32                 deltaTime        = 0.f;
            NkRenderMode            renderMode       = NkRenderMode::NK_SOLID;
        };

        // =============================================================================
        // Descripteur de passe de rendu 3D
        // =============================================================================
        struct NkRenderPass3DInfo {
            NkRenderTargetHandle  target;       // null = swapchain
            bool                  clearColor   = true;
            NkColorF              clearColorVal;
            bool                  clearDepth   = true;
            float32               clearDepthVal = 1.f;
            NkViewportF           viewport;     // {0,0,0,0} = auto swapchain
        };

        // =============================================================================
        // NkRender3D
        // =============================================================================
        class NkRender3D {
           public:
                NkRender3D()  = default;
                ~NkRender3D() { Shutdown(); }

                NkRender3D(const NkRender3D&)            = delete;
                NkRender3D& operator=(const NkRender3D&) = delete;

                // ── Init ──────────────────────────────────────────────────────────────────
                bool Init    (NkIDevice* device, NkResourceManager* resources,
                               NkMaterialSystem* materials, NkRenderGraph* graph,
                               uint32 width, uint32 height);
                void Shutdown();
                void Resize  (uint32 width, uint32 height);
                bool IsValid () const { return mDevice != nullptr; }

                // ── Frame ─────────────────────────────────────────────────────────────────

                // Ouvrir une scène 3D (déclare caméra, lumières, env)
                void BeginScene(const NkSceneContext3D& ctx);

                // Soumettre un objet unique
                void Submit(const NkDrawCall3D& dc);
                void Submit(NkMeshHandle mesh, NkMaterialInstHandle material,
                             const NkMat4f& transform,
                             bool castShadow = true);
                void Submit(NkMeshHandle mesh, NkMaterialInstHandle material,
                             const NkTransform3D& transform,
                             bool castShadow = true) {
                    Submit(mesh, material, transform.ToMatrix(), castShadow);
                }

                // Soumettre plusieurs instances en une seule passe GPU
                void SubmitInstanced(const NkDrawCallInstanced& dc);
                void SubmitInstanced(NkMeshHandle mesh, NkMaterialHandle material,
                                      const NkMat4f* transforms, uint32 count);

                // Soumettre un terrain
                void SubmitTerrain(NkMeshHandle heightmap,
                                    NkMaterialInstHandle material,
                                    const NkMat4f& transform);

                // Debug — dessin immédiat (non batché)
                void DrawLine    (const NkVec3f& a, const NkVec3f& b, const NkColorF& c, float32 thick = 1.f);
                void DrawAABB    (const NkAABB& aabb, const NkColorF& c);
                void DrawAxis    (const NkMat4f& transform, float32 size = 1.f);
                void DrawSphere  (const NkVec3f& center, float32 r, const NkColorF& c);
                void DrawGrid    (float32 size = 10.f, int32 divisions = 10, const NkColorF& c = NkColorF::Gray(.3f));
                void DrawFrustum (const NkCamera3D& cam, const NkColorF& c);
                void DrawSkeleton(const NkMat4f* boneMatrices, uint32 boneCount, const int32* parentIndices, const NkColorF& c = NkColorF::Yellow());

                // Clore et exécuter la scène
                void EndScene(NkICommandBuffer* cmd, const NkRenderPass3DInfo& pass = {});

                // ── Rendu direct (sans RenderGraph, pour cas spéciaux) ────────────────
                void RenderDirect(NkICommandBuffer* cmd, const NkSceneContext3D& ctx, const NkVector<NkDrawCall3D>& drawCalls);

                // ── Shadow maps ───────────────────────────────────────────────────────────
                // Retourne les render targets des shadow maps CSM générés
                const NkVector<NkRenderTargetHandle>& GetShadowMaps() const {
                    return mShadowMaps;
                }

                void SetShadowMapSize(uint32 size) { mShadowMapSize = size; }

                // ── Configuration post-frame ──────────────────────────────────────────────
                void SetRenderMode  (NkRenderMode mode) { mRenderMode = mode; }

                void SetWireframe   (bool v) {
                    mRenderMode = v ? NkRenderMode::NK_WIREFRAME
                                    : NkRenderMode::NK_SOLID;
                }

                void SetEnableShadows  (bool v) { mShadowsEnabled   = v; }

                void SetEnableInstancing(bool v) { mInstancingEnabled = v; }

                void SetEnableDebugDraw(bool v) { mDebugEnabled      = v; }

                // ── Statistiques ──────────────────────────────────────────────────────────
                struct Stats3D {
                    uint32 drawCalls       = 0;
                    uint32 instancedCalls  = 0;
                    uint32 shadowCasters   = 0;
                    uint32 triangles       = 0;
                    uint32 culledObjects   = 0;
                    uint32 debugLines      = 0;
                };
                
                const Stats3D& GetStats() const { return mStats; }

           private:
                // ── Implémentations ───────────────────────────────────────────────────────
                void SetupRenderGraph    ();
                void SortDrawCalls       ();
                void FrustumCull         (const NkMat4f& viewProj);
                void UpdateSceneUBO      ();
                void RenderShadowPasses  (NkICommandBuffer* cmd);
                void RenderGeometry      (NkICommandBuffer* cmd, bool shadowPass);
                void RenderDebugGeometry (NkICommandBuffer* cmd);
                void ComputeShadowCSM    (const NkCamera3D& cam,
                                           const NkLightDesc& sun,
                                           uint32 numCascades);

                NkIDevice*          mDevice    = nullptr;
                NkResourceManager*  mResources = nullptr;
                NkMaterialSystem*   mMaterials = nullptr;
                NkRenderGraph*      mGraph     = nullptr;

                uint32              mWidth     = 0;
                uint32              mHeight    = 0;
                NkRenderMode        mRenderMode       = NkRenderMode::NK_SOLID;
                bool                mShadowsEnabled   = true;
                bool                mInstancingEnabled= true;
                bool                mDebugEnabled     = false;
                uint32              mShadowMapSize    = 2048;

                // Buffer de draw calls (trié par matériau pour minimiser les state changes)
                NkVector<NkDrawCall3D>     mOpaqueCalls;
                NkVector<NkDrawCall3D>     mAlphaCalls;
                NkVector<NkDrawCallInstanced> mInstancedCalls;

                // Debug geometry
                struct DebugLine {
                    NkVec3f a, b; NkColorF c; float32 thick;
                };
                NkVector<DebugLine> mDebugLines;

                // Shadow maps (CSM)
                NkVector<NkRenderTargetHandle> mShadowMaps;
                NkVector<NkMat4f>              mShadowVP;   // un par cascade

                // GPU resources (IDs RHI opaques)
                uint64              mSceneUBORHI   = 0;
                uint64              mShadowUBORHI  = 0;
                uint64              mDebugVBORHI   = 0;
                uint64              mDebugShaderRHI= 0;
                uint64              mDebugPipeRHI  = 0;

                // Contexte de scène courant
                NkSceneContext3D    mCurrentScene;
                bool                mSceneOpen = false;

                Stats3D             mStats;
        };

    } // namespace renderer
} // namespace nkentseu
