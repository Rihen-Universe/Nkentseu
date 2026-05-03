// =============================================================================
// NkDeferredPass.cpp  — NKRenderer v4.0
// =============================================================================
#include "NkDeferredPass.h"
#include "NKRenderer/Core/NkTextureLibrary.h"
#include <cstring>

namespace nkentseu {
namespace renderer {

    NkDeferredPass::~NkDeferredPass() {
        Shutdown();
    }

    bool NkDeferredPass::Init(NkIDevice* device, NkRenderGraph* graph,
                               NkTextureLibrary* texLib,
                               uint32 width, uint32 height,
                               const NkDeferredConfig& cfg) {
        mDevice  = device;
        mGraph   = graph;
        mTexLib  = texLib;
        mWidth   = width;
        mHeight  = height;
        mCfg     = cfg;

        if (!CreateGBufferTextures()) return false;

        // Créer les buffers GPU pour la light-grid
        uint32 tileX = (width  + cfg.tileSize - 1) / cfg.tileSize;
        uint32 tileY = (height + cfg.tileSize - 1) / cfg.tileSize;
        uint32 gridCells = tileX * tileY;

        NkBufferDesc lightData;
        lightData.sizeBytes = sizeof(NkLightDesc) * cfg.maxLights;
        lightData.type  = NkBufferType::NK_STORAGE;
        lightData.usage = NkResourceUsage::NK_UPLOAD;
        mLightDataBuf = mDevice->CreateBuffer(lightData);

        NkBufferDesc gridDesc;
        gridDesc.sizeBytes = gridCells * cfg.maxLights * sizeof(uint32); // index list
        gridDesc.type  = NkBufferType::NK_STORAGE;
        gridDesc.usage = NkResourceUsage::NK_DEFAULT;
        mLightGridBuf = mDevice->CreateBuffer(gridDesc);

        NkBufferDesc listDesc;
        listDesc.sizeBytes = cfg.maxLights * sizeof(uint32);
        listDesc.type  = NkBufferType::NK_STORAGE;
        listDesc.usage = NkResourceUsage::NK_DEFAULT;
        mLightListBuf = mDevice->CreateBuffer(listDesc);

        mReady = true;
        return true;
    }

    void NkDeferredPass::Shutdown() {
        if (!mReady) return;
        DestroyGBufferTextures();
        if (mLightDataBuf.IsValid()) mDevice->DestroyBuffer(mLightDataBuf);
        if (mLightGridBuf.IsValid()) mDevice->DestroyBuffer(mLightGridBuf);
        if (mLightListBuf.IsValid()) mDevice->DestroyBuffer(mLightListBuf);
        mReady = false;
    }

    void NkDeferredPass::RegisterToRenderGraph() {
        if (!mReady || !mGraph) return;

        mResAlbedo   = mGraph->ImportTexture("GBuf_Albedo",   mTexLib->GetRHIHandle(mGBuffer.albedoMetallic),  NkResourceState::NK_RENDER_TARGET);
        mResNormal   = mGraph->ImportTexture("GBuf_Normal",   mTexLib->GetRHIHandle(mGBuffer.normalRoughness), NkResourceState::NK_RENDER_TARGET);
        mResEmissive = mGraph->ImportTexture("GBuf_Emissive", mTexLib->GetRHIHandle(mGBuffer.emissiveAO),      NkResourceState::NK_RENDER_TARGET);
        if (mCfg.outputVelocity)
            mResVelocity = mGraph->ImportTexture("GBuf_Velocity", mTexLib->GetRHIHandle(mGBuffer.velocity),   NkResourceState::NK_RENDER_TARGET);
        mResDepth    = mGraph->ImportTexture("GBuf_Depth",    mTexLib->GetRHIHandle(mGBuffer.depth),          NkResourceState::NK_DEPTH_WRITE);
        mResAccum    = mGraph->ImportTexture("LightAccum",    mTexLib->GetRHIHandle(mGBuffer.lightAccum),      NkResourceState::NK_RENDER_TARGET);

        // Passe géométrie
        NkPassBuilder& geom = mGraph->AddPass("Deferred_Geometry", NkPassType::NK_GEOMETRY);
        geom.Writes(mResAlbedo, mResNormal, mResEmissive)
            .Writes(mResDepth)
            .ClearWith({0.f,0.f,0.f});

        // Passe éclairage (compute tiled)
        NkPassBuilder& light = mGraph->AddComputePass("Deferred_Lighting");
        light.Reads(mResAlbedo, mResNormal)
             .Reads(mResEmissive)
             .Reads(mResDepth)
             .Writes(mResAccum);
    }

    void NkDeferredPass::SetCamera(const NkCamera3DData& cam, const NkMat4f& viewProj) {
        mCamPos   = cam.position;
        mViewProj = viewProj;
    }

    void NkDeferredPass::BeginGeometry(NkICommandBuffer* cmd) {
        if (!mReady) return;
        // Transition G-buffer attachments to render target
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.albedoMetallic),
                                NkResourceState::NK_SHADER_READ,
                                NkResourceState::NK_RENDER_TARGET);
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.normalRoughness),
                                NkResourceState::NK_SHADER_READ,
                                NkResourceState::NK_RENDER_TARGET);
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.emissiveAO),
                                NkResourceState::NK_SHADER_READ,
                                NkResourceState::NK_RENDER_TARGET);
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.depth),
                                NkResourceState::NK_SHADER_READ,
                                NkResourceState::NK_DEPTH_WRITE);
        cmd->SetViewport(NkViewport(0.f, 0.f, (float32)mWidth, (float32)mHeight));
        cmd->SetScissor(NkRect2D(0, 0, (int32)mWidth, (int32)mHeight));
    }

    void NkDeferredPass::EndGeometry(NkICommandBuffer* cmd) {
        if (!mReady) return;
        // Transition G-buffer to shader read for lighting pass
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.albedoMetallic),
                                NkResourceState::NK_RENDER_TARGET,
                                NkResourceState::NK_SHADER_READ);
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.normalRoughness),
                                NkResourceState::NK_RENDER_TARGET,
                                NkResourceState::NK_SHADER_READ);
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.emissiveAO),
                                NkResourceState::NK_RENDER_TARGET,
                                NkResourceState::NK_SHADER_READ);
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.depth),
                                NkResourceState::NK_DEPTH_WRITE,
                                NkResourceState::NK_SHADER_READ);
    }

    void NkDeferredPass::SubmitLight(const NkLightDesc& light) {
        if (mLights.Size() < mCfg.maxLights)
            mLights.PushBack(light);
    }

    void NkDeferredPass::SubmitLights(const NkLightDesc* lights, uint32 count) {
        for (uint32 i = 0; i < count && mLights.Size() < mCfg.maxLights; ++i)
            mLights.PushBack(lights[i]);
    }

    void NkDeferredPass::ClearLights() {
        mLights.Clear();
    }

    void NkDeferredPass::BeginLighting(NkICommandBuffer* cmd) {
        if (!mReady) return;
        BuildLightGrid(cmd);
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.lightAccum),
                                NkResourceState::NK_SHADER_READ,
                                NkResourceState::NK_RENDER_TARGET);
    }

    void NkDeferredPass::EndLighting(NkICommandBuffer* cmd) {
        if (!mReady) return;
        cmd->TextureBarrier(mTexLib->GetRHIHandle(mGBuffer.lightAccum),
                                NkResourceState::NK_RENDER_TARGET,
                                NkResourceState::NK_SHADER_READ);
    }

    bool NkDeferredPass::Resize(uint32 w, uint32 h) {
        if (w == mWidth && h == mHeight) return true;
        DestroyGBufferTextures();
        mWidth  = w;
        mHeight = h;
        return CreateGBufferTextures();
    }

    // ── Private ──────────────────────────────────────────────────────────────

    bool NkDeferredPass::CreateGBufferTextures() {
        mGBuffer.albedoMetallic  = mTexLib->CreateRenderTarget(mWidth, mHeight, NkGPUFormat::NK_RGBA8_UNORM,  false, true, "GBuf_Albedo");
        mGBuffer.normalRoughness = mTexLib->CreateRenderTarget(mWidth, mHeight, NkGPUFormat::NK_RGBA16_FLOAT, false, true, "GBuf_Normal");
        mGBuffer.emissiveAO      = mTexLib->CreateRenderTarget(mWidth, mHeight, NkGPUFormat::NK_RGBA16_FLOAT, false, true, "GBuf_Emissive");
        mGBuffer.depth           = mTexLib->CreateRenderTarget(mWidth, mHeight, NkGPUFormat::NK_D32_FLOAT_S8_UINT, true, true, "GBuf_Depth");
        mGBuffer.lightAccum      = mTexLib->CreateRenderTarget(mWidth, mHeight, NkGPUFormat::NK_RGBA16_FLOAT, false, true, "LightAccum");

        if (mCfg.outputVelocity)
            mGBuffer.velocity    = mTexLib->CreateRenderTarget(mWidth, mHeight, NkGPUFormat::NK_RG16_FLOAT,   false, true, "GBuf_Velocity");

        return mGBuffer.IsValid();
    }

    void NkDeferredPass::DestroyGBufferTextures() {
        if (mGBuffer.albedoMetallic.IsValid())  mTexLib->Release(mGBuffer.albedoMetallic);
        if (mGBuffer.normalRoughness.IsValid()) mTexLib->Release(mGBuffer.normalRoughness);
        if (mGBuffer.emissiveAO.IsValid())      mTexLib->Release(mGBuffer.emissiveAO);
        if (mGBuffer.depth.IsValid())           mTexLib->Release(mGBuffer.depth);
        if (mGBuffer.lightAccum.IsValid())      mTexLib->Release(mGBuffer.lightAccum);
        if (mGBuffer.velocity.IsValid())        mTexLib->Release(mGBuffer.velocity);
        mGBuffer = {};
    }

    void NkDeferredPass::BuildLightGrid(NkICommandBuffer* /*cmd*/) {
        if (!mLightDataBuf.IsValid() || mLights.Empty()) return;
        // Upload light data to GPU
        NkMappedMemory mapped = mDevice->MapBuffer(mLightDataBuf);
        if (mapped.IsValid()) {
            memcpy(mapped.ptr, mLights.Data(), mLights.Size() * sizeof(NkLightDesc));
            mDevice->UnmapBuffer(mLightDataBuf);
        }
        // Tiled light grid dispatch would be done here via a compute pipeline
    }

    NkTextureDesc NkDeferredPass::MakeGBufDesc(uint32 w, uint32 h,
                                                NkGPUFormat fmt, bool isDepth) const {
        NkTextureDesc d;
        d.type      = NkTextureType::NK_TEX2D;
        d.format    = fmt;
        d.width     = w;
        d.height    = h;
        d.mipLevels = 1;
        d.bindFlags = isDepth
            ? NkBindFlags::NK_DEPTH_STENCIL | NkBindFlags::NK_SHADER_RESOURCE
            : NkBindFlags::NK_RENDER_TARGET  | NkBindFlags::NK_SHADER_RESOURCE;
        return d;
    }

} // namespace renderer
} // namespace nkentseu
