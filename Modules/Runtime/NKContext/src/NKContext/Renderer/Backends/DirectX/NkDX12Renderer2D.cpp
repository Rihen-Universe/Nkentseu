// =============================================================================
// NkDX12Renderer2D.cpp — DirectX 12 2D renderer
// Root signature: root constants (16 floats = projection), descriptor table
// (SRV t0), static sampler (s0). No CBV heap needed.
// =============================================================================
#include "NkDX12Renderer2D.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)

#include "NKContext/Renderer/Resources/NkTexture.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NKContext/Graphics/DirectX/NkDirectXContextData.h"
#include "NKLogger/NkLog.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

// #include <d3dx12.h>

#define NK_DX12_2D_LOG(...) logger.Infof("[NkDX12-2D] " __VA_ARGS__)
#define NK_DX12_2D_ERR(...) logger.Errorf("[NkDX12-2D] " __VA_ARGS__)
#define NK_DX12_2D_CHECK(hr,msg) do{if(FAILED(hr)){NK_DX12_2D_ERR(msg " 0x%08X",(unsigned)(hr));return false;}}while(0)

namespace nkentseu {
    namespace renderer {

        // ── HLSL ─────────────────────────────────────────────────────────────────────
        static const char* kDX12_2D_HLSL = R"(
        struct Proj { float4x4 proj; };
        ConstantBuffer<Proj> u_CB : register(b0);

        Texture2D    t_Texture : register(t0);
        SamplerState s_Sampler : register(s0);

        struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };
        struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };

        VSOut VS(VSIn v) {
            VSOut o;
            o.pos = mul(u_CB.proj, float4(v.pos, 0.f, 1.f));
            o.uv  = v.uv;
            o.col = v.col;
            return o;
        }

        float4 PS(VSOut v) : SV_TARGET {
            return t_Texture.Sample(s_Sampler, v.uv) * v.col;
        }
        )";

        // =============================================================================
        bool NkDX12Renderer2D::Initialize(NkIGraphicsContext* ctx) {
            if (mIsValid) return false;
            if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_API_DIRECTX12) {
                NK_DX12_2D_ERR("Requires a DX12 context");
                return false;
            }
            mCtx = ctx;

            NkDX12ContextData* d = NkNativeContext::DX12(ctx);
            if (!d || !d->device) { NK_DX12_2D_ERR("Invalid DX12 context data"); return false; }
            mDevice  = d->device;
            mCmdList = d->cmdList;

            if (!CreateDescriptorHeap()) return false;
            if (!CreateSampler())        return false;
            if (!CreateRootSignature())  return false;
            if (!CreatePSOs())           return false;
            if (!CreateBuffers())        return false;
            if (!CreateWhiteTexture())   return false;

            NkContextInfo info = ctx->GetInfo();
            const uint32 W = info.windowWidth  > 0 ? info.windowWidth  : 800;
            const uint32 H = info.windowHeight > 0 ? info.windowHeight : 600;
            mDefaultView.center = { W * 0.5f, H * 0.5f };
            mDefaultView.size   = { (float)W, (float)H };
            mCurrentView        = mDefaultView;
            mViewport           = { 0, 0, (int32)W, (int32)H };

            float proj[16];
            mCurrentView.ToProjectionMatrix(proj);
            UploadProjection(proj);

            mIsValid = true;
            NK_DX12_2D_LOG("Initialized");
            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateDescriptorHeap() {
            // SRV heap (shader visible)
            D3D12_DESCRIPTOR_HEAP_DESC hd{};
            hd.NumDescriptors = kMaxSRVSlots;
            hd.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            hd.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            NK_DX12_2D_CHECK(mDevice->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&mSRVHeap)), "CreateSRVHeap");
            mSRVDescSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            // Sampler heap (shader visible, 1 slot)
            D3D12_DESCRIPTOR_HEAP_DESC sd{};
            sd.NumDescriptors = 1;
            sd.Type  = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            sd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            NK_DX12_2D_CHECK(mDevice->CreateDescriptorHeap(&sd, IID_PPV_ARGS(&mSamplerHeap)), "CreateSamplerHeap");

            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateSampler() {
            D3D12_SAMPLER_DESC sd{};
            sd.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            sd.AddressU = sd.AddressV = sd.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sd.MaxLOD   = D3D12_FLOAT32_MAX;
            mDevice->CreateSampler(&sd, mSamplerHeap->GetCPUDescriptorHandleForHeapStart());
            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateRootSignature() {
            // Slot 0: root constants (16 × float = proj)
            // Slot 1: descriptor table → SRV range (t0)
            // Slot 2: descriptor table → sampler range (s0)
            D3D12_ROOT_PARAMETER params[3]{};

            params[0].ParameterType            = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            params[0].Constants.ShaderRegister = 0;
            params[0].Constants.RegisterSpace  = 0;
            params[0].Constants.Num32BitValues = 16;
            params[0].ShaderVisibility         = D3D12_SHADER_VISIBILITY_VERTEX;

            D3D12_DESCRIPTOR_RANGE srvRange{};
            srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            srvRange.NumDescriptors                    = 1;
            srvRange.BaseShaderRegister                = 0;
            srvRange.RegisterSpace                     = 0;
            srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            params[1].ParameterType                    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[1].DescriptorTable.NumDescriptorRanges = 1;
            params[1].DescriptorTable.pDescriptorRanges   = &srvRange;
            params[1].ShaderVisibility                 = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_DESCRIPTOR_RANGE samplerRange{};
            samplerRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            samplerRange.NumDescriptors                    = 1;
            samplerRange.BaseShaderRegister                = 0;
            samplerRange.RegisterSpace                     = 0;
            samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            params[2].ParameterType                        = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            params[2].DescriptorTable.NumDescriptorRanges  = 1;
            params[2].DescriptorTable.pDescriptorRanges    = &samplerRange;
            params[2].ShaderVisibility                     = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC rsDesc{};
            rsDesc.NumParameters = 3;
            rsDesc.pParameters   = params;
            rsDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr<ID3DBlob> blob, errBlob;
            HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errBlob);
            if (FAILED(hr)) {
                NK_DX12_2D_ERR("Serialize root sig: %s",
                    errBlob ? (char*)errBlob->GetBufferPointer() : "?");
                return false;
            }
            NK_DX12_2D_CHECK(mDevice->CreateRootSignature(0,
                blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&mRootSig)),
                "CreateRootSig");
            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::MakePSO(D3D12_BLEND_DESC blendDesc,
                                        ComPtr<ID3D12PipelineState>& out,
                                        ID3DBlob* vsBlob, ID3DBlob* psBlob) {
            D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,      0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,      0,  8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            };

            D3D12_RASTERIZER_DESC rast{};
            rast.FillMode = D3D12_FILL_MODE_SOLID;
            rast.CullMode = D3D12_CULL_MODE_NONE;
            rast.DepthClipEnable = FALSE;

            D3D12_DEPTH_STENCIL_DESC dss{};
            dss.DepthEnable    = FALSE;
            dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
            psoDesc.pRootSignature        = mRootSig.Get();
            psoDesc.VS                    = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
            psoDesc.PS                    = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
            psoDesc.BlendState            = blendDesc;
            psoDesc.SampleMask            = UINT_MAX;
            psoDesc.RasterizerState       = rast;
            psoDesc.DepthStencilState     = dss;
            psoDesc.InputLayout           = { inputLayout, 3 };
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets      = 1;
            psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.DSVFormat             = DXGI_FORMAT_UNKNOWN;
            psoDesc.SampleDesc.Count      = 1;

            return SUCCEEDED(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&out)));
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreatePSOs() {
            ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
            HRESULT hr = D3DCompile(kDX12_2D_HLSL, strlen(kDX12_2D_HLSL),
                                    nullptr, nullptr, nullptr, "VS", "vs_5_1", 0, 0, &vsBlob, &errBlob);
            if (FAILED(hr)) { NK_DX12_2D_ERR("VS: %s", errBlob ? (char*)errBlob->GetBufferPointer() : "?"); return false; }
            errBlob.Reset();
            hr = D3DCompile(kDX12_2D_HLSL, strlen(kDX12_2D_HLSL),
                            nullptr, nullptr, nullptr, "PS", "ps_5_1", 0, 0, &psBlob, &errBlob);
            if (FAILED(hr)) { NK_DX12_2D_ERR("PS: %s", errBlob ? (char*)errBlob->GetBufferPointer() : "?"); return false; }

            // Helper to build a D3D12_BLEND_DESC for one render target
            auto MakeBlendDesc = [](D3D12_BLEND src, D3D12_BLEND dst,
                                    D3D12_BLEND srcA, D3D12_BLEND dstA,
                                    bool enable) -> D3D12_BLEND_DESC {
                D3D12_BLEND_DESC bd{};
                bd.RenderTarget[0].BlendEnable           = enable ? TRUE : FALSE;
                bd.RenderTarget[0].SrcBlend              = src;
                bd.RenderTarget[0].DestBlend             = dst;
                bd.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
                bd.RenderTarget[0].SrcBlendAlpha         = srcA;
                bd.RenderTarget[0].DestBlendAlpha        = dstA;
                bd.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
                bd.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
                return bd;
            };

            bool ok = true;
            ok &= MakePSO(MakeBlendDesc(D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA,
                                        D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, true),
                        mPSOAlpha, vsBlob.Get(), psBlob.Get());
            ok &= MakePSO(MakeBlendDesc(D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE,
                                        D3D12_BLEND_ONE, D3D12_BLEND_ONE, true),
                        mPSOAdd, vsBlob.Get(), psBlob.Get());
            ok &= MakePSO(MakeBlendDesc(D3D12_BLEND_DEST_COLOR, D3D12_BLEND_ZERO,
                                        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, true),
                        mPSOMul, vsBlob.Get(), psBlob.Get());
            ok &= MakePSO(MakeBlendDesc(D3D12_BLEND_ONE, D3D12_BLEND_ZERO,
                                        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, false),
                        mPSONone, vsBlob.Get(), psBlob.Get());
            return ok;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateBuffers() {
            const UINT64 vbSize = (UINT64)kMaxVertices * sizeof(NkVertex2D);
            const UINT64 ibSize = (UINT64)kMaxIndices  * sizeof(uint32);

            D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_UPLOAD };
            auto MakeBuf = [&](UINT64 sz, ComPtr<ID3D12Resource>& out) -> bool {
                D3D12_RESOURCE_DESC rd{};
                rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                rd.Width     = sz; rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
                rd.SampleDesc = {1,0};
                rd.Layout    = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                rd.Flags     = D3D12_RESOURCE_FLAG_NONE;
                return SUCCEEDED(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
                    &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out)));
            };

            if (!MakeBuf(vbSize, mVB)) { NK_DX12_2D_ERR("CreateVB"); return false; }
            if (!MakeBuf(ibSize, mIB)) { NK_DX12_2D_ERR("CreateIB"); return false; }

            mVB->Map(0, nullptr, &mVBMap);
            mIB->Map(0, nullptr, &mIBMap);

            mVBView.BufferLocation = mVB->GetGPUVirtualAddress();
            mVBView.SizeInBytes    = (UINT)vbSize;
            mVBView.StrideInBytes  = (UINT)sizeof(NkVertex2D);

            mIBView.BufferLocation = mIB->GetGPUVirtualAddress();
            mIBView.SizeInBytes    = (UINT)ibSize;
            mIBView.Format         = DXGI_FORMAT_R32_UINT;

            return true;
        }

        // =============================================================================
        bool NkDX12Renderer2D::CreateWhiteTexture() {
            // 1×1 white texture in upload heap (static, never updated)
            D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_DEFAULT };
            D3D12_RESOURCE_DESC rd{};
            rd.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            rd.Width = rd.Height  = 1;
            rd.DepthOrArraySize   = rd.MipLevels = 1;
            rd.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
            rd.SampleDesc.Count   = 1;
            rd.Flags              = D3D12_RESOURCE_FLAG_NONE;
            NK_DX12_2D_CHECK(mDevice->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE,
                &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mWhiteTex)),
                "CreateWhiteTex");

            // Upload via staging
            D3D12_HEAP_PROPERTIES uhp{ D3D12_HEAP_TYPE_UPLOAD };
            D3D12_RESOURCE_DESC urd{};
            urd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            urd.Width = 4; urd.Height = urd.DepthOrArraySize = urd.MipLevels = 1;
            urd.SampleDesc = {1,0}; urd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ComPtr<ID3D12Resource> staging;
            mDevice->CreateCommittedResource(&uhp, D3D12_HEAP_FLAG_NONE, &urd,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&staging));

            const uint32 white = 0xFFFFFFFFu;
            void* mapped = nullptr;
            staging->Map(0, nullptr, &mapped);
            memcpy(mapped, &white, 4);
            staging->Unmap(0, nullptr);

            D3D12_TEXTURE_COPY_LOCATION dst = {};
            dst.pResource = mWhiteTex.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION src = {};
            src.pResource = staging.Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint.Offset = 0;
            src.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            src.PlacedFootprint.Footprint.Width = 1;
            src.PlacedFootprint.Footprint.Height = 1;
            src.PlacedFootprint.Footprint.Depth = 1;
            src.PlacedFootprint.Footprint.RowPitch = 4;

            // One-shot copy
            NkDX12ContextData* d = NkNativeContext::DX12(mCtx);
            d->cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            TransitionResource(d->cmdList.Get(), mWhiteTex.Get(),
                            D3D12_RESOURCE_STATE_COPY_DEST,
                            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

            // Create SRV
            mWhiteSRVSlot = mNextSRVSlot++;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvd.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvd.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvd.Texture2D.MipLevels       = 1;
            mDevice->CreateShaderResourceView(mWhiteTex.Get(), &srvd, GetCPUHandle(mWhiteSRVSlot));

            return true;
        }

        // =============================================================================
        void NkDX12Renderer2D::TransitionResource(ID3D12GraphicsCommandList4* cmd,
                                                ID3D12Resource* res,
                                                D3D12_RESOURCE_STATES from,
                                                D3D12_RESOURCE_STATES to) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource   = res;
            b.Transition.StateBefore = from;
            b.Transition.StateAfter  = to;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmd->ResourceBarrier(1, &b);
        }

        // =============================================================================
        D3D12_GPU_DESCRIPTOR_HANDLE NkDX12Renderer2D::GetGPUHandle(uint32 slot) const {
            D3D12_GPU_DESCRIPTOR_HANDLE h = mSRVHeap->GetGPUDescriptorHandleForHeapStart();
            h.ptr += (uint64)slot * mSRVDescSize;
            return h;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE NkDX12Renderer2D::GetCPUHandle(uint32 slot) const {
            D3D12_CPU_DESCRIPTOR_HANDLE h = mSRVHeap->GetCPUDescriptorHandleForHeapStart();
            h.ptr += (uint64)slot * mSRVDescSize;
            return h;
        }

        // =============================================================================
        uint32 NkDX12Renderer2D::GetOrCreateSRVSlot(const NkTexture* tex) {
            if (!tex || !tex->IsValid()) return mWhiteSRVSlot;
            for (const auto& e : mTexSRVCache) {
                if (e.texture == tex) return e.slot;
            }
            if (mNextSRVSlot >= kMaxSRVSlots) return mWhiteSRVSlot;
            // tex->GetHandle() should be a ID3D12Resource* (the texture resource)
            ID3D12Resource* res = static_cast<ID3D12Resource*>(tex->GetHandle());
            if (!res) return mWhiteSRVSlot;

            uint32 slot = mNextSRVSlot++;
            D3D12_SHADER_RESOURCE_VIEW_DESC srvd{};
            srvd.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvd.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvd.Texture2D.MipLevels     = 1;
            mDevice->CreateShaderResourceView(res, &srvd, GetCPUHandle(slot));

            TexSRVEntry e{ tex, slot };
            mTexSRVCache.PushBack(e);
            return slot;
        }

        // =============================================================================
        void NkDX12Renderer2D::Shutdown() {
            if (!mIsValid) return;
            if (mVBMap) { mVB->Unmap(0, nullptr); mVBMap = nullptr; }
            if (mIBMap) { mIB->Unmap(0, nullptr); mIBMap = nullptr; }
            mIsValid = false;
            NK_DX12_2D_LOG("Shutdown");
        }

        // =============================================================================
        void NkDX12Renderer2D::Clear(const NkColor2D& col) {
            NkDX12ContextData* d = NkNativeContext::DX12(mCtx);
            if (!d || !d->cmdList) return;
            float fc[4] = { col.r * math::c1_255, col.g * math::c1_255, col.b * math::c1_255, col.a * math::c1_255 };
            const uint32 frame = d->currentBackBuffer;
            d->cmdList->ClearRenderTargetView(d->rtvHandles[frame], fc, 0, nullptr);
        }

        // =============================================================================
        void NkDX12Renderer2D::BeginBackend() {
            NkDX12ContextData* d = NkNativeContext::DX12(mCtx);
            if (!d || !d->cmdList) return;
            mCmdList = d->cmdList;

            ID3D12DescriptorHeap* heaps[] = { mSRVHeap.Get(), mSamplerHeap.Get() };
            mCmdList->SetDescriptorHeaps(2, heaps);
            mCmdList->SetGraphicsRootSignature(mRootSig.Get());
            mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mCmdList->IASetVertexBuffers(0, 1, &mVBView);
            mCmdList->IASetIndexBuffer(&mIBView);
            // Sampler: always the same
            mCmdList->SetGraphicsRootDescriptorTable(2,
                mSamplerHeap->GetGPUDescriptorHandleForHeapStart());
        }

        // =============================================================================
        void NkDX12Renderer2D::EndBackend() {}

        // =============================================================================
        void NkDX12Renderer2D::SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                            const NkVertex2D* verts, uint32 vCount,
                                            const uint32*     idx,   uint32 iCount) {
            if (!mIsValid || !mCmdList || !vCount || !iCount) return;

            memcpy(mVBMap, verts, vCount * sizeof(NkVertex2D));
            memcpy(mIBMap, idx,   iCount * sizeof(uint32));

            // Viewport + scissor
            D3D12_VIEWPORT vp{
                (float)mViewport.left, (float)mViewport.top,
                (float)mViewport.width, (float)mViewport.height,
                0.f, 1.f
            };
            D3D12_RECT scissor{ mViewport.left, mViewport.top,
                                mViewport.left + mViewport.width,
                                mViewport.top  + mViewport.height };
            mCmdList->RSSetViewports(1, &vp);
            mCmdList->RSSetScissorRects(1, &scissor);

            // Root constants: projection (16 × float)
            mCmdList->SetGraphicsRoot32BitConstants(0, 16, mProjection, 0);

            ID3D12PipelineState* currentPSO = nullptr;
            for (uint32 g = 0; g < groupCount; ++g) {
                const auto& group = groups[g];

                ID3D12PipelineState* pso = nullptr;
                switch (group.blendMode) {
                    case NkBlendMode::NK_ADD:      pso = mPSOAdd.Get();   break;
                    case NkBlendMode::NK_MULTIPLY: pso = mPSOMul.Get();   break;
                    case NkBlendMode::NK_NONE:     pso = mPSONone.Get();  break;
                    default:                    pso = mPSOAlpha.Get(); break;
                }
                if (pso != currentPSO) {
                    mCmdList->SetPipelineState(pso);
                    currentPSO = pso;
                }

                uint32 srvSlot = GetOrCreateSRVSlot(group.texture);
                mCmdList->SetGraphicsRootDescriptorTable(1, GetGPUHandle(srvSlot));
                mCmdList->DrawIndexedInstanced(group.indexCount, 1, group.indexStart, 0, 0);
            }
        }

        // =============================================================================
        void NkDX12Renderer2D::UploadProjection(const float32 proj[16]) {
            memcpy(mProjection, proj, 64);
        }

    } // namespace renderer
} // namespace nkentseu
#endif // WINDOWS