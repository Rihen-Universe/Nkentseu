#pragma once
/**
 * @File    NkPAGui.h
 * @Brief   Interface UI NKPA basée sur NKGui, rendue via NKCanvas
 *          (nkgui + NkGuiCanvasBackend) — la pile de Pong/NKCode, prouvée sur les
 *          5 backends. Pimpl : les en-têtes NKGui/NKCanvas restent internes au .cpp.
 */

#include "NKCore/NkTypes.h"
#include "UI/NkPAUIState.h"

namespace nkentseu {
	namespace renderer {
		class NkIRenderer2D;
	}
	namespace nkpa {

		class NkPAGui {
			public:
				NkPAGui();
				~NkPAGui();
				NkPAGui(const NkPAGui &) = delete;
				NkPAGui &operator=(const NkPAGui &) = delete;

				/// Monte le contexte NKGui + la police embarquée + le backend NKCanvas
				/// (utilise le renderer 2D de la fenêtre).
				bool Init(renderer::NkIRenderer2D *renderer, int32 w, int32 h);
				void OnResize(int32 w, int32 h);
				void Shutdown();
				bool IsValid() const;

				// ── Input (depuis les events NKWindow) ────────────────────────────────────
				void BeginInput();
				void SetMousePos(float32 x, float32 y);
				void SetMouseButton(int32 btn, bool down);
				void AddMouseWheel(float32 dy);

				/// Rectangle CENTRAL (la vue/simulation) en pixels, selon les panneaux.
				void GetViewportRect(const NkPAUIState &state, int32 &x, int32 &y, int32 &w, int32 &h) const;

				/// Construit les widgets du panneau pour la frame.
				void BuildFrame(float32 dt, NkPAUIState &state, const char *backendName);

				/// Soumet la draw-list NKGui au renderer NKCanvas (dans une frame active,
				/// entre Clear() et Display()). fbW/fbH = taille du framebuffer.
				void Render(uint32 fbW, uint32 fbH);

			private:
				struct Impl;
				Impl *mImpl = nullptr;
		};

	} // namespace nkpa
} // namespace nkentseu
