#include "pch.h"
//
// NkGestureRecognizer.cpp
// =============================================================================
// Description :
//   Les contacts bruts deviennent des gestes. Voir NkGestureRecognizer.h pour
//   la definition de chaque geste et le choix des seuils.
//
// Auteur   : Rihen
// Copyright: (c) 2024-2026 Rihen. Tous droits reserves.
// =============================================================================

#include "NKEvent/NkGestureRecognizer.h"

#include "NKMath/NkFunctions.h"

namespace nkentseu {

	namespace {

		constexpr float32 kDegParRad = 57.29577951308232f;

		/// Leve le drapeau le temps d'un appel, et le baisse a TOUTES les sorties.
		struct NkGardeAppel {
				bool &drapeau;
				explicit NkGardeAppel(bool &d) noexcept : drapeau(d) {
					drapeau = true;
				}
				~NkGardeAppel() {
					drapeau = false;
				}
		};

		float32 Distance(float32 ax, float32 ay, float32 bx, float32 by) noexcept {
			const float32 dx = bx - ax;
			const float32 dy = by - ay;
			return math::NkSqrt(dx * dx + dy * dy);
		}

		/// Ramene un ecart d'angle dans ]-180, 180] : deux doigts qui passent
		/// l'axe horizontal font sauter atan2 de +180 a -180, et sans ce
		/// repliement la rotation cumulee ferait un tour entier en une trame.
		float32 Replier(float32 deg) noexcept {
			while (deg > 180.f) {
				deg -= 360.f;
			}
			while (deg <= -180.f) {
				deg += 360.f;
			}
			return deg;
		}

	} // namespace

	NkGestureRecognizer::NkGestureRecognizer() noexcept {
		Reset();
	}

	void NkGestureRecognizer::SetConfig(const NkGestureConfig &config) noexcept {
		mConfig = config;
	}

	const NkGestureConfig &NkGestureRecognizer::GetConfig() const noexcept {
		return mConfig;
	}

	void NkGestureRecognizer::SetSink(NkGestureSink sink) {
		mSink = traits::NkMove(sink);
	}

	void NkGestureRecognizer::Reset() noexcept {
		for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
			mContacts[i] = NkContact{};
		}
		mCount = 0;
		mSession = false;
		mCancelled = false;
		mMoved = false;
		mLongPressDone = false;
		mPanActive = false;
		mMaxFingers = 0;
		mPairValid = false;
		mPinchActive = false;
		mRotateActive = false;
		mScaleBase = 1.f;
		mScale = 1.f;
		mAngleBase = 0.f;
		mAngle = 0.f;
		mHasLastTap = false;
		mTravelX = 0.f;
		mTravelY = 0.f;
		mVelX = 0.f;
		mVelY = 0.f;
	}

	uint32 NkGestureRecognizer::GetActiveContactCount() const noexcept {
		return mCount;
	}

	bool NkGestureRecognizer::IsPanning() const noexcept {
		return mSession && mPanActive;
	}

	bool NkGestureRecognizer::IsPinching() const noexcept {
		return mSession && mPinchActive && mPairValid;
	}

	bool NkGestureRecognizer::IsRotating() const noexcept {
		return mSession && mRotateActive && mPairValid;
	}

	uint64 NkGestureRecognizer::GetEmittedCount() const noexcept {
		return mEmitted;
	}

	// =========================================================================
	// Petits outils
	// =========================================================================

	int32 NkGestureRecognizer::FindContact(uint64 id) const noexcept {
		for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
			if (mContacts[i].active && mContacts[i].id == id) {
				return static_cast<int32>(i);
			}
		}
		return -1;
	}

	int32 NkGestureRecognizer::FreeSlot() const noexcept {
		for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
			if (!mContacts[i].active) {
				return static_cast<int32>(i);
			}
		}
		return -1;
	}

	void NkGestureRecognizer::Centroid(float32 &cx, float32 &cy) const noexcept {
		float32 sx = 0.f;
		float32 sy = 0.f;
		uint32 n = 0;
		for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
			if (mContacts[i].active) {
				sx += mContacts[i].x;
				sy += mContacts[i].y;
				++n;
			}
		}
		if (n == 0) {
			cx = 0.f;
			cy = 0.f;
			return;
		}
		cx = sx / static_cast<float32>(n);
		cy = sy / static_cast<float32>(n);
	}

	bool NkGestureRecognizer::FirstPair(int32 &a, int32 &b) const noexcept {
		a = -1;
		b = -1;
		for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
			if (!mContacts[i].active) {
				continue;
			}
			if (a < 0) {
				a = static_cast<int32>(i);
			} else {
				b = static_cast<int32>(i);
				return true;
			}
		}
		return false;
	}

	/// Le nombre de doigts vient de changer : le centroide et la paire de
	/// reference se recalent, SANS emettre. Les valeurs cumulees (echelle,
	/// angle) repartent de la ou elles etaient : un doigt ajoute en plein
	/// pincement ne remet pas le zoom a 1.
	void NkGestureRecognizer::Rereference() noexcept {
		float32 cx = 0.f;
		float32 cy = 0.f;
		Centroid(cx, cy);
		mRefCx = cx;
		mRefCy = cy;
		mPrevCx = cx;
		mPrevCy = cy;

		int32 a = -1;
		int32 b = -1;
		if (!FirstPair(a, b)) {
			mPairValid = false;
			return;
		}
		const NkContact &ca = mContacts[a];
		const NkContact &cb = mContacts[b];
		mPairA = ca.id;
		mPairB = cb.id;
		const float32 d = Distance(ca.x, ca.y, cb.x, cb.y);
		// Deux doigts poses au meme pixel : pas de rapport a calculer. On garde
		// une distance de reference non nulle pour ne jamais diviser par zero.
		mPairD0 = d > 1.0e-3f ? d : 1.0e-3f;
		mPairAngle0 = math::NkAtan2(cb.y - ca.y, cb.x - ca.x) * kDegParRad;
		mScaleBase = mScale;
		mAngleBase = mAngle;
		mPairValid = true;
	}

	void NkGestureRecognizer::StartSession(float64 timeMs) noexcept {
		mSession = true;
		mCancelled = false;
		mMoved = false;
		mLongPressDone = false;
		mPanActive = false;
		mPinchActive = false;
		mRotateActive = false;
		mSessionStartMs = timeMs;
		mMaxFingers = 0;
		mScaleBase = 1.f;
		mScale = 1.f;
		mAngleBase = 0.f;
		mAngle = 0.f;
		mTravelX = 0.f;
		mTravelY = 0.f;
		mVelX = 0.f;
		mVelY = 0.f;
		mPrevMs = timeMs;
		Centroid(mStartCx, mStartCy);
		Rereference();
	}

	void NkGestureRecognizer::Emit(NkEvent &event) noexcept {
		++mEmitted;
		++mEmittedThisCall;
		if (mSink) {
			mSink(event);
		}
	}

	// =========================================================================
	// Appui long : le temps, pas un evenement
	// =========================================================================

	uint32 NkGestureRecognizer::CheckLongPress(float64 timeMs) noexcept {
		if (!mSession || mCancelled || mMoved || mLongPressDone || mCount == 0) {
			return 0;
		}
		const float64 duree = timeMs - mSessionStartMs;
		if (duree < static_cast<float64>(mConfig.longPressMs)) {
			return 0;
		}
		mLongPressDone = true;
		NkGestureLongPressEvent e(mStartCx, mStartCy, static_cast<float32>(duree), mCount, mWindowId);
		Emit(e);
		return 1;
	}

	uint32 NkGestureRecognizer::Update(float64 timeMs) noexcept {
		if (mDansAppel) {
			return 0;
		}
		const NkGardeAppel garde(mDansAppel);
		mEmittedThisCall = 0;
		CheckLongPress(timeMs);
		return mEmittedThisCall;
	}

	// =========================================================================
	// Le mouvement : pan, pincer, tourner
	// =========================================================================

	uint32 NkGestureRecognizer::ProcessMotion(float64 timeMs) noexcept {
		const uint32 avant = mEmittedThisCall;
		if (!mSession || mCancelled || mCount == 0) {
			return 0;
		}

		// ── Un doigt a-t-il quitte la tolerance ? ──
		if (!mMoved) {
			for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
				const NkContact &c = mContacts[i];
				if (c.active && Distance(c.x0, c.y0, c.x, c.y) > mConfig.slopPx) {
					mMoved = true;
					break;
				}
			}
		}

		// ── Trajet et vitesse du centroide ──
		float32 cx = 0.f;
		float32 cy = 0.f;
		Centroid(cx, cy);
		const float32 pasX = cx - mPrevCx;
		const float32 pasY = cy - mPrevCy;
		mTravelX += pasX;
		mTravelY += pasY;
		const float64 dt = timeMs - mPrevMs;
		if (dt > 0.0) {
			// Moyenne glissante : un seul echantillon par trame est trop bruite
			// pour une inertie, et la moyenne de tout le geste oublie le coup
			// de doigt final.
			const float32 vx = static_cast<float32>(pasX * 1000.0 / dt);
			const float32 vy = static_cast<float32>(pasY * 1000.0 / dt);
			mVelX = 0.6f * vx + 0.4f * mVelX;
			mVelY = 0.6f * vy + 0.4f * mVelY;
			mPrevMs = timeMs;
		}
		mPrevCx = cx;
		mPrevCy = cy;

		// ── Pan ──
		if (mMoved && !mLongPressDone) {
			const float32 dx = cx - mRefCx;
			const float32 dy = cy - mRefCy;
			if (!mPanActive || dx != 0.f || dy != 0.f) {
				mPanActive = true;
				NkGesturePanEvent e(dx, dy, mVelX, mVelY, mCount, mWindowId);
				mRefCx = cx;
				mRefCy = cy;
				Emit(e);
			}
		}

		// ── Pincer et tourner : la paire de reference ──
		int32 a = -1;
		int32 b = -1;
		if (FirstPair(a, b)) {
			if (!mPairValid || mContacts[a].id != mPairA || mContacts[b].id != mPairB) {
				Rereference();
			}
			const NkContact &ca = mContacts[a];
			const NkContact &cb = mContacts[b];
			const float32 d = Distance(ca.x, ca.y, cb.x, cb.y);
			const float32 rapport = d / mPairD0;
			const float32 echelle = mScaleBase * rapport;
			const float32 ecartDeg = Replier(math::NkAtan2(cb.y - ca.y, cb.x - ca.x) * kDegParRad - mPairAngle0);
			const float32 angle = mAngleBase + ecartDeg;
			const float32 mx = 0.5f * (ca.x + cb.x);
			const float32 my = 0.5f * (ca.y + cb.y);

			if (!mPinchActive && math::NkFabs(rapport - 1.f) >= mConfig.pinchStartRatio) {
				mPinchActive = true;
				mMoved = true;
			}
			if (mPinchActive && echelle != mScale) {
				NkGesturePinchEvent e(echelle, echelle - mScale, mx, my, mWindowId);
				mScale = echelle;
				Emit(e);
			}

			if (!mRotateActive && math::NkFabs(ecartDeg) >= mConfig.rotateStartDeg) {
				mRotateActive = true;
				mMoved = true;
			}
			if (mRotateActive && angle != mAngle) {
				NkGestureRotateEvent e(angle, angle - mAngle, mWindowId);
				mAngle = angle;
				Emit(e);
			}
		}
		return mEmittedThisCall - avant;
	}

	// =========================================================================
	// Fin de session : balayage, puis tape
	// =========================================================================

	void NkGestureRecognizer::EndSession(float64 timeMs) noexcept {
		const float64 duree = timeMs - mSessionStartMs;
		bool tapee = false;

		if (!mCancelled) {
			// ── Balayage : rapide, court dans le temps, assez loin ──
			const float32 trajet = math::NkSqrt(mTravelX * mTravelX + mTravelY * mTravelY);
			if (!mLongPressDone && trajet >= mConfig.swipeMinPx && duree > 0.0 &&
				duree <= static_cast<float64>(mConfig.swipeMaxMs)) {
				const float32 vitesse = static_cast<float32>(trajet * 1000.0 / duree);
				if (vitesse >= mConfig.swipeMinSpeed) {
					NkSwipeDirection dir = NkSwipeDirection::NK_SWIPE_NONE;
					if (math::NkFabs(mTravelX) >= math::NkFabs(mTravelY)) {
						dir = mTravelX > 0.f ? NkSwipeDirection::NK_SWIPE_RIGHT : NkSwipeDirection::NK_SWIPE_LEFT;
					} else {
						// y vers le BAS : un doigt qui monte a un trajet negatif.
						dir = mTravelY > 0.f ? NkSwipeDirection::NK_SWIPE_DOWN : NkSwipeDirection::NK_SWIPE_UP;
					}
					NkGestureSwipeEvent e(dir, vitesse, mMaxFingers, mWindowId);
					Emit(e);
				}
			}

			// ── Tape ──
			if (!mMoved && !mLongPressDone && duree <= static_cast<float64>(mConfig.tapMaxMs)) {
				uint32 compte = 1;
				if (mHasLastTap && mLastTapFingers == mMaxFingers &&
					mSessionStartMs - mLastTapUpMs <= static_cast<float64>(mConfig.doubleTapMaxMs) &&
					Distance(mLastTapX, mLastTapY, mStartCx, mStartCy) <= mConfig.doubleTapSlopPx) {
					compte = mLastTapCount + 1;
				}
				NkGestureTapEvent e(mStartCx, mStartCy, compte, mMaxFingers, mWindowId);
				Emit(e);
				tapee = true;
				mHasLastTap = true;
				mLastTapUpMs = timeMs;
				mLastTapX = mStartCx;
				mLastTapY = mStartCy;
				mLastTapCount = compte;
				mLastTapFingers = mMaxFingers;
			}
		}

		// Tout geste qui n'est pas une tape rompt l'enchainement : glisser puis
		// taper ne fait pas une double tape.
		if (!tapee) {
			mHasLastTap = false;
		}
		mSession = false;
		mPanActive = false;
		mPinchActive = false;
		mRotateActive = false;
		mPairValid = false;
	}

	// =========================================================================
	// L'entree
	// =========================================================================

	uint32 NkGestureRecognizer::OnTouchEvent(const NkEvent &event, float64 timeMs) noexcept {
		// ⚠️ PAS DE REENTREE : le destinataire d'un geste peut lui-meme POSER des
		//    contacts (un banc, un rejeu). Les lire au milieu de la fin d'une
		//    session corromprait les contacts qu'on est en train de parcourir ;
		//    ils passent donc sans etre lus comme gestes.
		if (mDansAppel) {
			return 0;
		}
		const NkGardeAppel garde(mDansAppel);
		mEmittedThisCall = 0;
		const NkEventType::Value type = event.GetType();
		if (type != NkEventType::NK_TOUCH_BEGIN && type != NkEventType::NK_TOUCH_MOVE &&
			type != NkEventType::NK_TOUCH_END && type != NkEventType::NK_TOUCH_CANCEL) {
			return 0;
		}
		const NkTouchEvent &touch = static_cast<const NkTouchEvent &>(event);
		mWindowId = event.GetWindowId();

		// L'appui long a pu echoir entre deux evenements : il part AVANT ce qui
		// arrive, sinon un doigt leve a 700 ms donnerait une tape trop longue
		// au lieu de l'appui long qu'il etait.
		CheckLongPress(timeMs);

		// ⚠️ L'ANNULATION EFFACE TOUT, quel que soit le contenu de l'evenement :
		//    Android n'y met que le premier doigt, alors que le systeme a repris
		//    TOUS les contacts. Aucun geste ne part d'une sequence annulee.
		if (type == NkEventType::NK_TOUCH_CANCEL) {
			for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
				mContacts[i].active = false;
			}
			mCount = 0;
			mCancelled = true;
			EndSession(timeMs);
			mHasLastTap = false;
			return mEmittedThisCall;
		}

		const uint32 n = touch.GetNumTouches();

		if (type == NkEventType::NK_TOUCH_BEGIN) {
			const bool etaitVide = (mCount == 0);
			bool ajoute = false;
			for (uint32 k = 0; k < n; ++k) {
				const NkTouchPoint &p = touch.GetTouch(k);
				int32 slot = FindContact(p.id);
				if (slot < 0) {
					slot = FreeSlot();
					if (slot < 0) {
						continue; // 32 doigts deja suivis : le 33e est ignore
					}
					++mCount;
					ajoute = true;
				}
				NkContact &c = mContacts[slot];
				c.id = p.id;
				c.active = true;
				c.x0 = p.clientX;
				c.y0 = p.clientY;
				c.x = p.clientX;
				c.y = p.clientY;
			}
			if (etaitVide && mCount > 0) {
				StartSession(timeMs);
			} else if (ajoute) {
				Rereference();
			}
			if (mCount > mMaxFingers) {
				mMaxFingers = mCount;
			}
			return mEmittedThisCall;
		}

		// Move et End : on met d'abord les positions a jour. Les positions d'un
		// End sont un dernier mouvement ; les ignorer perdrait le bout du geste.
		for (uint32 k = 0; k < n; ++k) {
			const NkTouchPoint &p = touch.GetTouch(k);
			const int32 slot = FindContact(p.id);
			if (slot < 0) {
				continue;
			}
			mContacts[slot].x = p.clientX;
			mContacts[slot].y = p.clientY;
		}
		ProcessMotion(timeMs);

		if (type == NkEventType::NK_TOUCH_END) {
			bool retire = false;
			for (uint32 k = 0; k < n; ++k) {
				const int32 slot = FindContact(touch.GetTouch(k).id);
				if (slot < 0) {
					continue;
				}
				mContacts[slot].active = false;
				--mCount;
				retire = true;
			}
			if (mCount == 0 && mSession) {
				EndSession(timeMs);
			} else if (retire) {
				Rereference();
			}
		}
		return mEmittedThisCall;
	}

} // namespace nkentseu
