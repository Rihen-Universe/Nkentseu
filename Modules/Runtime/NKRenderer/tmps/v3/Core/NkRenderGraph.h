#pragma once
// =============================================================================
// NkRenderGraph.h
// Ordonnancement des passes de rendu avec gestion des dépendances.
//
// Le RenderGraph déclare les passes et leurs dépendances en ressources.
// À la compilation, il déduit automatiquement :
//   - Les barrières de transition d'état nécessaires
//   - L'ordre optimal d'exécution
//   - Les passes qui peuvent être mergées (render pass merging)
//   - Les ressources transient (qui peuvent être aliasées)
//
// Usage :
//   auto& graph = renderer->GetRenderGraph();
//   auto shadowPass = graph.AddPass("Shadow", NkPassType::NK_SHADOW);
//   shadowPass.Writes(shadowMap);
//
//   auto gPass = graph.AddPass("GBuffer", NkPassType::NK_GEOMETRY);
//   gPass.Reads(shadowMap);
//   gPass.Writes(albedoRT, normalRT, depthRT);
//
//   graph.Compile();   // déduit les dépendances, optimise
//   graph.Execute(cmd);
// =============================================================================
#include "NKRHI/Core/NkTypes.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Core/NkIDevice.h"

#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKContainers/Associative/NkUnorderedMap.h"

namespace nkentseu {

    // =============================================================================
    // Type de passe
    // =============================================================================
    enum class NkPassType : uint8 {
        NK_SHADOW,          // depth only
        NK_GEOMETRY,        // G-buffer / opaque forward
        NK_TRANSPARENT,     // blended geometry
        NK_LIGHTING,        // deferred lighting
        NK_COMPUTE,         // compute dispatch
        NK_POST_PROCESS,    // fullscreen effect
        NK_UI_OVERLAY,      // UI / debug overlay
        NK_PRESENT,         // blit final vers swapchain
        NK_CUSTOM,          // passe custom complète
    };

    // =============================================================================
    // Handle de ressource dans le graph
    // =============================================================================
    using NkGraphResourceId = uint32;
    static constexpr NkGraphResourceId NK_INVALID_GRAPH_RES = 0;

    // =============================================================================
    // Builder de passe
    // =============================================================================
    using NkPassCallback = NkFunction<void(NkICommandBuffer*)>;

    struct NkPassBuilder {
        NkString         name;
        NkPassType       type     = NkPassType::NK_CUSTOM;
        NkPassCallback   execute;

        // Déclaration des ressources
        NkVector<NkGraphResourceId> reads;
        NkVector<NkGraphResourceId> writes;

        // Clear overrides
        bool         clearColor   = false;
        NkClearColor clearColorValue;
        bool         clearDepth   = true;
        float32      clearDepthValue = 1.f;

        // Flags
        bool         enabled      = true;
        bool         async        = false;  // passe sur compute queue async
        float32      gpuTimeMs    = 0.f;    // mesuré au dernier frame

        NkPassBuilder& Reads (NkGraphResourceId r) { reads.PushBack(r);  return *this; }
        NkPassBuilder& Writes(NkGraphResourceId w) { writes.PushBack(w); return *this; }
        NkPassBuilder& Execute(NkPassCallback cb)  { execute = cb;        return *this; }
        NkPassBuilder& SetEnabled(bool v)          { enabled = v;         return *this; }
        NkPassBuilder& SetAsync(bool v)            { async   = v;         return *this; }
        NkPassBuilder& ClearWith(NkClearColor c)   { clearColor=true; clearColorValue=c; return *this; }
    };

    // =============================================================================
    // NkRenderGraph
    // =============================================================================
    class NkRenderGraph {
        public:
            explicit NkRenderGraph(NkIDevice* device);
            ~NkRenderGraph();

            // ── Déclaration des ressources ────────────────────────────────────────────
            NkGraphResourceId ImportTexture(const NkString& name, NkTextureHandle tex,
                                            NkResourceState initialState);
            NkGraphResourceId ImportBuffer(const NkString& name, NkBufferHandle buf,
                                            NkResourceState initialState);
            NkGraphResourceId CreateTransient(const NkString& name,
                                            const NkTextureDesc& desc); // ressource allouée temporairement

            // ── Déclaration des passes ────────────────────────────────────────────────
            NkPassBuilder& AddPass(const NkString& name, NkPassType type);
            NkPassBuilder& AddComputePass(const NkString& name);
            NkPassBuilder& AddPostProcessPass(const NkString& name);

            // ── Compilation & Exécution ───────────────────────────────────────────────
            bool Compile();   // appeler après toutes les déclarations
            void Execute(NkICommandBuffer* cmd);
            void Reset();     // réinitialise pour la frame suivante

            // ── Debug ─────────────────────────────────────────────────────────────────
            NkString DumpGraphDOT()  const;  // export graphe en DOT pour Graphviz
            NkString DumpTimings()   const;  // timing de chaque passe
            bool     IsCompiled()    const { return mCompiled; }

        private:
            NkIDevice*             mDevice;
            NkVector<NkPassBuilder> mPasses;
            NkVector<NkPassBuilder*> mSortedPasses;  // ordre d'exécution après tri topologique
            bool                   mCompiled = false;

            struct GraphResource {
                NkString          name;
                NkTextureHandle   texture;
                NkBufferHandle    buffer;
                NkResourceState   currentState = NkResourceState::NK_UNDEFINED;
                bool              isTransient  = false;
                NkTextureDesc     transientDesc;
            };
            NkVector<GraphResource>      mResources;
            NkUnorderedMap<NkString, NkGraphResourceId> mResourceByName;

            void TopoSort();
            void InsertBarriers(NkICommandBuffer* cmd, NkPassBuilder* pass);
            NkGraphResourceId NextResId();
            uint32 mNextResId = 1;
    };

} // namespace nkentseu