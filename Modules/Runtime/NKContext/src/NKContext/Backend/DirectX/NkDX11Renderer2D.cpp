// =============================================================================
// NkDX11Renderer2D.cpp — DirectX 11 2D renderer
// Uses a single VS/PS pair with one CB for the projection matrix.
// Dynamic VB/IB — re-mapped with D3D11_MAP_WRITE_DISCARD each frame.
// =============================================================================
#include "NkDX11Renderer2D.h"
#if defined(NKENTSEU_PLATFORM_WINDOWS)

#include "NKContext/Renderer/Resources/NkTexture.h"
#include "NKContext/Core/NkNativeContextAccess.h"
#include "NkDirectXContextData.h"
#include "NKLogger/NkLog.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define NK_DX11_2D_LOG(...) logger.Infof("[NkDX11-2D] " __VA_ARGS__)
#define NK_DX11_2D_ERR(...) logger.Errorf("[NkDX11-2D] " __VA_ARGS__)
#define NK_DX11_2D_CHECK(hr, msg) do { if(FAILED(hr)) { NK_DX11_2D_ERR(msg " 0x%08X", (unsigned)(hr)); return false; } } while(0)

namespace nkentseu {
    namespace renderer {

        // ── HLSL ─────────────────────────────────────────────────────────────────────
        static const char* kDX11_2D_HLSL = R"(
        cbuffer CB0 : register(b0) { float4x4 u_Projection; };

        struct VSIn  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };
        struct VSOut { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR; };

        VSOut VS(VSIn v) {
            VSOut o;
            o.pos = mul(u_Projection, float4(v.pos, 0.f, 1.f));
            o.uv  = v.uv;
            o.col = v.col;
            return o;
        }

        Texture2D    t_Texture : register(t0);
        SamplerState s_Sampler : register(s0);

        float4 PS(VSOut v) : SV_TARGET {
            return t_Texture.Sample(s_Sampler, v.uv) * v.col;
        }
        )";

        // =============================================================================
        bool NkDX11Renderer2D::Initialize(NkIGraphicsContext* ctx) {
            if (mIsValid) return false;
            if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_GFX_API_D3D11) {
                NK_DX11_2D_ERR("Requires a DX11 graphics context");
                return false;
            }
            mCtx = ctx;

            NkDX11ContextData* d = NkNativeContext::DX11(ctx);
            if (!d || !d->device || !d->context) {
                NK_DX11_2D_ERR("Invalid DX11 context data");
                return false;
            }
            mDevice  = d->device;
            mDevCtx  = d->context;

            if (!CreateShaders())      return false;
            if (!CreateBuffers())      return false;
            if (!CreateStates())       return false;
            if (!CreateWhiteTexture()) return false;

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
            NK_DX11_2D_LOG("Initialized");
            return true;
        }

        // =============================================================================
        bool NkDX11Renderer2D::CreateShaders() {
            ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
            HRESULT hr = D3DCompile(kDX11_2D_HLSL, strlen(kDX11_2D_HLSL),
                                    nullptr, nullptr, nullptr,
                                    "VS", "vs_4_0", 0, 0, &vsBlob, &errBlob);
            if (FAILED(hr)) {
                NK_DX11_2D_ERR("VS compile: %s",
                    errBlob ? (char*)errBlob->GetBufferPointer() : "?");
                return false;
            }
            hr = D3DCompile(kDX11_2D_HLSL, strlen(kDX11_2D_HLSL),
                            nullptr, nullptr, nullptr,
                            "PS", "ps_4_0", 0, 0, &psBlob, &errBlob);
            if (FAILED(hr)) {
                NK_DX11_2D_ERR("PS compile: %s",
                    errBlob ? (char*)errBlob->GetBufferPointer() : "?");
                return false;
            }

            NK_DX11_2D_CHECK(mDevice->CreateVertexShader(vsBlob->GetBufferPointer(),
                vsBlob->GetBufferSize(), nullptr, &mVS), "CreateVertexShader");
            NK_DX11_2D_CHECK(mDevice->CreatePixelShader(psBlob->GetBufferPointer(),
                psBlob->GetBufferSize(), nullptr, &mPS), "CreatePixelShader");

            D3D11_INPUT_ELEMENT_DESC layout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
            };
            NK_DX11_2D_CHECK(mDevice->CreateInputLayout(layout, 3,
                vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &mInputLayout),
                "CreateInputLayout");
            return true;
        }

        // =============================================================================
        bool NkDX11Renderer2D::CreateBuffers() {
            // Dynamic vertex buffer
            D3D11_BUFFER_DESC vbd{};
            vbd.ByteWidth      = kMaxVertices * (UINT)sizeof(NkVertex2D);
            vbd.Usage          = D3D11_USAGE_DYNAMIC;
            vbd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
            vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            NK_DX11_2D_CHECK(mDevice->CreateBuffer(&vbd, nullptr, &mVB), "CreateVB");

            // Dynamic index buffer
            D3D11_BUFFER_DESC ibd{};
            ibd.ByteWidth      = kMaxIndices * (UINT)sizeof(uint32);
            ibd.Usage          = D3D11_USAGE_DYNAMIC;
            ibd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
            ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            NK_DX11_2D_CHECK(mDevice->CreateBuffer(&ibd, nullptr, &mIB), "CreateIB");

            // Constant buffer for projection matrix
            D3D11_BUFFER_DESC cbd{};
            cbd.ByteWidth      = 64; // 4x4 float
            cbd.Usage          = D3D11_USAGE_DYNAMIC;
            cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            NK_DX11_2D_CHECK(mDevice->CreateBuffer(&cbd, nullptr, &mCBProj), "CreateCB");
            return true;
        }

        // =============================================================================
        bool NkDX11Renderer2D::CreateStates() {
            // Blend states
            auto MakeBlend = [&](D3D11_BLEND src, D3D11_BLEND dst,
                                D3D11_BLEND srcA, D3D11_BLEND dstA,
                                ComPtr<ID3D11BlendState>& out) -> bool {
                D3D11_BLEND_DESC bd{};
                bd.RenderTarget[0].BlendEnable           = TRUE;
                bd.RenderTarget[0].SrcBlend              = src;
                bd.RenderTarget[0].DestBlend             = dst;
                bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
                bd.RenderTarget[0].SrcBlendAlpha         = srcA;
                bd.RenderTarget[0].DestBlendAlpha        = dstA;
                bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
                bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
                return SUCCEEDED(mDevice->CreateBlendState(&bd, &out));
            };
            MakeBlend(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
                    D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, mBlendAlpha);
            MakeBlend(D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_ONE,
                    D3D11_BLEND_ONE, D3D11_BLEND_ONE, mBlendAdd);
            MakeBlend(D3D11_BLEND_DEST_COLOR, D3D11_BLEND_ZERO,
                    D3D11_BLEND_ONE, D3D11_BLEND_ZERO, mBlendMul);

            D3D11_BLEND_DESC bdn{}; // no blend
            bdn.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            mDevice->CreateBlendState(&bdn, &mBlendNone);

            // Rasterizer (no culling, no depth clip for 2D)
            D3D11_RASTERIZER_DESC rd{};
            rd.FillMode        = D3D11_FILL_SOLID;
            rd.CullMode        = D3D11_CULL_NONE;
            rd.ScissorEnable   = FALSE;
            rd.DepthClipEnable = FALSE;
            mDevice->CreateRasterizerState(&rd, &mRasterState);

            // Depth stencil (no depth test for 2D)
            D3D11_DEPTH_STENCIL_DESC dsd{};
            dsd.DepthEnable    = FALSE;
            dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            mDevice->CreateDepthStencilState(&dsd, &mDSSState);

            // Samplers
            D3D11_SAMPLER_DESC sd{};
            sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            mDevice->CreateSamplerState(&sd, &mSamplerLinear);
            sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            mDevice->CreateSamplerState(&sd, &mSamplerNearest);

            return true;
        }

        // =============================================================================
        bool NkDX11Renderer2D::CreateWhiteTexture() {
            D3D11_TEXTURE2D_DESC td{};
            td.Width = td.Height = 1;
            td.MipLevels = td.ArraySize = 1;
            td.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count   = 1;
            td.Usage              = D3D11_USAGE_IMMUTABLE;
            td.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
            const uint32 white = 0xFFFFFFFFu;
            D3D11_SUBRESOURCE_DATA init{ &white, 4, 4 };
            NK_DX11_2D_CHECK(mDevice->CreateTexture2D(&td, &init, &mWhiteTex), "CreateWhiteTex");
            NK_DX11_2D_CHECK(mDevice->CreateShaderResourceView(mWhiteTex.Get(), nullptr, &mWhiteSRV), "CreateWhiteSRV");
            return true;
        }

        // =============================================================================
        void NkDX11Renderer2D::Shutdown() {
            if (!mIsValid) return;
            mWhiteSRV.Reset(); mWhiteTex.Reset();
            mSamplerNearest.Reset(); mSamplerLinear.Reset();
            mDSSState.Reset(); mRasterState.Reset();
            mBlendNone.Reset(); mBlendMul.Reset(); mBlendAdd.Reset(); mBlendAlpha.Reset();
            mCBProj.Reset(); mIB.Reset(); mVB.Reset();
            mInputLayout.Reset(); mPS.Reset(); mVS.Reset();
            mIsValid = false;
            NK_DX11_2D_LOG("Shutdown");
        }

        // =============================================================================
        void NkDX11Renderer2D::Clear(const NkColor2D& col) {
            NkDX11ContextData* d = NkNativeContext::DX11(mCtx);
            if (!d || !d->context || !d->rtv) return;
            float fc[4] = { col.r * math::c1_255, col.g * math::c1_255, col.b * math::c1_255, col.a * math::c1_255 };
            d->context->ClearRenderTargetView(d->rtv.Get(), fc);
        }

        // =============================================================================
        void NkDX11Renderer2D::ApplyBlendMode(NkBlendMode mode) {
            if (mode == mLastBlend) return;
            mLastBlend = mode;
            const float factor[4] = {0,0,0,0};
            switch (mode) {
                case NkBlendMode::NK_ALPHA:    mDevCtx->OMSetBlendState(mBlendAlpha.Get(), factor, 0xFFFFFFFF); break;
                case NkBlendMode::NK_ADD:      mDevCtx->OMSetBlendState(mBlendAdd.Get(),   factor, 0xFFFFFFFF); break;
                case NkBlendMode::NK_MULTIPLY: mDevCtx->OMSetBlendState(mBlendMul.Get(),   factor, 0xFFFFFFFF); break;
                default:                    mDevCtx->OMSetBlendState(mBlendNone.Get(),  factor, 0xFFFFFFFF); break;
            }
        }

        // =============================================================================
        void NkDX11Renderer2D::BeginBackend() {
            mDevCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            mDevCtx->IASetInputLayout(mInputLayout.Get());
            mDevCtx->VSSetShader(mVS.Get(), nullptr, 0);
            mDevCtx->PSSetShader(mPS.Get(), nullptr, 0);
            mDevCtx->RSSetState(mRasterState.Get());
            mDevCtx->OMSetDepthStencilState(mDSSState.Get(), 0);
            ID3D11Buffer* cbs[] = { mCBProj.Get() };
            mDevCtx->VSSetConstantBuffers(0, 1, cbs);
            ID3D11SamplerState* samp[] = { mSamplerLinear.Get() };
            mDevCtx->PSSetSamplers(0, 1, samp);
            mLastBlend = NkBlendMode::NK_NONE;
            ApplyBlendMode(NkBlendMode::NK_ALPHA);
        }

        // =============================================================================
        void NkDX11Renderer2D::EndBackend() {}

        // =============================================================================
        void NkDX11Renderer2D::SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                            const NkVertex2D* verts, uint32 vCount,
                                            const uint32*     idx,   uint32 iCount) {
            if (!mIsValid || !vCount || !iCount) return;

            // Upload vertices
            {
                D3D11_MAPPED_SUBRESOURCE m{};
                mDevCtx->Map(mVB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
                memcpy(m.pData, verts, vCount * sizeof(NkVertex2D));
                mDevCtx->Unmap(mVB.Get(), 0);
            }

            // Upload indices
            {
                D3D11_MAPPED_SUBRESOURCE m{};
                mDevCtx->Map(mIB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
                memcpy(m.pData, idx, iCount * sizeof(uint32));
                mDevCtx->Unmap(mIB.Get(), 0);
            }

            UINT stride = (UINT)sizeof(NkVertex2D), off = 0;
            ID3D11Buffer* vbs[] = { mVB.Get() };
            mDevCtx->IASetVertexBuffers(0, 1, vbs, &stride, &off);
            mDevCtx->IASetIndexBuffer(mIB.Get(), DXGI_FORMAT_R32_UINT, 0);

            // Set viewport
            D3D11_VIEWPORT vp{};
            vp.TopLeftX = (float)mViewport.left;
            vp.TopLeftY = (float)mViewport.top;
            vp.Width    = (float)mViewport.width;
            vp.Height   = (float)mViewport.height;
            vp.MinDepth = 0.f; vp.MaxDepth = 1.f;
            mDevCtx->RSSetViewports(1, &vp);

            for (uint32 g = 0; g < groupCount; ++g) {
                const auto& group = groups[g];
                ApplyBlendMode(group.blendMode);

                ID3D11ShaderResourceView* srv = mWhiteSRV.Get();
                if (group.texture && group.texture->GetHandle()) {
                    srv = static_cast<ID3D11ShaderResourceView*>(group.texture->GetHandle());
                }
                mDevCtx->PSSetShaderResources(0, 1, &srv);
                mDevCtx->DrawIndexed((UINT)group.indexCount, group.indexStart, 0);
            }
        }

        // =============================================================================
        void NkDX11Renderer2D::UploadProjection(const float32 proj[16]) {
            if (!mCBProj) return;
            D3D11_MAPPED_SUBRESOURCE m{};
            mDevCtx->Map(mCBProj.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
            memcpy(m.pData, proj, 64);
            mDevCtx->Unmap(mCBProj.Get(), 0);
        }

    } // namespace renderer
} // namespace nkentseu
#endif // WINDOWS