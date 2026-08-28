// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/NkGem.h
// DESCRIPTION: Classe NkGem — une gemme du plateau (style Candy Crush).
//              Couleur logique, forme, état, position de grille et animation
//              légère (échange, chute, apparition, disparition).
//
//              v2 : NkGem NE CONNAÎT PAS NkEvent. Elle ne fait pas son propre
//              test de survol — c'est NkGemBoard qui résout la case concernée
//              en O(1) via WorldToCell() et appelle les hooks ci-dessous.
//
//              v3 (2026-08-27) : NkGem NE DESSINE PLUS. Elle portait trois
//              NkShape (cercle, rectangle, polygone) plus quatre formes
//              d'overlay, soit sept objets de rendu par gemme, et le dessin
//              plat qui en sortait ressemblait à un prototype. Le dessin est
//              passé à Ui/NkGemArt, qui lit l'état d'une gemme sans rien
//              savoir de ses entrailles.
//
//              CE QUE ÇA CHANGE, ET C'EST LE POINT : il n'existe plus qu'UN
//              SEUL chemin de dessin. Deux chemins auraient divergé au premier
//              changement de thème, et rien ne l'aurait signalé.
//
// AUTEUR: Rihen
// DATE: 2026-08-26 (v3 : 2026-08-27)
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_GAME_NKGEM_H
#define NKENTSEU_GAME_NKGEM_H

#include "NKCore/NkTypes.h"
#include "NKMath/NKMath.h"
#include "NKContainers/Functional/NkFunction.h"

namespace nkentseu {
	namespace game {

		// =================================================================
		// Enums : couleur logique, forme visuelle, état, animation
		// =================================================================
		enum class NkGemColor : uint8 {
			NK_GEM_COLOR_NONE,
			NK_GEM_COLOR_RED,
			NK_GEM_COLOR_GREEN,
			NK_GEM_COLOR_BLUE,
			NK_GEM_COLOR_YELLOW,
			NK_GEM_COLOR_PURPLE,
			NK_GEM_COLOR_ORANGE,
			NK_GEM_COLOR_COUNT
		};

		enum class NkGemShape : uint8 {
			NK_GEM_SHAPE_CIRCLE,
			NK_GEM_SHAPE_DIAMOND,
			NK_GEM_SHAPE_SQUARE,
			NK_GEM_SHAPE_TRIANGLE,
			NK_GEM_SHAPE_STAR,
			NK_GEM_SHAPE_HEXAGON,
			NK_GEM_SHAPE_COUNT
		};

		enum class NkGemState : uint8 {
			NK_GEM_STATE_IDLE,
			NK_GEM_STATE_HOVERED,
			NK_GEM_STATE_PRESSED,
			NK_GEM_STATE_SELECTED,
			NK_GEM_STATE_MATCHED,
			NK_GEM_STATE_REMOVED
		};

		/// @brief Type de gemme spéciale (créée en combinant plus de 3 gemmes).
		/// Règles (cf. Candy Crush Saga — recherche documentée) :
		///   - 4 alignées (ligne ou colonne)      -> Rayée (oriente le tir)
		///   - 5+ alignées en ligne droite         -> Bombe Couleur
		///   - Forme en L/T (ligne + colonne qui se croisent) -> Enveloppée
		///   - Bloc rectangulaire >= 2x2 (carré)   -> Poisson
		/// NOTE : le Poisson est simplifié — pas de « nage » vers une cible ;
		/// il efface directement jusqu'à 3 gemmes de la couleur la plus rare
		/// du plateau (voir NkGemBoard::FindRarestColorOnBoard).
		enum class NkGemSpecialKind : uint8 {
			NK_GEM_SPECIAL_NONE,
			NK_GEM_SPECIAL_STRIPED_HORIZONTAL,
			NK_GEM_SPECIAL_STRIPED_VERTICAL,
			NK_GEM_SPECIAL_WRAPPED,
			NK_GEM_SPECIAL_COLOR_BOMB,
			NK_GEM_SPECIAL_FISH
		};

		/// @brief Nature de l'animation en cours sur une gemme (une seule à la fois)
		enum class NkGemAnimationKind : uint8 {
			NK_GEM_ANIM_NONE,
			NK_GEM_ANIM_MOVE,	 ///< Interpolation de position (échange, retour, chute)
			NK_GEM_ANIM_MATCHED, ///< Rétrécit + s'estompe avant suppression
			NK_GEM_ANIM_SPAWN	 ///< Grandit + apparaît (nouvelle gemme)
		};

		// =================================================================
		// Classe : NkGem
		// =================================================================
		class NkGem {
			public:
				/// @brief Notifié une fois qu'une animation se termine (voir Update())
				using NkGemAnimationCallback = NkFunction<void(NkGem &)>;

				NkGem(int32 id, NkGemColor color, NkGemShape shape);
				virtual ~NkGem() = default;

				// -----------------------------------------------------------------
				// Identité / grille
				// -----------------------------------------------------------------
				int32 GetId() const noexcept {
					return mId;
				}

				void SetGridPosition(int32 row, int32 column) noexcept {
					mRow = row;
					mColumn = column;
				}

				int32 GetRow() const noexcept {
					return mRow;
				}

				int32 GetColumn() const noexcept {
					return mColumn;
				}

				// -----------------------------------------------------------------
				// Couleur / forme / état
				// -----------------------------------------------------------------
				NkGemColor GetColor() const noexcept {
					return mColor;
				}

				void SetColor(NkGemColor color) noexcept {
					mColor = color;
				}

				NkGemShape GetShape() const noexcept {
					return mShape;
				}

				void SetShape(NkGemShape shape) noexcept {
					mShape = shape;
				}

				NkGemState GetState() const noexcept {
					return mState;
				}

				void SetState(NkGemState state) noexcept {
					mState = state;
				}

				// -----------------------------------------------------------------
				// Gemme spéciale (créée par un match de 4+, ou en L/T)
				// -----------------------------------------------------------------
				NkGemSpecialKind GetSpecialKind() const noexcept {
					return mSpecialKind;
				}

				void SetSpecialKind(NkGemSpecialKind kind) noexcept {
					mSpecialKind = kind;
				}

				bool IsSpecial() const noexcept {
					return mSpecialKind != NkGemSpecialKind::NK_GEM_SPECIAL_NONE;
				}

				// -----------------------------------------------------------------
				// Position / taille
				// -----------------------------------------------------------------
				void SetSize(float32 size) noexcept {
					mSize = size;
				}

				float32 GetSize() const noexcept {
					return mSize;
				}

				/// @brief Positionne la gemme IMMÉDIATEMENT (sans animation).
				void SetPosition(const math::NkVec2f &position) noexcept {
					mPosition = position;
				}

				/// @brief Replace la gemme sur sa case APRÈS un changement de mise
				///        en page, même si elle est en train de bouger.
				///
				/// ⚠️ Une animation de DÉPLACEMENT vise une position calculée sur
				/// l'ANCIENNE grille : la laisser finir ramènerait la gemme à un
				/// endroit qui n'existe plus. On l'abandonne donc — perdre une
				/// interpolation de 0,26 s au moment d'une rotation d'écran ne se
				/// voit pas ; une gemme posée à côté de sa case se voit toujours.
				/// Les animations d'apparition et de disparition, elles, ne
				/// déplacent rien : on les laisse aller au bout.
				void SnapTo(const math::NkVec2f &position) noexcept {
					mPosition = position;
					if (mAnimationKind == NkGemAnimationKind::NK_GEM_ANIM_MOVE) {
						mAnimationKind = NkGemAnimationKind::NK_GEM_ANIM_NONE;
						mRenderScale = 1.f;
						mRenderAlpha = 1.f;
					}
				}

				math::NkVec2f GetPosition() const noexcept {
					return mPosition;
				}

				/// @brief Boîte englobante à l'écran (utile aux tests et au débogage).
				math::NkRect2f GetGlobalBounds() const noexcept {
					return math::NkRect2f(mPosition.x - mSize * 0.5f, mPosition.y - mSize * 0.5f, mSize, mSize);
				}

				// -----------------------------------------------------------------
				// Animation — pilotée par NkGemBoard, jamais par la gemme elle-même
				// -----------------------------------------------------------------
				void PlayMoveAnimation(const math::NkVec2f &fromPosition, const math::NkVec2f &toPosition,
									   float32 durationSeconds);
				void PlayMatchedAnimation(float32 durationSeconds);
				void PlaySpawnAnimation(float32 durationSeconds);

				bool IsAnimating() const noexcept {
					return mAnimationKind != NkGemAnimationKind::NK_GEM_ANIM_NONE;
				}

				NkGemAnimationKind GetAnimationKind() const noexcept {
					return mAnimationKind;
				}

				void SetAnimationCompleteCallback(NkGemAnimationCallback callback) {
					mAnimationCompleteCallback = static_cast<NkGemAnimationCallback &&>(callback);
				}

				/// @brief Avance l'animation en cours (à appeler chaque frame)
				void Update(float32 deltaTime);

				// -----------------------------------------------------------------
				// État VISUEL — le contrat entre la logique et l'apparence.
				//
				// Ui/NkGemArt lit ces deux valeurs pour dessiner ; il ne connaît
				// rien d'autre de NkGem. C'est ce qui permet de refaire tout le
				// rendu du jeu sans toucher une seule règle.
				// -----------------------------------------------------------------
				float32 GetRenderScale() const noexcept {
					return mRenderScale;
				}

				float32 GetRenderAlpha() const noexcept {
					return mRenderAlpha;
				}

				// -----------------------------------------------------------------
				// Hooks gameplay — appelés par NkGemBoard (case déjà résolue)
				// -----------------------------------------------------------------
				virtual void OnHoverEnter();
				virtual void OnHoverExit();
				virtual void OnPressed();
				virtual void OnReleased();
				/// @brief Pressé puis relâché sur la MÊME case (clic/tap simple)
				virtual void OnClicked();

			private:
				int32 mId;
				NkGemColor mColor;
				NkGemShape mShape;
				NkGemState mState = NkGemState::NK_GEM_STATE_IDLE;
				NkGemSpecialKind mSpecialKind = NkGemSpecialKind::NK_GEM_SPECIAL_NONE;

				int32 mRow = -1;
				int32 mColumn = -1;
				float32 mSize = 64.f;
				math::NkVec2f mPosition{0.f, 0.f};

				// Animation
				NkGemAnimationKind mAnimationKind = NkGemAnimationKind::NK_GEM_ANIM_NONE;
				float32 mAnimationElapsed = 0.f;
				float32 mAnimationDuration = 0.f;
				math::NkVec2f mAnimationFrom{0.f, 0.f};
				math::NkVec2f mAnimationTo{0.f, 0.f};
				float32 mRenderScale = 1.f;
				float32 mRenderAlpha = 1.f;
				NkGemAnimationCallback mAnimationCompleteCallback;
		};

	} // namespace game
} // namespace nkentseu

#endif // NKENTSEU_GAME_NKGEM_H
