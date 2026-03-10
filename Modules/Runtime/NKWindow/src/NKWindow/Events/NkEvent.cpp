// =============================================================================
// NkEvent.cpp
// Implémentation de NkEvent, NkEventType et NkEventCategory.
// =============================================================================

#include "NkEvent.h"
#include "NKContainers/String/NkFormat.h"
#include <chrono>
#include <cstdio>

namespace nkentseu {

    // =========================================================================
    // NkEventCategory — ToString / FromString
    // =========================================================================

    NkString NkEventCategory::ToString(NkEventCategory::Value value) {
        if (value == NK_CAT_NONE)  return "NONE";
        if (value == NK_CAT_ALL)   return "ALL";

        NkString result;
        auto append = [&](NkEventCategory::Value flag, const char* name) {
            if (NkCategoryHas(value, flag)) {
                if (!result.Empty()) result += '|';
                result += name;
            }
        };
        append(NK_CAT_APPLICATION, "APPLICATION");
        append(NK_CAT_INPUT,       "INPUT");
        append(NK_CAT_KEYBOARD,    "KEYBOARD");
        append(NK_CAT_MOUSE,       "MOUSE");
        append(NK_CAT_WINDOW,      "WINDOW");
        append(NK_CAT_GRAPHICS,    "GRAPHICS");
        append(NK_CAT_TOUCH,       "TOUCH");
        append(NK_CAT_GAMEPAD,     "GAMEPAD");
        append(NK_CAT_CUSTOM,      "CUSTOM");
        append(NK_CAT_TRANSFER,    "TRANSFER");
        append(NK_CAT_GENERIC_HID, "GENERIC_HID");
        append(NK_CAT_DROP,        "DROP");
        append(NK_CAT_SYSTEM,      "SYSTEM");
        return result.Empty() ? "UNKNOWN" : result;
    }

    NkEventCategory::Value NkEventCategory::FromString(const NkString& str) {
        if (str == "NONE")        return NK_CAT_NONE;
        if (str == "APPLICATION") return NK_CAT_APPLICATION;
        if (str == "INPUT")       return NK_CAT_INPUT;
        if (str == "KEYBOARD")    return NK_CAT_KEYBOARD;
        if (str == "MOUSE")       return NK_CAT_MOUSE;
        if (str == "WINDOW")      return NK_CAT_WINDOW;
        if (str == "GRAPHICS")    return NK_CAT_GRAPHICS;
        if (str == "TOUCH")       return NK_CAT_TOUCH;
        if (str == "GAMEPAD")     return NK_CAT_GAMEPAD;
        if (str == "CUSTOM")      return NK_CAT_CUSTOM;
        if (str == "TRANSFER")    return NK_CAT_TRANSFER;
        if (str == "GENERIC_HID") return NK_CAT_GENERIC_HID;
        if (str == "DROP")        return NK_CAT_DROP;
        if (str == "SYSTEM")      return NK_CAT_SYSTEM;
        if (str == "ALL")         return NK_CAT_ALL;
        return NK_CAT_NONE;
    }

    // =========================================================================
    // NkEventType — ToString / FromString
    // =========================================================================

    NkString NkEventType::ToString(NkEventType::Value value) {
        switch (value) {
            case NK_NONE:                   return "NK_NONE";
            case NK_APP_LAUNCH:             return "NK_APP_LAUNCH";
            case NK_APP_TICK:               return "NK_APP_TICK";
            case NK_APP_UPDATE:             return "NK_APP_UPDATE";
            case NK_APP_RENDER:             return "NK_APP_RENDER";
            case NK_APP_CLOSE:              return "NK_APP_CLOSE";
            case NK_WINDOW_CREATE:          return "NK_WINDOW_CREATE";
            case NK_WINDOW_CLOSE:           return "NK_WINDOW_CLOSE";
            case NK_WINDOW_DESTROY:         return "NK_WINDOW_DESTROY";
            case NK_WINDOW_PAINT:           return "NK_WINDOW_PAINT";
            case NK_WINDOW_RESIZE:          return "NK_WINDOW_RESIZE";
            case NK_WINDOW_RESIZE_BEGIN:    return "NK_WINDOW_RESIZE_BEGIN";
            case NK_WINDOW_RESIZE_END:      return "NK_WINDOW_RESIZE_END";
            case NK_WINDOW_MOVE:            return "NK_WINDOW_MOVE";
            case NK_WINDOW_MOVE_BEGIN:      return "NK_WINDOW_MOVE_BEGIN";
            case NK_WINDOW_MOVE_END:        return "NK_WINDOW_MOVE_END";
            case NK_WINDOW_FOCUS_GAINED:    return "NK_WINDOW_FOCUS_GAINED";
            case NK_WINDOW_FOCUS_LOST:      return "NK_WINDOW_FOCUS_LOST";
            case NK_WINDOW_MINIMIZE:        return "NK_WINDOW_MINIMIZE";
            case NK_WINDOW_MAXIMIZE:        return "NK_WINDOW_MAXIMIZE";
            case NK_WINDOW_RESTORE:         return "NK_WINDOW_RESTORE";
            case NK_WINDOW_FULLSCREEN:      return "NK_WINDOW_FULLSCREEN";
            case NK_WINDOW_WINDOWED:        return "NK_WINDOW_WINDOWED";
            case NK_WINDOW_DPI_CHANGE:      return "NK_WINDOW_DPI_CHANGE";
            case NK_WINDOW_THEME_CHANGE:    return "NK_WINDOW_THEME_CHANGE";
            case NK_WINDOW_SHOWN:           return "NK_WINDOW_SHOWN";
            case NK_WINDOW_HIDDEN:          return "NK_WINDOW_HIDDEN";
            case NK_KEY_PRESSED:            return "NK_KEY_PRESSED";
            case NK_KEY_REPEATED:           return "NK_KEY_REPEATED";
            case NK_KEY_RELEASED:           return "NK_KEY_RELEASED";
            case NK_TEXT_INPUT:             return "NK_TEXT_INPUT";
            case NK_CHAR_ENTERED:           return "NK_CHAR_ENTERED";
            case NK_MOUSE_MOVE:             return "NK_MOUSE_MOVE";
            case NK_MOUSE_RAW:              return "NK_MOUSE_RAW";
            case NK_MOUSE_BUTTON_PRESSED:   return "NK_MOUSE_BUTTON_PRESSED";
            case NK_MOUSE_BUTTON_RELEASED:  return "NK_MOUSE_BUTTON_RELEASED";
            case NK_MOUSE_DOUBLE_CLICK:     return "NK_MOUSE_DOUBLE_CLICK";
            case NK_MOUSE_WHEEL_VERTICAL:   return "NK_MOUSE_WHEEL_VERTICAL";
            case NK_MOUSE_WHEEL_HORIZONTAL: return "NK_MOUSE_WHEEL_HORIZONTAL";
            case NK_MOUSE_ENTER:            return "NK_MOUSE_ENTER";
            case NK_MOUSE_LEAVE:            return "NK_MOUSE_LEAVE";
            case NK_MOUSE_WINDOW:           return "NK_MOUSE_WINDOW";
            case NK_MOUSE_WINDOW_ENTER:     return "NK_MOUSE_WINDOW_ENTER";
            case NK_MOUSE_WINDOW_LEAVE:     return "NK_MOUSE_WINDOW_LEAVE";
            case NK_MOUSE_CAPTURE_BEGIN:    return "NK_MOUSE_CAPTURE_BEGIN";
            case NK_MOUSE_CAPTURE_END:      return "NK_MOUSE_CAPTURE_END";
            case NK_GAMEPAD_CONNECT:        return "NK_GAMEPAD_CONNECT";
            case NK_GAMEPAD_DISCONNECT:     return "NK_GAMEPAD_DISCONNECT";
            case NK_GAMEPAD_BUTTON_PRESSED: return "NK_GAMEPAD_BUTTON_PRESSED";
            case NK_GAMEPAD_BUTTON_RELEASED:return "NK_GAMEPAD_BUTTON_RELEASED";
            case NK_GAMEPAD_AXIS_MOTION:    return "NK_GAMEPAD_AXIS_MOTION";
            case NK_GAMEPAD_RUMBLE:         return "NK_GAMEPAD_RUMBLE";
            case NK_GAMEPAD_STICK:          return "NK_GAMEPAD_STICK";
            case NK_GAMEPAD_TRIGGERED:      return "NK_GAMEPAD_TRIGGERED";
            case NK_GENERIC_CONNECT:        return "NK_GENERIC_CONNECT";
            case NK_GENERIC_DISCONNECT:     return "NK_GENERIC_DISCONNECT";
            case NK_GENERIC_BUTTON_PRESSED: return "NK_GENERIC_BUTTON_PRESSED";
            case NK_GENERIC_BUTTON_RELEASED:return "NK_GENERIC_BUTTON_RELEASED";
            case NK_GENERIC_AXIS_MOTION:    return "NK_GENERIC_AXIS_MOTION";
            case NK_GENERIC_RUMBLE:         return "NK_GENERIC_RUMBLE";
            case NK_GENERIC_STICK:          return "NK_GENERIC_STICK";
            case NK_GENERIC_TRIGGERED:      return "NK_GENERIC_TRIGGERED";
            case NK_TOUCH_BEGIN:            return "NK_TOUCH_BEGIN";
            case NK_TOUCH_MOVE:             return "NK_TOUCH_MOVE";
            case NK_TOUCH_END:              return "NK_TOUCH_END";
            case NK_TOUCH_CANCEL:           return "NK_TOUCH_CANCEL";
            case NK_TOUCH_PRESSED:          return "NK_TOUCH_PRESSED";
            case NK_TOUCH_RELEASED:         return "NK_TOUCH_RELEASED";
            case NK_GESTURE_PINCH:          return "NK_GESTURE_PINCH";
            case NK_GESTURE_ROTATE:         return "NK_GESTURE_ROTATE";
            case NK_GESTURE_PAN:            return "NK_GESTURE_PAN";
            case NK_GESTURE_SWIPE:          return "NK_GESTURE_SWIPE";
            case NK_GESTURE_TAP:            return "NK_GESTURE_TAP";
            case NK_GESTURE_LONG_PRESS:     return "NK_GESTURE_LONG_PRESS";
            case NK_DROP_FILE:              return "NK_DROP_FILE";
            case NK_DROP_TEXT:              return "NK_DROP_TEXT";
            case NK_DROP_IMAGE:             return "NK_DROP_IMAGE";
            case NK_DROP_ENTER:             return "NK_DROP_ENTER";
            case NK_DROP_OVER:              return "NK_DROP_OVER";
            case NK_DROP_LEAVE:             return "NK_DROP_LEAVE";
            case NK_SYSTEM_POWER:           return "NK_SYSTEM_POWER";
            case NK_SYSTEM_LOCALE:          return "NK_SYSTEM_LOCALE";
            case NK_SYSTEM_DISPLAY:         return "NK_SYSTEM_DISPLAY";
            case NK_SYSTEM_MEMORY:          return "NK_SYSTEM_MEMORY";
            case NK_TRANSFER:               return "NK_TRANSFER";
            case NK_CUSTOM:                 return "NK_CUSTOM";
            default:                        return "NK_UNKNOWN";
        }
    }

    NkEventType::Value NkEventType::FromString(const NkString& str) {
        if (str == "NK_APP_LAUNCH")              return NK_APP_LAUNCH;
        if (str == "NK_APP_TICK")                return NK_APP_TICK;
        if (str == "NK_APP_UPDATE")              return NK_APP_UPDATE;
        if (str == "NK_APP_RENDER")              return NK_APP_RENDER;
        if (str == "NK_APP_CLOSE")               return NK_APP_CLOSE;
        if (str == "NK_WINDOW_CREATE")           return NK_WINDOW_CREATE;
        if (str == "NK_WINDOW_CLOSE")            return NK_WINDOW_CLOSE;
        if (str == "NK_WINDOW_DESTROY")          return NK_WINDOW_DESTROY;
        if (str == "NK_WINDOW_PAINT")            return NK_WINDOW_PAINT;
        if (str == "NK_WINDOW_RESIZE")           return NK_WINDOW_RESIZE;
        if (str == "NK_WINDOW_RESIZE_BEGIN")     return NK_WINDOW_RESIZE_BEGIN;
        if (str == "NK_WINDOW_RESIZE_END")       return NK_WINDOW_RESIZE_END;
        if (str == "NK_WINDOW_MOVE")             return NK_WINDOW_MOVE;
        if (str == "NK_WINDOW_MOVE_BEGIN")       return NK_WINDOW_MOVE_BEGIN;
        if (str == "NK_WINDOW_MOVE_END")         return NK_WINDOW_MOVE_END;
        if (str == "NK_WINDOW_FOCUS_GAINED")     return NK_WINDOW_FOCUS_GAINED;
        if (str == "NK_WINDOW_FOCUS_LOST")       return NK_WINDOW_FOCUS_LOST;
        if (str == "NK_WINDOW_MINIMIZE")         return NK_WINDOW_MINIMIZE;
        if (str == "NK_WINDOW_MAXIMIZE")         return NK_WINDOW_MAXIMIZE;
        if (str == "NK_WINDOW_RESTORE")          return NK_WINDOW_RESTORE;
        if (str == "NK_WINDOW_FULLSCREEN")       return NK_WINDOW_FULLSCREEN;
        if (str == "NK_WINDOW_WINDOWED")         return NK_WINDOW_WINDOWED;
        if (str == "NK_WINDOW_DPI_CHANGE")       return NK_WINDOW_DPI_CHANGE;
        if (str == "NK_WINDOW_THEME_CHANGE")     return NK_WINDOW_THEME_CHANGE;
        if (str == "NK_WINDOW_SHOWN")            return NK_WINDOW_SHOWN;
        if (str == "NK_WINDOW_HIDDEN")           return NK_WINDOW_HIDDEN;
        if (str == "NK_KEY_PRESSED")             return NK_KEY_PRESSED;
        if (str == "NK_KEY_REPEATED")            return NK_KEY_REPEATED;
        if (str == "NK_KEY_RELEASED")            return NK_KEY_RELEASED;
        if (str == "NK_TEXT_INPUT")              return NK_TEXT_INPUT;
        if (str == "NK_CHAR_ENTERED")            return NK_CHAR_ENTERED;
        if (str == "NK_MOUSE_MOVE")              return NK_MOUSE_MOVE;
        if (str == "NK_MOUSE_RAW")               return NK_MOUSE_RAW;
        if (str == "NK_MOUSE_BUTTON_PRESSED")    return NK_MOUSE_BUTTON_PRESSED;
        if (str == "NK_MOUSE_BUTTON_RELEASED")   return NK_MOUSE_BUTTON_RELEASED;
        if (str == "NK_MOUSE_DOUBLE_CLICK")      return NK_MOUSE_DOUBLE_CLICK;
        if (str == "NK_MOUSE_WHEEL_VERTICAL")    return NK_MOUSE_WHEEL_VERTICAL;
        if (str == "NK_MOUSE_WHEEL_HORIZONTAL")  return NK_MOUSE_WHEEL_HORIZONTAL;
        if (str == "NK_MOUSE_ENTER")             return NK_MOUSE_ENTER;
        if (str == "NK_MOUSE_LEAVE")             return NK_MOUSE_LEAVE;
        if (str == "NK_MOUSE_WINDOW")            return NK_MOUSE_WINDOW;
        if (str == "NK_MOUSE_WINDOW_ENTER")      return NK_MOUSE_WINDOW_ENTER;
        if (str == "NK_MOUSE_WINDOW_LEAVE")      return NK_MOUSE_WINDOW_LEAVE;
        if (str == "NK_MOUSE_CAPTURE_BEGIN")     return NK_MOUSE_CAPTURE_BEGIN;
        if (str == "NK_MOUSE_CAPTURE_END")       return NK_MOUSE_CAPTURE_END;
        if (str == "NK_TRANSFER")                return NK_TRANSFER;
        if (str == "NK_CUSTOM")                  return NK_CUSTOM;
        return NK_NONE;
    }

    // =========================================================================
    // NkEvent — implémentation
    // =========================================================================

    NkTimestampMs NkEvent::GetCurrentTimestamp() {
        using namespace std::chrono;
        return static_cast<NkTimestampMs>(
            duration_cast<milliseconds>(
                steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    NkEvent::NkEvent(NkU64 windowId)
        : mWindowID(windowId)
        , mTimestamp(GetCurrentTimestamp())
        , mHandled(false)
    {}

    NkEvent::NkEvent()
        : mWindowID(0)
        , mTimestamp(GetCurrentTimestamp())
        , mHandled(false)
    {}

    NkEvent::NkEvent(const NkEvent& other)
        : mWindowID(other.mWindowID)
        , mTimestamp(other.mTimestamp)
        , mHandled(other.mHandled)
    {}

    // --- Implémentations virtuelles par défaut ---

    NkEventType::Value NkEvent::GetType() const {
        return NkEventType::NK_NONE;
    }

    const char* NkEvent::GetName() const {
        return "NkEvent";
    }

    const char* NkEvent::GetTypeStr() const {
        return "NK_NONE";
    }

    NkU32 NkEvent::GetCategoryFlags() const {
        return static_cast<NkU32>(NkEventCategory::NK_CAT_NONE);
    }

    NkEvent* NkEvent::Clone() const {
        return new NkEvent(*this);
    }

    NkString NkEvent::ToString() const {
        char tsBuffer[32];
        char windowBuffer[32];
        ::snprintf(tsBuffer, sizeof(tsBuffer), "%llu", static_cast<unsigned long long>(mTimestamp));
        ::snprintf(windowBuffer, sizeof(windowBuffer), "%llu", static_cast<unsigned long long>(mWindowID));

        NkString out("[NkEvent type=");
        out.Append(GetTypeStr() ? GetTypeStr() : "NK_NONE");
        out.Append(" cat=");
        out.Append(NkEventCategory::ToString(GetCategory()));
        out.Append(" ts=");
        out.Append(tsBuffer);
        out.Append(" window=");
        out.Append(windowBuffer);
        out.Append(" handled=");
        out.Append(mHandled ? "true" : "false");
        out.Append("]");
        return out;
    }

    NkString NkEvent::TypeToString(NkEventType::Value type) {
        return NkEventType::ToString(type);
    }

    NkString NkEvent::CategoryToString(NkEventCategory::Value category) {
        return NkEventCategory::ToString(category);
    }

} // namespace nkentseu
