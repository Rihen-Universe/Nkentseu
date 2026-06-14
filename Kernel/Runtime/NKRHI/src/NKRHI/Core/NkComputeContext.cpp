// =============================================================================
// NkComputeContext.cpp
// =============================================================================
#include "NKRHI/Core/NkComputeContext.h"
#include "NKLogger/NkLog.h"

namespace nkentseu {

    // =========================================================================
    // Init / Shutdown
    // =========================================================================

    bool NkComputeContext::Init(NkIDevice* device) {
        if (!device) return false;
        mDevice = device;

        // Vérifie que le device supporte le compute
        const NkDeviceCaps& caps = device->GetCaps();
        if (!caps.computeShaders) {
            logger_src.Warnf("[NkComputeContext] Device ne supporte pas les compute shaders\n");
            return false;
        }

        mReady = true;
        logger_src.Infof("[NkComputeContext] Init OK | async=%s | maxGroupX=%u | sharedMem=%uKB\n",
                         SupportsAsyncCompute() ? "oui" : "non",
                         caps.maxComputeGroupSizeX,
                         caps.maxComputeSharedMemory / 1024);
        return true;
    }

    void NkComputeContext::Shutdown() {
        if (!mReady) return;

        // Détruit tous les pipelines en cache
        ClearPipelineCache();

        // Libère le descriptor set de travail
        if (mWorkingSet.IsValid()) {
            mDevice->FreeDescriptorSet(mWorkingSet);
            mWorkingSet = {};
        }
        if (mWorkingLayout.IsValid()) {
            mDevice->DestroyDescriptorSetLayout(mWorkingLayout);
            mWorkingLayout = {};
        }

        mDevice  = nullptr;
        mCmd     = nullptr;
        mReady   = false;
    }

    // =========================================================================
    // Passe compute
    // =========================================================================

    void NkComputeContext::BeginPass(NkICommandBuffer* cmd, const NkComputePassDesc& desc) {
        NKENTSEU_ASSERT(!mInPass);  // Pas de passes imbriquées
        mCmd     = cmd;
        mInPass  = true;
        mCurrentStats = {};
        mTimestampEnabled = desc.enableTimestamp;

        cmd->BeginComputePass(desc);
    }

    void NkComputeContext::EndPass() {
        if (!mInPass) return;

        // Flush les bindings restants si l'utilisateur a bindé sans dispatcher
        if (mBindingsDirty) FlushBindings();

        if (mTimestampEnabled) {
            mCmd->WriteTimestamp(mTimestampEndIdx);
        }

        mCmd->EndComputePass();
        mLastStats = mCurrentStats;
        mInPass    = false;
        mCmd       = nullptr;

        // Remet le pipeline à zéro pour la prochaine passe
        mBoundPipeline  = {};
        mPipelineBound  = false;
        mBindingsDirty  = false;
        mPendingBindings.Clear();
    }

    // =========================================================================
    // Cache de pipelines
    // =========================================================================

    NkPipelineHandle NkComputeContext::GetOrCreatePipeline(const char*    key,
                                                            NkShaderHandle shader,
                                                            const char*    debugName) {
        NkString k(key);
        auto* entry = mPipelineCache.Find(k);
        if (entry != nullptr) {
            return entry->pipeline;
        }

        NkComputePipelineDesc desc;
        desc.shader    = shader;
        desc.debugName = debugName ? debugName : key;

        NkPipelineHandle pipe = mDevice->CreateComputePipeline(desc);
        if (!pipe.IsValid()) {
            logger_src.Errorf("[NkComputeContext] Echec creation pipeline compute '%s'\n", key);
            return {};
        }

        NkComputePipelineCacheEntry e;
        e.shader   = shader;
        e.pipeline = pipe;
        mPipelineCache.Insert(k, e);
        logger_src.Infof("[NkComputeContext] Pipeline compute compile et cache: '%s'\n", key);
        return pipe;
    }

    NkPipelineHandle NkComputeContext::GetOrCompileGLSL(const char* key,
                                                          const char* glslSrc,
                                                          const char* debugName) {
        NkString k(key);
        auto* entry = mPipelineCache.Find(k);
        if (entry != nullptr) {
            return entry->pipeline;
        }

        // Compile le shader compute depuis GLSL
        NkShaderDesc shaderDesc;
        shaderDesc.debugName = debugName ? debugName : key;
        shaderDesc.AddGLSL(NkShaderStage::NK_COMPUTE, glslSrc);

        NkShaderHandle shader = mDevice->CreateShader(shaderDesc);
        if (!shader.IsValid()) {
            logger_src.Errorf("[NkComputeContext] Echec compilation GLSL compute '%s'\n", key);
            return {};
        }

        return GetOrCreatePipeline(key, shader, debugName);
    }

    void NkComputeContext::EvictPipeline(const char* key) {
        NkString k(key);
        auto* entry = mPipelineCache.Find(k);
        if (entry == nullptr) return;

        if (entry->pipeline.IsValid())
            mDevice->DestroyPipeline(entry->pipeline);
        if (entry->shader.IsValid())
            mDevice->DestroyShader(entry->shader);

        mPipelineCache.Erase(k);
    }

    void NkComputeContext::ClearPipelineCache() {
        for (auto& pair : mPipelineCache) {
            if (pair.Second.pipeline.IsValid()) mDevice->DestroyPipeline(pair.Second.pipeline);
            if (pair.Second.shader.IsValid())   mDevice->DestroyShader(pair.Second.shader);
        }
        mPipelineCache.Clear();
    }

    // =========================================================================
    // Pipeline binding
    // =========================================================================

    void NkComputeContext::SetPipeline(NkPipelineHandle pipeline) {
        if (!mInPass || !mCmd) return;
        if (mBoundPipeline == pipeline && mPipelineBound) return;  // déjà lié
        mCmd->BindComputePipeline(pipeline);
        mBoundPipeline = pipeline;
        mPipelineBound = true;
    }

    // =========================================================================
    // Descriptor bindings (lazy flush)
    // =========================================================================

    void NkComputeContext::BindBuffer(uint32 binding, NkBufferHandle buf,
                                       uint64 offset, uint64 range) {
        NkBoundResource r;
        r.binding   = binding;
        r.type      = NkDescriptorType::NK_STORAGE_BUFFER;
        r.buffer    = buf;
        r.bufOffset = offset;
        r.bufRange  = range;
        mPendingBindings.PushBack(r);
        mBindingsDirty    = true;
        mWorkingSetValid  = false;
    }

    void NkComputeContext::BindUniformBuffer(uint32 binding, NkBufferHandle buf,
                                              uint64 offset, uint64 range) {
        NkBoundResource r;
        r.binding   = binding;
        r.type      = NkDescriptorType::NK_UNIFORM_BUFFER;
        r.buffer    = buf;
        r.bufOffset = offset;
        r.bufRange  = range;
        mPendingBindings.PushBack(r);
        mBindingsDirty   = true;
        mWorkingSetValid = false;
    }

    void NkComputeContext::BindStorageTexture(uint32 binding, NkTextureHandle tex) {
        NkBoundResource r;
        r.binding = binding;
        r.type    = NkDescriptorType::NK_STORAGE_TEXTURE;
        r.texture = tex;
        mPendingBindings.PushBack(r);
        mBindingsDirty   = true;
        mWorkingSetValid = false;
    }

    void NkComputeContext::BindSampledTexture(uint32 binding, NkTextureHandle tex,
                                               NkSamplerHandle sampler) {
        NkBoundResource r;
        r.binding = binding;
        r.type    = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
        r.texture = tex;
        r.sampler = sampler;
        mPendingBindings.PushBack(r);
        mBindingsDirty   = true;
        mWorkingSetValid = false;
    }

    void NkComputeContext::BindDescriptorSet(NkDescSetHandle set, uint32 setIndex,
                                              uint32* dynamicOffsets,
                                              uint32  dynamicOffsetCount) {
        if (!mCmd) return;
        // Bypass le mécanisme lazy : lie directement le set fourni par l'utilisateur
        mPendingBindings.Clear();
        mBindingsDirty   = false;
        mWorkingSetValid = false;
        mCmd->BindDescriptorSet(set, setIndex, dynamicOffsets, dynamicOffsetCount);
    }

    void NkComputeContext::ClearBindings() {
        mPendingBindings.Clear();
        mBindingsDirty = false;
    }

    // =========================================================================
    // Push constants
    // =========================================================================

    void NkComputeContext::PushConstantsRaw(const void* data, uint32 size, uint32 offset) {
        if (!mCmd) return;
        mCmd->PushConstants(NkShaderStage::NK_COMPUTE, offset, size, data);
    }

    // =========================================================================
    // Dispatch (tous flushent les bindings)
    // =========================================================================

    void NkComputeContext::Dispatch(uint32 groupsX, uint32 groupsY, uint32 groupsZ) {
        if (!mCmd || !mInPass) return;
        FlushBindings();
        mCmd->Dispatch(groupsX, groupsY, groupsZ);
        TrackDispatch(groupsX, groupsY, groupsZ);
    }

    void NkComputeContext::Dispatch1D(uint32 count, uint32 groupSize) {
        Dispatch(NkComputeGroups(count, groupSize), 1, 1);
    }

    void NkComputeContext::Dispatch2D(uint32 width, uint32 height,
                                       uint32 tileX, uint32 tileY) {
        Dispatch(NkComputeGroups(width, tileX),
                 NkComputeGroups(height, tileY),
                 1);
    }

    void NkComputeContext::Dispatch3D(uint32 width, uint32 height, uint32 depth,
                                       uint32 tileX, uint32 tileY, uint32 tileZ) {
        Dispatch(NkComputeGroups(width, tileX),
                 NkComputeGroups(height, tileY),
                 NkComputeGroups(depth, tileZ));
    }

    void NkComputeContext::DispatchIndirect(NkBufferHandle argsBuffer, uint64 offset) {
        if (!mCmd || !mInPass) return;
        FlushBindings();
        mCmd->DispatchIndirect(argsBuffer, offset);
        ++mCurrentStats.dispatchCount;
    }

    // =========================================================================
    // Barrières
    // =========================================================================

    void NkComputeContext::UAVBarrier(NkBufferHandle buf) {
        if (!mCmd) return;
        mCmd->UAVBarrier(buf);
    }

    void NkComputeContext::UAVBarrier(NkTextureHandle tex) {
        if (!mCmd) return;
        NkTextureBarrier b{tex,
                           NkResourceState::NK_UNORDERED_ACCESS,
                           NkResourceState::NK_UNORDERED_ACCESS,
                           NkPipelineStage::NK_COMPUTE_SHADER,
                           NkPipelineStage::NK_COMPUTE_SHADER};
        mCmd->Barrier(nullptr, 0, &b, 1);
    }

    void NkComputeContext::TransitionForCompute(NkTextureHandle tex,
                                                  NkResourceState before) {
        if (!mCmd) return;
        mCmd->TextureBarrier(tex, before,
                              NkResourceState::NK_UNORDERED_ACCESS,
                              NkPipelineStage::NK_ALL_COMMANDS,
                              NkPipelineStage::NK_COMPUTE_SHADER);
    }

    void NkComputeContext::TransitionForGraphics(NkTextureHandle tex,
                                                   NkResourceState after) {
        if (!mCmd) return;
        mCmd->TextureBarrier(tex,
                              NkResourceState::NK_UNORDERED_ACCESS,
                              after,
                              NkPipelineStage::NK_COMPUTE_SHADER,
                              NkPipelineStage::NK_ALL_COMMANDS);
    }

    void NkComputeContext::FullBarrier() {
        if (!mCmd) return;
        mCmd->Barrier(nullptr, 0, nullptr, 0);  // backends émettront une barrière mémoire totale
    }

    // =========================================================================
    // Soumission asynchrone
    // =========================================================================

    NkFenceHandle NkComputeContext::SubmitAsync(NkICommandBuffer*  cmd,
                                                  NkSemaphoreHandle  waitSemaphore,
                                                  NkSemaphoreHandle  signalSemaphore) {
        if (!mDevice || !cmd) return {};

        NkFenceHandle fence = mDevice->CreateFence(false);

        // NkSubmitInfo uses raw pointer arrays — build local arrays on the stack.
        NkICommandBuffer* cbs[] = { cmd };
        static const NkPipelineStage kComputeStage = NkPipelineStage::NK_COMPUTE_SHADER;

        NkSubmitInfo submit{};
        submit.commandBuffers       = cbs;
        submit.commandBufferCount   = 1;
        submit.fence                = fence;

        if (waitSemaphore.IsValid()) {
            submit.waitSemaphores      = &waitSemaphore;
            submit.waitSemaphoreCount  = 1;
            submit.waitStages          = &kComputeStage;
        }
        if (signalSemaphore.IsValid()) {
            submit.signalSemaphores      = &signalSemaphore;
            submit.signalSemaphoreCount  = 1;
        }

        // Soumet sur la queue compute dédiée si disponible, sinon queue principale
        if (SupportsAsyncCompute()) {
            mDevice->SubmitOnQueue(NkQueueType::NK_COMPUTE, submit);
        } else {
            mDevice->Submit(submit.commandBuffers, submit.commandBufferCount, submit.fence);
        }

        return fence;
    }

    void NkComputeContext::WaitForFence(NkFenceHandle fence, uint64 timeoutNanos) {
        if (!mDevice || !fence.IsValid()) return;
        mDevice->WaitFence(fence, timeoutNanos);
        mDevice->DestroyFence(fence);
    }

    void NkComputeContext::SyncComputeToGraphics(NkSemaphoreHandle computeSignal,
                                                   NkICommandBuffer* graphicsCmd) {
        // La synchronisation queue-to-queue se fait au Submit via les semaphores.
        // Cette méthode insère la dépendance côté command buffer graphics.
        // Sur OpenGL (single queue), c'est un no-op car les queues sont la même.
        if (!graphicsCmd) return;
        // Le graphics cmd doit attendre le semaphore compute au pipeline stage COMPUTE_SHADER
        // Cela est géré au niveau Submit (NkSubmitInfo::waitSemaphores), pas ici.
        // On laisse cette méthode comme point d'extension pour les backends multi-queue.
        (void)computeSignal;
    }

    // =========================================================================
    // Capacités
    // =========================================================================

    bool NkComputeContext::SupportsAsyncCompute() const {
        if (!mDevice) return false;
        const auto& caps = mDevice->GetCaps();
        // Async compute = queue dédiée sur Vulkan et DX12
        // On détecte via un flag dans les caps ou via l'API du device
        return caps.computeShaders && mDevice->HasDedicatedComputeQueue();
    }

    bool NkComputeContext::SupportsIndirectDispatch() const {
        if (!mDevice) return false;
        return mDevice->GetCaps().indirectDispatch;
    }

    uint32 NkComputeContext::MaxGroupSizeX() const {
        return mDevice ? mDevice->GetCaps().maxComputeGroupSizeX : 0;
    }

    uint32 NkComputeContext::MaxGroupSizeY() const {
        return mDevice ? mDevice->GetCaps().maxComputeGroupSizeY : 0;
    }

    uint32 NkComputeContext::MaxGroupSizeZ() const {
        return mDevice ? mDevice->GetCaps().maxComputeGroupSizeZ : 0;
    }

    uint32 NkComputeContext::MaxSharedMemoryBytes() const {
        return mDevice ? mDevice->GetCaps().maxComputeSharedMemory : 0;
    }

    uint32 NkComputeContext::OptimalGroupSize1D() const {
        if (!mDevice) return 256;
        // Warp = 32 (NVIDIA), Wavefront = 64 (AMD), SIMD = 32 (Intel, Apple)
        // 256 = 8 warps = bon compromis général
        // Sur Metal Apple Silicon, 128 est souvent optimal
        const uint32 maxX = mDevice->GetCaps().maxComputeGroupSizeX;
        return (maxX >= 256) ? 256 : (maxX >= 128 ? 128 : maxX);
    }

    // =========================================================================
    // Privé : flush des bindings lazy
    // =========================================================================

    void NkComputeContext::FlushBindings() {
        if (!mBindingsDirty || mPendingBindings.IsEmpty()) {
            mBindingsDirty = false;
            return;
        }

        // Si le working set n'est pas encore alloué ou si le layout a changé,
        // on recréé le layout + set depuis les bindings courants.
        if (!mWorkingSetValid) {
            // Construire le descriptor set layout depuis les bindings en attente
            NkDescriptorSetLayoutDesc layoutDesc;
            for (uint32 i = 0; i < (uint32)mPendingBindings.Size(); ++i) {
                const NkBoundResource& r = mPendingBindings[i];
                layoutDesc.Add(r.binding, r.type, NkShaderStage::NK_COMPUTE);
            }

            // Recréer le layout si nécessaire
            if (mWorkingLayout.IsValid())
                mDevice->DestroyDescriptorSetLayout(mWorkingLayout);
            mWorkingLayout = mDevice->CreateDescriptorSetLayout(layoutDesc);

            // Recréer le set
            if (mWorkingSet.IsValid())
                mDevice->FreeDescriptorSet(mWorkingSet);
            mWorkingSet = mDevice->AllocateDescriptorSet(mWorkingLayout);
            mWorkingSetValid = true;
        }

        // Écrire les descriptors dans le set
        NkVector<NkDescriptorWrite> writes;
        for (uint32 i = 0; i < (uint32)mPendingBindings.Size(); ++i) {
            const NkBoundResource& r = mPendingBindings[i];
            NkDescriptorWrite w;
            w.set       = mWorkingSet;
            w.binding   = r.binding;
            w.arrayElem = 0;
            w.type      = r.type;

            if (r.buffer.IsValid()) {
                w.buffer       = r.buffer;
                w.bufferOffset = r.bufOffset;
                w.bufferRange  = r.bufRange > 0 ? r.bufRange : UINT64_MAX;
            }
            if (r.texture.IsValid()) {
                w.texture = r.texture;
                w.sampler = r.sampler;
            }
            writes.PushBack(w);
        }

        if (!writes.IsEmpty())
            mDevice->UpdateDescriptorSets(writes.Data(), (uint32)writes.Size());

        // Bind le set au command buffer
        mCmd->BindDescriptorSet(mWorkingSet, 0);

        mPendingBindings.Clear();
        mBindingsDirty = false;
    }

    void NkComputeContext::TrackDispatch(uint32 gx, uint32 gy, uint32 gz) {
        ++mCurrentStats.dispatchCount;
        mCurrentStats.totalGroupsX += gx;
        mCurrentStats.totalGroupsY += gy;
        mCurrentStats.totalGroupsZ += gz;
    }

} // namespace nkentseu
