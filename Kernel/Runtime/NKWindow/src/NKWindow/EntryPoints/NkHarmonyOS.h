#pragma once

// =============================================================================
// NkHarmonyOS.h
// HarmonyOS NativeAbility entry point.
//
// À inclure UNE SEULE FOIS dans le fichier source qui implémente nkmain().
// Équivalent exact de NkAndroid.h pour HarmonyOS.
//
// Fonctionnement :
//   Sur HarmonyOS, le code C++ natif est chargé comme une .so par la
//   NativeAbility ArkTS. Il n'y a pas de "main" classique — le point
//   d'entrée est un ensemble de fonctions C exportées, appelées par le
//   runtime ArkTS via OH_NativeXComponent.
//
//   Ce header exporte :
//     napi_value Init(napi_env, napi_value)  ← enregistrement du module NAPI
//
//   Et définit la macro NKENTSEU_HARMONY_DEFINE_MODULE qui génère le
//   NAPI_MODULE() standard attendu par le runtime HarmonyOS.
//
//   Flux de démarrage :
//     ArkTS charge la .so → NAPI_MODULE(entry, Init) est appelé
//     → Init() crée le NkEntryState + NkEntryRuntimeInit()
//     → nkmain(state) est lancé dans un thread dédié
//     → Le thread boucle jusqu'à ce que nkmain() retourne
//     → NkEntryRuntimeShutdown()
//
// Utilisation :
//   #include <NkentseuWindow/Core/NkMain.h>
//
//   int nkmain(const nkentseu::NkEntryState& state) {
//       nkentseu::NkWindowConfig cfg;
//       cfg.title = "Hello HarmonyOS";
//       // ...
//       return 0;
//   }
// =============================================================================

#include "NKWindow/Core/NkEntry.h"
#include "NKWindow/Platform/HarmonyOS/NkHarmonyWindow.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkTraits.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <napi/native_api.h>
#include <pthread.h>

#ifndef NK_APP_NAME
#define NK_APP_NAME "harmony_app"
#endif

#ifndef NK_HARMONY_BOOT_TAG
#define NK_HARMONY_BOOT_TAG "NkHarmonyBoot"
#endif

#define NK_HARMONY_BOOTLOG(...) logger.Infof(__VA_ARGS__)

// Déclaration forward de la fonction d'enregistrement des callbacks XComponent
// (définie dans NkHarmonyEventSystem.cpp)
namespace nkentseu {
    void NkHarmonyRegisterXComponentCallbacks(OH_NativeXComponent* xcomp);
    inline NkEntryState* gState = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Thread de l'application (nkmain tourne dans un thread séparé pour ne pas
// bloquer le thread UI ArkTS)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

    struct NkHarmonyMainArgs {
        nkentseu::NkEntryState* state = nullptr;
    };

    static void* NkHarmonyMainThread(void* arg) {
        NK_HARMONY_BOOTLOG("NkHarmonyMainThread: start");
        auto* args = static_cast<NkHarmonyMainArgs*>(arg);
        if (!args || !args->state) {
            NK_HARMONY_BOOTLOG("NkHarmonyMainThread: invalid args");
            return nullptr;
        }

        nkmain(*args->state);

        NK_HARMONY_BOOTLOG("NkHarmonyMainThread: nkmain returned");
        nkentseu::gState = nullptr;
        nkentseu::NkEntryRuntimeShutdown(true);
        delete args->state;
        delete args;

        NK_HARMONY_BOOTLOG("NkHarmonyMainThread: shutdown done");
        return nullptr;
    }

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// NAPI Init — appelé par le runtime HarmonyOS au chargement de la .so
// ─────────────────────────────────────────────────────────────────────────────

static napi_value NkHarmonyNapiInit(napi_env env, napi_value exports) {
    NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: enter");

    if (!nkentseu::NkEntryRuntimeInit(NK_APP_NAME)) {
        NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: NkEntryRuntimeInit failed");
        return exports;
    }

    // Construire le NkEntryState avec un vecteur d'arguments vide
    nkentseu::NkVector<nkentseu::NkString> args;
    args.PushBack(NK_APP_NAME);

    auto* state = new nkentseu::NkEntryState(
        nkentseu::traits::NkMove(args));
    nkentseu::NkApplyEntryAppName(*state, NK_APP_NAME);
    nkentseu::gState = state;

    // Lancer nkmain dans un thread séparé pour ne pas bloquer le thread UI
    auto* threadArgs  = new NkHarmonyMainArgs{ state };
    pthread_t thread;
    if (pthread_create(&thread, nullptr, NkHarmonyMainThread, threadArgs) != 0) {
        NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: pthread_create failed");
        delete threadArgs;
        delete state;
        nkentseu::gState = nullptr;
        nkentseu::NkEntryRuntimeShutdown(false);
        return exports;
    }
    pthread_detach(thread);

    NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: thread started");
    return exports;
}

// ─────────────────────────────────────────────────────────────────────────────
// Macro pour définir le module NAPI HarmonyOS
//
// Usage dans le fichier source de la NativeAbility :
//   NKENTSEU_HARMONY_DEFINE_MODULE(entry)
//   // "entry" doit correspondre au nom du module dans oh-package.json5
// ─────────────────────────────────────────────────────────────────────────────

#define NKENTSEU_HARMONY_DEFINE_MODULE(moduleName) \
    NAPI_MODULE(moduleName, NkHarmonyNapiInit)