#pragma once

// =============================================================================
// NkKeyboardEvent.h
// Hiérarchie complète des événements clavier.
//
// Contenu :
//   NkKey              — codes touches cross-platform (position US-QWERTY)
//   NkScancode         — code physique USB HID (position indépendante du layout)
//   NkModifierState    — état des modificateurs au moment de l'événement
//   NkKeyboardEvent    — base abstraite (catégorie KEYBOARD | INPUT)
//   NkKeyPressEvent    — touche pressée (première frappe)
//   NkKeyRepeatEvent   — touche maintenue (auto-repeat OS)
//   NkKeyReleaseEvent  — touche relâchée
//   NkTextInputEvent   — caractère Unicode produit (après traitement IME)
//
// Stratégie de conversion cross-platform :
//   Plateforme  → Scancode USB HID  → NkKey (via NkScancodeToKey)
//   Win32  : NkScancodeFromWin32(scancode, extended)
//   Linux  : NkScancodeFromLinux(evdev_keycode)  ou  NkScancodeFromXKeycode(xkeycode)
//   macOS  : NkScancodeFromMac(carbon_keyCode)
//   Web    : NkScancodeFromDOMCode(domCode)
// =============================================================================

#include "NkEvent.h"
#include "NKContainers/String/NkStringUtils.h"
#include <cstring>
#include <cstdio>
#include <string>

namespace nkentseu {

    // =========================================================================
    // NkKey — codes touches sémantiques (position physique US-QWERTY)
    //
    // Un NkKey identifie la POSITION d'une touche sur un clavier US-QWERTY,
    // indépendamment du layout utilisateur.
    // Ex : NkKey::NK_Q = touche à la position du Q en QWERTY
    //      (sur AZERTY, c'est la touche qui produit le caractère 'A')
    // =========================================================================

    enum class NkKey : NkU32 {
        NK_UNKNOWN = 0,

        // --- Touches de fonction ---
        NK_ESCAPE,
        NK_F1,  
        NK_F2,  
        NK_F3,  
        NK_F4,  
        NK_F5,  
        NK_F6,
        NK_F7,  
        NK_F8,  
        NK_F9,  
        NK_F10, 
        NK_F11, 
        NK_F12,
        NK_F13, 
        NK_F14, 
        NK_F15, 
        NK_F16, 
        NK_F17, 
        NK_F18,
        NK_F19, 
        NK_F20, 
        NK_F21, 
        NK_F22, 
        NK_F23, 
        NK_F24,

        // --- Ligne du haut ---
        NK_GRAVE,           ///< `~
        NK_NUM1, 
        NK_NUM2, 
        NK_NUM3, 
        NK_NUM4, 
        NK_NUM5,
        NK_NUM6, 
        NK_NUM7, 
        NK_NUM8, 
        NK_NUM9, 
        NK_NUM0,
        NK_MINUS,           ///< -_
        NK_EQUALS,          ///< =+
        NK_BACK,            ///< Backspace

        // --- Rangée QWERTY ---
        NK_TAB,
        NK_Q, 
        NK_W, 
        NK_E, 
        NK_R, 
        NK_T, 
        NK_Y,
        NK_U, 
        NK_I, 
        NK_O, 
        NK_P,
        NK_LBRACKET,        ///< [{
        NK_RBRACKET,        ///< ]}
        NK_BACKSLASH,       ///< \|

        // --- Rangée ASDF ---
        NK_CAPSLOCK,
        NK_A, 
        NK_S, 
        NK_D, 
        NK_F, 
        NK_G,
        NK_H, 
        NK_J, 
        NK_K, 
        NK_L,
        NK_SEMICOLON,       ///< ;:
        NK_APOSTROPHE,      ///< '"
        NK_ENTER,

        // --- Rangée ZXCV ---
        NK_LSHIFT,
        NK_Z, 
        NK_X, 
        NK_C, 
        NK_V, 
        NK_B,
        NK_N, 
        NK_M,
        NK_COMMA,           ///< ,<
        NK_PERIOD,          ///< .>
        NK_SLASH,           ///< /?
        NK_RSHIFT,

        // --- Rangée inférieure ---
        NK_LCTRL,
        NK_LSUPER,          ///< Win / Cmd / Meta gauche
        NK_LALT,
        NK_SPACE,
        NK_RALT,            ///< AltGr
        NK_RSUPER,          ///< Win / Cmd / Meta droit
        NK_MENU,            ///< Touche menu contextuel
        NK_RCTRL,

        // --- Bloc navigation ---
        NK_PRINT_SCREEN,
        NK_SCROLL_LOCK,
        NK_PAUSE_BREAK,
        NK_INSERT,
        NK_DELETE,
        NK_HOME,
        NK_END,
        NK_PAGE_UP,
        NK_PAGE_DOWN,

        // --- Flèches ---
        NK_UP, 
        NK_DOWN, 
        NK_LEFT, 
        NK_RIGHT,

        // --- Pavé numérique ---
        NK_NUM_LOCK,
        NK_NUMPAD_DIV,      ///< /
        NK_NUMPAD_MUL,      ///< *
        NK_NUMPAD_SUB,      ///< -
        NK_NUMPAD_ADD,      ///< +
        NK_NUMPAD_ENTER,
        NK_NUMPAD_DOT,
        NK_NUMPAD_0, 
        NK_NUMPAD_1, 
        NK_NUMPAD_2, 
        NK_NUMPAD_3, 
        NK_NUMPAD_4,
        NK_NUMPAD_5, 
        NK_NUMPAD_6, 
        NK_NUMPAD_7, 
        NK_NUMPAD_8, 
        NK_NUMPAD_9,
        NK_NUMPAD_EQUALS,   ///< = (Mac / certains claviers)

        // --- Médias ---
        NK_MEDIA_PLAY_PAUSE,
        NK_MEDIA_STOP,
        NK_MEDIA_NEXT,
        NK_MEDIA_PREV,
        NK_MEDIA_VOLUME_UP,
        NK_MEDIA_VOLUME_DOWN,
        NK_MEDIA_MUTE,

        // --- Navigateur ---
        NK_BROWSER_BACK,
        NK_BROWSER_FORWARD,
        NK_BROWSER_REFRESH,
        NK_BROWSER_HOME,
        NK_BROWSER_SEARCH,
        NK_BROWSER_FAVORITES,

        // --- International / IME ---
        NK_KANA,
        NK_KANJI,
        NK_CONVERT,
        NK_NONCONVERT,
        NK_HANGUL,
        NK_HANJA,

        // --- Divers ---
        NK_SLEEP,
        NK_CLEAR,
        NK_SEPARATOR,
        NK_OEM_1, 
        NK_OEM_2, 
        NK_OEM_3, 
        NK_OEM_4,
        NK_OEM_5, 
        NK_OEM_6, 
        NK_OEM_7, 
        NK_OEM_8,

        NK_KEY_MAX  ///< Sentinelle
    };

    // =========================================================================
    // Utilitaires NkKey
    // =========================================================================

    /// @brief Retourne le nom lisible d'une touche (ex: "NK_A", "NK_SPACE")
    const char* NkKeyToString(NkKey key) noexcept;

    /// @brief Retourne true si la touche est un modificateur (Ctrl, Alt, Shift, Super)
    bool NkKeyIsModifier(NkKey key) noexcept;

    /// @brief Retourne true si la touche appartient au pavé numérique
    bool NkKeyIsNumpad(NkKey key) noexcept;

    /// @brief Retourne true si la touche est une touche de fonction (F1–F24)
    bool NkKeyIsFunctionKey(NkKey key) noexcept;

    // =========================================================================
    // NkScancode — code physique USB HID (indépendant du layout clavier)
    //
    // Valeurs = USB HID Keyboard/Keypad Usage IDs (Table 10, spec HID 1.11)
    // =========================================================================

    enum class NkScancode : NkU32 {
        NK_SC_UNKNOWN = 0,

        // Lettres (ordre HID, pas alphabétique)
        NK_SC_A = 0x04, 
        NK_SC_B, 
        NK_SC_C, 
        NK_SC_D, 
        NK_SC_E, 
        NK_SC_F,
        NK_SC_G, 
        NK_SC_H, 
        NK_SC_I, 
        NK_SC_J, 
        NK_SC_K, 
        NK_SC_L,
        NK_SC_M, 
        NK_SC_N, 
        NK_SC_O, 
        NK_SC_P, 
        NK_SC_Q, 
        NK_SC_R,
        NK_SC_S, 
        NK_SC_T, 
        NK_SC_U, 
        NK_SC_V, 
        NK_SC_W, 
        NK_SC_X,
        NK_SC_Y = 0x1C, 
        NK_SC_Z = 0x1D,

        // Chiffres ligne du haut
        NK_SC_1 = 0x1E, 
        NK_SC_2, 
        NK_SC_3, 
        NK_SC_4, 
        NK_SC_5,
        NK_SC_6, 
        NK_SC_7, 
        NK_SC_8, 
        NK_SC_9,
        NK_SC_0 = 0x27,

        // Touches de contrôle principales
        NK_SC_ENTER      = 0x28, ///< Enter principal
        NK_SC_ESCAPE     = 0x29,
        NK_SC_BACKSPACE  = 0x2A,
        NK_SC_TAB        = 0x2B,
        NK_SC_SPACE      = 0x2C,
        NK_SC_MINUS      = 0x2D, ///< -_
        NK_SC_EQUALS     = 0x2E, ///< =+
        NK_SC_LBRACKET   = 0x2F, ///< [{
        NK_SC_RBRACKET   = 0x30, ///< ]}
        NK_SC_BACKSLASH  = 0x31, ///< \|
        NK_SC_NONUS_HASH = 0x32, ///< #~ (ISO non-US)
        NK_SC_SEMICOLON  = 0x33, ///< ;:
        NK_SC_APOSTROPHE = 0x34, ///< '"
        NK_SC_GRAVE      = 0x35, ///< `~
        NK_SC_COMMA      = 0x36, ///< ,<
        NK_SC_PERIOD     = 0x37, ///< .>
        NK_SC_SLASH      = 0x38, ///< /?

        // Touches de fonction
        NK_SC_CAPS_LOCK = 0x39,
        NK_SC_F1  = 0x3A, 
        NK_SC_F2,  
        NK_SC_F3,  
        NK_SC_F4,
        NK_SC_F5,  
        NK_SC_F6,  
        NK_SC_F7,  
        NK_SC_F8,
        NK_SC_F9,  
        NK_SC_F10, 
        NK_SC_F11, 
        NK_SC_F12,

        // Bloc de contrôle
        NK_SC_PRINT_SCREEN = 0x46,
        NK_SC_SCROLL_LOCK  = 0x47,
        NK_SC_PAUSE        = 0x48,
        NK_SC_INSERT       = 0x49,
        NK_SC_HOME         = 0x4A,
        NK_SC_PAGE_UP      = 0x4B,
        NK_SC_DELETE       = 0x4C,
        NK_SC_END          = 0x4D,
        NK_SC_PAGE_DOWN    = 0x4E,
        NK_SC_RIGHT        = 0x4F,
        NK_SC_LEFT         = 0x50,
        NK_SC_DOWN         = 0x51,
        NK_SC_UP           = 0x52,

        // Pavé numérique
        NK_SC_NUM_LOCK      = 0x53,
        NK_SC_NUMPAD_DIV    = 0x54,
        NK_SC_NUMPAD_MUL    = 0x55,
        NK_SC_NUMPAD_SUB    = 0x56,
        NK_SC_NUMPAD_ADD    = 0x57,
        NK_SC_NUMPAD_ENTER  = 0x58,
        NK_SC_NUMPAD_1      = 0x59, 
        NK_SC_NUMPAD_2, 
        NK_SC_NUMPAD_3,
        NK_SC_NUMPAD_4,     
        NK_SC_NUMPAD_5, 
        NK_SC_NUMPAD_6,
        NK_SC_NUMPAD_7,     
        NK_SC_NUMPAD_8, 
        NK_SC_NUMPAD_9,
        NK_SC_NUMPAD_0      = 0x62,
        NK_SC_NUMPAD_DOT    = 0x63,

        // Touches ISO / additionnelles
        NK_SC_NONUS_BACKSLASH = 0x64, ///< Touche ISO entre LShift et Z
        NK_SC_APPLICATION     = 0x65, ///< Touche menu / contextuel
        NK_SC_POWER           = 0x66,
        NK_SC_NUMPAD_EQUALS   = 0x67, ///< = pavé numérique (Mac)

        // F13–F24
        NK_SC_F13 = 0x68, 
        NK_SC_F14, 
        NK_SC_F15, 
        NK_SC_F16,
        NK_SC_F17, 
        NK_SC_F18, 
        NK_SC_F19, 
        NK_SC_F20,
        NK_SC_F21, 
        NK_SC_F22, 
        NK_SC_F23, 
        NK_SC_F24,

        // Touches multimédia (usage IDs Consumer)
        NK_SC_MEDIA_PLAY_PAUSE  = 0xE0CD,
        NK_SC_MEDIA_STOP        = 0xE0B7,
        NK_SC_MEDIA_NEXT        = 0xE0B5,
        NK_SC_MEDIA_PREV        = 0xE0B6,

        // ---[ 0x74–0x78 : Touches multimédia / contrôle ]------------------------
        NK_SC_EXECUTE = 0x74,
        NK_SC_HELP = 0x75,
        NK_SC_MENU = 0x76,
        NK_SC_SELECT = 0x77,
        NK_SC_STOP = 0x78,
        NK_SC_AGAIN = 0x79,
        NK_SC_UNDO = 0x7A,
        NK_SC_CUT = 0x7B,
        NK_SC_COPY = 0x7C,
        NK_SC_PASTE = 0x7D,
        NK_SC_FIND = 0x7E,
        NK_SC_MUTE = 0x7F,
        NK_SC_VOLUME_UP = 0x80,
        NK_SC_VOLUME_DOWN = 0x81,

        // Modificateurs
        NK_SC_LCTRL  = 0xE0,
        NK_SC_LSHIFT = 0xE1,
        NK_SC_LALT   = 0xE2,
        NK_SC_LSUPER = 0xE3, ///< Win / Cmd / Meta gauche
        NK_SC_RCTRL  = 0xE4,
        NK_SC_RSHIFT = 0xE5,
        NK_SC_RALT   = 0xE6, ///< AltGr
        NK_SC_RSUPER = 0xE7, ///< Win / Cmd / Meta droit

        NK_SC_MAX = 0x100    ///< Sentinelle
    };

    // =========================================================================
    // Conversions cross-platform : Scancode USB HID ↔ plateformes
    // =========================================================================

    /// @brief Retourne le nom lisible d'un scancode (ex: "SC_A")
    const char* NkScancodeToString(NkScancode sc) noexcept;

    /// @brief Convertit un scancode USB HID en NkKey (US-QWERTY invariant)
    NkKey NkScancodeToKey(NkScancode sc) noexcept;

    /// @brief Win32 PS/2 Set-1 → USB HID
    /// @param win32Scancode  Bits 16–23 du LPARAM de WM_KEYDOWN
    /// @param extended       Bit 24 du LPARAM (touche étendue)
    NkScancode NkScancodeFromWin32(NkU32 win32Scancode, bool extended) noexcept;

    /// @brief Linux evdev keycode → USB HID
    /// @note  Pour XCB/XLib, utiliser NkScancodeFromXKeycode(xcb_keycode_t)
    NkScancode NkScancodeFromLinux(NkU32 linuxKeycode) noexcept;

    /// @brief XCB / XLib keycode → USB HID (soustrait 8 avant conversion evdev)
    NkScancode NkScancodeFromXKeycode(NkU32 xkeycode) noexcept;

    /// @brief macOS NSEvent.keyCode (Carbon) → USB HID
    NkScancode NkScancodeFromMac(NkU32 macKeycode) noexcept;

    /// @brief DOM KeyboardEvent.code → USB HID (WebAssembly)
    /// @param domCode  ex: "KeyA", "Space", "ArrowLeft", "Numpad0"
    NkScancode NkScancodeFromDOMCode(const char* domCode) noexcept;

    // =========================================================================
    // NkModifierState — état des modificateurs au moment de l'événement
    // =========================================================================

    struct NkModifierState {
        bool ctrl    = false; ///< LCtrl ou RCtrl (au moins un enfoncé)
        bool alt     = false; ///< LAlt  (hors AltGr)
        bool shift   = false; ///< LShift ou RShift
        bool super   = false; ///< Win / Cmd / Meta
        bool altGr   = false; ///< AltGr (distinctif sur claviers internationaux)
        bool numLock = false; ///< Verrou numérique actif
        bool capLock = false; ///< Verrou majuscules actif
        bool scrLock = false; ///< Verrou défilement actif

        NkModifierState() = default;

        /// @brief Constructeur rapide pour les modificateurs communs
        NkModifierState(bool ctrl, bool alt, bool shift, bool super = false) noexcept
            : ctrl(ctrl), alt(alt), shift(shift), super(super) {}

        /// @brief Retourne true si au moins un modificateur principal est actif
        bool Any()    const noexcept { return ctrl || alt || shift || super || altGr; }
        /// @brief Retourne true si aucun modificateur principal n'est actif
        bool IsNone() const noexcept { return !Any(); }

        bool operator==(const NkModifierState& o) const noexcept {
            return ctrl  == o.ctrl  && alt   == o.alt   &&
                   shift == o.shift && super == o.super && altGr == o.altGr;
        }
        bool operator!=(const NkModifierState& o) const noexcept { return !(*this == o); }

        /// @brief Retourne une string lisible ex: "Ctrl+Shift"
        NkString ToString() const {
            NkString s;
            if (ctrl)  s += "Ctrl+";
            if (alt)   s += "Alt+";
            if (shift) s += "Shift+";
            if (super) s += "Super+";
            if (altGr) s += "AltGr+";
            if (s.Empty()) return "None";
            s.PopBack(); // enlever le dernier '+'
            return s;
        }
    };

    // =========================================================================
    // NkKeyboardEvent — base abstraite pour tous les événements clavier
    //
    // Catégorie : NK_CAT_KEYBOARD | NK_CAT_INPUT
    // =========================================================================

    class NkKeyboardEvent : public NkEvent {
        public:
            /// @brief Retourne les flags de catégorie (KEYBOARD + INPUT)
            NkU32 GetCategoryFlags() const override {
                return static_cast<NkU32>(NkEventCategory::NK_CAT_KEYBOARD)
                    | static_cast<NkU32>(NkEventCategory::NK_CAT_INPUT);
            }

            // --- Accesseurs communs ---

            /// @brief Touche logique (position US-QWERTY)
            NkKey             GetKey()        const noexcept { return mKey;        }
            /// @brief Code physique USB HID
            NkScancode        GetScancode()   const noexcept { return mScancode;   }
            /// @brief État des modificateurs au moment de l'événement
            NkModifierState   GetModifiers()  const noexcept { return mModifiers;  }
            /// @brief Code natif brut de la plateforme (VK, KeySym, keyCode…)
            NkU32             GetNativeKey()  const noexcept { return mNativeKey;  }
            /// @brief Retourne true si c'est une touche étendue (préfixe E0 PS/2)
            bool              IsExtended()    const noexcept { return mExtended;   }

            // --- Raccourcis modificateurs ---

            bool HasCtrl()  const noexcept { return mModifiers.ctrl;  }
            bool HasAlt()   const noexcept { return mModifiers.alt;   }
            bool HasShift() const noexcept { return mModifiers.shift; }
            bool HasSuper() const noexcept { return mModifiers.super; }
            bool HasAltGr() const noexcept { return mModifiers.altGr; }

        protected:
            /// @brief Constructeur interne — à appeler depuis les classes dérivées
            NkKeyboardEvent(NkKey                  key,
                            NkScancode             scancode,
                            const NkModifierState& mods,
                            NkU32                  nativeKey,
                            bool                   extended,
                            NkU64                  windowId) noexcept
                : NkEvent(windowId)
                , mKey(key), mScancode(scancode)
                , mModifiers(mods), mNativeKey(nativeKey), mExtended(extended)
            {}

            NkKey           mKey       = NkKey::NK_UNKNOWN;   ///< Touche logique
            NkScancode      mScancode  = NkScancode::NK_SC_UNKNOWN; ///< Code physique HID
            NkModifierState mModifiers;                        ///< Modificateurs actifs
            NkU32           mNativeKey = 0;                    ///< Code natif plateforme
            bool            mExtended  = false;                ///< Touche étendue ?
    };

    // =========================================================================
    // NkKeyPressEvent — touche pressée (première frappe)
    // =========================================================================

    class NkKeyPressEvent final : public NkKeyboardEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_KEY_PRESSED)

            /// @param key        Touche logique
            /// @param scancode   Code physique USB HID
            /// @param mods       Modificateurs actifs
            /// @param nativeKey  Code natif de la plateforme
            /// @param extended   Touche étendue (bit E0 PS/2)
            /// @param windowId   Identifiant de la fenêtre
            NkKeyPressEvent(NkKey                  key,
                            NkScancode             scancode  = NkScancode::NK_SC_UNKNOWN,
                            const NkModifierState& mods      = {},
                            NkU32                  nativeKey = 0,
                            bool                   extended  = false,
                            NkU64                  windowId  = 0) noexcept
                : NkKeyboardEvent(key, scancode, mods, nativeKey, extended, windowId) {}

            NkEvent*    Clone()    const override { return new NkKeyPressEvent(*this); }
            NkString ToString() const override {
                return "KeyPress(" + NkString(NkKeyToString(mKey))
                    + " mod=" + mModifiers.ToString() + ")";
            }
    };

    // =========================================================================
    // NkKeyRepeatEvent — touche maintenue (auto-repeat généré par l'OS)
    // =========================================================================

    class NkKeyRepeatEvent final : public NkKeyboardEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_KEY_REPEATED)

            NkKeyRepeatEvent(NkKey                  key,
                            NkU32                  repeatCount = 1,
                            NkScancode             scancode    = NkScancode::NK_SC_UNKNOWN,
                            const NkModifierState& mods        = {},
                            NkU32                  nativeKey   = 0,
                            bool                   extended    = false,
                            NkU64                  windowId    = 0) noexcept
                : NkKeyboardEvent(key, scancode, mods, nativeKey, extended, windowId)
                , mRepeatCount(repeatCount)
            {}

            NkEvent*    Clone()    const override { return new NkKeyRepeatEvent(*this); }
            NkString ToString() const override {
                return NkString::Fmt("KeyRepeat({0} x{1})",
                    NkKeyToString(mKey), mRepeatCount);
            }

            /// @brief Nombre de fois que la touche a répété depuis le dernier événement
            NkU32 GetRepeatCount() const noexcept { return mRepeatCount; }

        private:
            NkU32 mRepeatCount = 1; ///< Nombre de répétitions
    };

    // =========================================================================
    // NkKeyReleaseEvent — touche relâchée
    // =========================================================================

    class NkKeyReleaseEvent final : public NkKeyboardEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_KEY_RELEASED)

            NkKeyReleaseEvent(NkKey                  key,
                            NkScancode             scancode  = NkScancode::NK_SC_UNKNOWN,
                            const NkModifierState& mods      = {},
                            NkU32                  nativeKey = 0,
                            bool                   extended  = false,
                            NkU64                  windowId  = 0) noexcept
                : NkKeyboardEvent(key, scancode, mods, nativeKey, extended, windowId) {}

            NkEvent*    Clone()    const override { return new NkKeyReleaseEvent(*this); }
            NkString ToString() const override {
                return "KeyRelease(" + NkString(NkKeyToString(mKey)) + ")";
            }
    };

    // =========================================================================
    // NkTextInputEvent — caractère Unicode produit (après traitement IME/layout)
    //
    // Catégorie : NK_CAT_KEYBOARD | NK_CAT_INPUT
    // Utilisation : saisie de texte dans les champs, éditeurs, etc.
    // Ne pas utiliser pour les raccourcis — utiliser NkKeyPressEvent à la place.
    // =========================================================================

    class NkTextInputEvent final : public NkEvent {
        public:
            NK_EVENT_TYPE_FLAGS(NK_TEXT_INPUT)

            /// @brief Catégorie : KEYBOARD + INPUT
            NkU32 GetCategoryFlags() const override {
                return static_cast<NkU32>(NkEventCategory::NK_CAT_KEYBOARD)
                    | static_cast<NkU32>(NkEventCategory::NK_CAT_INPUT);
            }

            /// @brief Construit depuis un codepoint Unicode UTF-32
            /// @param codepoint  Codepoint Unicode (U+0020 … U+10FFFF)
            explicit NkTextInputEvent(NkU32 codepoint, NkU64 windowId = 0) noexcept
                : NkEvent(windowId), mCodepoint(codepoint)
            {
                EncodeUtf8(codepoint);
            }

            NkEvent*    Clone()    const override { return new NkTextInputEvent(*this); }
            NkString ToString() const override {
                return NkString("TextInput(U+") + ToHex(mCodepoint) + " \"" + mUtf8 + "\")";
            }

            /// @brief Codepoint Unicode UTF-32
            NkU32       GetCodepoint() const noexcept { return mCodepoint; }
            /// @brief Représentation UTF-8 du codepoint (null-terminated, max 4 octets)
            const char* GetUtf8()      const noexcept { return mUtf8; }
            /// @brief Retourne true si le caractère est imprimable (≥ U+0020, ≠ U+007F)
            bool        IsPrintable()  const noexcept { return mCodepoint >= 0x20 && mCodepoint != 0x7F; }
            /// @brief Retourne true si le caractère est dans la plage ASCII (< 0x80)
            bool        IsAscii()      const noexcept { return mCodepoint < 0x80; }

        private:
            NkU32 mCodepoint = 0;   ///< Codepoint Unicode
            char  mUtf8[5]   = {};  ///< Représentation UTF-8 (max 4 octets + null)

            /// @brief Encode le codepoint en UTF-8 dans mUtf8
            void EncodeUtf8(NkU32 cp) noexcept {
                if (cp < 0x80) {
                    mUtf8[0] = static_cast<char>(cp);
                } else if (cp < 0x800) {
                    mUtf8[0] = static_cast<char>(0xC0 | (cp >> 6));
                    mUtf8[1] = static_cast<char>(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    mUtf8[0] = static_cast<char>(0xE0 | (cp >> 12));
                    mUtf8[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    mUtf8[2] = static_cast<char>(0x80 | (cp & 0x3F));
                } else {
                    mUtf8[0] = static_cast<char>(0xF0 | (cp >> 18));
                    mUtf8[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                    mUtf8[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                    mUtf8[3] = static_cast<char>(0x80 | (cp & 0x3F));
                }
            }

            /// @brief Convertit un entier en string hexadécimale (4 chiffres min)
            static NkString ToHex(NkU32 v) {
                char buf[9];
                snprintf(buf, sizeof(buf), "%04X", v);
                return buf;
            }
    };

} // namespace nkentseu