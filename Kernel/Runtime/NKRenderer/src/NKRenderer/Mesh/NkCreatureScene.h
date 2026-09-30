// -----------------------------------------------------------------------------
// FICHIER: Kernel\Runtime\NKRenderer\src\NKRenderer\Mesh\NkCreatureScene.h
// DESCRIPTION: Lecteur du `.nkscene` version 2, partie squelette et profils (G1
//              du generateur de creatures) -> NkR32Document.
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------
//
// SPECIFICATION : 04-generateur-creatures-nkcraft.md §4 (le format), §4.2 (le
// vocabulaire). Le document reste du TEXTE, une directive par ligne.
//
// CE QUE G1 LIT
//   nkscene 2
//   creature "Nom" [style s] [symetrie X]
//   squelette
//     os NOM (racine [position x y z] | depuis REF) longueur L [direction x y z]
//            [rayon r0 [r1]] [reference x y z]
//     chaine NOM (racine | depuis REF) segments N longueur L
//            [direction x y z | vers ±X|±Y|±Z] [courbure x y z] [effilement e] [rayon r]
//         -> os NOM.1 ... NOM.N (c'est ainsi que le §4.1 les adresse : `colonne.1`)
//     membre NOM (racine | depuis REF) [direction x y z]
//         os A longueur L [direction ...] [rayon ...]   os B ...   (lignes plus indentees)
//         -> os A_g, B_g... : le suffixe du membre (`_g` / `_d`) passe a ses os
//     miroir *_g -> *_d
//   forme
//     profil MOTIF section superellipse largeur W hauteur H [exposant n]
//     profil MOTIF clefs s:WxH s:WxH ...       (MOTIF : un os, une chaine, ou `x_*`)
//     resolution anneaux M [boucles_articulation K] [bande_pli R]   (R en rayons, 1,1 par defaut)
//
//   REF : un os, une chaine (= son dernier os), ou `chaine.k` (le k-ieme, 1-based).
//   Largeur et hauteur sont des dimensions TOTALES (diametres).
//
// LA REFERENCE D'ORIENTATION D'UN ENFANT est celle du parent TRANSPORTEE le long du
// coude (rotation minimale), puis GELEE dans le document : le tube ne vrille pas,
// et R32 a une reference explicite a proteger (04 §3.2bis, cas 4).
//
// ⚠️ REFUS NOMMES, avec le palier qui les construira : `extremite`, `appendice`,
//    `repeter`, `boucle` (G2/G2c), `corps`, `nageoire`, `aile`, `tentacule`,
//    `segment`, `semis`, `greffe` (G2b/G2c), `volumes` et `volume` (G3), `tete`
//    (G4). Une ligne inconnue est refusee avec son numero -- jamais ignoree.
// ⚠️ IGNORES AVEC UN AVERTISSEMENT (ils ne changent pas la peau G1) : `style`,
//    `muscle` dans un profil, `retouches` et `sculpt` (la pile se charge a part).
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATURESCENE_H_INCLUDED
#define NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATURESCENE_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NKRenderer/Mesh/NkMeshR32.h"

// ============================================================
// DECLARATIONS
// ============================================================

namespace nkentseu {
	namespace renderer {

		/**
		 * @brief Lit un `.nkscene` v2 (squelette, profils, resolution) dans `doc`.
		 *
		 * @param[in]  texte Le document, zero-termine.
		 * @param[out] doc Le document C0 ; `generateur` vaut 2 et `densite` 0
		 *                 (espacement automatique).
		 * @param[out] pourquoi Refus nomme, avec le numero de ligne.
		 * @param[out] avertissements Ce qui a ete lu et ignore (peut etre nul).
		 * @return false au premier refus : un document a moitie lu n'est pas rendu.
		 */
		bool NkCreatureLireScene(const char *texte, NkR32Document &doc, char *pourquoi, uint32 cap,
								 char *avertissements = nullptr, uint32 capAvertissements = 0u);

	}  // namespace renderer
}  // namespace nkentseu

#endif  // NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATURESCENE_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
