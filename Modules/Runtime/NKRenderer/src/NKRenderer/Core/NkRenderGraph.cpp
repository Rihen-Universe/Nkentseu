// =============================================================================
// NkRenderGraph.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkRenderGraph.h"
#include <cstdio>

namespace nkentseu {
namespace renderer {

    NkRenderGraph::NkRenderGraph(NkIDevice* device) : mDevice(device) {}

    NkRenderGraph::~NkRenderGraph() { Reset(); }

    // ── Ressources ────────────────────────────────────────────────────────────
    NkGraphResId NkRenderGraph::ImportTexture(const NkString& name,
                                               NkTextureHandle tex,
                                               NkResourceState initialState) {
        GraphResource r;
        r.name         = name;
        r.texture      = tex;
        r.currentState = initialState;
        r.initialState = initialState;
        r.isImported   = true;
        NkGraphResId id = NextResId();
        mResources.PushBack(r);
        mResByName.Insert(name, id);
        return id;
    }

    NkGraphResId NkRenderGraph::ImportBuffer(const NkString& name,
                                              NkBufferHandle  buf,
                                              NkResourceState initialState) {
        GraphResource r;
        r.name         = name;
        r.buffer       = buf;
        r.currentState = initialState;
        r.initialState = initialState;
        r.isImported   = true;
        NkGraphResId id = NextResId();
        mResources.PushBack(r);
        mResByName.Insert(name, id);
        return id;
    }

    NkGraphResId NkRenderGraph::CreateTransient(const NkString& name,
                                                  const NkTextureDesc& desc) {
        GraphResource r;
        r.name          = name;
        r.isTransient   = true;
        r.transientDesc = desc;
        r.currentState  = NkResourceState::NK_UNDEFINED;
        NkGraphResId id = NextResId();
        mResources.PushBack(r);
        mResByName.Insert(name, id);
        return id;
    }

    // ── Passes ────────────────────────────────────────────────────────────────
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

    // ── Compilation ───────────────────────────────────────────────────────────
    bool NkRenderGraph::Compile() {
        // 1. Allouer les transient resources
        for (auto& res : mResources) {
            if (res.isTransient && !res.texture.IsValid()) {
                res.texture = mDevice->CreateTexture(res.transientDesc);
                if (!res.texture.IsValid()) return false;
            }
        }

        // 2. Tri topologique simple (préserve ordre déclaration si pas de cycle)
        TopoSort();

        mCompiled = true;
        return true;
    }

    void NkRenderGraph::TopoSort() {
        // Pour l'instant : ordre de déclaration
        // TODO: Gribb-Hartmann topological sort basé sur read/write deps
        mSorted.Clear();
        for (auto& p : mPasses) {
            if (p.enabled) mSorted.PushBack(&p);
        }
    }

    // ── Exécution ─────────────────────────────────────────────────────────────
    void NkRenderGraph::Execute(NkICommandBuffer* cmd) {
        if (!mCompiled) Compile();

        for (NkPassBuilder* pass : mSorted) {
            if (!pass->enabled || !pass->execute) continue;

            // Barrières d'entrée
            InsertBarriers(cmd, pass);

            // Begin render pass si besoin
            bool needsRenderPass = (pass->type != NkPassType::NK_COMPUTE);
            if (needsRenderPass) {
                NkRenderPassBeginDesc rpDesc;
                rpDesc.clearColor      = pass->clearColor;
                rpDesc.clearColorValue = {pass->clearColorValue.x,
                                           pass->clearColorValue.y,
                                           pass->clearColorValue.z,
                                           pass->clearColorAlpha};
                rpDesc.clearDepth      = pass->clearDepth;
                rpDesc.clearDepthValue = pass->clearDepthValue;
                cmd->BeginRenderPass(rpDesc);
            }

            // Exécuter la callback
            pass->execute(cmd);

            if (needsRenderPass) cmd->EndRenderPass();
        }
    }

    void NkRenderGraph::InsertBarriers(NkICommandBuffer* cmd, NkPassBuilder* pass) {
        // Transitions pour les ressources lues
        for (NkGraphResId resId : pass->reads) {
            uint32 idx = resId - 1;
            if (idx >= (uint32)mResources.Size()) continue;
            auto& res = mResources[idx];
            NkResourceState needed = NkResourceState::NK_SHADER_RESOURCE;
            if (res.currentState != needed) {
                if (res.texture.IsValid())
                    cmd->TransitionTexture(res.texture, res.currentState, needed);
                res.currentState = needed;
            }
        }
        // Transitions pour les ressources écrites
        for (NkGraphResId resId : pass->writes) {
            uint32 idx = resId - 1;
            if (idx >= (uint32)mResources.Size()) continue;
            auto& res = mResources[idx];
            NkResourceState needed = (pass->type == NkPassType::NK_SHADOW)
                ? NkResourceState::NK_DEPTH_WRITE
                : NkResourceState::NK_RENDER_TARGET;
            if (res.currentState != needed) {
                if (res.texture.IsValid())
                    cmd->TransitionTexture(res.texture, res.currentState, needed);
                res.currentState = needed;
            }
        }
    }

    // ── Reset ─────────────────────────────────────────────────────────────────
    void NkRenderGraph::Reset() {
        // Libérer les transient resources
        for (auto& res : mResources) {
            if (res.isTransient && res.texture.IsValid()) {
                mDevice->DestroyTexture(res.texture);
                res.texture = NkTextureHandle{};
            }
        }
        mPasses.Clear();
        mSorted.Clear();
        mResources.Clear();
        mResByName.Clear();
        mNextResId = 1;
        mCompiled = false;
    }

    // ── Debug ─────────────────────────────────────────────────────────────────
    NkString NkRenderGraph::DumpDOT() const {
        NkString out = "digraph NkRenderGraph {\n  rankdir=LR;\n";
        for (auto* p : mSorted) {
            out += "  \"" + p->name + "\" [shape=box];\n";
            for (NkGraphResId r : p->reads) {
                uint32 idx = r - 1;
                if (idx < (uint32)mResources.Size())
                    out += "  \"" + mResources[idx].name + "\" -> \"" + p->name + "\";\n";
            }
            for (NkGraphResId w : p->writes) {
                uint32 idx = w - 1;
                if (idx < (uint32)mResources.Size())
                    out += "  \"" + p->name + "\" -> \"" + mResources[idx].name + "\";\n";
            }
        }
        out += "}\n";
        return out;
    }

    NkString NkRenderGraph::DumpTimings() const {
        NkString out = "=== RenderGraph Timings ===\n";
        for (auto* p : mSorted) {
            char buf[128];
            snprintf(buf, sizeof(buf), "  %-24s : %.3f ms\n",
                     p->name.CStr(), p->gpuTimeMs);
            out += buf;
        }
        return out;
    }

} // namespace renderer
} // namespace nkentseu
