#pragma once

// =============================================================================
// NkEventState.h
// État global consolidé du système d'événements et des entrées.
//
// Ce fichier joue deux rôles complémentaires :
//
//   1. ÉTATS SÉMANTIQUES (classes légères) :
//      Petites classes décrivant l'état d'un composant particulier :
//        NkWindowState, NkRegionState, NkConnectionState, NkResizeState,
//        NkAxisState, NkAxisDirection, NkStatusState, NkDraggedState,
//        NkFocusState.
//      Ces états sont utilisés dans les événements eux-mêmes et dans les
//      systèmes d'entrée pour communiquer la nature d'une transition.
//
//   2. SNAPSHOTS D'ENTRÉE (structs de polling) :
//      NkKeyboardInputState — touches pressées, modificateurs, dernière touche
//      NkMouseInputState    — position, boutons, capture, survol
//      NkTouchInputState    — contacts actifs, centroïde
//      NkGamepadInputState  — boutons, axes, connexion par slot
//      NkEventState         — agrège les quatre snapshots ci-dessus
//
//      Ces snapshots sont mis à jour par EventSystem en réponse aux événements
//      et peuvent être lus à tout moment par le code applicatif sans créer de
//      couplage avec la boucle d'événements.
//
// Dépendances :
//   NkKeyboardEvents.h — NkKey, NkScancode, NkModifierState
//   NkMouseEvents.h    — NkMouseButton, NkButtonState, NkMouseButtons
//   NkTouchEvents.h    — NkTouchPoint, NK_MAX_TOUCH_POINTS
//   NkGamepadEvents.h  — NkGamepadButton, NkGamepadAxis, NkGamepadInfo
// =============================================================================

#include "NkKeyboardEvent.h"
#include "NkMouseEvent.h"
#include "NkTouchEvent.h"
#include "NkGamepadEvent.h"
#include "NKMemory/NkUtils.h"

#include <bitset>
#include <string>

namespace nkentseu {

    // =========================================================================
    // NkWindowState — état du cycle de vie d'une fenêtre
    // =========================================================================

    /**
     * @brief Décrit l'état courant d'une fenêtre (créée, en cours de fermeture…)
     */
    struct NkWindowState {
        using Code = uint32;
        enum Value : Code {
            NK_WINDOW_UNDEFINED = 0,
            NK_WINDOW_CREATED,          ///< Fenêtre créée et visible
            NK_WINDOW_MINIMIZED,        ///< Fenêtre minimisée
            NK_WINDOW_MAXIMIZED,        ///< Fenêtre maximisée
            NK_WINDOW_FULLSCREEN,       ///< Fenêtre en plein écran
            NK_WINDOW_HIDDEN,           ///< Fenêtre masquée
            NK_WINDOW_CLOSED            ///< Fenêtre fermée (handle détruit)
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_WINDOW_CREATED:     return "Created";
                case NK_WINDOW_MINIMIZED:   return "Minimized";
                case NK_WINDOW_MAXIMIZED:   return "Maximized";
                case NK_WINDOW_FULLSCREEN:  return "Fullscreen";
                case NK_WINDOW_HIDDEN:      return "Hidden";
                case NK_WINDOW_CLOSED:      return "Closed";
                default:                     return "Undefined";
            }
        }
    };

    // =========================================================================
    // NkRegionState — état d'une région (survol curseur / touche)
    // =========================================================================

    /**
     * @brief Indique si le curseur / contact est entré dans ou sorti d'une zone.
     */
    struct NkRegionState {
        using Code = uint32;
        enum Value : Code {
            NK_REGION_UNDEFINED = 0,
            NK_REGION_ENTERED,  ///< Vient d'entrer dans la zone
            NK_REGION_INSIDE,   ///< Est à l'intérieur
            NK_REGION_EXITED    ///< Vient de quitter la zone
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_REGION_ENTERED: return "Entered";
                case NK_REGION_INSIDE:  return "Inside";
                case NK_REGION_EXITED:  return "Exited";
                default:                 return "Undefined";
            }
        }
    };

    // =========================================================================
    // NkConnectionState — état de connexion d'un périphérique
    // =========================================================================

    /**
     * @brief Décrit si un périphérique (manette, HID…) est connecté ou non.
     */
    struct NkConnectionState {
        using Code = uint32;
        enum Value : Code {
            NK_CONNECTION_UNDEFINED   = 0,
            NK_CONNECTION_ESTABLISHED,  ///< Périphérique connecté et initialisé
            NK_CONNECTION_TERMINATED    ///< Périphérique déconnecté
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_CONNECTION_ESTABLISHED: return "Established";
                case NK_CONNECTION_TERMINATED:  return "Terminated";
                default:                         return "Undefined";
            }
        }
    };

    // =========================================================================
    // NkResizeState — nature d'un redimensionnement
    // =========================================================================

    /**
     * @brief Décrit si une fenêtre a grandi, rétréci, ou n'a pas changé de taille.
     */
    struct NkResizeState {
        using Code = uint32;
        enum Value : Code {
            NK_RESIZE_UNDEFINED  = 0,
            NK_RESIZE_EXPANDED,     ///< La fenêtre a grandi (nouvelle dim > ancienne)
            NK_RESIZE_REDUCED,      ///< La fenêtre a rétréci
            NK_RESIZE_NO_CHANGE     ///< Dimensions identiques (changement de position seulement)
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_RESIZE_EXPANDED:  return "Expanded";
                case NK_RESIZE_REDUCED:   return "Reduced";
                case NK_RESIZE_NO_CHANGE: return "NoChange";
                default:                   return "Undefined";
            }
        }
    };

    // =========================================================================
    // NkAxisState — état qualitatif d'un axe analogique
    // =========================================================================

    /**
     * @brief Classification qualitative de la valeur d'un axe analogique.
     * Utile pour déclencher une action logique sans seuil codé en dur.
     */
    struct NkAxisState {
        using Code = uint32;
        enum Value : Code {
            NK_AXIS_UNDEFINED = 0,
            NK_AXIS_NEUTRAL,    ///< Dans la zone morte (|value| < deadzone)
            NK_AXIS_POSITIVE,   ///< Dépasse le seuil positif
            NK_AXIS_NEGATIVE    ///< Dépasse le seuil négatif
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_AXIS_NEUTRAL:  return "Neutral";
                case NK_AXIS_POSITIVE: return "Positive";
                case NK_AXIS_NEGATIVE: return "Negative";
                default:                return "Undefined";
            }
        }

        /// @brief Classifie une valeur normalisée [-1,1] avec deadzone
        static Value Classify(float value, float deadzone = 0.1f) noexcept {
            if (value >  deadzone) return NK_AXIS_POSITIVE;
            if (value < -deadzone) return NK_AXIS_NEGATIVE;
            return NK_AXIS_NEUTRAL;
        }
    };

    // =========================================================================
    // NkAxisDirection — direction sémantique d'un axe ou d'un groupe d'axes
    // =========================================================================

    /**
     * @brief Décrit les dimensions d'un contrôle analogique multi-axe.
     */
    struct NkAxisDirection {
        using Code = uint32;
        enum Value : Code {
            NK_AXIS_DIR_UNDEFINED           = 0,
            NK_AXIS_DIR_HORIZONTAL,             ///< 1D horizontal (X)
            NK_AXIS_DIR_VERTICAL,               ///< 1D vertical (Y)
            NK_AXIS_DIR_DEPTH,                  ///< 1D profondeur (Z)
            NK_AXIS_DIR_HORIZONTAL_VERTICAL,    ///< 2D XY (stick standard)
            NK_AXIS_DIR_HORIZONTAL_DEPTH,       ///< 2D XZ
            NK_AXIS_DIR_VERTICAL_DEPTH,         ///< 2D YZ
            NK_AXIS_DIR_ALL                     ///< 3D XYZ
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_AXIS_DIR_HORIZONTAL:          return "Horizontal";
                case NK_AXIS_DIR_VERTICAL:            return "Vertical";
                case NK_AXIS_DIR_DEPTH:               return "Depth";
                case NK_AXIS_DIR_HORIZONTAL_VERTICAL: return "HorizVert";
                case NK_AXIS_DIR_HORIZONTAL_DEPTH:    return "HorizDepth";
                case NK_AXIS_DIR_VERTICAL_DEPTH:      return "VertDepth";
                case NK_AXIS_DIR_ALL:                  return "All";
                default:                               return "Undefined";
            }
        }
    };

    // =========================================================================
    // NkStatusState — disponibilité générique d'une ressource / service
    // =========================================================================

    /**
     * @brief État générique de disponibilité d'un périphérique ou service.
     */
    struct NkStatusState {
        using Code = uint32;
        enum Value : Code {
            NK_STATUS_UNDEFINED    = 0,
            NK_STATUS_CONNECTED,       ///< Opérationnel
            NK_STATUS_DISCONNECTED,    ///< Déconnecté mais récupérable
            NK_STATUS_UNAVAILABLE      ///< Non disponible (erreur matérielle, driver absent…)
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_STATUS_CONNECTED:    return "Connected";
                case NK_STATUS_DISCONNECTED: return "Disconnected";
                case NK_STATUS_UNAVAILABLE:  return "Unavailable";
                default:                      return "Undefined";
            }
        }
    };

    // =========================================================================
    // NkDraggedState — état d'une opération de drag & drop
    // =========================================================================

    /**
     * @brief Phase d'une opération de glisser-déposer.
     */
    struct NkDraggedState {
        using Code = uint32;
        enum Value : Code {
            NK_DRAGGED_UNDEFINED = 0,
            NK_DRAGGED_ACTIVE,       ///< L'objet est en cours de glissement
            NK_DRAGGED_DROPPED,      ///< L'objet a été déposé
            NK_DRAGGED_CANCELLED     ///< L'opération a été annulée (touche Échap…)
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_DRAGGED_ACTIVE:    return "Active";
                case NK_DRAGGED_DROPPED:   return "Dropped";
                case NK_DRAGGED_CANCELLED: return "Cancelled";
                default:                    return "Undefined";
            }
        }
    };

    // =========================================================================
    // NkFocusState — état du focus clavier d'une fenêtre
    // =========================================================================

    /**
     * @brief Indique si une fenêtre a ou non le focus clavier.
     */
    struct NkFocusState {
        using Code = uint32;
        enum Value : Code {
            NK_FOCUS_UNDEFINED = 0,
            NK_FOCUS_GAINED,    ///< Focus clavier obtenu
            NK_FOCUS_LOST       ///< Focus clavier perdu
        };

        static const char* ToString(Code v) noexcept {
            switch (v) {
                case NK_FOCUS_GAINED: return "Gained";
                case NK_FOCUS_LOST:   return "Lost";
                default:               return "Undefined";
            }
        }
    };

    // =========================================================================
    // NkKeyboardInputState — snapshot clavier
    // =========================================================================

    /**
     * @brief Snapshot de l'état courant du clavier.
     *
     * Mis à jour par EventSystem en réponse aux NkKeyPressEvent / NkKeyReleaseEvent.
     * La bitset est indexée par NkKey (valeur uint32).
     *
     * Taille : 256 bits = 32 octets pour les touches pressées +
     *           256 bits pour l'auto-repeat = 64 octets total + modifiers.
     */
    struct NkKeyboardInputState {
        static constexpr uint32 KEY_COUNT = 256;

        std::bitset<KEY_COUNT> pressed;     ///< Touches actuellement enfoncées
        std::bitset<KEY_COUNT> repeated;    ///< Touches en auto-repeat OS (subset de pressed)
        NkModifierState        modifiers;   ///< État courant des modificateurs
        NkKey                  lastKey      = NkKey::NK_UNKNOWN; ///< Dernière touche pressée
        NkScancode             lastScancode = NkScancode::NK_SC_UNKNOWN; ///< Dernier scancode

        // --- Prédicats ---

        bool IsKeyPressed(NkKey key) const noexcept {
            uint32 idx = static_cast<uint32>(key);
            return (idx < KEY_COUNT) && pressed[idx];
        }

        bool IsKeyRepeated(NkKey key) const noexcept {
            uint32 idx = static_cast<uint32>(key);
            return (idx < KEY_COUNT) && repeated[idx];
        }

        bool IsAnyKeyPressed()  const noexcept { return pressed.any(); }

        bool IsCtrlDown()  const noexcept { return modifiers.ctrl;  }
        bool IsAltDown()   const noexcept { return modifiers.alt;   }
        bool IsShiftDown() const noexcept { return modifiers.shift; }
        bool IsSuperDown() const noexcept { return modifiers.super; }

        // --- Mises à jour (appelées par EventSystem) ---

        void OnKeyPress(NkKey key, NkScancode sc, const NkModifierState& mods) noexcept {
            uint32 idx = static_cast<uint32>(key);
            if (idx < KEY_COUNT) pressed[idx] = true;
            lastKey      = key;
            lastScancode = sc;
            modifiers    = mods;
        }

        void OnKeyRepeat(NkKey key, NkScancode sc, const NkModifierState& mods) noexcept {
            uint32 idx = static_cast<uint32>(key);
            if (idx < KEY_COUNT) { pressed[idx] = true; repeated[idx] = true; }
            lastKey      = key;
            lastScancode = sc;
            modifiers    = mods;
        }

        void OnKeyRelease(NkKey key, const NkModifierState& mods) noexcept {
            uint32 idx = static_cast<uint32>(key);
            if (idx < KEY_COUNT) { pressed[idx] = false; repeated[idx] = false; }
            modifiers = mods;
        }

        void Clear() noexcept {
            pressed.reset();
            repeated.reset();
            modifiers    = {};
            lastKey      = NkKey::NK_UNKNOWN;
            lastScancode = NkScancode::NK_SC_UNKNOWN;
        }
    };

    // =========================================================================
    // NkMouseInputState — snapshot souris
    // =========================================================================

    /**
     * @brief Snapshot de l'état courant de la souris.
     *
     * Toutes les coordonnées sont en pixels physiques dans l'espace de la fenêtre
     * (coin supérieur gauche = 0,0).
     */
    struct NkMouseInputState {
        int32 x       = 0;  ///< Position X courante (zone client)
        int32 y       = 0;  ///< Position Y courante (zone client)
        int32 screenX = 0;  ///< Position X écran
        int32 screenY = 0;  ///< Position Y écran
        int32 deltaX  = 0;  ///< Déplacement X depuis le dernier poll
        int32 deltaY  = 0;  ///< Déplacement Y depuis le dernier poll
        int32 rawDeltaX = 0; ///< Mouvement brut X (sans accélération, pour FPS)
        int32 rawDeltaY = 0; ///< Mouvement brut Y

        NkMouseButtons buttons;     ///< Boutons actuellement enfoncés
        NkModifierState modifiers;  ///< Modificateurs clavier au dernier événement

        bool captured = false;      ///< Capture souris active (SetCapture)
        bool insideWindow = false;  ///< Curseur dans la zone client
        bool insideFrame  = false;  ///< Curseur dans le cadre de la fenêtre

        // --- Prédicats ---

        bool IsButtonPressed(NkMouseButton btn) const noexcept {
            return buttons.IsDown(btn);
        }
        bool IsAnyButtonPressed() const noexcept { return buttons.Any(); }
        bool IsLeftDown()   const noexcept { return buttons.IsDown(NkMouseButton::NK_MB_LEFT);   }
        bool IsRightDown()  const noexcept { return buttons.IsDown(NkMouseButton::NK_MB_RIGHT);  }
        bool IsMiddleDown() const noexcept { return buttons.IsDown(NkMouseButton::NK_MB_MIDDLE); }

        // --- Mises à jour (appelées par EventSystem) ---

        void OnMove(int32 nx, int32 ny, int32 nsx, int32 nsy) noexcept {
            deltaX = nx - x; deltaY = ny - y;
            x = nx; y = ny;
            screenX = nsx; screenY = nsy;
        }

        void OnRaw(int32 rdx, int32 rdy) noexcept {
            rawDeltaX = rdx; rawDeltaY = rdy;
        }

        void OnButtonPress(NkMouseButton btn, const NkModifierState& mods) noexcept {
            buttons.Set(btn); modifiers = mods;
        }

        void OnButtonRelease(NkMouseButton btn, const NkModifierState& mods) noexcept {
            buttons.Clear(btn); modifiers = mods;
        }

        void OnEnter()  noexcept { insideWindow = true; }
        void OnLeave()  noexcept { insideWindow = false; buttons = {}; }

        void Clear() noexcept {
            x = y = screenX = screenY = 0;
            deltaX = deltaY = rawDeltaX = rawDeltaY = 0;
            buttons     = {};
            modifiers   = {};
            captured    = false;
            insideWindow = false;
            insideFrame  = false;
        }
    };

    // =========================================================================
    // NkTouchInputState — snapshot des contacts tactiles
    // =========================================================================

    /**
     * @brief Snapshot de l'état courant du multi-touch.
     *
     * Maintient jusqu'à NK_MAX_TOUCH_POINTS contacts actifs simultanément.
     * Les contacts sont identifiés par leur id (stable tant qu'un contact est actif).
     */
    struct NkTouchInputState {
        /// @brief Contact simplifié pour le snapshot (différent de NkTouchPoint d'événement)
        struct TouchSlot {
            uint64  id       = 0;
            float32  x        = 0.f; ///< Position X client [pixels]
            float32  y        = 0.f; ///< Position Y client [pixels]
            float32  pressure = 1.f; ///< Pression [0,1]
            bool   active   = false;
        };

        TouchSlot slots[NK_MAX_TOUCH_POINTS] = {};
        uint32     activeCount = 0;   ///< Nombre de contacts actifs
        float32     centroidX   = 0.f; ///< X moyen des contacts actifs
        float32     centroidY   = 0.f; ///< Y moyen des contacts actifs

        bool IsTouching()    const noexcept { return activeCount > 0; }
        bool IsMultiTouch()  const noexcept { return activeCount >= 2; }

        /// @brief Recherche un slot actif par ID, retourne nullptr si absent
        const TouchSlot* FindById(uint64 id) const noexcept {
            for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i)
                if (slots[i].active && slots[i].id == id) return &slots[i];
            return nullptr;
        }

        // --- Mises à jour ---

        void OnBegin(const NkTouchPoint& pt) noexcept {
            // Chercher un slot libre
            for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
                if (!slots[i].active) {
                    slots[i] = { pt.id, pt.clientX, pt.clientY, pt.pressure, true };
                    ++activeCount;
                    UpdateCentroid();
                    return;
                }
            }
        }

        void OnMove(const NkTouchPoint& pt) noexcept {
            for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
                if (slots[i].active && slots[i].id == pt.id) {
                    slots[i].x        = pt.clientX;
                    slots[i].y        = pt.clientY;
                    slots[i].pressure = pt.pressure;
                    UpdateCentroid();
                    return;
                }
            }
        }

        void OnEnd(uint64 id) noexcept {
            for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i) {
                if (slots[i].active && slots[i].id == id) {
                    slots[i] = {};
                    if (activeCount > 0) --activeCount;
                    UpdateCentroid();
                    return;
                }
            }
        }

        void OnCancel() noexcept { Clear(); }

        void UpdateCentroid() noexcept {
            if (activeCount == 0) { centroidX = centroidY = 0.f; return; }
            float32 sx = 0.f, sy = 0.f;
            for (uint32 i = 0; i < NK_MAX_TOUCH_POINTS; ++i)
                if (slots[i].active) { sx += slots[i].x; sy += slots[i].y; }
            centroidX = sx / static_cast<float32>(activeCount);
            centroidY = sy / static_cast<float32>(activeCount);
        }

        void Clear() noexcept {
            for (auto& s : slots) s = {};
            activeCount = 0;
            centroidX   = centroidY = 0.f;
        }
    };

    // =========================================================================
    // NkGamepadInputState — snapshot d'une manette individuelle
    // =========================================================================

    /**
     * @brief Snapshot de l'état d'une manette à un instant donné.
     *
     * Indexé par NkGamepadButton (boutons) et NkGamepadAxis (axes).
     * La zone morte n'est PAS appliquée ici : c'est la responsabilité du backend.
     */
    struct NkGamepadInputState {
        static constexpr uint32 BUTTON_COUNT = static_cast<uint32>(NkGamepadButton::NK_GAMEPAD_BUTTON_MAX);
        static constexpr uint32 AXIS_COUNT   = static_cast<uint32>(NkGamepadAxis::NK_GAMEPAD_AXIS_MAX);

        bool          connected = false;
        NkGamepadInfo info;
        float32         batteryLevel = -1.f; ///< [0,1] ou -1 (filaire/inconnu)

        std::bitset<BUTTON_COUNT> buttons;  ///< Boutons actuellement enfoncés
        float32 axes[AXIS_COUNT]   = {};       ///< Valeurs des axes (après deadzone)
        float32 prevAxes[AXIS_COUNT] = {};     ///< Valeurs au poll précédent

        // --- Prédicats ---

        bool IsButtonDown(NkGamepadButton btn) const noexcept {
            uint32 idx = static_cast<uint32>(btn);
            return (idx < BUTTON_COUNT) && buttons[idx];
        }

        float32 GetAxis(NkGamepadAxis ax) const noexcept {
            uint32 idx = static_cast<uint32>(ax);
            return (idx < AXIS_COUNT) ? axes[idx] : 0.f;
        }

        float32 GetPrevAxis(NkGamepadAxis ax) const noexcept {
            uint32 idx = static_cast<uint32>(ax);
            return (idx < AXIS_COUNT) ? prevAxes[idx] : 0.f;
        }

        NkAxisState::Value GetAxisState(NkGamepadAxis ax,
                                        float32 deadzone = 0.1f) const noexcept {
            return NkAxisState::Classify(GetAxis(ax), deadzone);
        }

        bool IsConnected()  const noexcept { return connected; }
        bool IsCharging()   const noexcept { return false; } // rempli par NkGamepadBatteryEvent

        // --- Mises à jour ---

        void OnButtonPress(NkGamepadButton btn) noexcept {
            uint32 idx = static_cast<uint32>(btn);
            if (idx < BUTTON_COUNT) buttons[idx] = true;
        }

        void OnButtonRelease(NkGamepadButton btn) noexcept {
            uint32 idx = static_cast<uint32>(btn);
            if (idx < BUTTON_COUNT) buttons[idx] = false;
        }

        void OnAxisMove(NkGamepadAxis ax, float32 value) noexcept {
            uint32 idx = static_cast<uint32>(ax);
            if (idx < AXIS_COUNT) {
                prevAxes[idx] = axes[idx];
                axes[idx]     = value;
            }
        }

        void Clear() noexcept {
            connected     = false;
            batteryLevel  = -1.f;
            buttons.reset();
            memory::NkMemSet(axes,     0, sizeof(axes));
            memory::NkMemSet(prevAxes, 0, sizeof(prevAxes));
        }
    };

    // =========================================================================
    // NkGamepadSetState — ensemble des slots de manettes
    // =========================================================================

    /**
     * @brief Gère jusqu'à NK_MAX_GAMEPADS manettes simultanées.
     */
    static constexpr uint32 NK_MAX_GAMEPADS_STATE = 8;

    struct NkGamepadSetState {
        NkGamepadInputState slots[NK_MAX_GAMEPADS_STATE];
        uint32               connectedCount = 0;

        NkGamepadInputState*       GetSlot(uint32 idx)       noexcept {
            return (idx < NK_MAX_GAMEPADS_STATE) ? &slots[idx] : nullptr;
        }
        const NkGamepadInputState* GetSlot(uint32 idx) const noexcept {
            return (idx < NK_MAX_GAMEPADS_STATE) ? &slots[idx] : nullptr;
        }

        bool IsButtonDown(uint32 idx, NkGamepadButton btn) const noexcept {
            const auto* s = GetSlot(idx);
            return s && s->IsButtonDown(btn);
        }
        float32 GetAxis(uint32 idx, NkGamepadAxis ax) const noexcept {
            const auto* s = GetSlot(idx);
            return s ? s->GetAxis(ax) : 0.f;
        }

        void OnConnect(uint32 idx, const NkGamepadInfo& info) noexcept {
            if (idx >= NK_MAX_GAMEPADS_STATE) return;
            slots[idx].Clear();
            slots[idx].connected = true;
            slots[idx].info      = info;
            ++connectedCount;
        }

        void OnDisconnect(uint32 idx) noexcept {
            if (idx >= NK_MAX_GAMEPADS_STATE) return;
            slots[idx].Clear();
            if (connectedCount > 0) --connectedCount;
        }

        void Clear() noexcept {
            for (auto& s : slots) s.Clear();
            connectedCount = 0;
        }
    };

    // =========================================================================
    // NkEventState — snapshot global agrégé de toutes les entrées
    // =========================================================================

    /**
     * @brief Agrège les snapshots de tous les périphériques d'entrée.
     *
     * Instance unique maintenue par EventSystem et mise à jour en temps réel.
     * Peut être interrogé à tout moment par le code applicatif pour connaître
     * l'état courant d'une entrée sans attendre le prochain événement.
     *
     * @code
     *   const auto& state = EventSystem::Instance().GetInputState();
     *
     *   // Clavier
     *   if (state.keyboard.IsKeyPressed(NkKey::NK_SPACE)) Jump();
     *   if (state.keyboard.modifiers.ctrl) ShowDebugOverlay();
     *
     *   // Souris
     *   if (state.mouse.IsLeftDown()) StartDrag(state.mouse.x, state.mouse.y);
     *
     *   // Manette
     *   if (state.gamepads.IsButtonDown(0, NkGamepadButton::NK_GP_SOUTH)) Fire();
     *   float lx = state.gamepads.GetAxis(0, NkGamepadAxis::NK_GP_AXIS_LX);
     * @endcode
     */
    class NkEventState {
    public:
        NkEventState() = default;

        // --- Accès en lecture ---

        const NkKeyboardInputState& GetKeyboard()  const noexcept { return keyboard; }
        const NkMouseInputState&    GetMouse()      const noexcept { return mouse; }
        const NkTouchInputState&    GetTouch()      const noexcept { return touch; }
        const NkGamepadSetState&    GetGamepads()   const noexcept { return gamepads; }

        // --- Accès en écriture (réservé à EventSystem) ---

        NkKeyboardInputState& GetKeyboard()  noexcept { return keyboard; }
        NkMouseInputState&    GetMouse()     noexcept { return mouse; }
        NkTouchInputState&    GetTouch()     noexcept { return touch; }
        NkGamepadSetState&    GetGamepads()  noexcept { return gamepads; }

        // --- Prédicats globaux ---

        /// @brief Retourne true si au moins une entrée est active (touche, bouton, contact)
        bool IsAnyInputActive() const noexcept {
            return keyboard.IsAnyKeyPressed()
                || mouse.IsAnyButtonPressed()
                || touch.IsTouching()
                || gamepads.connectedCount > 0;
        }

        // --- Réinitialisation ---

        /// @brief Remet à zéro tous les snapshots (typiquement lors d'un changement de focus)
        void Clear() noexcept {
            keyboard.Clear();
            mouse.Clear();
            touch.Clear();
            gamepads.Clear();
        }

        // --- Membres publics (accès direct autorisé pour la performance) ---

        NkKeyboardInputState keyboard;
        NkMouseInputState    mouse;
        NkTouchInputState    touch;
        NkGamepadSetState    gamepads;
    };

} // namespace nkentseu
