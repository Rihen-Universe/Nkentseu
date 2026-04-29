// NkOverlayRenderer.cpp — NKRenderer v4.0
#include "NkOverlayRenderer.h"
#include "../Text/NkTextRenderer.h"
#include "../Render2D/NkRender2D.h"
#include <cstdio>
#include <cstdarg>

namespace nkentseu {
namespace renderer {

    bool NkOverlayRenderer::Init(NkIDevice* d, NkRender2D* r2d, NkTextRenderer* txt) {
        mDevice=d; mR2D=r2d; mTxt=txt;
        mFont = mTxt ? mTxt->GetDefaultFont() : NkFontHandle::Null();
        return true;
    }
    void NkOverlayRenderer::Shutdown() {}

    void NkOverlayRenderer::BeginOverlay(NkICommandBuffer* cmd, uint32 w, uint32 h) {
        mW=w; mH=h; mActive=true;
        if(mR2D) mR2D->Begin(cmd,w,h);
    }
    void NkOverlayRenderer::EndOverlay() {
        if(mR2D) mR2D->End();
        mActive=false;
    }
    void NkOverlayRenderer::FlushPending(NkICommandBuffer* cmd) {
        if(mR2D) mR2D->FlushPending(cmd);
    }

    void NkOverlayRenderer::DrawStats(const NkRendererStats& s, NkVec2f pos) {
        if (!mTxt || !mFont.IsValid()) return;
        char buf[256];
        snprintf(buf,sizeof(buf),
            "Draw:%u  Tris:%u  GPU:%.2fms  CPU:%.2fms  Batches:%u",
            s.drawCalls, s.triangles, s.gpuTimeMs, s.cpuTimeMs, s.batchCount);
        mTxt->DrawText(pos, buf, mFont, 14.f, 0xFFFFFFFF);

        // Background semi-transparent
        if(mR2D){
            float32 w2=400,h2=20;
            mR2D->FillRect({pos.x-2,pos.y-2,w2,h2+4},{0,0,0,0.5f});
        }
    }

    void NkOverlayRenderer::ShowTexture(NkTexHandle t, NkRectF dst) {
        if(mR2D) mR2D->DrawImage(t, dst);
    }

    void NkOverlayRenderer::DrawText(NkVec2f pos, const char* fmt, ...) {
        if(!mTxt||!mFont.IsValid()) return;
        char buf[512]; va_list va; va_start(va,fmt);
        vsnprintf(buf,sizeof(buf),fmt,va); va_end(va);
        mTxt->DrawText(pos, buf, mFont, 14.f, 0xFFFFFFFF);
    }

} // namespace renderer
} // namespace nkentseu
