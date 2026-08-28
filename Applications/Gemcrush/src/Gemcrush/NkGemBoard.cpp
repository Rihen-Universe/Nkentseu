// -----------------------------------------------------------------------------
// FICHIER: Game\NkGemBoard.cpp
// DESCRIPTION: Implémentation de NkGemBoard (v2).
// AUTEUR: Rihen
// DATE: 2026-08-26
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NkGemBoard.h"
#include "NkMatchFinder.h"
#include "NKMath/NkRandom.h"
#include "Gemcrush/Ui/NkGemArt.h"
#include "Gemcrush/Ui/NkGemTheme.h"

namespace nkentseu {
	namespace game {

		// =====================================================================
		// Petite aide locale : une case (row,column) est-elle déjà dans la liste ?
		// (math::NkVec2i n'a pas nécessairement operator== accessible ici, on
		// compare donc x/y explicitement — évite toute dépendance fragile.)
		// =====================================================================
		static bool ContainsCell(const NkVector<math::NkVec2i> &cells, int32 row, int32 column) noexcept {
			for (typename NkVector<math::NkVec2i>::SizeType i = 0; i < cells.Size(); ++i) {
				if (cells[i].x == row && cells[i].y == column) {
					return true;
				}
			}
			return false;
		}

		// =====================================================================
		// Construction / destruction
		// =====================================================================
		NkGemBoard::NkGemBoard(int32 rowCount, int32 columnCount, float32 cellSize)
			: mRowCount(rowCount), mColumnCount(columnCount), mCellSize(cellSize) {
			mCells.Resize(static_cast<NkVector<NkGem *>::SizeType>(mRowCount * mColumnCount));
			for (int32 i = 0; i < mRowCount * mColumnCount; ++i) {
				mCells[i] = nullptr;
			}
		}

		NkGemBoard::~NkGemBoard() {
			for (int32 i = 0; i < mRowCount * mColumnCount; ++i) {
				delete mCells[i];
				mCells[i] = nullptr;
			}
		}

		// =====================================================================
		// Dimensions et positionnement écran
		// =====================================================================
		// ---------------------------------------------------------------------
		// SetOrigin / SetCellSize : TOUTES les gemmes suivent la nouvelle grille.
		//
		// ⚠️ DEFAUT MESURE (signale sur Android, 2026-08-28) : SetOrigin ne
		// replacait que les gemmes AU REPOS. Une mise en page qui change pendant
		// une chute ou un melange laissait donc les gemmes concernees sur
		// l'ANCIENNE grille — definitivement, puisque leur animation se terminait
		// a une cible perimee. Le cadre bougeait, les gemmes non.
		//
		// C'est frequent sur mobile et rare sur PC : la surface Android change de
		// taille apres les premieres trames (barres systeme qui se stabilisent),
		// c'est-a-dire souvent pendant une animation.
		// ---------------------------------------------------------------------
		void NkGemBoard::SetOrigin(const math::NkVec2f &origin) noexcept {
			mOrigin = origin;
			SnapAllGemsToGrid();
		}

		void NkGemBoard::SnapAllGemsToGrid() noexcept {
			for (int32 row = 0; row < mRowCount; ++row) {
				for (int32 column = 0; column < mColumnCount; ++column) {
					NkGem *gem = GetGem(row, column);
					if (gem != nullptr) {
						gem->SnapTo(CellToWorld(row, column));
					}
				}
			}
		}

		math::NkVec2f NkGemBoard::CellToWorld(int32 row, int32 column) const noexcept {
			return math::NkVec2f(mOrigin.x + static_cast<float32>(column) * mCellSize + mCellSize * 0.5f,
								  mOrigin.y + static_cast<float32>(row) * mCellSize + mCellSize * 0.5f);
		}

		void NkGemBoard::SetCellSize(float32 cellSize) noexcept {
			if (cellSize <= 0.f) {
				return;
			}
			mCellSize = cellSize;
			for (int32 row = 0; row < mRowCount; ++row) {
				for (int32 column = 0; column < mColumnCount; ++column) {
					NkGem *gem = GetGem(row, column);
					if (gem != nullptr) {
						gem->SetSize(mCellSize * ui::NkTheme().gemInset);
						gem->SnapTo(CellToWorld(row, column));
					}
				}
			}
		}

		bool NkGemBoard::WorldToCell(const math::NkVec2f &worldPosition, int32 &outRow, int32 &outColumn) const noexcept {
			const float32 localX = worldPosition.x - mOrigin.x;
			const float32 localY = worldPosition.y - mOrigin.y;
			if (localX < 0.f || localY < 0.f || mCellSize <= 0.f) {
				return false;
			}
			const int32 column = static_cast<int32>(localX / mCellSize);
			const int32 row = static_cast<int32>(localY / mCellSize);
			if (!IsValidCell(row, column)) {
				return false;
			}
			outRow = row;
			outColumn = column;
			return true;
		}

		bool NkGemBoard::IsValidCell(int32 row, int32 column) const noexcept {
			return row >= 0 && row < mRowCount && column >= 0 && column < mColumnCount;
		}

		// =====================================================================
		// Accès aux gemmes
		// =====================================================================
		NkGem *NkGemBoard::GetGem(int32 row, int32 column) const noexcept {
			if (!IsValidCell(row, column)) {
				return nullptr;
			}
			return mCells[static_cast<NkVector<NkGem *>::SizeType>(IndexOf(row, column))];
		}

		void NkGemBoard::SetGem(int32 row, int32 column, NkGem *gem) {
			if (!IsValidCell(row, column)) {
				delete gem;
				return;
			}
			NkGem *&slot = SlotAt(row, column);
			delete slot;
			slot = gem;
			if (gem != nullptr) {
				gem->SetGridPosition(row, column);
				gem->SetSize(mCellSize * 0.85f);
				gem->SetPosition(CellToWorld(row, column));
			}
		}

		NkGem *NkGemBoard::CreateGem(NkGemColor color, NkGemShape shape) {
			// La FORME suit la COULEUR : deux couleurs n'ont jamais la meme
			// silhouette (cf. NkGemShapeForColor). `shape` n'est plus qu'un repli
			// pour une couleur inconnue — un joueur daltonien joue alors avec
			// exactement les memes informations que les autres.
			const NkGemShape resolved =
				(color != NkGemColor::NK_GEM_COLOR_NONE) ? ui::NkGemShapeForColor(color) : shape;
			NkGem *gem = new NkGem(mNextGemId++, color, resolved);
			gem->SetSize(mCellSize * ui::NkTheme().gemInset);
			return gem;
		}

		void NkGemBoard::Fill(NkGemColorFactory colorFactory, NkGemShape shape) {
			mColorFactory = static_cast<NkGemColorFactory &&>(colorFactory);
			mDefaultGemShape = shape;
			for (int32 row = 0; row < mRowCount; ++row) {
				for (int32 column = 0; column < mColumnCount; ++column) {
					if (GetGem(row, column) != nullptr) {
						continue;
					}
					const NkGemColor color = mColorFactory ? mColorFactory(row, column) : NkGemColor::NK_GEM_COLOR_RED;
					SetGem(row, column, CreateGem(color, mDefaultGemShape));
				}
			}
		}

		// =====================================================================
		// Requêtes de grille
		// =====================================================================
		bool NkGemBoard::AreAdjacent(int32 rowA, int32 columnA, int32 rowB, int32 columnB) const noexcept {
			const int32 deltaRow = (rowA > rowB) ? (rowA - rowB) : (rowB - rowA);
			const int32 deltaColumn = (columnA > columnB) ? (columnA - columnB) : (columnB - columnA);
			return (deltaRow + deltaColumn) == 1;
		}

		bool NkGemBoard::TrySwap(int32 rowA, int32 columnA, int32 rowB, int32 columnB) {
			if (!IsValidCell(rowA, columnA) || !IsValidCell(rowB, columnB) || !AreAdjacent(rowA, columnA, rowB, columnB)) {
				return false;
			}
			NkGem *&slotA = SlotAt(rowA, columnA);
			NkGem *&slotB = SlotAt(rowB, columnB);
			NkGem *temporary = slotA;
			slotA = slotB;
			slotB = temporary;
			if (slotA != nullptr) {
				slotA->SetGridPosition(rowA, columnA);
				slotA->SetPosition(CellToWorld(rowA, columnA));
			}
			if (slotB != nullptr) {
				slotB->SetGridPosition(rowB, columnB);
				slotB->SetPosition(CellToWorld(rowB, columnB));
			}
			return true;
		}

		void NkGemBoard::SwapCellsDataOnly(int32 rowA, int32 columnA, int32 rowB, int32 columnB) noexcept {
			NkGem *&slotA = SlotAt(rowA, columnA);
			NkGem *&slotB = SlotAt(rowB, columnB);
			NkGem *temporary = slotA;
			slotA = slotB;
			slotB = temporary;
			if (slotA != nullptr) {
				slotA->SetGridPosition(rowA, columnA);
			}
			if (slotB != nullptr) {
				slotB->SetGridPosition(rowB, columnB);
			}
		}

		// =====================================================================
		// Traduction événement -> case (O(1)) + sélection
		// =====================================================================
		void NkGemBoard::UpdateHover(const math::NkVec2f &worldPosition) {
			int32 row = 0, column = 0;
			const bool found = WorldToCell(worldPosition, row, column);
			if (mHasHoveredCell && found && mHoveredCell.x == row && mHoveredCell.y == column) {
				return; // toujours la même case : rien à faire
			}
			if (mHasHoveredCell) {
				NkGem *previous = GetGem(mHoveredCell.x, mHoveredCell.y);
				if (previous != nullptr) {
					previous->OnHoverExit();
				}
			}
			mHasHoveredCell = found;
			if (found) {
				mHoveredCell = math::NkVec2i(row, column);
				NkGem *gem = GetGem(row, column);
				if (gem != nullptr) {
					gem->OnHoverEnter();
				}
			}
		}

		void NkGemBoard::ResetDragState() noexcept {
			mPointerDown = false;
			mDragCommitted = false;
			mDragOriginCell = math::NkVec2i(-1, -1);
			mDragTargetCell = math::NkVec2i(-1, -1);
		}

		// ---------------------------------------------------------------------
		// ComputeDragDelta : deplacement depuis l'appui, ramene a l'axe DOMINANT
		// et borne a UNE case.
		//
		// Pourquoi l'axe dominant plutot que le vecteur brut : un match-3
		// n'echange que des voisines orthogonales. Un doigt qui part en diagonale
		// doit se voir attribuer un sens, sinon on echange une case que le joueur
		// n'a pas visee — et il n'a aucun moyen de comprendre pourquoi.
		// ---------------------------------------------------------------------
		math::NkVec2f NkGemBoard::ComputeDragDelta() const noexcept {
			const float32 dx = mPointerPos.x - mPointerDownPos.x;
			const float32 dy = mPointerPos.y - mPointerDownPos.y;
			const float32 adx = (dx < 0.f) ? -dx : dx;
			const float32 ady = (dy < 0.f) ? -dy : dy;
			if (adx >= ady) {
				return math::NkVec2f(math::NkClamp(dx, -mCellSize, mCellSize), 0.f);
			}
			return math::NkVec2f(0.f, math::NkClamp(dy, -mCellSize, mCellSize));
		}

		bool NkGemBoard::HandlePointerDown(const math::NkVec2f &worldPosition) {
			if (!IsIdle()) {
				return false; // le plateau est occupé (échange/chute en cours) : on ignore
			}
			int32 row = 0, column = 0;
			if (!WorldToCell(worldPosition, row, column)) {
				// Appui HORS plateau : on annule la selection en cours. Sans cela,
				// une selection oubliee provoque un echange surprise au prochain
				// appui, parfois plusieurs secondes plus tard.
				if (mHasSelectedCell) {
					NkGem *previous = GetGem(mSelectedCell.x, mSelectedCell.y);
					if (previous != nullptr) {
						previous->SetState(NkGemState::NK_GEM_STATE_IDLE);
					}
					mHasSelectedCell = false;
				}
				return false;
			}
			NkGem *gem = GetGem(row, column);
			if (gem == nullptr) {
				return false;
			}

			// L'appui ARME le geste, il ne le decide pas : c'est le relachement
			// (tap) ou le franchissement du seuil (glisse) qui tranchera.
			mPointerDown = true;
			mDragCommitted = false;
			mPointerDownPos = worldPosition;
			mPointerPos = worldPosition;
			mDragOriginCell = math::NkVec2i(row, column);
			mDragTargetCell = math::NkVec2i(-1, -1);
			gem->OnPressed();
			return true;
		}

		bool NkGemBoard::HandlePointerMove(const math::NkVec2f &worldPosition) {
			mPointerPos = worldPosition;
			UpdateHover(worldPosition);

			if (!mPointerDown || mDragCommitted || mDragOriginCell.x < 0) {
				return false;
			}
			if (!IsIdle()) {
				return false; // une resolution a demarre entre-temps
			}

			const math::NkVec2f delta = ComputeDragDelta();
			const float32 length = (delta.x != 0.f) ? ((delta.x < 0.f) ? -delta.x : delta.x)
													: ((delta.y < 0.f) ? -delta.y : delta.y);
			const float32 threshold = mCellSize * NK_GEM_DRAG_THRESHOLD_RATIO;

			// Case visee : la voisine dans le sens du glisse, des qu'on a depasse
			// un tiers du seuil — l'apercu s'allume AVANT la validation.
			math::NkVec2i target(-1, -1);
			if (length > threshold * 0.34f) {
				const int32 stepRow = (delta.y > 0.f) ? 1 : ((delta.y < 0.f) ? -1 : 0);
				const int32 stepColumn = (delta.x > 0.f) ? 1 : ((delta.x < 0.f) ? -1 : 0);
				const int32 targetRow = mDragOriginCell.x + stepRow;
				const int32 targetColumn = mDragOriginCell.y + stepColumn;
				if (IsValidCell(targetRow, targetColumn) && GetGem(targetRow, targetColumn) != nullptr) {
					target = math::NkVec2i(targetRow, targetColumn);
				}
			}
			mDragTargetCell = target;

			// Seuil franchi ET voisine valide : on echange SANS attendre le
			// relachement. C'est ce qui donne la reponse immediate qu'on attend au
			// doigt ; attendre le relachement se ressent comme de la latence.
			if (length >= threshold && target.x >= 0) {
				NkGem *origin = GetGem(mDragOriginCell.x, mDragOriginCell.y);
				NkGem *neighbour = GetGem(target.x, target.y);
				if (origin != nullptr && neighbour != nullptr) {
					if (mHasSelectedCell) {
						NkGem *previous = GetGem(mSelectedCell.x, mSelectedCell.y);
						if (previous != nullptr) {
							previous->SetState(NkGemState::NK_GEM_STATE_IDLE);
						}
						mHasSelectedCell = false;
					}
					origin->SetState(NkGemState::NK_GEM_STATE_IDLE);
					neighbour->SetState(NkGemState::NK_GEM_STATE_IDLE);

					const int32 rowA = mDragOriginCell.x;
					const int32 columnA = mDragOriginCell.y;
					const int32 rowB = target.x;
					const int32 columnB = target.y;
					mDragCommitted = true;
					mDragTargetCell = math::NkVec2i(-1, -1);

					// Une speciale impliquee : le coup est valide meme sans alignement
					// (regle Candy Crush) — exactement la meme porte que le tap.
					if (origin->IsSpecial() || neighbour->IsSpecial()) {
						ResolveSpecialSwap(rowA, columnA, rowB, columnB);
					} else {
						BeginSwapAnimation(rowA, columnA, rowB, columnB);
						mPhase = NkGemBoardPhase::NK_BOARD_PHASE_SWAPPING;
					}
					return true;
				}
			}
			return true;
		}

		bool NkGemBoard::HandlePointerUp(const math::NkVec2f &worldPosition) {
			mPointerPos = worldPosition;

			const bool wasPressed = mPointerDown;
			const bool committed = mDragCommitted;
			const math::NkVec2i origin = mDragOriginCell;
			ResetDragState();

			int32 row = 0, column = 0;
			const bool onBoard = WorldToCell(worldPosition, row, column);
			if (onBoard) {
				NkGem *gem = GetGem(row, column);
				if (gem != nullptr) {
					gem->OnReleased();
				}
			}
			if (!wasPressed || committed || origin.x < 0) {
				return onBoard; // l'echange a deja eu lieu pendant le glisse
			}

			// Sous le seuil : c'est un TAP. La case de reference est celle de
			// l'APPUI, pas celle du relachement — un doigt bouge toujours de
			// quelques pixels, et rien ne doit changer pour autant.
			NkGem *originGem = GetGem(origin.x, origin.y);
			if (originGem != nullptr &&
				!(mHasSelectedCell && mSelectedCell.x == origin.x && mSelectedCell.y == origin.y)) {
				originGem->SetState(NkGemState::NK_GEM_STATE_IDLE);
			}
			SelectOrSwapCell(origin.x, origin.y);
			return true;
		}

		// ---------------------------------------------------------------------
		// Decalage visuel pendant un glisse
		// ---------------------------------------------------------------------
		math::NkVec2f NkGemBoard::GetVisualOffset(int32 row, int32 column) const noexcept {
			if (!mPointerDown || mDragCommitted || mDragOriginCell.x < 0) {
				return math::NkVec2f(0.f, 0.f);
			}
			const math::NkVec2f delta = ComputeDragDelta();
			if (row == mDragOriginCell.x && column == mDragOriginCell.y) {
				return delta;
			}
			// La voisine visee RECULE un peu : l'echange se lit avant d'etre
			// valide, ce qui evite le coup joue par erreur.
			if (mDragTargetCell.x == row && mDragTargetCell.y == column) {
				return math::NkVec2f(-delta.x * 0.55f, -delta.y * 0.55f);
			}
			return math::NkVec2f(0.f, 0.f);
		}

		float32 NkGemBoard::GetLift(int32 row, int32 column) const noexcept {
			if (mPointerDown && !mDragCommitted && mDragOriginCell.x == row && mDragOriginCell.y == column) {
				return 1.f;
			}
			if (mHasSelectedCell && mSelectedCell.x == row && mSelectedCell.y == column) {
				return 0.6f;
			}
			return 0.f;
		}

		bool NkGemBoard::HandleDirectionalKey(NkKey key) {
			if (!IsIdle()) {
				return false;
			}
			switch (key) {
				case NkKey::NK_UP:
					mCursorCell.x = (mCursorCell.x > 0) ? mCursorCell.x - 1 : 0;
					return true;
				case NkKey::NK_DOWN:
					mCursorCell.x = (mCursorCell.x < mRowCount - 1) ? mCursorCell.x + 1 : mRowCount - 1;
					return true;
				case NkKey::NK_LEFT:
					mCursorCell.y = (mCursorCell.y > 0) ? mCursorCell.y - 1 : 0;
					return true;
				case NkKey::NK_RIGHT:
					mCursorCell.y = (mCursorCell.y < mColumnCount - 1) ? mCursorCell.y + 1 : mColumnCount - 1;
					return true;
				case NkKey::NK_ENTER:
				case NkKey::NK_SPACE:
					SelectOrSwapCell(mCursorCell.x, mCursorCell.y);
					return true;
				default:
					return false;
			}
		}

		void NkGemBoard::SelectOrSwapCell(int32 row, int32 column) {
			NkGem *gem = GetGem(row, column);
			if (gem == nullptr) {
				return;
			}

			if (!mHasSelectedCell) {
				mSelectedCell = math::NkVec2i(row, column);
				mHasSelectedCell = true;
				gem->SetState(NkGemState::NK_GEM_STATE_SELECTED);
				return;
			}

			const int32 selectedRow = mSelectedCell.x;
			const int32 selectedColumn = mSelectedCell.y;
			NkGem *selectedGem = GetGem(selectedRow, selectedColumn);

			// Reclic sur la même case : désélection.
			if (selectedRow == row && selectedColumn == column) {
				if (selectedGem != nullptr) {
					selectedGem->SetState(NkGemState::NK_GEM_STATE_IDLE);
				}
				gem->OnClicked();
				mHasSelectedCell = false;
				return;
			}

			// Case adjacente : on tente l'échange (animé).
			if (AreAdjacent(selectedRow, selectedColumn, row, column)) {
				if (selectedGem != nullptr) {
					selectedGem->SetState(NkGemState::NK_GEM_STATE_IDLE);
				}
				gem->SetState(NkGemState::NK_GEM_STATE_IDLE);
				mHasSelectedCell = false;
				BeginSwapAnimation(selectedRow, selectedColumn, row, column);
				mPhase = NkGemBoardPhase::NK_BOARD_PHASE_SWAPPING;
				return;
			}

			// Case non adjacente : on déplace simplement la sélection.
			if (selectedGem != nullptr) {
				selectedGem->SetState(NkGemState::NK_GEM_STATE_IDLE);
			}
			mSelectedCell = math::NkVec2i(row, column);
			gem->SetState(NkGemState::NK_GEM_STATE_SELECTED);
		}

		bool NkGemBoard::OnEvent(const NkEvent &event) {
			if (const auto *mouseMove = event.As<NkMouseMoveEvent>()) {
				HandlePointerMove(
					math::NkVec2f(static_cast<float32>(mouseMove->GetX()), static_cast<float32>(mouseMove->GetY())));
				return true;
			}
			if (const auto *mousePress = event.As<NkMouseButtonPressEvent>()) {
				if (mousePress->GetButton() != NkMouseButton::NK_MB_LEFT) {
					return false;
				}
				return HandlePointerDown(
					math::NkVec2f(static_cast<float32>(mousePress->GetX()), static_cast<float32>(mousePress->GetY())));
			}
			if (const auto *mouseRelease = event.As<NkMouseButtonReleaseEvent>()) {
				if (mouseRelease->GetButton() != NkMouseButton::NK_MB_LEFT) {
					return false;
				}
				return HandlePointerUp(
					math::NkVec2f(static_cast<float32>(mouseRelease->GetX()), static_cast<float32>(mouseRelease->GetY())));
			}
			if (const auto *touchBegin = event.As<NkTouchBeginEvent>()) {
				if (touchBegin->GetNumTouches() == 0) {
					return false;
				}
				const NkTouchPoint &touch = touchBegin->GetTouch(0);
				return HandlePointerDown(math::NkVec2f(touch.clientX, touch.clientY));
			}
			if (const auto *touchMove = event.As<NkTouchMoveEvent>()) {
				if (touchMove->GetNumTouches() == 0) {
					return false;
				}
				const NkTouchPoint &touch = touchMove->GetTouch(0);
				HandlePointerMove(math::NkVec2f(touch.clientX, touch.clientY));
				return true;
			}
			if (const auto *touchEnd = event.As<NkTouchEndEvent>()) {
				if (touchEnd->GetNumTouches() == 0) {
					return false;
				}
				const NkTouchPoint &touch = touchEnd->GetTouch(0);
				return HandlePointerUp(math::NkVec2f(touch.clientX, touch.clientY));
			}
			if (event.As<NkTouchCancelEvent>() != nullptr) {
				// Le systeme reprend le doigt (appel entrant, geste systeme) : on
				// ANNULE le glisse sans jouer de coup. Sans ce cas, le plateau reste
				// convaincu qu'un doigt est pose et la gemme suivante part toute
				// seule au premier deplacement.
				if (mPointerDown && mDragOriginCell.x >= 0) {
					NkGem *gem = GetGem(mDragOriginCell.x, mDragOriginCell.y);
					if (gem != nullptr) {
						gem->OnReleased();
					}
				}
				ResetDragState();
				return true;
			}
			if (const auto *keyPress = event.As<NkKeyPressEvent>()) {
				return HandleDirectionalKey(keyPress->GetKey());
			}
			return false;
		}

		// =====================================================================
		// Machine à états : échange, résolution, chute, réactions en chaîne
		// =====================================================================
		bool NkGemBoard::IsCellAnimating(int32 row, int32 column) const noexcept {
			const NkGem *gem = GetGem(row, column);
			return gem != nullptr && gem->IsAnimating();
		}

		bool NkGemBoard::AnyGemAnimating() const noexcept {
			for (int32 i = 0; i < mRowCount * mColumnCount; ++i) {
				const NkGem *gem = mCells[static_cast<NkVector<NkGem *>::SizeType>(i)];
				if (gem != nullptr && gem->IsAnimating()) {
					return true;
				}
			}
			return false;
		}

		void NkGemBoard::BeginSwapAnimation(int32 rowA, int32 columnA, int32 rowB, int32 columnB) {
			const math::NkVec2f worldA = CellToWorld(rowA, columnA);
			const math::NkVec2f worldB = CellToWorld(rowB, columnB);

			SwapCellsDataOnly(rowA, columnA, rowB, columnB);

			// Après l'échange logique, la case A contient l'ex-occupant de B (et vice versa) :
			// on anime chacun DEPUIS son ancienne position visuelle VERS sa nouvelle case.
			NkGem *nowAtA = GetGem(rowA, columnA);
			NkGem *nowAtB = GetGem(rowB, columnB);
			if (nowAtA != nullptr) {
				nowAtA->PlayMoveAnimation(worldB, worldA, NK_GEM_SWAP_DURATION_SECONDS);
			}
			if (nowAtB != nullptr) {
				nowAtB->PlayMoveAnimation(worldA, worldB, NK_GEM_SWAP_DURATION_SECONDS);
			}

			mPendingSwapA = math::NkVec2i(rowA, columnA);
			mPendingSwapB = math::NkVec2i(rowB, columnB);
		}

		void NkGemBoard::BeginResolveMatches(const NkVector<NkGemMatch> &matches) {
			// 1 = alignement simple, 2+ = cascade. Remis a zero au retour a IDLE
			// (CheckForMatchesOrIdle) : le compteur mesure UN coup, pas la partie.
			++mCascadeCount;
			mPendingMatchedCells.Clear();
			mPendingSpecialSpawns.Clear();

			for (typename NkVector<NkGemMatch>::SizeType m = 0; m < matches.Size(); ++m) {
				const NkGemMatch &match = matches[m];
				for (typename NkVector<math::NkVec2i>::SizeType c = 0; c < match.cells.Size(); ++c) {
					const math::NkVec2i &cell = match.cells[c];
					if (!ContainsCell(mPendingMatchedCells, cell.x, cell.y)) {
						mPendingMatchedCells.PushBack(cell);
					}
				}
				if (match.specialToCreate != NkGemSpecialKind::NK_GEM_SPECIAL_NONE && IsValidCell(match.spawnCell.x, match.spawnCell.y)) {
					NkPendingSpecialSpawn spawn;
					spawn.cell = match.spawnCell;
					spawn.kind = match.specialToCreate;
					spawn.color = match.color;
					mPendingSpecialSpawns.PushBack(spawn);
				}
			}

			// Réactions en chaîne "spéciale -> spéciale" : toute gemme DÉJÀ spéciale
			// prise dans ce match voit son effet ajouté aux cases à supprimer.
			mPendingMatchedCells = ExpandWithSpecialActivations(mPendingMatchedCells);

			for (typename NkVector<math::NkVec2i>::SizeType i = 0; i < mPendingMatchedCells.Size(); ++i) {
				NkGem *gem = GetGem(mPendingMatchedCells[i].x, mPendingMatchedCells[i].y);
				if (gem != nullptr) {
					gem->PlayMatchedAnimation(NK_GEM_MATCH_DURATION_SECONDS);
				}
			}

			if (mMatchCallback) {
				mMatchCallback(matches);
			}

			mPhase = NkGemBoardPhase::NK_BOARD_PHASE_RESOLVING_MATCH;
		}

		// =====================================================================
		// Gemmes spéciales
		// =====================================================================
		NkVector<math::NkVec2i> NkGemBoard::ComputeColorCells(NkGemColor color) const {
			NkVector<math::NkVec2i> cells;
			for (int32 row = 0; row < mRowCount; ++row) {
				for (int32 column = 0; column < mColumnCount; ++column) {
					const NkGem *gem = GetGem(row, column);
					if (gem != nullptr && gem->GetColor() == color) {
						cells.PushBack(math::NkVec2i(row, column));
					}
				}
			}
			return cells;
		}

		NkGemColor NkGemBoard::FindRarestColorOnBoard() const noexcept {
			int32 counts[static_cast<int32>(NkGemColor::NK_GEM_COLOR_COUNT)] = {};
			for (int32 row = 0; row < mRowCount; ++row) {
				for (int32 column = 0; column < mColumnCount; ++column) {
					const NkGem *gem = GetGem(row, column);
					if (gem != nullptr && gem->GetColor() != NkGemColor::NK_GEM_COLOR_NONE) {
						++counts[static_cast<int32>(gem->GetColor())];
					}
				}
			}
			NkGemColor rarest = NkGemColor::NK_GEM_COLOR_NONE;
			int32 rarestCount = 0;
			for (int32 c = 1; c < static_cast<int32>(NkGemColor::NK_GEM_COLOR_COUNT); ++c) {
				if (counts[c] > 0 && (rarest == NkGemColor::NK_GEM_COLOR_NONE || counts[c] < rarestCount)) {
					rarestCount = counts[c];
					rarest = static_cast<NkGemColor>(c);
				}
			}
			return rarest;
		}

		NkVector<math::NkVec2i> NkGemBoard::ComputeSpecialEffectCells(const NkGem *gem, int32 row, int32 column,
																		NkGemColor colorBombOverride) const {
			NkVector<math::NkVec2i> cells;
			if (gem == nullptr) {
				return cells;
			}
			switch (gem->GetSpecialKind()) {
				case NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_HORIZONTAL:
					for (int32 c = 0; c < mColumnCount; ++c) {
						cells.PushBack(math::NkVec2i(row, c));
					}
					break;
				case NkGemSpecialKind::NK_GEM_SPECIAL_STRIPED_VERTICAL:
					for (int32 r = 0; r < mRowCount; ++r) {
						cells.PushBack(math::NkVec2i(r, column));
					}
					break;
				case NkGemSpecialKind::NK_GEM_SPECIAL_WRAPPED:
					for (int32 r = row - 1; r <= row + 1; ++r) {
						for (int32 c = column - 1; c <= column + 1; ++c) {
							if (IsValidCell(r, c)) {
								cells.PushBack(math::NkVec2i(r, c));
							}
						}
					}
					break;
				case NkGemSpecialKind::NK_GEM_SPECIAL_COLOR_BOMB: {
					const NkGemColor target = (colorBombOverride != NkGemColor::NK_GEM_COLOR_NONE) ? colorBombOverride : gem->GetColor();
					cells = ComputeColorCells(target);
					break;
				}
				case NkGemSpecialKind::NK_GEM_SPECIAL_FISH: {
					// Simplifié : pas de "nage"/pathfinding vers une cible précise —
					// efface jusqu'à 3 gemmes de la couleur la plus rare du plateau,
					// plus sa propre case.
					const NkGemColor target = FindRarestColorOnBoard();
					if (target != NkGemColor::NK_GEM_COLOR_NONE) {
						int32 found = 0;
						for (int32 r = 0; r < mRowCount && found < 3; ++r) {
							for (int32 c = 0; c < mColumnCount && found < 3; ++c) {
								const NkGem *other = GetGem(r, c);
								if (other != nullptr && other->GetColor() == target) {
									cells.PushBack(math::NkVec2i(r, c));
									++found;
								}
							}
						}
					}
					if (!ContainsCell(cells, row, column)) {
						cells.PushBack(math::NkVec2i(row, column));
					}
					break;
				}
				default:
					break;
			}
			return cells;
		}

		NkVector<math::NkVec2i> NkGemBoard::ExpandWithSpecialActivations(NkVector<math::NkVec2i> cells) const {
			bool changed = true;
			int32 safetyIterations = 0;
			while (changed && safetyIterations < 20) {
				changed = false;
				++safetyIterations;
				NkVector<math::NkVec2i> toAdd;
				for (typename NkVector<math::NkVec2i>::SizeType i = 0; i < cells.Size(); ++i) {
					const NkGem *gem = GetGem(cells[i].x, cells[i].y);
					if (gem == nullptr || !gem->IsSpecial()) {
						continue;
					}
					const NkVector<math::NkVec2i> effect = ComputeSpecialEffectCells(gem, cells[i].x, cells[i].y);
					for (typename NkVector<math::NkVec2i>::SizeType e = 0; e < effect.Size(); ++e) {
						if (!ContainsCell(cells, effect[e].x, effect[e].y) && !ContainsCell(toAdd, effect[e].x, effect[e].y)) {
							toAdd.PushBack(effect[e]);
						}
					}
				}
				if (toAdd.Size() > 0) {
					for (typename NkVector<math::NkVec2i>::SizeType a = 0; a < toAdd.Size(); ++a) {
						cells.PushBack(toAdd[a]);
					}
					changed = true;
				}
			}
			return cells;
		}

		void NkGemBoard::ResolveSpecialSwap(int32 rowA, int32 columnA, int32 rowB, int32 columnB) {
			NkGem *gemA = GetGem(rowA, columnA);
			NkGem *gemB = GetGem(rowB, columnB);
			const bool aSpecial = gemA != nullptr && gemA->IsSpecial();
			const bool bSpecial = gemB != nullptr && gemB->IsSpecial();

			NkVector<math::NkVec2i> cellsToClear;

			// Règle officielle : Bombe Couleur échangée avec une gemme NORMALE ->
			// cible la couleur de CETTE gemme (pas la couleur "mémorisée" par la bombe).
			if (aSpecial && gemA->GetSpecialKind() == NkGemSpecialKind::NK_GEM_SPECIAL_COLOR_BOMB && !bSpecial && gemB != nullptr) {
				cellsToClear = ComputeColorCells(gemB->GetColor());
				if (!ContainsCell(cellsToClear, rowA, columnA)) {
					cellsToClear.PushBack(math::NkVec2i(rowA, columnA));
				}
			} else if (bSpecial && gemB->GetSpecialKind() == NkGemSpecialKind::NK_GEM_SPECIAL_COLOR_BOMB && !aSpecial && gemA != nullptr) {
				cellsToClear = ComputeColorCells(gemA->GetColor());
				if (!ContainsCell(cellsToClear, rowB, columnB)) {
					cellsToClear.PushBack(math::NkVec2i(rowB, columnB));
				}
			} else {
				// Cas général (une rayée/enveloppée seule, ou deux spéciales ensemble) :
				// chaque spéciale active son propre effet à sa position post-échange.
				// NOTE : les combos "exacts" du vrai jeu (rayé+enveloppé = triple ligne,
				// enveloppé+enveloppé = 5x5...) ne sont pas reproduits ici — piste
				// d'amélioration possible ; cette approximation reste cohérente et
				// produit déjà un effet combiné crédible.
				if (aSpecial) {
					const NkVector<math::NkVec2i> effect = ComputeSpecialEffectCells(gemA, rowA, columnA);
					for (typename NkVector<math::NkVec2i>::SizeType i = 0; i < effect.Size(); ++i) {
						if (!ContainsCell(cellsToClear, effect[i].x, effect[i].y)) {
							cellsToClear.PushBack(effect[i]);
						}
					}
				}
				if (bSpecial) {
					const NkVector<math::NkVec2i> effect = ComputeSpecialEffectCells(gemB, rowB, columnB);
					for (typename NkVector<math::NkVec2i>::SizeType i = 0; i < effect.Size(); ++i) {
						if (!ContainsCell(cellsToClear, effect[i].x, effect[i].y)) {
							cellsToClear.PushBack(effect[i]);
						}
					}
				}
			}

			NkGemMatch synthetic;
			synthetic.cells = cellsToClear;
			synthetic.color = NkGemColor::NK_GEM_COLOR_NONE;
			synthetic.specialToCreate = NkGemSpecialKind::NK_GEM_SPECIAL_NONE;
			NkVector<NkGemMatch> single;
			single.PushBack(synthetic);
			BeginResolveMatches(single);
		}

		void NkGemBoard::BeginFallAndRefill() {
			for (int32 column = 0; column < mColumnCount; ++column) {
				// ── Compacte les gemmes restantes vers le bas (chute animée) ──────
				int32 writeRow = mRowCount - 1;
				for (int32 row = mRowCount - 1; row >= 0; --row) {
					NkGem *gem = GetGem(row, column);
					if (gem == nullptr) {
						continue;
					}
					if (writeRow != row) {
						const math::NkVec2f fromWorld = gem->GetPosition();
						SlotAt(writeRow, column) = gem;
						SlotAt(row, column) = nullptr;
						gem->SetGridPosition(writeRow, column);
						const math::NkVec2f toWorld = CellToWorld(writeRow, column);
						gem->PlayMoveAnimation(fromWorld, toWorld, NK_GEM_FALL_DURATION_SECONDS);
					}
					--writeRow;
				}

				// ── Régénère les cases vides restées en haut, en les faisant
				//    "tomber" depuis au-dessus du plateau visible (effet cascade) ──
				int32 spawnOffset = 1;
				for (int32 row = writeRow; row >= 0; --row) {
					const NkGemColor color = mColorFactory ? mColorFactory(row, column) : NkGemColor::NK_GEM_COLOR_RED;
					NkGem *gem = CreateGem(color, mDefaultGemShape);
					gem->SetGridPosition(row, column);
					gem->SetSize(mCellSize * 0.85f);
					const math::NkVec2f spawnWorld = CellToWorld(row - spawnOffset, column);
					gem->SetPosition(spawnWorld);
					SlotAt(row, column) = gem;
					const math::NkVec2f targetWorld = CellToWorld(row, column);
					gem->PlayMoveAnimation(spawnWorld, targetWorld, NK_GEM_FALL_DURATION_SECONDS);
					++spawnOffset;
				}
			}

			mPhase = NkGemBoardPhase::NK_BOARD_PHASE_FALLING;
		}

		void NkGemBoard::CheckForMatchesOrIdle() {
			NkMatchFinder finder(*this);
			const NkVector<NkGemMatch> matches = finder.FindMatches();
			if (matches.Size() > 0) {
				// Réaction en chaîne : la chute vient de créer un nouvel alignement.
				BeginResolveMatches(matches);
			} else {
				mPhase = NkGemBoardPhase::NK_BOARD_PHASE_IDLE;
				mCascadeCount = 0;
			}
		}

		void NkGemBoard::Update(float32 deltaTime) {
			for (int32 i = 0; i < mRowCount * mColumnCount; ++i) {
				NkGem *gem = mCells[static_cast<NkVector<NkGem *>::SizeType>(i)];
				if (gem != nullptr) {
					gem->Update(deltaTime);
				}
			}

			switch (mPhase) {
				case NkGemBoardPhase::NK_BOARD_PHASE_IDLE:
					break;

				case NkGemBoardPhase::NK_BOARD_PHASE_SWAPPING: {
					if (IsCellAnimating(mPendingSwapA.x, mPendingSwapA.y) || IsCellAnimating(mPendingSwapB.x, mPendingSwapB.y)) {
						break; // l'échange visuel n'est pas terminé
					}
					NkGem *gemAtA = GetGem(mPendingSwapA.x, mPendingSwapA.y);
					NkGem *gemAtB = GetGem(mPendingSwapB.x, mPendingSwapB.y);
					const bool eitherSpecial = (gemAtA != nullptr && gemAtA->IsSpecial()) || (gemAtB != nullptr && gemAtB->IsSpecial());

					if (eitherSpecial) {
						// Règle Candy Crush : échanger une gemme spéciale (avec n'importe
						// quelle autre) est TOUJOURS un coup valide, même sans alignement.
						ResolveSpecialSwap(mPendingSwapA.x, mPendingSwapA.y, mPendingSwapB.x, mPendingSwapB.y);
						break;
					}

					NkMatchFinder finder(*this);
					NkVector<math::NkVec2i> preferred;
					preferred.PushBack(mPendingSwapA);
					preferred.PushBack(mPendingSwapB);
					finder.SetPreferredSpawnCells(preferred);
					const NkVector<NkGemMatch> matches = finder.FindMatches();
					if (matches.Size() > 0) {
						BeginResolveMatches(matches);
					} else {
						// Aucun alignement produit : on annule (ré-échange animé).
						BeginSwapAnimation(mPendingSwapA.x, mPendingSwapA.y, mPendingSwapB.x, mPendingSwapB.y);
						mPhase = NkGemBoardPhase::NK_BOARD_PHASE_REVERTING;
					}
					break;
				}

				case NkGemBoardPhase::NK_BOARD_PHASE_REVERTING: {
					if (!IsCellAnimating(mPendingSwapA.x, mPendingSwapA.y) && !IsCellAnimating(mPendingSwapB.x, mPendingSwapB.y)) {
						mPhase = NkGemBoardPhase::NK_BOARD_PHASE_IDLE;
					}
					break;
				}

				case NkGemBoardPhase::NK_BOARD_PHASE_RESOLVING_MATCH: {
					if (AnyGemAnimating()) {
						break; // les gemmes alignées finissent de disparaître
					}
					// Supprime réellement les gemmes disparues.
					for (typename NkVector<math::NkVec2i>::SizeType i = 0; i < mPendingMatchedCells.Size(); ++i) {
						NkGem *&slot = SlotAt(mPendingMatchedCells[i].x, mPendingMatchedCells[i].y);
						delete slot;
						slot = nullptr;
					}
					mPendingMatchedCells.Clear();

					// Fait apparaître les gemmes spéciales méritées par ce coup, à leur
					// case dédiée (elles ne "tombent" pas : elles apparaissent en place).
					for (typename NkVector<NkPendingSpecialSpawn>::SizeType i = 0; i < mPendingSpecialSpawns.Size(); ++i) {
						const NkPendingSpecialSpawn &spawn = mPendingSpecialSpawns[i];
						NkGem *specialGem = CreateGem(spawn.color, mDefaultGemShape);
						specialGem->SetSpecialKind(spawn.kind);
						SetGem(spawn.cell.x, spawn.cell.y, specialGem);
					}
					mPendingSpecialSpawns.Clear();

					BeginFallAndRefill(); // met mPhase = FALLING
					break;
				}

				case NkGemBoardPhase::NK_BOARD_PHASE_FALLING: {
					if (AnyGemAnimating()) {
						break; // chute/apparition encore en cours
					}
					// Chute terminée : vérifie une éventuelle RÉACTION EN CHAÎNE
					// (les gemmes retombées peuvent avoir formé un nouvel alignement).
					CheckForMatchesOrIdle();
					break;
				}
			}
		}

		// =====================================================================
		// Rendu
		// =====================================================================
		// ---------------------------------------------------------------------
		// Dessin
		//
		// DEUX PASSES, et ce n'est pas cosmetique : la gemme qu'on tire au doigt
		// franchit la frontiere de sa case. Dessinee dans l'ordre de la grille,
		// elle passerait SOUS ses voisines a mi-course — le geste devient
		// illisible exactement au moment ou le joueur regarde.
		// ---------------------------------------------------------------------
		void NkGemBoard::Draw(nkgui::NkGuiDrawList &drawList, float32 time) const {
			const math::NkVec2i selected = GetSelectedCell();
			const math::NkVec2i hovered = GetHoveredCell();

			for (int32 pass = 0; pass < 2; ++pass) {
				for (int32 row = 0; row < mRowCount; ++row) {
					for (int32 column = 0; column < mColumnCount; ++column) {
						NkGem *gem = GetGem(row, column);
						if (gem == nullptr || gem->GetState() == NkGemState::NK_GEM_STATE_REMOVED) {
							continue;
						}
						const float32 lift = GetLift(row, column);
						const bool onTop = (lift > 0.f);
						if ((pass == 1) != onTop) {
							continue; // passe 0 : les gemmes posees. passe 1 : celles qui flottent.
						}

						const math::NkVec2f offset = GetVisualOffset(row, column);
						const math::NkVec2f position = gem->GetPosition();

						ui::NkGemVisual visual;
						visual.center = math::NkVec2f(position.x + offset.x, position.y + offset.y);
						visual.size = mCellSize * ui::NkTheme().gemInset;
						visual.color = gem->GetColor();
						visual.shape = gem->GetShape();
						visual.special = gem->GetSpecialKind();
						visual.scale = gem->GetRenderScale();
						visual.alpha = gem->GetRenderAlpha();
						visual.lift = lift;
						visual.selected = (selected.x == row && selected.y == column) ||
										  (mDragOriginCell.x == row && mDragOriginCell.y == column && mPointerDown &&
										   !mDragCommitted);
						visual.hovered = (hovered.x == row && hovered.y == column);
						visual.time = time;
						ui::NkGemArt::Draw(drawList, visual);
					}
				}
			}
		}

		// ---------------------------------------------------------------------
		// FindHintMove : premier echange jouable trouve.
		//
		// Rend AUSSI le verdict « plateau bloque » (false), qui est l'autre
		// usage utile : c'est lui qui declenche un melange automatique.
		// ---------------------------------------------------------------------
		bool NkGemBoard::FindHintMove(math::NkVec2i &outCellA, math::NkVec2i &outCellB) {
			NkMatchFinder finder(*this);
			for (int32 row = 0; row < mRowCount; ++row) {
				for (int32 column = 0; column < mColumnCount; ++column) {
					const NkGem *gem = GetGem(row, column);
					if (gem == nullptr) {
						continue;
					}
					// Deux voisines suffisent : tester droite et bas couvre toutes
					// les paires du plateau sans en examiner aucune deux fois.
					const int32 neighbourRows[2] = {row, row + 1};
					const int32 neighbourColumns[2] = {column + 1, column};
					for (int32 k = 0; k < 2; ++k) {
						const int32 r = neighbourRows[k];
						const int32 c = neighbourColumns[k];
						if (!IsValidCell(r, c)) {
							continue;
						}
						const NkGem *other = GetGem(r, c);
						if (other == nullptr) {
							continue;
						}
						// Une speciale se joue toujours : c'est un coup valide meme
						// sans alignement, donc le plateau n'est pas bloque.
						if (gem->IsSpecial() || other->IsSpecial() ||
							finder.WouldSwapCreateMatch(row, column, r, c)) {
							outCellA = math::NkVec2i(row, column);
							outCellB = math::NkVec2i(r, c);
							return true;
						}
					}
				}
			}
			outCellA = math::NkVec2i(-1, -1);
			outCellB = math::NkVec2i(-1, -1);
			return false;
		}

		// ---------------------------------------------------------------------
		// Reset : le plateau a neuf.
		// ---------------------------------------------------------------------
		void NkGemBoard::Reset() {
			for (int32 i = 0; i < mRowCount * mColumnCount; ++i) {
				delete mCells[static_cast<NkVector<NkGem *>::SizeType>(i)];
				mCells[static_cast<NkVector<NkGem *>::SizeType>(i)] = nullptr;
			}
			mPhase = NkGemBoardPhase::NK_BOARD_PHASE_IDLE;
			mHasSelectedCell = false;
			mHasHoveredCell = false;
			mCascadeCount = 0;
			mPendingMatchedCells.Clear();
			mPendingSpecialSpawns.Clear();
			ResetDragState();

			if (mColorFactory) {
				Fill(mColorFactory, mDefaultGemShape);
			}
			for (int32 row = 0; row < mRowCount; ++row) {
				for (int32 column = 0; column < mColumnCount; ++column) {
					NkGem *gem = GetGem(row, column);
					if (gem != nullptr) {
						gem->PlaySpawnAnimation(NK_GEM_SPAWN_DURATION_SECONDS);
					}
				}
			}
		}

		// ---------------------------------------------------------------------
		// Shuffle : rebat les couleurs deja presentes.
		//
		// On PERMUTE au lieu de retirer au hasard : le joueur retrouve ses
		// couleurs, seulement ailleurs. Retirer a neuf changerait la difficulte
		// du niveau au milieu de la partie.
		//
		// La boucle s'arrete des que le plateau est SANS alignement immediat ET
		// JOUABLE. Les deux conditions comptent : un plateau sans alignement mais
		// bloque renvoie le joueur au meme mur.
		// ---------------------------------------------------------------------
		void NkGemBoard::Shuffle() {
			if (!IsIdle()) {
				return;
			}
			NkVector<NkGem *> gems;
			for (int32 i = 0; i < mRowCount * mColumnCount; ++i) {
				NkGem *gem = mCells[static_cast<NkVector<NkGem *>::SizeType>(i)];
				if (gem != nullptr) {
					gems.PushBack(gem);
				}
			}
			if (gems.Size() < 2) {
				return;
			}

			math::NkVec2i hintA(-1, -1), hintB(-1, -1);
			const int32 kMaxAttempts = 64;
			for (int32 attempt = 0; attempt < kMaxAttempts; ++attempt) {
				// Melange de Fisher-Yates sur les COULEURS, en place.
				for (int32 i = static_cast<int32>(gems.Size()) - 1; i > 0; --i) {
					const uint32 j = math::NkRandom::Instance().NextUInt32(0u, static_cast<uint32>(i + 1));
					NkGem *a = gems[static_cast<NkVector<NkGem *>::SizeType>(i)];
					NkGem *b = gems[static_cast<NkVector<NkGem *>::SizeType>(j)];
					const NkGemColor color = a->GetColor();
					const NkGemSpecialKind special = a->GetSpecialKind();
					a->SetColor(b->GetColor());
					a->SetShape(ui::NkGemShapeForColor(b->GetColor()));
					a->SetSpecialKind(b->GetSpecialKind());
					b->SetColor(color);
					b->SetShape(ui::NkGemShapeForColor(color));
					b->SetSpecialKind(special);
				}

				NkMatchFinder finder(*this);
				if (finder.FindMatches().Size() == 0 && FindHintMove(hintA, hintB)) {
					break;
				}
			}

			// Petite apparition sur chaque gemme : sans elle, le plateau change
			// d'un coup et le joueur ne sait pas ce qui vient de se passer.
			for (typename NkVector<NkGem *>::SizeType i = 0; i < gems.Size(); ++i) {
				gems[i]->PlaySpawnAnimation(NK_GEM_SPAWN_DURATION_SECONDS);
			}
		}

	} // namespace game
} // namespace nkentseu