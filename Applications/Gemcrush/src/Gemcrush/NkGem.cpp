// -----------------------------------------------------------------------------
// FICHIER: Gemcrush/NkGem.cpp
// DESCRIPTION: Implémentation de NkGem (v3 — sans rendu, voir NkGem.h).
// AUTEUR: Rihen
// DATE: 2026-08-26 (v3 : 2026-08-27)
// LICENCE: Proprietary - All Rights Reserved (see LICENSE)
// -----------------------------------------------------------------------------

#include "NkGem.h"

namespace nkentseu {
	namespace game {

		// =====================================================================
		// Construction
		// =====================================================================
		NkGem::NkGem(int32 id, NkGemColor color, NkGemShape shape) : mId(id), mColor(color), mShape(shape) {
		}

		// =====================================================================
		// Animation
		// =====================================================================
		void NkGem::PlayMoveAnimation(const math::NkVec2f &fromPosition, const math::NkVec2f &toPosition,
									  float32 durationSeconds) {
			mAnimationKind = NkGemAnimationKind::NK_GEM_ANIM_MOVE;
			mAnimationElapsed = 0.f;
			mAnimationDuration = (durationSeconds > 0.f) ? durationSeconds : 0.001f;
			mAnimationFrom = fromPosition;
			mAnimationTo = toPosition;
			mRenderScale = 1.f;
			mRenderAlpha = 1.f;
			SetPosition(fromPosition);
		}

		void NkGem::PlayMatchedAnimation(float32 durationSeconds) {
			mAnimationKind = NkGemAnimationKind::NK_GEM_ANIM_MATCHED;
			mAnimationElapsed = 0.f;
			mAnimationDuration = (durationSeconds > 0.f) ? durationSeconds : 0.001f;
			SetState(NkGemState::NK_GEM_STATE_MATCHED);
			mRenderScale = 1.f;
			mRenderAlpha = 1.f;
		}

		void NkGem::PlaySpawnAnimation(float32 durationSeconds) {
			mAnimationKind = NkGemAnimationKind::NK_GEM_ANIM_SPAWN;
			mAnimationElapsed = 0.f;
			mAnimationDuration = (durationSeconds > 0.f) ? durationSeconds : 0.001f;
			mRenderScale = 0.f;
			mRenderAlpha = 0.f;
		}

		void NkGem::Update(float32 deltaTime) {
			if (mAnimationKind == NkGemAnimationKind::NK_GEM_ANIM_NONE) {
				return;
			}

			mAnimationElapsed += deltaTime;
			float32 t = mAnimationElapsed / mAnimationDuration;
			if (t > 1.f) {
				t = 1.f;
			}
			// Ease-out quadratique : progression rapide puis ralentie (naturelle).
			const float32 eased = 1.f - (1.f - t) * (1.f - t);

			switch (mAnimationKind) {
				case NkGemAnimationKind::NK_GEM_ANIM_MOVE: {
					SetPosition(math::NkVec2f(mAnimationFrom.x + (mAnimationTo.x - mAnimationFrom.x) * eased,
											  mAnimationFrom.y + (mAnimationTo.y - mAnimationFrom.y) * eased));
					break;
				}
				case NkGemAnimationKind::NK_GEM_ANIM_MATCHED: {
					// Petit gonflement AVANT de disparaître : une gemme qui
					// rétrécit seulement se lit comme un bogue d'affichage ;
					// gonfler puis éclater se lit comme une récompense.
					const float32 pop = (t < 0.30f) ? (1.f + t * 0.55f) : (1.f + 0.165f) * (1.f - (t - 0.30f) / 0.70f);
					mRenderScale = math::NkMax(0.f, pop);
					mRenderAlpha = 1.f - eased;
					break;
				}
				case NkGemAnimationKind::NK_GEM_ANIM_SPAWN: {
					// Léger dépassement puis retour : une gemme qui apparaît
					// exactement à sa taille finale a l'air posée, pas surgie.
					mRenderScale = eased * (1.f + 0.40f * (1.f - eased) * eased * 4.f);
					mRenderAlpha = eased;
					break;
				}
				default:
					break;
			}

			if (t >= 1.f) {
				const NkGemAnimationKind finishedKind = mAnimationKind;
				mAnimationKind = NkGemAnimationKind::NK_GEM_ANIM_NONE;
				if (finishedKind == NkGemAnimationKind::NK_GEM_ANIM_MATCHED) {
					SetState(NkGemState::NK_GEM_STATE_REMOVED);
					mRenderScale = 0.f;
					mRenderAlpha = 0.f;
				} else {
					mRenderScale = 1.f;
					mRenderAlpha = 1.f;
				}
				if (mAnimationCompleteCallback) {
					mAnimationCompleteCallback(*this);
				}
			}
		}

		// =====================================================================
		// Hooks gameplay par défaut
		// =====================================================================
		void NkGem::OnHoverEnter() {
			if (mState == NkGemState::NK_GEM_STATE_IDLE) {
				SetState(NkGemState::NK_GEM_STATE_HOVERED);
			}
		}

		void NkGem::OnHoverExit() {
			if (mState == NkGemState::NK_GEM_STATE_HOVERED) {
				SetState(NkGemState::NK_GEM_STATE_IDLE);
			}
		}

		void NkGem::OnPressed() {
			SetState(NkGemState::NK_GEM_STATE_PRESSED);
		}

		void NkGem::OnReleased() {
			if (mState == NkGemState::NK_GEM_STATE_PRESSED) {
				SetState(NkGemState::NK_GEM_STATE_IDLE);
			}
		}

		void NkGem::OnClicked() {
			// À surcharger pour une gemme spéciale (bombe, ligne, effet sonore...).
		}

	} // namespace game
} // namespace nkentseu
