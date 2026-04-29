#pragma once
// NkOffscreenTarget.h — NKRenderer v4.0 (Tools/Offscreen/)
#include "../../Core/NkRendererTypes.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
namespace nkentseu { namespace renderer {
    class NkTextureLibrary;

    struct NkOffscreenDesc {
        uint32  width=1024, height=1024;
        bool    hasDepth=true, hasStencil=false;
        NkGPUFormat colorFmt=NkGPUFormat::NK_RGBA8_SRGB;
        NkGPUFormat depthFmt=NkGPUFormat::NK_D32_FLOAT;
        bool    hdr=false, readable=true, readback=false;
        uint32  layers=1;
        NkString name;
    };

    class NkOffscreenTarget {
    public:
        bool Init(NkIDevice* d, NkTextureLibrary* t, const NkOffscreenDesc& desc);
        void Shutdown();
        bool IsValid() const { return mValid; }
        void BeginCapture(NkICommandBuffer* cmd,
                           bool clearColor=true, NkVec4f cc={0,0,0,1},
                           bool clearDepth=true);
        void EndCapture(NkICommandBuffer* cmd);
        bool ReadbackPixels(uint8* dst, uint32 rowPitch=0);
        bool Resize(uint32 w, uint32 h);
        NkTexHandle GetColorResult() const { return mColor; }
        NkTexHandle GetDepthResult() const { return mDepth; }
        uint32 GetWidth()  const { return mDesc.width; }
        uint32 GetHeight() const { return mDesc.height; }
        const NkOffscreenDesc& GetDesc() const { return mDesc; }
    private:
        NkIDevice*       mDevice=nullptr;
        NkTextureLibrary*mTexLib=nullptr;
        NkOffscreenDesc  mDesc;
        NkTexHandle      mColor, mDepth;
        NkFramebufferHandle mFBO;
        NkRenderPassHandle  mRP;
        NkBufferHandle      mReadBuf;
        bool mValid=false, mCapturing=false;
    };
}} // namespace
