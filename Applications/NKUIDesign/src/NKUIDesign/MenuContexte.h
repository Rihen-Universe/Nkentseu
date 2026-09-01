#pragma once
// -----------------------------------------------------------------------------
// @File    MenuContexte.h
// @Brief   LE MENU DU CLIC DROIT (Lunacy), en MÉCANISME : ce qu'il propose, ce
//          qui agit, et la RAISON de ce qui n'agit pas.
// @Author  Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// =============================================================================
//  ⚠️ ÉCRIT AVANT LES DEUX CHEMINS QU'IL SERT, PAS APRÈS LE PREMIER QUI MORD
// =============================================================================
//  Porte du dépôt du 28/08 : *« demander quels chemins FRÈRES partagent le même
//  danger, et l'écrire une fois pour tous. »* Ici les frères sont **la TOILE et
//  la HIÉRARCHIE** — troisième fois que ce couple revient sur ce chantier après
//  les tables de clic et l'englobant, donc on ne recommence pas à écrire deux
//  fois.
//
//  ⚠️ ET LA DIFFÉRENCE ENTRE LES DEUX SURFACES EST **L'APPLICABILITÉ, JAMAIS LE
//     JEU D'ENTRÉES**. Lunacy montre le même menu depuis sa toile et depuis son
//     panneau Layers ; ce qui change, c'est ce qui est grisé. Deux jeux
//     d'entrées auraient dérivé au premier ajout — et l'utilisateur aurait
//     cherché dans un menu une commande qu'il venait de voir dans l'autre.
//
//  ⚠️ **UNE ENTRÉE QUI N'AGIT PAS PORTE SA RAISON, ET C'EST MESURÉ.** C'est la
//     règle de la maison appliquée au menu : un contrôle qui ne fait rien sans
//     rien dire est le défaut qu'on chasse. Ici l'oubli est *mécaniquement*
//     détectable — la recette parcourt les entrées et exige : qui n'agit pas a
//     une raison non vide ; qui agit n'en a **pas** (sinon la raison serait un
//     ornement qu'on cesse de lire).
//
//  Ce fichier ne connaît ni la souris, ni l'écran, ni NKGui : il se mesure.
// -----------------------------------------------------------------------------
#include "Document.h"

namespace nkuidesign {

	using nkentseu::int32;
	using nkentseu::uint32;

	/// CE QU'UNE ENTRÉE DÉCLENCHE. L'index d'affichage ne sert **jamais** de
	/// clé : il change dès qu'on insère une entrée, et l'appelant exécuterait
	/// alors le geste voisin. C'est l'action qui voyage, pas la position.
	enum class NkActionCtx {
		EditerTexte,
		IA,
		CollerParDessus,
		CopierComme,
		Dupliquer,
		EnvoyerDerriere,
		Grouper,
		Degrouper,
		CadrerSelection,
		AjouterAgencement,
		Renommer,
		Copier,
		Couper,
		Coller,
		Supprimer,
		NB ///< le compte, lu par la recette — jamais une liste écrite à la main
	};

	/// Une entrée construite : ce qui s'affiche, ce qui agit, et pourquoi pas.
	struct NkEntreeCtx {
			char libelle[80] = {}; ///< le texte final, RAISON INCLUSE quand il y en a une
			const char *raccourci = nullptr;
			const char *raison = nullptr; ///< non vide **si et seulement si** `!agit`
			bool agit = false;
			bool sousMenu = false;
			bool sepApres = false; ///< un trait sous cette entrée (les groupes de Lunacy)
			NkActionCtx action = NkActionCtx::EditerTexte;
	};

	/// L'état qui décide de l'applicabilité. Rien d'autre n'entre ici : ni
	/// pointeur d'interface, ni position de souris.
	struct NkContexteCtx {
			bool aTexte = false;			 ///< le nœud porte une clé `texte`
			bool estCadre = false;			 ///< `forme = frame` (une page)
			bool aEnfants = false;
			bool pasRacine = false;
			bool pressePapiersPlein = false;
			/// ⚠️ LA SURFACE NE CHANGE PAS LE JEU D'ENTRÉES, seulement deux
			///    applicabilités : dans une LISTE, renommer agit toujours (c'est
			///    le geste natif d'un arbre) et l'édition de texte n'a pas de
			///    point d'insertion. Dans la TOILE, c'est l'inverse.
			bool surfaceListe = false;
	};

	enum { kMaxEntreesCtx = 24 };
	struct NkMenuCtx {
			NkEntreeCtx items[kMaxEntreesCtx];
			uint32 n = 0;
	};

	namespace detail {
		inline void NkCopierCtx(char *dst, uint32 cap, const char *a, const char *b) {
			uint32 k = 0;
			for (const char *p = a; p && *p && k + 1 < cap; ++p)
				dst[k++] = *p;
			for (const char *p = b; p && *p && k + 1 < cap; ++p)
				dst[k++] = *p;
			dst[k] = 0;
		}
	} // namespace detail

	/// LE CONSTRUCTEUR. Une seule écriture pour les deux surfaces.
	///
	/// L'ordre et les groupes suivent `lunacy_clicdroit_1_084617.png`, relu à la
	/// source : IA / Coller par-dessus / Copier comme / Dupliquer — trait —
	/// Envoyer derrière — trait — Grouper / Cadrer — trait — Agencement /
	/// Renommer — trait — le presse-papiers et Supprimer.
	///
	/// ⚠️ DEUX ENTRÉES DE LUNACY SONT **VOLONTAIREMENT ABSENTES**, et c'est
	///    écrit ici pour que personne ne les « oublie » par accident : **Crop**
	///    (Ctrl+Entrée) et **Rasterize Selection**. Elles supposent une image
	///    matricielle, que ce document n'a pas — un `.nkuidoc` décrit des nœuds,
	///    pas des pixels. Les poser grisées serait promettre un chantier qui n'a
	///    pas lieu d'être ; les inventer serait pire. *Ce qui n'a pas de sens
	///    chez nous se note, il ne s'ébauche pas.*
	inline void NkConstruireMenuCtx(const NkContexteCtx &c, NkMenuCtx &out) {
		out.n = 0;
		auto ajouter = [&](const char *libelle, const char *raccourci, bool agit, const char *raison,
						   bool sousMenu, bool sepApres, NkActionCtx action) {
			if (out.n >= (uint32)kMaxEntreesCtx)
				return;
			NkEntreeCtx &e = out.items[out.n++];
			// ⚠️ LA RAISON EST DANS LE LIBELLÉ, pas dans une infobulle : une
			//    infobulle demande de survoler, donc de deviner qu'il y a
			//    quelque chose à survoler. Un menu se lit d'un coup d'œil.
			detail::NkCopierCtx(e.libelle, sizeof(e.libelle), libelle, agit ? nullptr : raison);
			e.raccourci = raccourci;
			e.raison = agit ? nullptr : raison;
			e.agit = agit;
			e.sousMenu = sousMenu;
			e.sepApres = sepApres;
			e.action = action;
		};

		// ── 1. Éditer, et c'est notre entrée la plus utilisée ────────────────
		const bool editable = c.aTexte && !c.surfaceListe;
		ajouter("Éditer le texte", nullptr, editable, c.surfaceListe
													  ? " (dans la toile)"
													  : " (pas de texte ici)",
				false, true, NkActionCtx::EditerTexte);

		// ── 2. Le groupe « IA / presse-papiers riche » de Lunacy ─────────────
		ajouter("IA", nullptr, false, " (à construire — le panneau IA porte la génération)", true,
				false, NkActionCtx::IA);
		ajouter("Coller à la même place", nullptr, false, " (à construire)", false, false,
				NkActionCtx::CollerParDessus);
		ajouter("Copier comme", nullptr, false, " (SVG/PNG/JSON/CSS — à construire)", true, false,
				NkActionCtx::CopierComme);
		ajouter("Dupliquer", "Ctrl+D", c.pasRacine, " (pas la racine)", false, true,
				NkActionCtx::Dupliquer);

		// ── 3. L'ordre de profondeur — un groupe à lui seul chez Lunacy ──────
		ajouter("Envoyer derrière", "Ctrl+Maj+[", false, " (l'ordre de profondeur à construire)",
				false, true, NkActionCtx::EnvoyerDerriere);

		// ── 4. Grouper / dégrouper / cadrer ──────────────────────────────────
		ajouter("Grouper la sélection", "Ctrl+G", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Grouper);
		ajouter("Dégrouper", "Ctrl+Maj+G", c.pasRacine && c.aEnfants && !c.estCadre,
				" (pas un groupe)", false, false, NkActionCtx::Degrouper);
		ajouter("Cadrer la sélection", "Ctrl+Alt+G", false, " (à construire)", false, true,
				NkActionCtx::CadrerSelection);

		// ── 5. Agencement et renommage ───────────────────────────────────────
		// ⚠️ L'AGENCEMENT EXISTE, mais dans l'Inspecteur : la raison DIT OÙ, au
		//    lieu de dire seulement « non ». Une raison qui indique la porte
		//    vaut mieux qu'une raison qui la ferme.
		ajouter("Ajouter un agencement", "Maj+A", false,
				" (par l'Inspecteur, section Disposition)", false, false,
				NkActionCtx::AjouterAgencement);
		// Renommer : natif dans une LISTE, réservé aux pages dans la toile (une
		// étiquette d'artboard est la seule chose qui porte un nom visible là).
		ajouter("Renommer", "F2", c.surfaceListe ? c.pasRacine : c.estCadre,
				c.surfaceListe ? " (pas la racine)" : " (par la Hiérarchie)", false, true,
				NkActionCtx::Renommer);

		// ── 6. Le presse-papiers, et la suppression en dernier ───────────────
		ajouter("Copier", "Ctrl+C", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Copier);
		ajouter("Couper", "Ctrl+X", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Couper);
		ajouter("Coller", "Ctrl+V", c.pressePapiersPlein, " (presse-papiers vide)", false, false,
				NkActionCtx::Coller);
		ajouter("Supprimer", "Suppr", c.pasRacine, " (pas la racine)", false, false,
				NkActionCtx::Supprimer);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	//  LA RANGÉE D'ICÔNES — les gestes fréquents en accès direct (Lunacy)
	// ═══════════════════════════════════════════════════════════════════════════
	/// ⚠️ SEPT ICÔNES, RELUES SUR LA RÉFÉRENCE ZOOMÉE : coller, dupliquer,
	///    couper, cadenas, œil, poubelle, composant. **Trois n'ont aucun champ
	///    dans le modèle** (verrouiller, masquer, créer un composant) : elles
	///    sont **présentes et grisées**, et leur infobulle DIT pourquoi. Les
	///    retirer aurait été plus simple et faux — l'utilisateur qui connaît
	///    Lunacy les cherche, et une absence ne se justifie pas toute seule.
	enum class NkIconeCtx {
		Coller,
		Dupliquer,
		Couper,
		Verrouiller,
		Masquer,
		Supprimer,
		CreerComposant,
		NB
	};

	/// L'infobulle. **Jamais vide**, dans les deux états — c'est ce que la
	/// recette vérifie. Une icône muette est un bouton qu'on n'ose pas presser.
	inline const char *NkInfobulleIconeCtx(NkIconeCtx i, bool agit) {
		switch (i) {
			case NkIconeCtx::Coller:
				return agit ? "Coller (Ctrl+V)" : "Coller — le presse-papiers est vide";
			case NkIconeCtx::Dupliquer:
				return agit ? "Dupliquer (Ctrl+D)" : "Dupliquer — rien de duplicable ici";
			case NkIconeCtx::Couper:
				return agit ? "Couper (Ctrl+X)" : "Couper — rien à couper ici";
			case NkIconeCtx::Verrouiller:
				return agit ? "Verrouiller" : "Verrouiller — aucun verrou dans le modèle (à construire)";
			case NkIconeCtx::Masquer:
				return agit ? "Masquer" : "Masquer — aucune visibilité par nœud dans le modèle (à construire)";
			case NkIconeCtx::Supprimer:
				return agit ? "Supprimer (Suppr)" : "Supprimer — pas la racine";
			default:
				return agit ? "Créer un composant" : "Créer un composant — le registre ne reçoit pas encore (à construire)";
		}
	}

	/// Ce qu'une icône déclenche quand elle agit — `NkActionCtx::NB` = aucune,
	/// et c'est précisément le cas des trois qui n'existent pas encore.
	inline NkActionCtx NkActionIconeCtx(NkIconeCtx i) {
		switch (i) {
			case NkIconeCtx::Coller: return NkActionCtx::Coller;
			case NkIconeCtx::Dupliquer: return NkActionCtx::Dupliquer;
			case NkIconeCtx::Couper: return NkActionCtx::Couper;
			case NkIconeCtx::Supprimer: return NkActionCtx::Supprimer;
			default: return NkActionCtx::NB;
		}
	}

	/// L'applicabilité d'une icône, dans le même contexte que le menu.
	inline bool NkIconeCtxAgit(NkIconeCtx i, const NkContexteCtx &c) {
		switch (i) {
			case NkIconeCtx::Coller: return c.pressePapiersPlein;
			case NkIconeCtx::Dupliquer:
			case NkIconeCtx::Couper:
			case NkIconeCtx::Supprimer: return c.pasRacine;
			default: return false; // verrou, visibilité, registre : rien dans le modèle
		}
	}

} // namespace nkuidesign
