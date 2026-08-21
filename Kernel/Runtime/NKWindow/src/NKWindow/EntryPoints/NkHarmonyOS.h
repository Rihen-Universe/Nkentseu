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
// Definition COMPLETE de NkWindow et evenements tactiles : le routage du
// toucher (OnDispatchTouchEventCB) appelle win->GetId() et construit des
// NkTouch*Event.
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWESystem.h"
#include "NKEvent/NkTouchEvent.h"
// Lecture des ressources empaquetees dans le HAP (resources/rawfile).
#include "NKFileSystem/NkFile.h"
#include <rawfile/raw_file_manager.h>
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

		// Le tactile est la SEULE entrée d'une application HarmonyOS de ce type :
		// il n'y a ni clavier ni souris sur téléphone. Ce callback était un stub
		// vide — les applications recevaient donc l'image, mais aucun geste ne
		// leur parvenait, alors que le même code fonctionne sur Android.
		//
		// On produit exactement les mêmes événements que le portage Android
		// (NkAndroidEventSystem), pour que le code applicatif n'ait pas à savoir
		// sur quelle plateforme il tourne.
		inline void OnDispatchTouchEventCB(OH_NativeXComponent *comp, void *window) {
			if (!comp || !window) {
				return;
			}

			OH_NativeXComponent_TouchEvent brut{};
			const int32_t lu = OH_NativeXComponent_GetTouchEvent(comp, window, &brut);

			// Trace du PREMIER contact recu. Sans elle, un tactile inerte laisse
			// deux hypotheses indiscernables : le systeme ne delivre rien, ou nous
			// recevons bien les evenements et c'est la suite de la chaine qui les
			// perd. Une seule ligne, au premier appel.
			{
				static bool premier = true;
				if (premier) {
					premier = false;
					NK_HARMONY_BOOTLOG("tactile : premier evenement recu (lecture=%d type=%d points=%u)", (int)lu,
									   (int)brut.type, (unsigned)brut.numPoints);
				}
			}

			if (lu != 0) {
				return;
			}

			NkWindow *win = NkHarmonyGetWindowForXComponent(comp);
			if (!win) {
				NK_HARMONY_BOOTLOG("tactile : AUCUNE fenetre associee au XComponent — evenement perdu");
				return;
			}

			// Le type porté par l'ÉVÉNEMENT décrit le geste dans son ensemble ;
			// chaque point garde le sien pour le multi-touch.
			const auto versPhase = [](OH_NativeXComponent_TouchEventType t) {
				switch (t) {
					case OH_NATIVEXCOMPONENT_DOWN: return NkTouchPhase::NK_TOUCH_PHASE_BEGAN;
					case OH_NATIVEXCOMPONENT_MOVE: return NkTouchPhase::NK_TOUCH_PHASE_MOVED;
					case OH_NATIVEXCOMPONENT_UP: return NkTouchPhase::NK_TOUCH_PHASE_ENDED;
					case OH_NATIVEXCOMPONENT_CANCEL: return NkTouchPhase::NK_TOUCH_PHASE_CANCELLED;
					default: return NkTouchPhase::NK_TOUCH_PHASE_STATIONARY;
				}
			};

			NkTouchPoint points[NK_MAX_TOUCH_POINTS];
			uint32 count = 0;
			const uint32 total = brut.numPoints < NK_MAX_TOUCH_POINTS ? brut.numPoints : NK_MAX_TOUCH_POINTS;
			for (uint32 i = 0; i < total; ++i) {
				const OH_NativeXComponent_TouchPoint &p = brut.touchPoints[i];
				NkTouchPoint &dst = points[count++];
				dst.id = static_cast<uint64>(p.id);
				dst.phase = versPhase(p.type);
				dst.clientX = p.x; // coordonnees LOCALES au XComponent
				dst.clientY = p.y;
				dst.screenX = p.screenX;
				dst.screenY = p.screenY;
				dst.pressure = p.force;
			}
			// Un evenement UP/CANCEL peut arriver avec numPoints a zero : on
			// synthetise alors le point porte par l'evenement lui-meme, sinon le
			// relachement serait perdu et l'application resterait « doigt pose ».
			if (count == 0) {
				NkTouchPoint &dst = points[count++];
				dst.id = 0;
				dst.phase = versPhase(brut.type);
				dst.clientX = brut.x;
				dst.clientY = brut.y;
				dst.screenX = brut.screenX;
				dst.screenY = brut.screenY;
				dst.pressure = brut.force;
			}

			switch (brut.type) {
				case OH_NATIVEXCOMPONENT_DOWN: {
					NkTouchBeginEvent evt(points, count);
					NkWESystem::Events().Enqueue_Public(evt, win->GetId());
					break;
				}
				case OH_NATIVEXCOMPONENT_MOVE: {
					NkTouchMoveEvent evt(points, count);
					NkWESystem::Events().Enqueue_Public(evt, win->GetId());
					break;
				}
				case OH_NATIVEXCOMPONENT_UP: {
					NkTouchEndEvent evt(points, count);
					NkWESystem::Events().Enqueue_Public(evt, win->GetId());
					break;
				}
				case OH_NATIVEXCOMPONENT_CANCEL: {
					NkTouchCancelEvent evt(points, count);
					NkWESystem::Events().Enqueue_Public(evt, win->GetId());
					break;
				}
				default: break;
			}
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
// Pont ArkTS → natif : ce que NkHarmonyBridge.ts appelle
//
// Le XComponent apporte la SURFACE, et rien d'autre. Tout le reste de l'état de
// la fenêtre — zone sûre (encoche, barre de gestes), orientation, clavier
// virtuel, focus, mode fenêtré sur PC 2in1 — n'existe que côté ArkTS et doit
// traverser NAPI pour atteindre le C++.
//
// Sans les exports ci-dessous, le pont s'exécutait, appelait
// `nkNative.onSafeAreaChanged?.(...)` — et l'appel optionnel ne trouvait rien.
// Aucune erreur, aucune trace : GetSafeAreaInsets() renvoyait des zéros pour
// toujours, et une interface plein écran passait sous l'encoche.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

	// Lecture d'un argument numérique, avec repli à 0. Les valeurs viennent
	// d'ArkTS où tout nombre est un double.
	inline double NkHarmonyNapiNombre(napi_env env, napi_value v) {
		double d = 0.0;
		napi_get_value_double(env, v, &d);
		return d;
	}

	inline bool NkHarmonyNapiBool(napi_env env, napi_value v) {
		bool b = false;
		napi_get_value_bool(env, v, &b);
		return b;
	}

	// Passerelle vers le ResourceManager de l'application.
	//
	// C'est le SEUL moyen de lire les fichiers empaquetés dans resources/rawfile
	// du HAP : la libc ne les voit pas. Sans lui, toute ouverture de ressource
	// échoue — constaté sur Pong : « 0/156 textures décodées, 156 manquantes »,
	// donc une scène géométriquement correcte mais entièrement noire, sans la
	// moindre erreur de rendu.
	//
	// L'export vit ici, dans le moteur, et non dans chaque application : la page
	// générée par Jenga l'appelle sur le onLoad du XComponent.
	napi_value NkHarmonyNapiSetResMgr(napi_env env, napi_callback_info info) {
		size_t argc = 1;
		napi_value args[1] = {};
		napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
		if (argc < 1) {
			return nullptr;
		}
		NativeResourceManager *mgr = OH_ResourceManager_InitNativeResourceManager(env, args[0]);
		if (mgr) {
			nkentseu::NkFile::SetHarmonyResourceManager(mgr);
			NK_HARMONY_BOOTLOG("NkHarmonyNapiSetResMgr: ResourceManager installe (%p)", (void *)mgr);
		} else {
			NK_HARMONY_BOOTLOG("NkHarmonyNapiSetResMgr: OH_ResourceManager_InitNativeResourceManager a echoue");
		}
		return nullptr;
	}

	napi_value NkHarmonyNapiSafeArea(napi_env env, napi_callback_info info) {
		size_t argc = 4;
		napi_value args[4] = {};
		napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
		if (argc >= 4) {
			nkentseu::NkHarmonyOnSafeAreaChanged(
				static_cast<float>(NkHarmonyNapiNombre(env, args[0])),
				static_cast<float>(NkHarmonyNapiNombre(env, args[1])),
				static_cast<float>(NkHarmonyNapiNombre(env, args[2])),
				static_cast<float>(NkHarmonyNapiNombre(env, args[3])));
		}
		return nullptr;
	}

	napi_value NkHarmonyNapiOrientation(napi_env env, napi_callback_info info) {
		size_t argc = 1;
		napi_value args[1] = {};
		napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
		if (argc >= 1) {
			nkentseu::NkHarmonyOnOrientationChanged(
				static_cast<nkentseu::int32>(NkHarmonyNapiNombre(env, args[0])));
		}
		return nullptr;
	}

	napi_value NkHarmonyNapiClavier(napi_env env, napi_callback_info info) {
		size_t argc = 2;
		napi_value args[2] = {};
		napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
		if (argc >= 2) {
			nkentseu::NkHarmonyOnVirtualKeyboardChanged(
				NkHarmonyNapiBool(env, args[0]),
				static_cast<nkentseu::uint32>(NkHarmonyNapiNombre(env, args[1])));
		}
		return nullptr;
	}

	napi_value NkHarmonyNapiFocus(napi_env env, napi_callback_info info) {
		size_t argc = 1;
		napi_value args[1] = {};
		napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
		if (argc >= 1) {
			nkentseu::NkHarmonyOnWindowFocusChanged(NkHarmonyNapiBool(env, args[0]));
		}
		return nullptr;
	}

	napi_value NkHarmonyNapiMinimise(napi_env, napi_callback_info) {
		nkentseu::NkHarmonyOnWindowMinimized();
		return nullptr;
	}

	napi_value NkHarmonyNapiMaximise(napi_env, napi_callback_info) {
		nkentseu::NkHarmonyOnWindowMaximized();
		return nullptr;
	}

	napi_value NkHarmonyNapiRestaure(napi_env, napi_callback_info) {
		nkentseu::NkHarmonyOnWindowRestored();
		return nullptr;
	}

	// Les noms exposés doivent correspondre EXACTEMENT à ceux qu'appelle
	// NkHarmonyBridge.ts : côté ArkTS l'appel est optionnel (`?.`), donc une
	// faute de frappe ne produirait aucune erreur — juste un silence.
	inline void NkHarmonyExporterPont(napi_env env, napi_value exports) {
		const napi_property_descriptor props[] = {
			{"onSafeAreaChanged", nullptr, NkHarmonyNapiSafeArea, nullptr, nullptr, nullptr, napi_enumerable, nullptr},
			{"onOrientationChanged", nullptr, NkHarmonyNapiOrientation, nullptr, nullptr, nullptr, napi_enumerable, nullptr},
			{"onVirtualKeyboardChanged", nullptr, NkHarmonyNapiClavier, nullptr, nullptr, nullptr, napi_enumerable, nullptr},
			{"onWindowFocusChanged", nullptr, NkHarmonyNapiFocus, nullptr, nullptr, nullptr, napi_enumerable, nullptr},
			{"onWindowMinimized", nullptr, NkHarmonyNapiMinimise, nullptr, nullptr, nullptr, napi_enumerable, nullptr},
			{"onWindowMaximized", nullptr, NkHarmonyNapiMaximise, nullptr, nullptr, nullptr, napi_enumerable, nullptr},
			{"onWindowRestored", nullptr, NkHarmonyNapiRestaure, nullptr, nullptr, nullptr, napi_enumerable, nullptr},
			{"nkSetResMgr", nullptr, NkHarmonyNapiSetResMgr, nullptr, nullptr, nullptr, napi_enumerable, nullptr},
		};
		const napi_status st = napi_define_properties(env, exports, sizeof(props) / sizeof(props[0]), props);
		NK_HARMONY_BOOTLOG("NkHarmonyExporterPont: %d fonctions exportees (status=%d)",
						   (int)(sizeof(props) / sizeof(props[0])), (int)st);
	}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// NAPI Init — appelé par le runtime HarmonyOS au chargement de la .so
// ─────────────────────────────────────────────────────────────────────────────

static napi_value NkHarmonyNapiInit(napi_env env, napi_value exports) {
	// DATE DE COMPILATION de la bibliotheque native, des la premiere ligne.
	//
	// Sur appareil, rien ne distingue deux versions d'une meme application : on
	// installe, on lance, et l'on discute d'un comportement sans savoir quel
	// binaire tourne. Cela a coute une longue confusion — un ecran juge « sans
	// changement » alors qu'il s'agissait d'un build anterieur aux correctifs.
	// Cette ligne tranche en une seconde, pour tout le monde.
	NK_HARMONY_BOOTLOG("NkHarmonyNapiInit: enter | natif compile le " __DATE__ " a " __TIME__);

	// ── Pont ArkTS : zone sure, orientation, clavier, focus, fenetrage PC ────
	// Enregistre a CHAQUE chargement, avant toute garde : l'objet rendu au
	// XComponent comme celui obtenu par import doivent porter ces fonctions.
	NkHarmonyExporterPont(env, exports);

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

// ─────────────────────────────────────────────────────────────────────────────
// Enregistrement AUTOMATIQUE du module natif
//
// Le XComponent de la page ArkTS réclame une bibliothèque par son `libraryname`,
// et le runtime ne charge la .so que s'il y trouve un module NAPI enregistré
// SOUS CE NOM EXACT. Tant que l'application devait écrire elle-même
// NKENTSEU_HARMONY_DEFINE_MODULE, l'oublier donnait une application qui
// s'installe, affiche son écran de démarrage… et n'exécute jamais une ligne de
// C++, sans le moindre message. Mou et Pong étaient dans ce cas.
//
// Le nom vient de Jenga (NK_HARMONY_MODULE_NAME = nom de la cible), qui écrit
// déjà le `libraryname` de la page : les deux ne peuvent donc pas diverger.
//
// On passe par napi_module_register plutôt que par la macro NAPI_MODULE parce
// que le nom nous arrive sous forme de CHAÎNE, pas d'identifiant.
// ─────────────────────────────────────────────────────────────────────────────

#ifndef NK_HARMONY_MODULE_NAME
#define NK_HARMONY_MODULE_NAME entry
#endif

// ⚠️ Ne PAS écrire NAPI_MODULE(NK_HARMONY_MODULE_NAME, …) : la macro du SDK
// transforme son argument en chaîne SANS l'expanser, le module s'enregistrerait
// donc sous le nom littéral « NK_HARMONY_MODULE_NAME » et le XComponent ne le
// trouverait jamais. On construit la structure à la main, où le nom est une
// chaîne que l'on peut composer.

#define NK_HARMONY_STR_(x) #x
#define NK_HARMONY_STR(x) NK_HARMONY_STR_(x)

namespace {

	napi_module gNkHarmonyModule = {
		/* nm_version       */ 1,
		/* nm_flags         */ 0,
		/* nm_filename      */ nullptr,
		/* nm_register_func */ NkHarmonyNapiInit,
		/* nm_modname       */ NK_HARMONY_STR(NK_HARMONY_MODULE_NAME),
		/* nm_priv          */ nullptr,
		/* reserved         */ {nullptr},
	};

	// Enregistrement au chargement de la bibliothèque, avant que le runtime ne
	// cherche le module — c'est ce que fait NAPI_MODULE elle-même.
	__attribute__((constructor)) void NkHarmonyRegisterModule() {
		napi_module_register(&gNkHarmonyModule);
	}

} // anonymous namespace

// Conservée pour les applications qui l'appellent encore : le module est
// enregistré ci-dessus, et un second enregistrement sous le même nom serait
// refusé. La macro ne fait donc plus rien.
#define NKENTSEU_HARMONY_DEFINE_MODULE(moduleName)