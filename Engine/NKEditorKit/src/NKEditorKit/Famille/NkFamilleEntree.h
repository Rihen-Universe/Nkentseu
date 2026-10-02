#pragma once
// -----------------------------------------------------------------------------
// @File    NkFamilleEntree.h
// @Brief   L'ENTREE de la famille : un evenement de la fenetre (NKEvent) devient
//          l'entree NKGui de la trame, sans qu'un clic se perde entre deux
//          trames ; et le curseur que NKGui demande redevient celui de la fenetre.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// D'OU CA VIENT
//   UnkenyEditor (NkEditeurApp::OnEvent, NkEditeurSouris.h), qui a recopie
//   NkEditorShell::HookEvents / MapEditKey quand il a quitte la coquille. Un
//   editeur qui peint sa disposition (UE5) n'a plus le shell pour remplir
//   l'entree : il le fait ICI, avec les memes correspondances.
//
// UTILISATION
//   static NkFamilleEntree entree;
//   bool OnEvent(NkEvent *e) { entree.Lire(ctx.input, *e); return false; }
//   ... trame ... ; entree.FinDeTrame(ctx.input);
//   fenetre.SetCursor(NkFamilleCurseur(ctx.wantCursor));
// -----------------------------------------------------------------------------

#include "NKEvent/NkEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKGui/Core/NkGuiInput.h"
#include "NKWindow/Core/NkWindow.h"

namespace nkentseu {
	namespace editorkit {

		/// Les boutons de la souris, SANS clic perdu entre deux trames (le clic plus
		/// court qu'une trame : son relachement attend que la trame ait VU l'appui).
		/// La recette de NkEditeurSouris.h, telle quelle.
		struct NkFamilleSouris {
				bool appuiNonVu[3] = {};
				bool relacheDiffere[3] = {};
				bool appuiDiffere[3] = {};
				bool relacheApresAppui[3] = {};

				void Appui(nkgui::NkGuiInput &in, int32 b) noexcept {
					if (b < 0 || b > 2) {
						return;
					}
					if (!in.mouseDown[b] && in.mousePrev[b]) {
						appuiDiffere[b] = true;
						return;
					}
					in.mouseDown[b] = true;
					appuiNonVu[b] = true;
				}

				void Relache(nkgui::NkGuiInput &in, int32 b) noexcept {
					if (b < 0 || b > 2) {
						return;
					}
					if (appuiDiffere[b]) {
						relacheApresAppui[b] = true;
					} else if (appuiNonVu[b]) {
						relacheDiffere[b] = true;
					} else {
						in.mouseDown[b] = false;
					}
				}

				void FinDeTrame(nkgui::NkGuiInput &in) noexcept {
					for (int32 b = 0; b < 3; ++b) {
						appuiNonVu[b] = false;
						if (relacheDiffere[b]) {
							relacheDiffere[b] = false;
							in.mouseDown[b] = false;
						}
						if (appuiDiffere[b]) {
							appuiDiffere[b] = false;
							in.mouseDown[b] = true;
							appuiNonVu[b] = true;
							if (relacheApresAppui[b]) {
								relacheApresAppui[b] = false;
								relacheDiffere[b] = true;
							}
						}
					}
				}
		};

		/// Convention NKGui : [0] gauche, [1] droit, [2] milieu.
		inline int32 NkFamilleIndiceBouton(NkMouseButton b) noexcept {
			switch (b) {
				case NkMouseButton::NK_MB_LEFT:   return 0;
				case NkMouseButton::NK_MB_RIGHT:  return 1;
				case NkMouseButton::NK_MB_MIDDLE: return 2;
				default:                          return -1;
			}
		}

		/// Le curseur que NKGui demande, dans le vocabulaire de la fenetre (la meme
		/// table que NkEditorShell::MapCursor).
		inline NkWindow::NkCursorType NkFamilleCurseur(nkgui::NkGuiCursor c) noexcept {
			switch (c) {
				case nkgui::NkGuiCursor::Text:     return NkWindow::NkCursorType::TextInput;
				case nkgui::NkGuiCursor::Hand:     return NkWindow::NkCursorType::Hand;
				case nkgui::NkGuiCursor::ResizeEW: return NkWindow::NkCursorType::ResizeWE;
				case nkgui::NkGuiCursor::ResizeNS: return NkWindow::NkCursorType::ResizeNS;
				default:                           return NkWindow::NkCursorType::Arrow;
			}
		}

		/// Touche de l'OS -> touche NKGui (la table d'UnkenyEditor / NkEditorShell).
		inline bool NkFamilleToucheGui(NkKey k, nkgui::NkGuiKey &sortie) noexcept {
			using G = nkgui::NkGuiKey;
			struct NkPaire {
					NkKey os;
					G gui;
			};
			static const NkPaire kTable[] = {
				{NkKey::NK_LEFT, G::Left},		   {NkKey::NK_RIGHT, G::Right},		  {NkKey::NK_UP, G::Up},
				{NkKey::NK_DOWN, G::Down},		   {NkKey::NK_HOME, G::Home},		  {NkKey::NK_END, G::End},
				{NkKey::NK_BACK, G::Backspace},	   {NkKey::NK_DELETE, G::Delete},	  {NkKey::NK_ENTER, G::Enter},
				{NkKey::NK_ESCAPE, G::Escape},	   {NkKey::NK_TAB, G::Tab},			  {NkKey::NK_F2, G::F2},
				{NkKey::NK_F5, G::F5},			   {NkKey::NK_F8, G::F8},			  {NkKey::NK_F12, G::F12},
				{NkKey::NK_SPACE, G::Space},	   {NkKey::NK_BACKSLASH, G::Backslash}, {NkKey::NK_PERIOD, G::Period},
				{NkKey::NK_SLASH, G::Slash},	   {NkKey::NK_LBRACKET, G::LBracket}, {NkKey::NK_RBRACKET, G::RBracket},
				{NkKey::NK_MINUS, G::Minus},	   {NkKey::NK_EQUALS, G::Equal},	  {NkKey::NK_COMMA, G::Comma},
				{NkKey::NK_NUM0, G::Num0},		   {NkKey::NK_NUMPAD_0, G::Num0},	  {NkKey::NK_NUM1, G::Num1},
				{NkKey::NK_NUM2, G::Num2},		   {NkKey::NK_NUM3, G::Num3},		  {NkKey::NK_NUMPAD_3, G::Num3},
				{NkKey::NK_NUM4, G::Num4},		   {NkKey::NK_NUMPAD_4, G::Num4},	  {NkKey::NK_NUM5, G::Num5},
				{NkKey::NK_NUMPAD_5, G::Num5},	   {NkKey::NK_NUM6, G::Num6},		  {NkKey::NK_NUMPAD_6, G::Num6},
				{NkKey::NK_A, G::A},			   {NkKey::NK_B, G::B},				  {NkKey::NK_C, G::C},
				{NkKey::NK_D, G::D},			   {NkKey::NK_E, G::E},				  {NkKey::NK_F, G::F},
				{NkKey::NK_G, G::G},			   {NkKey::NK_H, G::H},				  {NkKey::NK_I, G::I},
				{NkKey::NK_J, G::J},			   {NkKey::NK_K, G::K},				  {NkKey::NK_L, G::L},
				{NkKey::NK_M, G::M},			   {NkKey::NK_N, G::N},				  {NkKey::NK_O, G::O},
				{NkKey::NK_Q, G::Q},			   {NkKey::NK_R, G::R},				  {NkKey::NK_S, G::S},
				{NkKey::NK_T, G::T},			   {NkKey::NK_U, G::U},				  {NkKey::NK_V, G::V},
				{NkKey::NK_W, G::W},			   {NkKey::NK_Y, G::Y},				  {NkKey::NK_Z, G::Z},
			};
			for (const NkPaire &p : kTable) {
				if (p.os == k) {
					sortie = p.gui;
					return true;
				}
			}
			return false;
		}

		/// L'entree complete : la souris (sans clic perdu), la molette, le texte,
		/// le clavier et ses modificateurs, le double-clic de l'OS.
		struct NkFamilleEntree {
				NkFamilleSouris boutons;

				/// Rend vrai si l'evenement etait de ceux qu'une interface lit (il
				/// n'est pas « consomme » pour autant : le moteur peut le lire aussi).
				bool Lire(nkgui::NkGuiInput &in, const NkEvent &event) noexcept {
					if (const auto *e = event.As<NkMouseMoveEvent>()) {
						in.mousePos = nkgui::NkVec2{static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
						return true;
					}
					if (const auto *e = event.As<NkMouseButtonPressEvent>()) {
						in.mousePos = nkgui::NkVec2{static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
						boutons.Appui(in, NkFamilleIndiceBouton(e->GetButton()));
						Modificateurs(in, e->GetModifiers());
						return true;
					}
					if (const auto *e = event.As<NkMouseButtonReleaseEvent>()) {
						boutons.Relache(in, NkFamilleIndiceBouton(e->GetButton()));
						return true;
					}
					if (const auto *e = event.As<NkMouseDoubleClickEvent>()) {
						in.mousePos = nkgui::NkVec2{static_cast<float32>(e->GetX()), static_cast<float32>(e->GetY())};
						if (e->GetButton() == NkMouseButton::NK_MB_LEFT) {
							in.SetDoubleClick(0);
						}
						return true;
					}
					if (const auto *e = event.As<NkMouseWheelVerticalEvent>()) {
						in.wheel += static_cast<float32>(e->GetDeltaY());
						Modificateurs(in, e->GetModifiers());
						return true;
					}
					if (const auto *e = event.As<NkMouseWheelHorizontalEvent>()) {
						in.wheelH += static_cast<float32>(e->GetDeltaX());
						return true;
					}
					if (const auto *e = event.As<NkTextInputEvent>()) {
						in.PushChar(e->GetCodepoint());
						return true;
					}
					if (const auto *e = event.As<NkKeyPressEvent>()) {
						const NkKey k = e->GetKey();
						nkgui::NkGuiKey g;
						if (NkFamilleToucheGui(k, g)) {
							in.SetKey(g, true);
						}
						Modificateurs(in, e->GetModifiers());
						if (e->GetModifiers().ctrl) {
							// Copier / couper / coller / tout choisir des champs.
							if (k == NkKey::NK_C) {
								in.wantCopy = true;
							} else if (k == NkKey::NK_X) {
								in.wantCut = true;
							} else if (k == NkKey::NK_V) {
								in.wantPaste = true;
							} else if (k == NkKey::NK_A) {
								in.wantSelectAll = true;
							}
						}
						return true;
					}
					if (const auto *e = event.As<NkKeyReleaseEvent>()) {
						nkgui::NkGuiKey g;
						if (NkFamilleToucheGui(e->GetKey(), g)) {
							in.SetKey(g, false);
						}
						Modificateurs(in, e->GetModifiers());
						return true;
					}
					return false;
				}

				/// La trame a VU les appuis : les relachements retenus partent.
				void FinDeTrame(nkgui::NkGuiInput &in) noexcept {
					boutons.FinDeTrame(in);
				}

			private:
				template <typename M> static void Modificateurs(nkgui::NkGuiInput &in, const M &m) noexcept {
					in.ctrlDown = m.ctrl;
					in.shiftDown = m.shift;
					in.altDown = m.alt;
				}
		};

	} // namespace editorkit
} // namespace nkentseu
