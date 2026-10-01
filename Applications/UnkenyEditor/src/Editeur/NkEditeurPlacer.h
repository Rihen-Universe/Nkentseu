//
// NkEditeurPlacer.h
// =============================================================================
// Description :
//   « Placer des acteurs » (document 02 §4, le panneau Place Actors d'UE5), les
//   FORMES 2D et les COLLISIONS dans l'editeur :
//     - le panneau a gauche : onglets verticaux a icone (Favoris, Recents,
//       Base, Lumieres, Formes, Effets, Volumes, Tout), recherche, liste a
//       icones ; CLIC = poser au centre de la vue, GLISSER vers la vue = poser
//       au point du lacher (sur une entite, un collisionneur s'AJOUTE a elle) ;
//     - les entrees « Formes », « Collisionneur », « Volumes » des menus
//       (+ Ajouter, Ajouter ici, Ajouter un composant) ;
//     - les blocs des Details : la forme, la collision (calque, sommets,
//       rotation, materiau, depuis la forme / le sprite), et les calques de
//       collision de l'onglet Monde ;
//     - les POIGNEES du collisionneur dans la vue (taille, rayon, sommets,
//       decalage, rotation).
//
// ⚠️ CE FICHIER EST A CE CHANTIER, PAR ACCORD (01/10)
//   Deux autres chantiers refont les Details, le Content Browser et le tiroir.
//   Ceux-ci n'appellent qu'UNE fonction d'ici chacun, ajoutee en fin de liste :
//   NkEditeurBlocForme / NkEditeurBlocCollision (Details), NkEditeurSectionCalques
//   (Monde), NkEditeurMenuFormes / NkEditeurMenuCollisions (menus),
//   NkEditeurActionPlacer (NkEditeurExecuter), les poignees (vue).
//
// LA PLAGE D'ACTIONS : 1500-1599 (NkEditeurInterface.h, NK_A_FORME...).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURPLACER_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURPLACER_H__

#include "Editeur/NkEditeurInterface.h"

namespace nkentseu {
	namespace editeur {

		/// Les onglets verticaux du panneau, dans l'ordre d'UE5.
		enum class NkOngletPlacer : uint8 {
			NK_FAVORIS = 0,
			NK_RECENTS,
			NK_BASE,
			NK_LUMIERES,
			NK_FORMES,
			NK_EFFETS,
			NK_VOLUMES,
			NK_TOUT,
			NK_COUNT
		};
		const char *NkNomOngletPlacer(NkOngletPlacer o) noexcept;

		/// Ce que pose un element du catalogue.
		enum class NkNaturePlacer : uint8 {
			NK_VIDE = 0,	///< acteur vide (transform + nom)
			NK_SPRITE,		///< l'entite simple de l'editeur : sprite + boite + corps
			NK_PERSONNAGE,	///< capsule debout, corps dynamique sans rotation, controleur
			NK_SON,			///< une source sonore (NkSource2D)
			NK_DECLENCHEUR, ///< une zone qui detecte sans repousser
			NK_SIM,			///< un acteur du catalogue de simulation (NkActeurSim)
			NK_LUMIERE,		///< + NkTypeLumiere2D
			NK_EFFET,		///< + NkPresetEffet2D
			NK_FORME,		///< + NkGenreForme2D
			NK_CARRE,		///< un rectangle de cotes egaux
			NK_VOLUME		///< volume bloquant : + NkCollisionEditeur
		};

		/// Les collisionneurs que l'editeur sait ajouter (menus, panneau, Details).
		enum class NkCollisionEditeur : uint8 {
			NK_BOITE = 0,
			NK_CERCLE,
			NK_CAPSULE,
			NK_POLYGONE,
			NK_DEPUIS_FORME,  ///< le collisionneur assorti de la forme 2D
			NK_DEPUIS_SPRITE, ///< l'enveloppe des pixels opaques de l'image
			NK_COUNT
		};
		const char *NkNomCollisionEditeur(NkCollisionEditeur k) noexcept;

		struct NkElementPlacer {
				const char *nom;
				const char *aide;
				NkNaturePlacer nature;
				int32 indice;	 ///< le type, le preset, le genre ou l'acteur, selon la nature
				uint32 onglets; ///< bit = NkOngletPlacer (Tout et Recents/Favoris en plus)
		};
		/// LE catalogue du panneau (et de la plage NK_A_PLACER).
		const NkElementPlacer *NkEditeurCataloguePlacer(int32 &nombre) noexcept;

		/// Pose l'element `k` du catalogue au point `monde` (annulable, Ctrl+Z) ;
		/// le choisit et le retient dans les Recents. Rend l'entite.
		ecs::NkEntityId NkEditeurPoserElement(NkEditeurModele &m, NkEditeurInterface *ui, int32 k, const NkVec2f &monde);
		/// Une forme 2D du genre `g` au point `monde`, avec son collisionneur assorti
		/// (decor : sans corps, ce que le sol attend). Annulable.
		ecs::NkEntityId NkEditeurPoserForme(NkEditeurModele &m, NkGenreForme2D g, const NkVec2f &monde, bool carre = false);
		/// Ajoute (ou remplace) la forme 2D de l'entite `id`.
		bool NkEditeurAjouterForme(NkEditeurModele &m, ecs::NkEntityId id, NkGenreForme2D g);
		/// Ajoute (ou REMPLACE) le collisionneur de `id` ; un corps rigide deja la
		/// est refait. Faux (et dit) si « depuis la forme » sans forme, « depuis le
		/// sprite » sans image. Annulable.
		bool NkEditeurAjouterCollision(NkEditeurModele &m, ecs::NkEntityId id, NkCollisionEditeur k);
		/// La forme de `id` a change : son collisionneur suit-il ? (collisionSuit)
		void NkEditeurFormeChangee(NkEditeurModele &m, ecs::NkEntityId id);

		// --- L'interface -----------------------------------------------------
		/// Le panneau, dans `ui.placer`.
		void NkEditeurDessinerPlacer(NkEditeurCadre &c);
		/// Les actions de la plage 1500-1599. Rend vrai si `action` en etait.
		bool NkEditeurActionPlacer(NkEditeurCadre &c, int32 action);
		/// Les entrees « Formes » d'un menu (base = NK_A_FORME, NK_A_FORME_ICI...).
		void NkEditeurMenuFormes(NkVector<NkEntreeMenu> &out, int32 base);
		/// Les entrees « Collisionneur » et « Forme 2D » de « Ajouter un composant ».
		void NkEditeurMenuCollisions(NkEditeurCadre &c, NkVector<NkEntreeMenu> &out);
		/// Les volumes (bloquants, declencheur) de « Ajouter ici ».
		void NkEditeurMenuVolumes(NkVector<NkEntreeMenu> &out, int32 base);

		/// Les Details : la forme de `id` (rien si elle n'en a pas).
		void NkEditeurBlocForme(NkEditeurCadre &c, ecs::NkEntityId id);
		/// Les Details : la collision de `id` (calque, sommets, rotation, materiau,
		/// generation, edition dans la vue). Rien sans collisionneur.
		void NkEditeurBlocCollision(NkEditeurCadre &c, ecs::NkEntityId id);
		/// L'onglet Monde : les calques de collision (noms et matrice).
		void NkEditeurSectionCalques(NkEditeurCadre &c);

		/// Les poignees du collisionneur de la selection dans la vue (si
		/// `ui.editionCollision`) : le dessin, puis la souris. Rend vrai si la
		/// souris leur revient (le viseur ne la voit pas).
		void NkEditeurDessinerPoigneesCollision(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl);
		bool NkEditeurPoigneesCollisionSouris(NkEditeurCadre &c, const nkgui::NkRect &aire);

		/// `--selftest` : le banc de l'editeur pour ce chantier (« FORMES EDITEUR »).
		int32 NkEditeurLancerBancFormes();

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURPLACER_H__
