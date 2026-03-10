#pragma once

// =============================================================================
// NkAndroid.h
// Android NDK entry point.
// =============================================================================

#include "NKWindow/Core/NkEntry.h"
#include "NKLogger/NkLog.h"

#include <android/looper.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <string>
#include <vector>
#include <utility>

#ifndef NK_APP_NAME
#define NK_APP_NAME "android_app"
#endif

#ifndef NK_ANDROID_BOOT_TAG
#define NK_ANDROID_BOOT_TAG "NkAndroidBoot"
#endif

#define NK_ANDROID_BOOTLOG(...) logger.Infof(__VA_ARGS__)

namespace nkentseu {
inline NkEntryState* gState = nullptr;
extern android_app* nk_android_global_app;
} // namespace nkentseu

extern "C" void android_main(android_app* app) {
    NK_ANDROID_BOOTLOG("android_main enter app=%p", static_cast<void*>(app));
    if (!app) {
        NK_ANDROID_BOOTLOG("android_main abort: app == null");
        return;
    }

    // app_dummy();
    NK_ANDROID_BOOTLOG("app_dummy done");
    nkentseu::nk_android_global_app = app;

    // Wait explicitly for APP_CMD_INIT_WINDOW before entering nkmain().
    // This matches the stable startup pattern used in working examples.
    while (!app->window && !app->destroyRequested) {
        int events = 0;
        android_poll_source* source = nullptr;
        const int pollResult = ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source));
        if (pollResult >= 0 && source) {
            source->process(app, source);
        }
    }
    NK_ANDROID_BOOTLOG(
        "window wait done window=%p destroyRequested=%d",
        static_cast<void*>(app->window),
        app->destroyRequested);

    if (app->destroyRequested || !app->window) {
        NK_ANDROID_BOOTLOG("android_main early exit before state creation");
        nkentseu::nk_android_global_app = nullptr;
        return;
    }

    // Read package metadata with JNI.
    JNIEnv* env = nullptr;
    bool attachedThread = false;
    NK_ANDROID_BOOTLOG("before JNI attach vm=%p", app->activity ? static_cast<void*>(app->activity->vm) : nullptr);
    if (app->activity && app->activity->vm) {
        if (app->activity->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (app->activity->vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attachedThread = true;
            } else {
                env = nullptr;
            }
        }
    }
    NK_ANDROID_BOOTLOG("after JNI attach env=%p attachedThread=%d", static_cast<void*>(env), attachedThread ? 1 : 0);

    nkentseu::NkString packageName;
    if (env && app->activity) {
        jobject actObj = app->activity->clazz;
        if (actObj) {
            jclass actCls = env->GetObjectClass(actObj);
            if (actCls) {
                jmethodID getPkg = env->GetMethodID(actCls, "getPackageName", "()Ljava/lang/String;");
                if (getPkg) {
                    jstring jstr = static_cast<jstring>(env->CallObjectMethod(actObj, getPkg));
                    if (jstr) {
                        const char* cs = env->GetStringUTFChars(jstr, nullptr);
                        if (cs) {
                            packageName = cs;
                            env->ReleaseStringUTFChars(jstr, cs);
                        }
                        env->DeleteLocalRef(jstr);
                    }
                }
                env->DeleteLocalRef(actCls);
            }
        }
    }
    NK_ANDROID_BOOTLOG("packageName extracted length=%llu", static_cast<unsigned long long>(packageName.Size()));

    if (attachedThread && app->activity && app->activity->vm) {
        app->activity->vm->DetachCurrentThread();
    }
    NK_ANDROID_BOOTLOG("JNI detach done");

    nkentseu::NkVector<nkentseu::NkString> args;
    if (!packageName.Empty()) {
        args.PushBack(packageName);
    }
    NK_ANDROID_BOOTLOG("args built size=%llu", static_cast<unsigned long long>(args.Size()));

    NK_ANDROID_BOOTLOG("before NkEntryState ctor");
    nkentseu::NkEntryState state(app, std::move(args));
    NK_ANDROID_BOOTLOG("after NkEntryState ctor");
    state.appName = NK_APP_NAME;
    nkentseu::gState = &state;

    NK_ANDROID_BOOTLOG("before nkmain");
    nkmain(state);
    NK_ANDROID_BOOTLOG("after nkmain");

    nkentseu::gState = nullptr;
    nkentseu::nk_android_global_app = nullptr;
    NK_ANDROID_BOOTLOG("android_main exit");
}
