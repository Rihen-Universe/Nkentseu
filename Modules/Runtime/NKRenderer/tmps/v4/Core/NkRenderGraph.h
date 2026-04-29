#pragma once
// =============================================================================
// NkRenderGraph.h
// Ordonnancement des passes de rendu avec gestion automatique des barrières.
//
// Philosophie (proche de FrameGraph de Frostbite/Unreal) :
//   1. Déclarer les passes et leurs dépendances de textures (read/write)
//   2. Le graph compile l'ordre d'exécution et insère les barrières GPU
//   3. Chaque passe reçoit un callback qui émet des commandes via NkICommandBuffer
//
// Avantages :
//   - Zéro barrière manuelle à écrire
//   - Reuse automatique des render targets temporaires (aliasing)
//   - Debug visuel du graph possible
//   - Passe facilement de Forward à Deferred sans refactoring
//
// Usage :
//   NkRenderGraph graph;
//   graph.Begin(device, w, h);
//
//   auto gbuffer = graph.AddPass("GBuffer")
//       .Write(colorRT, albedoRT, normalRT)
//       .SetDepth(depthRT)
//       .SetCallback([](NkRenderGraphCtx& ctx) {
//           ctx.cmd->BindGraphicsPipeline(...);
//           // ... draw calls ...
//       });
//
//   auto lighting = graph.AddPass("Lighting")
//       .Read(albedoRT).Read(normalRT).Read(depthRT)
//       .Write(hdrRT)
//       .SetCallback([](NkRenderGraphCtx& ctx) { ... });
//
//   graph.AddPass("Tonemap")
//       .Read(hdrRT).WriteSwapchain()
//       .SetCallback([](NkRenderGraphCtx& ctx) { ... });
//
//   graph.Compile();
//   graph.Execute(cmd);
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Functional/NkFunction.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // Contexte passé au callback de chaque passe
        // =============================================================================
        struct NkRenderGraphCtx {
            NkICommandBuffer* cmd        = nullptr;
            uint32            width      = 0;
            uint32            height     = 0;
            uint32            frameIndex = 0;
            float32           time       = 0.f;
            float32           deltaTime  = 0.f;

            // Accès aux textures résolues de la passe courante
            NkTextureHandle GetInput (const NkString& name) const;
            NkTextureHandle GetOutput(const NkString& name) const;
        };

        // =============================================================================
        // Callback de passe
        // =============================================================================
        using NkPassCallback = NkFunction<void(NkRenderGraphCtx&)>;

        // =============================================================================
        // Descripteur d'une attachment de passe
        // =============================================================================
        struct NkPassAttachmentDesc {
            NkString       name;
            NkTextureHandle handle;
            NkPixelFormat  format   = NkPixelFormat::NK_RGBA16F;
            bool           clear    = true;
            NkColorF       clearColor;
            float32        clearDepth = 1.f;

            // Créer une nouvelle texture temporaire pour cette passe
            bool           isTransient = false;
            uint32         transientW  = 0;   // 0 = taille du swapchain
            uint32         transientH  = 0;
        };

        // =============================================================================
        // Constructeur fluide pour une passe
        // =============================================================================
        class NkRenderPassBuilder;
        class NkRenderGraph;

        class NkRenderPassBuilder {
           public:
                NkRenderPassBuilder(NkRenderGraph* graph, const char* name);

                // Déclarer une texture lue par cette passe
                NkRenderPassBuilder& Read(NkTextureHandle tex,
                                           const char* alias = nullptr);

                // Déclarer une texture écrite (color attachment)
                NkRenderPassBuilder& Write(NkTextureHandle tex,
                                            const char* alias = nullptr);

                // Déclarer une texture profondeur
                NkRenderPassBuilder& SetDepth(NkTextureHandle tex);

                // Écrire directement dans le swapchain
                NkRenderPassBuilder& WriteSwapchain();

                // Créer une texture transiente (allouée/libérée par le graph)
                NkRenderPassBuilder& CreateTransient(const char* name,
                                                      NkPixelFormat fmt,
                                                      float32 scaleW = 1.f,
                                                      float32 scaleH = 1.f);

                // Définir le callback de rendu
                NkRenderPassBuilder& SetCallback(NkPassCallback fn);

                // Options
                NkRenderPassBuilder& SetViewport(float32 scaleW = 1.f,
                                                  float32 scaleH = 1.f);
                NkRenderPassBuilder& DisableClear();
                NkRenderPassBuilder& SetClearColor(const NkColorF& c);
                NkRenderPassBuilder& SetClearDepth(float32 d = 1.f);
                NkRenderPassBuilder& SetEnabled(bool enabled);

                // Finalise et retourne le handle de la passe
                uint32 Build();

           private:
                NkRenderGraph* mGraph = nullptr;

                struct PassData {
                    NkString                       name;
                    NkVector<NkPassAttachmentDesc> reads;
                    NkVector<NkPassAttachmentDesc> writes;
                    NkPassAttachmentDesc           depth;
                    bool                           hasDepth      = false;
                    bool                           writeSwapchain= false;
                    NkPassCallback                 callback;
                    bool                           enabled       = true;
                    float32                        scaleW        = 1.f;
                    float32                        scaleH        = 1.f;
                    bool                           clearColor    = true;
                    bool                           clearDepth    = true;
                    NkColorF                       clearColorVal;
                    float32                        clearDepthVal = 1.f;
                };
                PassData mData;

                friend class NkRenderGraph;
        };

        // =============================================================================
        // NkRenderGraph
        // =============================================================================
        class NkRenderGraph {
           public:
                NkRenderGraph()  = default;
                ~NkRenderGraph() { Shutdown(); }

                NkRenderGraph(const NkRenderGraph&)            = delete;
                NkRenderGraph& operator=(const NkRenderGraph&) = delete;

                // ── Cycle de vie ──────────────────────────────────────────────────────────
                bool Init    (NkIDevice* device, uint32 width, uint32 height);
                void Shutdown();
                void Resize  (uint32 width, uint32 height);
                bool IsValid () const { return mDevice != nullptr; }

                // ── Construction du graph (chaque frame, ou seulement quand ça change) ───
                // Ouvrir une nouvelle déclaration de frame
                void Begin(float32 time = 0.f, float32 dt = 0.f);

                // Ajouter une passe de rendu (retourne un builder fluide)
                NkRenderPassBuilder AddPass(const char* name);

                // Compiler l'ordre d'exécution et allouer les textures transientes
                bool Compile();

                // Exécuter le graph compilé
                void Execute(NkICommandBuffer* cmd);

                // ── Accès ─────────────────────────────────────────────────────────────────
                uint32 GetPassCount()   const;
                bool   IsCompiled()     const { return mCompiled; }
                uint32 GetWidth()       const { return mWidth; }
                uint32 GetHeight()      const { return mHeight; }

                // Debug : dump du graph en texte
                NkString DumpGraph()    const;

                // Activer/désactiver une passe par nom
                void SetPassEnabled(const char* name, bool enabled);

                // Accès au device (pour les builders)
                NkIDevice* GetDevice() const { return mDevice; }

           private:
                // ── Données internes ──────────────────────────────────────────────────────
                struct PassNode {
                    NkRenderPassBuilder::PassData data;
                    uint32 order    = 0;         // ordre d'exécution compilé
                    bool   compiled = false;

                    // Handles RHI résolus après compilation
                    uint64 rhiRenderPass  = 0;
                    uint64 rhiFramebuffer = 0;
                    NkVector<uint64> rhiColorTexIds;
                    uint64           rhiDepthTexId = 0;
                };

                struct TransientTexture {
                    NkString  name;
                    uint64    rhiTextureId = 0;
                    uint64    rhiSamplerId = 0;
                    NkPixelFormat format;
                    bool      inUse = false;
                };

                bool TopoSort();
                bool AllocateTransients();
                void FreeTransients();
                void InsertBarriersFor(NkICommandBuffer* cmd,
                                        const PassNode& pass);

                void AddPassInternal(NkRenderPassBuilder::PassData&& data);

                NkIDevice*             mDevice   = nullptr;
                uint32                 mWidth    = 0;
                uint32                 mHeight   = 0;
                bool                   mCompiled = false;
                float32                mTime     = 0.f;
                float32                mDeltaTime= 0.f;

                NkVector<PassNode>          mPasses;
                NkVector<TransientTexture>  mTransients;

                friend class NkRenderPassBuilder;
        };

        // =============================================================================
        // NkStandardPasses — passes pré-définies réutilisables
        // Helpers pour construire rapidement un pipeline forward ou deferred.
        // =============================================================================
        class NkStandardPasses {
           public:
                // Setup Forward PBR : GBuffer → Lighting → Tonemap → Post
                static void SetupForwardPBR(NkRenderGraph& graph,
                                             NkTextureHandle depthRT,
                                             NkTextureHandle outputRT);

                // Setup Deferred PBR : GBuffer → Lighting → SSAO → Bloom → Tonemap
                static void SetupDeferredPBR(NkRenderGraph& graph,
                                              NkTextureHandle depthRT,
                                              NkTextureHandle outputRT,
                                              bool ssao  = true,
                                              bool bloom = true);

                // Setup Shadow pass (directional CSM)
                static NkVector<NkTextureHandle>
                       SetupShadowCSM(NkRenderGraph& graph,
                                       uint32 numCascades = 4,
                                       uint32 resolution  = 2048);
        };

    } // namespace renderer
} // namespace nkentseu
