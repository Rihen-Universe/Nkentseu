//
// NkArFlow.h
// =============================================================================
// Description :
//   Mesurer le MOUVEMENT DE LA CAMÉRA à partir de l'image seule, quand plus
//   aucun marqueur n'est visible.
//
// Pourquoi ce fichier existe :
//   Sans marqueur en vue, la carte du monde garde la dernière pose connue de
//   la caméra. Tant que la caméra ne bouge pas, c'est juste. Dès qu'elle
//   tourne, c'est faux — et cela se voit immédiatement : l'objet virtuel reste
//   collé à l'écran au lieu de sortir du champ. Défaut constaté à l'essai, et
//   il ruine l'illusion plus sûrement qu'une erreur de quelques centimètres.
//   Or l'image, elle, sait que la caméra a bougé : tout son contenu glisse. Il
//   suffit de mesurer ce glissement.
//
// Le principe, et ses limites — dites franchement :
//   On suit quelques dizaines de points saillants d'une image à l'autre, puis
//   on ajuste le seul modèle honnête à notre portée : une ROTATION pure de la
//   caméra (lacet, tangage, roulis). C'est exact quand la caméra pivote, ce
//   qui est le geste le plus courant et celui qui trahit le plus le défaut.
//   Une TRANSLATION, elle, n'est pas mesurable ainsi : deux points à des
//   profondeurs différentes ne glissent pas de la même quantité (parallaxe),
//   et démêler cela exige de connaître la profondeur — c'est le métier du
//   SLAM, que nous n'avons pas. Traduction pratique : tourner sur soi est
//   suivi, marcher ne l'est pas. Le résultat porte donc son propre indice de
//   confiance, à afficher plutôt qu'à masquer.
//
// Ce que ce module n'est PAS : un suivi visuel complet. Il ne construit pas de
// carte, ne se ferme pas en boucle, et dérive lentement — chaque estimation
// s'ajoute à la précédente. Il COMBLE le trou entre deux marqueurs ; il ne le
// remplace pas.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKARFLOW_H__
#define __NKENTSEU_XR_NKARFLOW_H__

#include "NKXR/AR/NkArMarker.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace xr {

		// ── Le mouvement estimé entre deux images ────────────────────────────
		struct NkArFlowResult {
			bool valid = false;      ///< false = trop peu de points fiables : ne rien conclure.
			float32 yawRad = 0.f;    ///< Rotation autour de +Y (haut) : tourner à gauche > 0.
			float32 pitchRad = 0.f;  ///< Rotation autour de +X (droite) : lever les yeux > 0.
			float32 rollRad = 0.f;   ///< Rotation autour de +Z (arrière) : pencher la tête.
			uint32 inliers = 0;      ///< Points ayant voté pour ce mouvement.
			uint32 candidates = 0;   ///< Points assez texturés pour être suivis.
			uint32 ambiguous = 0;    ///< Points ÉCARTÉS faute d'un pic net (voir plus bas).
			float32 residualPixels = 0.f; ///< Écart moyen restant : la qualité de l'ajustement.
			// Glissement médian observé, en pixels. Grand ET mal ajusté = la
			// scène a probablement changé (objet qui passe, lumière) plutôt que
			// la caméra : l'appelant peut refuser d'y croire.
			float32 medianShiftPixels = 0.f;
		};

		struct NkArFlowConfig {
			uint32 cellsX = 10;         ///< Grille de sélection : un point fort par case,
			uint32 cellsY = 8;          ///< pour couvrir l'image au lieu de s'agglutiner.
			uint32 patchRadius = 5;     ///< Demi-côté de la vignette comparée (11×11).
			// Déplacement maximal cherché, en pixels. À 30 images/s et 550 px de
			// focale, 40 px valent 4° d'un coup — soit un balayage à 120°/s. En
			// deçà, un mouvement vif sort de la fenêtre et le suivi s'accroche à
			// un mauvais minimum : il croit alors que RIEN n'a bougé, ce qui est
			// pire que d'avouer son ignorance.
			uint32 searchRadius = 40;
			// Pas du balayage grossier. Le pic reste trouvable car la vignette
			// est plus large que le pas ; l'affinage rattrape le reste.
			uint32 coarseStep = 3;
			// Relief minimal pour qu'une vignette soit suivie. Bas exprès : une
			// pièce ordinaire (murs clairs, lumière plate) n'offre pas beaucoup
			// mieux, et un seuil sévère laisserait le suivi sans aucun point.
			uint32 minGradient = 1200;
			// MAIS un vote « rien n'a bougé » exige BEAUCOUP plus de relief.
			// Asymétrie voulue, et voici pourquoi : un mur uni, un ciel, une zone
			// surexposée ne contiennent plus que du bruit et les blocs de
			// compression de la webcam — lesquels sont accrochés à la GRILLE DE
			// PIXELS et non à la scène. Une vignette prise là se recolle donc
			// parfaitement à sa PROPRE place, quoi qu'ait fait la caméra. Ces
			// faux votes se concentrent tous au même endroit (zéro) et forment
			// une majorité qui écrase les vrais. Les faux votes « ça a bougé »,
			// eux, sont dispersés et le rejet des intrus s'en charge. On se méfie
			// donc de l'immobilité, pas du mouvement. Constaté sur les captures.
			uint32 minGradientForStillVote = 2600;
			uint32 stillRadiusPixels = 2; ///< En deçà, le vote compte comme « immobile ».
			uint32 minInliers = 8;      ///< En dessous, on préfère ne rien dire.
			float32 inlierPixels = 4.f; ///< Tolérance autour du mouvement médian.
			// Le meilleur accord doit être NETTEMENT meilleur que le meilleur
			// accord concurrent situé ailleurs. Sans ce test, une vignette qui se
			// recolle presque aussi bien à dix endroits vote quand même — et comme
			// ces votes-là se concentrent sur « rien n'a bougé », ils forment une
			// fausse majorité qui fait rejeter les vrais points en mouvement.
			float32 ambiguityRatio = 0.75f;
			uint32 peakRadius = 4;      ///< « Ailleurs » = à plus de tant de pixels du pic.
		};

		// ── L'estimateur ─────────────────────────────────────────────────────
		// Il garde l'image précédente : appeler Track() à chaque image, même
		// quand un marqueur est visible, sinon la comparaison sauterait
		// plusieurs images d'un coup et le glissement sortirait du rayon de
		// recherche — un suivi qui ne s'entretient pas est un suivi qui ment
		// au moment précis où l'on a besoin de lui.
		class NkArImageFlow {
			public:
				void Initialize(const NkArFlowConfig &config) { mConfig = config; }
				void Reset();

				NkArFlowResult Track(const uint8 *gray, uint32 width, uint32 height,
									 const NkArCameraIntrinsics &intrinsics);

				const NkArFlowConfig &GetConfig() const { return mConfig; }

			private:
				NkArFlowConfig mConfig{};
				NkVector<uint8> mPrev;
				uint32 mWidth = 0;
				uint32 mHeight = 0;
				bool mHasPrev = false;
		};

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKARFLOW_H__
