//
// NkEditeurMaillage.h
// =============================================================================
// Description :
//   LE MAILLAGE 2D DANS L'EDITEUR (2026-10-02, R31). Rihen, 01/10 : « un
//   composant de maillage 2D (mesh renderer / mesh editing) avec SA FENETRE
//   D'EDITION : editer les sommets, decouper en parties, et donner une physique
//   independante a chaque partie si on le souhaite ».
//
//   - CREER : « Ajouter un composant > Maillage 2D » (depuis le sprite, depuis la
//     forme, ou vide), et le clic droit sur une entite a sprite (« Creer un
//     maillage 2D depuis le sprite »). Le sprite (ou la forme) est alors
//     MASQUE, pas retire : le maillage dessine la meme image, et Ctrl+Z rend
//     tout.
//   - LA FENETRE D'EDITION est un ONGLET DE DOCUMENT (NkEditeurDocuments.h,
//     genre NK_MAILLAGE), a la maniere de l'editeur de sprite d'Unity et de
//     l'editeur de maillage statique d'UE5 : la texture en apercu, les sommets
//     et les aretes, les parties colorees ; selectionner (clic, Maj+clic, cadre),
//     deplacer (avec l'aimant : sommets, aretes, grille), ajouter, supprimer,
//     couper une arete (double-clic), trianguler, refaire a une densite,
//     FAIRE UNE PARTIE (P) ; a droite, les parties, leur physique (aucune,
//     rigide, molle), leurs liens (rigide, elastique, pivot ; rupture).
//   - TOUT GESTE EST RETENU (NkEditeurRetenir) : Ctrl+Z / Ctrl+Y, dans la page
//     comme dans la scene -- le maillage est un composant de la scene.
//   - L'ASSET : « Enregistrer comme asset » ecrit Contenu/Maillages/<nom>.nkmesh2d
//     (Unkeny/Maillage/NkUnkenyMaillageFichier.h) ; « Utiliser » en pose un sur
//     l'entite ; un double-clic sur un .nkmesh2d du Contenu le pose dans la scene.
//
// Tout ce qui MODIFIE est une fonction du modele (NkEditeurActions.h, regle de
// l'editeur), eprouvee par --selftest (NkEditeurBancMaillage.cpp) ; la page ne
// fait que les appeler (NkEditeurPageMaillage.cpp).
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_UNKENYEDITOR_NKEDITEURMAILLAGE_H__
#define __NKENTSEU_UNKENYEDITOR_NKEDITEURMAILLAGE_H__

#include "Editeur/NkEditeurModele.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKGui/Core/NkGuiTypes.h"
#include "Unkeny/Maillage/NkUnkenyMaillage.h"
#include "Unkeny/Squelette/NkUnkenySquelette.h"

namespace nkentseu {
	namespace editeur {

		struct NkEditeurCadre;
		struct NkEditeurInterface;

		/// D'ou vient un maillage cree.
		enum class NkSourceMaillage : uint8 {
			NK_AUTO = 0, ///< le sprite s'il y en a un, sinon la forme, sinon vide
			NK_SPRITE,
			NK_FORME,
			NK_VIDE
		};

		/// Les ACTIONS du maillage (NkEditeurExecuter) : la plage 2450-2499,
		/// « LIBRES » au 02/10 (NkEditeurInterface.h).
		enum NkActionMaillage : int32 {
			NK_A_MAILLAGE = 2450,			   ///< + NkSourceMaillage : un maillage sur la selection
			NK_A_MAILLAGE_EDITER = 2460,	   ///< la fenetre d'edition du maillage de la selection
			NK_A_MAILLAGE_ASSET = 2461,		   ///< « Enregistrer comme asset » (le maillage de la selection)
			NK_A_MAILLAGE_UTILISER = 2470,	   ///< + k (< 20) : l'asset maillagesProposes[k] sur la selection
			NK_A_MAILLAGE_FIN = 2499
		};

		/// L'outil de la page.
		enum class NkOutilMaillage : uint8 {
			NK_SELECTION = 0, ///< clic : choisir ; glisser : deplacer (ou cadre de choix)
			NK_AJOUTER,		  ///< clic : un sommet
			// (2026-10-02, R30) Le SQUELETTE 2D (NkEditeurSquelette.h) : AJOUTES A LA FIN.
			NK_OS,	  ///< B : poser (glisser), chainer, deplacer la tete, tirer la queue (le REPOS)
			NK_POIDS, ///< W : peindre les poids de l'os choisi (Maj : retirer), auto-poids
			NK_POSE,  ///< R : tourner les os (la POSE, pas le repos)
			NK_IK	  ///< I : tirer une main / un pied : chaine a deux os, sens du coude
		};
		/// Un outil du squelette ?
		inline bool NkOutilSquelette(NkOutilMaillage o) noexcept {
			return o == NkOutilMaillage::NK_OS || o == NkOutilMaillage::NK_POIDS || o == NkOutilMaillage::NK_POSE || o == NkOutilMaillage::NK_IK;
		}

		/// Les boutons du panneau du squelette (le banc et les captures y visent).
		enum class NkBoutonSquelette : uint8 {
			NK_HUMANOIDE = 0,
			NK_QUADRUPEDE,
			NK_OISEAU,
			NK_CREATURE,
			NK_PROFIL,
			NK_SYMETRIE,
			NK_SUPPRIMER,
			NK_CHALEUR,
			NK_DISTANCE,
			NK_PARTIES,
			NK_NORMALISER,
			NK_REPOS,
			NK_COUDE_AUTO,
			NK_COUDE_GAUCHE,
			NK_COUDE_DROITE,
			NK_EMPLACEMENT,
			NK_ATTACHE,
			NK_CHAINE,
			NK_ASSET,
			NK_COUNT
		};

		/// Les boutons de la barre de la page (le banc et les captures y visent).
		enum class NkBoutonMaillage : uint8 {
			NK_SELECTION = 0,
			NK_AJOUTER,
			NK_TRIANGULER,
			NK_DENSITE_MOINS,
			NK_DENSITE_PLUS,
			NK_REFAIRE,
			NK_FAIRE_PARTIE,
			NK_SUPPRIMER,
			NK_TEXTURE,
			NK_TRIANGLES,
			NK_ASSET,
			NK_CADRER,
			NK_UV, ///< « UV collees » : deplacer un sommet garde la texture a sa place
			// (R30) Les outils du squelette.
			NK_OS,
			NK_POIDS,
			NK_POSE,
			NK_IK,
			NK_COUNT
		};

		/// Un document ouvert : le maillage d'UNE entite, par son identite.
		struct NkDocMaillage {
				nk_uint64 id = 0;
				uint64 cibleUid = 0; ///< NkIdentite2D : Ctrl+Z et Jouer changent les poignees, pas elle
				NkOutilMaillage outil = NkOutilMaillage::NK_SELECTION;
				/// Un octet par sommet (les doubles d'une couture se choisissent ensemble).
				uint8 choisis[unkeny::NK_MAILLAGE2D_SOMMETS_MAX] = {};
				int32 partie = 0; ///< la partie montree a droite
				int32 lien = -1;  ///< le lien montre a droite
				int32 densite = 6;
				bool texture = true;   ///< l'apercu de la texture
				bool triangles = true; ///< les aretes
				/// « UV COLLEES » (defaut) : deplacer un sommet change la GEOMETRIE, la
				/// texture reste a sa place (ses UV suivent, lues dans le maillage d'avant
				/// le geste) -- l'edition de maillage d'Unity et de Spine. Decoche : le
				/// sommet emporte son bout d'image (la texture se DEFORME).
				bool uvCollees = true;
				// --- La vue de la page (en metres) --------------------------------
				NkVec2f centre{0.f, 0.f};
				float32 zoom = 120.f; ///< pixels par metre
				bool cadre = false;	  ///< cadree une fois
				// --- Le geste en cours ----------------------------------------------
				int32 geste = 0; ///< 0 rien, 1 deplacer, 2 cadre de choix, 3 panoramique
				NkVec2f appui{0.f, 0.f};   ///< en pixels
				NkVec2f dernier{0.f, 0.f}; ///< en metres (repere du maillage)
				bool retenu = false;	   ///< le geste a ete retenu (une fois par geste)
				/// Les positions au debut du deplacement (l'aimant les lit), et les UV.
				NkVec2f depart[unkeny::NK_MAILLAGE2D_SOMMETS_MAX] = {};
				NkVec2f departUV[unkeny::NK_MAILLAGE2D_SOMMETS_MAX] = {};
				NkVec2f saisi{0.f, 0.f}; ///< le point saisi (repere du maillage)
				bool aimantVu = false;
				NkVec2f aimantPoint{0.f, 0.f};
				// --- Releves de la derniere trame (banc, captures) -------------------
				nkgui::NkRect vue{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect panneau{0.f, 0.f, 0.f, 0.f};
				nkgui::NkRect boutons[static_cast<uint32>(NkBoutonMaillage::NK_COUNT)] = {};
				/// Les rangees du panneau : une par partie (clic = la choisir).
				nkgui::NkRect rangeesParties[unkeny::NK_MAILLAGE2D_PARTIES_MAX] = {};
				/// Les trois choix de physique de la partie montree.
				nkgui::NkRect physique[3] = {};
				NkString annonce;
				// --- (2026-10-02, R30) LE SQUELETTE -----------------------------------
				int32 os = -1;			  ///< l'os choisi (panneau, poids, symetrie)
				int32 emplacement = -1;	  ///< l'emplacement montre
				float32 pinceauRayon = 0.15f; ///< m
				float32 pinceauForce = 0.35f; ///< par coup [0,1]
				bool pinceauRetirer = false;
				int32 coude = -1; ///< IK : -1 garde le sens, 0 a droite, 1 a gauche
				/// Le geste d'os en cours : 0 rien, 1 poser (glisser), 2 tete, 3 queue,
				/// 4 tourner (pose), 5 IK, 6 peindre, 7 deplacer la racine (pose).
				int32 osGeste = 0;
				uint32 osSaisi = 0;
				bool osParLeBout = false;
				bool coudeGeste = true; ///< le sens du coude retenu au debut d'un geste d'IK
				NkVec2f osAppui{0.f, 0.f}; ///< repere du maillage
				NkVec2f osTete{0.f, 0.f};
				NkVec2f osQueue{0.f, 0.f};
				NkVec2f pinceauDernier{0.f, 0.f};
				char nomOs[16] = {};
				int32 nomOsPour = -2; ///< l'os dont `nomOs` est le nom (rafraichi au changement)
				nkgui::NkRect rangeesOs[unkeny::NK_SQUELETTE2D_OS_MAX] = {};
				nkgui::NkRect boutonsSquelette[static_cast<uint32>(NkBoutonSquelette::NK_COUNT)] = {};
		};

		/// L'etat des pages de maillage : un membre de NkEditeurInterface.
		struct NkPagesMaillage {
				NkVector<NkDocMaillage> docs;
				nk_uint64 prochainId = 1;
		};

		// --- Le modele : CREER, et les gestes (tous RETENUS : Ctrl+Z) -----------------
		/// Un maillage sur `id` (le remplace s'il y en a un). NK_SPRITE : le contour de
		/// l'alpha de son image (texture comprise), le sprite MASQUE ; NK_FORME : la
		/// forme, MASQUEE ; NK_VIDE : aucun sommet (on les pose dans la page).
		bool NkEditeurCreerMaillage(NkEditeurModele &m, ecs::NkEntityId id, NkSourceMaillage source, int32 densite = 6);
		/// Les gestes de la page, sur le maillage de `id` (faux : pas de maillage, ou refus).
		bool NkEditeurMaillageDeplacer(NkEditeurModele &m, ecs::NkEntityId id, const uint8 *choisis, const NkVec2f &delta);
		int32 NkEditeurMaillageAjouter(NkEditeurModele &m, ecs::NkEntityId id, const NkVec2f &local);
		uint32 NkEditeurMaillageSupprimer(NkEditeurModele &m, ecs::NkEntityId id, const uint8 *choisis);
		int32 NkEditeurMaillageCouper(NkEditeurModele &m, ecs::NkEntityId id, uint32 a, uint32 b);
		bool NkEditeurMaillageTrianguler(NkEditeurModele &m, ecs::NkEntityId id);
		bool NkEditeurMaillageRefaire(NkEditeurModele &m, ecs::NkEntityId id, int32 densite);
		int32 NkEditeurMaillageFairePartie(NkEditeurModele &m, ecs::NkEntityId id, const uint8 *choisis);
		bool NkEditeurMaillageRetirerPartie(NkEditeurModele &m, ecs::NkEntityId id, uint32 k);
		/// La physique de la partie `k` (et son type de corps, pour un rigide).
		bool NkEditeurMaillagePhysique(NkEditeurModele &m, ecs::NkEntityId id, uint32 k, unkeny::NkPhysiquePartie2D p);
		int32 NkEditeurMaillageLier(NkEditeurModele &m, ecs::NkEntityId id, uint32 a, uint32 b, unkeny::NkGenreLienParties2D g);
		bool NkEditeurMaillageDelier(NkEditeurModele &m, ecs::NkEntityId id, uint32 k);
		/// « Enregistrer comme asset » : Contenu/Maillages/<nom de l'entite>.nkmesh2d (un
		/// nom libre). Rend le chemin ecrit (vide : echec). La source du composant le retient.
		NkString NkEditeurMaillageEnregistrerAsset(NkEditeurModele &m, ecs::NkEntityId id);
		/// Pose l'asset `chemin` (absolu, ou relatif au Contenu) sur `id` (RETENU).
		bool NkEditeurMaillageUtiliserAsset(NkEditeurModele &m, ecs::NkEntityId id, const char *chemin);
		/// Une entite neuve portant l'asset, posee en `position` (le double-clic du Contenu).
		ecs::NkEntityId NkEditeurPoserMaillageAsset(NkEditeurModele &m, const char *chemin, const NkVec2f &position);

		// --- La page --------------------------------------------------------------
		/// Ouvre (ou montre) la fenetre d'edition du maillage de `id` dans un onglet de
		/// document. Faux si `id` n'a pas de maillage.
		bool NkEditeurOuvrirMaillage(NkEditeurModele &m, NkEditeurInterface &ui, ecs::NkEntityId id);
		/// Le document au premier plan, ou nul.
		NkDocMaillage *NkEditeurDocMaillageActif(NkEditeurInterface &ui);
		NkDocMaillage *NkEditeurDocMaillage(NkEditeurInterface &ui, nk_uint64 id);
		/// L'entite du document (invalide si elle n'existe plus, ou n'a plus de maillage).
		ecs::NkEntityId NkEditeurCibleMaillage(NkEditeurModele &m, const NkDocMaillage &d);
		bool NkEditeurDetruireDocMaillage(NkEditeurInterface &ui, nk_uint64 id);
		NkString NkEditeurLibelleMaillage(NkEditeurModele &m, const NkDocMaillage &d);
		/// La page du document actif dans le corps. Faux : ce n'en est pas une.
		bool NkEditeurDessinerPageMaillage(NkEditeurCadre &c);
		/// Le clavier quand la page est au premier plan. Vrai = la touche est prise.
		bool NkEditeurPageMaillageAuClavier(NkEditeurCadre &c);
		/// Les actions NK_A_MAILLAGE* (NkEditeurExecuter). Faux si ce n'en est pas une.
		bool NkEditeurActionMaillage(NkEditeurCadre &c, int32 action);
		/// Les points de la page : repere du maillage <-> pixels de sa vue.
		NkVec2f NkEditeurMaillageVersEcran(const NkDocMaillage &d, const NkVec2f &local);
		NkVec2f NkEditeurMaillageDepuisEcran(const NkDocMaillage &d, const NkVec2f &ecran);

		// --- Banc et captures (NkEditeurBancMaillage.cpp) --------------------------
		int32 NkEditeurLancerBancMaillage();
		/// `--captures-maillage=DOSSIER` : les captures HORS ECRAN. 0 = toutes ecrites.
		int32 NkEditeurCapturesMaillage(const char *dossier);

	} // namespace editeur
} // namespace nkentseu

#endif // __NKENTSEU_UNKENYEDITOR_NKEDITEURMAILLAGE_H__
