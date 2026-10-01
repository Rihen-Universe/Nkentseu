//
// NogeDemoJeu.h
// =============================================================================
// Description :
//   Le « premier resultat » de Noge : UNE scene (un sol, un cube que le joueur
//   pousse, une balle qui lui tombe dessus, une camera, un soleil), jouee par
//   le VRAI chemin du moteur -- NkEngineLayer, ses systemes ECS, le pont
//   physique (NkPhysicsSystem), le pont de rendu (NkRenderSystem) et la carte
//   d'entree du noyau (NkInputMap, la meme qu'Unkeny).
//
//   Tout ce fichier est une fonction de l'ETAT : il ne connait ni fenetre ni
//   GPU. La fenetre (NogeDemoApp) et le banc (--selftest) appellent les MEMES
//   fonctions -- on ne mesure pas une scene pour en montrer une autre.
//
// Caracteristiques :
//   - Les entrees sont des ACTIONS (Deplacer, OrbiterSouris, OrbiterClavier,
//     Zoom, Sauter, Lacher, Sauver, Charger, Quitter), jamais des touches. Leur
//     texte par defaut est ecrit dans un fichier .nkinput que l'on peut
//     modifier (format de NkInputMap::Load).
//   - Le cube se deplace par la PHYSIQUE (sa vitesse est posee sur son
//     NkRigidbody3D, le pont la donne au corps) : il pousse la balle, il ne
//     traverse rien.
//   - Sauver / Charger passent par NkSceneSerializer (.nkscene, texte JSON).
//   - Les entites sont retrouvees par leur NOM : un rechargement rend d'autres
//     identifiants.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_NOGEDEMO_NOGEDEMOJEU_H__
#define __NKENTSEU_NOGEDEMO_NOGEDEMOJEU_H__

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"
#include "NKECS/World/NkWorld.h"
#include "NKEvent/NkInputMap.h"
#include "NKMath/NKMath.h"

namespace nkentseu {

	class NkPhysicsSystem;

	namespace nogedemo {

		// --- Les noms des entites de la scene ---------------------------------
		inline constexpr const char *kNomSol = "Sol";
		inline constexpr const char *kNomJoueur = "Joueur"; ///< le cube rouge que l'on pousse
		inline constexpr const char *kNomBalle = "Balle";	///< la balle jaune qui tombe
		inline constexpr const char *kNomCamera = "Camera";
		inline constexpr const char *kNomSoleil = "Soleil";
		inline constexpr const char *kPrefixeCaisse = "Caisse_"; ///< les caisses lachees (touche E)

		// --- Les grandeurs de la scene (le banc les lit, il ne les recopie pas)
		inline constexpr float32 kSolDemiLargeur = 10.f; ///< le sol fait 20 x 20 m
		inline constexpr float32 kSolEpaisseur = 1.f;	 ///< son dessus est a y = 0
		inline constexpr float32 kJoueurCote = 1.f;		 ///< repos : y = 0,5
		inline constexpr float32 kJoueurDepartY = 3.f;
		inline constexpr float32 kBalleRayon = 0.4f;
		inline constexpr float32 kBalleDepartY = 7.f; ///< a l'aplomb du joueur : elle lui tombe dessus
		inline constexpr int32 kCaissesMax = 12;

		/// Les couches de collision (bit = 1 << couche, cf. NkPhysicsSystem).
		inline constexpr uint32 kCoucheDecor = 0;
		inline constexpr uint32 kCoucheJoueur = 1;

		// --- Les actions --------------------------------------------------------
		struct NkDemoActions {
				NkInputActionId deplacer = NK_INPUT_ACTION_INVALID;		  ///< axe 2D : x = droite, y = avant
				NkInputActionId orbiterSouris = NK_INPUT_ACTION_INVALID;  ///< axe 2D : deplacement de souris (bouton droit)
				NkInputActionId orbiterClavier = NK_INPUT_ACTION_INVALID; ///< axe 2D : fleches, stick droit (une VITESSE)
				NkInputActionId zoom = NK_INPUT_ACTION_INVALID;			  ///< axe 1D : molette
				NkInputActionId sauter = NK_INPUT_ACTION_INVALID;
				NkInputActionId lacher = NK_INPUT_ACTION_INVALID;
				NkInputActionId sauver = NK_INPUT_ACTION_INVALID;
				NkInputActionId charger = NK_INPUT_ACTION_INVALID;
				NkInputActionId quitter = NK_INPUT_ACTION_INVALID;
		};

		/// La camera d'orbite : elle tourne autour d'un PIVOT, a une distance.
		/// Le lacet 0 la place sur +Z, regard vers -Z.
		struct NkDemoCameraOrbite {
				float32 lacetDeg = 35.f;
				float32 tangageDeg = 30.f;
				float32 distance = 10.f;
				math::NkVec3f pivot{0.f, 0.5f, 0.f};
		};

		/// Ce que le jeu garde d'une image a l'autre.
		struct NkDemoEtat {
				NkDemoActions actions;
				NkDemoCameraOrbite camera;
				int32 caissesLachees = 0;
				/// Demandes faites par les actions, EXECUTEES par l'hote (fichiers).
				bool demandeSauver = false;
				bool demandeCharger = false;
				bool demandeQuitter = false;
				/// Les reglages du jeu.
				float32 vitesseJoueur = 4.f;		///< m/s, sur le sol
				float32 impulsionSaut = 5.5f;		///< m/s, vers le haut
				float32 vitesseOrbiteDeg = 90.f;	///< deg/s au clavier
				float32 orbiteSourisDeg = 220.f;	///< deg par unite de l'axe (la souris est a l'echelle 0,005)
				float32 pasZoom = 1.5f;				///< m par cran de molette
		};

		/// Le texte d'entrees par defaut (format NkInputMap::Load). Les touches
		/// sont des POSITIONS : « Key:W » est le Z d'un clavier AZERTY.
		const char *NogeDemoTexteEntrees() noexcept;

		/// Charge `texte` dans la carte et retrouve les actions. false (et
		/// `erreur` rempli) si une ligne est refusee ou si une action manque.
		bool NogeDemoDeclarerEntrees(NkInputMap &carte, NkDemoEtat &etat, const char *texte,
									 NkString *erreur = nullptr);

		/// La scene de depart. Vide le monde AVANT (sans toucher a la physique :
		/// voir NogeDemoViderMonde).
		void NogeDemoConstruireScene(ecs::NkWorld &monde);

		/// L'entite qui porte ce nom (NkName), ou Invalid.
		ecs::NkEntityId NogeDemoTrouver(ecs::NkWorld &monde, const char *nom);

		/// Pose le transform de l'entite « Camera » d'apres l'orbite.
		void NogeDemoPoserCamera(ecs::NkWorld &monde, const NkDemoCameraOrbite &camera);

		/// Une image de jeu : lit les actions, tourne la camera, pousse le
		/// joueur, fait sauter, lache une caisse, note les demandes de fichier.
		/// A appeler APRES NkInputMap::Update et AVANT le pas physique suivant.
		void NogeDemoPas(NkDemoEtat &etat, ecs::NkWorld &monde, const NkInputMap &carte, float32 dt);

		/// Lache une caisse au-dessus du joueur. Rend Invalid au-dela de kCaissesMax.
		ecs::NkEntityId NogeDemoLacherCaisse(ecs::NkWorld &monde, NkDemoEtat &etat);

		/// Detruit toutes les entites. Si `physique` est donne, ses corps sont
		/// rendus d'abord (sinon ils resteraient dans le monde physique, sans
		/// entite, et continueraient de heurter).
		void NogeDemoViderMonde(ecs::NkWorld &monde, NkPhysicsSystem *physique);

		/// Ecrit la scene (NkSceneSerializer, texte JSON).
		bool NogeDemoSauver(ecs::NkWorld &monde, const char *chemin);

		/// Vide le monde (et la physique), relit le fichier, recale l'etat
		/// (nombre de caisses). false si le fichier est illisible : le monde est
		/// alors LAISSE TEL QUEL.
		bool NogeDemoCharger(ecs::NkWorld &monde, NkPhysicsSystem *physique, const char *chemin, NkDemoEtat &etat);

		/// Le nombre d'entites nommees (celles que la sauvegarde ecrit).
		int32 NogeDemoCompterNommees(ecs::NkWorld &monde);

	} // namespace nogedemo
} // namespace nkentseu

#endif // __NKENTSEU_NOGEDEMO_NOGEDEMOJEU_H__
