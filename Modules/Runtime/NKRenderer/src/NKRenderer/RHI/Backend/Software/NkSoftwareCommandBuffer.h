#pragma once
// =============================================================================
// NkSoftwareCommandBuffer.h — Command buffer enregistrement + replay CPU
//
// Corrections vs version originale :
//  - L'état d'exécution (pipeline, FBO, VBO, IBO, descriptors) est stocké
//    dans NkSWExecState (struct locale à Execute(), pas de thread_local global)
//  - BeginRenderPass stocke le FBO courant — Draw l'utilise
//  - Dispatch utilise correctement l'état courant du compute pipeline
//  - Support multi-color attachments
// =============================================================================
#include "NKRenderer/RHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NkSoftwareTypes.h"

namespace nkentseu {

class NkSoftwareDevice;

// État d'exécution partagé entre toutes les commandes d'un même Execute()
struct NkSWExecState {
    uint64 pipelineId    = 0;
    uint64 descSetId     = 0;
    uint64 framebufferId = 0;
    uint64 vbIds[8]      = {};
    uint64 vbOffsets[8]  = {};
    uint64 ibId          = 0;
    uint64 ibOffset      = 0;
    bool   ib32          = true;

    // Viewport
    float  vpX=0, vpY=0, vpW=0, vpH=0, vpMinZ=0.f, vpMaxZ=1.f;
    bool   hasViewport = false;

    // Scissor
    int32  scissorX=0, scissorY=0, scissorW=0, scissorH=0;
    bool   hasScissor = false;

    // Push constants
    uint8  pushConstants[256] = {};
    uint32 pushConstantsSize  = 0;

    // Instance courante (pour gl_InstanceIndex)
    uint32 instanceIndex = 0;

    // Résolution vers les objets réels
    NkSWDrawState ResolveDrawState(NkSoftwareDevice* dev) const;
};

// =============================================================================
class NkSoftwareCommandBuffer final : public NkICommandBuffer {
public:
    NkSoftwareCommandBuffer(NkSoftwareDevice* dev, NkCommandBufferType type);
    ~NkSoftwareCommandBuffer() override = default;

    void Begin()  override { mCommands.Clear(); mRecording = true; }
    void End()    override { mRecording = false; }
    void Reset()  override { mCommands.Clear(); mRecording = false; }
    bool IsValid()              const override { return true; }
    NkCommandBufferType GetType() const override { return mType; }

    // Replay : exécute toutes les commandes dans l'ordre d'enregistrement
    void Execute(NkSoftwareDevice* dev);

    // ── Render Pass ───────────────────────────────────────────────────────────
    void BeginRenderPass (NkRenderPassHandle rp, NkFramebufferHandle fb,
                          const NkRect2D& area) override;
    void EndRenderPass   () override;

    // ── Viewport / Scissor ────────────────────────────────────────────────────
    void SetViewport (const NkViewport& vp) override;
    void SetViewports(const NkViewport* vps, uint32 n) override;
    void SetScissor  (const NkRect2D& r) override;
    void SetScissors (const NkRect2D* r, uint32 n) override;

    // ── Pipeline ──────────────────────────────────────────────────────────────
    void BindGraphicsPipeline(NkPipelineHandle p) override;
    void BindComputePipeline (NkPipelineHandle p) override;

    // ── Descriptor Set ────────────────────────────────────────────────────────
    void BindDescriptorSet(NkDescSetHandle set, uint32 idx, uint32* off, uint32 cnt) override;
    void PushConstants    (NkShaderStage stages, uint32 offset, uint32 size,
                           const void* data) override;

    // ── Vertex / Index ────────────────────────────────────────────────────────
    void BindVertexBuffer (uint32 binding, NkBufferHandle buf, uint64 off) override;
    void BindVertexBuffers(uint32 first, const NkBufferHandle* bufs,
                           const uint64* offs, uint32 n) override;
    void BindIndexBuffer  (NkBufferHandle buf, NkIndexFormat fmt, uint64 off) override;

    // ── Draw ─────────────────────────────────────────────────────────────────
    void Draw             (uint32 v, uint32 i, uint32 fv, uint32 fi) override;
    void DrawIndexed      (uint32 idx, uint32 inst, uint32 fi,
                           int32 vo, uint32 fInst) override;
    void DrawIndirect     (NkBufferHandle buf, uint64 off,
                           uint32 cnt, uint32 stride) override;
    void DrawIndexedIndirect(NkBufferHandle buf, uint64 off,
                             uint32 cnt, uint32 stride) override;

    // ── Compute ───────────────────────────────────────────────────────────────
    void Dispatch        (uint32 gx, uint32 gy, uint32 gz) override;
    void DispatchIndirect(NkBufferHandle buf, uint64 off) override;

    // ── Copies ────────────────────────────────────────────────────────────────
    void CopyBuffer         (NkBufferHandle s, NkBufferHandle d,
                             const NkBufferCopyRegion& r) override;
    void CopyBufferToTexture(NkBufferHandle, NkTextureHandle,
                             const NkBufferTextureCopyRegion&) override;
    void CopyTextureToBuffer(NkTextureHandle, NkBufferHandle,
                             const NkBufferTextureCopyRegion&) override;
    void CopyTexture        (NkTextureHandle s, NkTextureHandle d,
                             const NkTextureCopyRegion& r) override;
    void BlitTexture        (NkTextureHandle, NkTextureHandle,
                             const NkTextureCopyRegion&, NkFilter) override;
    void GenerateMipmaps    (NkTextureHandle tex, NkFilter f) override;

    // ── Synchronisation (no-op CPU) ───────────────────────────────────────────
    void Barrier(const NkBufferBarrier*, uint32,
                 const NkTextureBarrier*, uint32) override {}

    // ── Debug ─────────────────────────────────────────────────────────────────
    void BeginDebugGroup (const char* name, float, float, float) override;
    void EndDebugGroup   () override;
    void InsertDebugLabel(const char* name) override;

private:
    // Chaque commande reçoit le device ET l'état courant d'exécution
    using Cmd = NkFunction<void(NkSoftwareDevice*, NkSWExecState&)>;

    void Push(Cmd&& c) {
        if (mRecording) mCommands.PushBack(traits::NkMove(c));
    }

    // Helpers internes utilisés par Draw/DrawIndexed
    static NkSWVertex TransformVertex(const uint8* vdata, uint32 vidx, uint32 stride,
                                       NkSWShader* shader, const void* uniforms);

    static void ExecuteDraw(NkSoftwareDevice* dev, const NkSWExecState& state,
                             const NkVector<NkSWVertex>& verts);

    NkSoftwareDevice*   mDev;
    NkCommandBufferType mType;
    bool                mRecording = false;
    NkVector<Cmd>       mCommands;
};

} // namespace nkentseu