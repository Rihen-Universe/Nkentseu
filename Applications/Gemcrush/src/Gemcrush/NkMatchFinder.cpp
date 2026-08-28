// -----------------------------------------------------------------------------
// FICHIER: Game\NkMatchFinder.cpp
// DESCRIPTION: Implémentation de NkMatchFinder.
// AUTEUR: Rihen
// DATE: 2026-08-26
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NkMatchFinder.h"

namespace nkentseu {
	namespace game {

		// =====================================================================
		// Aides locales (fichier uniquement — pas besoin d'exposer ces types)
		// =====================================================================
		namespace {

			/// @brief Un run brut avant regroupement : ligne, colonne, ou bloc carré 2x2.
			enum class NkRunKind : uint8 { Horizontal, Vertical, Square };

			struct NkRunInfo {
					NkGemColor color = NkGemColor::NK_GEM_COLOR_NONE;
					NkRunKind kind = NkRunKind::Horizontal;
					NkVector<math::NkVec2i> cells;
			};

			bool CellListContains(const NkVector<math::NkVec2i> &cells, int32 row, int32 column) noexcept {
				for (typename NkVector<math::NkVec2i>::SizeType i = 0; i < cells.Size(); ++i) {
					if (cells[i].x == row && cells[i].y == column) {
						return true;
					}
				}
				return false;
			}

			bool RunsShareCell(const NkRunInfo &a, const NkRunInfo &b) noexcept {
				for (typename NkVector<math::NkVec2i>::SizeType i = 0; i < a.cells.Size(); ++i) {
					if (CellListContains(b.cells, a.cells[i].x, a.cells[i].y)) {
						return true;
					}
				}
				return false;
			}

		} // namespace

		// =====================================================================
		// SameColor
		// =====================================================================
		bool NkMatchFinder::SameColor(const NkGem *first, const NkGem *second) noexcept {
			if (first == nullptr || second == nullptr) {
				return false;
			}
			if (first->GetColor() == NkGemColor::NK_GEM_COLOR_NONE) {
				return false;
			}
			return first->GetColor() == second->GetColor();
		}

		// =====================================================================
		// FindHorizontalRun / FindVerticalRun — utilisés par HasMatchAt uniquement
		// =====================================================================
		NkVector<math::NkVec2i> NkMatchFinder::FindHorizontalRun(int32 row, int32 column) const {
			NkVector<math::NkVec2i> run;
			const NkGem *origin = mBoard.GetGem(row, column);
			if (origin == nullptr) {
				return run;
			}
			int32 left = column;
			while (left - 1 >= 0 && SameColor(mBoard.GetGem(row, left - 1), origin)) {
				--left;
			}
			int32 right = column;
			while (right + 1 < mBoard.GetColumnCount() && SameColor(mBoard.GetGem(row, right + 1), origin)) {
				++right;
			}
			if (right - left + 1 >= 3) {
				for (int32 c = left; c <= right; ++c) {
					run.PushBack(math::NkVec2i(row, c));
				}
			}
			return run;
		}

		NkVector<math::NkVec2i> NkMatchFinder::FindVerticalRun(int32 row, int32 column) const {
			NkVector<math::NkVec2i> run;
			const NkGem *origin = mBoard.GetGem(row, column);
			if (origin == nullptr) {
				return run;
			}
			int32 top = row;
			while (top - 1 >= 0 && SameColor(mBoard.GetGem(top - 1, column), origin)) {
				--top;
			}
			int32 bottom = row;
			while (bottom + 1 < mBoard.GetRowCount() && SameColor(mBoard.GetGem(bottom + 1, column), origin)) {
				++bottom;
			}
			if (bottom - top + 1 >= 3) {
				for (int32 r = top; r <= bottom; ++r) {
					run.PushBack(math::NkVec2i(r, column));
				}
			}
			return run;
		}

		bool NkMatchFinder::HasMatchAt(int32 row, int32 column) const {
			return FindHorizontalRun(row, column).Size() > 0 || FindVerticalRun(row, column).Size() > 0;
		}

		bool NkMatchFinder::WouldSwapCreateMatch(int32 rowA, int32 columnA, int32 rowB, int32 columnB) {
			if (!mBoard.TrySwap(rowA, columnA, rowB, columnB)) {
				return false;
			}
			const bool createsMatch = HasMatchAt(rowA, columnA) || HasMatchAt(rowB, columnB);
			mBoard.TrySwap(rowA, columnA, rowB, columnB); // remet le plateau dans son état d'origine
			return createsMatch;
		}

		// =====================================================================
		// FindMatches — collecte les runs bruts, les regroupe en clusters
		// (un run H et un run V qui partagent une case = même cluster), puis
		// classe chaque cluster en gemme spéciale à créer (ou aucune).
		// =====================================================================
		NkVector<NkGemMatch> NkMatchFinder::FindMatches() const {
			NkVector<NkGemMatch> matches;

			// ── 1. Runs bruts (horizontaux puis verticaux) ─────────────────────
			NkVector<NkRunInfo> runs;

			for (int32 row = 0; row < mBoard.GetRowCount(); ++row) {
				int32 column = 0;
				while (column < mBoard.GetColumnCount()) {
					const NkGem *origin = mBoard.GetGem(row, column);
					if (origin == nullptr || origin->GetColor() == NkGemColor::NK_GEM_COLOR_NONE) {
						++column;
						continue;
					}
					int32 runEnd = column;
					while (runEnd + 1 < mBoard.GetColumnCount() && SameColor(mBoard.GetGem(row, runEnd + 1), origin)) {
						++runEnd;
					}
					if (runEnd - column + 1 >= 3) {
						NkRunInfo run;
						run.color = origin->GetColor();
						run.kind = NkRunKind::Horizontal;
						for (int32 c = column; c <= runEnd; ++c) {
							run.cells.PushBack(math::NkVec2i(row, c));
						}
						runs.PushBack(run);
					}
					column = runEnd + 1;
				}
			}

			for (int32 column = 0; column < mBoard.GetColumnCount(); ++column) {
				int32 row = 0;
				while (row < mBoard.GetRowCount()) {
					const NkGem *origin = mBoard.GetGem(row, column);
					if (origin == nullptr || origin->GetColor() == NkGemColor::NK_GEM_COLOR_NONE) {
						++row;
						continue;
					}
					int32 runEnd = row;
					while (runEnd + 1 < mBoard.GetRowCount() && SameColor(mBoard.GetGem(runEnd + 1, column), origin)) {
						++runEnd;
					}
					if (runEnd - row + 1 >= 3) {
						NkRunInfo run;
						run.color = origin->GetColor();
						run.kind = NkRunKind::Vertical;
						for (int32 r = row; r <= runEnd; ++r) {
							run.cells.PushBack(math::NkVec2i(r, column));
						}
						runs.PushBack(run);
					}
					row = runEnd + 1;
				}
			}

			// ── 1bis. Blocs carrés 2x2 (aucune ligne/colonne de 3+ ne les détecte :
			//      un carré 2x2 n'a que 2 cases dans chaque sens). Toute cellule qui
			//      chevauche ensuite avec une autre (carré adjacent, ou ligne) fusionne
			//      via l'union ci-dessous — un bloc 3x3 par ex. génère 4 carrés
			//      qui se recouvrent et se retrouvent dans le même cluster. ─────────
			for (int32 row = 0; row + 1 < mBoard.GetRowCount(); ++row) {
				for (int32 column = 0; column + 1 < mBoard.GetColumnCount(); ++column) {
					const NkGem *topLeft = mBoard.GetGem(row, column);
					if (topLeft == nullptr || topLeft->GetColor() == NkGemColor::NK_GEM_COLOR_NONE) {
						continue;
					}
					if (SameColor(mBoard.GetGem(row, column + 1), topLeft) && SameColor(mBoard.GetGem(row + 1, column), topLeft) &&
						SameColor(mBoard.GetGem(row + 1, column + 1), topLeft)) {
						NkRunInfo run;
						run.color = topLeft->GetColor();
						run.kind = NkRunKind::Square;
						run.cells.PushBack(math::NkVec2i(row, column));
						run.cells.PushBack(math::NkVec2i(row, column + 1));
						run.cells.PushBack(math::NkVec2i(row + 1, column));
						run.cells.PushBack(math::NkVec2i(row + 1, column + 1));
						runs.PushBack(run);
					}
				}
			}

			if (runs.Size() == 0) {
				return matches;
			}

			// ── 2. Regroupe les runs qui partagent une case (union simple —
			//      le plateau est petit, O(runs²) est largement suffisant) ─────
			NkVector<int32> clusterOf;
			for (typename NkVector<NkRunInfo>::SizeType i = 0; i < runs.Size(); ++i) {
				clusterOf.PushBack(static_cast<int32>(i));
			}

			auto Find = [&clusterOf](int32 i) noexcept {
				while (clusterOf[i] != i) {
					i = clusterOf[i];
				}
				return i;
			};

			for (typename NkVector<NkRunInfo>::SizeType i = 0; i < runs.Size(); ++i) {
				for (typename NkVector<NkRunInfo>::SizeType j = i + 1; j < runs.Size(); ++j) {
					if (RunsShareCell(runs[i], runs[j])) {
						const int32 rootI = Find(static_cast<int32>(i));
						const int32 rootJ = Find(static_cast<int32>(j));
						if (rootI != rootJ) {
							clusterOf[rootJ] = rootI;
						}
					}
				}
			}

			// ── 3. Un NkGemMatch par cluster, classé selon sa forme ────────────
			NkVector<int32> processedRoots;
			for (typename NkVector<NkRunInfo>::SizeType i = 0; i < runs.Size(); ++i) {
				const int32 root = Find(static_cast<int32>(i));
				bool alreadyProcessed = false;
				for (typename NkVector<int32>::SizeType k = 0; k < processedRoots.Size(); ++k) {
					if (processedRoots[k] == root) {
						alreadyProcessed = true;
						break;
					}
				}
				if (alreadyProcessed) {
					continue;
				}
				processedRoots.PushBack(root);

				NkVector<math::NkVec2i> clusterCells;
				int32 horizontalRunCount = 0;
				int32 verticalRunCount = 0;
				int32 squareRunCount = 0;
				int32 lastLineRunIndex = -1;

				for (typename NkVector<NkRunInfo>::SizeType a = 0; a < runs.Size(); ++a) {
					if (Find(static_cast<int32>(a)) != root) {
						continue;
					}
					if (runs[a].kind == NkRunKind::Horizontal) {
						++horizontalRunCount;
						lastLineRunIndex = static_cast<int32>(a);
					} else if (runs[a].kind == NkRunKind::Vertical) {
						++verticalRunCount;
						lastLineRunIndex = static_cast<int32>(a);
					} else {
						++squareRunCount;
					}
					for (typename NkVector<math::NkVec2i>::SizeType c = 0; c < runs[a].cells.Size(); ++c) {
						const math::NkVec2i &cell = runs[a].cells[c];
						if (!CellListContains(clusterCells, cell.x, cell.y)) {
							clusterCells.PushBack(cell);
						}
					}
				}

				NkGemMatch match;
				match.cells = clusterCells;
				match.color = clusterCells.Size() > 0 ? mBoard.GetGem(clusterCells[0].x, clusterCells[0].y)->GetColor()
													   : NkGemColor::NK_GEM_COLOR_NONE;

				const math::NkVec2i fallbackSpawn =
					clusterCells.Size() > 0 ? clusterCells[clusterCells.Size() / 2] : math::NkVec2i(-1, -1);

				if (horizontalRunCount >= 1 && verticalRunCount >= 1) {
					// Forme en L/T : un run horizontal ET un run vertical se croisent -> Enveloppée.
					// Priorité la plus haute : c'est la forme la plus "riche" possible ici.
					math::NkVec2i intersectionCell = fallbackSpawn;
					bool hasIntersection = false;
					for (typename NkVector<NkRunInfo>::SizeType a = 0; a < runs.Size() && !hasIntersection; ++a) {
						if (Find(static_cast<int32>(a)) != root || runs[a].kind != NkRunKind::Horizontal) {
							continue;
						}
						for (typename NkVector<NkRunInfo>::SizeType b = 0; b < runs.Size() && !hasIntersection; ++b) {
							if (Find(static_cast<int32>(b)) != root || runs[b].kind != NkRunKind::Vertical) {
								continue;
							}
							for (typename NkVector<math::NkVec2i>::SizeType ca = 0; ca < runs[a].cells.Size() && !hasIntersection;
								 ++ca) {
								if (CellListContains(runs[b].cells, runs[a].cells[ca].x, runs[a].cells[ca].y)) {
									intersectionCell = runs[a].cells[ca];
									hasIntersection = true;
								}
							}
						}
					}
					match.specialToCreate = NkGemSpecialKind::NK_GEM_SPECIAL_WRAPPED;
					match.spawnCell = intersectionCell;
				} else if (squareRunCount >= 1) {
					// Bloc rectangulaire (carré 2x2, ou plusieurs carrés adjacents
					// fusionnés en un bloc plus grand) sans ligne de 3+ associée -> Poisson.
					match.specialToCreate = NkGemSpecialKind::NK_GEM_SPECIAL_FISH;
					match.spawnCell = fallbackSpawn;
				} else {
					const int32 length = static_cast<int32>(clusterCells.Size());
					const bool isHorizontalRun =
						runs[static_cast<typename NkVector<NkRunInfo>::SizeType>(lastLineRunIndex)].kind == NkRunKind::Horizontal;
					if (length >= 5) {
						match.specialToCreate = NkGemSpecialKind::NK_GEM_SPECIAL_COLOR_BOMB;
						match.spawnCell = fallbackSpawn;
					} else if (length == 4) {
						match.specialToCreate = isHorizontalRun ? NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_HORIZONTAL
																 : NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_VERTICAL;
						match.spawnCell = fallbackSpawn;
					} else {
						match.specialToCreate = NkGemSpecialKind::NK_GEM_SPECIAL_NONE;
						match.spawnCell = math::NkVec2i(-1, -1);
					}
				}

				// Préfère une case d'échange joueur si elle appartient au cluster :
				// la spéciale apparaît alors là où le joueur a joué son coup.
				for (typename NkVector<math::NkVec2i>::SizeType p = 0; p < mPreferredSpawnCells.Size(); ++p) {
					if (CellListContains(clusterCells, mPreferredSpawnCells[p].x, mPreferredSpawnCells[p].y)) {
						match.spawnCell = mPreferredSpawnCells[p];
						break;
					}
				}

				matches.PushBack(match);
			}

			return matches;
		}

	} // namespace game
} // namespace nkentseu