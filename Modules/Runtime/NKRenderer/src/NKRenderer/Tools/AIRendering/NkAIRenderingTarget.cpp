// =============================================================================
// NkAIRenderingTarget.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkAIRenderingTarget.h"
#include <cstring>

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // NkAIRenderingTarget
    // =========================================================================

    NkAIRenderingTarget::NkAIRenderingTarget(const NkAITargetDesc& desc)
        : mDesc(desc) {
        // Pre-allocate ring slots
        for (uint32 i = 0; i < desc.ringSize; ++i) {
            RingSlot slot;
            memset(slot.ptrs, 0, sizeof(slot.ptrs));
            mRing.PushBack(slot);
        }
    }

    NkAIRenderingTarget::~NkAIRenderingTarget() {
        WaitAll();
    }

    void NkAIRenderingTarget::Capture(NkICommandBuffer* cmd,
                                        NkTexHandle colorTex,
                                        NkTexHandle depthTex,
                                        NkTexHandle normalTex,
                                        NkTexHandle albedoTex,
                                        NkTexHandle motionTex,
                                        NkTexHandle instanceTex) {
        if (!mEnabled || mRing.Empty()) return;

        RingSlot& slot = mRing[mWriteSlot];
        // If previous capture in this slot not yet consumed: skip frame
        if (slot.pending) return;

        slot.frameIdx  = mCaptureCount;
        slot.timestamp = 0.f; // would use a timer
        slot.pending   = true;

        if (NkAIHas(mDesc.channels, NkAIChannel::NK_AI_COLOR) && colorTex.IsValid())
            IssueCopy(cmd, colorTex, slot, 0);
        if (NkAIHas(mDesc.channels, NkAIChannel::NK_AI_DEPTH) && depthTex.IsValid())
            IssueCopy(cmd, depthTex, slot, 1);
        if (NkAIHas(mDesc.channels, NkAIChannel::NK_AI_NORMAL) && normalTex.IsValid())
            IssueCopy(cmd, normalTex, slot, 2);
        if (NkAIHas(mDesc.channels, NkAIChannel::NK_AI_ALBEDO) && albedoTex.IsValid())
            IssueCopy(cmd, albedoTex, slot, 3);
        if (NkAIHas(mDesc.channels, NkAIChannel::NK_AI_MOTION) && motionTex.IsValid())
            IssueCopy(cmd, motionTex, slot, 4);
        if (NkAIHas(mDesc.channels, NkAIChannel::NK_AI_INSTANCE_ID) && instanceTex.IsValid())
            IssueCopy(cmd, instanceTex, slot, 5);
        if (NkAIHas(mDesc.channels, NkAIChannel::NK_AI_CUSTOM) && mDesc.customTex.IsValid())
            IssueCopy(cmd, mDesc.customTex, slot, 6);

        mWriteSlot = (mWriteSlot + 1) % (uint32)mRing.Size();
        ++mCaptureCount;
    }

    void NkAIRenderingTarget::Flush() {
        for (uint32 i = 0; i < (uint32)mRing.Size(); ++i) {
            RingSlot& slot = mRing[mReadSlot];
            if (!slot.pending) {
                mReadSlot = (mReadSlot + 1) % (uint32)mRing.Size();
                continue;
            }
            // Check if GPU readback is complete (stub: assume ready in sync mode)
            if (mDesc.sync) {
                TriggerCallback(slot);
                slot.pending = false;
            }
            mReadSlot = (mReadSlot + 1) % (uint32)mRing.Size();
        }
    }

    void NkAIRenderingTarget::WaitAll() {
        // In async mode: block until all in-flight readbacks complete
        for (auto& slot : mRing) {
            if (slot.pending) {
                TriggerCallback(slot);
                slot.pending = false;
            }
        }
    }

    void NkAIRenderingTarget::SetCallback(NkAIFrameCallback cb) {
        mDesc.callback = cb;
    }

    void NkAIRenderingTarget::Enable(bool e) {
        mEnabled = e;
    }

    void NkAIRenderingTarget::IssueCopy(NkICommandBuffer* /*cmd*/,
                                         NkTexHandle /*src*/,
                                         RingSlot& /*slot*/,
                                         uint32 /*chanIdx*/) {
        // Issue GPU copy: src texture → slot.stagingBuf
        // Allocate staging buffer if not already done
        // Map the buffer and store pointer in slot.ptrs[chanIdx]
        // (Backend specific — stub)
    }

    void NkAIRenderingTarget::TriggerCallback(const RingSlot& slot) {
        if (!mDesc.callback) return;
        NkAIFrame frame;
        frame.frameIndex    = slot.frameIdx;
        frame.width         = mDesc.width;
        frame.height        = mDesc.height;
        frame.channels      = mDesc.channels;
        frame.format        = mDesc.format;
        frame.timestamp     = slot.timestamp;
        frame.colorData     = slot.ptrs[0];
        frame.depthData     = slot.ptrs[1];
        frame.normalData    = slot.ptrs[2];
        frame.albedoData    = slot.ptrs[3];
        frame.motionData    = slot.ptrs[4];
        frame.instanceIdData= slot.ptrs[5];
        frame.customData    = slot.ptrs[6];
        frame.strideBytes   = mDesc.width * 4; // RGBA8/FP32 — adjust per format
        mDesc.callback(frame);
    }

    // =========================================================================
    // NkAIRenderingSystem
    // =========================================================================

    NkAIRenderingSystem::~NkAIRenderingSystem() {
        Shutdown();
    }

    bool NkAIRenderingSystem::Init(NkIDevice* device) {
        mDevice = device;
        // Check if device supports async readback
        mAsyncSupported = true; // most modern GPUs do
        mReady = true;
        return true;
    }

    void NkAIRenderingSystem::Shutdown() {
        if (!mReady) return;
        for (auto* t : mTargets) delete t;
        mTargets.Clear();
        mReady = false;
    }

    NkAIRenderingTarget* NkAIRenderingSystem::Create(const NkAITargetDesc& desc) {
        NkAIRenderingTarget* t = new NkAIRenderingTarget(desc);
        mTargets.PushBack(t);
        return t;
    }

    void NkAIRenderingSystem::Destroy(NkAIRenderingTarget*& target) {
        if (!target) return;
        for (uint32 i = 0; i < (uint32)mTargets.Size(); ++i) {
            if (mTargets[i] == target) {
                mTargets.Erase(mTargets.Begin() + i);
                break;
            }
        }
        delete target;
        target = nullptr;
    }

    void NkAIRenderingSystem::FlushReadbacks() {
        for (auto* t : mTargets)
            t->Flush();
    }

} // namespace renderer
} // namespace nkentseu
