#pragma once
// =============================================================================
// NkRootPicker.h — Sélecteur de dossier MAISON, déplaçable (fenêtre modale
// légère) réutilisant NkOpenWsPanel(pickFolder=true). Piloté par NkCodeState :
// reqPickFolder -> ouvre ; le dossier choisi est reposé dans pickedFolder.
// Sert « Ajouter un dossier au workspace » (multi-racines) sans dialogue natif.
// =============================================================================
#include "NKEditorKit/NkEditorKit.h"
#include "NKCode/Shell/NkUi.h"
#include "NKCode/Shell/NkOpenWs.h"
#include "NKCode/Shell/NkI18n.h"
#include "NKCode/Project/NkCodeState.h"

namespace nkentseu {
	namespace nkcode {

		using namespace nkentseu::editorkit;

		struct NkRootPickerState {
				bool open = false;
				NkRect win{0.f, 0.f, 0.f, 0.f};
				bool drag = false;
				NkVec2 dragOff{0.f, 0.f};
				NkOpenWsState ow; // état de navigation propre au picker
		};

		inline NkRootPickerState &NkRootPicker() {
			static NkRootPickerState s;
			return s;
		}

		// À appeler CHAQUE frame (overlay). Ouvre sur st->reqPickFolder, dessine la
		// fenêtre déplaçable, repose le dossier choisi dans st->pickedFolder.
		inline void NkDrawRootPicker(NkEditorFrameContext &ec, NkRootPickerState *rp, NkCodeState *st,
									 NkCodeDialogs *dlg, const NkIcons &ic) {
			if (!rp || !st)
				return;
			if (st->reqPickFolder && !rp->open) { // demande -> ouvre, centre, réinit navigation
				st->reqPickFolder = false;
				rp->open = true;
				rp->ow.curDir[0] = 0; // force EnsureInit à repartir de la racine courante
				rp->ow.scanned = false;
				const float32 VW = static_cast<float32>(ec.Ui().viewW), VH = static_cast<float32>(ec.Ui().viewH);
				rp->win = {VW * 0.5f - 420.f, VH * 0.5f - 300.f, 840.f, 600.f};
			}
			if (!rp->open)
				return;

			const NkUi u = NkUi::From(ec, /*overlay=*/true);
			auto &in = ec.Ui().input;
			const NkVec2 m = in.mousePos;

			// Fond assombri (backdrop) : clic dessus = annule.
			u.Rect({0.f, 0.f, static_cast<float32>(ec.Ui().viewW), static_cast<float32>(ec.Ui().viewH)},
				   NkColor{0, 0, 0, 120});

			// ── Barre de titre DÉPLAÇABLE ──
			const float32 titleH = u.s(34);
			const NkRect title = {rp->win.x, rp->win.y, rp->win.w, titleH};
			u.Rect(rp->win, NkCol::background);
			u.Rect(title, NkCol::surface);
			u.Rect({rp->win.x, rp->win.y, rp->win.w, 2.f}, NkCol::primary); // liseré accent
			u.Text(rp->win.x + u.s(14), rp->win.y + (titleH - u.Lh()) * 0.5f, NkT("exp.addroot"), NkCol::foreground);
			// bouton ✕
			const NkRect xr = {rp->win.x + rp->win.w - titleH, rp->win.y, titleH, titleH};
			const bool xh = m.x >= xr.x && m.x < xr.x + xr.w && m.y >= xr.y && m.y < xr.y + xr.h;
			u.Text(xr.x + u.s(11), rp->win.y + (titleH - u.Lh()) * 0.5f, "X", xh ? NkColor{230, 90, 90, 255}
																				  : NkCol::mutedFg);
			if (xh && in.mouseClicked[0])
				rp->open = false;
			// drag par la barre (hors bouton ✕)
			const bool onTitle = m.x >= title.x && m.x < xr.x && m.y >= title.y && m.y < title.y + title.h;
			if (in.mouseClicked[0] && onTitle) {
				rp->drag = true;
				rp->dragOff = {m.x - rp->win.x, m.y - rp->win.y};
			}
			if (rp->drag && in.mouseDown[0]) {
				rp->win.x = m.x - rp->dragOff.x;
				rp->win.y = m.y - rp->dragOff.y;
			}
			if (!in.mouseDown[0])
				rp->drag = false;

			// ── Contenu : notre explorateur de dossiers en mode « choisir un dossier » ──
			const NkRect body = {rp->win.x, rp->win.y + titleH, rp->win.w, rp->win.h - titleH};
			const int32 res = NkOpenWsPanel(u, body, &rp->ow, st, dlg, ec.dt, ic, /*pickFolder=*/true);
			if (res == 2) { // « Choisir ce dossier »
				st->pickedFolder = NkString(rp->ow.curDir);
				rp->open = false;
			} else if (res == 1) // Annuler
				rp->open = false;

			// Échap ferme.
			if (in.KeyPressed(nkgui::NkGuiKey::Escape))
				rp->open = false;

			// Étanchéité : neutraliser SEULEMENT les clics/molette pour les panneaux
			// dessous (JAMAIS mousePos — piège du gel d'input). L'overlay lui-même a
			// déjà lu l'input réel ci-dessus.
			for (int32 b = 0; b < 3; ++b) {
				in.mouseClicked[b] = false;
				in.mouseDown[b] = false;
				in.mouseDoubleClicked[b] = false;
			}
			in.wheel = in.wheelH = 0.f;
			in.charCount = 0;
		}

	} // namespace nkcode
} // namespace nkentseu
