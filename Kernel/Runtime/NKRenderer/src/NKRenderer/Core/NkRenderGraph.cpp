// =============================================================================
// NkRenderGraph.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkRenderGraph.h"
#include "NKLogger/NkLog.h"
#include <cstdio>
#include <cstring>

namespace nkentseu {
    namespace renderer {

        NkRenderGraph::NkRenderGraph(NkIDevice* device) : mDevice(device) {}
        NkRenderGraph::~NkRenderGraph() { Reset(); }

        // =====================================================================
        // Resource declaration
        // =====================================================================
        NkGraphResId NkRenderGraph::ImportTexture(const NkString& name,
                                                   NkTextureHandle tex,
                                                   NkResourceState initialState) {
            GraphResource r;
            r.name         = name;
            r.texture      = tex;
            r.currentState = initialState;
            r.initialState = initialState;
            r.isImported   = true;
            r.isBuffer     = false;
            NkGraphResId id = NextResId();
            mResources.PushBack(r);
            mResByName.Insert(name, id);
            return id;
        }

        NkGraphResId NkRenderGraph::ImportBuffer(const NkString& name,
                                                  NkBufferHandle buf,
                                                  NkResourceState initialState) {
            GraphResource r;
            r.name         = name;
            r.buffer       = buf;
            r.currentState = initialState;
            r.initialState = initialState;
            r.isImported   = true;
            r.isBuffer     = true;
            NkGraphResId id = NextResId();
            mResources.PushBack(r);
            mResByName.Insert(name, id);
            return id;
        }

        NkGraphResId NkRenderGraph::CreateTransient(const NkString& name,
                                                     const NkTextureDesc& desc) {
            GraphResource r;
            r.name          = name;
            r.transientDesc = desc;
            r.isTransient   = true;
            r.isBuffer      = false;
            r.currentState  = NkResourceState::NK_UNDEFINED;
            r.initialState  = NkResourceState::NK_UNDEFINED;
            NkGraphResId id = NextResId();
            mResources.PushBack(r);
            mResByName.Insert(name, id);
            return id;
        }

        NkGraphResId NkRenderGraph::FindByName(const NkString& name) const {
            auto* p = mResByName.Find(name);
            return p ? *p : NK_INVALID_RES_ID;
        }

        // =====================================================================
        // Pass declaration
        // =====================================================================
        NkPassBuilder& NkRenderGraph::AddPass(const NkString& name, NkPassType type) {
            NkPassBuilder b;
            b.name = name;
            b.type = type;
            mPasses.PushBack(b);
            mCompiled = false;
            return mPasses[mPasses.Size() - 1];
        }

        NkPassBuilder& NkRenderGraph::AddComputePass(const NkString& name) {
            return AddPass(name, NkPassType::NK_COMPUTE);
        }
        NkPassBuilder& NkRenderGraph::AddPostProcessPass(const NkString& name) {
            return AddPass(name, NkPassType::NK_POST_PROCESS);
        }

        // =====================================================================
        // Lookup helpers
        // =====================================================================
        NkRenderGraph::GraphResource* NkRenderGraph::FindRes(NkGraphResId id) {
            if (id == NK_INVALID_RES_ID) return nullptr;
            uint32 idx = id - 1;
            if (idx >= (uint32)mResources.Size()) return nullptr;
            return &mResources[idx];
        }
        const NkRenderGraph::GraphResource* NkRenderGraph::FindRes(NkGraphResId id) const {
            if (id == NK_INVALID_RES_ID) return nullptr;
            uint32 idx = id - 1;
            if (idx >= (uint32)mResources.Size()) return nullptr;
            return &mResources[idx];
        }

        // =====================================================================
        // Topological sort (Kahn) + detection de cycles.
        //
        // Une passe A depend d'une passe B (B doit s'executer avant A) ssi A
        // lit (reads ou attachment color/depth) une ressource que B ecrit en
        // dernier. On construit le graphe de dependances, on calcule les
        // in-degrees, on traite la file FIFO des passes a in-degree 0, on
        // verifie qu'on a bien traite toutes les passes (sinon : cycle).
        //
        // L'ordre relatif de declaration est conserve pour les passes
        // independantes (FIFO stable), pour rester predictible cote utilisateur.
        // =====================================================================
        bool NkRenderGraph::TopoSort() {
            const uint32 N = (uint32)mPasses.Size();
            mSorted.Clear();
            mSorted.Reserve(N);

            if (N == 0) return true;

            // Pour chaque ressource : index du dernier pass qui l'ecrit.
            // -1 si pas encore ecrite (resource importee : producteur "externe").
            const uint32 R = (uint32)mResources.Size();
            NkVector<int32> lastWriter;
            lastWriter.Resize(R);
            for (uint32 i = 0; i < R; i++) lastWriter[i] = -1;

            auto resIdx = [](NkGraphResId rid) -> int32 {
                return (rid == NK_INVALID_RES_ID) ? -1 : (int32)(rid - 1);
            };

            // Collecte des dependances : adj[B] contient les passes qui dependent de B.
            NkVector<NkVector<uint32>> adj;
            adj.Resize(N);
            NkVector<uint32> inDegree;
            inDegree.Resize(N);
            for (uint32 i = 0; i < N; i++) inDegree[i] = 0;

            for (uint32 pi = 0; pi < N; pi++) {
                NkPassBuilder& p = mPasses[pi];
                if (!p.enabled) continue;

                // Cette passe lit chaque resource de reads / colors (load=LOAD) / depth (load=LOAD).
                // Pour chaque resource lue, on cree une arete (lastWriter -> pi).
                auto AddDep = [&](int32 ri) {
                    if (ri < 0 || ri >= (int32)R) return;
                    int32 wri = lastWriter[ri];
                    if (wri < 0) return;                       // ressource importee, pas de pass producteur
                    if ((uint32)wri == pi) return;             // self-loop : on ignore
                    adj[(uint32)wri].PushBack(pi);
                    inDegree[pi]++;
                };

                for (auto rid : p.reads) AddDep(resIdx(rid));
                for (auto& ca : p.colors) {
                    if (ca.loadOp == NkLoadOp::NK_LOAD) AddDep(resIdx(ca.resId));
                }
                if (p.hasDepth && p.depth.loadOp == NkLoadOp::NK_LOAD)
                    AddDep(resIdx(p.depth.resId));

                // Apres avoir resolu les dependances de cette passe, on l'enregistre comme
                // dernier writer de ses outputs.
                for (auto& ca : p.colors)        if (resIdx(ca.resId) >= 0) lastWriter[resIdx(ca.resId)] = (int32)pi;
                if (p.hasDepth)                  if (resIdx(p.depth.resId) >= 0) lastWriter[resIdx(p.depth.resId)] = (int32)pi;
                for (auto rid : p.storageWrites) if (resIdx(rid) >= 0) lastWriter[resIdx(rid)] = (int32)pi;
            }

            // Kahn : queue stable (FIFO sur l'index de declaration).
            NkVector<uint32> queue;
            queue.Reserve(N);
            for (uint32 i = 0; i < N; i++) {
                if (mPasses[i].enabled && inDegree[i] == 0) queue.PushBack(i);
            }

            uint32 head = 0;
            while (head < (uint32)queue.Size()) {
                uint32 cur = queue[head++];
                mSorted.PushBack(&mPasses[cur]);
                for (uint32 nxt : adj[cur]) {
                    if (--inDegree[nxt] == 0) queue.PushBack(nxt);
                }
            }

            // Si on n'a pas traite toutes les passes activees -> cycle
            uint32 enabledCount = 0;
            for (uint32 i = 0; i < N; i++) if (mPasses[i].enabled) enabledCount++;
            if ((uint32)mSorted.Size() != enabledCount) {
                NkRSetLastError(NkRResult::NK_ERR_VALIDATION_FAILED,
                                "NkRenderGraph::TopoSort cycle detected");
                return false;
            }
            return true;
        }

        // =====================================================================
        // Pass culling : eliminer les passes dont AUCUN output n'est consomme.
        //
        // Algorithme :
        //   1. Marquer comme "alive" toute resource importee dont l'etat final
        //      est PRESENT (ou tout ce qui est imported : c'est un output exterieur).
        //   2. Repeter : pour chaque passe ecrivant une resource alive, marquer
        //      la passe alive et marquer toutes ses inputs alive.
        //   3. Iterate jusqu'au point fixe.
        //   4. Desactiver les passes non alive.
        // =====================================================================
        void NkRenderGraph::CullDeadPasses() {
            const uint32 N = (uint32)mPasses.Size();
            const uint32 R = (uint32)mResources.Size();
            if (N == 0 || R == 0) return;

            NkVector<bool> aliveRes;  aliveRes.Resize(R);
            NkVector<bool> alivePass; alivePass.Resize(N);
            for (uint32 i = 0; i < R; i++) aliveRes[i]  = mResources[i].isImported;
            for (uint32 i = 0; i < N; i++) alivePass[i] = false;

            auto resIdx = [](NkGraphResId rid) -> int32 {
                return (rid == NK_INVALID_RES_ID) ? -1 : (int32)(rid - 1);
            };

            bool changed = true;
            while (changed) {
                changed = false;
                for (uint32 pi = 0; pi < N; pi++) {
                    NkPassBuilder& p = mPasses[pi];
                    if (!p.enabled || alivePass[pi]) continue;

                    bool writesAlive = false;
                    for (auto& ca : p.colors)        { int32 i = resIdx(ca.resId);     if (i >= 0 && aliveRes[i]) { writesAlive = true; break; } }
                    if (!writesAlive && p.hasDepth)  { int32 i = resIdx(p.depth.resId);if (i >= 0 && aliveRes[i])   writesAlive = true; }
                    if (!writesAlive) for (auto rid : p.storageWrites) { int32 i = resIdx(rid); if (i >= 0 && aliveRes[i]) { writesAlive = true; break; } }

                    if (writesAlive) {
                        alivePass[pi] = true;
                        for (auto rid : p.reads)            { int32 i = resIdx(rid);          if (i >= 0 && !aliveRes[i]) { aliveRes[i] = true; changed = true; } }
                        for (auto& ca : p.colors)           { int32 i = resIdx(ca.resId);     if (i >= 0 && !aliveRes[i] && ca.loadOp == NkLoadOp::NK_LOAD) { aliveRes[i] = true; changed = true; } }
                        if (p.hasDepth && p.depth.loadOp == NkLoadOp::NK_LOAD)
                                                            { int32 i = resIdx(p.depth.resId);if (i >= 0 && !aliveRes[i]) { aliveRes[i] = true; changed = true; } }
                        changed = true;
                    }
                }
            }

            // Desactive les passes non alive
            for (uint32 pi = 0; pi < N; pi++) {
                if (!alivePass[pi]) mPasses[pi].enabled = false;
            }
        }

        // =====================================================================
        // Allocation des transients
        // =====================================================================
        bool NkRenderGraph::AllocateTransients() {
            for (auto& res : mResources) {
                if (res.isTransient && !res.isBuffer && !res.texture.IsValid()) {
                    res.texture = mDevice->CreateTexture(res.transientDesc);
                    if (!res.texture.IsValid()) {
                        NkRSetLastError(NkRResult::NK_ERR_OUT_OF_MEMORY,
                                        "NkRenderGraph : transient texture creation failed");
                        return false;
                    }
                }
            }
            return true;
        }

        // =====================================================================
        // Compile
        // =====================================================================
        NkRResult NkRenderGraph::Compile() {
            if (!AllocateTransients()) return NkRResult::NK_ERR_OUT_OF_MEMORY;
            if (!TopoSort())           return NkRResult::NK_ERR_VALIDATION_FAILED;
            CullDeadPasses();
            mCompiled = true;
            return NkRResult::NK_OK;
        }

        // =====================================================================
        // Barrier insertion
        // =====================================================================
        void NkRenderGraph::TransitionResource(NkICommandBuffer* cmd, GraphResource& res,
                                                NkResourceState newState) {
            if (res.currentState == newState) return;
            if (res.isBuffer) {
                if (res.buffer.IsValid()) {
                    NkBufferBarrier b{res.buffer, res.currentState, newState,
                                       NkPipelineStage::NK_ALL_COMMANDS,
                                       NkPipelineStage::NK_ALL_COMMANDS};
                    cmd->Barrier(&b, 1, nullptr, 0);
                }
            } else {
                if (res.texture.IsValid())
                    cmd->TextureBarrier(res.texture, res.currentState, newState);
            }
            res.currentState = newState;
        }

        void NkRenderGraph::InsertBarriers(NkICommandBuffer* cmd, NkPassBuilder& pass) {
            // Reads -> SHADER_READ
            for (NkGraphResId rid : pass.reads) {
                if (auto* r = FindRes(rid)) TransitionResource(cmd, *r, NkResourceState::NK_SHADER_READ);
            }
            // Color attachments -> RENDER_TARGET
            for (auto& ca : pass.colors) {
                if (auto* r = FindRes(ca.resId)) TransitionResource(cmd, *r, NkResourceState::NK_RENDER_TARGET);
            }
            // Depth attachment -> DEPTH_WRITE (ou DEPTH_READ si store-op==DONT_CARE+load==LOAD ?
            // pour simplicite : DEPTH_WRITE)
            if (pass.hasDepth) {
                if (auto* r = FindRes(pass.depth.resId))
                    TransitionResource(cmd, *r, NkResourceState::NK_DEPTH_WRITE);
            }
            // Storage writes -> UNORDERED_ACCESS
            for (NkGraphResId rid : pass.storageWrites) {
                if (auto* r = FindRes(rid)) TransitionResource(cmd, *r, NkResourceState::NK_UNORDERED_ACCESS);
            }
        }

        // =====================================================================
        // Execute
        // =====================================================================
        NkRResult NkRenderGraph::Execute(NkICommandBuffer* cmd) {
            if (!cmd) return NkRResult::NK_ERR_INVALID_ARGUMENT;
            if (!mCompiled) {
                NkRResult r = Compile();
                if (!NkROk(r)) return r;
            }

            // Dimensions du target — pour OpenGL on utilise le default framebuffer
            // du swapchain. Vulkan/DX12 utiliseront un FB dedie cree par le graph.
            const uint32 swW = mDevice->GetSwapchainWidth();
            const uint32 swH = mDevice->GetSwapchainHeight();

            static int sFrameCounter = 0;
            const bool logFrame = (sFrameCounter++ < 3);
            if (logFrame) logger.Info("[NkRenderGraph] Execute frame={0} : passes={1} swap={2}x{3}\n",
                                       (uint32)sFrameCounter, (uint32)mSorted.Size(), swW, swH);

            for (NkPassBuilder* passPtr : mSorted) {
                NkPassBuilder& pass = *passPtr;
                if (logFrame) logger.Info("[NkRenderGraph]   pass '{0}' enabled={1} hasExecute={2} colors={3} hasDepth={4}\n",
                                           pass.name, pass.enabled, (bool)pass.execute,
                                           (uint32)pass.colors.Size(), pass.hasDepth);
                if (!pass.enabled || !pass.execute) continue;

                // 1. Barrieres d'entree
                InsertBarriers(cmd, pass);

                bool needsRP = (pass.type != NkPassType::NK_COMPUTE);
                if (needsRP) {
                    // Setup des valeurs de clear (consommees par BeginRenderPass)
                    if (!pass.colors.Empty()) {
                        const auto& c0 = pass.colors[0];
                        if (c0.loadOp == NkLoadOp::NK_CLEAR) {
                            cmd->SetClearColor(c0.clearRGBA.x, c0.clearRGBA.y,
                                               c0.clearRGBA.z, c0.clearRGBA.w);
                        }
                    }
                    if (pass.hasDepth && pass.depth.loadOp == NkLoadOp::NK_CLEAR) {
                        cmd->SetClearDepth(pass.depth.clearDepth, pass.depth.clearStencil);
                    }

                    // ── Choix du framebuffer ────────────────────────────────────
                    // Si la passe ecrit dans des color/depth attachments transients
                    // (= textures off-screen), on cree un FBO custom cache par nom.
                    // Si tous les color/depth sont la swapchain (texture invalide
                    // dans le GraphResource), on utilise FBO 0 (default framebuffer).
                    NkFramebufferHandle fbHandle{};
                    uint32 rpW = swW, rpH = swH;

                    // Sur OpenGL, le swapchain (FBO 0) ne peut PAS etre attache a
                    // un FBO custom. On ne cree donc un FBO custom que si AU MOINS
                    // une color est une texture transiente off-screen. Sinon FBO 0
                    // est utilise — qui a son propre depth, donc un depth transient
                    // declare sur cette pass est ignore (cas Demo3D actuel sans HDR).
                    bool needsCustomFB = false;
                    for (auto& ca : pass.colors) {
                        if (auto* r = FindRes(ca.resId)) {
                            if (r->isTransient && r->texture.IsValid()) {
                                needsCustomFB = true;
                                rpW = r->transientDesc.width;
                                rpH = r->transientDesc.height;
                                break;
                            }
                        }
                    }
                    // Pass shadow-only (depth transient sans color) : FBO custom OK
                    if (!needsCustomFB && pass.colors.Empty() && pass.hasDepth) {
                        if (auto* r = FindRes(pass.depth.resId)) {
                            if (r->isTransient && r->texture.IsValid()) {
                                needsCustomFB = true;
                                rpW = r->transientDesc.width;
                                rpH = r->transientDesc.height;
                            }
                        }
                    }

                    if (needsCustomFB) {
                        // Cache hit ?
                        if (auto* cached = mFBCache.Find(pass.name)) {
                            fbHandle = *cached;
                        } else {
                            NkFramebufferDesc fbd;
                            fbd.width = rpW; fbd.height = rpH;
                            fbd.debugName = pass.name.CStr();
                            for (auto& ca : pass.colors) {
                                if (auto* r = FindRes(ca.resId)) {
                                    if (r->texture.IsValid())
                                        fbd.colorAttachments.PushBack(r->texture);
                                }
                            }
                            if (pass.hasDepth) {
                                if (auto* r = FindRes(pass.depth.resId)) {
                                    if (r->texture.IsValid())
                                        fbd.depthAttachment = r->texture;
                                }
                            }
                            fbHandle = mDevice->CreateFramebuffer(fbd);
                            if (fbHandle.IsValid()) {
                                mFBCache.Insert(pass.name, fbHandle);
                            }
                        }
                    }
                    // Si !needsCustomFB ou si CreateFramebuffer a echoue, fbHandle
                    // reste invalide -> BeginRenderPass utilise FBO 0 (swapchain).

                    NkRect2D area((int32)0, (int32)0, (int32)rpW, (int32)rpH);
                    cmd->BeginRenderPass(NkRenderPassHandle{}, fbHandle, area);
                }

                // 2. Callback utilisateur (les draw calls vont dans le RP courant)
                pass.execute(cmd);

                if (needsRP) {
                    cmd->EndRenderPass();
                }
            }

            return NkRResult::NK_OK;
        }

        // =====================================================================
        // Reset (libere transients, prepare pour la frame suivante)
        // =====================================================================
        void NkRenderGraph::Reset() {
            for (auto& r : mResources) {
                if (r.isTransient && r.texture.IsValid()) {
                    mDevice->DestroyTexture(r.texture);
                    r.texture = NkTextureHandle{};
                }
            }
            // Liberer le cache de FBO custom — ils referencaient les transients
            // qu'on vient de detruire, donc invalides desormais.
            for (auto it = mFBCache.begin(); it != mFBCache.end(); ++it) {
                if (it->Second.IsValid()) mDevice->DestroyFramebuffer(it->Second);
            }
            mFBCache.Clear();
            mPasses.Clear();
            mSorted.Clear();
            mResources.Clear();
            mResByName.Clear();
            mNextResId = 1;
            mCompiled  = false;
        }

        // =====================================================================
        // Debug : DOT (graphviz) et timings
        // =====================================================================
        NkString NkRenderGraph::DumpDOT() const {
            NkString out = "digraph NkRenderGraph {\n  rankdir=LR;\n  node [shape=box];\n";

            // Resources : ovales
            for (uint32 i = 0; i < (uint32)mResources.Size(); i++) {
                const auto& r = mResources[i];
                out += "  \"" + r.name + "\" [shape=oval";
                if (r.isTransient) out += ", style=dashed";
                out += "];\n";
            }

            // Passes
            for (uint32 pi = 0; pi < (uint32)mPasses.Size(); pi++) {
                const auto& p = mPasses[pi];
                if (!p.enabled) continue;
                out += "  \"" + p.name + "\" [shape=box, style=filled, fillcolor=lightblue];\n";
                for (auto rid : p.reads) {
                    if (auto* r = FindRes(rid)) out += "  \"" + r->name + "\" -> \"" + p.name + "\";\n";
                }
                for (auto& ca : p.colors) {
                    if (auto* r = FindRes(ca.resId))
                        out += "  \"" + p.name + "\" -> \"" + r->name + "\" [color=red];\n";
                }
                if (p.hasDepth) {
                    if (auto* r = FindRes(p.depth.resId))
                        out += "  \"" + p.name + "\" -> \"" + r->name + "\" [color=blue,label=depth];\n";
                }
                for (auto rid : p.storageWrites) {
                    if (auto* r = FindRes(rid))
                        out += "  \"" + p.name + "\" -> \"" + r->name + "\" [color=green,label=storage];\n";
                }
            }

            out += "}\n";
            return out;
        }

        NkString NkRenderGraph::DumpTimings() const {
            NkString out = "=== RenderGraph Timings ===\n";
            float32 totalGpu = 0.f, totalCpu = 0.f;
            for (auto* p : mSorted) {
                char buf[160];
                snprintf(buf, sizeof(buf), "  %-24s : GPU %.3f ms  CPU %.3f ms\n",
                         p->name.CStr(), p->gpuTimeMs, p->cpuTimeMs);
                out += buf;
                totalGpu += p->gpuTimeMs;
                totalCpu += p->cpuTimeMs;
            }
            char total[128];
            snprintf(total, sizeof(total),
                     "  ----------------------------------------\n"
                     "  TOTAL                    : GPU %.3f ms  CPU %.3f ms\n",
                     totalGpu, totalCpu);
            out += total;
            return out;
        }

    } // namespace renderer
} // namespace nkentseu
