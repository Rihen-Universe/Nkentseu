// -----------------------------------------------------------------------------
// FICHIER: Game\NkGemBoard.h
// DESCRIPTION: Classe NkGemBoard — plateau / grille de jeu (style Candy Crush).
//
//              NkGemBoard est LA SEULE classe qui connaît NkEvent. Elle
//              résout la case concernée par un événement moteur en O(1) via
//              WorldToCell() (pas de boucle sur toutes les gemmes) et
//              n'appelle que les hooks de la ou des gemmes réellement
//              concernées — les voisins ne sont impliqués que quand la
//              logique de jeu l'exige (résolution d'un alignement complet).
//
//              NkGemBoard pilote aussi une petite machine à états
//              (NkGemBoardPhase) qui orchestre échange -> vérification ->
//              (retour si invalide | suppression animée -> chute animée ->
//              re-vérification), permettant les réactions en chaîne tout en
//              gardant NkGem totalement ignorante de cette logique.
//
// AUTEUR: Rihen
// DATE: 2026-08-26
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------
//
// DÉPENDANCES:
//   - Game/NkGem.h                          : NkGem, NkGemColor, NkGemState
//   - NKContainers/Sequential/NkVector.h     : NkVector (conteneur zero-STL)
//   - NKWindow/Core/NkEvent.h                : NkEvent + événements concrets
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_NKGEMBOARD_H
#define NKENTSEU_GAME_NKGEMBOARD_H

#include "NkGem.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKGui/Core/NkGuiDrawList.h" // le plateau se dessine dans la liste de dessin NKGui

namespace nkentseu {
	namespace game {

		// Déclaration anticipée — définie dans NkMatchFinder.h, utilisée par
		// référence uniquement dans les signatures publiques de ce fichier.
		struct NkGemMatch;

		/// @brief Phase courante de résolution du plateau
		enum class NkGemBoardPhase : uint8 {
			NK_BOARD_PHASE_IDLE,		   ///< Prêt à recevoir une entrée
			NK_BOARD_PHASE_SWAPPING,	   ///< Deux gemmes s'échangent (animation)
			NK_BOARD_PHASE_REVERTING,	   ///< L'échange n'a rien aligné -> retour animé
			NK_BOARD_PHASE_RESOLVING_MATCH, ///< Gemmes alignées en train de disparaître
			NK_BOARD_PHASE_FALLING		   ///< Gemmes qui chutent / nouvelles gemmes qui apparaissent
		};

		// =================================================================
		// Classe : NkGemBoard
		// =================================================================
		class NkGemBoard {
			public:
				using NkGemColorFactory = NkFunction<NkGemColor(int32 row, int32 column)>;
				using NkGemMatchCallback = NkFunction<void(const NkVector<NkGemMatch> &matches)>;

				NkGemBoard(int32 rowCount, int32 columnCount, float32 cellSize);
				~NkGemBoard();

				NkGemBoard(const NkGemBoard &) = delete;
				NkGemBoard &operator=(const NkGemBoard &) = delete;

				// -----------------------------------------------------------------
				// Dimensions et positionnement écran
				// -----------------------------------------------------------------
				int32 GetRowCount() const noexcept {
					return mRowCount;
				}

				int32 GetColumnCount() const noexcept {
					return mColumnCount;
				}

				float32 GetCellSize() const noexcept {
					return mCellSize;
				}

				void SetOrigin(const math::NkVec2f &origin) noexcept;

				math::NkVec2f GetOrigin() const noexcept {
					return mOrigin;
				}

				/// @brief Change la taille de case à chaud (rotation d'écran, resize
				///        fenêtre...) et replace/redimensionne toutes les gemmes.
				///        À utiliser pour un plateau responsive (mobile portrait).
				void SetCellSize(float32 cellSize) noexcept;

				math::NkVec2f CellToWorld(int32 row, int32 column) const noexcept;

				/// @brief O(1) — résout directement la case sous un point écran
				bool WorldToCell(const math::NkVec2f &worldPosition, int32 &outRow, int32 &outColumn) const noexcept;

				bool IsValidCell(int32 row, int32 column) const noexcept;

				/// @brief true si le plateau est disponible pour une nouvelle entrée
				///        (aucun échange/chute/résolution en cours)
				bool IsIdle() const noexcept {
					return mPhase == NkGemBoardPhase::NK_BOARD_PHASE_IDLE;
				}

				NkGemBoardPhase GetPhase() const noexcept {
					return mPhase;
				}

				// -----------------------------------------------------------------
				// Accès aux gemmes
				// -----------------------------------------------------------------
				NkGem *GetGem(int32 row, int32 column) const noexcept;
				void SetGem(int32 row, int32 column, NkGem *gem);

				/// @brief Remplit chaque cellule vide ; mémorise colorFactory/shape
				///        pour les régénérations ultérieures (chute + réapprovisionnement)
				void Fill(NkGemColorFactory colorFactory, NkGemShape shape = NkGemShape::NK_GEM_SHAPE_CIRCLE);

				/// @brief Notifié à chaque fois qu'un ou plusieurs alignements sont
				///        supprimés (utile pour le score côté application)
				void SetMatchCallback(NkGemMatchCallback callback) {
					mMatchCallback = static_cast<NkGemMatchCallback &&>(callback);
				}

				// -----------------------------------------------------------------
				// Requêtes de grille
				// -----------------------------------------------------------------
				bool AreAdjacent(int32 rowA, int32 columnA, int32 rowB, int32 columnB) const noexcept;

				/// @brief Échange instantané (SANS animation) — utile pour la
				///        simulation (NkMatchFinder::WouldSwapCreateMatch) ou l'IA.
				///        Pour un échange JOUEUR avec animation, voir OnEvent().
				bool TrySwap(int32 rowA, int32 columnA, int32 rowB, int32 columnB);

				// -----------------------------------------------------------------
				// Cycle de vie : à appeler chaque frame par l'application
				// -----------------------------------------------------------------
				/// @brief Avance les animations de toutes les gemmes et fait
				///        progresser la machine à états du plateau
				void Update(float32 deltaTime);

				/// @brief Dessine toutes les gemmes vivantes.
				/// @param time horloge du jeu (pulsations de selection, rotation de
				///        la couronne des bombes couleur).
				/// @note Le CADRE et le damier ne sont PAS dessines ici : ils
				///       appartiennent au HUD (Ui/NkGemHud::DrawBoardFrame), qui
				///       doit passer AVANT cet appel.
				void Draw(nkgui::NkGuiDrawList &drawList, float32 time) const;

				/// @brief Point d'entrée UNIQUE pour tout événement moteur (souris,
				///        tactile, clavier). Résout la case concernée en O(1) et ne
				///        transmet qu'aux gemmes réellement concernées.
				/// @return true si l'événement a été pris en compte par le plateau
				bool OnEvent(const NkEvent &event);

				// -----------------------------------------------------------------
				// GLISSER-DÉPOSER — souris et tactile, un seul geste
				//
				// Deux façons de jouer coexistent, et c'est voulu : le TAP (une
				// gemme puis sa voisine) reste disponible au clavier et à la
				// souris, le GLISSÉ est le geste naturel au doigt. Les deux
				// finissent dans SelectOrSwapCell/BeginSwapAnimation : il n'y a
				// pas deux chemins de règles, seulement deux entrées.
				// -----------------------------------------------------------------
				/// @brief Un glissé est en cours (le doigt/bouton est baissé sur une case).
				bool IsDragging() const noexcept {
					return mPointerDown && mDragOriginCell.x >= 0;
				}

				/// @brief Case actuellement VISÉE par le glissé (x=ligne, y=colonne),
				///        {-1,-1} si aucune. Le HUD la met en avant.
				math::NkVec2i GetDragTargetCell() const noexcept {
					return mDragTargetCell;
				}

				/// @brief Case sélectionnée par tap, {-1,-1} si aucune.
				math::NkVec2i GetSelectedCell() const noexcept {
					return mHasSelectedCell ? mSelectedCell : math::NkVec2i(-1, -1);
				}

				/// @brief Case survolée (souris), {-1,-1} si aucune.
				math::NkVec2i GetHoveredCell() const noexcept {
					return mHasHoveredCell ? mHoveredCell : math::NkVec2i(-1, -1);
				}

				/// @brief Décalage visuel à appliquer à la gemme (row, column) parce
				///        qu'un glissé est en cours. La gemme tirée suit le doigt, sa
				///        voisine recule : c'est ce qui rend l'échange lisible AVANT
				///        d'être validé.
				math::NkVec2f GetVisualOffset(int32 row, int32 column) const noexcept;

				/// @brief 0..1 — la gemme est-elle « soulevée » (tirée ou sélectionnée) ?
				float32 GetLift(int32 row, int32 column) const noexcept;

				// -----------------------------------------------------------------
				// Aides au joueur
				// -----------------------------------------------------------------
				/// @brief Cherche un échange jouable.
				/// @return false si le plateau est BLOQUÉ (aucun coup possible) —
				///         l'appelant doit alors mélanger.
				bool FindHintMove(math::NkVec2i &outCellA, math::NkVec2i &outCellB);

				/// @brief Rebat les couleurs jusqu'à obtenir un plateau jouable et
				///        sans alignement immédiat.
				void Shuffle();

				/// @brief Remet le plateau à neuf (bouton REJOUER) : détruit toutes
				///        les gemmes et refait un remplissage avec la fabrique de
				///        couleurs mémorisée par Fill().
				/// @note Remet AUSSI à zéro la sélection, le glissé, la phase et le
				///       compteur de cascade — un état d'entrée oublié fait rejouer
				///       un coup fantôme à la première touche de la partie suivante.
				void Reset();

				/// @brief Nombre de résolutions enchaînées depuis le dernier coup
				///        (1 = échange simple, 2+ = cascade). Sert au combo du HUD.
				int32 GetCascadeCount() const noexcept {
					return mCascadeCount;
				}

			private:
				int32 mRowCount;
				int32 mColumnCount;
				float32 mCellSize;
				math::NkVec2f mOrigin{0.f, 0.f};

				NkVector<NkGem *> mCells; ///< Taille mRowCount * mColumnCount, rangement ligne-major
				int32 mNextGemId = 0;

				NkGemColorFactory mColorFactory;
				NkGemShape mDefaultGemShape = NkGemShape::NK_GEM_SHAPE_CIRCLE;
				NkGemMatchCallback mMatchCallback;

				/// @brief Une gemme spéciale en attente de création après suppression
				///        des cases qui ont formé le match (voir BeginResolveMatches).
				struct NkPendingSpecialSpawn {
						math::NkVec2i cell{-1, -1};
						NkGemSpecialKind kind = NkGemSpecialKind::NK_GEM_SPECIAL_NONE;
						NkGemColor color = NkGemColor::NK_GEM_COLOR_NONE;
				};
				NkVector<NkPendingSpecialSpawn> mPendingSpecialSpawns;

				// ── Survol / sélection (souris + tactile + clavier unifiés) ──────
				bool mHasHoveredCell = false;
				math::NkVec2i mHoveredCell{0, 0};
				bool mHasSelectedCell = false;
				math::NkVec2i mSelectedCell{0, 0};

				// ── Glissé en cours ─────────────────────────────────────────────
				bool mPointerDown = false;	  ///< bouton/doigt baissé
				bool mDragCommitted = false;   ///< l'échange a déjà été lancé par le glissé
				math::NkVec2f mPointerDownPos{0.f, 0.f};
				math::NkVec2f mPointerPos{0.f, 0.f};
				math::NkVec2i mDragOriginCell{-1, -1};
				math::NkVec2i mDragTargetCell{-1, -1};
				int32 mCascadeCount = 0;
				math::NkVec2i mCursorCell{0, 0}; ///< Case courante pour la navigation clavier

				// ── Machine à états de résolution ─────────────────────────────────
				NkGemBoardPhase mPhase = NkGemBoardPhase::NK_BOARD_PHASE_IDLE;
				math::NkVec2i mPendingSwapA{0, 0};
				math::NkVec2i mPendingSwapB{0, 0};
				NkVector<math::NkVec2i> mPendingMatchedCells;

				static constexpr float32 NK_GEM_SWAP_DURATION_SECONDS = 0.16f;
				static constexpr float32 NK_GEM_MATCH_DURATION_SECONDS = 0.20f;
				static constexpr float32 NK_GEM_FALL_DURATION_SECONDS = 0.26f;
				static constexpr float32 NK_GEM_SPAWN_DURATION_SECONDS = 0.20f;

				int32 IndexOf(int32 row, int32 column) const noexcept {
					return row * mColumnCount + column;
				}

				NkGem *&SlotAt(int32 row, int32 column) noexcept {
					return mCells[static_cast<NkVector<NkGem *>::SizeType>(IndexOf(row, column))];
				}

				NkGem *CreateGem(NkGemColor color, NkGemShape shape);

				/// @brief Seuil de glissé, en fraction de case. En dessous, le geste
				///        est un TAP. 0,30 : mesuré confortable au doigt sans
				///        déclencher sur un tremblement.
				static constexpr float32 NK_GEM_DRAG_THRESHOLD_RATIO = 0.30f;

				// ── Traduction événement -> case (O(1)) ────────────────────────
				void UpdateHover(const math::NkVec2f &worldPosition);
				bool HandlePointerDown(const math::NkVec2f &worldPosition);
				bool HandlePointerMove(const math::NkVec2f &worldPosition);
				bool HandlePointerUp(const math::NkVec2f &worldPosition);
				void ResetDragState() noexcept;

				/// @brief Déplacement du pointeur depuis l'appui, ramené à l'axe
				///        DOMINANT et borné à une case. Une diagonale n'existe pas
				///        dans un match-3 : la laisser passer produirait un échange
				///        que le joueur n'a pas voulu.
				math::NkVec2f ComputeDragDelta() const noexcept;
				bool HandleDirectionalKey(NkKey key);

				/// @brief Logique de sélection/échange commune souris+tactile+clavier
				void SelectOrSwapCell(int32 row, int32 column);

				// ── Machine à états ─────────────────────────────────────────────
				void BeginSwapAnimation(int32 rowA, int32 columnA, int32 rowB, int32 columnB);
				void SwapCellsDataOnly(int32 rowA, int32 columnA, int32 rowB, int32 columnB) noexcept;
				bool IsCellAnimating(int32 row, int32 column) const noexcept;
				bool AnyGemAnimating() const noexcept;
				void BeginResolveMatches(const NkVector<NkGemMatch> &matches);
				void BeginFallAndRefill();
				void CheckForMatchesOrIdle();

				// ── Gemmes spéciales (recherche Candy Crush — voir NkGem.h) ──────
				/// @brief Cases affectées par l'activation d'une gemme spéciale à
				///        (row, column). colorBombOverride permet de forcer la couleur
				///        ciblée par une Bombe Couleur (règle : "échangée avec N'IMPORTE
				///        quelle gemme -> efface la couleur de CETTE gemme").
				NkVector<math::NkVec2i> ComputeSpecialEffectCells(const NkGem *gem, int32 row, int32 column,
																	NkGemColor colorBombOverride = NkGemColor::NK_GEM_COLOR_NONE) const;
				NkVector<math::NkVec2i> ComputeColorCells(NkGemColor color) const;

				/// @brief Couleur la moins présente actuellement sur le plateau
				///        (utilisée par le Poisson — voir ComputeSpecialEffectCells).
				NkGemColor FindRarestColorOnBoard() const noexcept;

				/// @brief Étend un ensemble de cases : toute gemme spéciale présente
				///        parmi elles voit son effet ajouté à l'ensemble (en boucle,
				///        pour gérer les réactions en chaîne spéciale -> spéciale).
				NkVector<math::NkVec2i> ExpandWithSpecialActivations(NkVector<math::NkVec2i> cells) const;

				/// @brief Gère un échange où au moins une des deux gemmes est spéciale
				///        (toujours un coup valide, même sans alignement classique).
				void ResolveSpecialSwap(int32 rowA, int32 columnA, int32 rowB, int32 columnB);
		};

	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_NKGEMBOARD_H