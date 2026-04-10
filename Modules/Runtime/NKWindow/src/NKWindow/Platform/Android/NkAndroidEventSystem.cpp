// =============================================================================
// NkAndroidEventSystem.cpp
// Android implementation of platform-specific NkEventSystem methods.
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)

#include "NKEvent/NkEventSystem.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKEvent/NkTouchEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Platform/Android/NkAndroidDropTarget.h"
#include "NKWindow/Platform/Android/NkAndroidGamepad.h"
#include "NKWindow/Platform/Android/NkAndroidWindow.h"
#include "NKLogger/NkLog.h"

#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKContainers/Sequential/NkVector.h"

#include "NkAndroidEventSystem.h"

#include <android/input.h>
#include <android/keycodes.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <jni.h>

#define NKLOGD(...) logger.Debugf(__VA_ARGS__)

namespace nkentseu {
    using namespace math;

    extern android_app* nk_android_global_app;

    static NkEventSystem* gAndroidEventSystem = nullptr;
    static NkWindowId gFocusedWindowId = NK_INVALID_WINDOW_ID;

    template <typename Fn>
    static void ForEachAndroidWindow(Fn&& fn) {
        const nkentseu::NkVector<NkWindow*> windows = NkAndroidGetWindowsSnapshot();
        for (NkWindow* window : windows) {
            if (!window) {
                continue;
            }
            const NkWindowId id = window->GetId();
            if (id == NK_INVALID_WINDOW_ID) {
                continue;
            }
            fn(*window, id);
        }
    }

    static NkWindowId ResolveActiveWindowId() {
        if (gFocusedWindowId != NK_INVALID_WINDOW_ID) {
            if (NkAndroidFindWindowById(gFocusedWindowId) != nullptr) {
                return gFocusedWindowId;
            }
            gFocusedWindowId = NK_INVALID_WINDOW_ID;
        }

        if (NkWindow* last = NkAndroidGetLastWindow()) {
            const NkWindowId id = last->GetId();
            if (id != NK_INVALID_WINDOW_ID) {
                return id;
            }
        }

        const nkentseu::NkVector<NkWindow*> windows = NkAndroidGetWindowsSnapshot();
        for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
            NkWindow* window = *it;
            if (!window) {
                continue;
            }
            const NkWindowId id = window->GetId();
            if (id != NK_INVALID_WINDOW_ID) {
                return id;
            }
        }

        return NK_INVALID_WINDOW_ID;
    }

    static NkWindowId ResolveDropWindowId(NkWindowId requestedId) {
        if (requestedId != NK_INVALID_WINDOW_ID && NkAndroidFindWindowById(requestedId)) {
            return requestedId;
        }
        return ResolveActiveWindowId();
    }

    static void ForwardAndroidGamepadInput(AInputEvent* ev) {
        NkIGamepad* backend = NkSystem::Gamepads().GetBackend();
        auto* androidBackend = dynamic_cast<NkAndroidGamepad*>(backend);
        if (androidBackend) {
            androidBackend->OnInputEvent(ev);
        }
    }

    static void UpdateWindowNativeSurface(NkWindow& window, android_app* app, bool detachOnly = false) {
        if (window.mData.mExternal) {
            if (!window.mData.mAndroidApp && app) {
                window.mData.mAndroidApp = app;
            }
            window.mData.mPrevWidth = window.mData.mWidth;
            window.mData.mPrevHeight = window.mData.mHeight;
            if (window.mData.mNativeWindow) {
                window.mData.mWidth = static_cast<uint32>(ANativeWindow_getWidth(window.mData.mNativeWindow));
                window.mData.mHeight = static_cast<uint32>(ANativeWindow_getHeight(window.mData.mNativeWindow));
            } else {
                window.mData.mWidth = 0;
                window.mData.mHeight = 0;
            }
            return;
        }

        window.mData.mAndroidApp = app;

        ANativeWindow* nativeWindow = (app && !detachOnly) ? app->window : nullptr;
        if (window.mData.mNativeWindow != nativeWindow) {
            if (window.mData.mNativeWindow) {
                ANativeWindow_release(window.mData.mNativeWindow);
            }
            window.mData.mNativeWindow = nativeWindow;
            if (window.mData.mNativeWindow) {
                ANativeWindow_acquire(window.mData.mNativeWindow);
                ANativeWindow_setBuffersGeometry(window.mData.mNativeWindow, 0, 0, WINDOW_FORMAT_RGBA_8888);
            }
        }

        window.mData.mPrevWidth = window.mData.mWidth;
        window.mData.mPrevHeight = window.mData.mHeight;

        if (window.mData.mNativeWindow) {
            window.mData.mWidth = static_cast<uint32>(ANativeWindow_getWidth(window.mData.mNativeWindow));
            window.mData.mHeight = static_cast<uint32>(ANativeWindow_getHeight(window.mData.mNativeWindow));
        } else {
            window.mData.mWidth = 0;
            window.mData.mHeight = 0;
        }
    }

    static NkTouchPoint AndroidMotionToTouchPoint(AInputEvent* ev, int32_t idx, NkTouchPhase phase) {
        NkTouchPoint point;
        point.id = static_cast<uint64>(AMotionEvent_getPointerId(ev, static_cast<size_t>(idx)));
        point.phase = phase;
        point.clientX = AMotionEvent_getX(ev, static_cast<size_t>(idx));
        point.clientY = AMotionEvent_getY(ev, static_cast<size_t>(idx));
        point.screenX = AMotionEvent_getRawX(ev, static_cast<size_t>(idx));
        point.screenY = AMotionEvent_getRawY(ev, static_cast<size_t>(idx));
        return point;
    }

    static NkKey AKeyToNkKey(int32_t keycode) {
        switch (keycode) {
            case AKEYCODE_BACK:
            case AKEYCODE_ESCAPE:
                return NkKey::NK_ESCAPE;
            case AKEYCODE_DPAD_UP:
                return NkKey::NK_UP;
            case AKEYCODE_DPAD_DOWN:
                return NkKey::NK_DOWN;
            case AKEYCODE_DPAD_LEFT:
                return NkKey::NK_LEFT;
            case AKEYCODE_DPAD_RIGHT:
                return NkKey::NK_RIGHT;
            case AKEYCODE_ENTER:
            case AKEYCODE_NUMPAD_ENTER:
                return NkKey::NK_ENTER;
            case AKEYCODE_DEL:
                return NkKey::NK_BACK;
            case AKEYCODE_FORWARD_DEL:
                return NkKey::NK_DELETE;
            case AKEYCODE_TAB:
                return NkKey::NK_TAB;
            case AKEYCODE_SPACE:
                return NkKey::NK_SPACE;
            case AKEYCODE_HOME:
                return NkKey::NK_HOME;
            case AKEYCODE_MOVE_END:
                return NkKey::NK_END;
            case AKEYCODE_PAGE_UP:
                return NkKey::NK_PAGE_UP;
            case AKEYCODE_PAGE_DOWN:
                return NkKey::NK_PAGE_DOWN;
            case AKEYCODE_SHIFT_LEFT:
                return NkKey::NK_LSHIFT;
            case AKEYCODE_SHIFT_RIGHT:
                return NkKey::NK_RSHIFT;
            case AKEYCODE_CTRL_LEFT:
                return NkKey::NK_LCTRL;
            case AKEYCODE_CTRL_RIGHT:
                return NkKey::NK_RCTRL;
            case AKEYCODE_ALT_LEFT:
                return NkKey::NK_LALT;
            case AKEYCODE_ALT_RIGHT:
                return NkKey::NK_RALT;
            case AKEYCODE_A:
            case AKEYCODE_B:
            case AKEYCODE_C:
            case AKEYCODE_D:
            case AKEYCODE_E:
            case AKEYCODE_F:
            case AKEYCODE_G:
            case AKEYCODE_H:
            case AKEYCODE_I:
            case AKEYCODE_J:
            case AKEYCODE_K:
            case AKEYCODE_L:
            case AKEYCODE_M:
            case AKEYCODE_N:
            case AKEYCODE_O:
            case AKEYCODE_P:
            case AKEYCODE_Q:
            case AKEYCODE_R:
            case AKEYCODE_S:
            case AKEYCODE_T:
            case AKEYCODE_U:
            case AKEYCODE_V:
            case AKEYCODE_W:
            case AKEYCODE_X:
            case AKEYCODE_Y:
            case AKEYCODE_Z:
                return static_cast<NkKey>(
                    static_cast<int>(NkKey::NK_A) + (keycode - AKEYCODE_A));
            case AKEYCODE_0:
            case AKEYCODE_1:
            case AKEYCODE_2:
            case AKEYCODE_3:
            case AKEYCODE_4:
            case AKEYCODE_5:
            case AKEYCODE_6:
            case AKEYCODE_7:
            case AKEYCODE_8:
            case AKEYCODE_9:
                return static_cast<NkKey>(
                    static_cast<int>(NkKey::NK_NUM0) + (keycode - AKEYCODE_0));
            case AKEYCODE_F1:
                return NkKey::NK_F1;
            case AKEYCODE_F2:
                return NkKey::NK_F2;
            case AKEYCODE_F3:
                return NkKey::NK_F3;
            case AKEYCODE_F4:
                return NkKey::NK_F4;
            case AKEYCODE_F5:
                return NkKey::NK_F5;
            case AKEYCODE_F6:
                return NkKey::NK_F6;
            case AKEYCODE_F7:
                return NkKey::NK_F7;
            case AKEYCODE_F8:
                return NkKey::NK_F8;
            case AKEYCODE_F9:
                return NkKey::NK_F9;
            case AKEYCODE_F10:
                return NkKey::NK_F10;
            case AKEYCODE_F11:
                return NkKey::NK_F11;
            case AKEYCODE_F12:
                return NkKey::NK_F12;
            default:
                return NkKey::NK_UNKNOWN;
        }
    }

    static int32_t OnAndroidInputEvent(android_app* /*app*/, AInputEvent* ev) {
        if (!gAndroidEventSystem || !ev) {
            return 0;
        }

        const int32_t evType = AInputEvent_getType(ev);
        const int32_t source = AInputEvent_getSource(ev);

        const bool isGamepadSource =
            ((source & AINPUT_SOURCE_JOYSTICK) != 0) ||
            ((source & AINPUT_SOURCE_GAMEPAD) != 0);

        if (isGamepadSource) {
            ForwardAndroidGamepadInput(ev);
            return 1;
        }

        NkWindowId targetWinId = ResolveActiveWindowId();
        if (targetWinId != NK_INVALID_WINDOW_ID) {
            gFocusedWindowId = targetWinId;
        }

        if (evType == AINPUT_EVENT_TYPE_KEY) {
            const int32_t keycode = AKeyEvent_getKeyCode(ev);
            const int32_t action = AKeyEvent_getAction(ev);
            const NkKey key = AKeyToNkKey(keycode);
            bool handledKey = false;
            if (key != NkKey::NK_UNKNOWN) {
                NkModifierState mods{};
                const int32_t meta = AKeyEvent_getMetaState(ev);
                mods.shift = (meta & AMETA_SHIFT_ON) != 0;
                mods.ctrl = (meta & AMETA_CTRL_ON) != 0;
                mods.alt = (meta & AMETA_ALT_ON) != 0;

                if (action == AKEY_EVENT_ACTION_DOWN) {
                    NkKeyPressEvent event(key, NkScancode::NK_SC_UNKNOWN, mods, static_cast<uint32>(keycode));
                    gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                    handledKey = true;
                } else if (action == AKEY_EVENT_ACTION_UP) {
                    NkKeyReleaseEvent event(key, NkScancode::NK_SC_UNKNOWN, mods, static_cast<uint32>(keycode));
                    gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                    handledKey = true;
                }
            }

            if (keycode == AKEYCODE_BACK || keycode == AKEYCODE_ESCAPE) {
                return 1;
            }
            return handledKey ? 1 : 0;
        }

        if (evType == AINPUT_EVENT_TYPE_MOTION) {
            const int32_t action = AMotionEvent_getAction(ev);
            const int32_t act = action & AMOTION_EVENT_ACTION_MASK;
            const int32_t ptrIdx =
                (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
            const bool isTouchSource =
                (source & AINPUT_SOURCE_TOUCHSCREEN) == AINPUT_SOURCE_TOUCHSCREEN ||
                (source & AINPUT_SOURCE_TOUCHPAD) == AINPUT_SOURCE_TOUCHPAD;
            const bool isMouseSource =
                (source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE;

            NkTouchPoint points[NK_MAX_TOUCH_POINTS];
            uint32 count = 0;

            if (isTouchSource) {
                switch (act) {
                    case AMOTION_EVENT_ACTION_DOWN:
                    case AMOTION_EVENT_ACTION_POINTER_DOWN: {
                        points[0] = AndroidMotionToTouchPoint(ev, ptrIdx, NkTouchPhase::NK_TOUCH_PHASE_BEGAN);
                        NkTouchBeginEvent event(points, 1);
                        gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                        break;
                    }
                    case AMOTION_EVENT_ACTION_MOVE: {
                        const int32_t n = static_cast<int32_t>(AMotionEvent_getPointerCount(ev));
                        count = 0;
                        for (int32_t i = 0; i < n && count < NK_MAX_TOUCH_POINTS; ++i) {
                            points[count++] = AndroidMotionToTouchPoint(ev, i, NkTouchPhase::NK_TOUCH_PHASE_MOVED);
                        }
                        NkTouchMoveEvent event(points, count);
                        gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                        break;
                    }
                    case AMOTION_EVENT_ACTION_UP:
                    case AMOTION_EVENT_ACTION_POINTER_UP: {
                        points[0] = AndroidMotionToTouchPoint(ev, ptrIdx, NkTouchPhase::NK_TOUCH_PHASE_ENDED);
                        NkTouchEndEvent event(points, 1);
                        gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                        break;
                    }
                    case AMOTION_EVENT_ACTION_CANCEL: {
                        points[0] = AndroidMotionToTouchPoint(ev, 0, NkTouchPhase::NK_TOUCH_PHASE_CANCELLED);
                        NkTouchCancelEvent event(points, 1);
                        gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                        break;
                    }
                    default:
                        break;
                }
                return 1;
            }

            if (isMouseSource) {
                const float x = AMotionEvent_getX(ev, 0);
                const float y = AMotionEvent_getY(ev, 0);
                switch (act) {
                    case AMOTION_EVENT_ACTION_MOVE: {
                        NkMouseMoveEvent event(
                            static_cast<int32>(x),
                            static_cast<int32>(y),
                            static_cast<int32>(x),
                            static_cast<int32>(y),
                            0,
                            0);
                        gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                        break;
                    }
                    case AMOTION_EVENT_ACTION_DOWN: {
                        NkMouseButtonPressEvent event(
                            NkMouseButton::NK_MB_LEFT,
                            static_cast<int32>(x),
                            static_cast<int32>(y));
                        gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                        break;
                    }
                    case AMOTION_EVENT_ACTION_UP: {
                        NkMouseButtonReleaseEvent event(
                            NkMouseButton::NK_MB_LEFT,
                            static_cast<int32>(x),
                            static_cast<int32>(y));
                        gAndroidEventSystem->Enqueue_Public(event, targetWinId);
                        break;
                    }
                    default:
                        break;
                }
                return 1;
            }
        }

        return 0;
    }

    static void OnAndroidAppCmd(android_app* app, int32_t cmd) {
        if (!gAndroidEventSystem) {
            return;
        }

        switch (cmd) {
            case APP_CMD_INIT_WINDOW: {
                ForEachAndroidWindow([&](NkWindow& window, NkWindowId id) {
                    UpdateWindowNativeSurface(window, app);
                    NkWindowShownEvent event;
                    gAndroidEventSystem->Enqueue_Public(event, id);
                });
                gFocusedWindowId = ResolveActiveWindowId();
                break;
            }
            case APP_CMD_TERM_WINDOW: {
                ForEachAndroidWindow([&](NkWindow& window, NkWindowId id) {
                    UpdateWindowNativeSurface(window, app, true);
                    NkWindowHiddenEvent event;
                    gAndroidEventSystem->Enqueue_Public(event, id);
                });
                break;
            }
            case APP_CMD_GAINED_FOCUS: {
                gFocusedWindowId = ResolveActiveWindowId();
                ForEachAndroidWindow([&](NkWindow& /*window*/, NkWindowId id) {
                    NkWindowFocusGainedEvent event;
                    gAndroidEventSystem->Enqueue_Public(event, id);
                });
                break;
            }
            case APP_CMD_LOST_FOCUS: {
                ForEachAndroidWindow([&](NkWindow& /*window*/, NkWindowId id) {
                    NkWindowFocusLostEvent event;
                    gAndroidEventSystem->Enqueue_Public(event, id);
                });
                gFocusedWindowId = NK_INVALID_WINDOW_ID;
                break;
            }
            case APP_CMD_WINDOW_RESIZED: {
                ForEachAndroidWindow([&](NkWindow& window, NkWindowId id) {
                    const uint32 prevWidth = window.mData.mWidth;
                    const uint32 prevHeight = window.mData.mHeight;
                    UpdateWindowNativeSurface(window, app);
                    NkWindowResizeEvent event(
                        window.mData.mWidth,
                        window.mData.mHeight,
                        prevWidth,
                        prevHeight);
                    gAndroidEventSystem->Enqueue_Public(event, id);
                });
                break;
            }
            case APP_CMD_DESTROY: {
                ForEachAndroidWindow([&](NkWindow& /*window*/, NkWindowId id) {
                    NkWindowDestroyEvent event;
                    gAndroidEventSystem->Enqueue_Public(event, id);
                });
                break;
            }
            default:
                break;
        }
    }

    bool NkEventSystem::Init() {
        if (mReady) {
            return true;
        }

        mData = new NkEventSystemData;
        if (mData == nullptr) return false;

        mTotalEventCount = 0;
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
        }
        mPumping = false;

        android_app* app = nk_android_global_app;
        if (!app) {
            NKLOGD("NkEventSystem::Init() - nk_android_global_app is null");
            return false;
        }

        mData->mAndroidApp = app;
        gAndroidEventSystem = this;
        gFocusedWindowId = ResolveActiveWindowId();

        app->onAppCmd = OnAndroidAppCmd;
        app->onInputEvent = OnAndroidInputEvent;

        mReady = true;
        return true;
    }

    void NkEventSystem::Shutdown() {
        android_app* app = mData->mAndroidApp ? mData->mAndroidApp : nk_android_global_app;
        if (app) {
            if (app->onAppCmd == OnAndroidAppCmd) {
                app->onAppCmd = nullptr;
            }
            if (app->onInputEvent == OnAndroidInputEvent) {
                app->onInputEvent = nullptr;
            }
        }

        gAndroidEventSystem = nullptr;
        gFocusedWindowId = NK_INVALID_WINDOW_ID;
        mData->mAndroidApp = nullptr;

        ClearAllCallbacks();
        mHidMapper.Clear();
        {
            NkScopedSpinLock lock(mQueueMutex);
            mEventQueue.Clear();
            mCurrentEvent.Reset();
        }
        mWindowCallbacks.Clear();
        mTotalEventCount = 0;
        mPumping = false;
        mPumpThreadId = 0;
        mReady = false;

        delete mData;
        mData = nullptr;
    }

    void NkEventSystem::PumpOS() {
        if (mPumping) {
            return;
        }
        mPumping = true;

        android_app* app = mData->mAndroidApp ? mData->mAndroidApp : nk_android_global_app;
        if (!app) {
            mPumping = false;
            return;
        }

        if (!app->looper) {
            mPumping = false;
            return;
        }

        int events = 0;
        while (true) {
            android_poll_source* source = nullptr;
            const int pollResult = ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source));
            if (pollResult < 0) {
                break;
            }

            if (source) {
                source->process(app, source);
            }

            if (app->destroyRequested) {
                ForEachAndroidWindow([&](NkWindow& /*window*/, NkWindowId id) {
                    NkWindowCloseEvent event(false);
                    Enqueue(event, id);
                });
                break;
            }
        }

        mPumping = false;
    }

    const char* NkEventSystem::GetPlatformName() const noexcept {
        return "Android";
    }

    void NkEventSystem::Enqueue_Public(NkEvent& evt, NkWindowId winId) {
        Enqueue(evt, winId);
    }

    static NkString JStringToUtf8(JNIEnv* env, jstring value) {
        if (!env || !value) {
            return {};
        }

        const char* raw = env->GetStringUTFChars(value, nullptr);
        if (!raw) {
            return {};
        }

        NkString utf8(raw);
        env->ReleaseStringUTFChars(value, raw);
        return utf8;
    }

} // namespace nkentseu

extern "C" JNIEXPORT void JNICALL
Java_com_nkentseu_nkwindow_NkAndroidDropBridge_nativeDragEnter(
    JNIEnv* /*env*/, jclass /*clazz*/, jlong windowId, jfloat x, jfloat y,
    jint numItems, jboolean hasText, jboolean hasImage)
{
    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId(static_cast<nkentseu::NkWindowId>(windowId));
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    nkentseu::NkAndroidDropTarget::DispatchDragEnter(
        target,
        x,
        y,
        static_cast<nkentseu::uint32>(numItems < 0 ? 0 : numItems),
        hasText == JNI_TRUE,
        hasImage == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nkentseu_nkwindow_NkAndroidDropBridge_nativeDragOver(
    JNIEnv* /*env*/, jclass /*clazz*/, jlong windowId, jfloat x, jfloat y)
{
    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId(static_cast<nkentseu::NkWindowId>(windowId));
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    nkentseu::NkAndroidDropTarget::DispatchDragOver(target, x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nkentseu_nkwindow_NkAndroidDropBridge_nativeDragLeave(
    JNIEnv* /*env*/, jclass /*clazz*/, jlong windowId)
{
    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId(static_cast<nkentseu::NkWindowId>(windowId));
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    nkentseu::NkAndroidDropTarget::DispatchDragLeave(target);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nkentseu_nkwindow_NkAndroidDropBridge_nativeDropText(
    JNIEnv* env, jclass /*clazz*/, jlong windowId, jfloat x, jfloat y,
    jstring text, jstring mimeType)
{
    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId(static_cast<nkentseu::NkWindowId>(windowId));
    if (target == nkentseu::NK_INVALID_WINDOW_ID) {
        return;
    }

    const nkentseu::NkString textUtf8 = nkentseu::JStringToUtf8(env, text);
    const nkentseu::NkString mimeUtf8 = nkentseu::JStringToUtf8(env, mimeType);
    nkentseu::NkAndroidDropTarget::DispatchDropText(target, x, y, textUtf8, mimeUtf8);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nkentseu_nkwindow_NkAndroidDropBridge_nativeDropFiles(
    JNIEnv* env, jclass /*clazz*/, jlong windowId, jfloat x, jfloat y,
    jobjectArray paths)
{
    const nkentseu::NkWindowId target = nkentseu::ResolveDropWindowId(static_cast<nkentseu::NkWindowId>(windowId));
    if (target == nkentseu::NK_INVALID_WINDOW_ID || !env) {
        return;
    }

    nkentseu::NkVector<nkentseu::NkString> utf8Paths;
    if (paths) {
        const jsize count = env->GetArrayLength(paths);
        utf8Paths.Reserve(static_cast<size_t>(count));

        for (jsize i = 0; i < count; ++i) {
            jobject obj = env->GetObjectArrayElement(paths, i);
            jstring str = static_cast<jstring>(obj);
            utf8Paths.PushBack(nkentseu::JStringToUtf8(env, str));
            env->DeleteLocalRef(str);
        }
    }

    nkentseu::NkAndroidDropTarget::DispatchDropFiles(target, x, y, utf8Paths);
}

#endif // NKENTSEU_PLATFORM_ANDROID
