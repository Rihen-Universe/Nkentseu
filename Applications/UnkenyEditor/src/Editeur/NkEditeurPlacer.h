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
//   (les cartes Forme 2D et Collisionneur des Details, 2026-10-01), NkEditeurSectionCalques
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

		// (2026-10-01, R33) NkEditeurBlocForme / NkEditeurBlocCollision sont devenus
		// les CARTES Forme 2D et Collisionneur de NkEditeurDetails.cpp.
		/// L'onglet Monde : les calques de collision (noms et matrice).
		void NkEditeurSectionCalques(NkEditeurCadre &c);
		/// La fenetre « Reglages du projet : calques de collision » (Fenetre >),
		/// flottante au-dessus du corps, la meme section en plus grand.
		void NkEditeurDessinerReglagesCollision(NkEditeurCadre &c);

		/// Les poignees du collisionneur de la selection dans la vue (si
		/// `ui.editionCollision`) : le dessin, puis la souris. Rend vrai si la
		/// souris leur revient (le viseur ne la voit pas).
		void NkEditeurDessinerPoigneesCollision(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl);
		bool NkEditeurPoigneesCollisionSouris(NkEditeurCadre &c, const nkgui::NkRect &aire);

		// --- L'AIMANT (Rihen, 01/10 ; NkEditeurAimant.cpp) -------------------
		/// Ce a quoi l'aimant colle, par PRIORITE : un sommet, sinon une arete,
		/// sinon la face (un point entre dans un objet est ramene a sa surface).
		enum class NkGenreAimant : uint8 { NK_SOMMET = 0, NK_ARETE, NK_FACE };

		struct NkAccrocheAimant {
				bool trouve = false;
				NkGenreAimant genre = NkGenreAimant::NK_SOMMET;
				NkVec2f point{0.f, 0.f}; ///< le point de l'AUTRE objet ou l'on colle
				NkVec2f delta{0.f, 0.f}; ///< ce qu'il faut ajouter a la cible
				ecs::NkEntityId cible;	 ///< l'objet colle
		};

		/// L'aimant joue-t-il ? (le bouton de la barre flottante, ou V maintenue)
		bool NkEditeurAimantActif(const NkEditeurCadre &c);
		/// Un POINT (une poignee, un sommet) : le meilleur accroche dans `rayon` (m),
		/// hors de l'entite `exclue`. Faux s'il n'y a rien.
		bool NkEditeurAimanterPoint(NkEditeurModele &m, const NkVec2f &p, float32 rayon, ecs::NkEntityId exclue,
									NkAccrocheAimant &sortie);
		/// Un BLOC (sprite, forme, collisionneur) amene en `cible` (son centre) : ses
		/// coins et son centre cherchent un accroche ; `sortie.delta` corrige la cible.
		bool NkEditeurAimanterBloc(NkEditeurModele &m, ecs::NkEntityId bloc, const NkVec2f &cible, float32 rayon,
								   NkAccrocheAimant &sortie);
		/// La porte de la vue pour un DEPLACEMENT : la cible corrigee (et
		/// l'indicateur pose) si l'aimant joue, telle quelle sinon.
		NkVec2f NkEditeurAimanterDeplacement(NkEditeurCadre &c, const NkVec2f &cible);
		/// La meme pour un POINT (poignee) : le point colle, ou tel quel.
		NkVec2f NkEditeurAimanterPoignee(NkEditeurCadre &c, const NkVec2f &p, ecs::NkEntityId exclue);
		/// L'indicateur du point d'accroche (peint dans la vue, puis oublie).
		void NkEditeurDessinerAimant(NkEditeurCadre &c, nkgui::NkGuiDrawList &dl);

		/// `--exemple=formes` : une scene qui montre TOUTES les formes (decor, avec
		/// contour) et, au-dessus, des formes DYNAMIQUES qui tombent en Jouer.
		void NkEditeurSceneFormes(NkEditeurModele &m);
		/// Les options de demarrage de ce chantier (--placer=, --exemple=formes,
		/// --selection-forme=, --collision=editer). Rend vrai si `arg` en est une.
		bool NkEditeurOptionPlacer(NkEditeurInterface &ui, const NkString &arg);
		/// Applique, une fois la scene de depart prete, les demandes ci-dessus.
		void NkEditeurDemarrerPlacer(NkEditeurModele &m, NkEditeurInterface &ui);

		/// `--selftest` : le banc de l'editeur pour ce chantier (« FORMES EDITEUR »).
		int32 NkEditeurLancerBancFormes();
		/// `--captures-formes=DOSSIER` : des captures HORS ECRAN (trame rasterisee,
		/// sans fenetre ni GPU) des gestes qu'une capture de fenetre ne montre
		/// pas (l'aimant pendant un glisser). Rend 0 si tout est ecrit.
		int32 NkEditeurCapturesFormes(const char *dossier);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURPLACER_H__
