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
#include "NKCore/NkAtomic.h"

#include <android/configuration.h>
#include <android/native_window.h>
#include <android/window.h>
#include <android_native_app_glue.h>
#include <jni.h>

namespace nkentseu {
    using namespace math;

    android_app* nk_android_global_app = nullptr;
    static NkSpinLock sAndroidWindowsMutex;
    static NkWindow* sAndroidLastWindow = nullptr;

    // Function-local statics avoid static init order fiasco with NkAllocator.
    static NkVector<NkWindow*>& AndroidWindows() {
        static NkVector<NkWindow*> sVec;
        return sVec;
    }
    static NkUnorderedMap<NkWindowId, NkWindow*>& AndroidWindowById() {
        static NkUnorderedMap<NkWindowId, NkWindow*> sMap;
        if (sMap.BucketCount() == 0) {
            sMap.Rehash(32);
        }
        return sMap;
    }

    NkWindow* NkAndroidFindWindowById(NkWindowId id) {
        NkScopedSpinLock lock(sAndroidWindowsMutex);
        auto* win = AndroidWindowById().Find(id);
        return win ? *win : nullptr;
    }

    NkVector<NkWindow*> NkAndroidGetWindowsSnapshot() {
        NkScopedSpinLock lock(sAndroidWindowsMutex);
        return AndroidWindows();
    }

    NkWindow* NkAndroidGetLastWindow() {
        NkScopedSpinLock lock(sAndroidWindowsMutex);
        return sAndroidLastWindow;
    }

    void NkAndroidRegisterWindow(NkWindow* window) {
        if (!window) return;
        const NkWindowId id = window->GetId();
        if (id == NK_INVALID_WINDOW_ID) return;

        NkScopedSpinLock lock(sAndroidWindowsMutex);
        auto& windows = AndroidWindows();
        bool found = false;
        for (uint32 i = 0; i < windows.Size(); ++i) {
            if (windows[i] == window) { found = true; break; }
        }
        if (!found) windows.PushBack(window);
        AndroidWindowById()[id] = window;
        sAndroidLastWindow = window;
    }

    void NkAndroidUnregisterWindow(NkWindow* window) {
        if (!window) return;

        NkScopedSpinLock lock(sAndroidWindowsMutex);
        auto& windows = AndroidWindows();

        // Remove window from vector
        for (uint32 i = 0; i < windows.Size(); ++i) {
            if (windows[i] == window) {
                windows.Erase(windows.begin() + i);
                break;
            }
        }

        // Remove from map
        NkWindowId staleId = NK_INVALID_WINDOW_ID;
        AndroidWindowById().ForEach([&](NkWindowId id, NkWindow* const& v) {
            if (v == window && staleId == NK_INVALID_WINDOW_ID) staleId = id;
        });
        if (staleId != NK_INVALID_WINDOW_ID) {
            AndroidWindowById().Erase(staleId);
        }

        if (sAndroidLastWindow == window) {
            sAndroidLastWindow = windows.Empty() ? nullptr : windows.Back();
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

    // =========================================================================
    // Fonctions de synchronisation mData ↔ mConfig
    // =========================================================================

    static void SyncConfigFromWindow(const NkAndroidWindowData& data, NkWindowConfig& config) {
        config.width = data.mWidth;
        config.height = data.mHeight;
        config.screenOrientation = data.mOrientation;
        config.fullscreen = data.mFullscreen;
        // La visibilité est implicite sur Android, on garde config.visible
        // Le titre n'est pas récupérable, on garde config.title
        // La position n'est pas pertinente, on garde config.x/config.y
    }

    static void SyncWindowFromConfig(NkAndroidWindowData& data, const NkWindowConfig& config) {
        data.mOrientation = config.screenOrientation;
        data.mFullscreen = config.fullscreen;
        // Les autres propriétés seront appliquées via les méthodes dédiées
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
        mData.mAppliedHints = config.surfaceHints;
        mData.mExternal = false;
        mData.mAndroidApp = config.native.externalDisplayHandle != 0
            ? reinterpret_cast<android_app*>(config.native.externalDisplayHandle)
            : nk_android_global_app;

        const bool wantsExternal = config.native.useExternalWindow;
        const bool hasExternalHandle = (config.native.externalWindowHandle != 0);
        if (wantsExternal && !hasExternalHandle) {
            mLastError = NkError(1, "Android: useExternalWindow=true but externalWindowHandle is null");
            return false;
        }

        if (wantsExternal && hasExternalHandle) {
            mData.mNativeWindow = reinterpret_cast<ANativeWindow*>(config.native.externalWindowHandle);
            mData.mExternal = true;
        } else {
            if (!mData.mAndroidApp) {
                mLastError = NkError(1, "Android: android_app is null");
                return false;
            }
            if (!mData.mAndroidApp->window) {
                mLastError = NkError(2, "Android: ANativeWindow is null");
                return false;
            }
            mData.mNativeWindow = mData.mAndroidApp->window;
        }

        if (!mData.mNativeWindow) {
            mLastError = NkError(2, "Android: ANativeWindow is null");
            return false;
        }

        ANativeWindow_acquire(mData.mNativeWindow);
        ANativeWindow_setBuffersGeometry(mData.mNativeWindow, 0, 0, WINDOW_FORMAT_RGBA_8888);

        mData.mWidth = static_cast<uint32>(ANativeWindow_getWidth(mData.mNativeWindow));
        mData.mHeight = static_cast<uint32>(ANativeWindow_getHeight(mData.mNativeWindow));
        mData.mPrevWidth = mData.mWidth;
        mData.mPrevHeight = mData.mHeight;

        // Synchroniser mConfig avec les dimensions réelles
        mConfig.width = mData.mWidth;
        mConfig.height = mData.mHeight;

        if (mData.mAConfig) {
            AConfiguration_delete(mData.mAConfig);
            mData.mAConfig = nullptr;
        }
        mData.mAConfig = AConfiguration_new();
        if (mData.mAConfig && mData.mAndroidApp->activity && mData.mAndroidApp->activity->assetManager) {
            AConfiguration_fromAssetManager(mData.mAConfig, mData.mAndroidApp->activity->assetManager);
        }

        mData.mOrientation = config.screenOrientation;
        if (mData.mAndroidApp) {
            NkAndroidApplyOrientation(*this, mData.mOrientation);
            NkAndroidUpdateSafeArea(*this);
        }

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
        mData.mExternal = false;
    }

    bool NkWindow::IsOpen() const { return mIsOpen; }

    bool NkWindow::IsValid() const { return mIsOpen && mData.mNativeWindow != nullptr; }

    NkError NkWindow::GetLastError() const { return mLastError; }

    NkWindowConfig NkWindow::GetConfig() const {
        // Synchroniser avant de retourner
        if (mIsOpen) {
            SyncConfigFromWindow(mData, const_cast<NkWindow*>(this)->mConfig);
        }
        return mConfig;
    }

    NkString NkWindow::GetTitle() const { return mConfig.title; }

    NkVec2u NkWindow::GetSize() const {
        if (mData.mNativeWindow) {
            uint32 width = static_cast<uint32>(ANativeWindow_getWidth(mData.mNativeWindow));
            uint32 height = static_cast<uint32>(ANativeWindow_getHeight(mData.mNativeWindow));
            
            // Synchroniser mData et mConfig
            const_cast<NkWindow*>(this)->mData.mWidth = width;
            const_cast<NkWindow*>(this)->mData.mHeight = height;
            const_cast<NkWindow*>(this)->mConfig.width = width;
            const_cast<NkWindow*>(this)->mConfig.height = height;
            
            return {width, height};
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

    void NkWindow::SetTitle(const NkString& title) { 
        mConfig.title = title; 
        // Sur Android, le titre n'est pas directement modifiable après création
    }

    void NkWindow::SetSize(uint32 width, uint32 height) {
        mConfig.width = width;
        mConfig.height = height;
        // Sur Android, la taille est contrôlée par le système
    }

    void NkWindow::SetPosition(int32, int32) {}

    void NkWindow::SetVisible(bool visible) {
        mConfig.visible = visible;
        // Sur Android, la visibilité est contrôlée par le système
    }

    void NkWindow::Minimize() {}

    void NkWindow::Maximize() {}

    void NkWindow::Restore() {}

    void NkWindow::SetFullscreen(bool fullscreen) {
        mConfig.fullscreen = fullscreen;
        mData.mFullscreen = fullscreen;
        // Sur Android, le plein écran est géré par le système
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

    void NkWindow::SetMousePosition(uint32, uint32) {}

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
        const auto size = GetSize();  // GetSize synchronise déjà mConfig
        desc.width = size.x;
        desc.height = size.y;
        desc.dpi = GetDpiScale();
        desc.nativeWindow = mData.mNativeWindow;
        desc.appliedHints = mData.mAppliedHints;
        return desc;
    }

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_ANDROID