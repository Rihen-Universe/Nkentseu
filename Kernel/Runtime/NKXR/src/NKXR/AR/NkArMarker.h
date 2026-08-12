//
// NkArMarker.h
// =============================================================================
// Description :
//   Détection de marqueurs plans et estimation de pose 6DoF, ÉCRITES DE ZÉRO :
//   seuillage, suivi de contours, réduction en quadrilatères, redressement par
//   homographie, lecture du code, puis pose depuis l'homographie.
//   C'est la voie d'accès à l'AR que la mission juge réaliste : le SLAM est un
//   projet de recherche, un marqueur imprimé est de la vision classique.
//
// Caractéristiques :
//   - ZÉRO STL, zéro dépendance vision. Entrée = une image en niveaux de gris.
//   - Testable SANS caméra : une image de synthèse d'un marqueur à pose connue
//     suffit à vérifier la chaîne complète (même méthode que le simulateur XR
//     pour la VR — c'est ce qui permet d'avancer sans matériel).
//   - Le dictionnaire est une grille de N×N bits avec bordure noire ; la
//     lecture essaie les 4 rotations, ce qui donne l'orientation du marqueur
//     en même temps que son identité.
//
// Algorithmes implémentés :
//   - Seuillage d'Otsu (le seuil qui sépare le mieux deux populations de gris)
//   - Suivi de contour de Moore (parcours du bord d'une composante connexe)
//   - Simplification de polygone type Douglas-Peucker
//   - Homographie par DLT sur 4 correspondances (résolution 8×8 par Gauss)
//   - Pose plane depuis l'homographie (décomposition de Zhang, réorthonormée)
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKARMARKER_H__
#define __NKENTSEU_XR_NKARMARKER_H__

#include "NKXR/NkXrTypes.h"
#include "NKXR/NkXrPose.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace xr {

		// ── Paramètres intrinsèques de la caméra ─────────────────────────────
		// Sans eux, une pose n'a pas d'échelle : c'est la focale qui dit si un
		// marqueur est petit et proche ou grand et loin.
		struct NkArCameraIntrinsics {
			float32 fx = 0.f;  ///< Focale en pixels (axe X).
			float32 fy = 0.f;  ///< Focale en pixels (axe Y).
			float32 cx = 0.f;  ///< Centre optique X (souvent largeur/2).
			float32 cy = 0.f;  ///< Centre optique Y (souvent hauteur/2).

			// Estimation utilisable quand la caméra n'est pas calibrée : un
			// champ horizontal supposé. Approximatif et ASSUMÉ — une vraie
			// calibration viendra (damier), mais mieux vaut une pose à 10 %
			// près que pas d'AR du tout.
			static NkArCameraIntrinsics FromFovX(uint32 width, uint32 height, float32 fovXDegrees) {
				NkArCameraIntrinsics k;
				const float32 half = fovXDegrees * 0.5f * math::NK_PI_F / 180.f;
				k.fx = (float32(width) * 0.5f) / math::NkTan(half);
				k.fy = k.fx; // pixels carrés
				k.cx = float32(width) * 0.5f;
				k.cy = float32(height) * 0.5f;
				return k;
			}
		};

		// ── Un marqueur détecté ──────────────────────────────────────────────
		struct NkArDetection {
			int32 id = -1;             ///< Code lu, -1 si le quad n'en portait pas.
			NkVec2f corners[4]{};      ///< Coins en pixels, sens horaire, coin 0 = repère du code.
			float32 edgeLength = 0.f;  ///< Côté moyen en pixels (indice de distance).
		};

		// ── Réglages de détection ────────────────────────────────────────────
		struct NkArDetectorConfig {
			uint32 gridBits = 4;          ///< Grille du code (4x4 utiles + bordure).
			uint32 minEdgePixels = 24;    ///< Sous cette taille, le code est illisible.
			uint32 samplesPerCell = 3;    ///< Échantillons par cellule (médiane).
			// ── Seuillage ────────────────────────────────────────────────
			// OTSU GLOBAL par défaut : éprouvé (23/23 au self-test), rapide,
			// suffisant sous éclairage uniforme.
			// ADAPTATIF (opt-in) : seuil calculé LOCALEMENT dans une fenêtre
			// autour de chaque pixel, par image intégrale — fait pour les cas
			// où un seuil global échoue : marqueur affiché sur un ÉCRAN
			// (halos, reflets), éclairage de biais, ombre portée sur la
			// feuille. ⚠️ ÉTAT HONNÊTE : il RATE sur nos images de SYNTHÈSE
			// (fonds parfaitement uniformes, où la moyenne locale égale le
			// pixel), donc il n'est pas activé par défaut et n'est pas encore
			// couvert par le self-test. À valider sur caméra réelle, cas pour
			// lequel il a été écrit.
			bool adaptive = false;
			uint32 adaptiveWindow = 41;   ///< côté de la fenêtre, impair
			int32 adaptiveBias = 7;       ///< marge sous la moyenne locale
			bool useOtsu = true;          ///< si adaptive == false
			uint8 fixedThreshold = 128;
			// Journalise OÙ la chaîne abandonne (contours, quads, bordure,
			// code). Un champ et non une variable d'environnement : une
			// application doit pouvoir l'allumer depuis son interface.
			bool debugCounters = false;
		};

		// ── L'API ────────────────────────────────────────────────────────────

		// Détecte les marqueurs d'une image en niveaux de gris (ligne = width).
		// Rend le nombre de marqueurs trouvés.
		// outMask (optionnel, width*height octets) reçoit le masque binaire
		// APRÈS seuillage : 255 = sombre (ce que la chaîne considère comme de
		// l'encre). C'est LA façon de savoir pourquoi un marqueur n'est pas vu
		// — on regarde ce que le détecteur regarde, au lieu de supposer.
		uint32 NkArDetectMarkers(const uint8 *gray, uint32 width, uint32 height,
								 const NkArDetectorConfig &config, NkVector<NkArDetection> &outDetections,
								 uint8 *outMask = nullptr);

		// Pose du marqueur DANS le repère caméra (avant = -Z, comme partout
		// dans NKXR) : le marqueur est un carré de côté sizeMeters, centré sur
		// son origine, dans le plan z = 0.
		bool NkArPoseFromDetection(const NkArDetection &detection, float32 sizeMeters,
								   const NkArCameraIntrinsics &intrinsics, NkXrPose &outPose);

		// Fabrique l'image d'un marqueur (pour l'imprimer, et pour les tests) :
		// MARGE BLANCHE d'une cellule (indispensable — sans elle, un marqueur
		// affiché sur fond sombre n'a plus de contour fermé), puis la bordure
		// noire d'une cellule, puis gridBits × gridBits cellules utiles.
		// Le carré NOIR mesure donc (gridBits+2)/(gridBits+4) de l'image.
		bool NkArRenderMarker(int32 id, uint32 gridBits, uint8 *outGray, uint32 size);

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKARMARKER_H__
