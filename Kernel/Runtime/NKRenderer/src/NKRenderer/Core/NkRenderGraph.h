#pragma once
// =============================================================================
// NkRenderGraph.h  — NKRenderer v5.0  (Core/)
//
// Render Graph (Frame Graph) inspire UE5/Frostbite/Granite.
//
// Concept :
//   1. Declarer les ressources (Import si externe, CreateTransient si gerees ici)
//   2. Declarer les passes (fluent API : Reads/SetColor/SetDepth/Execute)
//   3. Compile() : tri topologique, pass culling, allocation transient,
//      construction du plan de barrieres
//   4. Execute(cmd) : applique les barrieres, lance chaque pass via callback,
//      mesure les timings
//   5. Reset() : libere les transients, prepare pour la frame suivante
//
// Avantages :
//   - Decouplage des subsystemes (Render2D, Render3D, Shadow, PostProcess)
//   - Pass culling automatique (les passes dont les outputs ne sont pas consommes
//     sont supprimees a la compilation par traversee inverse depuis les imported)
//   - Topological sort Kahn avec detection de cycles (NK_ERR_VALIDATION_FAILED)
//   - Barriers automatiques par tracking d'etat des ressources (Read/Write/Depth)
//   - Resource lifecycle : transients alloues par AllocateTransients(),
//     liberes par Reset()
//
// Sur backends a render-pass explicite (Vulkan/DX12), une couche cache
// RenderPass+Framebuffer pourra etre branchee derriere l'API courante en
// reutilisant la description NkPassColorAttachment / NkPassDepthAttachment.
// =============================================================================
#include "NkRendererTypes.h"
#include "NkRendererResult.h"
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

        // =====================================================================
        // Types de passe
        // =====================================================================
        enum class NkPassType : uint8 {
            NK_SHADOW,        // depth-only (CSM cascades)
            NK_GEOMETRY,      // GBuffer / Forward+ opaque
            NK_TRANSPARENT,   // alpha-blend, back-to-front
            NK_LIGHTING,      // deferred lighting / clustered light cull
            NK_COMPUTE,       // pure compute (no render targets)
            NK_POST_PROCESS,  // fullscreen ops on color buffer
            NK_UI_OVERLAY,    // 2D UI on top
            NK_PRESENT,       // copy to swapchain
            NK_CUSTOM,
        };

        // =====================================================================
        // Attachment d'une passe (slot color ou depth)
        // =====================================================================
        struct NkPassColorAttachment {
            NkGraphResId  resId    = NK_INVALID_RES_ID;
            NkLoadOp      loadOp   = NkLoadOp::NK_LOAD;
            NkStoreOp     storeOp  = NkStoreOp::NK_STORE;
            NkVec4f       clearRGBA= {0.05f, 0.05f, 0.07f, 1.f};
        };
        struct NkPassDepthAttachment {
            NkGraphResId  resId       = NK_INVALID_RES_ID;
            NkLoadOp      loadOp      = NkLoadOp::NK_CLEAR;
            NkStoreOp     storeOp     = NkStoreOp::NK_STORE;
            NkLoadOp      stencilLoad = NkLoadOp::NK_DONT_CARE;
            NkStoreOp     stencilStore= NkStoreOp::NK_DONT_CARE;
            float32       clearDepth  = 1.f;
            uint8         clearStencil= 0;
        };

        // =====================================================================
        // Builder de passe (fluent API)
        // =====================================================================
        struct NkPassBuilder {
            NkString               name;
            NkPassType             type     = NkPassType::NK_CUSTOM;
            NkPassCallback         execute;

            // Reads (inputs : sampled textures, UBOs, SSBOs)
            NkVector<NkGraphResId>   reads;

            // Writes (outputs non-render-target : compute storage)
            NkVector<NkGraphResId>   storageWrites;

            // Color attachments (jusqu'a 8 slots — MRT)
            NkVector<NkPassColorAttachment> colors;

            // Depth attachment (au plus 1)
            bool                     hasDepth = false;
            NkPassDepthAttachment    depth;

            // Flags
            bool     enabled       = true;
            bool     asyncCompute  = false;
            // Si true, CullDeadPasses ne tue pas cette passe meme si elle n'a
            // aucun attachment ecrit dans le graph. Utile pour les passes
            // "callback-only" (ex : Shadow, Compute setup) qui produisent leurs
            // outputs hors-graph (FBO custom interne au sous-systeme).
            bool     alwaysExecute = false;

            // Stats (mises a jour par Execute)
            float32  gpuTimeMs    = 0.f;
            float32  cpuTimeMs    = 0.f;

            // ── Inputs ────────────────────────────────────────────────────────
            NkPassBuilder& Reads(NkGraphResId r) { reads.PushBack(r); return *this; }
            NkPassBuilder& Reads(NkGraphResId a, NkGraphResId b) {
                reads.PushBack(a); reads.PushBack(b); return *this;
            }
            NkPassBuilder& Reads(NkGraphResId a, NkGraphResId b, NkGraphResId c) {
                reads.PushBack(a); reads.PushBack(b); reads.PushBack(c); return *this;
            }

            // ── Color attachments ─────────────────────────────────────────────
            NkPassBuilder& SetColor(uint32 slot, NkGraphResId res,
                                    NkLoadOp load = NkLoadOp::NK_LOAD,
                                    NkVec4f  clearRGBA = {0,0,0,1}) {
                while (colors.Size() <= slot) colors.PushBack({});
                colors[slot].resId    = res;
                colors[slot].loadOp   = load;
                colors[slot].clearRGBA= clearRGBA;
                return *this;
            }

            // Raccourci : ajoute un slot color avec load=CLEAR, valeur fournie
            NkPassBuilder& ClearColor(NkGraphResId res, NkVec4f rgba = {0.05f,0.05f,0.07f,1.f}) {
                return SetColor((uint32)colors.Size(), res, NkLoadOp::NK_CLEAR, rgba);
            }

            // ── Depth attachment ──────────────────────────────────────────────
            NkPassBuilder& SetDepth(NkGraphResId res,
                                    NkLoadOp load = NkLoadOp::NK_CLEAR,
                                    float32  clearD = 1.f) {
                hasDepth = true;
                depth.resId      = res;
                depth.loadOp     = load;
                depth.clearDepth = clearD;
                return *this;
            }

            // ── Storage write (compute) ───────────────────────────────────────
            NkPassBuilder& WritesStorage(NkGraphResId r) {
                storageWrites.PushBack(r); return *this;
            }

            // ── Compatibilite : Writes() est rerouter vers SetColor (slot 0..N)
            //  pour les passes graphics, ou vers WritesStorage pour les compute.
            // ───────────────────────────────────────────────────────────────────
            NkPassBuilder& Writes(NkGraphResId r) {
                if (type == NkPassType::NK_COMPUTE) return WritesStorage(r);
                return SetColor((uint32)colors.Size(), r, NkLoadOp::NK_LOAD);
            }
            NkPassBuilder& Writes(NkGraphResId a, NkGraphResId b) {
                Writes(a); Writes(b); return *this;
            }
            NkPassBuilder& Writes(NkGraphResId a, NkGraphResId b, NkGraphResId c) {
                Writes(a); Writes(b); Writes(c); return *this;
            }

            // ── Callback ──────────────────────────────────────────────────────
            NkPassBuilder& Execute(NkPassCallback cb) { execute = cb; return *this; }

            // ── Flags ─────────────────────────────────────────────────────────
            NkPassBuilder& SetEnabled(bool v)       { enabled = v; return *this; }
            NkPassBuilder& SetAsync  (bool v)       { asyncCompute = v; return *this; }
            NkPassBuilder& SetAlwaysExecute(bool v) { alwaysExecute = v; return *this; }

            // ── Compatibilite : ClearWith(color) → premiere color attachment
            // ───────────────────────────────────────────────────────────────────
            NkPassBuilder& ClearWith(NkVec3f c, float32 a = 1.f) {
                if (colors.Empty()) colors.PushBack({});
                colors[0].loadOp   = NkLoadOp::NK_CLEAR;
                colors[0].clearRGBA= {c.x, c.y, c.z, a};
                return *this;
            }
            NkPassBuilder& ClearWith(NkVec4f rgba) {
                if (colors.Empty()) colors.PushBack({});
                colors[0].loadOp    = NkLoadOp::NK_CLEAR;
                colors[0].clearRGBA = rgba;
                return *this;
            }
            NkPassBuilder& ClearWith(math::NkColorF c) {
                if (colors.Empty()) colors.PushBack({});
                colors[0].loadOp    = NkLoadOp::NK_CLEAR;
                colors[0].clearRGBA = {c.r, c.g, c.b, c.a};
                return *this;
            }
        };

        // =====================================================================
        // NkRenderGraph
        // =====================================================================
        class NkRenderGraph {
            public:
                explicit NkRenderGraph(NkIDevice* device);
                ~NkRenderGraph();

                NkRenderGraph(const NkRenderGraph&)            = delete;
                NkRenderGraph& operator=(const NkRenderGraph&) = delete;

                // ── Declaration des ressources ──────────────────────────────────
                NkGraphResId ImportTexture (const NkString& name, NkTextureHandle tex,
                                            NkResourceState initialState);
                NkGraphResId ImportBuffer  (const NkString& name, NkBufferHandle  buf,
                                            NkResourceState initialState);
                NkGraphResId CreateTransient(const NkString& name, const NkTextureDesc& desc);
                NkGraphResId FindByName    (const NkString& name) const;

                // Resoud un NkGraphResId vers le NkTextureHandle RHI sous-jacent.
                // Utile aux passes PostProcess qui ont besoin du handle pour binder
                // un transient comme input shader (ex : HDR -> tonemap).
                // Retourne un handle invalide si l'id ne correspond a rien ou si
                // la ressource est un buffer ou n'a pas encore ete allouee.
                NkTextureHandle GetResourceTexture(NkGraphResId id) const;

                // Recupere le RenderPass handle associe au framebuffer cache d'une
                // passe (par nom). Utile pour le lazy create de pipelines qui
                // doivent etre RP-compatibles avec le fb cible (Vulkan/DX12).
                // Retourne un handle invalide si la pass n'a pas encore execute,
                // si elle n'a pas de FB custom (= utilise le swapchain) ou si le
                // backend ne fournit pas de RP (ex : OpenGL renvoie {}).
                NkRenderPassHandle GetPassRenderPass(const NkString& passName) const;

                // ── Declaration des passes ──────────────────────────────────────
                NkPassBuilder& AddPass            (const NkString& name, NkPassType type);
                NkPassBuilder& AddComputePass     (const NkString& name);
                NkPassBuilder& AddPostProcessPass (const NkString& name);

                // ── Compilation & Execution ─────────────────────────────────────
                NkRResult Compile();
                NkRResult Execute(NkICommandBuffer* cmd);
                void      Reset();

                // ── Debug / inspection ──────────────────────────────────────────
                NkString DumpDOT()      const;
                NkString DumpTimings()  const;
                bool     IsCompiled()   const noexcept { return mCompiled; }
                uint32   GetPassCount() const noexcept { return (uint32)mPasses.Size(); }
                uint32   GetActivePassCount() const noexcept { return (uint32)mSorted.Size(); }
                uint32   GetResourceCount()   const noexcept { return (uint32)mResources.Size(); }

            private:
                struct GraphResource {
                    NkString        name;
                    NkTextureHandle texture;
                    NkBufferHandle  buffer;
                    NkResourceState currentState  = NkResourceState::NK_UNDEFINED;
                    NkResourceState initialState  = NkResourceState::NK_UNDEFINED;
                    NkResourceState finalState    = NkResourceState::NK_UNDEFINED;
                    bool            isTransient   = false;
                    NkTextureDesc   transientDesc = {};
                    bool            isImported    = false;
                    bool            isBuffer      = false;
                    int32           lastWriter    = -1;     // index du dernier pass qui ecrit
                    int32           firstReader   = -1;     // index du premier pass qui lit
                    int32           lifeBegin     = -1;     // pour aliasing
                    int32           lifeEnd       = -1;
                };

                NkIDevice*                            mDevice;
                NkVector<NkPassBuilder>               mPasses;
                NkVector<NkPassBuilder*>              mSorted;
                NkVector<GraphResource>               mResources;
                NkHashMap<NkString, NkGraphResId>     mResByName;
                bool                                  mCompiled   = false;
                uint32                                mNextResId  = 1;

                // Cache de framebuffers crees a partir des color/depth attachments
                // de chaque pass — evite la recreation a chaque frame. La key est
                // le nom du pass (suppose stable entre frames). Reset() les libere.
                NkHashMap<NkString, NkFramebufferHandle> mFBCache;

                // Cache de RenderPass custom pour les passes qui ecrivent dans le
                // swapchain. Le swapchain RP du device a loadOp=CLEAR fixe, donc
                // toutes les passes consecutives effacent le contenu precedent.
                // On cree un RP par pass avec ses loadOp specifiques (compatible
                // avec le swapchain FB car memes formats). Key = pass.name + suffix
                // loadOp signature. Reset() les libere.
                NkHashMap<NkString, NkRenderPassHandle> mSwapchainRPCache;

                // ── Helpers internes ────────────────────────────────────────────
                NkGraphResId NextResId() noexcept { return mNextResId++; }
                GraphResource* FindRes(NkGraphResId id);
                const GraphResource* FindRes(NkGraphResId id) const;

                bool TopoSort();
                void CullDeadPasses();
                bool AllocateTransients();
                void TransitionResource(NkICommandBuffer* cmd, GraphResource& res, NkResourceState newState);
                void InsertBarriers(NkICommandBuffer* cmd, NkPassBuilder& pass);
        };

    } // namespace renderer
} // namespace nkentseu
