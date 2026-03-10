#pragma once

// =============================================================================
// NkGamepadEvent.h
// Hiérarchie complète des événements manette / joystick.
//
// Catégorie : NK_CAT_GAMEPAD
//
// Enumerations :
//   NkGamepadType    — famille de manette (Xbox, PlayStation, Nintendo...)
//   NkGamepadButton  — boutons (layout universel Xbox-compatible)
//   NkGamepadAxis    — axes analogiques (sticks, gâchettes, D-pad analogique)
//
// Structures :
//   NkGamepadInfo    — métadonnées de la manette connectée
//
// Classes :
//   NkGamepadEvent               — base abstraite (catégorie GAMEPAD)
//     NkGamepadConnectEvent      — manette branchée
//     NkGamepadDisconnectEvent   — manette débranchée
//     NkGamepadButtonPressEvent  — bouton enfoncé
//     NkGamepadButtonReleaseEvent— bouton relâché
//     NkGamepadAxisEvent         — axe analogique modifié
//     NkGamepadRumbleEvent       — commande de vibration haptique
//     NkGamepadBatteryEvent      — changement de niveau de batterie
// =============================================================================

#include "NkEvent.h"
#include "NkMouseEvent.h"    // pour NkButtonState
#include "NKContainers/String/NkStringUtils.h"
#include <cstring>
#include <string>

namespace nkentseu {

    // =========================================================================
    // NkGamepadType — famille de manette
    // =========================================================================

    enum class NkGamepadType : NkU32 {
        NK_GP_TYPE_UNKNOWN     = 0,
        NK_GP_TYPE_XBOX,           ///< Xbox 360 / One / Series X|S
        NK_GP_TYPE_PLAYSTATION,    ///< DualShock 3/4, DualSense
        NK_GP_TYPE_NINTENDO,       ///< Joy-Con, Pro Controller, Switch SNES
        NK_GP_TYPE_STEAM,          ///< Steam Controller
        NK_GP_TYPE_STADIA,         ///< Stadia Controller
        NK_GP_TYPE_GENERIC,        ///< Périphérique HID générique
        NK_GP_TYPE_MOBILE,         ///< Manette mobile (iOS MFi, Android)
        NK_GP_TYPE_MAX
    };

    inline const char* NkGamepadTypeToString(NkGamepadType t) noexcept {
        switch (t) {
            case NkGamepadType::NK_GP_TYPE_XBOX:         return "Xbox";
            case NkGamepadType::NK_GP_TYPE_PLAYSTATION:  return "PlayStation";
            case NkGamepadType::NK_GP_TYPE_NINTENDO:     return "Nintendo";
            case NkGamepadType::NK_GP_TYPE_STEAM:        return "Steam";
            case NkGamepadType::NK_GP_TYPE_STADIA:       return "Stadia";
            case NkGamepadType::NK_GP_TYPE_GENERIC:      return "Generic";
            case NkGamepadType::NK_GP_TYPE_MOBILE:       return "Mobile";
            default:                                       return "Unknown";
        }
    }

    // =========================================================================
    // NkGamepadButton — boutons (layout Xbox universel)
    // =========================================================================

    enum class NkGamepadButton : NkU32 {
        NK_GP_UNKNOWN = 0,
        // Boutons de face (labels Xbox / PlayStation)
        NK_GP_SOUTH,            ///< A (Xbox) / Croix (PS) / B (Nintendo)
        NK_GP_EAST,             ///< B (Xbox) / Rond (PS) / A (Nintendo)
        NK_GP_WEST,             ///< X (Xbox) / Carré (PS) / Y (Nintendo)
        NK_GP_NORTH,            ///< Y (Xbox) / Triangle (PS) / X (Nintendo)
        // Bumpers / Gâchettes digitaux
        NK_GP_LB,               ///< LB / L1
        NK_GP_RB,               ///< RB / R1
        NK_GP_LT_DIGITAL,       ///< LT digital (seuil)
        NK_GP_RT_DIGITAL,       ///< RT digital (seuil)
        // Clicks de sticks
        NK_GP_LSTICK,           ///< L3 (clic stick gauche)
        NK_GP_RSTICK,           ///< R3 (clic stick droit)
        // D-Pad
        NK_GP_DPAD_UP,
        NK_GP_DPAD_DOWN,
        NK_GP_DPAD_LEFT,
        NK_GP_DPAD_RIGHT,
        // Boutons spéciaux
        NK_GP_START,            ///< Start / Options / +
        NK_GP_BACK,             ///< Back / Select / Share / -
        NK_GP_GUIDE,            ///< Xbox / PS / Home
        NK_GP_TOUCHPAD,         ///< Clic pavé tactile (DS4/DS5)
        NK_GP_CAPTURE,          ///< Capture (Switch)
        NK_GP_MIC,              ///< Microphone (DualSense)
        NK_GP_PADDLE_1,         ///< Paddle 1 (Elite, SCUF...)
        NK_GP_PADDLE_2,
        NK_GP_PADDLE_3,
        NK_GP_PADDLE_4,
        NK_GAMEPAD_BUTTON_MAX
    };

    inline const char* NkGamepadButtonToString(NkGamepadButton b) noexcept {
        switch (b) {
            case NkGamepadButton::NK_GP_SOUTH:       return "South(A/Cross)";
            case NkGamepadButton::NK_GP_EAST:        return "East(B/Circle)";
            case NkGamepadButton::NK_GP_WEST:        return "West(X/Square)";
            case NkGamepadButton::NK_GP_NORTH:       return "North(Y/Triangle)";
            case NkGamepadButton::NK_GP_LB:          return "LB/L1";
            case NkGamepadButton::NK_GP_RB:          return "RB/R1";
            case NkGamepadButton::NK_GP_LT_DIGITAL:  return "LT";
            case NkGamepadButton::NK_GP_RT_DIGITAL:  return "RT";
            case NkGamepadButton::NK_GP_LSTICK:      return "L3";
            case NkGamepadButton::NK_GP_RSTICK:      return "R3";
            case NkGamepadButton::NK_GP_DPAD_UP:     return "DUp";
            case NkGamepadButton::NK_GP_DPAD_DOWN:   return "DDown";
            case NkGamepadButton::NK_GP_DPAD_LEFT:   return "DLeft";
            case NkGamepadButton::NK_GP_DPAD_RIGHT:  return "DRight";
            case NkGamepadButton::NK_GP_START:       return "Start";
            case NkGamepadButton::NK_GP_BACK:        return "Back";
            case NkGamepadButton::NK_GP_GUIDE:       return "Guide";
            case NkGamepadButton::NK_GP_TOUCHPAD:    return "Touchpad";
            case NkGamepadButton::NK_GP_CAPTURE:     return "Capture";
            case NkGamepadButton::NK_GP_MIC:         return "Mic";
            case NkGamepadButton::NK_GP_PADDLE_1:    return "Paddle1";
            case NkGamepadButton::NK_GP_PADDLE_2:    return "Paddle2";
            case NkGamepadButton::NK_GP_PADDLE_3:    return "Paddle3";
            case NkGamepadButton::NK_GP_PADDLE_4:    return "Paddle4";
            default:                                   return "Unknown";
        }
    }

    // =========================================================================
    // NkGamepadAxis — axes analogiques
    // =========================================================================

    enum class NkGamepadAxis : NkU32 {
        NK_GP_AXIS_LX = 0,  ///< Stick gauche horizontal  [-1=gauche, +1=droite]
        NK_GP_AXIS_LY,      ///< Stick gauche vertical    [-1=bas, +1=haut]  (Y+ = haut selon HID)
        NK_GP_AXIS_RX,      ///< Stick droit horizontal
        NK_GP_AXIS_RY,      ///< Stick droit vertical
        NK_GP_AXIS_LT,      ///< Gâchette gauche           [0=relâchée, 1=plein]
        NK_GP_AXIS_RT,      ///< Gâchette droite
        NK_GP_AXIS_DPAD_X,  ///< D-Pad analogique horizontal [-1=gauche, +1=droite]
        NK_GP_AXIS_DPAD_Y,  ///< D-Pad analogique vertical
        NK_GAMEPAD_AXIS_MAX
    };

    inline const char* NkGamepadAxisToString(NkGamepadAxis a) noexcept {
        switch (a) {
            case NkGamepadAxis::NK_GP_AXIS_LX:     return "LeftX";
            case NkGamepadAxis::NK_GP_AXIS_LY:     return "LeftY";
            case NkGamepadAxis::NK_GP_AXIS_RX:     return "RightX";
            case NkGamepadAxis::NK_GP_AXIS_RY:     return "RightY";
            case NkGamepadAxis::NK_GP_AXIS_LT:     return "TriggerLeft";
            case NkGamepadAxis::NK_GP_AXIS_RT:     return "TriggerRight";
            case NkGamepadAxis::NK_GP_AXIS_DPAD_X: return "DPadX";
            case NkGamepadAxis::NK_GP_AXIS_DPAD_Y: return "DPadY";
            default:                                return "Unknown";
        }
    }

    // =========================================================================
    // NkGamepadInfo — métadonnées de la manette connectée
    // =========================================================================

    /**
     * @brief Informations sur une manette connectée.
     *
     * index  : indice du joueur (0 = joueur 1, 1 = joueur 2...).
     * id     : identifiant opaque fourni par le système (GUID SDL, chemin Linux...).
     * name   : nom lisible (ex: "Xbox Wireless Controller").
     */
    struct NkGamepadInfo {
        NkU32         index       = 0;
        char          id[128]     = {};     ///< GUID ou chemin opaque
        char          name[128]   = {};     ///< Nom lisible
        NkGamepadType type        = NkGamepadType::NK_GP_TYPE_UNKNOWN;
        NkU16         vendorId    = 0;
        NkU16         productId   = 0;

        // Capacités
        NkU32 numButtons        = 0;
        NkU32 numAxes           = 0;
        bool  hasRumble         = false;   ///< Moteurs de vibration
        bool  hasTriggerRumble  = false;   ///< Vibration dans les gâchettes
        bool  hasTouchpad       = false;   ///< Pavé tactile intégré
        bool  hasGyro           = false;   ///< Gyroscope / accéléromètre
        bool  hasLED            = false;   ///< LED de couleur programmable
        bool  hasBattery        = false;   ///< Batterie (manette sans fil)

        NkGamepadInfo() {
            std::memset(id,   0, sizeof(id));
            std::memset(name, 0, sizeof(name));
        }
    };

    // =========================================================================
    // NkGamepadEvent — base abstraite pour tous les événements manette
    // =========================================================================

    /**
     * @brief Classe de base pour les événements manette de jeu.
     *
     * Catégorie : NK_CAT_GAMEPAD
     */
    class NkGamepadEvent : public NkEvent {
    public:
        NK_EVENT_CATEGORY_FLAGS(NkEventCategory::NK_CAT_GAMEPAD)

        /// @brief Index du joueur (0 = joueur 1)
        NkU32 GetGamepadIndex() const noexcept { return mGamepadIndex; }

    protected:
        explicit NkGamepadEvent(NkU32 index,
                                 NkU64 windowId = 0) noexcept
            : NkEvent(windowId)
            , mGamepadIndex(index)
        {}

        NkU32 mGamepadIndex = 0;
    };

    // =========================================================================
    // NkGamepadConnectEvent — manette branchée
    // =========================================================================

    /**
     * @brief Émis lorsqu'une manette est détectée et initialisée.
     */
    class NkGamepadConnectEvent final : public NkGamepadEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_CONNECT)

        explicit NkGamepadConnectEvent(const NkGamepadInfo& info,
                                        NkU64 windowId = 0) noexcept
            : NkGamepadEvent(info.index, windowId)
            , mInfo(info)
        {}

        NkEvent*    Clone()    const override { return new NkGamepadConnectEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GamepadConnect(#{0} \"{1}\" {2})",
                mInfo.index, mInfo.name, NkGamepadTypeToString(mInfo.type));
        }

        const NkGamepadInfo& GetInfo() const noexcept { return mInfo; }

    private:
        NkGamepadInfo mInfo;
    };

    // =========================================================================
    // NkGamepadDisconnectEvent — manette débranchée
    // =========================================================================

    /**
     * @brief Émis lorsqu'une manette est déconnectée ou son signal perdu.
     */
    class NkGamepadDisconnectEvent final : public NkGamepadEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_DISCONNECT)

        NkGamepadDisconnectEvent(NkU32 index,
                                  NkU64 windowId = 0) noexcept
            : NkGamepadEvent(index, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkGamepadDisconnectEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GamepadDisconnect(#{0})", mGamepadIndex);
        }
    };

    // =========================================================================
    // NkGamepadButtonEvent — base interne pour press / release
    // =========================================================================

    /**
     * @brief Base interne partagée entre NkGamepadButtonPressEvent et
     *        NkGamepadButtonReleaseEvent.
     */
    class NkGamepadButtonEvent : public NkGamepadEvent {
    public:
        NkGamepadButton GetButton()      const noexcept { return mButton; }
        NkButtonState   GetState()       const noexcept { return mState; }
        /// @brief Valeur analogique [0,1] pour les gâchettes mappées en bouton
        NkF32           GetAnalogValue() const noexcept { return mAnalogValue; }

    protected:
        NkGamepadButtonEvent(NkU32           index,
                              NkGamepadButton button,
                              NkButtonState   state,
                              NkF32           analogValue,
                              NkU64           windowId) noexcept
            : NkGamepadEvent(index, windowId)
            , mButton(button)
            , mState(state)
            , mAnalogValue(analogValue)
        {}

        NkGamepadButton mButton      = NkGamepadButton::NK_GP_UNKNOWN;
        NkButtonState   mState       = NkButtonState::NK_RELEASED;
        NkF32           mAnalogValue = 0.f;
    };

    // =========================================================================
    // NkGamepadButtonPressEvent
    // =========================================================================

    /**
     * @brief Émis lorsqu'un bouton de manette est enfoncé.
     */
    class NkGamepadButtonPressEvent final : public NkGamepadButtonEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_BUTTON_PRESSED)

        /**
         * @param index       Index du joueur.
         * @param button      Bouton enfoncé.
         * @param analogValue Valeur analogique si applicable [0,1] (défaut 1).
         * @param windowId    Identifiant de la fenêtre.
         */
        NkGamepadButtonPressEvent(NkU32 index,
                                   NkGamepadButton button,
                                   NkF32 analogValue = 1.f,
                                   NkU64 windowId    = 0) noexcept
            : NkGamepadButtonEvent(index, button,
                                   NkButtonState::NK_PRESSED,
                                   analogValue, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkGamepadButtonPressEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GamepadButtonPress(#{0} {1})",
                mGamepadIndex, NkGamepadButtonToString(mButton));
        }
    };

    // =========================================================================
    // NkGamepadButtonReleaseEvent
    // =========================================================================

    /**
     * @brief Émis lorsqu'un bouton de manette est relâché.
     */
    class NkGamepadButtonReleaseEvent final : public NkGamepadButtonEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_BUTTON_RELEASED)

        NkGamepadButtonReleaseEvent(NkU32 index,
                                     NkGamepadButton button,
                                     NkU64 windowId = 0) noexcept
            : NkGamepadButtonEvent(index, button,
                                   NkButtonState::NK_RELEASED,
                                   0.f, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkGamepadButtonReleaseEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GamepadButtonRelease(#{0} {1})",
                mGamepadIndex, NkGamepadButtonToString(mButton));
        }
    };

    // =========================================================================
    // NkGamepadAxisEvent — axe analogique modifié
    // =========================================================================

    /**
     * @brief Émis lorsqu'un axe analogique (stick, gâchette) change de valeur.
     *
     * La zone morte (deadzone) est appliquée par le couche de saisie : si
     * |value| < deadzone, value est forcée à 0.0 avant émission.
     * Les sticks sont dans [-1, +1], les gâchettes dans [0, +1].
     */
    class NkGamepadAxisEvent final : public NkGamepadEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_AXIS_MOTION)

        /// @brief Zone morte par défaut [proportion, 0.0-1.0]
        static constexpr NkF32 DEFAULT_DEADZONE = 0.08f;

        /**
         * @param index      Index du joueur.
         * @param axis       Axe concerné.
         * @param value      Valeur courante (après application de la deadzone).
         * @param prevValue  Valeur précédente.
         * @param deadzone   Zone morte utilisée pour le filtrage.
         * @param windowId   Identifiant de la fenêtre.
         */
        NkGamepadAxisEvent(NkU32 index,
                            NkGamepadAxis axis,
                            NkF32 value,
                            NkF32 prevValue,
                            NkF32 deadzone  = DEFAULT_DEADZONE,
                            NkU64 windowId  = 0) noexcept
            : NkGamepadEvent(index, windowId)
            , mAxis(axis)
            , mValue(value)
            , mPrevValue(prevValue)
            , mDelta(value - prevValue)
            , mDeadzone(deadzone)
        {}

        NkEvent*    Clone()    const override { return new NkGamepadAxisEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GamepadAxis(#{0} {1}={2:.3} delta={3:.3})",
                mGamepadIndex, NkGamepadAxisToString(mAxis), mValue, mDelta);
        }

        NkGamepadAxis GetAxis()      const noexcept { return mAxis; }
        NkF32         GetValue()     const noexcept { return mValue; }
        NkF32         GetPrevValue() const noexcept { return mPrevValue; }
        NkF32         GetDelta()     const noexcept { return mDelta; }
        NkF32         GetDeadzone()  const noexcept { return mDeadzone; }
        bool          IsInDeadzone() const noexcept {
            return mValue > -mDeadzone && mValue < mDeadzone;
        }

    private:
        NkGamepadAxis mAxis      = NkGamepadAxis::NK_GP_AXIS_LX;
        NkF32         mValue     = 0.f;
        NkF32         mPrevValue = 0.f;
        NkF32         mDelta     = 0.f;
        NkF32         mDeadzone  = DEFAULT_DEADZONE;
    };

    // =========================================================================
    // NkGamepadRumbleEvent — commande de vibration haptique
    // =========================================================================

    /**
     * @brief Commande envoyée par l'application pour déclencher une vibration.
     *
     * Cet événement est "sortant" : il est émis par la couche applicative et
     * consommé par le backend d'entrée pour piloter les moteurs haptiques.
     *
     * motorLow    : moteur basse fréquence [0,1] (poignée gauche, effets larges)
     * motorHigh   : moteur haute fréquence [0,1] (poignée droite, détails fins)
     * triggerLeft : vibration gâchette gauche [0,1] (DualSense / Xbox Elite)
     * triggerRight: vibration gâchette droite [0,1]
     * durationMs  : durée [ms], 0 = jusqu'au prochain appel Stop
     */
    class NkGamepadRumbleEvent final : public NkGamepadEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_RUMBLE)

        /**
         * @param index        Index du joueur.
         * @param motorLow     Intensité moteur basse fréquence [0,1].
         * @param motorHigh    Intensité moteur haute fréquence [0,1].
         * @param triggerLeft  Intensité gâchette gauche [0,1].
         * @param triggerRight Intensité gâchette droite [0,1].
         * @param durationMs   Durée [ms] (0 = infini jusqu'à stop).
         * @param windowId     Identifiant de la fenêtre.
         */
        NkGamepadRumbleEvent(NkU32 index,
                              NkF32 motorLow     = 0.f,
                              NkF32 motorHigh    = 0.f,
                              NkF32 triggerLeft  = 0.f,
                              NkF32 triggerRight = 0.f,
                              NkU32 durationMs   = 100,
                              NkU64 windowId     = 0) noexcept
            : NkGamepadEvent(index, windowId)
            , mMotorLow(motorLow)
            , mMotorHigh(motorHigh)
            , mTriggerLeft(triggerLeft)
            , mTriggerRight(triggerRight)
            , mDurationMs(durationMs)
        {}

        NkEvent*    Clone()    const override { return new NkGamepadRumbleEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("GamepadRumble(#{0} lo={1:.3} hi={2:.3} {3}ms)",
                mGamepadIndex, mMotorLow, mMotorHigh, mDurationMs);
        }

        NkF32 GetMotorLow()     const noexcept { return mMotorLow; }
        NkF32 GetMotorHigh()    const noexcept { return mMotorHigh; }
        NkF32 GetTriggerLeft()  const noexcept { return mTriggerLeft; }
        NkF32 GetTriggerRight() const noexcept { return mTriggerRight; }
        NkU32 GetDurationMs()   const noexcept { return mDurationMs; }
        bool  IsStop()          const noexcept {
            return mMotorLow == 0.f && mMotorHigh == 0.f
                && mTriggerLeft == 0.f && mTriggerRight == 0.f;
        }

    private:
        NkF32 mMotorLow     = 0.f;
        NkF32 mMotorHigh    = 0.f;
        NkF32 mTriggerLeft  = 0.f;
        NkF32 mTriggerRight = 0.f;
        NkU32 mDurationMs   = 100;
    };

    // =========================================================================
    // NkGamepadBatteryEvent — changement du niveau de batterie
    // =========================================================================

    /**
     * @brief Émis lorsque le niveau de batterie d'une manette sans fil change.
     *
     * batteryLevel : proportion [0,1] ou -1 si manette filaire / inconnu.
     * isCharging   : true si la manette est en charge.
     */
    class NkGamepadBatteryEvent final : public NkGamepadEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_GAMEPAD_CONNECT)  // proxy (pas de type dédié NK_GAMEPAD_BATTERY)

        /**
         * @param index        Index du joueur.
         * @param level        Niveau de batterie [0,1] (-1 = filaire / inconnu).
         * @param isCharging   true si la manette est en charge.
         * @param windowId     Identifiant de la fenêtre.
         */
        NkGamepadBatteryEvent(NkU32 index,
                               NkF32 level      = -1.f,
                               bool  isCharging  = false,
                               NkU64 windowId    = 0) noexcept
            : NkGamepadEvent(index, windowId)
            , mLevel(level)
            , mIsCharging(isCharging)
        {}

        NkEvent*    Clone()    const override { return new NkGamepadBatteryEvent(*this); }
        NkString ToString() const override {
            NkString level = (mLevel < 0.f)
                ? NkString("wired")
                : NkString::Fmt("{0}%", static_cast<int>(mLevel * 100));
            return NkString::Fmt("GamepadBattery(#{0} {1}{2})",
                mGamepadIndex, level, mIsCharging ? " charging" : "");
        }

        NkF32 GetLevel()     const noexcept { return mLevel; }
        bool  IsCharging()   const noexcept { return mIsCharging; }
        bool  IsWired()      const noexcept { return mLevel < 0.f; }
        bool  IsCritical()   const noexcept { return mLevel >= 0.f && mLevel < 0.1f; }

    private:
        NkF32 mLevel      = -1.f;
        bool  mIsCharging = false;
    };

} // namespace nkentseu