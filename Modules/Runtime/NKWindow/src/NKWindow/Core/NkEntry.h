#pragma once

// =============================================================================
// NkEntry.h
// NkEntryState â€” conteneur des arguments de dÃ©marrage par plateforme.
// La variable globale gState est crÃ©Ã©e par le point d'entrÃ©e de plateforme
// et dÃ©truite aprÃ¨s le retour de nkmain().
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTraits.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMemory/NkMemory.h"
#include "NkSystem.h"

// Inclusions conditionnelles des types natifs
#if (defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP)) || defined(NKENTSEU_PLATFORM_XBOX)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(NKENTSEU_WINDOWING_WAYLAND)
// Wayland: no global native handle in entry state.
#elif defined(NKENTSEU_WINDOWING_XCB)
#include <xcb/xcb.h>
#elif defined(NKENTSEU_WINDOWING_XLIB)
#include <X11/Xlib.h>
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#include <android_native_app_glue.h>
#endif

/**
 * @brief Namespace nkentseu.
 */
namespace nkentseu {

	// ---------------------------------------------------------------------------
	// NkEntryState
	// ---------------------------------------------------------------------------

	struct NkEntryState {
		// --- Arguments communs Ã  toutes les plateformes ---
		NkString appName;
		NkVector<NkString> args;

		// --- Handles natifs optionnels (nullptr / 0 sur les autres plateformes) ---

	#if defined(NKENTSEU_PLATFORM_UWP)
		// UWP : CoreWindow est fourni par le runtime (entrypoint UWP).
		void *uwpCoreWindow = nullptr;

		NkEntryState() = default;
		explicit NkEntryState(const NkVector<NkString> &a, void *coreWindow = nullptr)
			: args(traits::NkMove(a)), uwpCoreWindow(coreWindow) {
		}

	#elif defined(NKENTSEU_PLATFORM_XBOX)
		void *xboxNativeWindow = nullptr;
		HINSTANCE hInstance = nullptr;
		HINSTANCE hPrevInstance = nullptr;
		LPSTR lpCmdLine = nullptr;
		int nCmdShow = 1;

		NkEntryState(HINSTANCE hi, HINSTANCE hpi, LPSTR cmd, int ncmd, const NkVector<NkString> &a,
					 void *nativeWindow = nullptr)
			: xboxNativeWindow(nativeWindow), hInstance(hi), hPrevInstance(hpi), lpCmdLine(cmd), nCmdShow(ncmd), args(traits::NkMove(a)) {
		}

	#elif defined(NKENTSEU_PLATFORM_WINDOWS)
		HINSTANCE hInstance = nullptr;
		HINSTANCE hPrevInstance = nullptr;
		LPSTR lpCmdLine = nullptr;
		int nCmdShow = 1;

		NkEntryState(HINSTANCE hi, HINSTANCE hpi, LPSTR cmd, int ncmd, const NkVector<NkString> &a)
			: hInstance(hi), hPrevInstance(hpi), lpCmdLine(cmd), nCmdShow(ncmd), args(traits::NkMove(a)) {
		}

	#elif defined(NKENTSEU_WINDOWING_WAYLAND)
		// Wayland : wl_display est créé par fenêtre dans NkWaylandWindow::Create()
		// — aucun handle global à transmettre ici.
		NkEntryState() = default;
		explicit NkEntryState(const NkVector<NkString> &a) : args(traits::NkMove(a)) {
		}

	#elif defined(NKENTSEU_WINDOWING_XCB)
		xcb_connection_t *connection = nullptr;
		xcb_screen_t *screen = nullptr;

		NkEntryState(xcb_connection_t *c, xcb_screen_t *s, const NkVector<NkString> &a)
			: connection(c), screen(s), args(traits::NkMove(a)) {
		}

	#elif defined(NKENTSEU_WINDOWING_XLIB)
		Display *display = nullptr;

		explicit NkEntryState(Display *d, const NkVector<NkString> &a) : display(d), args(traits::NkMove(a)) {
		}

	#elif defined(NKENTSEU_PLATFORM_ANDROID)
		android_app *androidApp = nullptr;

		explicit NkEntryState(android_app *app, NkVector<NkString> a) : androidApp(app), args(traits::NkMove(a)) {
		}

	#else
		NkEntryState() = default;
		explicit NkEntryState(const NkVector<NkString> &a) : args(traits::NkMove(a)) {
		}
	#endif

		// Accesseurs gÃ©nÃ©riques
		const NkVector<NkString> &GetArgs() const {
			return args;
		}
		const NkString &GetAppName() const {
			return appName;
		}
	};

	// ---------------------------------------------------------------------------
	// Variable globale (dÃ©finie dans chaque entry point de plateforme)
	// ---------------------------------------------------------------------------

	extern NkEntryState *gState;

	// -----------------------------------------------------------------------
	// Runtime lifecycle helpers pour les entrypoints
	// -----------------------------------------------------------------------

	using NkEntryAppDataUpdater = void (*)(NkAppData&);
	inline void NkSetEntryAppDataUpdater(NkEntryAppDataUpdater updater);

	/**
	 * @brief Enregistre automatiquement un updater d'AppData pendant
	 * l'initialisation statique du TU.
	 *
	 * Usage:
	 *   static void Configure(NkAppData& d) { d.appName = "MyApp"; }
	 *   NK_REGISTER_ENTRY_APPDATA_UPDATER(Configure);
	 */
	struct NkEntryAppDataAutoRegister {
		explicit NkEntryAppDataAutoRegister(NkEntryAppDataUpdater updater) {
			NkSetEntryAppDataUpdater(updater);
		}
	};

	inline NkEntryAppDataUpdater& NkEntryAppDataUpdaterSlot() {
		static NkEntryAppDataUpdater sUpdater = nullptr;
		return sUpdater;
	}

	inline NkAppData& NkEntryAppDataOverrideSlot() {
		static NkAppData sData{};
		return sData;
	}

	inline nk_bool& NkEntryHasAppDataOverrideSlot() {
		static nk_bool sHasOverride = false;
		return sHasOverride;
	}

	inline NkAppData& NkEntryRuntimeAppDataSlot() {
		static NkAppData sRuntimeAppData{};
		return sRuntimeAppData;
	}

	inline void NkSetEntryAppDataUpdater(NkEntryAppDataUpdater updater) {
		NkEntryAppDataUpdaterSlot() = updater;
	}

	inline void NkRegisterEntryAppDataUpdater(NkEntryAppDataUpdater updater) {
		NkSetEntryAppDataUpdater(updater);
	}

	inline void NkSetEntryAppData(const NkAppData& data) {
		NkEntryAppDataOverrideSlot() = data;
		NkEntryHasAppDataOverrideSlot() = true;
	}

	inline void NkClearEntryAppDataOverride() {
		NkEntryHasAppDataOverrideSlot() = false;
	}

	inline NkAppData NkBuildEntryAppData(const char* defaultAppName = nullptr) {
		const nk_bool hasOverride = NkEntryHasAppDataOverrideSlot();
		NkAppData data = hasOverride ? NkEntryAppDataOverrideSlot() : NkAppData{};
		if ((!hasOverride || data.appName.Empty()) && defaultAppName && defaultAppName[0] != '\0') {
			data.appName = defaultAppName;
		}

		NkEntryAppDataUpdater updater = NkEntryAppDataUpdaterSlot();
		if (updater) {
			updater(data);
		}
		return data;
	}

	inline const NkAppData& NkGetEntryRuntimeAppData() {
		return NkEntryRuntimeAppDataSlot();
	}

	inline void NkApplyEntryAppName(NkEntryState& state, const char* fallbackAppName = nullptr) {
		const NkString& runtimeAppName = NkGetEntryRuntimeAppData().appName;
		if (!runtimeAppName.Empty()) {
			state.appName = runtimeAppName;
			return;
		}

		if (fallbackAppName && fallbackAppName[0] != '\0') {
			state.appName = fallbackAppName;
		}
	}

	inline nk_bool NkEntryRuntimeInit(const char* defaultAppName = nullptr) {
		memory::NkMemorySystem::Instance().Initialize(nullptr);

		const NkAppData appData = NkBuildEntryAppData(defaultAppName);
		NkEntryRuntimeAppDataSlot() = appData;
		if (!NkInitialise(appData)) {
			NkEntryRuntimeAppDataSlot() = NkAppData{};
			memory::NkMemorySystem::Instance().Shutdown(true);
			return false;
		}
		return true;
	}

	inline void NkEntryRuntimeShutdown(nk_bool reportLeaks = true) {
		// Ferme proprement le runtime fenêtre/événements (idempotent).
		NkClose();
		// Puis vide le système mémoire + GC global.
		memory::NkMemorySystem::Instance().Shutdown(reportLeaks);
		NkEntryRuntimeAppDataSlot() = NkAppData{};
	}

	#if defined(NKENTSEU_PLATFORM_UWP)
	// API runtime exposÃ©e par l'entrypoint UWP (NkUWP.h) et utilisÃ©e
	// par le backend UWP dans NkUWPWindow/NkUWPEventSystem.
	bool  NkUWPIsCoreWindowReady();
	void *NkUWPGetCoreWindowHandle();
	void  NkUWPPumpSystemEvents();
	#endif

	#if defined(NKENTSEU_PLATFORM_XBOX)
	// API runtime exposée par l'entrypoint Xbox (NkXbox.h) et utilisée
	// par le backend Xbox dans NkXboxWindow/NkXboxEventSystem.
	bool  NkXboxIsNativeWindowReady();
	void *NkXboxGetNativeWindowHandle();
	void  NkXboxPumpSystemEvents();
	#endif

} // namespace nkentseu

#ifndef NKENTSEU_INTERNAL_CONCAT_IMPL_
#define NKENTSEU_INTERNAL_CONCAT_IMPL_(a, b) a##b
#endif

#ifndef NKENTSEU_INTERNAL_CONCAT_
#define NKENTSEU_INTERNAL_CONCAT_(a, b) NKENTSEU_INTERNAL_CONCAT_IMPL_(a, b)
#endif

/**
 * @brief Macro utilitaire: enregistre un callback d'AppData avant l'entrée main.
 *
 * À placer dans le même .cpp que nkmain():
 *   static void ConfigureAppData(nkentseu::NkAppData& d) { ... }
 *   NK_REGISTER_ENTRY_APPDATA_UPDATER(ConfigureAppData)
 */
#define NK_REGISTER_ENTRY_APPDATA_UPDATER(fn)                                                    \
	namespace {                                                                                   \
		const nkentseu::NkEntryAppDataAutoRegister                                                \
			NKENTSEU_INTERNAL_CONCAT_(gNkEntryAppDataAutoRegister_, __LINE__)(fn);               \
	}

/**
 * @brief Définit AppData (1 seul paramètre) et enregistre automatiquement son updater.
 *
 * Exemple:
 *   nkentseu::NkAppData appData{};
 *   appData.appName = "Sandbox";
 *   appData.enableEventLogging = true;
 *   NKENTSEU_DEFINE_APP_DATA(appData);
 */
#define NKENTSEU_DEFINE_APP_DATA(appData)                                                        \
	static void NKENTSEU_INTERNAL_CONCAT_(NkConfigureAppData_, __LINE__)(nkentseu::NkAppData& d) { \
		d = appData;                                                                              \
	}                                                                                             \
	NK_REGISTER_ENTRY_APPDATA_UPDATER(NKENTSEU_INTERNAL_CONCAT_(NkConfigureAppData_, __LINE__))

// Alias lisible (1 seul paramètre).
#define NKENTSEU_APP_DATA_DEFINED(appData) NKENTSEU_DEFINE_APP_DATA(appData)


// ---------------------------------------------------------------------------
// Prototype de la fonction utilisateur Ã  implÃ©menter
// ---------------------------------------------------------------------------

int nkmain(const nkentseu::NkEntryState &state);
