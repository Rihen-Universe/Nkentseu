// =============================================================================
// NkLauncher.cpp
// -----------------------------------------------------------------------------
// Implementation multi-plateforme. Chaque section est gatee par les macros
// NKENTSEU_PLATFORM_* (definies dans NKPlatform/NkPlatformDetect.h).
// =============================================================================

#include "NKWindow/Core/NkLauncher.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"

#include <cstdlib>   // system()
#include <cstring>   // strlen
#include <cstdio>    // snprintf

// ── Windows ──────────────────────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <shellapi.h>  // ShellExecuteA
    #pragma comment(lib, "shell32.lib")
#endif

// ── Android ──────────────────────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_ANDROID)
    #include <jni.h>
    #include <android_native_app_glue.h>
    namespace nkentseu {
        // Variable globale exposee par NkAndroidWindow.cpp : pointe sur
        // l'android_app* courant (necessaire pour JNI : VM + activity).
        extern struct android_app* nk_android_global_app;
    }
#endif

// ── iOS (Obj-C bridge) ───────────────────────────────────────────────────────
// Note : iOS necessite que ce fichier soit compile en Obj-C++ (.mm). Pour
// rester C++ pur, on declare juste un wrapper extern "C" qui sera implemente
// dans un NkLauncherIOS.mm si/quand on cible iOS. Pour l'instant, stub.
#if defined(NKENTSEU_PLATFORM_IOS)
    // Stub : a remplacer par appel UIApplication openURL via un .mm.
#endif

// ── Emscripten ───────────────────────────────────────────────────────────────
#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(__EMSCRIPTEN__)
    #include <emscripten.h>
#endif

namespace nkentseu {

    // ─────────────────────────────────────────────────────────────────────────
    // OpenURL — selon plateforme
    // ─────────────────────────────────────────────────────────────────────────
    bool NkLauncher::OpenURL(const char* url) noexcept
    {
        if (!url || !*url) return false;

#if defined(NKENTSEU_PLATFORM_WINDOWS)
        // ShellExecute retourne un HINSTANCE > 32 si OK (Win32 historique).
        const HINSTANCE r = ::ShellExecuteA(nullptr, "open", url, nullptr,
                                            nullptr, SW_SHOWNORMAL);
        const bool ok = (reinterpret_cast<INT_PTR>(r) > 32);
        if (!ok) {
            logger.Warn("[NkLauncher] ShellExecute failed for url={0}", url);
        }
        return ok;

#elif defined(NKENTSEU_PLATFORM_ANDROID)
        // Android : Intent.ACTION_VIEW + Uri.parse(url) + startActivity.
        struct android_app* app = nk_android_global_app;
        if (!app || !app->activity || !app->activity->vm) {
            logger.Warn("[NkLauncher] Android : android_app indisponible");
            return false;
        }
        JavaVM* vm = app->activity->vm;
        JNIEnv* env = nullptr;
        bool attached = false;
        // Tente GetEnv ; si pas attache, attach le thread courant.
        if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
                logger.Warn("[NkLauncher] Android : AttachCurrentThread fail");
                return false;
            }
            attached = true;
        }
        bool ok = false;
        do {
            // Uri uri = Uri.parse(url);
            jclass uriClass = env->FindClass("android/net/Uri");
            if (!uriClass) break;
            jmethodID parseM = env->GetStaticMethodID(
                uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
            if (!parseM) break;
            jstring jurl = env->NewStringUTF(url);
            jobject uri = env->CallStaticObjectMethod(uriClass, parseM, jurl);
            env->DeleteLocalRef(jurl);
            if (!uri) break;
            // Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            jclass intentClass = env->FindClass("android/content/Intent");
            if (!intentClass) break;
            jmethodID intentInit = env->GetMethodID(
                intentClass, "<init>",
                "(Ljava/lang/String;Landroid/net/Uri;)V");
            if (!intentInit) break;
            jstring action = env->NewStringUTF("android.intent.action.VIEW");
            jobject intent = env->NewObject(intentClass, intentInit, action, uri);
            env->DeleteLocalRef(action);
            if (!intent) break;
            // intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK = 0x10000000)
            jmethodID addFlagsM = env->GetMethodID(
                intentClass, "addFlags", "(I)Landroid/content/Intent;");
            if (addFlagsM) {
                env->CallObjectMethod(intent, addFlagsM, 0x10000000);
            }
            // activity.startActivity(intent);
            jobject activity = app->activity->clazz;
            jclass activityClass = env->GetObjectClass(activity);
            jmethodID startA = env->GetMethodID(
                activityClass, "startActivity",
                "(Landroid/content/Intent;)V");
            if (!startA) break;
            env->CallVoidMethod(activity, startA, intent);
            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                break;
            }
            ok = true;
        } while (false);
        if (attached) vm->DetachCurrentThread();
        if (!ok) logger.Warn("[NkLauncher] Android JNI startActivity failed url={0}", url);
        return ok;

#elif defined(NKENTSEU_PLATFORM_LINUX)
        // xdg-open via system(). On entoure l'URL de "" simples pour eviter
        // l'expansion shell. Pas de sanitization -- l'appelant doit garantir
        // que l'URL ne contient pas de "'" malveillant.
        char cmd[2048];
        std::snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 &", url);
        return std::system(cmd) == 0;

#elif defined(NKENTSEU_PLATFORM_MACOS)
        char cmd[2048];
        std::snprintf(cmd, sizeof(cmd), "open '%s' >/dev/null 2>&1 &", url);
        return std::system(cmd) == 0;

#elif defined(NKENTSEU_PLATFORM_IOS)
        // TODO iOS : implementer via UIApplication openURL dans un .mm.
        logger.Warn("[NkLauncher] iOS : OpenURL non implemente (need .mm)");
        (void)url;
        return false;

#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(__EMSCRIPTEN__)
        // window.open dans un nouvel onglet.
        EM_ASM({
            window.open(UTF8ToString($0), '_blank');
        }, url);
        return true;

#else
        logger.Warn("[NkLauncher] Plateforme non supportee : url={0}", url);
        (void)url;
        return false;
#endif
    }

    // ─────────────────────────────────────────────────────────────────────────
    // OpenFile — ouvre un fichier avec son app par defaut
    // ─────────────────────────────────────────────────────────────────────────
    bool NkLauncher::OpenFile(const char* filePath) noexcept
    {
        if (!filePath || !*filePath) return false;
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        const HINSTANCE r = ::ShellExecuteA(nullptr, "open", filePath, nullptr,
                                            nullptr, SW_SHOWNORMAL);
        return (reinterpret_cast<INT_PTR>(r) > 32);
#elif defined(NKENTSEU_PLATFORM_LINUX)
        char cmd[2048];
        std::snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 &", filePath);
        return std::system(cmd) == 0;
#elif defined(NKENTSEU_PLATFORM_MACOS)
        char cmd[2048];
        std::snprintf(cmd, sizeof(cmd), "open '%s' >/dev/null 2>&1 &", filePath);
        return std::system(cmd) == 0;
#elif defined(NKENTSEU_PLATFORM_ANDROID)
        // Android : pour ouvrir un fichier local on doit passer par un Intent
        // ACTION_VIEW avec un content:// URI (via FileProvider). Implementation
        // future. Pour l'instant, stub.
        logger.Warn("[NkLauncher] Android : OpenFile non implemente");
        (void)filePath;
        return false;
#else
        (void)filePath;
        return false;
#endif
    }

    // ─────────────────────────────────────────────────────────────────────────
    // OpenFolder — ouvre un dossier dans l'explorateur systeme
    // ─────────────────────────────────────────────────────────────────────────
    bool NkLauncher::OpenFolder(const char* folderPath) noexcept
    {
        if (!folderPath || !*folderPath) return false;
#if defined(NKENTSEU_PLATFORM_WINDOWS)
        const HINSTANCE r = ::ShellExecuteA(nullptr, "explore", folderPath, nullptr,
                                            nullptr, SW_SHOWNORMAL);
        return (reinterpret_cast<INT_PTR>(r) > 32);
#elif defined(NKENTSEU_PLATFORM_LINUX)
        char cmd[2048];
        std::snprintf(cmd, sizeof(cmd), "xdg-open '%s' >/dev/null 2>&1 &", folderPath);
        return std::system(cmd) == 0;
#elif defined(NKENTSEU_PLATFORM_MACOS)
        char cmd[2048];
        std::snprintf(cmd, sizeof(cmd), "open '%s' >/dev/null 2>&1 &", folderPath);
        return std::system(cmd) == 0;
#else
        (void)folderPath;
        return false;
#endif
    }

} // namespace nkentseu
