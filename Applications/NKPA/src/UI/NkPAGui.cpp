/**
 * @File    NkPAGui.cpp
 * @Brief   UI NKPA sur NKGui, rendue via NKCanvas (NkGuiCanvasBackend) — pile
 *          Pong/NKCode, prouvée sur les 5 backends. La vue centrale est laissée
 *          VIDE : la sim (dessinée avant l'UI, dans le rect central) transparaît.
 */

#include "UI/NkPAGui.h"

#include "NKGui/NKGui.h"
#include "NKCanvas/UI/NkGuiCanvasBackend.h"
#include "NKCanvas/Renderer/Core/NkIRenderer2D.h"
#include "NKLogger/NkLog.h"

#include <cstdio>

using namespace nkentseu;
using namespace nkentseu::nkpa;

namespace {
	// ── Métriques du layout éditeur (partagées BuildFrame ↔ GetViewportRect) ──────
	constexpr float32 kMenuH = 26.f;    // barre de menu
	constexpr float32 kToolH = 40.f;    // toolbar
	constexpr float32 kHierW = 200.f;   // largeur hiérarchie
	constexpr float32 kInspW = 280.f;   // largeur inspecteur
	constexpr float32 kViewHdrH = 24.f; // en-tête du panneau viewport central
} // namespace

namespace nkentseu {
	namespace nkpa {

		struct NkPAGui::Impl {
				renderer::NkIRenderer2D *renderer = nullptr;
				int32 w = 1280, h = 720;
				bool valid = false;

				nkgui::NkGuiContext ctx;
				nkgui::NkGuiFont font;
				renderer::NkGuiCanvasBackend backend;
		};

		// ─────────────────────────────────────────────────────────────────────────────
		NkPAGui::NkPAGui() {
			mImpl = new Impl();
		}
		NkPAGui::~NkPAGui() {
			Shutdown();
			delete mImpl;
			mImpl = nullptr;
		}

		bool NkPAGui::Init(renderer::NkIRenderer2D *renderer, int32 w, int32 h) {
			auto &m = *mImpl;
			m.renderer = renderer;
			m.w = w;
			m.h = h;

			m.ctx.Init(w, h);
			nkgui::SetCurrentContext(&m.ctx);
			if (!m.font.LoadEmbedded(NkEmbeddedFontId::DroidSans, 18.f, false))
				m.font.LoadEmbedded(NkEmbeddedFontId::ProggyClean, 16.f, false);
			m.ctx.font = &m.font;

			if (!m.backend.Init(renderer)) {
				logger.Error("[NKPA-Gui] backend NKCanvas echec");
				return false;
			}
			if (m.font.pixels && m.font.atlasW > 0 && m.font.atlasH > 0)
				m.backend.UploadFontGray8(m.font.TexId(), m.font.pixels, m.font.atlasW, m.font.atlasH);

			m.valid = true;
			logger.Info("[NKPA-Gui] UI NKGui/NKCanvas prete (atlas {0}x{1})", m.font.atlasW, m.font.atlasH);
			return true;
		}

		void NkPAGui::OnResize(int32 w, int32 h) {
			mImpl->w = w;
			mImpl->h = h;
		}

		void NkPAGui::GetViewportRect(const NkPAUIState &state, int32 &x, int32 &y, int32 &w, int32 &h) const {
			auto &m = *mImpl;
			const int32 leftW = state.showHierarchy ? (int32)kHierW : 0;
			const int32 rightW = state.showInspector ? (int32)kInspW : 0;
			const int32 topY = (int32)(kMenuH + kToolH + kViewHdrH); // sous l'en-tête viewport
			x = leftW;
			y = topY;
			w = m.w - leftW - rightW;
			h = m.h - topY;
			if (w < 16)
				w = 16;
			if (h < 16)
				h = 16;
		}

		bool NkPAGui::IsValid() const {
			return mImpl && mImpl->valid;
		}

		void NkPAGui::Shutdown() {
			auto &m = *mImpl;
			m.valid = false;
			m.renderer = nullptr;
		}

		// ── Input ─────────────────────────────────────────────────────────────────
		void NkPAGui::BeginInput() {}

		void NkPAGui::SetMousePos(float32 x, float32 y) {
			mImpl->ctx.input.mousePos = {x, y};
		}
		void NkPAGui::SetMouseButton(int32 btn, bool down) {
			if (btn >= 0 && btn < 3)
				mImpl->ctx.input.mouseDown[btn] = down;
		}
		void NkPAGui::AddMouseWheel(float32 dy) {
			mImpl->ctx.input.wheel += dy;
		}

		// ── BuildFrame : layout éditeur (menu + toolbar + hiérarchie + inspecteur) ──
		// La ZONE CENTRALE est laissée vide : la sim, dessinée AVANT l'UI dans ce rect,
		// transparaît (viewport dans le layout éditeur).
		void NkPAGui::BuildFrame(float32 dt, NkPAUIState &state, const char *backendName) {
			auto &m = *mImpl;
			if (!m.valid)
				return;
			using namespace nkgui;
			SetCurrentContext(&m.ctx);
			m.ctx.viewW = m.w;
			m.ctx.viewH = m.h;
			m.ctx.BeginFrame(dt);

			const float32 W = (float32)m.w, H = (float32)m.h;
			const float32 menuH = kMenuH, toolH = kToolH;
			const float32 topY = menuH + toolH;
			const float32 leftW = state.showHierarchy ? kHierW : 0.f;
			const float32 rightW = state.showInspector ? kInspW : 0.f;

			static const char *kApis[5] = {"OpenGL", "Vulkan", "DX11", "DX12", "Software"};
			char buf[128];

			// ── Barre de menu ─────────────────────────────────────────────────────────
			if (BeginMenuBar(m.ctx, {0.f, 0.f, W, menuH})) {
				if (BeginMenu(m.ctx, "Fichier")) {
					if (MenuItem(m.ctx, "Quitter", "Echap")) {
					}
					EndMenu(m.ctx);
				}
				if (BeginMenu(m.ctx, "Affichage")) {
					if (MenuItem(m.ctx, "Hierarchie"))
						state.showHierarchy = !state.showHierarchy;
					if (MenuItem(m.ctx, "Inspecteur"))
						state.showInspector = !state.showInspector;
					EndMenu(m.ctx);
				}
				if (BeginMenu(m.ctx, "Backend")) {
					for (int32 i = 0; i < 5; ++i)
						if (MenuItem(m.ctx, kApis[i])) {
							state.requestBackendIndex = i;
							state.requestRestart = true;
						}
					EndMenu(m.ctx);
				}
				EndMenuBar(m.ctx);
			}

			// ── Toolbar : bande transport (play/pause/record), sans titre ─────────────
			m.ctx.dl.AddRectFilled({0.f, menuH, W, toolH}, m.ctx.theme.header);
			m.ctx.dl.AddRectFilled({0.f, menuH + toolH - 1.f, W, 1.f}, m.ctx.theme.accent);
			{
				const float32 by = menuH + 6.f, bh = toolH - 12.f;
				if (Button(m.ctx, state.paused ? "  Play  " : " Pause ", {8.f, by, 84.f, bh}))
					state.paused = !state.paused;
				if (Button(m.ctx, state.recording ? "Stop Rec" : " Record ", {98.f, by, 96.f, bh}))
					state.recording = !state.recording;
				if (state.recording)
					TextAt(m.ctx, {202.f, menuH + 13.f}, "REC");
			}

			// ── En-tête du panneau viewport central (au-dessus de la render-texture) ───
			{
				const float32 cx = leftW;
				const float32 cw = W - leftW - rightW;
				const float32 hy = topY; // = menuH + toolH
				m.ctx.dl.AddRectFilled({cx, hy, cw, kViewHdrH}, m.ctx.theme.header);
				m.ctx.dl.AddRectFilled({cx, hy + kViewHdrH - 1.f, cw, 1.f}, m.ctx.theme.accent);
				TextAt(m.ctx, {cx + 10.f, hy + 5.f}, "Scene");
				// Cadre fin autour de la zone viewport (contenu = render-texture).
				const float32 vy = hy + kViewHdrH;
				const float32 vh = H - vy;
				m.ctx.dl.AddRectFilled({cx, vy, 1.f, vh}, m.ctx.theme.accent);			  // bord gauche
				m.ctx.dl.AddRectFilled({cx + cw - 1.f, vy, 1.f, vh}, m.ctx.theme.accent); // bord droit
			}

			// ── Hiérarchie (gauche) : univers → espèces ───────────────────────────────
			if (state.showHierarchy && BeginPanel(m.ctx, "Hierarchie", {0.f, topY, leftW, H - topY})) {
				Text(m.ctx, "Univers");
				Separator(m.ctx);
				for (int32 i = 0; i < kSpeciesCount; ++i) {
					m.ctx.PushId((const void *)&state.speciesEnabled[i]);
					Checkbox(m.ctx, "", state.speciesEnabled[i]);
					m.ctx.SameLine();
					if (Selectable(m.ctx, SpeciesName(i), state.selectedSpecies == i))
						state.selectedSpecies = i;
					m.ctx.PopId();
				}
				EndPanel(m.ctx);
			}

			// ── Inspecteur (droite) : propriétés + réglages globaux ───────────────────
			if (state.showInspector && BeginPanel(m.ctx, "Inspecteur", {W - rightW, topY, rightW, H - topY})) {
				std::snprintf(buf, sizeof(buf), "Backend : %s", backendName ? backendName : "?");
				Text(m.ctx, buf);
				SliderFloat(m.ctx, "Vitesse", state.speedScale, 0.1f, 3.0f);
				Separator(m.ctx);

				if (state.selectedSpecies >= 0 && state.selectedSpecies < kSpeciesCount) {
					std::snprintf(buf, sizeof(buf), "Espece : %s", SpeciesName(state.selectedSpecies));
					Text(m.ctx, buf);
					Checkbox(m.ctx, "Active", state.speciesEnabled[state.selectedSpecies]);
					bool oneTrail =
						state.trailsEnabled && !state.trailAll && state.trailSpecies == state.selectedSpecies;
					if (Checkbox(m.ctx, "Trainee (cette espece)", oneTrail)) {
						if (oneTrail) {
							state.trailsEnabled = true;
							state.trailAll = false;
							state.trailSpecies = state.selectedSpecies;
						} else if (!state.trailAll && state.trailSpecies == state.selectedSpecies) {
							state.trailsEnabled = false;
						}
					}
				} else {
					Text(m.ctx, "(selectionne une espece)");
				}

				Separator(m.ctx);
				Text(m.ctx, "Trainees (global) :");
				Checkbox(m.ctx, "Activer", state.trailsEnabled);
				Checkbox(m.ctx, "Toutes especes", state.trailAll);

				Separator(m.ctx);
				Text(m.ctx, "Backend (redemarre) :");
				for (int32 i = 0; i < 5; ++i)
					if (Button(m.ctx, kApis[i])) {
						state.requestBackendIndex = i;
						state.requestRestart = true;
					}

				EndPanel(m.ctx);
			}

			m.ctx.EndFrame();
		}

		// ── Render : soumet la draw-list NKGui au renderer NKCanvas ─────────────────
		void NkPAGui::Render(uint32 fbW, uint32 fbH) {
			auto &m = *mImpl;
			if (!m.valid)
				return;
			m.backend.Submit(m.ctx.dl, fbW, fbH);
			m.backend.Submit(m.ctx.dlOverlay, fbW, fbH);
		}

	} // namespace nkpa
} // namespace nkentseu
