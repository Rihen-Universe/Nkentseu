// =============================================================================
// NkDX11Renderer2D.cpp — DirectX 11 2D renderer
// Uses a single VS/PS pair with one CB for the projection matrix.
// Dynamic VB/IB — re-mapped with D3D11_MAP_WRITE_DISCARD each frame.
// =============================================================================
#include "NkDX11Renderer2D.h"
#if defined(NKENTSEU_PLATFORM_WINDOWS)

#include "NKCanvas/Renderer/Resources/NkTexture.h"
#include "NKCanvas/Renderer/Resources/NkTextureBackend.h"
#include "NKCanvas/Renderer/Resources/NkShaderBackend.h"
#include "NKCanvas/Renderer/Targets/NkRenderTextureBackend.h"
#include "NKCanvas/Core/NkNativeContextAccess.h"
#include "NkDirectXContextData.h"
#include "NKLogger/NkLog.h"
#include "NKContainers/Sequential/NkVector.h"

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

        // ── Registry globale des textures DX11 ───────────────────────────────────────
        // Les callbacks de la dispatch table sont des `static` sans `this` ; on stocke
        // donc device/context capturés à l'init + une table des textures vivantes.
        // L'ID exposé à NkTexture est 1-based (0 = invalide), il indexe directement
        // `entries[id - 1]`. Les slots libérés restent à nullptr (table append-only
        // simple ; pas de free-list — suffisant pour A.7).
        struct NkDX11TextureEntry {
            ID3D11Texture2D*           texture = nullptr;
            ID3D11ShaderResourceView*  srv     = nullptr;
            ID3D11SamplerState*        sampler = nullptr;
            NkTextureFilter            filter  = NkTextureFilter::NK_LINEAR;
            NkTextureWrap              wrap    = NkTextureWrap::NK_CLAMP;
            uint32                     width   = 0;
            uint32                     height  = 0;
        };

        static struct {
            ID3D11Device*                 device  = nullptr;
            ID3D11DeviceContext*          context = nullptr;
            NkVector<NkDX11TextureEntry*> entries;  // index = id - 1
        } gDX11Registry;

        // ── Render-texture registry (FBO offscreen). Une entry contient le
        // Tex2D + RTV pour le rendu, et reutilise une entry du registry texture
        // (entries[colorTexId-1]) pour exposer l'image en sampler downstream.
        struct NkDX11RTEntry {
            ID3D11Texture2D*        tex      = nullptr;
            ID3D11RenderTargetView* rtv      = nullptr;
            uint32                  colorTextureId = 0; // index dans gDX11Registry.entries
            uint32                  width    = 0;
            uint32                  height   = 0;
        };
        static struct {
            NkVector<NkDX11RTEntry*> entries;
            // Sauvegarde du RTV courant lors d'un Bind, pour pouvoir restorer.
            ID3D11RenderTargetView*  savedRTV = nullptr;
            ID3D11DepthStencilView*  savedDSV = nullptr;
        } gDX11RTRegistry;

        // Recupere l'entry depuis un ID 1-based (nullptr si invalide).
        static NkDX11TextureEntry* DX11_GetEntry(uint32 id) {
            if (id == 0 || id > gDX11Registry.entries.Size()) return nullptr;
            return gDX11Registry.entries[id - 1];
        }

        // Construit (ou reconstruit) le sampler d'une entry depuis filter/wrap.
        static void DX11_RebuildSampler(NkDX11TextureEntry* e) {
            if (!e || !gDX11Registry.device) return;
            if (e->sampler) { e->sampler->Release(); e->sampler = nullptr; }

            D3D11_SAMPLER_DESC sd{};
            sd.Filter = (e->filter == NkTextureFilter::NK_NEAREST)
                            ? D3D11_FILTER_MIN_MAG_MIP_POINT
                            : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            D3D11_TEXTURE_ADDRESS_MODE addr = D3D11_TEXTURE_ADDRESS_CLAMP;
            if      (e->wrap == NkTextureWrap::NK_REPEAT)        addr = D3D11_TEXTURE_ADDRESS_WRAP;
            else if (e->wrap == NkTextureWrap::NK_MIRROR_REPEAT) addr = D3D11_TEXTURE_ADDRESS_MIRROR;
            sd.AddressU = sd.AddressV = sd.AddressW = addr;
            sd.MinLOD = 0.f;
            sd.MaxLOD = D3D11_FLOAT32_MAX;
            gDX11Registry.device->CreateSamplerState(&sd, &e->sampler);
        }

        // =============================================================================
        // Callbacks dispatch table NkTexture
        // =============================================================================
        uint32 NkDX11Renderer2D::CreateDX11Texture(uint32 w, uint32 h, const uint8* rgba) {
            if (!gDX11Registry.device || w == 0 || h == 0) return 0;

            D3D11_TEXTURE2D_DESC td{};
            td.Width            = w;
            td.Height           = h;
            td.MipLevels        = 1;
            td.ArraySize        = 1;
            td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
            td.SampleDesc.Count = 1;
            td.Usage            = D3D11_USAGE_DEFAULT;
            td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

            D3D11_SUBRESOURCE_DATA init{};
            init.pSysMem     = rgba;
            init.SysMemPitch = w * 4;

            ID3D11Texture2D* tex = nullptr;
            HRESULT hr = gDX11Registry.device->CreateTexture2D(
                &td, rgba ? &init : nullptr, &tex);
            if (FAILED(hr) || !tex) return 0;

            D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
            sd.Format              = td.Format;
            sd.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MostDetailedMip = 0;
            sd.Texture2D.MipLevels       = 1;

            ID3D11ShaderResourceView* srv = nullptr;
            hr = gDX11Registry.device->CreateShaderResourceView(tex, &sd, &srv);
            if (FAILED(hr) || !srv) { tex->Release(); return 0; }

            NkDX11TextureEntry* e = new NkDX11TextureEntry();
            e->texture = tex;
            e->srv     = srv;
            e->width   = w;
            e->height  = h;
            e->filter  = NkTextureFilter::NK_LINEAR;
            e->wrap    = NkTextureWrap::NK_CLAMP;
            DX11_RebuildSampler(e);

            gDX11Registry.entries.PushBack(e);
            return (uint32)gDX11Registry.entries.Size(); // ID 1-based
        }

        void NkDX11Renderer2D::UpdateDX11Texture(uint32 id, uint32 x, uint32 y,
                                                 uint32 w, uint32 h, const uint8* rgba) {
            NkDX11TextureEntry* e = DX11_GetEntry(id);
            if (!e || !e->texture || !gDX11Registry.context || !rgba) return;
            if (w == 0 || h == 0) return;

            D3D11_BOX box{};
            box.left   = x;
            box.top    = y;
            box.front  = 0;
            box.right  = x + w;
            box.bottom = y + h;
            box.back   = 1;

            gDX11Registry.context->UpdateSubresource(
                e->texture, 0, &box, rgba, w * 4, 0);
        }

        void NkDX11Renderer2D::DeleteDX11Texture(uint32 id) {
            NkDX11TextureEntry* e = DX11_GetEntry(id);
            if (!e) return;
            if (e->sampler) { e->sampler->Release(); e->sampler = nullptr; }
            if (e->srv)     { e->srv->Release();     e->srv     = nullptr; }
            if (e->texture) { e->texture->Release(); e->texture = nullptr; }
            delete e;
            gDX11Registry.entries[id - 1] = nullptr;
        }

        void NkDX11Renderer2D::SetDX11TextureFilter(uint32 id, NkTextureFilter f) {
            NkDX11TextureEntry* e = DX11_GetEntry(id);
            if (!e) return;
            e->filter = f;
            DX11_RebuildSampler(e);
        }

        void NkDX11Renderer2D::SetDX11TextureWrap(uint32 id, NkTextureWrap w) {
            NkDX11TextureEntry* e = DX11_GetEntry(id);
            if (!e) return;
            e->wrap = w;
            DX11_RebuildSampler(e);
        }

        // =============================================================================
        bool NkDX11Renderer2D::Initialize(NkIGraphicsContext* ctx) {
            if (mIsValid) return false;
            if (!ctx || ctx->GetApi() != NkGraphicsApi::NK_GFX_API_DX11) {
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

            // ── Enregistrement dispatch table NkTexture (cf NkTextureBackend.h) ──
            // Capture device/context dans la registry globale puis publie les 5
            // callbacks statiques. NkTexture::Create/Update/Destroy/SetFilter/
            // SetWrap utilisera ces helpers pour gerer ses textures DX11.
            gDX11Registry.device  = mDevice.Get();
            gDX11Registry.context = mDevCtx.Get();
            {
                NkTextureBackend backend{};
                backend.Create    = &NkDX11Renderer2D::CreateDX11Texture;
                backend.Update    = &NkDX11Renderer2D::UpdateDX11Texture;
                backend.Destroy   = &NkDX11Renderer2D::DeleteDX11Texture;
                backend.SetFilter = &NkDX11Renderer2D::SetDX11TextureFilter;
                backend.SetWrap   = &NkDX11Renderer2D::SetDX11TextureWrap;
                NkTextureSetBackend(backend);
            }

            // NkShader sur DX11 : compile HLSL via D3DCompile + CreatePixel/
            // VertexShader. La switch shader user / shader engine se fait en
            // OMSet*/PSSet* dans SubmitBatches. Implementation differee — stub
            // installe pour preserver la consistance API.
            NkShaderInstallUnsupportedBackend("DX11");
            NkRenderTextureInstallUnsupportedBackend("DX11");

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
            // Scissor toujours active : on fournit un rect plein-viewport quand il n'y
            // a pas de clip, ou le rect de clip quand il y en a un (cf. SubmitBatches).
            rd.ScissorEnable   = TRUE;
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

            // Desenregistre la dispatch table (NkTexture::Destroy ulterieurs no-op)
            // et libere toutes les textures restantes dans la registry globale.
            {
                NkTextureBackend empty{};
                NkTextureSetBackend(empty);
            }
            for (usize i = 0; i < gDX11Registry.entries.Size(); ++i) {
                NkDX11TextureEntry* e = gDX11Registry.entries[i];
                if (!e) continue;
                if (e->sampler) { e->sampler->Release(); e->sampler = nullptr; }
                if (e->srv)     { e->srv->Release();     e->srv     = nullptr; }
                if (e->texture) { e->texture->Release(); e->texture = nullptr; }
                delete e;
                gDX11Registry.entries[i] = nullptr;
            }
            gDX11Registry.entries.Clear();
            gDX11Registry.device  = nullptr;
            gDX11Registry.context = nullptr;

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
            math::NkColorF cf = col.ToColorF();
            float fc[4] = { cf.r, cf.g, cf.b, cf.a };
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
            // Bind la RTV du contexte comme cible de rendu active. Sans ca, les
            // drawcalls n'ont aucune cible liee -> invisibles (alors que Clear, via
            // ClearRenderTargetView, efface bien le fond car il cible la RTV
            // directement sans la binder). C'etait LA cause du "background OK mais
            // drawcalls invisibles" en DX11. DSV null : le 2D desactive le depth.
            if (NkDX11ContextData* d = NkNativeContext::DX11(mCtx)) {
                if (d->context && d->rtv) {
                    ID3D11RenderTargetView* rtvs[] = { d->rtv.Get() };
                    d->context->OMSetRenderTargets(1, rtvs, nullptr);
                }
            }
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

            // Scissor (rasterizer state a ScissorEnable=TRUE) : clip si actif, sinon
            // plein viewport. D3D11_RECT = {left, top, right, bottom}, origine haut-gauche.
            D3D11_RECT sc{};
            if (mHasClip) {
                sc.left   = (LONG)mClipRect.x;
                sc.top    = (LONG)mClipRect.y;
                sc.right  = (LONG)(mClipRect.x + mClipRect.width);
                sc.bottom = (LONG)(mClipRect.y + mClipRect.height);
            } else {
                sc.left   = (LONG)mViewport.left;
                sc.top    = (LONG)mViewport.top;
                sc.right  = (LONG)(mViewport.left + mViewport.width);
                sc.bottom = (LONG)(mViewport.top  + mViewport.height);
            }
            mDevCtx->RSSetScissorRects(1, &sc);

            for (uint32 g = 0; g < groupCount; ++g) {
                const auto& group = groups[g];
                ApplyBlendMode(group.blendMode);

                // Resolution texture : ID 1-based stocke dans mGPUId par NkTexture
                // -> entry registry (SRV + sampler dedie). Fallback sur les SRV/
                // sampler par defaut (white tex + linear clamp) si aucune.
                ID3D11ShaderResourceView* srv     = mWhiteSRV.Get();
                ID3D11SamplerState*       sampler = mSamplerLinear.Get();
                if (group.texture) {
                    NkDX11TextureEntry* e = DX11_GetEntry(group.texture->GetGPUId());
                    if (e) {
                        if (e->srv)     srv     = e->srv;
                        if (e->sampler) sampler = e->sampler;
                    } else if (group.texture->GetHandle()) {
                        // Compat : un caller externe a injecte directement un SRV.
                        srv = static_cast<ID3D11ShaderResourceView*>(group.texture->GetHandle());
                    }
                }
                mDevCtx->PSSetShaderResources(0, 1, &srv);
                mDevCtx->PSSetSamplers(0, 1, &sampler);
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