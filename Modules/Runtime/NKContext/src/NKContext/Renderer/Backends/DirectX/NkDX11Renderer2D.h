#pragma once
// =============================================================================
// NkDX11Renderer2D.h — DirectX 11 2D renderer backend
// =============================================================================
#include "NKContext/Renderer/Batch/NkBatchRenderer2D.h"

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
using Microsoft::WRL::ComPtr;

namespace nkentseu {
    namespace renderer {

        class NkDX11Renderer2D final : public NkBatchRenderer2D {
            public:
                NkDX11Renderer2D()  = default;
                ~NkDX11Renderer2D() override { if (IsValid()) Shutdown(); }

                bool Initialize(NkIGraphicsContext* ctx) override;
                void Shutdown()                          override;
                bool IsValid()                   const   override { return mIsValid; }
                void Clear(const NkColor2D& col)         override;

            protected:
                void BeginBackend()  override;
                void EndBackend()    override;
                void SubmitBatches(const NkBatchGroup* groups, uint32 groupCount,
                                const NkVertex2D* verts, uint32 vCount,
                                const uint32*     idx,   uint32 iCount) override;
                void UploadProjection(const float32 proj[16]) override;

            private:
                bool CreateShaders();
                bool CreateBuffers();
                bool CreateStates();
                bool CreateWhiteTexture();
                void ApplyBlendMode(NkBlendMode mode);

                NkIGraphicsContext*       mCtx     = nullptr;
                bool                      mIsValid = false;

                ComPtr<ID3D11Device1>        mDevice;
                ComPtr<ID3D11DeviceContext1>  mDevCtx;

                ComPtr<ID3D11VertexShader>   mVS;
                ComPtr<ID3D11PixelShader>    mPS;
                ComPtr<ID3D11InputLayout>    mInputLayout;

                ComPtr<ID3D11Buffer>         mVB;          // vertex buffer (dynamic)
                ComPtr<ID3D11Buffer>         mIB;          // index buffer (dynamic)
                ComPtr<ID3D11Buffer>         mCBProj;      // constant buffer: projection

                ComPtr<ID3D11BlendState>     mBlendAlpha;
                ComPtr<ID3D11BlendState>     mBlendAdd;
                ComPtr<ID3D11BlendState>     mBlendMul;
                ComPtr<ID3D11BlendState>     mBlendNone;
                ComPtr<ID3D11RasterizerState> mRasterState;
                ComPtr<ID3D11DepthStencilState> mDSSState;
                ComPtr<ID3D11SamplerState>   mSamplerLinear;
                ComPtr<ID3D11SamplerState>   mSamplerNearest;

                ComPtr<ID3D11Texture2D>          mWhiteTex;
                ComPtr<ID3D11ShaderResourceView> mWhiteSRV;

                NkBlendMode mLastBlend = NkBlendMode::NK_NONE;
        };

    } // namespace renderer
} // namespace nkentseu
#endif // WINDOWS