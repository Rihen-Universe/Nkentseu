// -----------------------------------------------------------------------------
// @File    NkGuiCibles.inl
// @Brief   La cible courante : le défaut compilé, et la surcharge d'aperçu.
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#ifndef NK_GUI_DOC_NKGUICIBLES_INL
#define NK_GUI_DOC_NKGUICIBLES_INL

namespace nkentseu {
	namespace nkgui {

		/// ⚠️ UNE VARIABLE DE FONCTION, PAS UNE GLOBALE DE FICHIER. Un en-tête inclus
		///    par plusieurs unités de compilation doit donner UNE SEULE cible : une
		///    globale par unité aurait rendu l'aperçu vrai dans l'une et faux dans
		///    l'autre, et le désaccord se serait vu en PIXELS, jamais dans un compteur.
		///    C'est exactement la forme que prennent déjà les registres des jetons et
		///    des icônes.
		inline NkGuiCible &NkGCibleMutable() noexcept {
			static NkGuiCible s = NkGuiCible::Toutes;
			return s;
		}

		inline NkGuiCible NkGuiCibleCourante() noexcept {
			const NkGuiCible c = NkGCibleMutable();
			// `Toutes` veut dire « personne n'a rien imposé » : on rend ce que la
			// compilation dit. Mettre le défaut compilé DANS la variable à
			// l'initialisation aurait marché aussi — et aurait rendu impossible de
			// distinguer « l'hôte a demandé Bureau » de « on tourne sur un PC ».
			if (c == NkGuiCible::Toutes)
				return NkGuiCibleCompilee();
			return c;
		}

		inline void NkGuiPoserCible(NkGuiCible c) noexcept {
			NkGCibleMutable() = c;
		}

	} // namespace nkgui
} // namespace nkentseu

#endif // NK_GUI_DOC_NKGUICIBLES_INL

// =============================================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================
