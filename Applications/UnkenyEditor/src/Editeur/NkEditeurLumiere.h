//
// NkEditeurLumiere.h
// =============================================================================
// Description :
//   Tout ce que l'editeur sait de l'ECLAIRAGE 2D et des EFFETS (2026-09-30) :
//   les actions (poser une lumiere, appliquer un preset, regler une portee,
//   la scene de nuit d'exemple), le dessin dans le viseur (effets, carte de
//   lumiere, icones, cercle de portee et sa poignee) et les sections des
//   Details et de l'onglet Monde.
//
// Caracteristiques :
//   - UN SEUL FICHIER A DEPLACER. Un autre chantier refait l'inspecteur (les
//     Details a la facon de Unity), le navigateur de contenu et l'Outliner :
//     les sections ci-dessous sont des fonctions LIBRES, appelees depuis
//     NkEditeurDetails.cpp par trois lignes, et depuis le viseur par deux.
//     Refaire l'inspecteur, c'est deplacer ces appels, rien de plus.
//   - Chaque geste est une fonction du MODELE, eprouvee par
//     NkEditeurLancerBancLumiere (--selftest), comme le reste de l'editeur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURLUMIERE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURLUMIERE_H__

#include "Editeur/NkEditeurModele.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;

		// --- Actions (sans interface, eprouvees par le banc) ------------------
		/// Une entite NOUVELLE portant une lumiere du type donne, en `monde`.
		/// Selectionnee. L'eclairage de la scene n'est PAS allume pour autant
		/// (il est facultatif) : l'annonce le dit, et la section l'offre.
		ecs::NkEntityId NkEditeurPoserLumiere(NkEditeurModele &m, const NkVec2f &monde, NkTypeLumiere2D type);
		/// Une entite NOUVELLE portant un emetteur au preset donne. Selectionnee.
		ecs::NkEntityId NkEditeurPoserEffet(NkEditeurModele &m, const NkVec2f &monde, NkPresetEffet2D preset);
		/// Donne a une entite existante une lumiere du type donne (« + Ajouter »).
		bool NkEditeurAjouterLumiere(NkEditeurModele &m, ecs::NkEntityId id, NkTypeLumiere2D type);
		/// Donne a une entite existante un emetteur au preset donne.
		bool NkEditeurAjouterEffet(NkEditeurModele &m, ecs::NkEntityId id, NkPresetEffet2D preset);
		/// Remplace les reglages d'un emetteur par ceux d'un preset. La GRAINE et
		/// l'etat actif restent : changer de recette ne change pas de hasard.
		bool NkEditeurAppliquerPreset(NkEditeurModele &m, ecs::NkEntityId id, NkPresetEffet2D preset);
		/// La portee d'une lumiere (ou de la lumiere liee d'un emetteur), bornee a
		/// [0,1 ; 100] m. C'est ce que fait la poignee du cercle de portee.
		bool NkEditeurPoserPortee(NkEditeurModele &m, ecs::NkEntityId id, float32 portee);
		/// La portee affichee pour `id`, ou 0 s'il ne porte pas de lumiere.
		float32 NkEditeurPortee(NkEditeurModele &m, ecs::NkEntityId id);
		/// Remplace la scene par la NUIT d'exemple : sol, rochers, arbre, un feu
		/// de camp qui eclaire (flammes, fumee, etincelles), une lanterne, un
		/// reverbere en cone et le clair de lune. Eclairage ACTIF.
		void NkEditeurSceneNuit(NkEditeurModele &m);
		/// L'entite dont l'ICONE (lumiere, emetteur) est sous `monde`, dans un
		/// rayon de `rayonPx` pixels. Les icones ne se montrent qu'hors jeu.
		bool NkEditeurIconeSous(NkEditeurModele &m, const NkVec2f &monde, float32 rayonPx, ecs::NkEntityId &sortie);

		// --- Dessin dans le viseur --------------------------------------------
		/// Les chiffres de l'image que NkDessinerPartie (Unkeny/Partie) vient de
		/// poser : c'est elle qui dessine effets et lumiere depuis la fusion avec
		/// la livraison (2026-09-30), pour que le jeu construit les ait aussi.
		void NkEditeurRetenirChiffresLumiere(const NkStatsEclairage2D &st, int32 particulesDessinees);
		/// Les icones des lumieres et emetteurs, et le cercle de portee de la
		/// selection (avec sa poignee). Surcouche d'editeur : jamais eclairee.
		void NkEditeurDessinerIconesLumiere(nkgui::NkGuiDrawList &dl, NkEditeurModele &m);
		/// La poignee de portee et la souris. Rend true si elle a PRIS le geste.
		bool NkEditeurGizmoPorteeSouris(NkEditeurCadre &c, const nkgui::NkRect &aire);

		/// Les chiffres de la derniere image (barre d'etat, onglet Monde).
		struct NkEditeurChiffresLumiere {
				int32 lumieres = 0;
				int32 ignorees = 0;
				int32 occulteurs = 0;
				int32 mailles = 0;
				int32 points = 0;
				int32 particulesDessinees = 0;
		};
		const NkEditeurChiffresLumiere &NkEditeurDerniersChiffresLumiere() noexcept;

		// --- Les sections (Details, Monde) -------------------------------------
		void NkEditeurSectionLumiere(NkEditeurCadre &c, ecs::NkEntityId id);
		void NkEditeurSectionEmetteur(NkEditeurCadre &c, ecs::NkEntityId id);
		void NkEditeurSectionEclairageMonde(NkEditeurCadre &c);

		/// Le banc de l'eclairage et des effets COTE EDITEUR (x1..), compte a part.
		int32 NkEditeurLancerBancLumiere();

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURLUMIERE_H__
