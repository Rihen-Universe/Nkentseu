// -----------------------------------------------------------------------------
// FICHIER: Kernel\Runtime\NKRenderer\src\NKRenderer\Mesh\NkCreatureMesures.h
// DESCRIPTION: Les criteres de topologie du §6, MESURES sur une peau de creature :
//              ce que chaque palier (G1, G2...) doit tenir.
// AUTEUR: TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE: 2026-09-29
// VERSION: 0.1.0
// -----------------------------------------------------------------------------
//
// SPECIFICATION : 04-generateur-creatures-nkcraft.md §2.1 et §6.
//
// CE MODULE MESURE, IL NE JUGE PAS. Il rend des chiffres ; les seuils du §6
// vivent dans NkCreatureCriteres, a un seul endroit, et le banc les applique.
//
// ⚠️ CE QU'IL NE MESURE PAS (et le dit)
//   - la fidelite de la peau aux VOLUMES du blockout : il n'y a pas de volumes
//     avant G3 ;
//   - deux triangles COPLANAIRES qui se chevauchent : le test d'auto-intersection
//     est arete-contre-triangle, il voit toute traversee, pas la superposition
//     a plat.
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATUREMESURES_H_INCLUDED
#define NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATUREMESURES_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NKRenderer/Mesh/NkMeshR32.h"

// ============================================================
// DECLARATIONS
// ============================================================

namespace nkentseu {
	namespace renderer {

		/** @brief Les poids du banc de pose. */
		enum class NkCreaturePoids : uint8 {
			/** 0 / 0,25 / 0,5 / 0,75 / 1 a travers les trois boucles (04 §9 : poids par construction). */
			Nk_CreaturePoids_Construction = 0,
			/** Tout ou rien a l'anneau du joint : le TEMOIN, qui doit faire moins bien. */
			Nk_CreaturePoids_Rigide
		};

		/**
		 * @brief Le skinning du banc de pose.
		 *
		 * DECISION DU 29/09 (proposee sous delegation de Rodolf, a confirmer) : le
		 * critere « pli a 90 degres » du §6 se JUGE en quaternions duaux ; le lineaire
		 * est mesure et AFFICHE. Mesure : aucune largeur de bande de pli ne tient
		 * « perte <= 15 % et aucune pliure » en lineaire (bras 12-32 %, serpent
		 * 26-40 %) -- c'est le defaut connu du melange lineaire, qui rapproche de
		 * l'axe les points melanges. En quaternions duaux : 2 a 5 % de perte.
		 * Le moteur ne fait aujourd'hui QUE du lineaire : porter le DQS au skinning
		 * GPU (NkAnima) est une tache de G4.
		 */
		enum class NkCreatureSkinning : uint8 {
			Nk_CreatureSkinning_QuaternionsDuaux = 0,
			Nk_CreatureSkinning_Lineaire
		};

		/** @brief Les chiffres du §6 sur une peau. */
		struct NkCreatureMesure {
				// ── faces de la cage ─────────────────────────────────────────
				uint32 faces = 0u;
				uint32 quads = 0u;
				float32 partQuads = 0.f;
				// ── valence, sur la cage subdivisee UNE fois (Catmull-Clark) ────
				uint32 sommetsSubdivises = 0u;
				uint32 valence4 = 0u;
				float32 partValence4 = 0.f;
				/** @brief Σ (4 - valence) sur la cage subdivisee. */
				int32 sommeEcarts = 0;
				/** @brief 8 - 8g par composante fermee (§2.1). */
				int32 sommeAttendue = 0;
				// ── variete ──────────────────────────────────────────────────
				int32 euler = 0;
				int32 genre = -1;
				uint32 composantes = 0u;
				uint32 aretesNonManifold = 0u;
				uint32 aretesBord = 0u;
				uint32 autoIntersections = 0u;
				/** @brief Les os des deux faces de la premiere auto-intersection trouvee. */
				char intersectionOs[2][NK_R32_NOM_MAX] = {{0}, {0}};
				/** @brief Leur adresse : lieu, s0, s1, a0. */
				uint32 intersectionLieu[2] = {0u, 0u};
				float32 intersectionS[2][2] = {{0.f, 0.f}, {0.f, 0.f}};
				float32 intersectionA[2] = {0.f, 0.f};
				// ── boucles et poles ─────────────────────────────────────────
				uint32 articulations = 0u;
				uint32 bouclesMin = 0u;
				uint32 bouclesMax = 0u;
				uint32 poles = 0u;
				/** @brief Poles a moins de 2 anneaux d'une bande de pli. */
				uint32 polesPresDesPlis = 0u;
				int32 distancePoleMin = 0;
				// ── symetrie (plan x = 0) ────────────────────────────────────
				float32 ecartSymetrie = 0.f;
				// ── regularite des quads ─────────────────────────────────────
				uint32 facesRegulieres = 0u;
				float32 partReguliere = 0.f;
				float32 rapportCotesMax = 0.f;
				/** @brief L'os et le lieu (0 tube, 1 debut, 2 fin) du quad le plus allonge. */
				char rapportOs[NK_R32_NOM_MAX] = {0};
				uint32 rapportLieu = 0u;
				// ── banc de pose : chaque articulation pliee a 90 degres ──────
				// Les champs sans suffixe sont ceux du skinning JUGE (quaternions
				// duaux) ; `...Lineaire` est le skinning actuel du moteur, affiche.
				// Une face RETOURNEE est a l'envers pour les DEUX os (une pliure) :
				// la premiere definition (ecart a une normale melangee) comptait aussi
				// des faces seulement plus tournees que prevu (bras : 39 contre 22).
				uint32 articulationsPliees = 0u;
				float32 perteVolumeLocalePire = 0.f;
				float32 perteVolumeTotalePire = 0.f;
				uint32 facesRetournees = 0u;
				float32 perteVolumeLocaleLineaire = 0.f;
				uint32 facesRetourneesLineaire = 0u;
				/** @brief L'articulation (os enfant) qui a le plus de pliures, et combien. */
				char articulationPire[NK_R32_NOM_MAX] = {0};
				uint32 pliuresPire = 0u;
				/** @brief L'articulation qui perd le plus de volume (juge). */
				char articulationPerte[NK_R32_NOM_MAX] = {0};
		};

		/**
		 * @brief Mesure la peau `peau` construite depuis `doc` (qui doit porter les
		 *        adresses de sommets : generateur 2).
		 * @param poids Les poids du banc de pose (construction, ou le temoin rigide).
		 */
		bool NkCreatureMesurer(const NkR32Document &doc, const NkR32Peau &peau, NkCreatureMesure &m, char *pourquoi,
							   uint32 cap, NkCreaturePoids poids = NkCreaturePoids::Nk_CreaturePoids_Construction);

		/** @brief Les seuils du §6, a UN seul endroit. */
		struct NkCreatureCriteres {
				float32 partQuads = 1.f;
				float32 partValence4 = 0.95f;
				uint32 bouclesMin = 2u;
				uint32 bouclesMax = 3u;
				int32 distancePole = 2;
				float32 ecartSymetrie = 1e-6f;
				float32 rapportCotes = 3.f;
				float32 partReguliere = 0.95f;
				float32 perteVolume = 0.15f;
		};

	}  // namespace renderer
}  // namespace nkentseu

#endif  // NK_NKRENDERER_SRC_NKRENDERER_MESH_NKCREATUREMESURES_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Creation Date: 2026-09-29
// ============================================================
