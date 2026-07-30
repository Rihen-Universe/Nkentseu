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
#include "NKMemory/NkAllocator.h" // NkGetDefaultAllocator().New/Delete (regle maison : pas de new/delete)

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <napi/native_api.h>
#include <pthread.h>
#include <unistd.h> // usleep — attente de la surface XComponent avant nkmain()

#ifndef NK_APP_NAME
#define NK_APP_NAME "harmony_app"
#endif

#ifndef NK_HARMONY_BOOT_TAG
#define NK_HARMONY_BOOT_TAG "NkHarmonyBoot"
#endif

#define NK_HARMONY_BOOTLOG(...) logger.Infof(__VA_ARGS__)

namespace nkentseu {
	inline NkEntryState *gState = nullptr;

	// ─────────────────────────────────────────────────────────────────────────
	// Callbacks XComponent → NkHarmonyWindow
	//
	// NkHarmonyEventSystem.cpp est un STUB (cf. son en-tête) : l'enregistrement
	// des callbacks de surface est donc fait ICI, dans le TU unique de l'app
	// (ce header n'est inclus qu'une fois, par le fichier qui définit nkmain).
	// Les événements touch/mouse/key reviendront avec la réécriture du stub.
	// ─────────────────────────────────────────────────────────────────────────

	namespace harmonydetail {

		inline void OnSurfaceCreatedCB(OH_NativeXComponent *comp, void *window) {
			logger.Infof("NkHarmonyOS: OnSurfaceCreated (comp=%p win=%p)", (void *)comp, window);
			NkHarmonyOnSurfaceCreated(comp, static_cast<OHNativeWindow *>(window));
		}

		inline void OnSurfaceChangedCB(OH_NativeXComponent *comp, void *window) {
			NkHarmonyOnSurfaceChanged(comp, static_cast<OHNativeWindow *>(window));
		}

		inline void OnSurfaceDestroyedCB(OH_NativeXComponent *comp, void *window) {
			(void)window;
			NkHarmonyOnSurfaceDestroyed(comp);
		}

		inline void OnDispatchTouchEventCB(OH_NativeXComponent *comp, void *window) {
			// Stub : les événements tactiles seront routés vers NkWESystem quand
			// NkHarmonyEventSystem sera réécrit (cf. commentaire du stub).
			(void)comp;
			(void)window;
		}

	} // namespace harmonydetail

	inline void NkHarmonyRegisterXComponentCallbacks(OH_NativeXComponent *xcomp) {
		if (!xcomp) {
			return;
		}
		// La struct DOIT survivre à l'appel (le runtime garde le pointeur).
		static OH_NativeXComponent_Callback sCallbacks = {
			&harmonydetail::OnSurfaceCreatedCB,
			&harmonydetail::OnSurfaceChangedCB,
			&harmonydetail::OnSurfaceDestroyedCB,
			&harmonydetail::OnDispatchTouchEventCB,
		};
		OH_NativeXComponent_RegisterCallback(xcomp, &sCallbacks);
	}
} // namespace nkentseu

// ─────────────────────────────────────────────────────────────────────────────
// Thread de l'application (nkmain tourne dans un thread séparé pour ne pas
// bloquer le thread UI ArkTS)
// ─────────────────────────────────────────────────────────────────────────────

namespace {

	struct NkHarmonyMainArgs {
			nkentseu::NkEntryState *state = nullptr;
	};

	static void *NkHarmonyMainThread(void *arg) {
		NK_HARMONY_BOOTLOG("NkHarmonyMainThread: start");
		auto *args = static_cast<NkHarmonyMainArgs *>(arg);
		if (!args || !args->state) {
			NK_HARMONY_BOOTLOG("NkHarmonyMainThread: invalid args");
			return nullptr;
		}

		// Attendre la surface XComponent avant d'entrer dans nkmain() —
		// équivalent HarmonyOS de l'attente APP_CMD_INIT_WINDOW d'Android
		// (NkAndroid.h). Sans surface, NkWindow::GetSurfaceDesc() renverrait un
		// ohNativeWindow nul et l'init du device RHI échouerait immédiatement.
		{
			const int kMaxWaitMs = 20000;
			int waitedMs = 0;
			while (!nkentseu::NkHarmonySurfaceReady() && waitedMs < kMaxWaitMs) {
				usleep(10000); // 10 ms
				waitedMs += 10;
			}
			NK_HARMONY_BOOTLOG("NkHarmonyMainThread: surface wait done (ready=%d, %d ms)",
							   (int)nkentseu::NkHarmonySurfaceReady(), waitedMs);
		}

		nkmain(*args->state);

		NK_HARMONY_BOOTLOG("NkHarmonyMainThread: nkmain returned");
		nkentseu::gState = nullptr;
		nkentseu::NkEntryRuntimeShutdown(true);
		nkentseu::memory::NkGetDefaultAllocator().Delete(args->state);
		nkentseu::memory::NkGetDefaultAllocator().Delete(args);

		NK_HARMONY_BOOTLOG("NkHarmonyMainThread: shutdown done");
		return nullptr;
	}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// Hook applicatif OPTIONNEL (symbole faible) : permet a l'application d'ajouter
// ses propres exports NAPI (fonctions appelables depuis ArkTS) sans modifier ce
// header. Exemple (renderdemo) : export de `nkSetResMgr(resourceManager)` qui
// transmet le ResourceManager ArkTS au repli rawfile de NkFile (lecture des
// shaders packages dans resources/rawfile/ du HAP). Si l'app ne definit pas le
// symbole, le pointeur est nul et rien ne change (aucune dependance ajoutee).
// ─────────────────────────────────────────────────────────────────────────────
extern "C" void NkHarmonyOnNapiInitExtra(napi_env env, napi_value exports) __attribute__((weak));

// ─────────────────────────────────────────────────────────────────────────────
// NAPI Init — appelé par le runtime HarmonyOS au chargement de la .so
// ─────────────────────────────────────────────────────────────────────────────

static napi_value NkHarmonyNapiInit(napi_env env, napi_value exports) {
	NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: enter");

	// ── Exports applicatifs additionnels (hook faible, cf. ci-dessus) ────────
	// Appele a CHAQUE chargement (avant la garde anti double-init) : les exports
	// doivent exister sur l'objet retourne au XComponent, meme si nkmain tourne.
	if (NkHarmonyOnNapiInitExtra) {
		NkHarmonyOnNapiInitExtra(env, exports);
		NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: exports applicatifs enregistres (hook)");
	}

	// ── Récupérer l'OH_NativeXComponent depuis exports ───────────────────────
	// Quand la .so est chargée par un XComponent ArkTS (libraryname), le
	// runtime attache l'instance native sous la propriété OH_NATIVE_XCOMPONENT_OBJ
	// ("__NATIVE_XCOMPONENT_OBJ"). C'est LE pont surface → C++ : sans cet
	// enregistrement, OnSurfaceCreated n'est jamais appelé et nkmain() ne
	// reçoit jamais de fenêtre native.
	{
		napi_value exportInstance = nullptr;
		OH_NativeXComponent *xcomp = nullptr;
		if (napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) == napi_ok &&
			exportInstance != nullptr &&
			napi_unwrap(env, exportInstance, reinterpret_cast<void **>(&xcomp)) == napi_ok && xcomp != nullptr) {
			nkentseu::NkHarmonyRegisterXComponentCallbacks(xcomp);
			NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: XComponent callbacks registered (xcomp=%p)", (void *)xcomp);
		} else {
			NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: no XComponent in exports (module importe sans XComponent ?)");
		}
	}

	// Garde anti double-init : le module peut être chargé plusieurs fois
	// (requireNapi côté bridge + libraryname du XComponent). nkmain() ne doit
	// démarrer qu'une seule fois ; les chargements suivants ne servent qu'à
	// (ré)enregistrer les callbacks XComponent ci-dessus.
	static bool sMainStarted = false;
	if (sMainStarted) {
		NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: nkmain deja demarre, enregistrement seul");
		return exports;
	}

	if (!nkentseu::NkEntryRuntimeInit(NK_APP_NAME)) {
		NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: NkEntryRuntimeInit failed");
		return exports;
	}

	// Construire le NkEntryState avec un vecteur d'arguments vide
	nkentseu::NkVector<nkentseu::NkString> args;
	args.PushBack(NK_APP_NAME);

	auto *state = nkentseu::memory::NkGetDefaultAllocator().New<nkentseu::NkEntryState>(nkentseu::traits::NkMove(args));
	nkentseu::NkApplyEntryAppName(*state, NK_APP_NAME);
	nkentseu::gState = state;

	// Lancer nkmain dans un thread séparé pour ne pas bloquer le thread UI
	auto *threadArgs = nkentseu::memory::NkGetDefaultAllocator().New<NkHarmonyMainArgs>(NkHarmonyMainArgs{state});
	pthread_t thread;
	if (pthread_create(&thread, nullptr, NkHarmonyMainThread, threadArgs) != 0) {
		NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: pthread_create failed");
		nkentseu::memory::NkGetDefaultAllocator().Delete(threadArgs);
		nkentseu::memory::NkGetDefaultAllocator().Delete(state);
		nkentseu::gState = nullptr;
		nkentseu::NkEntryRuntimeShutdown(false);
		return exports;
	}
	pthread_detach(thread);
	sMainStarted = true;

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

#define NKENTSEU_HARMONY_DEFINE_MODULE(moduleName) NAPI_MODULE(moduleName, NkHarmonyNapiInit)