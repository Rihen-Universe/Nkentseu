// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NkOverlayRenderer.cpp — NKRenderer v4.0
#include "NkOverlayRenderer.h"
#include "NKRenderer/Tools/Text/NkTextRenderer.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"
#include <cstdio>
#include <cstdarg>
#include "NKCore/Text/NkSnprintf.h"
// Suppress Win32 GDI macro after all headers
#ifdef DrawText
#undef DrawText
#endif

namespace nkentseu {
	namespace renderer {

		bool NkOverlayRenderer::Init(NkIDevice *d, NkRender2D *r2d, NkTextRenderer *txt) {
			mDevice = d;
			mR2D = r2d;
			mTxt = txt;
			mFont = mTxt ? mTxt->GetDefaultFont() : NkFontHandle::Null();
			return true;
		}

		void NkOverlayRenderer::Shutdown() {
		}

		void NkOverlayRenderer::BeginOverlay(NkICommandBuffer *cmd, uint32 w, uint32 h) {
			mW = w;
			mH = h;
			mActive = true;
			if (mR2D)
				mR2D->Begin(cmd, w, h);
		}

		void NkOverlayRenderer::EndOverlay() {
			if (mR2D)
				mR2D->End();
			mActive = false;
		}

		void NkOverlayRenderer::FlushPending(NkICommandBuffer *cmd) {
			if (mR2D)
				mR2D->FlushPending(cmd);
		}

		void NkOverlayRenderer::DrawStats(const NkRendererStats &s, NkVec2f pos) {
			if (!mTxt || !mFont.IsValid())
				return;
			char buf[256];
			// Un instrument absent se dit (« -- »), il ne rend pas un faux zero (2026-09-04).
			char gpu[24];
			if (s.gpuTimeValid)
				snprintf(gpu, sizeof(gpu), "%.2fms", s.gpuTimeMs);
			else
				snprintf(gpu, sizeof(gpu), "--");
			nkentseu::NkSnprintf(buf, sizeof(buf), "Draw:%u  Tris:%u  GPU:%s  CPU:%.2fms  Batches:%u", s.drawCalls, s.triangles, gpu,
					 s.cpuTimeMs, s.batchCount);
			mTxt->DrawText(pos, buf, mFont, 14.f, 0xFFFFFFFF);

			// Background semi-transparent
			if (mR2D) {
				float32 w2 = 400, h2 = 20;
				mR2D->FillRect({pos.x - 2, pos.y - 2, w2, h2 + 4}, {0, 0, 0, 0.5f});
			}
		}

		void NkOverlayRenderer::ShowTexture(NkTexHandle t, NkRectF dst) {
			if (mR2D)
				mR2D->DrawImage(t, dst);
		}

		void NkOverlayRenderer::DrawText(NkVec2f pos, const char *fmt, ...) {
			if (!mTxt || !mFont.IsValid())
				return;
			char buf[512];
			va_list va;
			va_start(va, fmt);
			nkentseu::NkVsnprintf(buf, sizeof(buf), fmt, va);
			va_end(va);
			mTxt->DrawText(pos, buf, mFont, 14.f, 0xFFFFFFFF);
		}

	} // namespace renderer
} // namespace nkentseu
