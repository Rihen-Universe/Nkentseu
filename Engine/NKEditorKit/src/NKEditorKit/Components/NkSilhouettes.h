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

		/// ①③ UNE NUANCE DE LA MEME TEINTE, calculée sur les composantes.
		/// `k > 0` éclaircit vers le blanc, `k < 0` assombrit vers le noir ; l'alpha ne
		/// bouge pas.
		/// ⚠️ L'EMPAQUETAGE EST `0xRRGGBBAA` — `NkGuiComponentPaint::Unpack` lit le ROUGE
		///    dans les bits 24-31. Un code qui masquait `& 0x00FFFFFF` en croyant à
		///    `0xAARRGGBB` mettait le rouge à zéro : d'un ambre, il restait du VERT.
		///    C'est le défaut que Rodolf a vu sur l'onglet des dossiers. Une teinte ne se
		///    bricole pas au masque.
		inline uint32 NkTeinter(uint32 rgba, float32 k) {
			const int32 r0 = (int32)((rgba >> 24) & 0xFFu), g0 = (int32)((rgba >> 16) & 0xFFu);
			const int32 b0 = (int32)((rgba >> 8) & 0xFFu), a0 = (int32)(rgba & 0xFFu);
			auto mix = [&](int32 c) -> uint32 {
				const float32 cible = k >= 0.f ? 255.f : 0.f;
				const float32 t = k >= 0.f ? k : -k;
				float32 v = (float32)c + (cible - (float32)c) * t;
				if (v < 0.f) v = 0.f;
				if (v > 255.f) v = 255.f;
				return (uint32)(v + 0.5f);
			};
			return (mix(r0) << 24) | (mix(g0) << 16) | (mix(b0) << 8) | (uint32)a0;
		}

		// ── CE QU'UN DOSSIER CONTIENT (2026-09-05, v5) ────────────────────
		// Rodolf : « il faut aussi distinguer dossier vide de dossier plein ».
		//
		// ⚠️ UN ETAT A COTE DE LA NATURE, PAS DANS L'ENUMERATION. On aurait pu ajouter
		//    `DossierVide` et `DossierPlein` a `NkAssetIcone` : il aurait alors fallu
		//    `ImagesVide`, `ImagesPlein`, `BureauVide`... soit onze natures fois trois
		//    etats. Un attribut ORTHOGONAL se combine avec les onze silhouettes sans en
		//    ajouter une seule.
		//
		// ⚠️ ET IL Y A UN QUATRIEME ETAT, `Illisible`. Un dossier dont la lecture est
		//    refusee (droits, volume demonte) n'est NI vide NI plein. Le dessiner vide
		//    serait un mensonge : c'est la meme faute que `NkDirectory::Empty`, qui rend
		//    « vide » ce qu'il n'a pas pu ouvrir.
		enum class NkContenuDossier : uint8 {
			Inconnu = 0, ///< pas encore demande au disque -- le dossier garde son dessin nu
			Vide,		 ///< ouvert, parcouru, rien dedans
			Plein,		 ///< au moins un element : des feuilles depassent derriere le rabat
			Illisible	 ///< lecture refusee : ni l'un ni l'autre, et l'infobulle le dit
		};

		/// Peint la silhouette de `genre` dans `r`, teintée par `role`.
		/// Définie dans `NkContentBrowserDraw.cpp` ; appelée par la GRILLE et par le
		/// RAIL — une seule fonction, deux volets. Deux tables auraient divergé dès
		/// le premier ajout de nature.
		/// @param contenu État de remplissage (`NkContenuDossier`), n'a de sens que pour
		///        un dossier. `Inconnu` par défaut : les consommateurs qui ne le
		///        renseignent pas gardent EXACTEMENT le dessin d'avant.
		void NkDessinerSilhouette(NkComponentPaint &p, const NkPaintRect &r, NkAssetIcone genre,
								  uint16 role, uint8 contenu = 0);

	} // namespace editorkit
} // namespace nkentseu
