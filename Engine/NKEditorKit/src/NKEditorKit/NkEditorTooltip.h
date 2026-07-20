#pragma once
// -----------------------------------------------------------------------------
// @File    NkEditorTooltip.h
// @Brief   Infobulle REUTILISABLE avec DELAI de survol (facon VSCode) : appeler
//          NkTooltip(ctx, hovered, "texte") chaque frame pour chaque widget qui
//          merite une bulle ; elle apparait apres ~0,45 s de survol continu et
//          disparait des que le survol cesse. S'appuie sur nkgui::SetTooltip
//          (couche overlay, jamais rognee) — ce fichier n'ajoute QUE la logique
//          de temporisation partagee, pas un nouveau rendu.
// @Author  Rihen
// @License Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
#include "NKGui/NKGui.h"

namespace nkentseu {
	namespace editorkit {

		using namespace nkentseu;
		using namespace nkentseu::nkgui;

		// Etat GLOBAL unique : un seul widget est survole a la fois, donc une seule
		// temporisation suffit pour toute l'application (cle = pointeur du texte,
		// stable pour les litteraux et les chaines NkT()).
		struct NkTooltipState {
				const void *owner = nullptr; // texte du widget en cours de survol
				float32 dwell = 0.f;		 // temps de survol accumule (s)
		};

		inline NkTooltipState &NkTooltipG() {
			static NkTooltipState s;
			return s;
		}

		// A appeler CHAQUE frame : `hovered` = le widget est sous la souris (et pas
		// masque par un popup — passer `hovered && ctx.popupDepth == 0` si besoin).
		// `delay` = temps de survol avant apparition (defaut 0,45 s).
		inline void NkTooltip(NkGuiContext &ctx, bool hovered, const char *text, float32 delay = 0.45f) {
			NkTooltipState &st = NkTooltipG();
			if (hovered && text && *text) {
				if (st.owner != (const void *)text) { // nouveau widget : repart de zero
					st.owner = (const void *)text;
					st.dwell = 0.f;
				}
				st.dwell += ctx.input.dt;
				if (st.dwell >= delay)
					SetTooltip(ctx, text);
			} else if (st.owner == (const void *)text) { // survol termine pour CE widget
				st.owner = nullptr;
				st.dwell = 0.f;
			}
		}

	} // namespace editorkit
} // namespace nkentseu
