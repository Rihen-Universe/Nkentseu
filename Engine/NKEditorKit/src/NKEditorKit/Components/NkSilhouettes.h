#pragma once
// -----------------------------------------------------------------------------
// @File    NkSilhouettes.h
// @Brief   LES SILHOUETTES DESSINÉES — la petite bibliothèque d'icônes que
//          partagent le navigateur de contenu (la grille) et le tree_view (le
//          rail d'un sélecteur de fichiers).
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// @License Proprietary - All Rights Reserved (see LICENSE)
//
// -----------------------------------------------------------------------------
//  POURQUOI CE FICHIER EXISTE (2026-09-05, nuit)
//
//  Rodolf : « le panneau de gauche ne montre pas les icônes associées aux
//  différents dossiers » — le rail était du texte nu pendant que la grille avait
//  ses onze silhouettes.
//
//  Les deux volets doivent donc appeler LA MÊME fonction. Elle vivait dans
//  `NkContentBrowserModel.h` ; l'y laisser aurait obligé le dessin du
//  `tree_view` à dépendre du modèle du navigateur de contenu — un composant qui
//  dépend d'un autre composant de même rang, pour une seule fonction de tracé.
//
//  ⚠️ CE FICHIER NE CONNAÎT NI L'UN NI L'AUTRE. Il ne contient qu'une
//     énumération et une signature ; `NkContentBrowserModel.h` l'inclut (rien ne
//     change pour ses consommateurs), et `NkTreeViewDraw.cpp` aussi.
//
//  ⚠️ ET RIEN N'EST UN GLYPHE DE POLICE (porte du 04/09) :
//     `NkComponentPaint::Icon` peint un carré plein — son en-tête le dit, il
//     n'existe aucun atlas d'icônes. Les silhouettes sont tracées avec
//     `FillColor` / `Line` / `PolygonHex`, sur une grille de 16 unités.
// -----------------------------------------------------------------------------

#include "NKCore/NkTypes.h"
#include "NKEditorKit/Components/NkComponentPaint.h"

namespace nkentseu {
	namespace editorkit {

		// ── LES NATURES DESSINÉES ───────────────────────────────────────────────
		// Une bibliothèque FERMÉE, et c'est délibéré : ce sont les natures que TOUT
		// système de fichiers connaît, pas celles d'une application. Une application
		// qui a ses propres natures les exprime par `kindRole` / `kindLabel`.
		// ⚠️ Règle append-only : ces valeurs finissent dans des fichiers.
		enum class NkAssetIcone : uint8 {
			Auto = 0,	  ///< déduite de « est-ce un dossier » : dossier, ou fichier inconnu
			Dossier,
			DossierImages, ///< les dossiers CONNUS se reconnaissent à leur chemin
			DossierDocuments,
			DossierTelechargements,
			DossierBureau,
			Image,
			Texte,
			Code,
			Archive,
			Executable,
			Inconnu,
			Volume,	 ///< un disque : un boîtier, PAS un dossier
			Section, ///< un titre de rail : ce n'est pas un objet du système de fichiers
			Count
		};

		/// Peint la silhouette de `genre` dans `r`, teintée par `role`.
		/// Définie dans `NkContentBrowserDraw.cpp` ; appelée par la GRILLE et par le
		/// RAIL — une seule fonction, deux volets. Deux tables auraient divergé dès
		/// le premier ajout de nature.
		void NkDessinerSilhouette(NkComponentPaint &p, const NkPaintRect &r, NkAssetIcone genre,
								  uint16 role);

	} // namespace editorkit
} // namespace nkentseu
