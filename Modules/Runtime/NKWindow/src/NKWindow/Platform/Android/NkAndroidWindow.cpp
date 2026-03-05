// =============================================================================
// NkAndroidWindow.cpp - NkWindow implementation for Android
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)

#include "NKWindow/Platform/Android/NkAndroidWindow.h"
#include "NKWindow/Platform/Android/NkAndroidDropTarget.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKWindow/Events/NkEventSystem.h"

#include <android/configuration.h>
#include <android/native_window.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <jni.h>
#include <algorithm>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace nkentseu {

    android_app* nk_android_global_app = nullptr;
    static std::mutex sAndroidWindowsMutex;
    static std::vector<NkWindow*> sAndroidWindows;
    static std::unordered_map<NkWindowId, NkWindow*> sAndroidWindowById;
    static NkWindow* sAndroidLastWindow = nullptr;

    NkWindow* NkAndroidFindWindowById(NkWindowId id) {
        std::lock_guard<std::mutex> lock(sAndroidWindowsMutex);
        auto it = sAndroidWindowById.find(id);
        return (it != sAndroidWindowById.end()) ? it->second : nullptr;
    }

    std::vector<NkWindow*> NkAndroidGetWindowsSnapshot() {
        std::lock_guard<std::mutex> lock(sAndroidWindowsMutex);
        return sAndroidWindows;
    }

    NkWindow* NkAndroidGetLastWindow() {
        std::lock_guard<std::mutex> lock(sAndroidWindowsMutex);
        return sAndroidLastWindow;
    }

    void NkAndroidRegisterWindow(NkWindow* window) {
        if (!window) {
            return;
        }

        const NkWindowId id = window->GetId();
        if (id == NK_INVALID_WINDOW_ID) {
            return;
        }

        std::lock_guard<std::mutex> lock(sAndroidWindowsMutex);
        if (std::find(sAndroidWindows.begin(), sAndroidWindows.end(), window) == sAndroidWindows.end()) {
            sAndroidWindows.push_back(window);
        }
        sAndroidWindowById[id] = window;
        sAndroidLastWindow = window;
    }

    void NkAndroidUnregisterWindow(NkWindow* window) {
        if (!window) {
            return;
        }

        std::lock_guard<std::mutex> lock(sAndroidWindowsMutex);

        sAndroidWindows.erase(
            std::remove(sAndroidWindows.begin(), sAndroidWindows.end(), window),
            sAndroidWindows.end());

        NkWindowId staleId = NK_INVALID_WINDOW_ID;
        for (const auto& kv : sAndroidWindowById) {
            if (kv.second == window) {
                staleId = kv.first;
                break;
            }
        }
        if (staleId != NK_INVALID_WINDOW_ID) {
            sAndroidWindowById.erase(staleId);
        }

        if (sAndroidLastWindow == window) {
            sAndroidLastWindow = sAndroidWindows.empty() ? nullptr : sAndroidWindows.back();
        }
    }

    static bool NkAndroidAcquireJniEnv(android_app* app, JNIEnv** outEnv, bool* outAttached) {
        if (!app || !app->activity || !app->activity->vm || !outEnv || !outAttached) {
            return false;
        }

        *outEnv = nullptr;
        *outAttached = false;

        JavaVM* vm = app->activity->vm;
        if (vm->GetEnv(reinterpret_cast<void**>(outEnv), JNI_VERSION_1_6) == JNI_OK) {
            return *outEnv != nullptr;
        }

        if (vm->AttachCurrentThread(outEnv, nullptr) != JNI_OK || !*outEnv) {
            return false;
        }

        *outAttached = true;
        return true;
    }

    static void NkAndroidReleaseJniEnv(android_app* app, bool attached) {
        if (!attached || !app || !app->activity || !app->activity->vm) {
            return;
        }
        app->activity->vm->DetachCurrentThread();
    }

    static void NkAndroidUpdateSafeArea(NkWindow& window) {
        window.mData.mSafeArea = {};

        android_app* app = window.mData.mAndroidApp;
        if (!app || !app->activity || !app->activity->clazz) {
            return;
        }

        JNIEnv* env = nullptr;
        bool attached = false;
        if (!NkAndroidAcquireJniEnv(app, &env, &attached)) {
            return;
        }

        jobject activity = app->activity->clazz;
        jclass actClass = env->GetObjectClass(activity);
        if (!actClass) {
            NkAndroidReleaseJniEnv(app, attached);
            return;
        }

        jobject windowObj = nullptr;
        jobject decorView = nullptr;
        jobject insetsObj = nullptr;
        jclass insetsClass = nullptr;

        do {
            jmethodID getWindow = env->GetMethodID(actClass, "getWindow", "()Landroid/view/Window;");
            if (!getWindow) {
                break;
            }

            windowObj = env->CallObjectMethod(activity, getWindow);
            if (!windowObj || env->ExceptionCheck()) {
                env->ExceptionClear();
                break;
            }

            jclass windowClass = env->GetObjectClass(windowObj);
            if (!windowClass) {
                break;
            }

            jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
            env->DeleteLocalRef(windowClass);
            if (!getDecorView) {
                break;
            }

            decorView = env->CallObjectMethod(windowObj, getDecorView);
            if (!decorView || env->ExceptionCheck()) {
                env->ExceptionClear();
                break;
            }

            jclass viewClass = env->GetObjectClass(decorView);
            if (!viewClass) {
                break;
            }

            jmethodID getRootInsets = env->GetMethodID(viewClass, "getRootWindowInsets", "()Landroid/view/WindowInsets;");
            env->DeleteLocalRef(viewClass);
            if (!getRootInsets) {
                break;
            }

            insetsObj = env->CallObjectMethod(decorView, getRootInsets);
            if (!insetsObj || env->ExceptionCheck()) {
                env->ExceptionClear();
                break;
            }

            insetsClass = env->GetObjectClass(insetsObj);
            if (!insetsClass) {
                break;
            }

            auto getInset = [&](const char* methodName) -> float {
                jmethodID method = env->GetMethodID(insetsClass, methodName, "()I");
                if (!method) {
                    return 0.0f;
                }
                jint value = env->CallIntMethod(insetsObj, method);
                if (env->ExceptionCheck()) {
                    env->ExceptionClear();
                    return 0.0f;
                }
                return static_cast<float>(value);
            };

            window.mData.mSafeArea.top = getInset("getSystemWindowInsetTop");
            window.mData.mSafeArea.bottom = getInset("getSystemWindowInsetBottom");
            window.mData.mSafeArea.left = getInset("getSystemWindowInsetLeft");
            window.mData.mSafeArea.right = getInset("getSystemWindowInsetRight");
        } while (false);

        if (insetsClass) {
            env->DeleteLocalRef(insetsClass);
        }
        if (insetsObj) {
            env->DeleteLocalRef(insetsObj);
        }
        if (decorView) {
            env->DeleteLocalRef(decorView);
        }
        if (windowObj) {
            env->DeleteLocalRef(windowObj);
        }
        env->DeleteLocalRef(actClass);
        NkAndroidReleaseJniEnv(app, attached);
    }

    static bool NkAndroidApplyOrientation(NkWindow& window, NkScreenOrientation orientation) {
        android_app* app = window.mData.mAndroidApp;
        if (!app || !app->activity || !app->activity->clazz) {
            return false;
        }

        JNIEnv* env = nullptr;
        bool attached = false;
        if (!NkAndroidAcquireJniEnv(app, &env, &attached)) {
            return false;
        }

        bool ok = false;
        jobject activity = app->activity->clazz;
        jclass actClass = env->GetObjectClass(activity);
        if (actClass) {
            jmethodID setRequestedOrientation =
                env->GetMethodID(actClass, "setRequestedOrientation", "(I)V");
            if (setRequestedOrientation) {
                jint requested = 10; // ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR
                if (orientation == NkScreenOrientation::NK_SCREEN_ORIENTATION_PORTRAIT) {
                    requested = 1; // SCREEN_ORIENTATION_PORTRAIT
                } else if (orientation == NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE) {
                    requested = 0; // SCREEN_ORIENTATION_LANDSCAPE
                }

                env->CallVoidMethod(activity, setRequestedOrientation, requested);
                if (!env->ExceptionCheck()) {
                    ok = true;
                } else {
                    env->ExceptionClear();
                }
            }
            env->DeleteLocalRef(actClass);
        }

        NkAndroidReleaseJniEnv(app, attached);
        return ok;
    }

    NkWindow::NkWindow() = default;

    NkWindow::NkWindow(const NkWindowConfig& config) {
        Create(config);
    }

    NkWindow::~NkWindow() {
        if (mIsOpen) {
            Close();
        }
    }

    bool NkWindow::Create(const NkWindowConfig& config) {
        mConfig = config;
        mData.mAndroidApp = nk_android_global_app;

        if (!mData.mAndroidApp) {
            mLastError = NkError(1, "Android: android_app is null");
            return false;
        }
        if (!mData.mAndroidApp->window) {
            mLastError = NkError(2, "Android: ANativeWindow is null");
            return false;
        }

        mData.mNativeWindow = mData.mAndroidApp->window;
        ANativeWindow_acquire(mData.mNativeWindow);
        ANativeWindow_setBuffersGeometry(mData.mNativeWindow, 0, 0, WINDOW_FORMAT_RGBA_8888);

        mData.mWidth = static_cast<NkU32>(ANativeWindow_getWidth(mData.mNativeWindow));
        mData.mHeight = static_cast<NkU32>(ANativeWindow_getHeight(mData.mNativeWindow));
        mData.mPrevWidth = mData.mWidth;
        mData.mPrevHeight = mData.mHeight;

        if (mData.mAConfig) {
            AConfiguration_delete(mData.mAConfig);
            mData.mAConfig = nullptr;
        }
        mData.mAConfig = AConfiguration_new();
        if (mData.mAConfig && mData.mAndroidApp->activity && mData.mAndroidApp->activity->assetManager) {
            AConfiguration_fromAssetManager(mData.mAConfig, mData.mAndroidApp->activity->assetManager);
        }

        mData.mOrientation = config.screenOrientation;
        NkAndroidApplyOrientation(*this, mData.mOrientation);
        NkAndroidUpdateSafeArea(*this);

        mId = NkSystem::Instance().RegisterWindow(this);
        NkAndroidRegisterWindow(this);

        mData.mDropTarget = new NkAndroidDropTarget(mId);
        if (mData.mDropTarget) {
            mData.mDropTarget->SetDropEnterCallback([this](const NkDropEnterEvent& event) {
                NkDropEnterEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropOverCallback([this](const NkDropOverEvent& event) {
                NkDropOverEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropLeaveCallback([this](const NkDropLeaveEvent& event) {
                NkDropLeaveEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropFileCallback([this](const NkDropFileEvent& event) {
                NkDropFileEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropTextCallback([this](const NkDropTextEvent& event) {
                NkDropTextEvent copy(event);
                NkSystem::Events().Enqueue_Public(copy, mId);
            });
        }

        mIsOpen = true;

        NkWindowCreateEvent e(mData.mWidth, mData.mHeight);
        NkSystem::Events().Enqueue_Public(e, mId);
        return true;
    }

    void NkWindow::Close() {
        if (!mIsOpen) {
            return;
        }
        mIsOpen = false;

        NkAndroidUnregisterWindow(this);
        NkSystem::Instance().UnregisterWindow(mId);
        mId = NK_INVALID_WINDOW_ID;

        if (mData.mDropTarget) {
            delete mData.mDropTarget;
            mData.mDropTarget = nullptr;
        }

        if (mData.mAConfig) {
            AConfiguration_delete(mData.mAConfig);
            mData.mAConfig = nullptr;
        }

        if (mData.mNativeWindow) {
            ANativeWindow_release(mData.mNativeWindow);
            mData.mNativeWindow = nullptr;
        }

        mData.mWidth = 0;
        mData.mHeight = 0;
        mData.mPrevWidth = 0;
        mData.mPrevHeight = 0;
        mData.mSafeArea = {};
        mData.mAndroidApp = nullptr;
    }

    bool NkWindow::IsOpen() const { return mIsOpen; }

    bool NkWindow::IsValid() const { return mIsOpen && mData.mNativeWindow != nullptr; }

    NkError NkWindow::GetLastError() const { return mLastError; }

    NkWindowConfig NkWindow::GetConfig() const { return mConfig; }

    std::string NkWindow::GetTitle() const { return mConfig.title; }

    NkVec2u NkWindow::GetSize() const {
        if (mData.mNativeWindow) {
            return {
                static_cast<NkU32>(ANativeWindow_getWidth(mData.mNativeWindow)),
                static_cast<NkU32>(ANativeWindow_getHeight(mData.mNativeWindow)),
            };
        }
        return {mConfig.width, mConfig.height};
    }

    NkVec2u NkWindow::GetPosition() const { return {0, 0}; }

    float NkWindow::GetDpiScale() const {
        android_app* app = mData.mAndroidApp ? mData.mAndroidApp : nk_android_global_app;
        if (!app || !app->activity || !app->activity->assetManager) {
            return 1.0f;
        }

        AConfiguration* config = AConfiguration_new();
        if (!config) {
            return 1.0f;
        }
        AConfiguration_fromAssetManager(config, app->activity->assetManager);
        const int32_t density = AConfiguration_getDensity(config);
        AConfiguration_delete(config);

        if (density <= 0) {
            return 1.0f;
        }
        return static_cast<float>(density) / 160.0f;
    }

    NkVec2u NkWindow::GetDisplaySize() const { return GetSize(); }

    NkVec2u NkWindow::GetDisplayPosition() const { return {0, 0}; }

    void NkWindow::SetTitle(const std::string& title) { mConfig.title = title; }

    void NkWindow::SetSize(NkU32, NkU32) {}

    void NkWindow::SetPosition(NkI32, NkI32) {}

    void NkWindow::SetVisible(bool) {}

    void NkWindow::Minimize() {}

    void NkWindow::Maximize() {}

    void NkWindow::Restore() {}

    void NkWindow::SetFullscreen(bool fullscreen) {
        mConfig.fullscreen = fullscreen;
    }

    bool NkWindow::SupportsOrientationControl() const { return true; }

    void NkWindow::SetScreenOrientation(NkScreenOrientation orientation) {
        if (NkAndroidApplyOrientation(*this, orientation)) {
            mData.mOrientation = orientation;
            mConfig.screenOrientation = orientation;
        }
    }

    NkScreenOrientation NkWindow::GetScreenOrientation() const {
        return mData.mOrientation;
    }

    void NkWindow::SetAutoRotateEnabled(bool enabled) {
        if (enabled) {
            SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO);
            return;
        }

        NkVec2u size = GetSize();
        const bool isLandscape = size.x >= size.y;
        SetScreenOrientation(
            isLandscape
                ? NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE
                : NkScreenOrientation::NK_SCREEN_ORIENTATION_PORTRAIT);
    }

    bool NkWindow::IsAutoRotateEnabled() const {
        return mData.mOrientation == NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO;
    }

    void NkWindow::SetMousePosition(NkU32, NkU32) {}

    void NkWindow::ShowMouse(bool) {}

    void NkWindow::CaptureMouse(bool) {}

    void NkWindow::SetWebInputOptions(const NkWebInputOptions& options) {
        mConfig.webInput = options;
    }

    NkWebInputOptions NkWindow::GetWebInputOptions() const { return mConfig.webInput; }

    void NkWindow::SetProgress(float) {}

    NkSafeAreaInsets NkWindow::GetSafeAreaInsets() const {
        if (!mConfig.respectSafeArea) {
            return {};
        }
        return mData.mSafeArea;
    }

    NkSurfaceDesc NkWindow::GetSurfaceDesc() const {
        NkSurfaceDesc desc;
        const auto size = GetSize();
        desc.width = size.x;
        desc.height = size.y;
        desc.dpi = GetDpiScale();
        desc.nativeWindow = mData.mNativeWindow;
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_ANDROID
