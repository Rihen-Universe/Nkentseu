// =============================================================================
// NkRaytracingSystem.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkRaytracingSystem.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include "NKRenderer/Mesh/NkMeshSystem.h"

namespace nkentseu {
namespace renderer {

    NkRaytracingSystem::~NkRaytracingSystem() {
        Shutdown();
    }

    bool NkRaytracingSystem::Init(NkIDevice* device,
                                   NkTextureLibrary* texLib,
                                   NkMeshSystem* meshSys,
                                   const NkRaytracingConfig& cfg) {
        mDevice    = device;
        mTexLib    = texLib;
        mMesh      = meshSys;
        mCfg       = cfg;
        mSupported = CheckDeviceSupport();

        if (mSupported) {
            CreateRTPipelines();
        }
        mReady = true;
        return true; // success even if RT not supported (fallback mode)
    }

    void NkRaytracingSystem::Shutdown() {
        if (!mReady) return;
        if (mSupported) {
            DestroyRTPipelines();
        }
        mBLASCache.Clear();
        mInstances.Clear();
        mReady = false;
    }

    NkBLASHandle NkRaytracingSystem::BuildBLAS(NkMeshHandle mesh, bool /*allowUpdate*/) {
        // Check cache first
        auto* cached = mBLASCache.Find(mesh.id);
        if (cached) return *cached;

        if (!mSupported) return NkBLASHandle::Null();

        // BLAS construction is backend-specific — stub with unique id
        static uint64 sNextId = 1;
        NkBLASHandle blas;
        blas.id = sNextId++;
        mBLASCache.Insert(mesh.id, blas);
        return blas;
    }

    void NkRaytracingSystem::UpdateBLAS(NkBLASHandle /*blas*/, NkMeshHandle /*mesh*/,
                                         NkICommandBuffer* /*cmd*/) {
        // Stub: refit BLAS for skinned/deformed geometry
    }

    void NkRaytracingSystem::DestroyBLAS(NkBLASHandle& blas) {
        // Remove from cache (reverse lookup by value)
        blas = NkBLASHandle::Null();
    }

    void NkRaytracingSystem::BeginFrame(const NkCamera3DData& cam,
                                         const NkMat4f& viewProj) {
        mCamPos   = cam.position;
        mView     = viewProj; // store for dispatch
        mInstances.Clear();
    }

    void NkRaytracingSystem::SetInstances(const NkRtInstance* instances, uint32 count) {
        mInstances.Clear();
        for (uint32 i = 0; i < count; ++i)
            mInstances.PushBack(instances[i]);
    }

    void NkRaytracingSystem::BuildTLAS(NkICommandBuffer* /*cmd*/) {
        if (!mSupported) return;
        // Stub: build top-level AS from mInstances
    }

    void NkRaytracingSystem::DispatchShadows(NkICommandBuffer* /*cmd*/,
                                               NkTexHandle /*gbufDepth*/,
                                               NkTexHandle /*outShadow*/) {
        if (!mSupported || !NkRtHas(mCfg.enabledEffects, NkRtEffect::NK_RT_SHADOWS)) return;
        // Dispatch shadow rays — backend specific
    }

    void NkRaytracingSystem::DispatchReflections(NkICommandBuffer* /*cmd*/,
                                                   NkTexHandle /*gbufNormal*/,
                                                   NkTexHandle /*gbufAlbedo*/,
                                                   NkTexHandle /*gbufDepth*/,
                                                   NkTexHandle /*outReflect*/) {
        if (!mSupported || !NkRtHas(mCfg.enabledEffects, NkRtEffect::NK_RT_REFLECTIONS)) return;
    }

    void NkRaytracingSystem::DispatchAO(NkICommandBuffer* /*cmd*/,
                                         NkTexHandle /*gbufNormal*/,
                                         NkTexHandle /*gbufDepth*/,
                                         NkTexHandle /*outAO*/) {
        if (!mSupported || !NkRtHas(mCfg.enabledEffects, NkRtEffect::NK_RT_AO)) return;
    }

    void NkRaytracingSystem::DispatchGI(NkICommandBuffer* /*cmd*/,
                                         NkTexHandle /*gbufNormal*/,
                                         NkTexHandle /*gbufAlbedo*/,
                                         NkTexHandle /*gbufDepth*/,
                                         NkTexHandle /*outGI*/) {
        if (!mSupported || !NkRtHas(mCfg.enabledEffects, NkRtEffect::NK_RT_GI)) return;
    }

    void NkRaytracingSystem::SetConfig(const NkRaytracingConfig& cfg) {
        mCfg = cfg;
    }

    void NkRaytracingSystem::SetEffect(NkRtEffect effect, bool enabled) {
        uint32 mask = (uint32)mCfg.enabledEffects;
        if (enabled) mask |=  (uint32)effect;
        else         mask &= ~(uint32)effect;
        mCfg.enabledEffects = (NkRtEffect)mask;
    }

    bool NkRaytracingSystem::EffectActive(NkRtEffect e) const {
        return mSupported && NkRtHas(mCfg.enabledEffects, e);
    }

    bool NkRaytracingSystem::CheckDeviceSupport() {
        if (!mDevice) return false;
        // Query device capability — requires backend-specific API
        // For now: return false (conservative — avoids hard crashes)
        return false;
    }

    void NkRaytracingSystem::CreateRTPipelines() {
        // Create ray gen / closest hit / miss shader pipelines
        // Backend specific — stub
    }

    void NkRaytracingSystem::DestroyRTPipelines() {
        // Destroy RT pipelines — stub
    }

} // namespace renderer
} // namespace nkentseu
