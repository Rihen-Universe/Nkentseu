// -----------------------------------------------------------------------------
// FICHIER: Game\NkMatchFinder.h
// DESCRIPTION: Classe NkMatchFinder — détection des alignements (>=3) sur un
//              NkGemBoard, ET classification en gemmes spéciales selon la
//              forme du match (recherche Candy Crush) :
//                - run de 3          -> match normal, aucune spéciale
//                - run de 4          -> Rayée (orientation = celle du run)
//                - run de 5+         -> Bombe Couleur
//                - run H + run V qui se croisent (forme en L/T) -> Enveloppée
//              Pure logique, aucune dépendance de rendu.
// AUTEUR: Rihen
// DATE: 2026-08-26
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_NKMATCHFINDER_H
#define NKENTSEU_GAME_NKMATCHFINDER_H

#include "NkGemBoard.h"

namespace nkentseu {
	namespace game {

		// =================================================================
		// Struct : NkGemMatch
		// =================================================================
		struct NkGemMatch {
				NkGemColor color = NkGemColor::NK_GEM_COLOR_NONE;
				NkVector<math::NkVec2i> cells; ///< (x = row, y = column) — toutes les cases du cluster
				NkGemSpecialKind specialToCreate = NkGemSpecialKind::NK_GEM_SPECIAL_NONE;
				math::NkVec2i spawnCell{-1, -1}; ///< case où apparaît la spéciale (si specialToCreate != NONE)
		};

		// =================================================================
		// Classe : NkMatchFinder
		// =================================================================
		class NkMatchFinder {
			public:
				explicit NkMatchFinder(NkGemBoard &board) noexcept : mBoard(board) {
				}

				/// @brief Cases à privilégier comme emplacement de spawn si elles
				///        appartiennent au cluster détecté (typiquement : les 2 cases
				///        d'un échange joueur — la spéciale apparaît là où il a joué).
				void SetPreferredSpawnCells(const NkVector<math::NkVec2i> &cells) {
					mPreferredSpawnCells = cells;
				}

				void ClearPreferredSpawnCells() {
					mPreferredSpawnCells.Clear();
				}

				/// @brief Recherche tous les alignements valides du plateau, groupés en
				///        clusters (une case en L/T = UN seul NkGemMatch, pas deux)
				NkVector<NkGemMatch> FindMatches() const;

				bool HasMatchAt(int32 row, int32 column) const;

				/// @brief Simule un échange (swap + test + swap retour) et indique s'il
				///        produirait un alignement.
				bool WouldSwapCreateMatch(int32 rowA, int32 columnA, int32 rowB, int32 columnB);

			private:
				NkGemBoard &mBoard;
				NkVector<math::NkVec2i> mPreferredSpawnCells;

				static bool SameColor(const NkGem *first, const NkGem *second) noexcept;

				NkVector<math::NkVec2i> FindHorizontalRun(int32 row, int32 column) const;
				NkVector<math::NkVec2i> FindVerticalRun(int32 row, int32 column) const;
		};

	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_NKMATCHFINDER_H