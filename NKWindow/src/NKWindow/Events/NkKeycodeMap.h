#pragma once

// =============================================================================
// NkKeycodeMap.h
// Conversion bidirectionnelle entre codes natifs de chaque plateforme
// et le NkKey cross-platform du framework.
//
// THEORIE : Scancode vs Keycode vs NkKey
// =======================================
//
// SCANCODE
//   Identifie une POSITION physique sur le clavier, independamment de la
//   disposition (layout). Base sur le standard USB HID Usage Table (p. 53).
//
// NkKey
//   Code semantique cross-platform du framework, base sur POSITION (US-QWERTY).
//   Equivalent conceptuellement au scancode USB HID.
//
// =============================================================================

#include "NkKeyboardEvent.h"

namespace nkentseu {

    // ===========================================================================
    // NkKeycodeMap - table de conversion cross-platform
    // ===========================================================================

    class NkKeycodeMap {
        public:
            // -----------------------------------------------------------------------
            // NkScancode <-> NkKey
            // -----------------------------------------------------------------------

            static NkKey      ScancodeToNkKey(NkScancode sc);
            static NkScancode NkKeyToScancode(NkKey key);

            // -----------------------------------------------------------------------
            // Win32 : Virtual Key (VK_*) <-> NkKey
            // -----------------------------------------------------------------------

            static NkKey NkKeyFromWin32VK(NkU32 vk, bool extended = false);
            static NkKey NkKeyFromWin32Scancode(NkU32 scancode, bool extended);

            // -----------------------------------------------------------------------
            // X11 (XLib et XCB) : KeySym <-> NkKey
            // -----------------------------------------------------------------------

            static NkKey NkKeyFromX11KeySym(NkU32 keysym);
            static NkKey NkKeyFromX11Keycode(NkU32 keycode); // evdev keycode = xkeycode - 8

            // -----------------------------------------------------------------------
            // macOS / iOS : Carbon keyCode <-> NkKey
            // -----------------------------------------------------------------------

            static NkKey NkKeyFromMacKeyCode(NkU16 keyCode);

            // -----------------------------------------------------------------------
            // Web / WASM : DOM key codes (KeyboardEvent.code) <-> NkKey
            // -----------------------------------------------------------------------

            static NkKey NkKeyFromDomCode(const char *domCode);

            // -----------------------------------------------------------------------
            // Android : AKEYCODE_* <-> NkKey
            // -----------------------------------------------------------------------

            static NkKey NkKeyFromAndroid(NkU32 aKeycode);

            // -----------------------------------------------------------------------
            // Utilitaires
            // -----------------------------------------------------------------------

            static NkKey Normalize(NkKey key) {
                switch (key) {
                case NkKey::NK_RSHIFT: return NkKey::NK_LSHIFT;
                case NkKey::NK_RCTRL:  return NkKey::NK_LCTRL;
                case NkKey::NK_RALT:   return NkKey::NK_LALT;
                case NkKey::NK_RSUPER: return NkKey::NK_LSUPER;
                default:               return key;
                }
            }

            static bool SameKey(NkKey a, NkKey b) { return Normalize(a) == Normalize(b); }
    };

    // ===========================================================================
    // NkKeycodeMap implementation - ScancodeToNkKey
    // ===========================================================================

    inline NkKey NkKeycodeMap::ScancodeToNkKey(NkScancode sc) {
        return NkScancodeToKey(sc);
    }

    inline NkScancode NkKeycodeMap::NkKeyToScancode(NkKey /*key*/) {
        return NkScancode::NK_SC_UNKNOWN; // not needed for event reception
    }

    // ===========================================================================
    // Win32 VK -> NkKey
    // ===========================================================================

    inline NkKey NkKeycodeMap::NkKeyFromWin32VK(NkU32 vk, bool extended) {
        switch (vk) {
        case 0x08: return NkKey::NK_BACK;
        case 0x09: return NkKey::NK_TAB;
        case 0x0C: return NkKey::NK_CLEAR;
        case 0x0D: return extended ? NkKey::NK_NUMPAD_ENTER : NkKey::NK_ENTER;
        case 0x10: return NkKey::NK_LSHIFT;
        case 0x11: return extended ? NkKey::NK_RCTRL : NkKey::NK_LCTRL;
        case 0x12: return extended ? NkKey::NK_RALT  : NkKey::NK_LALT;
        case 0x13: return NkKey::NK_PAUSE_BREAK;
        case 0x14: return NkKey::NK_CAPSLOCK;
        case 0x15: return NkKey::NK_KANA;
        case 0x17: return NkKey::NK_KANJI;
        case 0x1B: return NkKey::NK_ESCAPE;
        case 0x1C: return NkKey::NK_CONVERT;
        case 0x1D: return NkKey::NK_NONCONVERT;
        case 0x20: return NkKey::NK_SPACE;
        case 0x21: return NkKey::NK_PAGE_UP;
        case 0x22: return NkKey::NK_PAGE_DOWN;
        case 0x23: return NkKey::NK_END;
        case 0x24: return NkKey::NK_HOME;
        case 0x25: return NkKey::NK_LEFT;
        case 0x26: return NkKey::NK_UP;
        case 0x27: return NkKey::NK_RIGHT;
        case 0x28: return NkKey::NK_DOWN;
        case 0x2C: return NkKey::NK_PRINT_SCREEN;
        case 0x2D: return extended ? NkKey::NK_INSERT    : NkKey::NK_NUMPAD_0;
        case 0x2E: return extended ? NkKey::NK_DELETE     : NkKey::NK_NUMPAD_DOT;
        case 0x30: return NkKey::NK_NUM0; case 0x31: return NkKey::NK_NUM1;
        case 0x32: return NkKey::NK_NUM2; case 0x33: return NkKey::NK_NUM3;
        case 0x34: return NkKey::NK_NUM4; case 0x35: return NkKey::NK_NUM5;
        case 0x36: return NkKey::NK_NUM6; case 0x37: return NkKey::NK_NUM7;
        case 0x38: return NkKey::NK_NUM8; case 0x39: return NkKey::NK_NUM9;
        case 0x41: return NkKey::NK_A; case 0x42: return NkKey::NK_B;
        case 0x43: return NkKey::NK_C; case 0x44: return NkKey::NK_D;
        case 0x45: return NkKey::NK_E; case 0x46: return NkKey::NK_F;
        case 0x47: return NkKey::NK_G; case 0x48: return NkKey::NK_H;
        case 0x49: return NkKey::NK_I; case 0x4A: return NkKey::NK_J;
        case 0x4B: return NkKey::NK_K; case 0x4C: return NkKey::NK_L;
        case 0x4D: return NkKey::NK_M; case 0x4E: return NkKey::NK_N;
        case 0x4F: return NkKey::NK_O; case 0x50: return NkKey::NK_P;
        case 0x51: return NkKey::NK_Q; case 0x52: return NkKey::NK_R;
        case 0x53: return NkKey::NK_S; case 0x54: return NkKey::NK_T;
        case 0x55: return NkKey::NK_U; case 0x56: return NkKey::NK_V;
        case 0x57: return NkKey::NK_W; case 0x58: return NkKey::NK_X;
        case 0x59: return NkKey::NK_Y; case 0x5A: return NkKey::NK_Z;
        case 0x5B: return NkKey::NK_LSUPER;
        case 0x5C: return NkKey::NK_RSUPER;
        case 0x5D: return NkKey::NK_MENU;
        case 0x5F: return NkKey::NK_SLEEP;
        case 0x60: return NkKey::NK_NUMPAD_0; case 0x61: return NkKey::NK_NUMPAD_1;
        case 0x62: return NkKey::NK_NUMPAD_2; case 0x63: return NkKey::NK_NUMPAD_3;
        case 0x64: return NkKey::NK_NUMPAD_4; case 0x65: return NkKey::NK_NUMPAD_5;
        case 0x66: return NkKey::NK_NUMPAD_6; case 0x67: return NkKey::NK_NUMPAD_7;
        case 0x68: return NkKey::NK_NUMPAD_8; case 0x69: return NkKey::NK_NUMPAD_9;
        case 0x6A: return NkKey::NK_NUMPAD_MUL;
        case 0x6B: return NkKey::NK_NUMPAD_ADD;
        case 0x6C: return NkKey::NK_SEPARATOR;
        case 0x6D: return NkKey::NK_NUMPAD_SUB;
        case 0x6E: return NkKey::NK_NUMPAD_DOT;
        case 0x6F: return NkKey::NK_NUMPAD_DIV;
        case 0x70: return NkKey::NK_F1;  case 0x71: return NkKey::NK_F2;
        case 0x72: return NkKey::NK_F3;  case 0x73: return NkKey::NK_F4;
        case 0x74: return NkKey::NK_F5;  case 0x75: return NkKey::NK_F6;
        case 0x76: return NkKey::NK_F7;  case 0x77: return NkKey::NK_F8;
        case 0x78: return NkKey::NK_F9;  case 0x79: return NkKey::NK_F10;
        case 0x7A: return NkKey::NK_F11; case 0x7B: return NkKey::NK_F12;
        case 0x7C: return NkKey::NK_F13; case 0x7D: return NkKey::NK_F14;
        case 0x7E: return NkKey::NK_F15; case 0x7F: return NkKey::NK_F16;
        case 0x80: return NkKey::NK_F17; case 0x81: return NkKey::NK_F18;
        case 0x82: return NkKey::NK_F19; case 0x83: return NkKey::NK_F20;
        case 0x84: return NkKey::NK_F21; case 0x85: return NkKey::NK_F22;
        case 0x86: return NkKey::NK_F23; case 0x87: return NkKey::NK_F24;
        case 0x90: return NkKey::NK_NUM_LOCK;
        case 0x91: return NkKey::NK_SCROLL_LOCK;
        case 0xA0: return NkKey::NK_LSHIFT;
        case 0xA1: return NkKey::NK_RSHIFT;
        case 0xA2: return NkKey::NK_LCTRL;
        case 0xA3: return NkKey::NK_RCTRL;
        case 0xA4: return NkKey::NK_LALT;
        case 0xA5: return NkKey::NK_RALT;
        case 0xA6: return NkKey::NK_BROWSER_BACK;
        case 0xA7: return NkKey::NK_BROWSER_FORWARD;
        case 0xA8: return NkKey::NK_BROWSER_REFRESH;
        case 0xAA: return NkKey::NK_BROWSER_SEARCH;
        case 0xAB: return NkKey::NK_BROWSER_FAVORITES;
        case 0xAC: return NkKey::NK_BROWSER_HOME;
        case 0xAD: return NkKey::NK_MEDIA_MUTE;
        case 0xAE: return NkKey::NK_MEDIA_VOLUME_DOWN;
        case 0xAF: return NkKey::NK_MEDIA_VOLUME_UP;
        case 0xB0: return NkKey::NK_MEDIA_NEXT;
        case 0xB1: return NkKey::NK_MEDIA_PREV;
        case 0xB2: return NkKey::NK_MEDIA_STOP;
        case 0xB3: return NkKey::NK_MEDIA_PLAY_PAUSE;
        case 0xBA: return NkKey::NK_SEMICOLON;
        case 0xBB: return NkKey::NK_EQUALS;
        case 0xBC: return NkKey::NK_COMMA;
        case 0xBD: return NkKey::NK_MINUS;
        case 0xBE: return NkKey::NK_PERIOD;
        case 0xBF: return NkKey::NK_SLASH;
        case 0xC0: return NkKey::NK_GRAVE;
        case 0xDB: return NkKey::NK_LBRACKET;
        case 0xDC: return NkKey::NK_BACKSLASH;
        case 0xDD: return NkKey::NK_RBRACKET;
        case 0xDE: return NkKey::NK_APOSTROPHE;
        case 0x19: return NkKey::NK_KANJI;
        case 0xF2: return NkKey::NK_HANGUL;
        case 0xF1: return NkKey::NK_HANJA;
        default:   return NkKey::NK_UNKNOWN;
        }
    }

    // ===========================================================================
    // Win32 Scancode PS/2 -> NkKey
    // ===========================================================================

    inline NkKey NkKeycodeMap::NkKeyFromWin32Scancode(NkU32 sc, bool extended) {
        return NkScancodeToKey(NkScancodeFromWin32(sc, extended));
    }

    // ===========================================================================
    // X11 KeySym -> NkKey
    // ===========================================================================

    inline NkKey NkKeycodeMap::NkKeyFromX11KeySym(NkU32 ks) {
        // Lettres minuscules/majuscules -> meme NkKey (position)
        if (ks >= 0x61 && ks <= 0x7A)
            return static_cast<NkKey>(static_cast<NkU32>(NkKey::NK_A) + (ks - 0x61));
        if (ks >= 0x41 && ks <= 0x5A)
            return static_cast<NkKey>(static_cast<NkU32>(NkKey::NK_A) + (ks - 0x41));

        switch (ks) {
        case 0x30: return NkKey::NK_NUM0; case 0x31: return NkKey::NK_NUM1;
        case 0x32: return NkKey::NK_NUM2; case 0x33: return NkKey::NK_NUM3;
        case 0x34: return NkKey::NK_NUM4; case 0x35: return NkKey::NK_NUM5;
        case 0x36: return NkKey::NK_NUM6; case 0x37: return NkKey::NK_NUM7;
        case 0x38: return NkKey::NK_NUM8; case 0x39: return NkKey::NK_NUM9;
        case 0xFF08: return NkKey::NK_BACK;
        case 0xFF09: return NkKey::NK_TAB;
        case 0xFF0D: return NkKey::NK_ENTER;
        case 0xFF1B: return NkKey::NK_ESCAPE;
        case 0x0020: return NkKey::NK_SPACE;
        case 0xFF13: return NkKey::NK_PAUSE_BREAK;
        case 0xFF14: return NkKey::NK_SCROLL_LOCK;
        case 0xFF15: return NkKey::NK_PRINT_SCREEN;
        case 0xFF50: return NkKey::NK_HOME;
        case 0xFF51: return NkKey::NK_LEFT;
        case 0xFF52: return NkKey::NK_UP;
        case 0xFF53: return NkKey::NK_RIGHT;
        case 0xFF54: return NkKey::NK_DOWN;
        case 0xFF55: return NkKey::NK_PAGE_UP;
        case 0xFF56: return NkKey::NK_PAGE_DOWN;
        case 0xFF57: return NkKey::NK_END;
        case 0xFF60: return NkKey::NK_MENU;
        case 0xFF61: return NkKey::NK_PRINT_SCREEN;
        case 0xFF63: return NkKey::NK_INSERT;
        case 0xFFFF: return NkKey::NK_DELETE;
        case 0xFFBE: return NkKey::NK_F1;  case 0xFFBF: return NkKey::NK_F2;
        case 0xFFC0: return NkKey::NK_F3;  case 0xFFC1: return NkKey::NK_F4;
        case 0xFFC2: return NkKey::NK_F5;  case 0xFFC3: return NkKey::NK_F6;
        case 0xFFC4: return NkKey::NK_F7;  case 0xFFC5: return NkKey::NK_F8;
        case 0xFFC6: return NkKey::NK_F9;  case 0xFFC7: return NkKey::NK_F10;
        case 0xFFC8: return NkKey::NK_F11; case 0xFFC9: return NkKey::NK_F12;
        case 0xFFCA: return NkKey::NK_F13; case 0xFFCB: return NkKey::NK_F14;
        case 0xFFCC: return NkKey::NK_F15; case 0xFFCD: return NkKey::NK_F16;
        case 0xFFCE: return NkKey::NK_F17; case 0xFFCF: return NkKey::NK_F18;
        case 0xFFD0: return NkKey::NK_F19; case 0xFFD1: return NkKey::NK_F20;
        case 0xFFD2: return NkKey::NK_F21; case 0xFFD3: return NkKey::NK_F22;
        case 0xFFD4: return NkKey::NK_F23; case 0xFFD5: return NkKey::NK_F24;
        case 0xFF7F: return NkKey::NK_NUM_LOCK;
        case 0xFFAA: return NkKey::NK_NUMPAD_MUL;
        case 0xFFAB: return NkKey::NK_NUMPAD_ADD;
        case 0xFFAC: return NkKey::NK_SEPARATOR;
        case 0xFFAD: return NkKey::NK_NUMPAD_SUB;
        case 0xFFAE: return NkKey::NK_NUMPAD_DOT;
        case 0xFFAF: return NkKey::NK_NUMPAD_DIV;
        case 0xFFB0: return NkKey::NK_NUMPAD_0; case 0xFFB1: return NkKey::NK_NUMPAD_1;
        case 0xFFB2: return NkKey::NK_NUMPAD_2; case 0xFFB3: return NkKey::NK_NUMPAD_3;
        case 0xFFB4: return NkKey::NK_NUMPAD_4; case 0xFFB5: return NkKey::NK_NUMPAD_5;
        case 0xFFB6: return NkKey::NK_NUMPAD_6; case 0xFFB7: return NkKey::NK_NUMPAD_7;
        case 0xFFB8: return NkKey::NK_NUMPAD_8; case 0xFFB9: return NkKey::NK_NUMPAD_9;
        case 0xFF8D: return NkKey::NK_NUMPAD_ENTER;
        case 0xFFBD: return NkKey::NK_NUMPAD_EQUALS;
        case 0xFFE1: return NkKey::NK_LSHIFT;
        case 0xFFE2: return NkKey::NK_RSHIFT;
        case 0xFFE3: return NkKey::NK_LCTRL;
        case 0xFFE4: return NkKey::NK_RCTRL;
        case 0xFFE9: return NkKey::NK_LALT;
        case 0xFFEA: return NkKey::NK_RALT;
        case 0xFFEB: return NkKey::NK_LSUPER;
        case 0xFFEC: return NkKey::NK_RSUPER;
        case 0xFFED: return NkKey::NK_LSUPER;
        case 0xFFEE: return NkKey::NK_RSUPER;
        case 0xFFE5: return NkKey::NK_CAPSLOCK;
        case 0x0060: return NkKey::NK_GRAVE;
        case 0x002D: return NkKey::NK_MINUS;
        case 0x003D: return NkKey::NK_EQUALS;
        case 0x005B: return NkKey::NK_LBRACKET;
        case 0x005D: return NkKey::NK_RBRACKET;
        case 0x005C: return NkKey::NK_BACKSLASH;
        case 0x003B: return NkKey::NK_SEMICOLON;
        case 0x0027: return NkKey::NK_APOSTROPHE;
        case 0x002C: return NkKey::NK_COMMA;
        case 0x002E: return NkKey::NK_PERIOD;
        case 0x002F: return NkKey::NK_SLASH;
        case 0x1008FF14: return NkKey::NK_MEDIA_PLAY_PAUSE;
        case 0x1008FF15: return NkKey::NK_MEDIA_STOP;
        case 0x1008FF16: return NkKey::NK_MEDIA_PREV;
        case 0x1008FF17: return NkKey::NK_MEDIA_NEXT;
        case 0x1008FF12: return NkKey::NK_MEDIA_MUTE;
        case 0x1008FF13: return NkKey::NK_MEDIA_VOLUME_UP;
        case 0x1008FF11: return NkKey::NK_MEDIA_VOLUME_DOWN;
        default: return NkKey::NK_UNKNOWN;
        }
    }

    // Evdev keycode = X11 keycode - 8
    inline NkKey NkKeycodeMap::NkKeyFromX11Keycode(NkU32 keycode) {
        return NkScancodeToKey(NkScancodeFromXKeycode(keycode));
    }

    // ===========================================================================
    // macOS Carbon keyCode -> NkKey
    // ===========================================================================

    inline NkKey NkKeycodeMap::NkKeyFromMacKeyCode(NkU16 kc) {
        switch (kc) {
        case 0x00: return NkKey::NK_A;
        case 0x01: return NkKey::NK_S;
        case 0x02: return NkKey::NK_D;
        case 0x03: return NkKey::NK_F;
        case 0x04: return NkKey::NK_H;
        case 0x05: return NkKey::NK_G;
        case 0x06: return NkKey::NK_Z;
        case 0x07: return NkKey::NK_X;
        case 0x08: return NkKey::NK_C;
        case 0x09: return NkKey::NK_V;
        case 0x0B: return NkKey::NK_B;
        case 0x0C: return NkKey::NK_Q;
        case 0x0D: return NkKey::NK_W;
        case 0x0E: return NkKey::NK_E;
        case 0x0F: return NkKey::NK_R;
        case 0x10: return NkKey::NK_Y;
        case 0x11: return NkKey::NK_T;
        case 0x12: return NkKey::NK_NUM1;
        case 0x13: return NkKey::NK_NUM2;
        case 0x14: return NkKey::NK_NUM3;
        case 0x15: return NkKey::NK_NUM4;
        case 0x16: return NkKey::NK_NUM6;
        case 0x17: return NkKey::NK_NUM5;
        case 0x18: return NkKey::NK_EQUALS;
        case 0x19: return NkKey::NK_NUM9;
        case 0x1A: return NkKey::NK_NUM7;
        case 0x1B: return NkKey::NK_MINUS;
        case 0x1C: return NkKey::NK_NUM8;
        case 0x1D: return NkKey::NK_NUM0;
        case 0x1E: return NkKey::NK_RBRACKET;
        case 0x1F: return NkKey::NK_O;
        case 0x20: return NkKey::NK_U;
        case 0x21: return NkKey::NK_LBRACKET;
        case 0x22: return NkKey::NK_I;
        case 0x23: return NkKey::NK_P;
        case 0x24: return NkKey::NK_ENTER;
        case 0x25: return NkKey::NK_L;
        case 0x26: return NkKey::NK_J;
        case 0x27: return NkKey::NK_APOSTROPHE;
        case 0x28: return NkKey::NK_K;
        case 0x29: return NkKey::NK_SEMICOLON;
        case 0x2A: return NkKey::NK_BACKSLASH;
        case 0x2B: return NkKey::NK_COMMA;
        case 0x2C: return NkKey::NK_SLASH;
        case 0x2D: return NkKey::NK_N;
        case 0x2E: return NkKey::NK_M;
        case 0x2F: return NkKey::NK_PERIOD;
        case 0x30: return NkKey::NK_TAB;
        case 0x31: return NkKey::NK_SPACE;
        case 0x32: return NkKey::NK_GRAVE;
        case 0x33: return NkKey::NK_BACK;
        case 0x35: return NkKey::NK_ESCAPE;
        case 0x37: return NkKey::NK_LSUPER;
        case 0x38: return NkKey::NK_LSHIFT;
        case 0x39: return NkKey::NK_CAPSLOCK;
        case 0x3A: return NkKey::NK_LALT;
        case 0x3B: return NkKey::NK_LCTRL;
        case 0x3C: return NkKey::NK_RSHIFT;
        case 0x3D: return NkKey::NK_RALT;
        case 0x3E: return NkKey::NK_RCTRL;
        case 0x3F: return NkKey::NK_LSUPER;
        case 0x40: return NkKey::NK_F17;
        case 0x41: return NkKey::NK_NUMPAD_DOT;
        case 0x43: return NkKey::NK_NUMPAD_MUL;
        case 0x45: return NkKey::NK_NUMPAD_ADD;
        case 0x47: return NkKey::NK_NUM_LOCK;
        case 0x4B: return NkKey::NK_NUMPAD_DIV;
        case 0x4C: return NkKey::NK_NUMPAD_ENTER;
        case 0x4E: return NkKey::NK_NUMPAD_SUB;
        case 0x4F: return NkKey::NK_F18;
        case 0x50: return NkKey::NK_F19;
        case 0x51: return NkKey::NK_NUMPAD_EQUALS;
        case 0x52: return NkKey::NK_NUMPAD_0;
        case 0x53: return NkKey::NK_NUMPAD_1;
        case 0x54: return NkKey::NK_NUMPAD_2;
        case 0x55: return NkKey::NK_NUMPAD_3;
        case 0x56: return NkKey::NK_NUMPAD_4;
        case 0x57: return NkKey::NK_NUMPAD_5;
        case 0x58: return NkKey::NK_NUMPAD_6;
        case 0x59: return NkKey::NK_NUMPAD_7;
        case 0x5A: return NkKey::NK_F20;
        case 0x5B: return NkKey::NK_NUMPAD_8;
        case 0x5C: return NkKey::NK_NUMPAD_9;
        case 0x60: return NkKey::NK_F5;
        case 0x61: return NkKey::NK_F6;
        case 0x62: return NkKey::NK_F7;
        case 0x63: return NkKey::NK_F3;
        case 0x64: return NkKey::NK_F8;
        case 0x65: return NkKey::NK_F9;
        case 0x67: return NkKey::NK_F11;
        case 0x69: return NkKey::NK_F13;
        case 0x6A: return NkKey::NK_F16;
        case 0x6B: return NkKey::NK_F14;
        case 0x6D: return NkKey::NK_F10;
        case 0x6F: return NkKey::NK_F12;
        case 0x71: return NkKey::NK_F15;
        case 0x72: return NkKey::NK_INSERT;
        case 0x73: return NkKey::NK_HOME;
        case 0x74: return NkKey::NK_PAGE_UP;
        case 0x75: return NkKey::NK_DELETE;
        case 0x76: return NkKey::NK_F4;
        case 0x77: return NkKey::NK_END;
        case 0x78: return NkKey::NK_F2;
        case 0x79: return NkKey::NK_PAGE_DOWN;
        case 0x7A: return NkKey::NK_F1;
        case 0x7B: return NkKey::NK_LEFT;
        case 0x7C: return NkKey::NK_RIGHT;
        case 0x7D: return NkKey::NK_DOWN;
        case 0x7E: return NkKey::NK_UP;
        default:   return NkKey::NK_UNKNOWN;
        }
    }

    // ===========================================================================
    // DOM KeyboardEvent.code -> NkKey (Web / WASM)
    // ===========================================================================

    inline NkKey NkKeycodeMap::NkKeyFromDomCode(const char *code) {
        if (!code) return NkKey::NK_UNKNOWN;
        struct { const char *code; NkKey key; } table[] = {
            {"Backquote",          NkKey::NK_GRAVE},
            {"Digit1",             NkKey::NK_NUM1},
            {"Digit2",             NkKey::NK_NUM2},
            {"Digit3",             NkKey::NK_NUM3},
            {"Digit4",             NkKey::NK_NUM4},
            {"Digit5",             NkKey::NK_NUM5},
            {"Digit6",             NkKey::NK_NUM6},
            {"Digit7",             NkKey::NK_NUM7},
            {"Digit8",             NkKey::NK_NUM8},
            {"Digit9",             NkKey::NK_NUM9},
            {"Digit0",             NkKey::NK_NUM0},
            {"Minus",              NkKey::NK_MINUS},
            {"Equal",              NkKey::NK_EQUALS},
            {"Backspace",          NkKey::NK_BACK},
            {"Tab",                NkKey::NK_TAB},
            {"KeyQ",  NkKey::NK_Q}, {"KeyW", NkKey::NK_W},
            {"KeyE",  NkKey::NK_E}, {"KeyR", NkKey::NK_R},
            {"KeyT",  NkKey::NK_T}, {"KeyY", NkKey::NK_Y},
            {"KeyU",  NkKey::NK_U}, {"KeyI", NkKey::NK_I},
            {"KeyO",  NkKey::NK_O}, {"KeyP", NkKey::NK_P},
            {"BracketLeft",        NkKey::NK_LBRACKET},
            {"BracketRight",       NkKey::NK_RBRACKET},
            {"Backslash",          NkKey::NK_BACKSLASH},
            {"CapsLock",           NkKey::NK_CAPSLOCK},
            {"KeyA",  NkKey::NK_A}, {"KeyS", NkKey::NK_S},
            {"KeyD",  NkKey::NK_D}, {"KeyF", NkKey::NK_F},
            {"KeyG",  NkKey::NK_G}, {"KeyH", NkKey::NK_H},
            {"KeyJ",  NkKey::NK_J}, {"KeyK", NkKey::NK_K},
            {"KeyL",  NkKey::NK_L},
            {"Semicolon",          NkKey::NK_SEMICOLON},
            {"Quote",              NkKey::NK_APOSTROPHE},
            {"Enter",              NkKey::NK_ENTER},
            {"ShiftLeft",          NkKey::NK_LSHIFT},
            {"KeyZ",  NkKey::NK_Z}, {"KeyX", NkKey::NK_X},
            {"KeyC",  NkKey::NK_C}, {"KeyV", NkKey::NK_V},
            {"KeyB",  NkKey::NK_B}, {"KeyN", NkKey::NK_N},
            {"KeyM",  NkKey::NK_M},
            {"Comma",              NkKey::NK_COMMA},
            {"Period",             NkKey::NK_PERIOD},
            {"Slash",              NkKey::NK_SLASH},
            {"ShiftRight",         NkKey::NK_RSHIFT},
            {"ControlLeft",        NkKey::NK_LCTRL},
            {"MetaLeft",           NkKey::NK_LSUPER},
            {"AltLeft",            NkKey::NK_LALT},
            {"Space",              NkKey::NK_SPACE},
            {"AltRight",           NkKey::NK_RALT},
            {"MetaRight",          NkKey::NK_RSUPER},
            {"ContextMenu",        NkKey::NK_MENU},
            {"ControlRight",       NkKey::NK_RCTRL},
            {"PrintScreen",        NkKey::NK_PRINT_SCREEN},
            {"ScrollLock",         NkKey::NK_SCROLL_LOCK},
            {"Pause",              NkKey::NK_PAUSE_BREAK},
            {"Insert",             NkKey::NK_INSERT},
            {"Home",               NkKey::NK_HOME},
            {"PageUp",             NkKey::NK_PAGE_UP},
            {"Delete",             NkKey::NK_DELETE},
            {"End",                NkKey::NK_END},
            {"PageDown",           NkKey::NK_PAGE_DOWN},
            {"ArrowRight",         NkKey::NK_RIGHT},
            {"ArrowLeft",          NkKey::NK_LEFT},
            {"ArrowDown",          NkKey::NK_DOWN},
            {"ArrowUp",            NkKey::NK_UP},
            {"Escape",             NkKey::NK_ESCAPE},
            {"F1",  NkKey::NK_F1},  {"F2",  NkKey::NK_F2},
            {"F3",  NkKey::NK_F3},  {"F4",  NkKey::NK_F4},
            {"F5",  NkKey::NK_F5},  {"F6",  NkKey::NK_F6},
            {"F7",  NkKey::NK_F7},  {"F8",  NkKey::NK_F8},
            {"F9",  NkKey::NK_F9},  {"F10", NkKey::NK_F10},
            {"F11", NkKey::NK_F11}, {"F12", NkKey::NK_F12},
            {"NumLock",            NkKey::NK_NUM_LOCK},
            {"NumpadDivide",       NkKey::NK_NUMPAD_DIV},
            {"NumpadMultiply",     NkKey::NK_NUMPAD_MUL},
            {"NumpadSubtract",     NkKey::NK_NUMPAD_SUB},
            {"NumpadAdd",          NkKey::NK_NUMPAD_ADD},
            {"NumpadEnter",        NkKey::NK_NUMPAD_ENTER},
            {"NumpadDecimal",      NkKey::NK_NUMPAD_DOT},
            {"Numpad0",            NkKey::NK_NUMPAD_0},
            {"Numpad1",            NkKey::NK_NUMPAD_1},
            {"Numpad2",            NkKey::NK_NUMPAD_2},
            {"Numpad3",            NkKey::NK_NUMPAD_3},
            {"Numpad4",            NkKey::NK_NUMPAD_4},
            {"Numpad5",            NkKey::NK_NUMPAD_5},
            {"Numpad6",            NkKey::NK_NUMPAD_6},
            {"Numpad7",            NkKey::NK_NUMPAD_7},
            {"Numpad8",            NkKey::NK_NUMPAD_8},
            {"Numpad9",            NkKey::NK_NUMPAD_9},
            {"NumpadEqual",        NkKey::NK_NUMPAD_EQUALS},
            {"MediaPlayPause",     NkKey::NK_MEDIA_PLAY_PAUSE},
            {"MediaStop",          NkKey::NK_MEDIA_STOP},
            {"MediaTrackNext",     NkKey::NK_MEDIA_NEXT},
            {"MediaTrackPrevious", NkKey::NK_MEDIA_PREV},
            {"AudioVolumeMute",    NkKey::NK_MEDIA_MUTE},
            {"AudioVolumeUp",      NkKey::NK_MEDIA_VOLUME_UP},
            {"AudioVolumeDown",    NkKey::NK_MEDIA_VOLUME_DOWN},
            {"BrowserBack",        NkKey::NK_BROWSER_BACK},
            {"BrowserForward",     NkKey::NK_BROWSER_FORWARD},
            {"BrowserRefresh",     NkKey::NK_BROWSER_REFRESH},
            {"BrowserHome",        NkKey::NK_BROWSER_HOME},
            {"BrowserSearch",      NkKey::NK_BROWSER_SEARCH},
            {"BrowserFavorites",   NkKey::NK_BROWSER_FAVORITES},
            {nullptr,              NkKey::NK_UNKNOWN}
        };
        for (auto *e = table; e->code; ++e)
            if (strcmp(code, e->code) == 0) return e->key;
        return NkKey::NK_UNKNOWN;
    }

    // ===========================================================================
    // Android AKEYCODE -> NkKey
    // ===========================================================================

    inline NkKey NkKeycodeMap::NkKeyFromAndroid(NkU32 kc) {
        switch (kc) {
        case 4:   return NkKey::NK_BACK;
        case 7:   return NkKey::NK_NUM0;
        case 8:   return NkKey::NK_NUM1;
        case 9:   return NkKey::NK_NUM2;
        case 10:  return NkKey::NK_NUM3;
        case 11:  return NkKey::NK_NUM4;
        case 12:  return NkKey::NK_NUM5;
        case 13:  return NkKey::NK_NUM6;
        case 14:  return NkKey::NK_NUM7;
        case 15:  return NkKey::NK_NUM8;
        case 16:  return NkKey::NK_NUM9;
        case 17:  return NkKey::NK_NUMPAD_MUL;
        case 19:  return NkKey::NK_UP;
        case 20:  return NkKey::NK_DOWN;
        case 21:  return NkKey::NK_LEFT;
        case 22:  return NkKey::NK_RIGHT;
        case 23:  return NkKey::NK_ENTER;
        case 29:  return NkKey::NK_A;  case 30:  return NkKey::NK_B;
        case 31:  return NkKey::NK_C;  case 32:  return NkKey::NK_D;
        case 33:  return NkKey::NK_E;  case 34:  return NkKey::NK_F;
        case 35:  return NkKey::NK_G;  case 36:  return NkKey::NK_H;
        case 37:  return NkKey::NK_I;  case 38:  return NkKey::NK_J;
        case 39:  return NkKey::NK_K;  case 40:  return NkKey::NK_L;
        case 41:  return NkKey::NK_M;  case 42:  return NkKey::NK_N;
        case 43:  return NkKey::NK_O;  case 44:  return NkKey::NK_P;
        case 45:  return NkKey::NK_Q;  case 46:  return NkKey::NK_R;
        case 47:  return NkKey::NK_S;  case 48:  return NkKey::NK_T;
        case 49:  return NkKey::NK_U;  case 50:  return NkKey::NK_V;
        case 51:  return NkKey::NK_W;  case 52:  return NkKey::NK_X;
        case 53:  return NkKey::NK_Y;  case 54:  return NkKey::NK_Z;
        case 55:  return NkKey::NK_COMMA;
        case 56:  return NkKey::NK_PERIOD;
        case 57:  return NkKey::NK_LALT;
        case 58:  return NkKey::NK_RALT;
        case 59:  return NkKey::NK_LSHIFT;
        case 60:  return NkKey::NK_RSHIFT;
        case 61:  return NkKey::NK_TAB;
        case 62:  return NkKey::NK_SPACE;
        case 66:  return NkKey::NK_ENTER;
        case 67:  return NkKey::NK_BACK;
        case 68:  return NkKey::NK_GRAVE;
        case 69:  return NkKey::NK_MINUS;
        case 70:  return NkKey::NK_EQUALS;
        case 71:  return NkKey::NK_LBRACKET;
        case 72:  return NkKey::NK_RBRACKET;
        case 73:  return NkKey::NK_BACKSLASH;
        case 74:  return NkKey::NK_SEMICOLON;
        case 75:  return NkKey::NK_APOSTROPHE;
        case 76:  return NkKey::NK_SLASH;
        case 82:  return NkKey::NK_MENU;
        case 85:  return NkKey::NK_MEDIA_PLAY_PAUSE;
        case 86:  return NkKey::NK_MEDIA_STOP;
        case 87:  return NkKey::NK_MEDIA_NEXT;
        case 88:  return NkKey::NK_MEDIA_PREV;
        case 91:  return NkKey::NK_MEDIA_MUTE;
        case 92:  return NkKey::NK_PAGE_UP;
        case 93:  return NkKey::NK_PAGE_DOWN;
        case 111: return NkKey::NK_ESCAPE;
        case 112: return NkKey::NK_DELETE;
        case 113: return NkKey::NK_LCTRL;
        case 114: return NkKey::NK_RCTRL;
        case 115: return NkKey::NK_CAPSLOCK;
        case 116: return NkKey::NK_SCROLL_LOCK;
        case 117: return NkKey::NK_LSUPER;
        case 118: return NkKey::NK_RSUPER;
        case 120: return NkKey::NK_PRINT_SCREEN;
        case 121: return NkKey::NK_PAUSE_BREAK;
        case 122: return NkKey::NK_HOME;
        case 123: return NkKey::NK_END;
        case 124: return NkKey::NK_INSERT;
        case 131: return NkKey::NK_F1;  case 132: return NkKey::NK_F2;
        case 133: return NkKey::NK_F3;  case 134: return NkKey::NK_F4;
        case 135: return NkKey::NK_F5;  case 136: return NkKey::NK_F6;
        case 137: return NkKey::NK_F7;  case 138: return NkKey::NK_F8;
        case 139: return NkKey::NK_F9;  case 140: return NkKey::NK_F10;
        case 141: return NkKey::NK_F11; case 142: return NkKey::NK_F12;
        case 143: return NkKey::NK_NUM_LOCK;
        case 144: return NkKey::NK_NUMPAD_0;
        case 145: return NkKey::NK_NUMPAD_1;
        case 146: return NkKey::NK_NUMPAD_2;
        case 147: return NkKey::NK_NUMPAD_3;
        case 148: return NkKey::NK_NUMPAD_4;
        case 149: return NkKey::NK_NUMPAD_5;
        case 150: return NkKey::NK_NUMPAD_6;
        case 151: return NkKey::NK_NUMPAD_7;
        case 152: return NkKey::NK_NUMPAD_8;
        case 153: return NkKey::NK_NUMPAD_9;
        case 154: return NkKey::NK_NUMPAD_DIV;
        case 155: return NkKey::NK_NUMPAD_MUL;
        case 156: return NkKey::NK_NUMPAD_SUB;
        case 157: return NkKey::NK_NUMPAD_ADD;
        case 158: return NkKey::NK_NUMPAD_DOT;
        case 160: return NkKey::NK_NUMPAD_ENTER;
        case 164: return NkKey::NK_MEDIA_VOLUME_UP;
        case 165: return NkKey::NK_MEDIA_VOLUME_DOWN;
        case 220: return NkKey::NK_MEDIA_MUTE;
        default:  return NkKey::NK_UNKNOWN;
        }
    }

} // namespace nkentseu
