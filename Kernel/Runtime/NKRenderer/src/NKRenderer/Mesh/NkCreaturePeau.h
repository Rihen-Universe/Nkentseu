// -----------------------------------------------------------------------------
// FICHIER: Kernel\Runtime\NKRenderer\src\NKRenderer\Mesh\NkCreaturePeau.h
// DESCRIPTION: La peau G1 du generateur de creatures : un tube de quads continu
//              par CHAINE d'os, boucles d'articulation, capuchons en quads.
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------
//
// SPECIFICATION : Applications/NKCraft/design/04-generateur-creatures-nkcraft.md
//   §2 (la topologie par construction), §5.1 (tubes), §5.3 (capuchons), §5.4
//   (parite), §6 (criteres), §14 G1.
//
// CE QU'ELLE CONSTRUIT
//   - un tube continu par chaine : l'anneau du joint est PARTAGE par le parent et
//     l'enfant, pose dans le plan BISSECTEUR du coude (onglet 1/cos(demi-angle)) ;
//   - trois boucles par articulation (1 - delta sur le parent, le joint, delta sur
//     l'enfant) : la « bande rouge » des mannequins, qui porte le pli ;
//   - des anneaux espaces comme le pas AUTOUR de l'os (densite = 0) : des quads
//     proches du carre, quelle que soit l'epaisseur ;
//   - un capuchon a chaque bout de chaine, en GRILLE n x m dont le bord
//     (2(n + m) = M) epouse l'anneau : 100 % quads, et 4 sommets de valence 3 par
//     capuchon, soit Σ(4 - val) = 8 par chaine -- le minimum du §2.1 (g = 0) ;
//   - les sections en superellipse (profil a clefs), la meme fonction que R32
//     (NkR32PointSection) : les adresses (os, s, a) gardent leur sens.
//
// ⚠️ CE QU'ELLE REFUSE, NOMMEMENT
//   - un os qui porte plusieurs os : c'est une JONCTION (§5.2, G2) ;
//   - 2 boucles par articulation (seules 3 sont construites en G1) ;
//   - un os trop court pour loger ses boucles.
// ⚠️ CE QU'ELLE NE FAIT PAS
//   - aucun volume C1 : la peau suit les PROFILS, pas le blockout (G3) ;
//   - l'adresse d'une face de capuchon ne porte que (os, bout) : pas de (r, a).
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATUREPEAU_H_INCLUDED
#define NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATUREPEAU_H_INCLUDED

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
		 * @brief Construit la peau G1 (un tube continu par chaine) depuis le document.
		 *
		 * @param[in]  doc Le document (`generateur` est ignore ici : on construit G1).
		 * @param[out] out La peau : maillage, adresse de chaque face (`Face::origine`
		 *                 = f + 1, comme en G0) et adresse de chaque sommet.
		 * @param[out] pourquoi Refus nomme (embranchement, anneaux impairs, os trop court...).
		 * @return false si la peau ne se construit pas.
		 * @post Deterministe : meme document -> meme maillage AU BIT.
		 */
		bool NkCreatureConstruirePeau(const NkR32Document &doc, NkR32Peau &out, char *pourquoi, uint32 cap);

	}  // namespace renderer
}  // namespace nkentseu

#endif  // NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATUREPEAU_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
