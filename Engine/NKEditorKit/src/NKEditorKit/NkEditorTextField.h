#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorTextField.h
// @Brief   Champ de saisie mono-ligne reutilisable pour les overlays (positionne
//          en absolu, edition complete : caret, SELECTION, copier/couper/coller,
//          double-clic, clic-position). Brique partagee des widgets NKEditorKit
//          (picker de fichiers, barres de recherche, dialogues) — INDEPENDANT de
//          toute application (raw NkGuiContext / NkGuiDrawList / NkGuiFont).
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// ── Le STYLE OPTIONNEL du champ superpose (2026-08-31, edition en place
		//    sur la toile — regle de Rodolf : « on doit voir qu'on edite DANS la
		//    toile, pas dans une boite ») ─────────────────────────────────────
		// Additif : nullptr = le comportement historique exact (fond opaque,
		// aligne a gauche, echelle 1). Le mecanisme est UNIQUE — chaque site
		// d'edition en place (toile, etiquette, hierarchie) passe par lui, la
		// regle du contrat (Entree valide, Echap annule, clic ailleurs valide)
		// ne se recopie pas par site.
		struct NkOverlayFieldStyle {
				bool fond = true;	   ///< false = TRANSPARENT (edition dans la toile)
				bool bord = true;	   ///< le lisere de focus
				int32 align = 0;	   ///< 0 gauche, 1 centre, 2 droite — l'alignement du NOEUD
				float32 echelle = 1.f; ///< corps affiche = police x echelle (texte zoome)
				NkColor texte = {230, 237, 243, 255};
		};

		// ── Champ de saisie mono-ligne minimal pour l'overlay (positionne en absolu) ──
		// Edite `buf` quand `focused` ; renvoie via les codepoints tapes + Backspace.
		inline void NkOverlayTextField(NkGuiContext &ctx, NkGuiDrawList &dl, const NkGuiFont *f, const NkRect &r,
									   char *buf, int32 cap, bool focused,
									   const NkOverlayFieldStyle *style = nullptr) {
			static const NkOverlayFieldStyle kDefaut;
			const NkOverlayFieldStyle &st = style ? *style : kDefaut;
			const float32 ech = st.echelle > 0.f ? st.echelle : 1.f;
			const float32 asc = (f ? f->Ascent() : 0.f) * ech, lh = (f ? f->LineHeight() : 0.f) * ech;
			if (st.fond)
				dl.AddRectFilled(r, NkColor{22, 27, 34, 255}, 4.f);
			if (st.bord)
				dl.AddRect(r, focused ? NkColor{88, 166, 255, 255} : NkColor{48, 54, 61, 255}, 1.f);
			// Edition complete (caret, SELECTION, copier/couper/coller, double-clic, clic-position).
			// Etat statique associe au buffer focus (un seul champ edite a la fois).
			static const void *s_owner = nullptr;
			static int32 s_caret = 0, s_anchor = -1;
			static float32 s_blink = 0.f;
			static bool s_drag = false;
			const float32 pad = st.fond ? 8.f : 2.f; // transparent : le texte reste A SA PLACE
			int32 len = 0;
			while (buf[len])
				++len;
			// ⚠️ TOUTES les mesures passent par l'ECHELLE : caret, selection,
			//    defilement et dessin restent d'accord entre eux quel que soit
			//    le zoom du document.
			auto measW = [&](const char *s) -> float32 {
				return f ? f->MeasureWidth(s) * ech : 0.f;
			};
			auto slice = [&](int32 n, char *out) {
				int32 m = n < 599 ? n : 599;
				for (int32 k = 0; k < m; ++k)
					out[k] = buf[k];
				out[m] = '\0';
			};
			// L'ORIGINE DU TEXTE SUIT L'ALIGNEMENT DU NOEUD (regle de Rodolf :
			// « l'alignement doit etre respecte PENDANT la frappe, pas au
			// valider ») — tant que le texte tient ; s'il deborde, gauche +
			// defilement, comme avant.
			auto origineX = [&]() -> float32 {
				const float32 availW0 = r.w - pad * 2.f;
				const float32 tw = measW(buf);
				if (tw <= availW0) {
					if (st.align == 1)
						return r.x + (r.w - tw) * 0.5f;
					if (st.align == 2)
						return r.x + r.w - pad - tw;
				}
				return r.x + pad;
			};
			if (focused) {
				if (s_owner != buf) {
					s_owner = buf;
					s_caret = len;
					s_anchor = -1;
					s_drag = false; // pas de drag herite d'un autre champ
				}
				auto &in = ctx.input;
				s_blink += in.dt;
				if (s_caret > len)
					s_caret = len;
				if (s_caret < 0)
					s_caret = 0;
				if (s_anchor > len)
					s_anchor = len;
				const bool shift = in.shiftDown;
				auto hasSel = [&]() { return s_anchor >= 0 && s_anchor != s_caret; };
				auto selLo = [&]() { return s_anchor < s_caret ? s_anchor : s_caret; };
				auto selHi = [&]() { return s_anchor < s_caret ? s_caret : s_anchor; };
				auto startSel = [&]() {
					if (s_anchor < 0)
						s_anchor = s_caret;
				};
				auto delSel = [&]() {
					const int32 lo = selLo(), n = selHi() - lo;
					for (int32 k = lo; k + n <= len; ++k)
						buf[k] = buf[k + n];
					len -= n;
					buf[len] = '\0';
					s_caret = lo;
					s_anchor = -1;
				};
				auto copySel = [&]() {
					char t2[600];
					int32 lo = selLo(), hi = selHi(), j = 0;
					for (int32 k = lo; k < hi && j < 599; ++k)
						t2[j++] = buf[k];
					t2[j] = '\0';
					ctx.SetClipboard(t2);
				};
				const NkVec2 mp = in.mousePos;
				const bool hit = (mp.x >= r.x && mp.x <= r.x + r.w && mp.y >= r.y && mp.y <= r.y + r.h);
				auto caretAtMouse = [&]() -> int32 {
					const float32 availW0 = r.w - pad * 2.f;
					char tp0[600];
					slice(s_caret, tp0);
					const float32 pw = measW(tp0);
					const float32 offc = (pw > availW0) ? (pw - availW0) : 0.f;
					const float32 target = mp.x - origineX() + offc;
					int32 best = 0;
					float32 bestd = 1e9f;
					char acc[600];
					int32 an = 0;
					for (int32 i = 0;; ++i) {
						acc[an] = '\0';
						const float32 wv = measW(acc);
						const float32 d = wv > target ? wv - target : target - wv;
						if (d < bestd) {
							bestd = d;
							best = i;
						}
						if (!buf[i] || an >= 599)
							break;
						acc[an++] = buf[i];
					}
					return best;
				};
				// Double-clic -> tout ; appui -> ancre ; glisser -> etend ; clic simple -> curseur + deselection.
				if (hit && in.mouseDoubleClicked[0]) {
					s_anchor = 0;
					s_caret = len;
					s_drag = false;
					s_blink = 0.f;
				} else if (hit && in.mouseClicked[0]) {
					const int32 c = caretAtMouse();
					s_caret = c;
					s_anchor = c;
					s_drag = true;
					s_blink = 0.f;
				} else if (s_drag && in.mouseDown[0]) {
					s_caret = caretAtMouse();
					s_blink = 0.f;
				}
				if (!in.mouseDown[0])
					s_drag = false;
				if (in.KeyPressedRepeat(NkGuiKey::Left)) {
					if (shift) {
						startSel();
						if (s_caret > 0)
							--s_caret;
					} else {
						if (hasSel())
							s_caret = selLo();
						else if (s_caret > 0)
							--s_caret;
						s_anchor = -1;
					}
					s_blink = 0.f;
				}
				if (in.KeyPressedRepeat(NkGuiKey::Right)) {
					if (shift) {
						startSel();
						if (s_caret < len)
							++s_caret;
					} else {
						if (hasSel())
							s_caret = selHi();
						else if (s_caret < len)
							++s_caret;
						s_anchor = -1;
					}
					s_blink = 0.f;
				}
				if (in.KeyPressed(NkGuiKey::Home)) {
					if (shift)
						startSel();
					else
						s_anchor = -1;
					s_caret = 0;
					s_blink = 0.f;
				}
				if (in.KeyPressed(NkGuiKey::End)) {
					if (shift)
						startSel();
					else
						s_anchor = -1;
					s_caret = len;
					s_blink = 0.f;
				}
				if (in.wantSelectAll) {
					s_anchor = 0;
					s_caret = len;
					in.wantSelectAll = false;
					s_blink = 0.f;
				}
				if (in.wantCopy) {
					if (hasSel())
						copySel();
					in.wantCopy = false;
				}
				if (in.wantCut) {
					if (hasSel()) {
						copySel();
						delSel();
					}
					in.wantCut = false;
					s_blink = 0.f;
				}
				if (in.KeyPressedRepeat(NkGuiKey::Backspace)) {
					if (hasSel())
						delSel();
					else if (s_caret > 0) {
						for (int32 k = s_caret - 1; k < len; ++k)
							buf[k] = buf[k + 1];
						--s_caret;
						--len;
					}
					s_blink = 0.f;
				}
				if (in.KeyPressedRepeat(NkGuiKey::Delete)) {
					if (hasSel())
						delSel();
					else if (s_caret < len) {
						for (int32 k = s_caret; k < len; ++k)
							buf[k] = buf[k + 1];
						--len;
					}
					s_blink = 0.f;
				}
				if (in.wantPaste) {
					if (hasSel())
						delSel();
					const NkString cb = ctx.GetClipboard();
					for (const char *s = cb.CStr(); *s; ++s) {
						if ((unsigned char)*s < 32 || len + 1 >= cap)
							continue;
						for (int32 k = len; k >= s_caret; --k)
							buf[k + 1] = buf[k];
						buf[s_caret] = *s;
						++s_caret;
						++len;
					}
					in.wantPaste = false;
					s_blink = 0.f;
				}
				for (int32 i = 0; i < in.charCount; ++i) {
					const uint32 cp = in.chars[i];
					if (cp < 32 || cp >= 127)
						continue;
					if (hasSel())
						delSel();
					if (len + 1 >= cap)
						break;
					for (int32 k = len; k >= s_caret; --k)
						buf[k + 1] = buf[k];
					buf[s_caret] = (char)cp;
					++s_caret;
					++len;
					// Taper EFFONDRE toujours la selection. Sans cette ligne,
					// l'ancre survivait a la frappe et une selection FANTOME
					// apparaissait.
					//
					// Scenario rapporte en beta : un double-clic — ou Ctrl+A —
					// sur un champ VIDE pose s_anchor = 0 et s_caret = len = 0.
					// Aucune selection visible, les deux valeurs etant egales.
					// Mais a la premiere lettre le curseur passe a 1 tandis que
					// l'ancre reste a 0 : la lettre se retrouve selectionnee, et
					// la frappe suivante l'efface. D'ou « la premiere lettre du
					// nom n'est jamais prise en compte » a la creation d'une
					// classe, d'une structure ou d'une union.
					s_anchor = -1;
					s_blink = 0.f;
				}
				if (s_caret > len)
					s_caret = len;
				if (s_anchor > len)
					s_anchor = len;
			}
			if (f) {
				const float32 availW = r.w - pad * 2.f;
				char tp[600];
				slice(focused ? s_caret : 0, tp);
				const float32 caretW = focused ? measW(tp) : 0.f;
				const float32 offX = (focused && caretW > availW) ? (caretW - availW) : 0.f;
				const float32 ox = origineX();
				const NkRect clip = {r.x + pad, r.y, r.w - pad * 2.f, r.h};
				dl.PushClipRect(clip, true);
				if (focused && s_anchor >= 0 && s_anchor != s_caret) { // surbrillance selection
					const int32 lo = s_anchor < s_caret ? s_anchor : s_caret,
								hi = s_anchor < s_caret ? s_caret : s_anchor;
					char a[600], b[600];
					slice(lo, a);
					slice(hi, b);
					const float32 xa = measW(a), xb = measW(b);
					dl.AddRectFilled({ox - offX + xa, r.y + 4.f, xb - xa, r.h - 8.f},
									 NkColor{46, 110, 190, 140});
				}
				// `AddTextScaled` retombe sur AddText a l'echelle ~1 (pixel-snap
				// conserve pour tous les champs historiques).
				dl.AddTextScaled(f->Face(), f->TexId(), {ox - offX, r.y + (r.h - lh) * 0.5f + asc},
								 buf[0] ? buf : "", st.texte, ech);
				if (focused) {
					const float32 phase = s_blink - (float32)(int64)s_blink;
					if (phase < 0.55f) {
						const float32 caretX = ox + (caretW - offX) + 1.f;
						const float32 caretH = lh > 8.f ? lh : (r.h - 10.f);
						dl.AddRectFilled({caretX, r.y + (r.h - caretH) * 0.5f, 1.5f, caretH},
										 NkColor{200, 210, 220, 255});
					}
				}
				dl.PopClipRect();
			}
		}

	} // namespace editorkit
} // namespace nkentseu
