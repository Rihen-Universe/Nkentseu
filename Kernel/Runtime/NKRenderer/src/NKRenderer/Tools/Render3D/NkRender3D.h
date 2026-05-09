#pragma once
// =============================================================================
// NkRender3D.h  — NKRenderer v4.0  (Tools/Render3D/)
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRenderer/Core/NkCamera.h"
#include "NKRenderer/Core/NkSceneContext.h"
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRenderer/Materials/NkMaterialSystem.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        class NkShadowSystem;
        class NkEnvironmentSystem;
        class NkShaderLibrary;
        class NkResources;

        // (NkViewMode et NkSceneContext sont definis dans Core/NkRendererTypes.h)

        class NkRender3D {
            public:
                NkRender3D() = default;
                ~NkRender3D();

                bool Init(NkIDevice* device, NkMeshSystem* mesh, NkMaterialSystem* mat,
                        NkRenderGraph* graph,
                        NkShadowSystem* shadow,
                        NkEnvironmentSystem* env,
                        NkShaderLibrary* shaderLib,
                        NkResources* resources);
                void Shutdown();

                // Notification de redimensionnement (propage par NkRendererImpl).
                // Les RT sont geres par le PostProcess/le RenderGraph ; ici on cache
                // juste la taille courante pour le viewport implicite.
                void OnResize(uint32 w, uint32 h) { mW = w; mH = h; }

                // ── Frame ────────────────────────────────────────────────────────────
                void BeginScene(const NkSceneContext& ctx);
                void Flush(NkICommandBuffer* cmd);

                // ── Submit ───────────────────────────────────────────────────────────
                void Submit         (const NkDrawCall3D& dc);
                void SubmitMany     (const NkDrawCall3D* dcs, uint32 count);
                void SubmitInstanced(const NkDrawCallInstanced& dc);
                void SubmitSkinned  (const NkDrawCallSkinned& dc);
                void SubmitSkinnedTinted(const NkDrawCallSkinned& dc,
                                        NkVec3f tint, float32 alpha=1.f);

                void SetWireframe(bool e) { mWireframe=e; }

                // ── Debug gizmos ─────────────────────────────────────────────────────
                void DrawDebugLine  (NkVec3f a, NkVec3f b,   NkVec4f color, float32 life=0.f);
                void DrawDebugSphere(NkVec3f c, float32 r,   NkVec4f color);
                void DrawDebugCircle(NkVec3f c, float32 r, NkVec3f normal,   NkVec4f color);
                void DrawDebugAABB  (const NkAABB& box,       NkVec4f color);
                void DrawDebugAxes  (const NkMat4f& t, float32 size=1.f);
                void DrawDebugGrid  (NkVec3f origin, float32 spacing, int32 lines, NkVec4f color);
                void DrawDebugArrow (NkVec3f from, NkVec3f to, NkVec4f color);

            private:
                struct SortedDC { NkDrawCall3D dc; float32 depth; };
                struct DebugLine { NkVec3f a,b; NkVec4f color; float32 life; };

                NkIDevice*           mDevice  = nullptr;
                NkMeshSystem*        mMesh    = nullptr;
                NkMaterialSystem*    mMat     = nullptr;
                NkRenderGraph*       mGraph   = nullptr;
                NkShadowSystem*      mShadow  = nullptr;
                NkEnvironmentSystem* mEnv     = nullptr;
                NkShaderLibrary*     mShaderLib = nullptr;
                NkResources*         mResources = nullptr;

                NkSceneContext    mCtx;
                bool              mInScene  = false;
                bool              mWireframe= false;
                uint32            mW = 0, mH = 0;  // taille courante (mise a jour par OnResize)

                NkVector<SortedDC>              mOpaque;
                NkVector<SortedDC>              mTransparent;
                NkVector<NkDrawCallInstanced>   mInstanced;
                NkVector<NkDrawCallSkinned>     mSkinned;
                NkVector<DebugLine>             mDebugLines;

                NkBufferHandle  mUBOCamera;
                NkBufferHandle  mUBOObject;
                NkBufferHandle  mUBOLights;
                NkBufferHandle  mSSBOBones;

                // Descriptor sets: set 0 = per-frame (camera+lights+shadow+env+shadowMap),
                //                  set 1 = per-object (model+bones)
                NkDescSetHandle mGlobalLayout;
                NkDescSetHandle mGlobalSet;
                NkDescSetHandle mObjectLayout;
                NkDescSetHandle mObjectSet;

                // PBR pipeline + shader (charges depuis Resources/Shaders/PBR/GL/).
                ::nkentseu::NkShaderHandle mPBRShader;     // RHI shader handle
                NkPipelineHandle           mPBRPipeline;   // pipeline graphics PBR

                void UploadUBOs(NkICommandBuffer* cmd);
                void FlushOpaque     (NkICommandBuffer* cmd);
                void FlushTransparent(NkICommandBuffer* cmd);
                void FlushInstanced  (NkICommandBuffer* cmd);
                void FlushSkinned    (NkICommandBuffer* cmd);
                void FlushDebug      (NkICommandBuffer* cmd);
                void SortDrawCalls();
        };

    } // namespace renderer
} // namespace nkentseu
