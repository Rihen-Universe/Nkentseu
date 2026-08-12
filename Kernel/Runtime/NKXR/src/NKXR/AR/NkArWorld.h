//
// NkArWorld.h
// =============================================================================
// Description :
//   Le MONDE de l'AR : un repère commun, stable, indépendant de la caméra.
//   Les objets y sont posés une fois et y restent — la caméra peut se
//   déplacer, sortir, revenir : elle les retrouve à leur place.
//
// Le principe, sans SLAM :
//   Une pose de marqueur seule dit « où est le marqueur PAR RAPPORT À
//   L'OBJECTIF ». C'est insuffisant : si la caméra bouge, cette pose change
//   alors que le marqueur n'a pas bougé. On inverse donc le raisonnement.
//   Le PREMIER marqueur vu définit l'origine du monde. Voir un marqueur connu
//   donne alors la pose de la CAMÉRA dans ce monde — et non l'inverse. Les
//   objets, eux, sont rangés en coordonnées de MONDE : ils ne dépendent plus
//   de ce que voit l'objectif à cet instant.
//   Quand un marqueur INCONNU apparaît en même temps qu'un marqueur connu, on
//   déduit sa place dans le monde et on l'ajoute à la carte. De proche en
//   proche, la pièce entière devient repérable : on peut se déplacer d'un
//   marqueur à l'autre — de planète en planète — sans jamais perdre la scène.
//
// Ce que cela permet (et que le mode ancre simple ne permettait pas) :
//   - décoration d'intérieur : un meuble posé reste à SA place dans la pièce ;
//   - se déplacer autour d'une scène et la retrouver au retour ;
//   - caméra virtuelle de cinéma : la prise de vue réelle et la scène
//     virtuelle partagent un même repère, donc les deux tiennent ensemble.
//
// La limite, dite franchement : entre deux marqueurs visibles, la pose de la
// caméra n'est plus mesurée — on garde la dernière connue. Combler ces trous
// demande une centrale inertielle (étage 3, Android) puis du SLAM (recherche).
// Autrement dit : ce module rend le monde PERSISTANT, pas le suivi CONTINU.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKARWORLD_H__
#define __NKENTSEU_XR_NKARWORLD_H__

#include "NKXR/AR/NkArSession.h"
#include "NKXR/AR/NkArFlow.h"

namespace nkentseu {
	namespace xr {

		// ── Un marqueur situé dans le monde ──────────────────────────────────
		struct NkArMapEntry {
			int32 id = -1;
			NkXrPose poseInWorld{};   ///< Où est ce marqueur, dans le monde.
			bool isOrigin = false;    ///< Le premier vu : il DÉFINIT le monde.
			uint32 observations = 0;  ///< Combien de fois confirmé (confiance).
		};

		// ── Un objet posé dans le monde ──────────────────────────────────────
		struct NkArAnchor {
			uint32 handle = 0;
			NkXrPose poseInWorld{};
		};

		struct NkArWorldConfig {
			// Étendre la carte : un marqueur inconnu vu EN MÊME TEMPS qu'un
			// connu entre dans la carte. Sans cela, seul le premier marqueur
			// compte et l'on ne peut pas parcourir une pièce.
			bool extendMap = true;
			// Un marqueur vu de très loin ou de trop biais donne une pose
			// imprécise : l'inscrire dans la carte propagerait l'erreur à tout
			// ce qui sera posé ensuite. On exige une taille minimale à l'image.
			float32 minEdgePixelsToMap = 60.f;
			// Suivre la ROTATION de la caméra par l'image quand aucun marqueur
			// n'est visible. Sans cela, l'objet reste collé à l'écran dès qu'on
			// pivote — le défaut le plus voyant de l'AR par marqueurs. Ne couvre
			// PAS la translation (voir NkArFlow, qui l'explique).
			bool trackByImage = true;
			NkArFlowConfig flow;
			// Au-delà de ce résidu d'ajustement, on refuse le mouvement estimé :
			// la scène a changé (objet qui passe, forte parallaxe) plutôt que la
			// caméra. Mieux vaut figer que partir n'importe où.
			float32 maxFlowResidualPixels = 2.5f;
			// Images sans aucun repère AVANT de déclarer la pose perdue. 0 =
			// jamais. Passé ce délai, une pose entretenue à l'estime ne vaut
			// plus rien : l'afficher tromperait l'utilisateur.
			uint32 maxBlindFrames = 240;
		};

		// ── Le monde ─────────────────────────────────────────────────────────
		class NkArWorld {
			public:
				void Initialize(const NkArWorldConfig &config) { mConfig = config; }
				void Reset();

				// À appeler après chaque NkArSession::ProcessFrame : met à jour
				// la pose de la caméra dans le monde et étend la carte.
				// Rend true si la caméra est localisée à CETTE image.
				bool Update(const NkArSession &session);

				// Pose de la caméra dans le monde. « localisée à cette image »
				// se lit avec IsLocalizedNow() : une pose ancienne reste
				// utilisable, mais l'application doit savoir qu'elle vieillit.
				const NkXrPose &GetCameraPose() const { return mCameraInWorld; }
				bool IsLocalizedNow() const { return mLocalizedThisFrame; }
				bool HasEverLocalized() const { return mHasOrigin; }
				uint32 GetFramesSinceLocalized() const { return mFramesSinceLocalized; }

				// La caméra est suivie PAR L'IMAGE à cette image : aucun
				// marqueur en vue, mais sa rotation vient d'être mesurée. La
				// place affichée reste crédible en rotation, elle dérive en
				// translation — état à montrer, pas à taire.
				bool IsTrackingByImage() const { return mFlowThisFrame; }
				const NkArFlowResult &GetLastFlow() const { return mLastFlow; }
				// La pose est-elle encore utilisable ? Faux quand plus rien ne
				// l'entretient depuis trop longtemps : l'application doit alors
				// CACHER ses objets plutôt que d'afficher une place inventée.
				bool IsPoseUsable() const;

				// ── Objets posés dans le monde ───────────────────────────────
				// Poser DEVANT la caméra, à une distance donnée : le geste le
				// plus courant (« place ça ici »).
				uint32 PlaceInFrontOfCamera(float32 metersAhead);
				uint32 Place(const NkXrPose &poseInWorld);
				bool Remove(uint32 handle);
				void RemoveAll();

				// Pose d'un objet DANS LE REPÈRE CAMÉRA, pour le dessiner.
				// Rend false si la caméra n'a jamais été localisée.
				bool GetAnchorInCamera(uint32 handle, NkXrPose &outPose) const;

				// Transporter N'IMPORTE QUELLE pose du monde vers le repère de
				// l'objectif. C'est ce qu'il faut pour dessiner un marqueur de la
				// CARTE plutôt que sa détection : la détection n'existe que tant
				// qu'on voit le marqueur, la carte le garde. Un objet dessiné
				// depuis la carte ne disparaît donc pas quand on cache le
				// marqueur — il reste à sa place tant que la caméra sait où elle
				// est.
				bool ToCamera(const NkXrPose &poseInWorld, NkXrPose &outPose) const;

				// Un marqueur de la carte, par son identifiant.
				const NkArMapEntry *Find(int32 id) const { return FindMapEntry(id); }

				const NkVector<NkArAnchor> &GetAnchors() const { return mAnchors; }
				const NkVector<NkArMapEntry> &GetMap() const { return mMap; }

			private:
				const NkArMapEntry *FindMapEntry(int32 id) const;

				NkArWorldConfig mConfig{};
				NkVector<NkArMapEntry> mMap;
				NkVector<NkArAnchor> mAnchors;
				NkXrPose mCameraInWorld{};
				NkArImageFlow mFlow;
				NkArFlowResult mLastFlow{};
				bool mFlowThisFrame = false;
				uint32 mBlindFrames = 0;   ///< Images sans AUCUN repère (ni marqueur, ni image).
				bool mHasOrigin = false;
				bool mLocalizedThisFrame = false;
				uint32 mFramesSinceLocalized = 0;
				uint32 mNextHandle = 1;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKARWORLD_H__
