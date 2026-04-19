// =============================================================================
// NkRenderGraph.cpp — FrameGraph : compilation et exécution des passes
// =============================================================================
#include "NKRenderer/Core/NkRenderGraph.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include <algorithm>

namespace nkentseu {
    namespace renderer {

        // =============================================================================
        // NkRenderGraphCtx
        // =============================================================================
        NkTextureHandle NkRenderGraphCtx::GetInput(const NkString& name) const {
            auto* found = mInputMap ? mInputMap->Find(name) : nullptr;
            return found ? *found : NkTextureHandle{};
        }
        NkTextureHandle NkRenderGraphCtx::GetOutput(const NkString& name) const {
            auto* found = mOutputMap ? mOutputMap->Find(name) : nullptr;
            return found ? *found : NkTextureHandle{};
        }

        // =============================================================================
        // NkRenderPassBuilder
        // =============================================================================
        NkRenderPassBuilder::NkRenderPassBuilder(NkRenderGraph* graph, const char* name)
            : mGraph(graph)
        {
            mPassIdx = graph->AllocPassSlot();
            auto& p = graph->mPasses[mPassIdx];
            p.name     = name;
            p.enabled  = true;
            p.isScreen = false;
        }

        NkRenderPassBuilder& NkRenderPassBuilder::Read(NkTextureHandle tex,
                                                         const char* name)
        {
            auto& p = mGraph->mPasses[mPassIdx];
            NkPassAttachmentDesc att{};
            att.name   = name ? name : "";
            att.handle = tex;
            p.inputs.PushBack(att);
            return *this;
        }

        NkRenderPassBuilder& NkRenderPassBuilder::Write(NkTextureHandle tex,
                                                          const char* name,
                                                          bool clear,
                                                          NkColorF clearColor)
        {
            auto& p = mGraph->mPasses[mPassIdx];
            NkPassAttachmentDesc att{};
            att.name       = name ? name : "";
            att.handle     = tex;
            att.clear      = clear;
            att.clearColor = clearColor;
            p.outputs.PushBack(att);
            return *this;
        }

        NkRenderPassBuilder& NkRenderPassBuilder::WriteTransient(
                const char* name, NkPixelFormat fmt, uint32 w, uint32 h)
        {
            auto& p = mGraph->mPasses[mPassIdx];
            NkPassAttachmentDesc att{};
            att.name        = name;
            att.format      = fmt;
            att.isTransient = true;
            att.transientW  = w;
            att.transientH  = h;
            att.clear       = true;
            p.outputs.PushBack(att);
            return *this;
        }

        NkRenderPassBuilder& NkRenderPassBuilder::SetDepth(NkTextureHandle tex,
                                                             bool clear,
                                                             float32 clearDepth)
        {
            auto& p = mGraph->mPasses[mPassIdx];
            p.depthAttachment.handle     = tex;
            p.depthAttachment.clear      = clear;
            p.depthAttachment.clearDepth = clearDepth;
            p.hasDepth = true;
            return *this;
        }

        NkRenderPassBuilder& NkRenderPassBuilder::WriteSwapchain() {
            auto& p = mGraph->mPasses[mPassIdx];
            p.isScreen = true;
            return *this;
        }

        NkRenderPassBuilder& NkRenderPassBuilder::SetCallback(NkPassCallback cb) {
            mGraph->mPasses[mPassIdx].callback = NkTraits::NkMove(cb);
            return *this;
        }

        NkRenderPassBuilder& NkRenderPassBuilder::SetEnabled(bool v) {
            mGraph->mPasses[mPassIdx].enabled = v;
            return *this;
        }

        NkRenderPassBuilder& NkRenderPassBuilder::After(const char* passName) {
            mGraph->mPasses[mPassIdx].dependencies.PushBack(NkString(passName));
            return *this;
        }

        NkTextureHandle NkRenderPassBuilder::GetOutput(const char* name) const {
            auto& p = mGraph->mPasses[mPassIdx];
            for (uint32 i=0;i<(uint32)p.outputs.Size();++i) {
                if (p.outputs[i].name == name) return p.outputs[i].handle;
            }
            return {};
        }

        // =============================================================================
        // NkRenderGraph
        // =============================================================================
        bool NkRenderGraph::Begin(NkIDevice* device, uint32 w, uint32 h,
                                    uint32 frameIndex)
        {
            if (!device) return false;
            mDevice     = device;
            mWidth      = w;
            mHeight     = h;
            mFrameIndex = frameIndex;
            mPasses.Clear();
            mTransientTextures.Clear();
            mCompiled   = false;
            return true;
        }

        NkRenderPassBuilder NkRenderGraph::AddPass(const char* name) {
            return NkRenderPassBuilder(this, name);
        }

        uint32 NkRenderGraph::AllocPassSlot() {
            NkPassEntry p{};
            mPasses.PushBack(NkTraits::NkMove(p));
            return (uint32)mPasses.Size() - 1;
        }

        // =============================================================================
        bool NkRenderGraph::Compile() {
            if (mCompiled) return true;
            if (!mDevice)  return false;

            // 1. Créer les textures transitoires
            for (uint32 i = 0; i < (uint32)mPasses.Size(); ++i) {
                NkPassEntry& p = mPasses[i];
                if (!p.enabled) continue;
                for (uint32 j = 0; j < (uint32)p.outputs.Size(); ++j) {
                    NkPassAttachmentDesc& att = p.outputs[j];
                    if (!att.isTransient) continue;
                    uint32 w = att.transientW ? att.transientW : mWidth;
                    uint32 h = att.transientH ? att.transientH : mHeight;
                    auto td = NkTextureDesc::RenderTarget(w, h, att.format);
                    td.debugName = att.name.CStr();
                    NkTextureHandle tex = mDevice->CreateTransientTexture(td);
                    att.handle = tex;
                    mTransientTextures.PushBack(tex);
                    // Propager le handle aux passes qui lisent ce nom
                    for (uint32 k = i+1; k < (uint32)mPasses.Size(); ++k) {
                        for (uint32 m = 0; m < (uint32)mPasses[k].inputs.Size(); ++m) {
                            if (mPasses[k].inputs[m].name == att.name) {
                                mPasses[k].inputs[m].handle = tex;
                            }
                        }
                    }
                }
            }

            // 2. Tri topologique (dépendances explicites via After())
            TopoSort();

            mCompiled = true;
            return true;
        }

        void NkRenderGraph::TopoSort() {
            // Tri topologique simple basé sur les dépendances nommées
            NkVector<NkPassEntry> sorted;
            sorted.Reserve(mPasses.Size());
            NkVector<bool> visited((uint32)mPasses.Size(), false);

            // Créer un mapping name → index
            NkHashMap<NkString,uint32> nameIdx;
            for (uint32 i = 0; i < (uint32)mPasses.Size(); ++i)
                nameIdx[mPasses[i].name] = i;

            // DFS
            NkVector<bool> inStack((uint32)mPasses.Size(), false);
            NkFunction<void(uint32)> visit = [&](uint32 idx) {
                if (visited[idx] || inStack[idx]) return;
                inStack[idx] = true;
                for (uint32 d = 0; d < (uint32)mPasses[idx].dependencies.Size(); ++d) {
                    auto* depIdx = nameIdx.Find(mPasses[idx].dependencies[d]);
                    if (depIdx) visit(*depIdx);
                }
                inStack[idx]  = false;
                visited[idx]  = true;
                sorted.PushBack(mPasses[idx]);
            };

            for (uint32 i = 0; i < (uint32)mPasses.Size(); ++i)
                if (!visited[i]) visit(i);

            mPasses = NkTraits::NkMove(sorted);
        }

        // =============================================================================
        void NkRenderGraph::Execute(NkICommandBuffer* cmd, float32 time,
                                     float32 deltaTime)
        {
            NK_ASSERT(mCompiled, "Execute appelé avant Compile()");

            for (uint32 i = 0; i < (uint32)mPasses.Size(); ++i) {
                NkPassEntry& p = mPasses[i];
                if (!p.enabled || !p.callback) continue;

                // ── Barrières d'entrée : lecture ────────────────────────────────────
                for (uint32 j = 0; j < (uint32)p.inputs.Size(); ++j) {
                    if (!p.inputs[j].handle.IsValid()) continue;
                    cmd->TextureBarrier(p.inputs[j].handle,
                                         NkResourceState::NK_RENDER_TARGET,
                                         NkResourceState::NK_SHADER_READ);
                }

                // ── Début de la passe de rendu ──────────────────────────────────────
                NkClearFlags clearFlags = NkClearFlags::NK_NONE;
                NkColorF     clearColor = {};
                float32      clearDepth = 1.f;

                if (p.isScreen) {
                    cmd->BeginSwapchainRenderPass(clearColor, clearDepth);
                } else {
                    // Construire la render target à la volée si nécessaire
                    if (!p.outputs.IsEmpty() || p.hasDepth) {
                        if (p.outputs[0].clear) clearFlags |= NkClearFlags::NK_COLOR;
                        if (p.hasDepth && p.depthAttachment.clear) clearFlags |= NkClearFlags::NK_DEPTH;
                        clearColor = p.outputs.IsEmpty() ? NkColorF{} : p.outputs[0].clearColor;
                        clearDepth = p.hasDepth ? p.depthAttachment.clearDepth : 1.f;

                        // Construire descriptor set transitoire pour la passe
                        NkRenderTargetDesc rtDesc{};
                        rtDesc.width  = mWidth;
                        rtDesc.height = mHeight;
                        for (uint32 j = 0; j < (uint32)p.outputs.Size(); ++j)
                            rtDesc.colorAttachments.PushBack(p.outputs[j].handle);
                        if (p.hasDepth)
                            rtDesc.SetDepth(p.depthAttachment.handle);

                        NkRenderTargetHandle rt = mDevice->GetOrCreateRenderTarget(rtDesc);
                        cmd->BeginRenderTarget(rt, clearFlags, clearColor, clearDepth, 0);
                    }
                }

                cmd->SetViewport({0.f, 0.f, (float32)mWidth, (float32)mHeight});
                cmd->SetScissor ({0, 0, (int32)mWidth, (int32)mHeight});

                // ── Callback utilisateur ────────────────────────────────────────────
                NkRenderGraphCtx ctx{};
                ctx.cmd        = cmd;
                ctx.width      = mWidth;
                ctx.height     = mHeight;
                ctx.frameIndex = mFrameIndex;
                ctx.time       = time;
                ctx.deltaTime  = deltaTime;
                // Lier les maps input/output
                NkHashMap<NkString,NkTextureHandle> inMap, outMap;
                for (auto& att : p.inputs)  inMap[att.name]  = att.handle;
                for (auto& att : p.outputs) outMap[att.name] = att.handle;
                ctx.mInputMap  = &inMap;
                ctx.mOutputMap = &outMap;
                p.callback(ctx);

                // ── Fin de la passe ─────────────────────────────────────────────────
                if (p.isScreen) cmd->EndSwapchainRenderPass();
                else if (!p.outputs.IsEmpty() || p.hasDepth) cmd->EndRenderTarget();

                // ── Barrières de sortie : écriture → lecture ────────────────────────
                for (uint32 j = 0; j < (uint32)p.outputs.Size(); ++j) {
                    if (!p.outputs[j].handle.IsValid()) continue;
                    cmd->TextureBarrier(p.outputs[j].handle,
                                         NkResourceState::NK_RENDER_TARGET,
                                         NkResourceState::NK_SHADER_READ);
                }
            }
        }

        // =============================================================================
        void NkRenderGraph::Reset() {
            // Libérer les textures transitoires
            for (uint32 i = 0; i < (uint32)mTransientTextures.Size(); ++i) {
                if (mTransientTextures[i].IsValid() && mDevice) {
                    mDevice->ReleaseTransientTexture(mTransientTextures[i]);
                }
            }
            mTransientTextures.Clear();
            mPasses.Clear();
            mCompiled = false;
        }

        void NkRenderGraph::Shutdown() {
            Reset();
            mDevice = nullptr;
        }

        NkTextureHandle NkRenderGraph::GetPassOutput(const char* passName,
                                                       const char* attName) const
        {
            for (uint32 i = 0; i < (uint32)mPasses.Size(); ++i) {
                if (mPasses[i].name != passName) continue;
                for (uint32 j = 0; j < (uint32)mPasses[i].outputs.Size(); ++j) {
                    if (mPasses[i].outputs[j].name == attName)
                        return mPasses[i].outputs[j].handle;
                }
            }
            return {};
        }

    } // namespace renderer
} // namespace nkentseu
