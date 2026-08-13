//
// NkArCalibration.h
// =============================================================================
// Description :
//   Mesurer la GÉOMÉTRIE RÉELLE de l'objectif, au lieu de la supposer.
//
// Pourquoi ce fichier existe :
//   Jusqu'ici la chaîne AR devinait la focale à partir d'un champ de vision
//   « probablement 60° ». Cette supposition se paie partout : la distance
//   annoncée est fausse de plusieurs pour cent, la perspective ne colle pas à
//   l'image, et l'inclinaison des objets pivote — ce que Rihen résume d'un
//   « l'orientation et la perspective ne sont pas vraiment top ». Aucun
//   filtrage ne rattrape une focale fausse : il faut la MESURER.
//
// La méthode, et pourquoi celle-ci :
//   On montre à la caméra une planche PLANE dont on connaît la géométrie, sous
//   plusieurs angles. Chaque vue donne une homographie entre le plan de la
//   planche et l'image ; deux contraintes s'en déduisent sur la matrice de
//   l'objectif, et trois vues suffisent à la résoudre (méthode de Zhang).
//
//   La planche est faite de NOS marqueurs, à positions connues — pas d'un
//   damier. Deux raisons : le détecteur de marqueurs existe déjà et il est
//   éprouvé, et surtout chaque marqueur porte son IDENTITÉ, donc on sait quel
//   coin est lequel même si la planche est partiellement visible ou tournée.
//   Un damier, lui, exige de retrouver l'ordre des coins — un problème de plus,
//   sans rapport avec ce qu'on cherche.
//
// Ce que la calibration NE fait PAS :
//   elle ne corrige pas la DISTORSION de l'objectif (les lignes droites qui
//   s'incurvent près des bords). C'est un second chantier ; sur un téléphone
//   moderne à champ modéré, elle reste sous le pour cent au centre de l'image,
//   là où on regarde un marqueur.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits réservés.
// =============================================================================

#pragma once

#ifndef __NKENTSEU_XR_NKARCALIBRATION_H__
#define __NKENTSEU_XR_NKARCALIBRATION_H__

#include "NKXR/AR/NkArMarker.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
	namespace xr {

		// ── La planche de calibration ────────────────────────────────────────
		// Une grille de marqueurs à positions CONNUES. L'identifiant du premier
		// marqueur est `firstId` ; ils se suivent en balayant les lignes.
		struct NkArCalibrationBoard {
			uint32 cols = 3;
			uint32 rows = 3;
			uint32 gridBits = 4;
			int32 firstId = 100;
			/// Côté du CARRÉ NOIR d'un marqueur, en mètres, tel qu'il est
			/// affiché ou imprimé. C'est lui qui donne l'échelle.
			float32 markerSizeMeters = 0.04f;
			/// Distance entre les CENTRES de deux marqueurs voisins.
			float32 spacingMeters = 0.06f;

			/// Position du centre d'un marqueur dans le plan de la planche.
			bool CenterOf(int32 id, float32 &outX, float32 &outY) const;
		};

		// ── Résultat ─────────────────────────────────────────────────────────
		struct NkArCalibrationResult {
			bool valid = false;
			NkArCameraIntrinsics intrinsics{};
			uint32 viewsUsed = 0;
			/// Erreur moyenne de reprojection, en pixels. C'est LE chiffre qui
			/// dit si la calibration vaut quelque chose : sous un pixel, elle
			/// est bonne ; au-delà de trois, quelque chose cloche (planche non
			/// plane, vues trop semblables, détections fausses).
			float32 reprojectionErrorPixels = 0.f;
			/// Champ horizontal correspondant, en degrés — pour comparer d'un
			/// coup d'œil à la valeur qu'on supposait.
			float32 fovXDegrees = 0.f;
		};

		// ── Le calibrateur ───────────────────────────────────────────────────
		class NkArCalibration {
			public:
				void Initialize(const NkArCalibrationBoard &board, uint32 imageWidth, uint32 imageHeight);
				void Reset();

				/// Ajoute une vue depuis les marqueurs détectés dans une image.
				/// Rend false si la vue n'apporte rien (trop peu de marqueurs,
				/// ou point de vue trop proche d'une vue déjà prise — deux vues
				/// identiques donnent deux fois la même équation, donc aucune
				/// information supplémentaire).
				bool AddView(const NkVector<NkArDetection> &detections);

				uint32 GetViewCount() const { return uint32(mViews.Size()); }
				/// Trois vues suffisent en théorie ; on en demande davantage,
				/// car chacune est bruitée et la moyenne vaut mieux qu'un pari.
				bool IsReady() const { return mViews.Size() >= mMinViews; }

				/// Résout la géométrie de l'objectif. À appeler quand IsReady().
				NkArCalibrationResult Solve() const;

				/// Supposer le centre optique AU MILIEU de l'image et ne
				/// résoudre que les deux focales.
				///
				/// Ce n'est pas un renoncement, c'est le bon compromis. Avec une
				/// planche plane et une poignée d'angles, le centre optique est
				/// de loin le paramètre le plus mal contraint : mesuré sur le
				/// téléphone de Rihen, il sortait à −186 pixels pour une image
				/// large de 720 — une valeur qui n'a aucun sens physique et qui
				/// empoisonne la pose. Un capteur de téléphone est centré à
				/// quelques pour cent près ; supposer le centre coûte donc bien
				/// moins que de le deviner mal.
				/// À rouvrir le jour où l'on saura recueillir vingt vues
				/// franchement inclinées.
				void SetAssumeCenteredPrincipalPoint(bool centered) { mAssumeCentered = centered; }

				// Volontairement SANS lecture ni écriture de fichier : un module
				// de calcul géométrique n'a pas à connaître le disque, et NKXR ne
				// dépend pas du système de fichiers. C'est à l'application de
				// ranger le résultat où elle veut — quatre nombres suffisent.

			private:
				struct View {
					NkVector<NkVec2f> imagePoints;  ///< coins observés, en pixels
					NkVector<NkVec2f> boardPoints;  ///< mêmes coins sur la planche, en mètres
					float32 homography[9] = {};
				};

				NkArCalibrationBoard mBoard{};
				NkVector<View> mViews;
				uint32 mWidth = 0;
				uint32 mHeight = 0;
				uint32 mMinViews = 6;
				/// Différence minimale de FORME entre deux vues retenues, une fois
				/// leur centre et leur échelle retirés.
				///
				/// C'est bien la forme qu'il faut mesurer, et non la position :
				/// déplacer la planche ou s'en éloigner ne change que son endroit
				/// et sa taille à l'image, sans rien apprendre au système. Seul
				/// un changement d'ANGLE la déforme, et seule cette déformation
				/// apporte une équation nouvelle.
				/// 0,10 vaut dix pour cent de l'étendue apparente de la planche :
				/// bien au-delà d'un tremblement de main, atteignable d'un simple
				/// mouvement du poignet. Mesuré : à 0,05 six vues étaient retenues
				/// en un sixième de seconde — six fois le même point de vue.
				float32 mMinShapeDifference = 0.14f;
				bool mAssumeCentered = true;
		};

		/// Dessine la planche de calibration en niveaux de gris (255 = blanc).
		/// L'appelant fournit un tampon de `outWidth * outHeight`.
		bool NkArRenderCalibrationBoard(const NkArCalibrationBoard &board, uint8 *outGray, uint32 outWidth,
										uint32 outHeight);

	} // namespace xr
} // namespace nkentseu

#endif // __NKENTSEU_XR_NKARCALIBRATION_H__
