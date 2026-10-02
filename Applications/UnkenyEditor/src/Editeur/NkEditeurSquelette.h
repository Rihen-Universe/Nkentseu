//
// NkEditeurSquelette.h
// =============================================================================
// Description :
//   LE SQUELETTE 2D DANS L'EDITEUR (2026-10-02, R30). Le plan valide par Rihen :
//   « editeur d'os dans la fenetre du maillage (poser, chainer, renommer,
//   longueur/angle, symetrie G/D) ; mode Poids (peindre, normaliser, auto-poids
//   par distance/chaleur) ; poses ; IK 2D (tirer une main/un pied : chaine a
//   deux os + cible, avec sens du coude) ; animer les os sur la FRISE partagee
//   et melanger dans l'Animateur ».
//
//   - CREER : « Ajouter un composant > Squelette 2D (Humanoide, Quadrupede,
//     Oiseau, Creature libre, vide) » ; le modele est mis a la boite du maillage
//     (cree depuis le sprite s'il manque) et les poids sont poses par la
//     CHALEUR. La carte « Squelette 2D » des Details ouvre la fenetre.
//   - LA FENETRE est celle du maillage (NkEditeurPageMaillage.cpp), quatre
//     outils de plus : Os (B), Poids (W), Pose (R), IK (I).
//   - TOUT GESTE EST RETENU : Ctrl+Z / Ctrl+Y, dans la page comme dans la scene.
//   - L'ASSET : « Enregistrer le squelette » ecrit Contenu/Squelettes/<nom>.nkskel
//     (le meme fichier que NkAnimaEditor ouvre : NKAnima/Skeleton/NkSkeleton2D.h).
//
// Tout ce qui MODIFIE est une fonction du modele, eprouvee par --selftest
// (NkEditeurBancSquelette.cpp) ; la page ne fait que l'appeler.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURSQUELETTE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURSQUELETTE_H__

#include "Editeur/NkEditeurModele.h"
#include "NKContainers/String/NkString.h"
#include "Unkeny/Squelette/NkUnkenySquelette.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;

		/// Les ACTIONS du squelette (NkEditeurExecuter) : la plage 2550-2599.
		enum NkActionSquelette : int32 {
			NK_A_SQUELETTE = 2550,		   ///< + NkSkeleton2DTemplate (0..4) : un squelette de ce modele sur la selection
			NK_A_SQUELETTE_VIDE = 2559,	   ///< un squelette vide (les os se posent dans la fenetre)
			NK_A_SQUELETTE_EDITER = 2560,  ///< la fenetre du maillage de la selection, outil Os
			NK_A_SQUELETTE_ASSET = 2561,   ///< « Enregistrer le squelette » (.nkskel)
			NK_A_SQUELETTE_REPOS = 2562,   ///< la pose revient au repos
			NK_A_SQUELETTE_FIN = 2599
		};

		// --- Le modele : CREER, et les gestes (tous RETENUS : Ctrl+Z) -----------------
		/// Un squelette sur `id` (le remplace). `modele` : un NkSkeleton2DTemplate, ou -1
		/// (vide). Sans maillage, il est cree depuis le sprite (ou la forme) d'abord.
		/// Les poids sont poses par la chaleur.
		bool NkEditeurCreerSquelette(NkEditeurModele &m, ecs::NkEntityId id, int32 modele);
		int32 NkEditeurSqueletteAjouterOs(NkEditeurModele &m, ecs::NkEntityId id, int32 parent, const NkVec2f &tete, const NkVec2f &queue,
										  const char *nom = nullptr);
		bool NkEditeurSqueletteReposOs(NkEditeurModele &m, ecs::NkEntityId id, uint32 j, const NkVec2f &tete, const NkVec2f &queue);
		bool NkEditeurSqueletteRetirerOs(NkEditeurModele &m, ecs::NkEntityId id, uint32 j);
		bool NkEditeurSqueletteRenommer(NkEditeurModele &m, ecs::NkEntityId id, uint32 j, const char *nom);
		uint32 NkEditeurSqueletteSymetrie(NkEditeurModele &m, ecs::NkEntityId id, uint32 j);
		bool NkEditeurSqueletteModele(NkEditeurModele &m, ecs::NkEntityId id, anim::NkSkeleton2DTemplate t);
		/// La pose revient au repos.
		bool NkEditeurSqueletteRepos(NkEditeurModele &m, ecs::NkEntityId id);
		/// L'IK de l'editeur : l'os `saisi` est TIRE vers `cible` (repere de l'entite).
		/// `parLeBout` : c'est sa QUEUE qu'on tire (chaine = lui et son parent), sinon sa
		/// TETE (chaine = son parent et son grand-parent). `coude` : -1 garde le sens
		/// actuel, 0 a droite, 1 a gauche. Rend vrai si la cible est atteinte.
		bool NkEditeurSqueletteIK(NkEditeurModele &m, ecs::NkEntityId id, uint32 saisi, bool parLeBout, const NkVec2f &cible, int32 coude);
		/// La pose de l'os `j` tourne a l'angle LOCAL `angle` (radians).
		bool NkEditeurSqueletteTourner(NkEditeurModele &m, ecs::NkEntityId id, uint32 j, float32 angle);
		/// Les auto-poids : 0 chaleur, 1 distance, 2 une partie = un os.
		bool NkEditeurSqueletteAutoPoids(NkEditeurModele &m, ecs::NkEntityId id, int32 methode);
		bool NkEditeurSqueletteNormaliser(NkEditeurModele &m, ecs::NkEntityId id);
		/// UN coup de pinceau (retenu si `retenir` : le premier d'un trait).
		uint32 NkEditeurSquelettePinceau(NkEditeurModele &m, ecs::NkEntityId id, uint32 os, const NkVec2f &centre, float32 rayon, float32 force,
										 bool retirer, bool retenir);
		int32 NkEditeurSqueletteEmplacement(NkEditeurModele &m, ecs::NkEntityId id, uint32 os, int32 partie, const char *nom = nullptr);
		bool NkEditeurSqueletteAttache(NkEditeurModele &m, ecs::NkEntityId id, uint32 emplacement, int32 partie);
		int32 NkEditeurSqueletteChaine(NkEditeurModele &m, ecs::NkEntityId id, uint32 premier, uint32 nombre);
		bool NkEditeurSqueletteRetirerChaine(NkEditeurModele &m, ecs::NkEntityId id, uint32 k);
		/// « Enregistrer le squelette » : Contenu/Squelettes/<nom de l'entite>.nkskel. Rend
		/// le chemin ecrit (vide : echec) ; la source du composant le retient.
		NkString NkEditeurSqueletteEnregistrerAsset(NkEditeurModele &m, ecs::NkEntityId id);
		/// Pose le .nkskel `chemin` sur `id` (RETENU).
		bool NkEditeurSqueletteUtiliserAsset(NkEditeurModele &m, ecs::NkEntityId id, const char *chemin);
		/// La chaine d'IK d'un os saisi : l'os du milieu et l'effecteur (repere local du milieu).
		bool NkEditeurChaineIK(const unkeny::NkSquelette2D &s, uint32 saisi, bool parLeBout, uint32 &mid, NkVec2f &effecteur);
		/// Les os du squelette de l'entite dessines dans la VUE de la scene (selection).
		void NkEditeurDessinerOsScene(NkEditeurCadre &c);

		/// Les actions NK_A_SQUELETTE* (NkEditeurExecuter). Faux si ce n'en est pas une.
		bool NkEditeurActionSquelette(NkEditeurCadre &c, int32 action);

		// --- Banc et captures (NkEditeurBancSquelette.cpp) --------------------------
		int32 NkEditeurLancerBancSquelette();
		/// `--captures-squelette=DOSSIER` : les captures HORS ECRAN. 0 = toutes ecrites.
		int32 NkEditeurCapturesSquelette(const char *dossier);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURSQUELETTE_H__
