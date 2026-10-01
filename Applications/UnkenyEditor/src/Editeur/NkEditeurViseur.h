// =============================================================================
// NkEditeurViseur.h — le viseur : la scene, la grille, la selection
//
// ⚠️ CE QUE LE VISEUR MONTRE EN PLUS DE LA SCENE, ET POURQUOI
//   L'aire d'APPAREIL SIMULE et sa ZONE SURE sont dessinees par-dessus. Sans
//   elles, l'editeur montre une scene sur un ecran de bureau et ne dit rien du
//   telephone — or c'est precisement la que les defauts se paient.
//   Ce depot a mesure onze defauts sur GemCrush dont SEPT etaient
//   structurellement invisibles depuis la machine de developpement.
// =============================================================================
#pragma once

#include "Editeur/NkEditeurAppareils.h"
#include "NKGui/Core/NkGuiContext.h"
#include "Unkeny/Unkeny.h"

namespace nkentseu {
	namespace editeur {

		using namespace nkentseu::unkeny;

		/// Dessine le viseur en entier. Rend les mesures du rendu, pour que le
		/// pied de page les affiche : sans compteur, « est-ce que le hors-champ
		/// fonctionne » n'a pas de reponse.
		struct NkEditeurModele;
		/// Dessine la scene du modele dans le viseur : formes, sprites, matiere,
		/// selection, zone sure de l'appareil simule. Il DESSINE, il ne modifie rien.
		NkStatsRendu NkDessinerViseur(nkgui::NkGuiDrawList &dl, NkEditeurModele &m, const nkgui::NkRect &viseur,
									  const nkgui::NkRect &appareil);

		/// L'aire d'APPAREIL SIMULE, a l'interieur du viseur.
		///
		/// Elle garde le RAPPORT largeur/hauteur du profil, centree, avec une
		/// marge. C'est ce rapport qui permet de juger une mise en page mobile
		/// depuis un ecran de bureau -- l'etirer la rendrait mensongere.
		///
		/// ⚠️ Fonction LIBRE, et non plus une methode d'un objet de disposition :
		/// avec NKEditorKit, le viseur est un panneau ancre qui apprend son
		/// rectangle au moment ou il se dessine. Il n'y a plus de disposition
		/// calculee d'avance a qui poser la question.
		///
		/// (2026-10-01) `avecCadre` : la place du CADRE (bordure, boutons, pied)
		/// est reservee, tournee avec l'appareil. Rend toujours l'ECRAN seul.
		nkgui::NkRect NkAireAppareil(const nkgui::NkRect &viseur, const NkProfilAppareil &profil,
									 bool avecCadre = false) noexcept;

		/// L'ECRAN DU JEU dans l'editeur (document 03, §2.5) : la zone sure de
		/// l'appareil simule (NkLayoutSimule), sur son ecran dans le viseur, posee
		/// dans m.scene (NkScene::PoserEcran). Le bureau : le viseur, sans marge.
		void NkEditeurPoserEcranDuJeu(NkEditeurModele &m, const nkgui::NkRect &viseur, const nkgui::NkRect &appareil);

		/// Le cadre de l'appareil simule, en vecteurs (NkEditeurCadreAppareil.cpp) :
		/// bordure, coins, boutons, decoupe, zone sure, marge conseillee, selon
		/// les interrupteurs de `r`. `ecran` vient de NkAireAppareil ; ce qui est
		/// hors de l'appareil, dans `viseur`, est voile quand le cadre est montre.
		void NkDessinerAppareil(nkgui::NkGuiDrawList &dl, const NkProfilAppareil &p, const nkgui::NkRect &ecran,
								const NkReglagesAppareil &r, const nkgui::NkRect &viseur);

		// Ce qui est sous le curseur : NkEditeurPrendreSous (NkEditeurActions.h),
		// qui suit l'ordre de dessin de NkDessinerViseur (30/09). L'ancien
		// NkEntiteSous, boite droite des seuls sprites, n'existe plus que dans
		// le banc, pour la contre-epreuve du temoin e41.

	} // namespace editeur
} // namespace nkentseu
