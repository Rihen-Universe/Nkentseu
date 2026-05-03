#pragma once
// NkOverlayRenderer.h — NKRenderer v4.0 (Tools/Overlay/)
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
namespace nkentseu { namespace renderer {
    class NkRender2D; class NkTextRenderer;
    class NkOverlayRenderer {
    public:
        bool Init(NkIDevice* d, NkRender2D* r2d, NkTextRenderer* txt);
        void Shutdown();
        void BeginOverlay(NkICommandBuffer* cmd, uint32 w, uint32 h);
        void EndOverlay();
        void FlushPending(NkICommandBuffer* cmd);
        void DrawStats(const NkRendererStats& s, NkVec2f pos={10,10});
        void ShowTexture(NkTexHandle t, NkRectF dst);
        void DrawText(NkVec2f pos, const char* fmt, ...);
    private:
        NkIDevice*      mDevice=nullptr;
        NkRender2D*     mR2D=nullptr;
        NkTextRenderer* mTxt=nullptr;
        NkFontHandle    mFont;
        uint32 mW=0,mH=0; bool mActive=false;
    };
}} // namespace
