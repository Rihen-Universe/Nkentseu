#pragma once

// =============================================================================
// NkTouchEvent.h
// Hiérarchie complète des événements tactiles et gestes.
//
// Contenu :
//   Énumérations :
//     NkTouchPhase      — phase d'un contact individuel
//     NkSwipeDirection  — direction d'un swipe
//
//   Structs :
//     NkTouchPoint      — état d'un contact individuel
//
//   Classes :
//     NkTouchEvent           — base abstraite (catégorie TOUCH)
//       NkTouchBeginEvent    — nouveau(x) contact(s)
//       NkTouchMoveEvent     — contact(s) déplacé(s)
//       NkTouchEndEvent      — contact(s) levé(s)
//       NkTouchCancelEvent   — contact(s) annulé(s)
//       NkGesturePinchEvent  — pincement / zoom (2 doigts)
//       NkGestureRotateEvent — rotation (2 doigts)
//       NkGesturePanEvent    — panoramique (N doigts)
//       NkGestureSwipeEvent  — balayage rapide
//       NkGestureTapEvent    — tape simple / double
//       NkGestureLongPressEvent — appui long
// =============================================================================

#include "NkEvent.h"
#include "NKContainers/String/NkStringUtils.h"
#include <algorithm>

namespace nkentseu {

    // =========================================================================
    // NkTouchPhase — phase d'un contact tactile
    // =========================================================================

    enum class NkTouchPhase : NkU32 {
        NK_TOUCH_PHASE_BEGAN       = 0,
        NK_TOUCH_PHASE_MOVED,
        NK_TOUCH_PHASE_STATIONARY,
        NK_TOUCH_PHASE_ENDED,
        NK_TOUCH_PHASE_CANCELLED,
        NK_TOUCH_PHASE_MAX
    };

    // =========================================================================
    // NkSwipeDirection — direction du swipe
    // =========================================================================

    enum class NkSwipeDirection : NkU32 {
        NK_SWIPE_NONE  = 0,
        NK_SWIPE_LEFT,
        NK_SWIPE_RIGHT,
        NK_SWIPE_UP,
        NK_SWIPE_DOWN
    };

    inline const char* NkSwipeDirectionToString(NkSwipeDirection d) noexcept {
        switch (d) {
            case NkSwipeDirection::NK_SWIPE_LEFT:  return "LEFT";
            case NkSwipeDirection::NK_SWIPE_RIGHT: return "RIGHT";
            case NkSwipeDirection::NK_SWIPE_UP:    return "UP";
            case NkSwipeDirection::NK_SWIPE_DOWN:  return "DOWN";
            default:                                return "NONE";
        }
    }

    // =========================================================================
    // NkTouchPoint — un contact individuel
    // =========================================================================

    static constexpr NkU32 NK_MAX_TOUCH_POINTS = 32;

    struct NkTouchPoint {
        NkU64 id = 0; ///< Identifiant unique du contact
        NkTouchPhase phase = NkTouchPhase::NK_TOUCH_PHASE_BEGAN;

        // Coordonnées dans la zone client (pixels physiques)
        float clientX = 0.f, clientY = 0.f;
        // Coordonnées écran
        float screenX = 0.f, screenY = 0.f;
        // Coordonnées normalisées [0,1] dans la zone client
        float normalX = 0.f, normalY = 0.f;
        // Déplacement depuis l'événement précédent
        float deltaX = 0.f, deltaY = 0.f;
        // Pression [0,1]
        float pressure = 1.f;
        // Rayon de contact [pixels]
        float radiusX = 0.f, radiusY = 0.f;
        // Angle du contact [degrés]
        float angle = 0.f;

        bool HasMoved() const { return deltaX != 0.f || deltaY != 0.f; }
        bool IsActive() const {
            return phase == NkTouchPhase::NK_TOUCH_PHASE_BEGAN ||
                   phase == NkTouchPhase::NK_TOUCH_PHASE_MOVED ||
                   phase == NkTouchPhase::NK_TOUCH_PHASE_STATIONARY;
        }
    };

    // =========================================================================
    // NkTouchEvent — base abstraite pour les événements tactiles
    // =========================================================================

    /**
     * @brief Base abstraite pour tous les événements tactiles.
     * Catégorie : NK_CAT_TOUCH
     */
    class NkTouchEvent : public NkEvent {
    public:
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_TOUCH)

        NkU32 GetNumTouches() const noexcept { return mNumTouches; }
        const NkTouchPoint& GetTouch(NkU32 i) const noexcept { return mTouches[i]; }
        float GetCentroidX() const noexcept { return mCentroidX; }
        float GetCentroidY() const noexcept { return mCentroidY; }

    protected:
        explicit NkTouchEvent(const NkTouchPoint* points, NkU32 count, NkU64 windowId = 0) noexcept
            : NkEvent(windowId), mNumTouches(0), mCentroidX(0.f), mCentroidY(0.f)
        {
            mNumTouches = (count > NK_MAX_TOUCH_POINTS) ? NK_MAX_TOUCH_POINTS : count;
            float sumX = 0.f, sumY = 0.f;
            for (NkU32 i = 0; i < mNumTouches; ++i) {
                mTouches[i] = points[i];
                sumX += points[i].clientX;
                sumY += points[i].clientY;
            }
            if (mNumTouches > 0) {
                mCentroidX = sumX / static_cast<float>(mNumTouches);
                mCentroidY = sumY / static_cast<float>(mNumTouches);
            }
        }

        NkTouchPoint mTouches[NK_MAX_TOUCH_POINTS] = {};
        NkU32        mNumTouches = 0;
        float        mCentroidX  = 0.f;
        float        mCentroidY  = 0.f;
    };

    // =========================================================================
    // NkTouchBeginEvent
    // =========================================================================

    /**
     * @brief Nouveau contact ou multiple contacts
     */
    class NkTouchBeginEvent final : public NkTouchEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TOUCH_BEGIN)

        explicit NkTouchBeginEvent(const NkTouchPoint* points, NkU32 count, NkU64 windowId = 0) noexcept
            : NkTouchEvent(points, count, windowId) {}

        NkEvent* Clone() const override { return new NkTouchBeginEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("TouchBegin({0} contacts)", mNumTouches);
        }
    };

    // =========================================================================
    // NkTouchMoveEvent
    // =========================================================================

    /**
     * @brief Contact(s) déplacé(s)
     */
    class NkTouchMoveEvent final : public NkTouchEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TOUCH_MOVE)

        explicit NkTouchMoveEvent(const NkTouchPoint* points, NkU32 count, NkU64 windowId = 0) noexcept
            : NkTouchEvent(points, count, windowId) {}

        NkEvent* Clone() const override { return new NkTouchMoveEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("TouchMove({0} contacts)", mNumTouches);
        }
    };

    // =========================================================================
    // NkTouchEndEvent
    // =========================================================================

    /**
     * @brief Contact(s) levé(s)
     */
    class NkTouchEndEvent final : public NkTouchEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TOUCH_END)

        explicit NkTouchEndEvent(const NkTouchPoint* points, NkU32 count, NkU64 windowId = 0) noexcept
            : NkTouchEvent(points, count, windowId) {}

        NkEvent* Clone() const override { return new NkTouchEndEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("TouchEnd({0} contacts)", mNumTouches);
        }
    };

    // =========================================================================
    // NkTouchCancelEvent
    // =========================================================================

    /**
     * @brief Contact(s) annulé(s)
     */
    class NkTouchCancelEvent final : public NkTouchEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_TOUCH_CANCEL)

        explicit NkTouchCancelEvent(const NkTouchPoint* points, NkU32 count, NkU64 windowId = 0) noexcept
            : NkTouchEvent(points, count, windowId) {}

        NkEvent* Clone() const override { return new NkTouchCancelEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("TouchCancel({0} contacts)", mNumTouches);
        }
    };

    // =========================================================================
    // NkGesturePinchEvent — pincement / zoom (2 doigts)
    // =========================================================================

    /**
     * @brief Geste pincement (zoom)
     */
    class NkGesturePinchEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GESTURE_PINCH)

        NkGesturePinchEvent(float scale, float scaleDelta, float centerX = 0.f, float centerY = 0.f,
                             NkU64 windowId = 0) noexcept
            : NkEvent(windowId), mScale(scale), mScaleDelta(scaleDelta), mCenterX(centerX), mCenterY(centerY) {}

        NkEvent* Clone() const override { return new NkGesturePinchEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GesturePinch(scale={0:.3})", mScale);
        }

        float GetScale() const noexcept { return mScale; }
        float GetScaleDelta() const noexcept { return mScaleDelta; }
        float GetCenterX() const noexcept { return mCenterX; }
        float GetCenterY() const noexcept { return mCenterY; }
        bool IsZoomIn() const noexcept { return mScaleDelta > 0.f; }
        bool IsZoomOut() const noexcept { return mScaleDelta < 0.f; }

    private:
        float mScale = 1.f, mScaleDelta = 0.f;
        float mCenterX = 0.f, mCenterY = 0.f;
    };

    // =========================================================================
    // NkGestureRotateEvent
    // =========================================================================

    /**
     * @brief Geste rotation (2 doigts)
     */
    class NkGestureRotateEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GESTURE_ROTATE)

        NkGestureRotateEvent(float angleDegrees, float angleDeltaDegrees, NkU64 windowId = 0) noexcept
            : NkEvent(windowId), mAngle(angleDegrees), mAngleDelta(angleDeltaDegrees) {}

        NkEvent* Clone() const override { return new NkGestureRotateEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GestureRotate({0:.3}°)", mAngle);
        }

        float GetAngle() const noexcept { return mAngle; }
        float GetAngleDelta() const noexcept { return mAngleDelta; }
        bool IsClockwise() const noexcept { return mAngleDelta > 0.f; }

    private:
        float mAngle = 0.f, mAngleDelta = 0.f;
    };

    // =========================================================================
    // NkGesturePanEvent
    // =========================================================================

    /**
     * @brief Geste panoramique (scroll N-doigts)
     */
    class NkGesturePanEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GESTURE_PAN)

        NkGesturePanEvent(float deltaX, float deltaY, float velocityX = 0.f, float velocityY = 0.f,
                           NkU32 numFingers = 1, NkU64 windowId = 0) noexcept
            : NkEvent(windowId), mDeltaX(deltaX), mDeltaY(deltaY), mVelocityX(velocityX),
              mVelocityY(velocityY), mNumFingers(numFingers) {}

        NkEvent* Clone() const override { return new NkGesturePanEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GesturePan(d={0:.3},{1:.3})", mDeltaX, mDeltaY);
        }

        float GetDeltaX() const noexcept { return mDeltaX; }
        float GetDeltaY() const noexcept { return mDeltaY; }
        float GetVelocityX() const noexcept { return mVelocityX; }
        float GetVelocityY() const noexcept { return mVelocityY; }
        NkU32 GetNumFingers() const noexcept { return mNumFingers; }

    private:
        float mDeltaX = 0.f, mDeltaY = 0.f;
        float mVelocityX = 0.f, mVelocityY = 0.f;
        NkU32 mNumFingers = 1;
    };

    // =========================================================================
    // NkGestureSwipeEvent
    // =========================================================================

    /**
     * @brief Geste balayage rapide
     */
    class NkGestureSwipeEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GESTURE_SWIPE)

        NkGestureSwipeEvent(NkSwipeDirection direction, float speed = 0.f, NkU32 numFingers = 1,
                             NkU64 windowId = 0) noexcept
            : NkEvent(windowId), mDirection(direction), mSpeed(speed), mNumFingers(numFingers) {}

        NkEvent* Clone() const override { return new NkGestureSwipeEvent(*this); }
        NkString ToString() const override {
            return NkString("GestureSwipe(") + NkSwipeDirectionToString(mDirection) + ")";
        }

        NkSwipeDirection GetDirection() const noexcept { return mDirection; }
        float GetSpeed() const noexcept { return mSpeed; }
        NkU32 GetNumFingers() const noexcept { return mNumFingers; }
        bool IsLeft() const noexcept { return mDirection == NkSwipeDirection::NK_SWIPE_LEFT; }
        bool IsRight() const noexcept { return mDirection == NkSwipeDirection::NK_SWIPE_RIGHT; }
        bool IsUp() const noexcept { return mDirection == NkSwipeDirection::NK_SWIPE_UP; }
        bool IsDown() const noexcept { return mDirection == NkSwipeDirection::NK_SWIPE_DOWN; }

    private:
        NkSwipeDirection mDirection = NkSwipeDirection::NK_SWIPE_NONE;
        float mSpeed = 0.f;
        NkU32 mNumFingers = 1;
    };

    // =========================================================================
    // NkGestureTapEvent
    // =========================================================================

    /**
     * @brief Geste tape simple ou multiple
     */
    class NkGestureTapEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GESTURE_TAP)

        NkGestureTapEvent(float x = 0.f, float y = 0.f, NkU32 tapCount = 1, NkU32 numFingers = 1,
                           NkU64 windowId = 0) noexcept
            : NkEvent(windowId), mX(x), mY(y), mTapCount(tapCount), mNumFingers(numFingers) {}

        NkEvent* Clone() const override { return new NkGestureTapEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GestureTap(x{0})", mTapCount);
        }

        float GetX() const noexcept { return mX; }
        float GetY() const noexcept { return mY; }
        NkU32 GetTapCount() const noexcept { return mTapCount; }
        NkU32 GetNumFingers() const noexcept { return mNumFingers; }
        bool IsDoubleTap() const noexcept { return mTapCount >= 2; }

    private:
        float mX = 0.f, mY = 0.f;
        NkU32 mTapCount = 1;
        NkU32 mNumFingers = 1;
    };

    // =========================================================================
    // NkGestureLongPressEvent
    // =========================================================================

    /**
     * @brief Geste appui long
     */
    class NkGestureLongPressEvent final : public NkEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GESTURE_LONG_PRESS)

        NkGestureLongPressEvent(float x = 0.f, float y = 0.f, float durationMs = 0.f, NkU32 numFingers = 1,
                                 NkU64 windowId = 0) noexcept
            : NkEvent(windowId), mX(x), mY(y), mDurationMs(durationMs), mNumFingers(numFingers) {}

        NkEvent* Clone() const override { return new NkGestureLongPressEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GestureLongPress({0:.1}ms)", mDurationMs);
        }

        float GetX() const noexcept { return mX; }
        float GetY() const noexcept { return mY; }
        float GetDurationMs() const noexcept { return mDurationMs; }
        NkU32 GetNumFingers() const noexcept { return mNumFingers; }

    private:
        float mX = 0.f, mY = 0.f;
        float mDurationMs = 0.f;
        NkU32 mNumFingers = 1;
    };

} // namespace nkentseu
