#pragma once
// =============================================================================
// NkRenderGraph.h  — NKRenderer v4.0  (Core/)
// Ordonnancement des passes GPU, insertion automatique des barrières,
// tri topologique, ressources transient aliasables.
// =============================================================================
#include "NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/Associative/NkHashMap.h"

namespace nkentseu {
namespace renderer {

    using NkPassCallback = NkFunction<void(NkICommandBuffer*)>;
    using NkGraphResId   = uint32;
    static constexpr NkGraphResId NK_INVALID_RES_ID = 0;

    // =========================================================================
    // Types de passe
    // =========================================================================
    enum class NkPassType : uint8 {
        NK_SHADOW,
        NK_GEOMETRY,
        NK_TRANSPARENT,
        NK_LIGHTING,
        NK_COMPUTE,
        NK_POST_PROCESS,
        NK_UI_OVERLAY,
        NK_PRESENT,
        NK_CUSTOM,
    };

    // =========================================================================
    // Builder de passe (fluent API)
    // =========================================================================
    struct NkPassBuilder {
        NkString            name;
        NkPassType          type     = NkPassType::NK_CUSTOM;
        NkPassCallback      execute;
        NkVector<NkGraphResId> reads;
        NkVector<NkGraphResId> writes;
        bool     enabled            = true;
        bool     asyncCompute       = false;
        bool     clearColor         = false;
        NkVec3f  clearColorValue    = {0.05f,0.05f,0.07f};
        float32  clearColorAlpha    = 1.f;
        bool     clearDepth         = true;
        float32  clearDepthValue    = 1.f;
        float32  gpuTimeMs          = 0.f;   // mesuré frame précédente

        NkPassBuilder& Reads(NkGraphResId r)     { reads.PushBack(r);  return *this; }
        NkPassBuilder& Writes(NkGraphResId w)    { writes.PushBack(w); return *this; }

        NkPassBuilder& Reads(NkGraphResId a, NkGraphResId b)
            { reads.PushBack(a); reads.PushBack(b); return *this; }
        NkPassBuilder& Writes(NkGraphResId a, NkGraphResId b)
            { writes.PushBack(a); writes.PushBack(b); return *this; }
        NkPassBuilder& Writes(NkGraphResId a, NkGraphResId b, NkGraphResId c)
            { writes.PushBack(a); writes.PushBack(b); writes.PushBack(c); return *this; }

        NkPassBuilder& Execute(NkPassCallback cb) { execute = cb;    return *this; }
        NkPassBuilder& SetEnabled(bool v)          { enabled = v;    return *this; }
        NkPassBuilder& SetAsync(bool v)            { asyncCompute=v; return *this; }
        NkPassBuilder& ClearWith(NkVec3f c, float32 a = 1.f)
            { clearColor=true; clearColorValue=c; clearColorAlpha=a; return *this; }
    };

    // =========================================================================
    // NkRenderGraph
    // =========================================================================
    class NkRenderGraph {
    public:
        explicit NkRenderGraph(NkIDevice* device);
        ~NkRenderGraph();

        // ── Déclaration des ressources ────────────────────────────────────────
        NkGraphResId ImportTexture(const NkString& name,
                                    NkTextureHandle tex,
                                    NkResourceState initialState);
        NkGraphResId ImportBuffer (const NkString& name,
                                    NkBufferHandle  buf,
                                    NkResourceState initialState);
        NkGraphResId CreateTransient(const NkString& name,
                                      const NkTextureDesc& desc);

        // ── Déclaration des passes ────────────────────────────────────────────
        NkPassBuilder& AddPass(const NkString& name, NkPassType type);
        NkPassBuilder& AddComputePass(const NkString& name);
        NkPassBuilder& AddPostProcessPass(const NkString& name);

        // ── Compilation & Exécution ───────────────────────────────────────────
        bool Compile();
        void Execute(NkICommandBuffer* cmd);
        void Reset();

        // ── Debug ─────────────────────────────────────────────────────────────
        NkString DumpDOT()     const;
        NkString DumpTimings() const;
        bool     IsCompiled()  const { return mCompiled; }
        uint32   GetPassCount()const { return (uint32)mPasses.Size(); }

    private:
        struct GraphResource {
            NkString        name;
            NkTextureHandle texture;
            NkBufferHandle  buffer;
            NkResourceState currentState  = NkResourceState::NK_UNDEFINED;
            NkResourceState initialState  = NkResourceState::NK_UNDEFINED;
            bool            isTransient   = false;
            NkTextureDesc   transientDesc = {};
            bool            isImported    = false;
        };

        NkIDevice*                            mDevice;
        NkVector<NkPassBuilder>               mPasses;
        NkVector<NkPassBuilder*>              mSorted;
        NkVector<GraphResource>               mResources;
        NkHashMap<NkString, NkGraphResId>     mResByName;
        bool                                  mCompiled = false;
        uint32                                mNextResId = 1;

        NkGraphResId NextResId() { return mNextResId++; }
        void TopoSort();
        void InsertBarriers(NkICommandBuffer* cmd, NkPassBuilder* pass);
        void TransitionResource(NkICommandBuffer* cmd, NkGraphResId resId,
                                 NkResourceState newState);
    };

} // namespace renderer
} // namespace nkentseu
