// =============================================================================
// NkKeyboardEvent.cpp
// Implémentation des utilitaires clavier :
//   - NkKeyToString, NkKeyIsModifier, NkKeyIsNumpad, NkKeyIsFunctionKey
//   - NkScancodeToString, NkScancodeToKey
//   - NkScancodeFromWin32, NkScancodeFromLinux, NkScancodeFromXKeycode
//   - NkScancodeFromMac, NkScancodeFromDOMCode
// =============================================================================

#include "NkKeyboardEvent.h"
#include <cstring>

namespace nkentseu {

    // =========================================================================
    // NkKeyToString
    // =========================================================================

    const char* NkKeyToString(NkKey key) noexcept {
        switch (key) {
            case NkKey::NK_UNKNOWN:          return "NK_UNKNOWN";
            case NkKey::NK_ESCAPE:           return "NK_ESCAPE";
            case NkKey::NK_F1:               return "NK_F1";
            case NkKey::NK_F2:               return "NK_F2";
            case NkKey::NK_F3:               return "NK_F3";
            case NkKey::NK_F4:               return "NK_F4";
            case NkKey::NK_F5:               return "NK_F5";
            case NkKey::NK_F6:               return "NK_F6";
            case NkKey::NK_F7:               return "NK_F7";
            case NkKey::NK_F8:               return "NK_F8";
            case NkKey::NK_F9:               return "NK_F9";
            case NkKey::NK_F10:              return "NK_F10";
            case NkKey::NK_F11:              return "NK_F11";
            case NkKey::NK_F12:              return "NK_F12";
            case NkKey::NK_F13:              return "NK_F13";
            case NkKey::NK_F14:              return "NK_F14";
            case NkKey::NK_F15:              return "NK_F15";
            case NkKey::NK_F16:              return "NK_F16";
            case NkKey::NK_F17:              return "NK_F17";
            case NkKey::NK_F18:              return "NK_F18";
            case NkKey::NK_F19:              return "NK_F19";
            case NkKey::NK_F20:              return "NK_F20";
            case NkKey::NK_F21:              return "NK_F21";
            case NkKey::NK_F22:              return "NK_F22";
            case NkKey::NK_F23:              return "NK_F23";
            case NkKey::NK_F24:              return "NK_F24";
            case NkKey::NK_GRAVE:            return "NK_GRAVE";
            case NkKey::NK_NUM1:             return "NK_NUM1";
            case NkKey::NK_NUM2:             return "NK_NUM2";
            case NkKey::NK_NUM3:             return "NK_NUM3";
            case NkKey::NK_NUM4:             return "NK_NUM4";
            case NkKey::NK_NUM5:             return "NK_NUM5";
            case NkKey::NK_NUM6:             return "NK_NUM6";
            case NkKey::NK_NUM7:             return "NK_NUM7";
            case NkKey::NK_NUM8:             return "NK_NUM8";
            case NkKey::NK_NUM9:             return "NK_NUM9";
            case NkKey::NK_NUM0:             return "NK_NUM0";
            case NkKey::NK_MINUS:            return "NK_MINUS";
            case NkKey::NK_EQUALS:           return "NK_EQUALS";
            case NkKey::NK_BACK:             return "NK_BACK";
            case NkKey::NK_TAB:              return "NK_TAB";
            case NkKey::NK_Q:                return "NK_Q";
            case NkKey::NK_W:                return "NK_W";
            case NkKey::NK_E:                return "NK_E";
            case NkKey::NK_R:                return "NK_R";
            case NkKey::NK_T:                return "NK_T";
            case NkKey::NK_Y:                return "NK_Y";
            case NkKey::NK_U:                return "NK_U";
            case NkKey::NK_I:                return "NK_I";
            case NkKey::NK_O:                return "NK_O";
            case NkKey::NK_P:                return "NK_P";
            case NkKey::NK_LBRACKET:         return "NK_LBRACKET";
            case NkKey::NK_RBRACKET:         return "NK_RBRACKET";
            case NkKey::NK_BACKSLASH:        return "NK_BACKSLASH";
            case NkKey::NK_CAPSLOCK:         return "NK_CAPSLOCK";
            case NkKey::NK_A:                return "NK_A";
            case NkKey::NK_S:                return "NK_S";
            case NkKey::NK_D:                return "NK_D";
            case NkKey::NK_F:                return "NK_F";
            case NkKey::NK_G:                return "NK_G";
            case NkKey::NK_H:                return "NK_H";
            case NkKey::NK_J:                return "NK_J";
            case NkKey::NK_K:                return "NK_K";
            case NkKey::NK_L:                return "NK_L";
            case NkKey::NK_SEMICOLON:        return "NK_SEMICOLON";
            case NkKey::NK_APOSTROPHE:       return "NK_APOSTROPHE";
            case NkKey::NK_ENTER:            return "NK_ENTER";
            case NkKey::NK_LSHIFT:           return "NK_LSHIFT";
            case NkKey::NK_Z:                return "NK_Z";
            case NkKey::NK_X:                return "NK_X";
            case NkKey::NK_C:                return "NK_C";
            case NkKey::NK_V:                return "NK_V";
            case NkKey::NK_B:                return "NK_B";
            case NkKey::NK_N:                return "NK_N";
            case NkKey::NK_M:                return "NK_M";
            case NkKey::NK_COMMA:            return "NK_COMMA";
            case NkKey::NK_PERIOD:           return "NK_PERIOD";
            case NkKey::NK_SLASH:            return "NK_SLASH";
            case NkKey::NK_RSHIFT:           return "NK_RSHIFT";
            case NkKey::NK_LCTRL:            return "NK_LCTRL";
            case NkKey::NK_LSUPER:           return "NK_LSUPER";
            case NkKey::NK_LALT:             return "NK_LALT";
            case NkKey::NK_SPACE:            return "NK_SPACE";
            case NkKey::NK_RALT:             return "NK_RALT";
            case NkKey::NK_RSUPER:           return "NK_RSUPER";
            case NkKey::NK_MENU:             return "NK_MENU";
            case NkKey::NK_RCTRL:            return "NK_RCTRL";
            case NkKey::NK_PRINT_SCREEN:     return "NK_PRINT_SCREEN";
            case NkKey::NK_SCROLL_LOCK:      return "NK_SCROLL_LOCK";
            case NkKey::NK_PAUSE_BREAK:      return "NK_PAUSE_BREAK";
            case NkKey::NK_INSERT:           return "NK_INSERT";
            case NkKey::NK_DELETE:           return "NK_DELETE";
            case NkKey::NK_HOME:             return "NK_HOME";
            case NkKey::NK_END:              return "NK_END";
            case NkKey::NK_PAGE_UP:          return "NK_PAGE_UP";
            case NkKey::NK_PAGE_DOWN:        return "NK_PAGE_DOWN";
            case NkKey::NK_UP:               return "NK_UP";
            case NkKey::NK_DOWN:             return "NK_DOWN";
            case NkKey::NK_LEFT:             return "NK_LEFT";
            case NkKey::NK_RIGHT:            return "NK_RIGHT";
            case NkKey::NK_NUM_LOCK:         return "NK_NUM_LOCK";
            case NkKey::NK_NUMPAD_DIV:       return "NK_NUMPAD_DIV";
            case NkKey::NK_NUMPAD_MUL:       return "NK_NUMPAD_MUL";
            case NkKey::NK_NUMPAD_SUB:       return "NK_NUMPAD_SUB";
            case NkKey::NK_NUMPAD_ADD:       return "NK_NUMPAD_ADD";
            case NkKey::NK_NUMPAD_ENTER:     return "NK_NUMPAD_ENTER";
            case NkKey::NK_NUMPAD_DOT:       return "NK_NUMPAD_DOT";
            case NkKey::NK_NUMPAD_0:         return "NK_NUMPAD_0";
            case NkKey::NK_NUMPAD_1:         return "NK_NUMPAD_1";
            case NkKey::NK_NUMPAD_2:         return "NK_NUMPAD_2";
            case NkKey::NK_NUMPAD_3:         return "NK_NUMPAD_3";
            case NkKey::NK_NUMPAD_4:         return "NK_NUMPAD_4";
            case NkKey::NK_NUMPAD_5:         return "NK_NUMPAD_5";
            case NkKey::NK_NUMPAD_6:         return "NK_NUMPAD_6";
            case NkKey::NK_NUMPAD_7:         return "NK_NUMPAD_7";
            case NkKey::NK_NUMPAD_8:         return "NK_NUMPAD_8";
            case NkKey::NK_NUMPAD_9:         return "NK_NUMPAD_9";
            case NkKey::NK_NUMPAD_EQUALS:    return "NK_NUMPAD_EQUALS";
            case NkKey::NK_MEDIA_PLAY_PAUSE: return "NK_MEDIA_PLAY_PAUSE";
            case NkKey::NK_MEDIA_STOP:       return "NK_MEDIA_STOP";
            case NkKey::NK_MEDIA_NEXT:       return "NK_MEDIA_NEXT";
            case NkKey::NK_MEDIA_PREV:       return "NK_MEDIA_PREV";
            case NkKey::NK_MEDIA_VOLUME_UP:  return "NK_MEDIA_VOLUME_UP";
            case NkKey::NK_MEDIA_VOLUME_DOWN:return "NK_MEDIA_VOLUME_DOWN";
            case NkKey::NK_MEDIA_MUTE:       return "NK_MEDIA_MUTE";
            case NkKey::NK_BROWSER_BACK:     return "NK_BROWSER_BACK";
            case NkKey::NK_BROWSER_FORWARD:  return "NK_BROWSER_FORWARD";
            case NkKey::NK_BROWSER_REFRESH:  return "NK_BROWSER_REFRESH";
            case NkKey::NK_BROWSER_HOME:     return "NK_BROWSER_HOME";
            case NkKey::NK_BROWSER_SEARCH:   return "NK_BROWSER_SEARCH";
            case NkKey::NK_BROWSER_FAVORITES:return "NK_BROWSER_FAVORITES";
            case NkKey::NK_SLEEP:            return "NK_SLEEP";
            case NkKey::NK_CLEAR:            return "NK_CLEAR";
            case NkKey::NK_SEPARATOR:        return "NK_SEPARATOR";
            default:                         return "NK_UNKNOWN";
        }
    }

    // =========================================================================
    // NkKeyIsModifier / NkKeyIsNumpad / NkKeyIsFunctionKey
    // =========================================================================

    bool NkKeyIsModifier(NkKey key) noexcept {
        switch (key) {
            case NkKey::NK_LCTRL: case NkKey::NK_RCTRL:
            case NkKey::NK_LSHIFT: case NkKey::NK_RSHIFT:
            case NkKey::NK_LALT: case NkKey::NK_RALT:
            case NkKey::NK_LSUPER: case NkKey::NK_RSUPER:
                return true;
            default:
                return false;
        }
    }

    bool NkKeyIsNumpad(NkKey key) noexcept {
        return key >= NkKey::NK_NUM_LOCK && key <= NkKey::NK_NUMPAD_EQUALS;
    }

    bool NkKeyIsFunctionKey(NkKey key) noexcept {
        return key >= NkKey::NK_F1 && key <= NkKey::NK_F24;
    }

    // =========================================================================
    // NkScancodeToString
    // =========================================================================

    const char* NkScancodeToString(NkScancode sc) noexcept {
        switch (sc) {
            case NkScancode::NK_SC_A:               return "SC_A";
            case NkScancode::NK_SC_B:               return "SC_B";
            case NkScancode::NK_SC_C:               return "SC_C";
            case NkScancode::NK_SC_D:               return "SC_D";
            case NkScancode::NK_SC_E:               return "SC_E";
            case NkScancode::NK_SC_F:               return "SC_F";
            case NkScancode::NK_SC_G:               return "SC_G";
            case NkScancode::NK_SC_H:               return "SC_H";
            case NkScancode::NK_SC_I:               return "SC_I";
            case NkScancode::NK_SC_J:               return "SC_J";
            case NkScancode::NK_SC_K:               return "SC_K";
            case NkScancode::NK_SC_L:               return "SC_L";
            case NkScancode::NK_SC_M:               return "SC_M";
            case NkScancode::NK_SC_N:               return "SC_N";
            case NkScancode::NK_SC_O:               return "SC_O";
            case NkScancode::NK_SC_P:               return "SC_P";
            case NkScancode::NK_SC_Q:               return "SC_Q";
            case NkScancode::NK_SC_R:               return "SC_R";
            case NkScancode::NK_SC_S:               return "SC_S";
            case NkScancode::NK_SC_T:               return "SC_T";
            case NkScancode::NK_SC_U:               return "SC_U";
            case NkScancode::NK_SC_V:               return "SC_V";
            case NkScancode::NK_SC_W:               return "SC_W";
            case NkScancode::NK_SC_X:               return "SC_X";
            case NkScancode::NK_SC_Y:               return "SC_Y";
            case NkScancode::NK_SC_Z:               return "SC_Z";
            case NkScancode::NK_SC_1:               return "SC_1";
            case NkScancode::NK_SC_2:               return "SC_2";
            case NkScancode::NK_SC_3:               return "SC_3";
            case NkScancode::NK_SC_4:               return "SC_4";
            case NkScancode::NK_SC_5:               return "SC_5";
            case NkScancode::NK_SC_6:               return "SC_6";
            case NkScancode::NK_SC_7:               return "SC_7";
            case NkScancode::NK_SC_8:               return "SC_8";
            case NkScancode::NK_SC_9:               return "SC_9";
            case NkScancode::NK_SC_0:               return "SC_0";
            case NkScancode::NK_SC_ENTER:           return "SC_ENTER";
            case NkScancode::NK_SC_ESCAPE:          return "SC_ESCAPE";
            case NkScancode::NK_SC_BACKSPACE:       return "SC_BACKSPACE";
            case NkScancode::NK_SC_TAB:             return "SC_TAB";
            case NkScancode::NK_SC_SPACE:           return "SC_SPACE";
            case NkScancode::NK_SC_MINUS:           return "SC_MINUS";
            case NkScancode::NK_SC_EQUALS:          return "SC_EQUALS";
            case NkScancode::NK_SC_LBRACKET:        return "SC_LBRACKET";
            case NkScancode::NK_SC_RBRACKET:        return "SC_RBRACKET";
            case NkScancode::NK_SC_BACKSLASH:       return "SC_BACKSLASH";
            case NkScancode::NK_SC_SEMICOLON:       return "SC_SEMICOLON";
            case NkScancode::NK_SC_APOSTROPHE:      return "SC_APOSTROPHE";
            case NkScancode::NK_SC_GRAVE:           return "SC_GRAVE";
            case NkScancode::NK_SC_COMMA:           return "SC_COMMA";
            case NkScancode::NK_SC_PERIOD:          return "SC_PERIOD";
            case NkScancode::NK_SC_SLASH:           return "SC_SLASH";
            case NkScancode::NK_SC_CAPS_LOCK:       return "SC_CAPS_LOCK";
            case NkScancode::NK_SC_F1:              return "SC_F1";
            case NkScancode::NK_SC_F2:              return "SC_F2";
            case NkScancode::NK_SC_F3:              return "SC_F3";
            case NkScancode::NK_SC_F4:              return "SC_F4";
            case NkScancode::NK_SC_F5:              return "SC_F5";
            case NkScancode::NK_SC_F6:              return "SC_F6";
            case NkScancode::NK_SC_F7:              return "SC_F7";
            case NkScancode::NK_SC_F8:              return "SC_F8";
            case NkScancode::NK_SC_F9:              return "SC_F9";
            case NkScancode::NK_SC_F10:             return "SC_F10";
            case NkScancode::NK_SC_F11:             return "SC_F11";
            case NkScancode::NK_SC_F12:             return "SC_F12";
            case NkScancode::NK_SC_PRINT_SCREEN:    return "SC_PRINT_SCREEN";
            case NkScancode::NK_SC_SCROLL_LOCK:     return "SC_SCROLL_LOCK";
            case NkScancode::NK_SC_PAUSE:           return "SC_PAUSE";
            case NkScancode::NK_SC_INSERT:          return "SC_INSERT";
            case NkScancode::NK_SC_HOME:            return "SC_HOME";
            case NkScancode::NK_SC_PAGE_UP:         return "SC_PAGE_UP";
            case NkScancode::NK_SC_DELETE:          return "SC_DELETE";
            case NkScancode::NK_SC_END:             return "SC_END";
            case NkScancode::NK_SC_PAGE_DOWN:       return "SC_PAGE_DOWN";
            case NkScancode::NK_SC_RIGHT:           return "SC_RIGHT";
            case NkScancode::NK_SC_LEFT:            return "SC_LEFT";
            case NkScancode::NK_SC_DOWN:            return "SC_DOWN";
            case NkScancode::NK_SC_UP:              return "SC_UP";
            case NkScancode::NK_SC_NUM_LOCK:        return "SC_NUM_LOCK";
            case NkScancode::NK_SC_NUMPAD_DIV:      return "SC_NUMPAD_DIV";
            case NkScancode::NK_SC_NUMPAD_MUL:      return "SC_NUMPAD_MUL";
            case NkScancode::NK_SC_NUMPAD_SUB:      return "SC_NUMPAD_SUB";
            case NkScancode::NK_SC_NUMPAD_ADD:      return "SC_NUMPAD_ADD";
            case NkScancode::NK_SC_NUMPAD_ENTER:    return "SC_NUMPAD_ENTER";
            case NkScancode::NK_SC_NUMPAD_0:        return "SC_NUMPAD_0";
            case NkScancode::NK_SC_NUMPAD_1:        return "SC_NUMPAD_1";
            case NkScancode::NK_SC_NUMPAD_2:        return "SC_NUMPAD_2";
            case NkScancode::NK_SC_NUMPAD_3:        return "SC_NUMPAD_3";
            case NkScancode::NK_SC_NUMPAD_4:        return "SC_NUMPAD_4";
            case NkScancode::NK_SC_NUMPAD_5:        return "SC_NUMPAD_5";
            case NkScancode::NK_SC_NUMPAD_6:        return "SC_NUMPAD_6";
            case NkScancode::NK_SC_NUMPAD_7:        return "SC_NUMPAD_7";
            case NkScancode::NK_SC_NUMPAD_8:        return "SC_NUMPAD_8";
            case NkScancode::NK_SC_NUMPAD_9:        return "SC_NUMPAD_9";
            case NkScancode::NK_SC_NUMPAD_DOT:      return "SC_NUMPAD_DOT";
            case NkScancode::NK_SC_NUMPAD_EQUALS:   return "SC_NUMPAD_EQUALS";
            case NkScancode::NK_SC_APPLICATION:     return "SC_APPLICATION";
            case NkScancode::NK_SC_MUTE:            return "SC_MUTE";
            case NkScancode::NK_SC_VOLUME_UP:       return "SC_VOLUME_UP";
            case NkScancode::NK_SC_VOLUME_DOWN:     return "SC_VOLUME_DOWN";
            case NkScancode::NK_SC_LCTRL:           return "SC_LCTRL";
            case NkScancode::NK_SC_LSHIFT:          return "SC_LSHIFT";
            case NkScancode::NK_SC_LALT:            return "SC_LALT";
            case NkScancode::NK_SC_LSUPER:          return "SC_LSUPER";
            case NkScancode::NK_SC_RCTRL:           return "SC_RCTRL";
            case NkScancode::NK_SC_RSHIFT:          return "SC_RSHIFT";
            case NkScancode::NK_SC_RALT:            return "SC_RALT";
            case NkScancode::NK_SC_RSUPER:          return "SC_RSUPER";
            default:                                 return "SC_UNKNOWN";
        }
    }

    // =========================================================================
    // NkScancodeToKey — USB HID → NkKey (US-QWERTY invariant)
    // =========================================================================

    NkKey NkScancodeToKey(NkScancode sc) noexcept {
        switch (sc) {
            case NkScancode::NK_SC_A:            return NkKey::NK_A;
            case NkScancode::NK_SC_B:            return NkKey::NK_B;
            case NkScancode::NK_SC_C:            return NkKey::NK_C;
            case NkScancode::NK_SC_D:            return NkKey::NK_D;
            case NkScancode::NK_SC_E:            return NkKey::NK_E;
            case NkScancode::NK_SC_F:            return NkKey::NK_F;
            case NkScancode::NK_SC_G:            return NkKey::NK_G;
            case NkScancode::NK_SC_H:            return NkKey::NK_H;
            case NkScancode::NK_SC_I:            return NkKey::NK_I;
            case NkScancode::NK_SC_J:            return NkKey::NK_J;
            case NkScancode::NK_SC_K:            return NkKey::NK_K;
            case NkScancode::NK_SC_L:            return NkKey::NK_L;
            case NkScancode::NK_SC_M:            return NkKey::NK_M;
            case NkScancode::NK_SC_N:            return NkKey::NK_N;
            case NkScancode::NK_SC_O:            return NkKey::NK_O;
            case NkScancode::NK_SC_P:            return NkKey::NK_P;
            case NkScancode::NK_SC_Q:            return NkKey::NK_Q;
            case NkScancode::NK_SC_R:            return NkKey::NK_R;
            case NkScancode::NK_SC_S:            return NkKey::NK_S;
            case NkScancode::NK_SC_T:            return NkKey::NK_T;
            case NkScancode::NK_SC_U:            return NkKey::NK_U;
            case NkScancode::NK_SC_V:            return NkKey::NK_V;
            case NkScancode::NK_SC_W:            return NkKey::NK_W;
            case NkScancode::NK_SC_X:            return NkKey::NK_X;
            case NkScancode::NK_SC_Y:            return NkKey::NK_Y;
            case NkScancode::NK_SC_Z:            return NkKey::NK_Z;
            case NkScancode::NK_SC_1:            return NkKey::NK_NUM1;
            case NkScancode::NK_SC_2:            return NkKey::NK_NUM2;
            case NkScancode::NK_SC_3:            return NkKey::NK_NUM3;
            case NkScancode::NK_SC_4:            return NkKey::NK_NUM4;
            case NkScancode::NK_SC_5:            return NkKey::NK_NUM5;
            case NkScancode::NK_SC_6:            return NkKey::NK_NUM6;
            case NkScancode::NK_SC_7:            return NkKey::NK_NUM7;
            case NkScancode::NK_SC_8:            return NkKey::NK_NUM8;
            case NkScancode::NK_SC_9:            return NkKey::NK_NUM9;
            case NkScancode::NK_SC_0:            return NkKey::NK_NUM0;
            case NkScancode::NK_SC_ENTER:        return NkKey::NK_ENTER;
            case NkScancode::NK_SC_ESCAPE:       return NkKey::NK_ESCAPE;
            case NkScancode::NK_SC_BACKSPACE:    return NkKey::NK_BACK;
            case NkScancode::NK_SC_TAB:          return NkKey::NK_TAB;
            case NkScancode::NK_SC_SPACE:        return NkKey::NK_SPACE;
            case NkScancode::NK_SC_MINUS:        return NkKey::NK_MINUS;
            case NkScancode::NK_SC_EQUALS:       return NkKey::NK_EQUALS;
            case NkScancode::NK_SC_LBRACKET:     return NkKey::NK_LBRACKET;
            case NkScancode::NK_SC_RBRACKET:     return NkKey::NK_RBRACKET;
            case NkScancode::NK_SC_BACKSLASH:    return NkKey::NK_BACKSLASH;
            case NkScancode::NK_SC_SEMICOLON:    return NkKey::NK_SEMICOLON;
            case NkScancode::NK_SC_APOSTROPHE:   return NkKey::NK_APOSTROPHE;
            case NkScancode::NK_SC_GRAVE:        return NkKey::NK_GRAVE;
            case NkScancode::NK_SC_COMMA:        return NkKey::NK_COMMA;
            case NkScancode::NK_SC_PERIOD:       return NkKey::NK_PERIOD;
            case NkScancode::NK_SC_SLASH:        return NkKey::NK_SLASH;
            case NkScancode::NK_SC_CAPS_LOCK:    return NkKey::NK_CAPSLOCK;
            case NkScancode::NK_SC_F1:           return NkKey::NK_F1;
            case NkScancode::NK_SC_F2:           return NkKey::NK_F2;
            case NkScancode::NK_SC_F3:           return NkKey::NK_F3;
            case NkScancode::NK_SC_F4:           return NkKey::NK_F4;
            case NkScancode::NK_SC_F5:           return NkKey::NK_F5;
            case NkScancode::NK_SC_F6:           return NkKey::NK_F6;
            case NkScancode::NK_SC_F7:           return NkKey::NK_F7;
            case NkScancode::NK_SC_F8:           return NkKey::NK_F8;
            case NkScancode::NK_SC_F9:           return NkKey::NK_F9;
            case NkScancode::NK_SC_F10:          return NkKey::NK_F10;
            case NkScancode::NK_SC_F11:          return NkKey::NK_F11;
            case NkScancode::NK_SC_F12:          return NkKey::NK_F12;
            case NkScancode::NK_SC_F13:          return NkKey::NK_F13;
            case NkScancode::NK_SC_F14:          return NkKey::NK_F14;
            case NkScancode::NK_SC_F15:          return NkKey::NK_F15;
            case NkScancode::NK_SC_F16:          return NkKey::NK_F16;
            case NkScancode::NK_SC_F17:          return NkKey::NK_F17;
            case NkScancode::NK_SC_F18:          return NkKey::NK_F18;
            case NkScancode::NK_SC_F19:          return NkKey::NK_F19;
            case NkScancode::NK_SC_F20:          return NkKey::NK_F20;
            case NkScancode::NK_SC_F21:          return NkKey::NK_F21;
            case NkScancode::NK_SC_F22:          return NkKey::NK_F22;
            case NkScancode::NK_SC_F23:          return NkKey::NK_F23;
            case NkScancode::NK_SC_F24:          return NkKey::NK_F24;
            case NkScancode::NK_SC_PRINT_SCREEN: return NkKey::NK_PRINT_SCREEN;
            case NkScancode::NK_SC_SCROLL_LOCK:  return NkKey::NK_SCROLL_LOCK;
            case NkScancode::NK_SC_PAUSE:        return NkKey::NK_PAUSE_BREAK;
            case NkScancode::NK_SC_INSERT:       return NkKey::NK_INSERT;
            case NkScancode::NK_SC_HOME:         return NkKey::NK_HOME;
            case NkScancode::NK_SC_PAGE_UP:      return NkKey::NK_PAGE_UP;
            case NkScancode::NK_SC_DELETE:       return NkKey::NK_DELETE;
            case NkScancode::NK_SC_END:          return NkKey::NK_END;
            case NkScancode::NK_SC_PAGE_DOWN:    return NkKey::NK_PAGE_DOWN;
            case NkScancode::NK_SC_RIGHT:        return NkKey::NK_RIGHT;
            case NkScancode::NK_SC_LEFT:         return NkKey::NK_LEFT;
            case NkScancode::NK_SC_DOWN:         return NkKey::NK_DOWN;
            case NkScancode::NK_SC_UP:           return NkKey::NK_UP;
            case NkScancode::NK_SC_NUM_LOCK:     return NkKey::NK_NUM_LOCK;
            case NkScancode::NK_SC_NUMPAD_DIV:   return NkKey::NK_NUMPAD_DIV;
            case NkScancode::NK_SC_NUMPAD_MUL:   return NkKey::NK_NUMPAD_MUL;
            case NkScancode::NK_SC_NUMPAD_SUB:   return NkKey::NK_NUMPAD_SUB;
            case NkScancode::NK_SC_NUMPAD_ADD:   return NkKey::NK_NUMPAD_ADD;
            case NkScancode::NK_SC_NUMPAD_ENTER: return NkKey::NK_NUMPAD_ENTER;
            case NkScancode::NK_SC_NUMPAD_0:     return NkKey::NK_NUMPAD_0;
            case NkScancode::NK_SC_NUMPAD_1:     return NkKey::NK_NUMPAD_1;
            case NkScancode::NK_SC_NUMPAD_2:     return NkKey::NK_NUMPAD_2;
            case NkScancode::NK_SC_NUMPAD_3:     return NkKey::NK_NUMPAD_3;
            case NkScancode::NK_SC_NUMPAD_4:     return NkKey::NK_NUMPAD_4;
            case NkScancode::NK_SC_NUMPAD_5:     return NkKey::NK_NUMPAD_5;
            case NkScancode::NK_SC_NUMPAD_6:     return NkKey::NK_NUMPAD_6;
            case NkScancode::NK_SC_NUMPAD_7:     return NkKey::NK_NUMPAD_7;
            case NkScancode::NK_SC_NUMPAD_8:     return NkKey::NK_NUMPAD_8;
            case NkScancode::NK_SC_NUMPAD_9:     return NkKey::NK_NUMPAD_9;
            case NkScancode::NK_SC_NUMPAD_DOT:   return NkKey::NK_NUMPAD_DOT;
            case NkScancode::NK_SC_NUMPAD_EQUALS:return NkKey::NK_NUMPAD_EQUALS;
            case NkScancode::NK_SC_APPLICATION:  return NkKey::NK_MENU;
            case NkScancode::NK_SC_MUTE:         return NkKey::NK_MEDIA_MUTE;
            case NkScancode::NK_SC_VOLUME_UP:    return NkKey::NK_MEDIA_VOLUME_UP;
            case NkScancode::NK_SC_VOLUME_DOWN:  return NkKey::NK_MEDIA_VOLUME_DOWN;
            case NkScancode::NK_SC_MEDIA_PLAY_PAUSE: return NkKey::NK_MEDIA_PLAY_PAUSE;
            case NkScancode::NK_SC_MEDIA_STOP:   return NkKey::NK_MEDIA_STOP;
            case NkScancode::NK_SC_MEDIA_NEXT:   return NkKey::NK_MEDIA_NEXT;
            case NkScancode::NK_SC_MEDIA_PREV:   return NkKey::NK_MEDIA_PREV;
            case NkScancode::NK_SC_LCTRL:        return NkKey::NK_LCTRL;
            case NkScancode::NK_SC_LSHIFT:       return NkKey::NK_LSHIFT;
            case NkScancode::NK_SC_LALT:         return NkKey::NK_LALT;
            case NkScancode::NK_SC_LSUPER:       return NkKey::NK_LSUPER;
            case NkScancode::NK_SC_RCTRL:        return NkKey::NK_RCTRL;
            case NkScancode::NK_SC_RSHIFT:       return NkKey::NK_RSHIFT;
            case NkScancode::NK_SC_RALT:         return NkKey::NK_RALT;
            case NkScancode::NK_SC_RSUPER:       return NkKey::NK_RSUPER;
            default:                             return NkKey::NK_UNKNOWN;
        }
    }

    // =========================================================================
    // NkScancodeFromWin32 — PS/2 Set-1 → USB HID
    // Bits 16–23 du LPARAM de WM_KEYDOWN, bit 24 = extended
    // =========================================================================

    NkScancode NkScancodeFromWin32(NkU32 win32, bool ext) noexcept {
        // Table PS/2 Set-1 normale (0x00–0x58)
        static const NkScancode kTable[0x80] = {
            NkScancode::NK_SC_UNKNOWN,     // 00
            NkScancode::NK_SC_ESCAPE,      // 01
            NkScancode::NK_SC_1,           // 02
            NkScancode::NK_SC_2,           // 03
            NkScancode::NK_SC_3,           // 04
            NkScancode::NK_SC_4,           // 05
            NkScancode::NK_SC_5,           // 06
            NkScancode::NK_SC_6,           // 07
            NkScancode::NK_SC_7,           // 08
            NkScancode::NK_SC_8,           // 09
            NkScancode::NK_SC_9,           // 0A
            NkScancode::NK_SC_0,           // 0B
            NkScancode::NK_SC_MINUS,       // 0C
            NkScancode::NK_SC_EQUALS,      // 0D
            NkScancode::NK_SC_BACKSPACE,   // 0E
            NkScancode::NK_SC_TAB,         // 0F
            NkScancode::NK_SC_Q,           // 10
            NkScancode::NK_SC_W,           // 11
            NkScancode::NK_SC_E,           // 12
            NkScancode::NK_SC_R,           // 13
            NkScancode::NK_SC_T,           // 14
            NkScancode::NK_SC_Y,           // 15
            NkScancode::NK_SC_U,           // 16
            NkScancode::NK_SC_I,           // 17
            NkScancode::NK_SC_O,           // 18
            NkScancode::NK_SC_P,           // 19
            NkScancode::NK_SC_LBRACKET,    // 1A
            NkScancode::NK_SC_RBRACKET,    // 1B
            NkScancode::NK_SC_ENTER,       // 1C
            NkScancode::NK_SC_LCTRL,       // 1D
            NkScancode::NK_SC_A,           // 1E
            NkScancode::NK_SC_S,           // 1F
            NkScancode::NK_SC_D,           // 20
            NkScancode::NK_SC_F,           // 21
            NkScancode::NK_SC_G,           // 22
            NkScancode::NK_SC_H,           // 23
            NkScancode::NK_SC_J,           // 24
            NkScancode::NK_SC_K,           // 25
            NkScancode::NK_SC_L,           // 26
            NkScancode::NK_SC_SEMICOLON,   // 27
            NkScancode::NK_SC_APOSTROPHE,  // 28
            NkScancode::NK_SC_GRAVE,       // 29
            NkScancode::NK_SC_LSHIFT,      // 2A
            NkScancode::NK_SC_BACKSLASH,   // 2B
            NkScancode::NK_SC_Z,           // 2C
            NkScancode::NK_SC_X,           // 2D
            NkScancode::NK_SC_C,           // 2E
            NkScancode::NK_SC_V,           // 2F
            NkScancode::NK_SC_B,           // 30
            NkScancode::NK_SC_N,           // 31
            NkScancode::NK_SC_M,           // 32
            NkScancode::NK_SC_COMMA,       // 33
            NkScancode::NK_SC_PERIOD,      // 34
            NkScancode::NK_SC_SLASH,       // 35
            NkScancode::NK_SC_RSHIFT,      // 36
            NkScancode::NK_SC_NUMPAD_MUL,  // 37
            NkScancode::NK_SC_LALT,        // 38
            NkScancode::NK_SC_SPACE,       // 39
            NkScancode::NK_SC_CAPS_LOCK,   // 3A
            NkScancode::NK_SC_F1,          // 3B
            NkScancode::NK_SC_F2,          // 3C
            NkScancode::NK_SC_F3,          // 3D
            NkScancode::NK_SC_F4,          // 3E
            NkScancode::NK_SC_F5,          // 3F
            NkScancode::NK_SC_F6,          // 40
            NkScancode::NK_SC_F7,          // 41
            NkScancode::NK_SC_F8,          // 42
            NkScancode::NK_SC_F9,          // 43
            NkScancode::NK_SC_F10,         // 44
            NkScancode::NK_SC_NUM_LOCK,    // 45
            NkScancode::NK_SC_SCROLL_LOCK, // 46
            NkScancode::NK_SC_NUMPAD_7,    // 47
            NkScancode::NK_SC_NUMPAD_8,    // 48
            NkScancode::NK_SC_NUMPAD_9,    // 49
            NkScancode::NK_SC_NUMPAD_SUB,  // 4A
            NkScancode::NK_SC_NUMPAD_4,    // 4B
            NkScancode::NK_SC_NUMPAD_5,    // 4C
            NkScancode::NK_SC_NUMPAD_6,    // 4D
            NkScancode::NK_SC_NUMPAD_ADD,  // 4E
            NkScancode::NK_SC_NUMPAD_1,    // 4F
            NkScancode::NK_SC_NUMPAD_2,    // 50
            NkScancode::NK_SC_NUMPAD_3,    // 51
            NkScancode::NK_SC_NUMPAD_0,    // 52
            NkScancode::NK_SC_NUMPAD_DOT,  // 53
            NkScancode::NK_SC_UNKNOWN,     // 54 SysRq
            NkScancode::NK_SC_UNKNOWN,     // 55
            NkScancode::NK_SC_NONUS_BACKSLASH, // 56
            NkScancode::NK_SC_F11,         // 57
            NkScancode::NK_SC_F12,         // 58
            // 59–7F → UNKNOWN (rempli par le compilateur à 0)
        };

        if (win32 >= 0x80) return NkScancode::NK_SC_UNKNOWN;

        if (ext) {
            // Touches étendues (préfixe E0 PS/2)
            switch (win32) {
                case 0x1C: return NkScancode::NK_SC_NUMPAD_ENTER;
                case 0x1D: return NkScancode::NK_SC_RCTRL;
                case 0x35: return NkScancode::NK_SC_NUMPAD_DIV;
                case 0x37: return NkScancode::NK_SC_PRINT_SCREEN;
                case 0x38: return NkScancode::NK_SC_RALT;
                case 0x47: return NkScancode::NK_SC_HOME;
                case 0x48: return NkScancode::NK_SC_UP;
                case 0x49: return NkScancode::NK_SC_PAGE_UP;
                case 0x4B: return NkScancode::NK_SC_LEFT;
                case 0x4D: return NkScancode::NK_SC_RIGHT;
                case 0x4F: return NkScancode::NK_SC_END;
                case 0x50: return NkScancode::NK_SC_DOWN;
                case 0x51: return NkScancode::NK_SC_PAGE_DOWN;
                case 0x52: return NkScancode::NK_SC_INSERT;
                case 0x53: return NkScancode::NK_SC_DELETE;
                case 0x5B: return NkScancode::NK_SC_LSUPER;
                case 0x5C: return NkScancode::NK_SC_RSUPER;
                case 0x5D: return NkScancode::NK_SC_APPLICATION;
                default:   return NkScancode::NK_SC_UNKNOWN;
            }
        }
        return kTable[win32];
    }

    // =========================================================================
    // NkScancodeFromLinux — evdev keycode → USB HID
    // =========================================================================

    NkScancode NkScancodeFromLinux(NkU32 kc) noexcept {
        // evdev (Linux kernel) → USB HID
        switch (kc) {
            case  1: return NkScancode::NK_SC_ESCAPE;
            case  2: return NkScancode::NK_SC_1;
            case  3: return NkScancode::NK_SC_2;
            case  4: return NkScancode::NK_SC_3;
            case  5: return NkScancode::NK_SC_4;
            case  6: return NkScancode::NK_SC_5;
            case  7: return NkScancode::NK_SC_6;
            case  8: return NkScancode::NK_SC_7;
            case  9: return NkScancode::NK_SC_8;
            case 10: return NkScancode::NK_SC_9;
            case 11: return NkScancode::NK_SC_0;
            case 12: return NkScancode::NK_SC_MINUS;
            case 13: return NkScancode::NK_SC_EQUALS;
            case 14: return NkScancode::NK_SC_BACKSPACE;
            case 15: return NkScancode::NK_SC_TAB;
            case 16: return NkScancode::NK_SC_Q;
            case 17: return NkScancode::NK_SC_W;
            case 18: return NkScancode::NK_SC_E;
            case 19: return NkScancode::NK_SC_R;
            case 20: return NkScancode::NK_SC_T;
            case 21: return NkScancode::NK_SC_Y;
            case 22: return NkScancode::NK_SC_U;
            case 23: return NkScancode::NK_SC_I;
            case 24: return NkScancode::NK_SC_O;
            case 25: return NkScancode::NK_SC_P;
            case 26: return NkScancode::NK_SC_LBRACKET;
            case 27: return NkScancode::NK_SC_RBRACKET;
            case 28: return NkScancode::NK_SC_ENTER;
            case 29: return NkScancode::NK_SC_LCTRL;
            case 30: return NkScancode::NK_SC_A;
            case 31: return NkScancode::NK_SC_S;
            case 32: return NkScancode::NK_SC_D;
            case 33: return NkScancode::NK_SC_F;
            case 34: return NkScancode::NK_SC_G;
            case 35: return NkScancode::NK_SC_H;
            case 36: return NkScancode::NK_SC_J;
            case 37: return NkScancode::NK_SC_K;
            case 38: return NkScancode::NK_SC_L;
            case 39: return NkScancode::NK_SC_SEMICOLON;
            case 40: return NkScancode::NK_SC_APOSTROPHE;
            case 41: return NkScancode::NK_SC_GRAVE;
            case 42: return NkScancode::NK_SC_LSHIFT;
            case 43: return NkScancode::NK_SC_BACKSLASH;
            case 44: return NkScancode::NK_SC_Z;
            case 45: return NkScancode::NK_SC_X;
            case 46: return NkScancode::NK_SC_C;
            case 47: return NkScancode::NK_SC_V;
            case 48: return NkScancode::NK_SC_B;
            case 49: return NkScancode::NK_SC_N;
            case 50: return NkScancode::NK_SC_M;
            case 51: return NkScancode::NK_SC_COMMA;
            case 52: return NkScancode::NK_SC_PERIOD;
            case 53: return NkScancode::NK_SC_SLASH;
            case 54: return NkScancode::NK_SC_RSHIFT;
            case 55: return NkScancode::NK_SC_NUMPAD_MUL;
            case 56: return NkScancode::NK_SC_LALT;
            case 57: return NkScancode::NK_SC_SPACE;
            case 58: return NkScancode::NK_SC_CAPS_LOCK;
            case 59: return NkScancode::NK_SC_F1;
            case 60: return NkScancode::NK_SC_F2;
            case 61: return NkScancode::NK_SC_F3;
            case 62: return NkScancode::NK_SC_F4;
            case 63: return NkScancode::NK_SC_F5;
            case 64: return NkScancode::NK_SC_F6;
            case 65: return NkScancode::NK_SC_F7;
            case 66: return NkScancode::NK_SC_F8;
            case 67: return NkScancode::NK_SC_F9;
            case 68: return NkScancode::NK_SC_F10;
            case 69: return NkScancode::NK_SC_NUM_LOCK;
            case 70: return NkScancode::NK_SC_SCROLL_LOCK;
            case 71: return NkScancode::NK_SC_NUMPAD_7;
            case 72: return NkScancode::NK_SC_NUMPAD_8;
            case 73: return NkScancode::NK_SC_NUMPAD_9;
            case 74: return NkScancode::NK_SC_NUMPAD_SUB;
            case 75: return NkScancode::NK_SC_NUMPAD_4;
            case 76: return NkScancode::NK_SC_NUMPAD_5;
            case 77: return NkScancode::NK_SC_NUMPAD_6;
            case 78: return NkScancode::NK_SC_NUMPAD_ADD;
            case 79: return NkScancode::NK_SC_NUMPAD_1;
            case 80: return NkScancode::NK_SC_NUMPAD_2;
            case 81: return NkScancode::NK_SC_NUMPAD_3;
            case 82: return NkScancode::NK_SC_NUMPAD_0;
            case 83: return NkScancode::NK_SC_NUMPAD_DOT;
            case 86: return NkScancode::NK_SC_NONUS_BACKSLASH;
            case 87: return NkScancode::NK_SC_F11;
            case 88: return NkScancode::NK_SC_F12;
            case 96: return NkScancode::NK_SC_NUMPAD_ENTER;
            case 97: return NkScancode::NK_SC_RCTRL;
            case 98: return NkScancode::NK_SC_NUMPAD_DIV;
            case 99: return NkScancode::NK_SC_PRINT_SCREEN;
            case 100: return NkScancode::NK_SC_RALT;
            case 102: return NkScancode::NK_SC_HOME;
            case 103: return NkScancode::NK_SC_UP;
            case 104: return NkScancode::NK_SC_PAGE_UP;
            case 105: return NkScancode::NK_SC_LEFT;
            case 106: return NkScancode::NK_SC_RIGHT;
            case 107: return NkScancode::NK_SC_END;
            case 108: return NkScancode::NK_SC_DOWN;
            case 109: return NkScancode::NK_SC_PAGE_DOWN;
            case 110: return NkScancode::NK_SC_INSERT;
            case 111: return NkScancode::NK_SC_DELETE;
            case 113: return NkScancode::NK_SC_MUTE;
            case 114: return NkScancode::NK_SC_VOLUME_DOWN;
            case 115: return NkScancode::NK_SC_VOLUME_UP;
            case 119: return NkScancode::NK_SC_PAUSE;
            case 125: return NkScancode::NK_SC_LSUPER;
            case 126: return NkScancode::NK_SC_RSUPER;
            case 127: return NkScancode::NK_SC_APPLICATION;
            default:  return NkScancode::NK_SC_UNKNOWN;
        }
    }

    NkScancode NkScancodeFromXKeycode(NkU32 xkeycode) noexcept {
        // XCB/XLib keycodes = evdev + 8
        return (xkeycode >= 8) ? NkScancodeFromLinux(xkeycode - 8)
                                : NkScancode::NK_SC_UNKNOWN;
    }

    // =========================================================================
    // NkScancodeFromMac — Carbon/Cocoa NSEvent.keyCode → USB HID
    // =========================================================================

    NkScancode NkScancodeFromMac(NkU32 kc) noexcept {
        switch (kc) {
            case 0x00: return NkScancode::NK_SC_A;
            case 0x01: return NkScancode::NK_SC_S;
            case 0x02: return NkScancode::NK_SC_D;
            case 0x03: return NkScancode::NK_SC_F;
            case 0x04: return NkScancode::NK_SC_H;
            case 0x05: return NkScancode::NK_SC_G;
            case 0x06: return NkScancode::NK_SC_Z;
            case 0x07: return NkScancode::NK_SC_X;
            case 0x08: return NkScancode::NK_SC_C;
            case 0x09: return NkScancode::NK_SC_V;
            case 0x0B: return NkScancode::NK_SC_B;
            case 0x0C: return NkScancode::NK_SC_Q;
            case 0x0D: return NkScancode::NK_SC_W;
            case 0x0E: return NkScancode::NK_SC_E;
            case 0x0F: return NkScancode::NK_SC_R;
            case 0x10: return NkScancode::NK_SC_Y;
            case 0x11: return NkScancode::NK_SC_T;
            case 0x12: return NkScancode::NK_SC_1;
            case 0x13: return NkScancode::NK_SC_2;
            case 0x14: return NkScancode::NK_SC_3;
            case 0x15: return NkScancode::NK_SC_4;
            case 0x16: return NkScancode::NK_SC_6;
            case 0x17: return NkScancode::NK_SC_5;
            case 0x18: return NkScancode::NK_SC_EQUALS;
            case 0x19: return NkScancode::NK_SC_9;
            case 0x1A: return NkScancode::NK_SC_7;
            case 0x1B: return NkScancode::NK_SC_MINUS;
            case 0x1C: return NkScancode::NK_SC_8;
            case 0x1D: return NkScancode::NK_SC_0;
            case 0x1E: return NkScancode::NK_SC_RBRACKET;
            case 0x1F: return NkScancode::NK_SC_O;
            case 0x20: return NkScancode::NK_SC_U;
            case 0x21: return NkScancode::NK_SC_LBRACKET;
            case 0x22: return NkScancode::NK_SC_I;
            case 0x23: return NkScancode::NK_SC_P;
            case 0x24: return NkScancode::NK_SC_ENTER;
            case 0x25: return NkScancode::NK_SC_L;
            case 0x26: return NkScancode::NK_SC_J;
            case 0x27: return NkScancode::NK_SC_APOSTROPHE;
            case 0x28: return NkScancode::NK_SC_K;
            case 0x29: return NkScancode::NK_SC_SEMICOLON;
            case 0x2A: return NkScancode::NK_SC_BACKSLASH;
            case 0x2B: return NkScancode::NK_SC_COMMA;
            case 0x2C: return NkScancode::NK_SC_SLASH;
            case 0x2D: return NkScancode::NK_SC_N;
            case 0x2E: return NkScancode::NK_SC_M;
            case 0x2F: return NkScancode::NK_SC_PERIOD;
            case 0x30: return NkScancode::NK_SC_TAB;
            case 0x31: return NkScancode::NK_SC_SPACE;
            case 0x32: return NkScancode::NK_SC_GRAVE;
            case 0x33: return NkScancode::NK_SC_BACKSPACE;
            case 0x35: return NkScancode::NK_SC_ESCAPE;
            case 0x36: return NkScancode::NK_SC_RSUPER; // Cmd droit
            case 0x37: return NkScancode::NK_SC_LSUPER; // Cmd gauche
            case 0x38: return NkScancode::NK_SC_LSHIFT;
            case 0x39: return NkScancode::NK_SC_CAPS_LOCK;
            case 0x3A: return NkScancode::NK_SC_LALT;
            case 0x3B: return NkScancode::NK_SC_LCTRL;
            case 0x3C: return NkScancode::NK_SC_RSHIFT;
            case 0x3D: return NkScancode::NK_SC_RALT;
            case 0x3E: return NkScancode::NK_SC_RCTRL;
            case 0x40: return NkScancode::NK_SC_F17;
            case 0x41: return NkScancode::NK_SC_NUMPAD_DOT;
            case 0x43: return NkScancode::NK_SC_NUMPAD_MUL;
            case 0x45: return NkScancode::NK_SC_NUMPAD_ADD;
            case 0x47: return NkScancode::NK_SC_NUM_LOCK; // Clear
            case 0x48: return NkScancode::NK_SC_VOLUME_UP;
            case 0x49: return NkScancode::NK_SC_VOLUME_DOWN;
            case 0x4A: return NkScancode::NK_SC_MUTE;
            case 0x4B: return NkScancode::NK_SC_NUMPAD_DIV;
            case 0x4C: return NkScancode::NK_SC_NUMPAD_ENTER;
            case 0x4E: return NkScancode::NK_SC_NUMPAD_SUB;
            case 0x4F: return NkScancode::NK_SC_F18;
            case 0x50: return NkScancode::NK_SC_F19;
            case 0x51: return NkScancode::NK_SC_NUMPAD_EQUALS;
            case 0x52: return NkScancode::NK_SC_NUMPAD_0;
            case 0x53: return NkScancode::NK_SC_NUMPAD_1;
            case 0x54: return NkScancode::NK_SC_NUMPAD_2;
            case 0x55: return NkScancode::NK_SC_NUMPAD_3;
            case 0x56: return NkScancode::NK_SC_NUMPAD_4;
            case 0x57: return NkScancode::NK_SC_NUMPAD_5;
            case 0x58: return NkScancode::NK_SC_NUMPAD_6;
            case 0x59: return NkScancode::NK_SC_NUMPAD_7;
            case 0x5A: return NkScancode::NK_SC_F20;
            case 0x5B: return NkScancode::NK_SC_NUMPAD_8;
            case 0x5C: return NkScancode::NK_SC_NUMPAD_9;
            case 0x60: return NkScancode::NK_SC_F5;
            case 0x61: return NkScancode::NK_SC_F6;
            case 0x62: return NkScancode::NK_SC_F7;
            case 0x63: return NkScancode::NK_SC_F3;
            case 0x64: return NkScancode::NK_SC_F8;
            case 0x65: return NkScancode::NK_SC_F9;
            case 0x67: return NkScancode::NK_SC_F11;
            case 0x69: return NkScancode::NK_SC_F13;
            case 0x6A: return NkScancode::NK_SC_F16;
            case 0x6B: return NkScancode::NK_SC_F14;
            case 0x6D: return NkScancode::NK_SC_F10;
            case 0x6F: return NkScancode::NK_SC_F12;
            case 0x71: return NkScancode::NK_SC_F15;
            case 0x72: return NkScancode::NK_SC_INSERT; // Help = Insert
            case 0x73: return NkScancode::NK_SC_HOME;
            case 0x74: return NkScancode::NK_SC_PAGE_UP;
            case 0x75: return NkScancode::NK_SC_DELETE;
            case 0x76: return NkScancode::NK_SC_F4;
            case 0x77: return NkScancode::NK_SC_END;
            case 0x78: return NkScancode::NK_SC_F2;
            case 0x79: return NkScancode::NK_SC_PAGE_DOWN;
            case 0x7A: return NkScancode::NK_SC_F1;
            case 0x7B: return NkScancode::NK_SC_LEFT;
            case 0x7C: return NkScancode::NK_SC_RIGHT;
            case 0x7D: return NkScancode::NK_SC_DOWN;
            case 0x7E: return NkScancode::NK_SC_UP;
            default:   return NkScancode::NK_SC_UNKNOWN;
        }
    }

    // =========================================================================
    // NkScancodeFromDOMCode — DOM KeyboardEvent.code → USB HID
    // Référence : https://www.w3.org/TR/uievents-code/
    // =========================================================================

    NkScancode NkScancodeFromDOMCode(const char* code) noexcept {
        if (!code || !code[0]) return NkScancode::NK_SC_UNKNOWN;

        // Table de correspondance DOM code → NkScancode
        struct Entry { const char* dom; NkScancode sc; };
        static const Entry kTable[] = {
            // Lettres
            {"KeyA", NkScancode::NK_SC_A}, {"KeyB", NkScancode::NK_SC_B},
            {"KeyC", NkScancode::NK_SC_C}, {"KeyD", NkScancode::NK_SC_D},
            {"KeyE", NkScancode::NK_SC_E}, {"KeyF", NkScancode::NK_SC_F},
            {"KeyG", NkScancode::NK_SC_G}, {"KeyH", NkScancode::NK_SC_H},
            {"KeyI", NkScancode::NK_SC_I}, {"KeyJ", NkScancode::NK_SC_J},
            {"KeyK", NkScancode::NK_SC_K}, {"KeyL", NkScancode::NK_SC_L},
            {"KeyM", NkScancode::NK_SC_M}, {"KeyN", NkScancode::NK_SC_N},
            {"KeyO", NkScancode::NK_SC_O}, {"KeyP", NkScancode::NK_SC_P},
            {"KeyQ", NkScancode::NK_SC_Q}, {"KeyR", NkScancode::NK_SC_R},
            {"KeyS", NkScancode::NK_SC_S}, {"KeyT", NkScancode::NK_SC_T},
            {"KeyU", NkScancode::NK_SC_U}, {"KeyV", NkScancode::NK_SC_V},
            {"KeyW", NkScancode::NK_SC_W}, {"KeyX", NkScancode::NK_SC_X},
            {"KeyY", NkScancode::NK_SC_Y}, {"KeyZ", NkScancode::NK_SC_Z},
            // Chiffres
            {"Digit1", NkScancode::NK_SC_1}, {"Digit2", NkScancode::NK_SC_2},
            {"Digit3", NkScancode::NK_SC_3}, {"Digit4", NkScancode::NK_SC_4},
            {"Digit5", NkScancode::NK_SC_5}, {"Digit6", NkScancode::NK_SC_6},
            {"Digit7", NkScancode::NK_SC_7}, {"Digit8", NkScancode::NK_SC_8},
            {"Digit9", NkScancode::NK_SC_9}, {"Digit0", NkScancode::NK_SC_0},
            // Spéciales
            {"Enter",        NkScancode::NK_SC_ENTER},
            {"Escape",       NkScancode::NK_SC_ESCAPE},
            {"Backspace",    NkScancode::NK_SC_BACKSPACE},
            {"Tab",          NkScancode::NK_SC_TAB},
            {"Space",        NkScancode::NK_SC_SPACE},
            {"Minus",        NkScancode::NK_SC_MINUS},
            {"Equal",        NkScancode::NK_SC_EQUALS},
            {"BracketLeft",  NkScancode::NK_SC_LBRACKET},
            {"BracketRight", NkScancode::NK_SC_RBRACKET},
            {"Backslash",    NkScancode::NK_SC_BACKSLASH},
            {"Semicolon",    NkScancode::NK_SC_SEMICOLON},
            {"Quote",        NkScancode::NK_SC_APOSTROPHE},
            {"Backquote",    NkScancode::NK_SC_GRAVE},
            {"Comma",        NkScancode::NK_SC_COMMA},
            {"Period",       NkScancode::NK_SC_PERIOD},
            {"Slash",        NkScancode::NK_SC_SLASH},
            {"CapsLock",     NkScancode::NK_SC_CAPS_LOCK},
            // Fonction
            {"F1",  NkScancode::NK_SC_F1},  {"F2",  NkScancode::NK_SC_F2},
            {"F3",  NkScancode::NK_SC_F3},  {"F4",  NkScancode::NK_SC_F4},
            {"F5",  NkScancode::NK_SC_F5},  {"F6",  NkScancode::NK_SC_F6},
            {"F7",  NkScancode::NK_SC_F7},  {"F8",  NkScancode::NK_SC_F8},
            {"F9",  NkScancode::NK_SC_F9},  {"F10", NkScancode::NK_SC_F10},
            {"F11", NkScancode::NK_SC_F11}, {"F12", NkScancode::NK_SC_F12},
            {"F13", NkScancode::NK_SC_F13}, {"F14", NkScancode::NK_SC_F14},
            {"F15", NkScancode::NK_SC_F15}, {"F16", NkScancode::NK_SC_F16},
            {"F17", NkScancode::NK_SC_F17}, {"F18", NkScancode::NK_SC_F18},
            {"F19", NkScancode::NK_SC_F19}, {"F20", NkScancode::NK_SC_F20},
            {"F21", NkScancode::NK_SC_F21}, {"F22", NkScancode::NK_SC_F22},
            {"F23", NkScancode::NK_SC_F23}, {"F24", NkScancode::NK_SC_F24},
            // Navigation
            {"PrintScreen",  NkScancode::NK_SC_PRINT_SCREEN},
            {"ScrollLock",   NkScancode::NK_SC_SCROLL_LOCK},
            {"Pause",        NkScancode::NK_SC_PAUSE},
            {"Insert",       NkScancode::NK_SC_INSERT},
            {"Home",         NkScancode::NK_SC_HOME},
            {"PageUp",       NkScancode::NK_SC_PAGE_UP},
            {"Delete",       NkScancode::NK_SC_DELETE},
            {"End",          NkScancode::NK_SC_END},
            {"PageDown",     NkScancode::NK_SC_PAGE_DOWN},
            {"ArrowRight",   NkScancode::NK_SC_RIGHT},
            {"ArrowLeft",    NkScancode::NK_SC_LEFT},
            {"ArrowDown",    NkScancode::NK_SC_DOWN},
            {"ArrowUp",      NkScancode::NK_SC_UP},
            // Pavé numérique
            {"NumLock",       NkScancode::NK_SC_NUM_LOCK},
            {"NumpadDivide",  NkScancode::NK_SC_NUMPAD_DIV},
            {"NumpadMultiply",NkScancode::NK_SC_NUMPAD_MUL},
            {"NumpadSubtract",NkScancode::NK_SC_NUMPAD_SUB},
            {"NumpadAdd",     NkScancode::NK_SC_NUMPAD_ADD},
            {"NumpadEnter",   NkScancode::NK_SC_NUMPAD_ENTER},
            {"NumpadDecimal", NkScancode::NK_SC_NUMPAD_DOT},
            {"Numpad0",       NkScancode::NK_SC_NUMPAD_0},
            {"Numpad1",       NkScancode::NK_SC_NUMPAD_1},
            {"Numpad2",       NkScancode::NK_SC_NUMPAD_2},
            {"Numpad3",       NkScancode::NK_SC_NUMPAD_3},
            {"Numpad4",       NkScancode::NK_SC_NUMPAD_4},
            {"Numpad5",       NkScancode::NK_SC_NUMPAD_5},
            {"Numpad6",       NkScancode::NK_SC_NUMPAD_6},
            {"Numpad7",       NkScancode::NK_SC_NUMPAD_7},
            {"Numpad8",       NkScancode::NK_SC_NUMPAD_8},
            {"Numpad9",       NkScancode::NK_SC_NUMPAD_9},
            {"NumpadEqual",   NkScancode::NK_SC_NUMPAD_EQUALS},
            // Modificateurs
            {"ShiftLeft",    NkScancode::NK_SC_LSHIFT},
            {"ShiftRight",   NkScancode::NK_SC_RSHIFT},
            {"ControlLeft",  NkScancode::NK_SC_LCTRL},
            {"ControlRight", NkScancode::NK_SC_RCTRL},
            {"AltLeft",      NkScancode::NK_SC_LALT},
            {"AltRight",     NkScancode::NK_SC_RALT},
            {"MetaLeft",     NkScancode::NK_SC_LSUPER},
            {"MetaRight",    NkScancode::NK_SC_RSUPER},
            {"OSLeft",       NkScancode::NK_SC_LSUPER},  // alias
            {"OSRight",      NkScancode::NK_SC_RSUPER},  // alias
            {"ContextMenu",  NkScancode::NK_SC_APPLICATION},
            // ISO
            {"IntlBackslash",NkScancode::NK_SC_NONUS_BACKSLASH},
            // Médias
            {"AudioVolumeMute",     NkScancode::NK_SC_MUTE},
            {"AudioVolumeUp",       NkScancode::NK_SC_VOLUME_UP},
            {"AudioVolumeDown",     NkScancode::NK_SC_VOLUME_DOWN},
            {"MediaPlayPause",      NkScancode::NK_SC_MEDIA_PLAY_PAUSE},
            {"MediaStop",           NkScancode::NK_SC_MEDIA_STOP},
            {"MediaTrackNext",      NkScancode::NK_SC_MEDIA_NEXT},
            {"MediaTrackPrevious",  NkScancode::NK_SC_MEDIA_PREV},
            // Sentinelle
            {nullptr, NkScancode::NK_SC_UNKNOWN}
        };

        for (const auto* e = kTable; e->dom != nullptr; ++e) {
            if (strcmp(code, e->dom) == 0) return e->sc;
        }
        return NkScancode::NK_SC_UNKNOWN;
    }

} // namespace nkentseu