#pragma once

// =============================================================================
// NkMouseEvent.h
// Hiérarchie complète des événements souris.
//
// Contenu :
//   NkMouseButton              — énumération des boutons souris
//   NkButtonState              — état d'un bouton (pressé / relâché)
//   NkMouseButtons             — masque des boutons enfoncés simultanément
//
//   NkMouseEvent               — base abstraite (catégorie MOUSE | INPUT)
//   NkMouseMoveEvent           — déplacement curseur (coordonnées client)
//   NkMouseRawEvent            — mouvement brut (sans accélération OS)
//   NkMouseButtonEvent         — base interne press/release/dblclick
//     NkMouseButtonPressEvent
//     NkMouseButtonReleaseEvent
//     NkMouseDoubleClickEvent
//   NkMouseWheelEvent          — base interne molettes
//     NkMouseWheelVerticalEvent
//     NkMouseWheelHorizontalEvent
//   NkMouseEnterEvent          — curseur entre dans la zone client
//   NkMouseLeaveEvent          — curseur quitte la zone client
//   NkMouseWindowEnterEvent    — curseur entre dans la fenêtre
//   NkMouseWindowLeaveEvent    — curseur quitte la fenêtre
//   NkMouseCaptureBeginEvent   — début de capture souris exclusive
//   NkMouseCaptureEndEvent     — fin de capture souris exclusive
// =============================================================================

#include "NkEvent.h"
#include "NkKeyboardEvent.h"  // NkModifierState
#include "NKContainers/String/NkStringUtils.h"

namespace nkentseu {

    // =========================================================================
    // NkMouseButton — boutons de la souris
    // =========================================================================

    enum class NkMouseButton : uint32 {
        NK_MB_UNKNOWN = 0,
        NK_MB_LEFT,         ///< Bouton gauche (principal)
        NK_MB_RIGHT,        ///< Bouton droit (contextuel)
        NK_MB_MIDDLE,       ///< Bouton central (molette cliquable)
        NK_MB_BACK,         ///< Bouton latéral arrière (navigation)
        NK_MB_FORWARD,      ///< Bouton latéral avant (navigation)
        NK_MB_6,            ///< Bouton supplémentaire 6
        NK_MB_7,            ///< Bouton supplémentaire 7
        NK_MB_8,            ///< Bouton supplémentaire 8
        NK_MOUSE_BUTTON_MAX ///< Sentinelle
    };

    /// @brief Retourne le nom lisible d'un bouton souris
    inline const char* NkMouseButtonToString(NkMouseButton b) noexcept {
        switch (b) {
            case NkMouseButton::NK_MB_LEFT:    return "LEFT";
            case NkMouseButton::NK_MB_RIGHT:   return "RIGHT";
            case NkMouseButton::NK_MB_MIDDLE:  return "MIDDLE";
            case NkMouseButton::NK_MB_BACK:    return "BACK";
            case NkMouseButton::NK_MB_FORWARD: return "FORWARD";
            case NkMouseButton::NK_MB_6:       return "MB6";
            case NkMouseButton::NK_MB_7:       return "MB7";
            case NkMouseButton::NK_MB_8:       return "MB8";
            default:                            return "UNKNOWN";
        }
    }

    // =========================================================================
    // NkButtonState — état d'un bouton
    // =========================================================================

    enum class NkButtonState : uint32 {
        NK_RELEASED = 0, ///< Bouton relâché
        NK_PRESSED   = 1  ///< Bouton pressé
    };

    // =========================================================================
    // NkMouseButtons — masque de bits des boutons enfoncés simultanément
    // =========================================================================

    struct NkMouseButtons {
        uint32 mask = 0; ///< Bit i = bouton i enfoncé

        /// @brief Marque le bouton b comme enfoncé
        void Set(NkMouseButton b)    noexcept { mask |=  (1u << static_cast<uint32>(b)); }
        /// @brief Marque le bouton b comme relâché
        void Clear(NkMouseButton b)  noexcept { mask &= ~(1u << static_cast<uint32>(b)); }
        /// @brief Retourne true si le bouton b est enfoncé
        bool IsDown(NkMouseButton b) const noexcept { return (mask & (1u << static_cast<uint32>(b))) != 0; }
        /// @brief Retourne true si au moins un bouton est enfoncé
        bool Any()  const noexcept { return mask != 0; }
        /// @brief Retourne true si aucun bouton n'est enfoncé
        bool IsNone() const noexcept { return mask == 0; }
    };

    // =========================================================================
    // NkMouseEvent — base abstraite pour tous les événements souris
    //
    // Catégorie : NK_CAT_MOUSE | NK_CAT_INPUT
    // =========================================================================

    class NkMouseEvent : public NkEvent {
    public:
        /// @brief Retourne les flags de catégorie (MOUSE + INPUT)
        uint32 GetCategoryFlags() const override {
            return static_cast<uint32>(NkEventCategory::NK_CAT_MOUSE)
                 | static_cast<uint32>(NkEventCategory::NK_CAT_INPUT);
        }

    protected:
        explicit NkMouseEvent(uint64 windowId = 0) noexcept
            : NkEvent(windowId) {}
    };

    // =========================================================================
    // NkMouseMoveEvent — déplacement du curseur
    // =========================================================================

    class NkMouseMoveEvent final : public NkMouseEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_MOVE)

        /// @param x / y           Position dans la zone client de la fenêtre (pixels)
        /// @param screenX / screenY Position sur l'écran (pixels)
        /// @param deltaX / deltaY  Déplacement depuis le dernier événement
        /// @param buttons          Boutons enfoncés durant le déplacement
        /// @param mods             Modificateurs clavier actifs
        NkMouseMoveEvent(int32 x, int32 y,
                         int32 screenX, int32 screenY,
                         int32 deltaX, int32 deltaY,
                         const NkMouseButtons&  buttons  = {},
                         const NkModifierState& mods     = {},
                         uint64 windowId = 0) noexcept
            : NkMouseEvent(windowId)
            , mX(x), mY(y)
            , mScreenX(screenX), mScreenY(screenY)
            , mDeltaX(deltaX), mDeltaY(deltaY)
            , mButtons(buttons), mModifiers(mods)
        {}

        NkEvent*    Clone()    const override { return new NkMouseMoveEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("MouseMove({0},{1} d={2},{3})",
                mX, mY, mDeltaX, mDeltaY);
        }

        int32           GetX()          const noexcept { return mX;          }
        int32           GetY()          const noexcept { return mY;          }
        int32           GetScreenX()    const noexcept { return mScreenX;    }
        int32           GetScreenY()    const noexcept { return mScreenY;    }
        int32           GetDeltaX()     const noexcept { return mDeltaX;     }
        int32           GetDeltaY()     const noexcept { return mDeltaY;     }
        NkMouseButtons  GetButtons()    const noexcept { return mButtons;    }
        NkModifierState GetModifiers()  const noexcept { return mModifiers;  }

        /// @brief Retourne true si le bouton donné est enfoncé durant le déplacement
        bool IsButtonDown(NkMouseButton b) const noexcept { return mButtons.IsDown(b); }

    private:
        int32           mX = 0, mY = 0;                 ///< Position client
        int32           mScreenX = 0, mScreenY = 0;     ///< Position écran
        int32           mDeltaX = 0, mDeltaY = 0;       ///< Delta depuis dernier evt
        NkMouseButtons  mButtons;                        ///< Boutons enfoncés
        NkModifierState mModifiers;                      ///< Modificateurs clavier
    };

    // =========================================================================
    // NkMouseRawEvent — mouvement brut (sans accélération / scaling OS)
    //
    // Utile pour les FPS / jeux 3D où l'on veut le mouvement physique pur.
    // deltaZ est disponible sur certaines souris 3D ou pour la molette brute.
    // =========================================================================

    class NkMouseRawEvent final : public NkMouseEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_RAW)

        NkMouseRawEvent(int32 deltaX, int32 deltaY, int32 deltaZ = 0,
                        uint64 windowId = 0) noexcept
            : NkMouseEvent(windowId)
            , mDeltaX(deltaX), mDeltaY(deltaY), mDeltaZ(deltaZ)
        {}

        NkEvent*    Clone()    const override { return new NkMouseRawEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("MouseRaw({0},{1})", mDeltaX, mDeltaY);
        }

        int32 GetDeltaX() const noexcept { return mDeltaX; }
        int32 GetDeltaY() const noexcept { return mDeltaY; }
        /// @brief Delta Z brut (molette ou axe 3D, si disponible)
        int32 GetDeltaZ() const noexcept { return mDeltaZ; }

    private:
        int32 mDeltaX = 0, mDeltaY = 0, mDeltaZ = 0;
    };

    // =========================================================================
    // NkMouseButtonEvent — base interne pour press / release / double-clic
    // Non instanciable directement — utiliser les classes dérivées.
    // =========================================================================

    class NkMouseButtonEvent : public NkMouseEvent {
    public:
        NkMouseButton   GetButton()      const noexcept { return mButton;     }
        NkButtonState   GetState()       const noexcept { return mState;      }
        int32           GetX()           const noexcept { return mX;          }
        int32           GetY()           const noexcept { return mY;          }
        int32           GetScreenX()     const noexcept { return mScreenX;    }
        int32           GetScreenY()     const noexcept { return mScreenY;    }
        uint32           GetClickCount()  const noexcept { return mClickCount; }
        NkModifierState GetModifiers()   const noexcept { return mModifiers;  }

        /// @brief Raccourcis pour les boutons communs
        bool IsLeft()   const noexcept { return mButton == NkMouseButton::NK_MB_LEFT;   }
        bool IsRight()  const noexcept { return mButton == NkMouseButton::NK_MB_RIGHT;  }
        bool IsMiddle() const noexcept { return mButton == NkMouseButton::NK_MB_MIDDLE; }

    protected:
        NkMouseButtonEvent(NkMouseButton          button,
                           NkButtonState          state,
                           int32                  x,       int32 y,
                           int32                  screenX, int32 screenY,
                           uint32                  clickCount,
                           const NkModifierState& mods,
                           uint64                  windowId) noexcept
            : NkMouseEvent(windowId)
            , mButton(button), mState(state)
            , mX(x), mY(y), mScreenX(screenX), mScreenY(screenY)
            , mClickCount(clickCount), mModifiers(mods)
        {}

        NkMouseButton   mButton     = NkMouseButton::NK_MB_UNKNOWN; ///< Bouton concerné
        NkButtonState   mState      = NkButtonState::NK_RELEASED;   ///< État du bouton
        int32           mX = 0, mY = 0;                              ///< Position client
        int32           mScreenX = 0, mScreenY = 0;                  ///< Position écran
        uint32           mClickCount = 1;                             ///< 1 = simple, 2 = double…
        NkModifierState mModifiers;                                  ///< Modificateurs actifs
    };

    // =========================================================================
    // NkMouseButtonPressEvent — bouton pressé
    // =========================================================================

    class NkMouseButtonPressEvent final : public NkMouseButtonEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_BUTTON_PRESSED)

        NkMouseButtonPressEvent(NkMouseButton          button,
                                int32                  x,        int32 y,
                                int32                  screenX   = 0,
                                int32                  screenY   = 0,
                                uint32                  clickCount = 1,
                                const NkModifierState& mods      = {},
                                uint64                  windowId  = 0) noexcept
            : NkMouseButtonEvent(button, NkButtonState::NK_PRESSED,
                                 x, y, screenX, screenY, clickCount, mods, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkMouseButtonPressEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("MouseButtonPress({0} {1},{2})",
                NkMouseButtonToString(mButton), mX, mY);
        }
    };

    // =========================================================================
    // NkMouseButtonReleaseEvent — bouton relâché
    // =========================================================================

    class NkMouseButtonReleaseEvent final : public NkMouseButtonEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_BUTTON_RELEASED)

        NkMouseButtonReleaseEvent(NkMouseButton          button,
                                  int32                  x,        int32 y,
                                  int32                  screenX   = 0,
                                  int32                  screenY   = 0,
                                  uint32                  clickCount = 1,
                                  const NkModifierState& mods      = {},
                                  uint64                  windowId  = 0) noexcept
            : NkMouseButtonEvent(button, NkButtonState::NK_RELEASED,
                                 x, y, screenX, screenY, clickCount, mods, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkMouseButtonReleaseEvent(*this); }
        NkString ToString() const override {
            return NkString("MouseButtonRelease(") + NkMouseButtonToString(mButton) + ")";
        }
    };

    // =========================================================================
    // NkMouseDoubleClickEvent — double-clic détecté par l'OS
    // =========================================================================

    class NkMouseDoubleClickEvent final : public NkMouseButtonEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_DOUBLE_CLICK)

        NkMouseDoubleClickEvent(NkMouseButton          button,
                                int32                  x,       int32 y,
                                int32                  screenX  = 0,
                                int32                  screenY  = 0,
                                const NkModifierState& mods     = {},
                                uint64                  windowId = 0) noexcept
            : NkMouseButtonEvent(button, NkButtonState::NK_PRESSED,
                                 x, y, screenX, screenY, 2, mods, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkMouseDoubleClickEvent(*this); }
        NkString ToString() const override {
            return NkString("MouseDoubleClick(") + NkMouseButtonToString(mButton) + ")";
        }
    };

    // =========================================================================
    // NkMouseWheelEvent — base interne pour les événements de molette
    // Non instanciable directement.
    // =========================================================================

    class NkMouseWheelEvent : public NkMouseEvent {
    public:
        float64           GetDeltaX()       const noexcept { return mDeltaX;       }
        float64           GetDeltaY()       const noexcept { return mDeltaY;       }
        float64           GetPixelDeltaX()  const noexcept { return mPixelDeltaX;  }
        float64           GetPixelDeltaY()  const noexcept { return mPixelDeltaY;  }
        int32           GetX()            const noexcept { return mX;            }
        int32           GetY()            const noexcept { return mY;            }
        /// @brief Retourne true si la molette supporte la haute précision (trackpad…)
        bool            IsHighPrecision() const noexcept { return mHighPrecision; }
        NkModifierState GetModifiers()    const noexcept { return mModifiers;     }

    protected:
        NkMouseWheelEvent(float64 dX, float64 dY, float64 pdX, float64 pdY,
                          int32 x, int32 y, bool highPrecision,
                          const NkModifierState& mods,
                          uint64 windowId) noexcept
            : NkMouseEvent(windowId)
            , mDeltaX(dX), mDeltaY(dY)
            , mPixelDeltaX(pdX), mPixelDeltaY(pdY)
            , mX(x), mY(y)
            , mHighPrecision(highPrecision), mModifiers(mods)
        {}

        float64           mDeltaX      = 0.0; ///< Delta logique X (en "clics de molette")
        float64           mDeltaY      = 0.0; ///< Delta logique Y
        float64           mPixelDeltaX = 0.0; ///< Delta en pixels (haute précision)
        float64           mPixelDeltaY = 0.0;
        int32           mX = 0, mY = 0;     ///< Position curseur lors de l'événement
        bool            mHighPrecision = false; ///< Molette trackpad / haute précision
        NkModifierState mModifiers;
    };

    // =========================================================================
    // NkMouseWheelVerticalEvent — molette verticale (scroll haut/bas)
    // =========================================================================

    class NkMouseWheelVerticalEvent final : public NkMouseWheelEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_WHEEL_VERTICAL)

        /// @param deltaY      Delta en "clics" (positif = vers le haut)
        /// @param x / y       Position du curseur
        /// @param pixelDeltaY Delta en pixels pour les trackpads haute précision
        NkMouseWheelVerticalEvent(float64 deltaY,
                                  int32 x = 0, int32 y = 0,
                                  float64 pixelDeltaY     = 0.0,
                                  bool  highPrecision   = false,
                                  const NkModifierState& mods = {},
                                  uint64 windowId        = 0) noexcept
            : NkMouseWheelEvent(0.0, deltaY, 0.0, pixelDeltaY, x, y, highPrecision, mods, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkMouseWheelVerticalEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("MouseWheelV({0})", mDeltaY);
        }

        /// @brief Retourne true si le scroll est vers le haut (deltaY > 0)
        bool ScrollsUp()   const noexcept { return mDeltaY > 0.0; }
        /// @brief Retourne true si le scroll est vers le bas (deltaY < 0)
        bool ScrollsDown() const noexcept { return mDeltaY < 0.0; }
    };

    // =========================================================================
    // NkMouseWheelHorizontalEvent — molette horizontale (scroll gauche/droite)
    // =========================================================================

    class NkMouseWheelHorizontalEvent final : public NkMouseWheelEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_WHEEL_HORIZONTAL)

        /// @param deltaX      Delta en "clics" (positif = vers la droite)
        NkMouseWheelHorizontalEvent(float64 deltaX,
                                    int32 x = 0, int32 y = 0,
                                    float64 pixelDeltaX    = 0.0,
                                    bool  highPrecision  = false,
                                    const NkModifierState& mods = {},
                                    uint64 windowId       = 0) noexcept
            : NkMouseWheelEvent(deltaX, 0.0, pixelDeltaX, 0.0, x, y, highPrecision, mods, windowId)
        {}

        NkEvent*    Clone()    const override { return new NkMouseWheelHorizontalEvent(*this); }
        NkString ToString() const override {
            return NkString::Fmt("MouseWheelH({0})", mDeltaX);
        }

        /// @brief Retourne true si le scroll est vers la gauche (deltaX < 0)
        bool ScrollsLeft()  const noexcept { return mDeltaX < 0.0; }
        /// @brief Retourne true si le scroll est vers la droite (deltaX > 0)
        bool ScrollsRight() const noexcept { return mDeltaX > 0.0; }
    };

    // =========================================================================
    // NkMouseEnterEvent / NkMouseLeaveEvent
    // Émis quand le curseur entre ou quitte la zone cliente de la fenêtre.
    // =========================================================================

    class NkMouseEnterEvent final : public NkMouseEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_ENTER)
        explicit NkMouseEnterEvent(uint64 windowId = 0) noexcept
            : NkMouseEvent(windowId) {}
        NkEvent*    Clone()    const override { return new NkMouseEnterEvent(*this); }
        NkString ToString() const override { return "MouseEnter()"; }
    };

    class NkMouseLeaveEvent final : public NkMouseEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_LEAVE)
        explicit NkMouseLeaveEvent(uint64 windowId = 0) noexcept
            : NkMouseEvent(windowId) {}
        NkEvent*    Clone()    const override { return new NkMouseLeaveEvent(*this); }
        NkString ToString() const override { return "MouseLeave()"; }
    };

    // =========================================================================
    // NkMouseWindowEnterEvent / NkMouseWindowLeaveEvent
    // Émis au niveau de la fenêtre (cadre inclus, pas seulement la zone client).
    // =========================================================================

    class NkMouseWindowEnterEvent final : public NkMouseEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_WINDOW_ENTER)
        explicit NkMouseWindowEnterEvent(uint64 windowId = 0) noexcept
            : NkMouseEvent(windowId) {}
        NkEvent*    Clone()    const override { return new NkMouseWindowEnterEvent(*this); }
        NkString ToString() const override { return "MouseWindowEnter()"; }
    };

    class NkMouseWindowLeaveEvent final : public NkMouseEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_WINDOW_LEAVE)
        explicit NkMouseWindowLeaveEvent(uint64 windowId = 0) noexcept
            : NkMouseEvent(windowId) {}
        NkEvent*    Clone()    const override { return new NkMouseWindowLeaveEvent(*this); }
        NkString ToString() const override { return "MouseWindowLeave()"; }
    };

    // =========================================================================
    // NkMouseCaptureBeginEvent / NkMouseCaptureEndEvent
    // La capture souris force tous les événements souris vers cette fenêtre,
    // même quand le curseur sort de la zone client.
    // =========================================================================

    class NkMouseCaptureBeginEvent final : public NkMouseEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_CAPTURE_BEGIN)
        explicit NkMouseCaptureBeginEvent(uint64 windowId = 0) noexcept
            : NkMouseEvent(windowId) {}
        NkEvent*    Clone()    const override { return new NkMouseCaptureBeginEvent(*this); }
        NkString ToString() const override { return "MouseCaptureBegin()"; }
    };

    class NkMouseCaptureEndEvent final : public NkMouseEvent {
    public:
        NK_EVENT_TYPE_FLAGS(NK_MOUSE_CAPTURE_END)
        explicit NkMouseCaptureEndEvent(uint64 windowId = 0) noexcept
            : NkMouseEvent(windowId) {}
        NkEvent*    Clone()    const override { return new NkMouseCaptureEndEvent(*this); }
        NkString ToString() const override { return "MouseCaptureEnd()"; }
    };

} // namespace nkentseu