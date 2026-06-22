// =============================================================================
// NkAndroidWindow.cpp - NkWindow implementation for Android
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_ANDROID)

#include "NKWindow/Platform/Android/NkAndroidWindow.h"
#include "NKWindow/Platform/Android/NkAndroidDropTarget.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkEventSystem.h"
#include "NKMemory/NkAllocator.h"   // NkGetDefaultAllocator().New/Delete (regle maison : pas de new/delete)
#include "NKCore/NkAtomic.h"
#include "NKFileSystem/NkFile.h"

#include <android/configuration.h>
#include <android/native_window.h>
#include <android/native_activity.h>
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

    // =========================================================================
    // Masquage des barres système (status bar + navigation bar) via JNI
    // =========================================================================
    
    bool NkAndroidHideSystemUI(android_app* app) {
        if (!app || !app->activity || !app->activity->clazz) {
            return false;
        }

        JNIEnv* env = nullptr;
        bool attached = false;
        if (!NkAndroidAcquireJniEnv(app, &env, &attached)) {
            return false;
        }

#if defined(AWINDOW_FLAG_FULLSCREEN)
        ANativeActivity_setWindowFlags(app->activity, AWINDOW_FLAG_FULLSCREEN, 0);
#endif

        bool ok = false;
        jobject activity = app->activity->clazz;
        jclass actClass = env->GetObjectClass(activity);
        if (actClass) {
            jmethodID getWindow = env->GetMethodID(actClass, "getWindow", "()Landroid/view/Window;");
            if (getWindow) {
                jobject windowObj = env->CallObjectMethod(activity, getWindow);
                if (windowObj && !env->ExceptionCheck()) {
                    jclass windowClass = env->GetObjectClass(windowObj);
                    if (windowClass) {
                        // API 30+ : cacher status bar + navigation bar via WindowInsetsController.
                        jmethodID getInsetsController = env->GetMethodID(
                            windowClass, "getInsetsController", "()Landroid/view/WindowInsetsController;"
                        );
                        if (env->ExceptionCheck()) {
                            env->ExceptionClear();
                            getInsetsController = nullptr;
                        }
                        if (getInsetsController) {
                            jobject controller = env->CallObjectMethod(windowObj, getInsetsController);
                            if (controller && !env->ExceptionCheck()) {
                                jclass controllerClass = env->GetObjectClass(controller);
                                jclass typeClass = env->FindClass("android/view/WindowInsets$Type");
                                if (env->ExceptionCheck()) {
                                    env->ExceptionClear();
                                    typeClass = nullptr;
                                }

                                if (controllerClass && typeClass) {
                                    jmethodID hideBars = env->GetMethodID(controllerClass, "hide", "(I)V");
                                    if (env->ExceptionCheck()) {
                                        env->ExceptionClear();
                                        hideBars = nullptr;
                                    }
                                    jmethodID setBehavior = env->GetMethodID(controllerClass, "setSystemBarsBehavior", "(I)V");
                                    if (env->ExceptionCheck()) {
                                        env->ExceptionClear();
                                        setBehavior = nullptr;
                                    }
                                    jmethodID systemBars = env->GetStaticMethodID(typeClass, "systemBars", "()I");
                                    if (env->ExceptionCheck()) {
                                        env->ExceptionClear();
                                        systemBars = nullptr;
                                    }

                                    jint bars = 0;
                                    if (systemBars) {
                                        bars = env->CallStaticIntMethod(typeClass, systemBars);
                                        if (env->ExceptionCheck()) {
                                            env->ExceptionClear();
                                            bars = 0;
                                        }
                                    }
                                    if (setBehavior) {
                                        env->CallVoidMethod(controller, setBehavior, 2);
                                        if (env->ExceptionCheck()) {
                                            env->ExceptionClear();
                                        }
                                    }
                                    if (hideBars && bars != 0) {
                                        env->CallVoidMethod(controller, hideBars, bars);
                                        if (!env->ExceptionCheck()) {
                                            ok = true;
                                        } else {
                                            env->ExceptionClear();
                                        }
                                    }
                                }

                                if (typeClass) {
                                    env->DeleteLocalRef(typeClass);
                                }
                                if (controllerClass) {
                                    env->DeleteLocalRef(controllerClass);
                                }
                                env->DeleteLocalRef(controller);
                            } else {
                                env->ExceptionClear();
                            }
                        }

                        // Appel à DecorView
                        jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
                        if (getDecorView) {
                            jobject decorView = env->CallObjectMethod(windowObj, getDecorView);
                            if (decorView && !env->ExceptionCheck()) {
                                jclass viewClass = env->GetObjectClass(decorView);
                                if (viewClass) {
                                    // setSystemUiVisibility(flags)
                                    // Flags: LOW_PROFILE=1, HIDE_NAVIGATION=2, FULLSCREEN=4,
                                    // LAYOUT_STABLE=256, LAYOUT_HIDE_NAVIGATION=512,
                                    // LAYOUT_FULLSCREEN=1024, IMMERSIVE=2048,
                                    // IMMERSIVE_STICKY=4096.
                                    jmethodID setSystemUiVis = env->GetMethodID(
                                        viewClass, "setSystemUiVisibility", "(I)V"
                                    );
                                    if (setSystemUiVis) {
                                        jint flags = 1 | 2 | 4 | 256 | 512 | 1024 | 2048 | 4096;
                                        env->CallVoidMethod(decorView, setSystemUiVis, flags);
                                        if (!env->ExceptionCheck()) {
                                            ok = true;
                                        } else {
                                            env->ExceptionClear();
                                        }
                                    }
                                    env->DeleteLocalRef(viewClass);
                                }
                                env->DeleteLocalRef(decorView);
                            } else {
                                env->ExceptionClear();
                            }
                        }
                        env->DeleteLocalRef(windowClass);
                    }
                    env->DeleteLocalRef(windowObj);
                } else {
                    env->ExceptionClear();
                }
            }
            env->DeleteLocalRef(actClass);
        }

        NkAndroidReleaseJniEnv(app, attached);
        return ok;
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
    // Interrogation du taux de rafraîchissement de l'écran (JNI)
    // Chaîne : activity.getWindowManager().getDefaultDisplay().getRefreshRate()
    // Retourne 60 Hz par défaut si l'API est indisponible.
    // =========================================================================

    static uint32 NkAndroidQueryRefreshRate(android_app* app) {
        if (!app || !app->activity || !app->activity->clazz) {
            return 60u;
        }

        JNIEnv* env = nullptr;
        bool attached = false;
        if (!NkAndroidAcquireJniEnv(app, &env, &attached)) {
            return 60u;
        }

        uint32 refresh = 60u;
        jobject activity = app->activity->clazz;
        jclass actClass = env->GetObjectClass(activity);
        if (actClass) {
            jmethodID getWM = env->GetMethodID(actClass, "getWindowManager", "()Landroid/view/WindowManager;");
            if (getWM) {
                jobject wm = env->CallObjectMethod(activity, getWM);
                if (wm && !env->ExceptionCheck()) {
                    jclass wmClass = env->GetObjectClass(wm);
                    if (wmClass) {
                        jmethodID getDisplay = env->GetMethodID(wmClass, "getDefaultDisplay", "()Landroid/view/Display;");
                        if (getDisplay) {
                            jobject display = env->CallObjectMethod(wm, getDisplay);
                            if (display && !env->ExceptionCheck()) {
                                jclass displayClass = env->GetObjectClass(display);
                                if (displayClass) {
                                    jmethodID getRefresh = env->GetMethodID(displayClass, "getRefreshRate", "()F");
                                    if (getRefresh) {
                                        jfloat rate = env->CallFloatMethod(display, getRefresh);
                                        if (!env->ExceptionCheck() && rate > 0.0f) {
                                            refresh = static_cast<uint32>(rate + 0.5f);
                                        } else {
                                            env->ExceptionClear();
                                        }
                                    }
                                    env->DeleteLocalRef(displayClass);
                                }
                                env->DeleteLocalRef(display);
                            } else {
                                env->ExceptionClear();
                            }
                        }
                        env->DeleteLocalRef(wmClass);
                    }
                    env->DeleteLocalRef(wm);
                } else {
                    env->ExceptionClear();
                }
            }
            env->DeleteLocalRef(actClass);
        }

        NkAndroidReleaseJniEnv(app, attached);
        return refresh;
    }

    // =========================================================================
    // Fonctions de synchronisation mData ↔ mConfig
    // =========================================================================

    static void SyncConfigFromWindow(const NkWindowData& data, NkWindowConfig& config) {
        config.width = data.mWidth;
        config.height = data.mHeight;
        config.screenOrientation = data.mOrientation;
        config.fullscreen = data.mFullscreen;
        // La visibilité est implicite sur Android, on garde config.visible
        // Le titre n'est pas récupérable, on garde config.title
        // La position n'est pas pertinente, on garde config.x/config.y
    }

    static void SyncWindowFromConfig(NkWindowData& data, const NkWindowConfig& config) {
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
            // Expose l'AAssetManager au sous-systeme fichier pour que NkFile
            // (et donc NkImage::Load, futures NKFont/NKAudio) puisse acceder
            // aux ressources empaquetees dans assets/ de l'APK.
            NkFile::SetAndroidAssetManager(mData.mAndroidApp->activity->assetManager);
        }

        mData.mOrientation = config.screenOrientation;
        if (mData.mAndroidApp) {
            NkAndroidApplyOrientation(*this, mData.mOrientation);
            NkAndroidUpdateSafeArea(*this);
            // Appliquer la configuration hideSystemUI a la creation de la fenetre
            if (config.hideSystemUI) {
                NkAndroidHideSystemUI(mData.mAndroidApp);
            }
        }

        mId = NkWESystem::Instance().RegisterWindow(this);
        NkAndroidRegisterWindow(this);

        mData.mDropTarget = memory::NkGetDefaultAllocator().New<NkAndroidDropTarget>(mId);
        if (mData.mDropTarget) {
            mData.mDropTarget->SetDropEnterCallback([this](const NkDropEnterEvent& event) {
                NkDropEnterEvent copy(event);
                NkWESystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropOverCallback([this](const NkDropOverEvent& event) {
                NkDropOverEvent copy(event);
                NkWESystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropLeaveCallback([this](const NkDropLeaveEvent& event) {
                NkDropLeaveEvent copy(event);
                NkWESystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropFileCallback([this](const NkDropFileEvent& event) {
                NkDropFileEvent copy(event);
                NkWESystem::Events().Enqueue_Public(copy, mId);
            });
            mData.mDropTarget->SetDropTextCallback([this](const NkDropTextEvent& event) {
                NkDropTextEvent copy(event);
                NkWESystem::Events().Enqueue_Public(copy, mId);
            });
        }

        mIsOpen = true;

        NkWindowCreateEvent e(mData.mWidth, mData.mHeight);
        NkWESystem::Events().Enqueue_Public(e, mId);
        return true;
    }

    void NkWindow::Close() {
        if (!mIsOpen) {
            return;
        }
        mIsOpen = false;

        NkAndroidUnregisterWindow(this);
        NkWESystem::Instance().UnregisterWindow(mId);
        mId = NK_INVALID_WINDOW_ID;

        if (mData.mDropTarget) {
            memory::NkGetDefaultAllocator().Delete(mData.mDropTarget);
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

    // =========================================================================
    // Énumération des moniteurs / DPI
    //
    // Android est mono-écran du point de vue de l'application : un seul moniteur
    // factice est exposé, rempli à partir de la taille de l'ANativeWindow et de
    // la densité (DisplayMetrics via AConfiguration). La densité de référence
    // Android est 160 dpi (et non 96), donc dpiScale = densityDpi / 160.
    // =========================================================================

    // Construit l'unique NkDisplayInfo décrivant l'écran Android courant.
    static NkDisplayInfo NkAndroidFillDisplayInfo(const NkWindow& window) {
        NkDisplayInfo info;
        info.index = 0;
        info.isPrimary = true;

        // Taille de l'écran = taille de la surface plein écran.
        const NkVec2u size = window.GetDisplaySize();
        info.width      = size.x;
        info.height     = size.y;
        info.physWidth  = size.x;
        info.physHeight = size.y;

        // Densité réelle (dpi) via AConfiguration ; baseline = 160 dpi.
        android_app* app = window.mData.mAndroidApp ? window.mData.mAndroidApp : nk_android_global_app;
        uint32 densityDpi = 160;
        if (app && app->activity && app->activity->assetManager) {
            AConfiguration* config = AConfiguration_new();
            if (config) {
                AConfiguration_fromAssetManager(config, app->activity->assetManager);
                const int32_t density = AConfiguration_getDensity(config);
                AConfiguration_delete(config);
                if (density > 0) {
                    densityDpi = static_cast<uint32>(density);
                }
            }
        }
        info.dpiX     = static_cast<float32>(densityDpi);
        info.dpiY     = static_cast<float32>(densityDpi);
        info.dpiScale = static_cast<float32>(densityDpi) / 160.0f;

        // Fréquence de rafraîchissement via Display.getRefreshRate() (JNI).
        info.refreshRate = NkAndroidQueryRefreshRate(app);

        // Nom lisible.
        const char* name = "Android Display";
        usize i = 0;
        for (; name[i] != '\0' && i < sizeof(info.name) - 1; ++i) info.name[i] = name[i];
        info.name[i] = '\0';
        return info;
    }

    NkVector<NkDisplayInfo> NkWindow::EnumerateMonitors() const {
        NkVector<NkDisplayInfo> out;
        out.PushBack(NkAndroidFillDisplayInfo(*this));
        return out;
    }

    NkDisplayInfo NkWindow::GetCurrentMonitor() const {
        return NkAndroidFillDisplayInfo(*this);
    }

    uint32 NkWindow::GetMonitorCount() const { return 1u; }

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

    void NkWindow::SetHideSystemUI(bool hide) {
        mConfig.hideSystemUI = hide;
        if (hide && mData.mAndroidApp) {
            NkAndroidHideSystemUI(mData.mAndroidApp);
        }
    }

    bool NkWindow::GetHideSystemUI() const {
        return mConfig.hideSystemUI;
    }

    void NkWindow::SetLockOrientation(bool lock) {
        mConfig.lockOrientation = lock;
        if (lock && mData.mOrientation == NkScreenOrientation::NK_SCREEN_ORIENTATION_AUTO) {
            // Forcer le paysage si verrouillé et en AUTO
            SetScreenOrientation(NkScreenOrientation::NK_SCREEN_ORIENTATION_LANDSCAPE);
        }
    }

    bool NkWindow::GetLockOrientation() const {
        return mConfig.lockOrientation;
    }

    void NkWindow::SetMousePosition(uint32, uint32) {}

    void NkWindow::ShowMouse(bool) {}

    void NkWindow::CaptureMouse(bool) {}

    // Mobile : pas de curseur natif (input tactile), no-op.
    void NkWindow::ClipMouseToClient(bool) {}

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
