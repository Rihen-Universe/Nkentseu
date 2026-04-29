#pragma once
// =============================================================================
// NkRender3D.h  — NKRenderer v4.0  (Tools/Render3D/)
// =============================================================================
#include "../../Core/NkRendererTypes.h"
#include "../../Core/NkCamera.h"
#include "../../Core/NkRenderGraph.h"
#include "../../Materials/NkMaterialSystem.h"
#include "../../Mesh/NkMeshSystem.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
namespace renderer {

    class NkShadowSystem;

    enum class NkViewMode : uint8 {
        NK_SOLID, NK_WIREFRAME, NK_NORMALS, NK_UV, NK_DEPTH, NK_AO, NK_UNLIT,
    };

    struct NkSceneContext {
        NkCamera3D              camera;
        NkVector<NkLightDesc>   lights;
        NkTexHandle             envMap;
        float32                 ambientIntensity = 0.2f;
        float32                 time   = 0.f;
        float32                 deltaTime = 0.f;
        NkViewMode              viewMode  = NkViewMode::NK_SOLID;
    };

    class NkRender3D {
    public:
        NkRender3D() = default;
        ~NkRender3D();

        bool Init(NkIDevice* device, NkMeshSystem* mesh, NkMaterialSystem* mat,
                   NkRenderGraph* graph, NkShadowSystem* shadow = nullptr);
        void Shutdown();

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
        void DrawDebugAABB  (const NkAABB& box,       NkVec4f color);
        void DrawDebugAxes  (const NkMat4f& t, float32 size=1.f);
        void DrawDebugGrid  (NkVec3f origin, float32 spacing, int32 lines, NkVec4f color);
        void DrawDebugArrow (NkVec3f from, NkVec3f to, NkVec4f color);

    private:
        struct SortedDC { NkDrawCall3D dc; float32 depth; };
        struct DebugLine { NkVec3f a,b; NkVec4f color; float32 life; };

        NkIDevice*        mDevice  = nullptr;
        NkMeshSystem*     mMesh    = nullptr;
        NkMaterialSystem* mMat     = nullptr;
        NkRenderGraph*    mGraph   = nullptr;
        NkShadowSystem*   mShadow  = nullptr;

        NkSceneContext    mCtx;
        bool              mInScene  = false;
        bool              mWireframe= false;

        NkVector<SortedDC>              mOpaque;
        NkVector<SortedDC>              mTransparent;
        NkVector<NkDrawCallInstanced>   mInstanced;
        NkVector<NkDrawCallSkinned>     mSkinned;
        NkVector<DebugLine>             mDebugLines;

        NkBufferHandle mUBOCamera;
        NkBufferHandle mUBOObject;
        NkBufferHandle mUBOLights;
        NkBufferHandle mSSBOBones;

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
