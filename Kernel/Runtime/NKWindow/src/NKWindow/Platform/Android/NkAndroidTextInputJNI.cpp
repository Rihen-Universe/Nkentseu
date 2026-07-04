// =============================================================================
// NkAndroidTextInputJNI.cpp
// Implémentation native des méthodes de com.nkentseu.window.NkNativeActivity.
// Reçoit le texte composé par l'IME (via InputConnection) et le convertit en
// NkTextInputEvent / NkKeyPressEvent, poussés dans le système d'événements.
//
// Actif seulement si l'app inclut les .java (androidjavafiles) et déclare
// androidactivityclass("com.nkentseu.window.NkNativeActivity"). Sinon ces
// symboles ne sont jamais appelés (le backend retombe sur KeyCharacterMap).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)

#include <jni.h>
#include <android/keycodes.h>
#include <android/input.h>
#include <cstdint>

#include "NKWindow/Platform/Android/NkAndroidWindow.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"

namespace {
    using namespace nkentseu;

    // Fenêtre destinataire des événements clavier logiciel. Sur mobile il n'y a
    // en pratique qu'une fenêtre ; on prend la dernière enregistrée.
    NkWindowId TargetWindowId() {
        NkWindow* w = NkAndroidGetLastWindow();
        return w ? w->GetId() : NK_INVALID_WINDOW_ID;
    }

    void EmitKey(NkKey key, NkScancode sc, jint action, NkWindowId win) {
        if (win == NK_INVALID_WINDOW_ID) {
            return;
        }
        if (action == AKEY_EVENT_ACTION_DOWN) {
            NkKeyPressEvent e(key, sc, {}, 0, false, win);
            NkWESystem::Events().Enqueue_Public(e, win);
        } else if (action == AKEY_EVENT_ACTION_UP) {
            NkKeyReleaseEvent e(key, sc, {}, 0, false, win);
            NkWESystem::Events().Enqueue_Public(e, win);
        }
    }
} // namespace

extern "C" {

// public static native void nkCommitText(String text);
JNIEXPORT void JNICALL
Java_com_nkentseu_window_NkNativeActivity_nkCommitText(JNIEnv* env, jclass, jstring text) {
    if (!text) {
        return;
    }
    const nkentseu::NkWindowId win = TargetWindowId();
    if (win == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }
    const jsize len = env->GetStringLength(text);
    if (len <= 0) {
        return;
    }
    const jchar* u16 = env->GetStringChars(text, nullptr);
    if (!u16) {
        return;
    }

    for (jsize i = 0; i < len; ++i) {
        uint32_t cp = static_cast<uint32_t>(u16[i]);
        // Recombine les paires de substitution UTF-16 en point de code.
        if (cp >= 0xD800 && cp <= 0xDBFF && (i + 1) < len) {
            const uint32_t lo = static_cast<uint32_t>(u16[i + 1]);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                ++i;
            }
        }
        if (cp == '\n' || cp == '\r') {
            EmitKey(nkentseu::NkKey::NK_ENTER, nkentseu::NkScancode::NK_SC_ENTER,
                    AKEY_EVENT_ACTION_DOWN, win);
            EmitKey(nkentseu::NkKey::NK_ENTER, nkentseu::NkScancode::NK_SC_ENTER,
                    AKEY_EVENT_ACTION_UP, win);
            continue;
        }
        if (cp < 0x20 || cp == 0x7F) {
            continue;
        }
        nkentseu::NkTextInputEvent evt(cp, win);
        nkentseu::NkWESystem::Events().Enqueue_Public(evt, win);
    }
    env->ReleaseStringChars(text, u16);
}

// public static native void nkKeyEvent(int keyCode, int action, int metaState);
JNIEXPORT void JNICALL
Java_com_nkentseu_window_NkNativeActivity_nkKeyEvent(JNIEnv*, jclass,
                                                     jint keyCode, jint action, jint /*metaState*/) {
    const nkentseu::NkWindowId win = TargetWindowId();
    if (win == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }
    nkentseu::NkKey key = nkentseu::NkKey::NK_UNKNOWN;
    nkentseu::NkScancode sc = nkentseu::NkScancode::NK_SC_UNKNOWN;
    switch (keyCode) {
        case AKEYCODE_DEL:         key = nkentseu::NkKey::NK_BACK;   sc = nkentseu::NkScancode::NK_SC_BACKSPACE; break;
        case AKEYCODE_FORWARD_DEL: key = nkentseu::NkKey::NK_DELETE; sc = nkentseu::NkScancode::NK_SC_DELETE;   break;
        case AKEYCODE_ENTER:
        case AKEYCODE_NUMPAD_ENTER: key = nkentseu::NkKey::NK_ENTER; sc = nkentseu::NkScancode::NK_SC_ENTER;   break;
        case AKEYCODE_TAB:         key = nkentseu::NkKey::NK_TAB;    sc = nkentseu::NkScancode::NK_SC_TAB;      break;
        case AKEYCODE_DPAD_LEFT:   key = nkentseu::NkKey::NK_LEFT;   break;
        case AKEYCODE_DPAD_RIGHT:  key = nkentseu::NkKey::NK_RIGHT;  break;
        case AKEYCODE_DPAD_UP:     key = nkentseu::NkKey::NK_UP;     break;
        case AKEYCODE_DPAD_DOWN:   key = nkentseu::NkKey::NK_DOWN;   break;
        default: return;
    }
    EmitKey(key, sc, action, win);
}

// Ancre anti-GC : ces natives ne sont appelées QUE depuis Java (aucun référent
// C++), donc le linker les élimine du .so (--gc-sections) et l'appel Java échoue
// avec UnsatisfiedLinkError. On les référence ici ; NkWindow::Create (Android)
// appelle NkAndroidImeJniAnchor() pour forcer la conservation de ce fichier.
JNIEXPORT void* NkAndroidImeJniAnchor() {
    static void* const anchor[] = {
        reinterpret_cast<void*>(&Java_com_nkentseu_window_NkNativeActivity_nkCommitText),
        reinterpret_cast<void*>(&Java_com_nkentseu_window_NkNativeActivity_nkKeyEvent),
    };
    return anchor[0];
}

} // extern "C"

#endif // NKENTSEU_PLATFORM_ANDROID
