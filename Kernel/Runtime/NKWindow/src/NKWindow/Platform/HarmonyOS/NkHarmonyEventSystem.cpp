// =============================================================================
// NkHarmonyEventSystem.cpp — HarmonyOS event system (STUB)
//
// Implémentation STUB : pour l'instant, le build NKWindow HarmonyOS doit
// passer sans device de test physique disponible. Les callbacks XComponent
// (touch, mouse, key, surface) reviendront sous forme complète quand un
// device HarmonyOS sera connecté pour test.
//
// L'ancien fichier complet utilisait :
//   - <ace/xcomponent/native_xcomponent_event.h>      (header retiré dans
//     les versions récentes du SDK OHOS — consolidé dans
//     <ace/xcomponent/native_interface_xcomponent.h>)
//   - constantes KEY_BACKSPACE/LEFT/RIGHT/UP/DOWN/END (manquantes du SDK)
//   - types NkTouch{Began,Moved,Ended,Cancelled}Event obsolètes (renommés
//     NkTouch{Begin,Move,End,Cancel}Event)
//   - constantes NK_MOUSE_BUTTON_{LEFT,MIDDLE,RIGHT} obsolètes
//
// La réécriture s'appuiera sur :
//   - OH_NativeXComponent_RegisterCallback (déjà fait dans NkHarmonyWindow)
//   - NkTouchPoint (membres id/clientX/clientY/screenX/screenY/pressure)
//   - NkKey (enum class — chiffres NK_NUMx, lettres NK_x, etc.)
//   - Noms d'événements corrects : NkKeyPressEvent / NkKeyReleaseEvent
//                                  NkMouseButtonPressEvent / ReleaseEvent
//                                  NkMouseMoveEvent
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"

#if defined(NKENTSEU_PLATFORM_HARMONYOS)
#include "NKWindow/Platform/HarmonyOS/NkHarmonyEventSystem.h"
// Header SDK OHOS gardé pour exposer OH_NativeXComponent (utilisé par
// NkEventSystemData et NkHarmonyOnSurfaceCreated dans NkHarmonyWindow.cpp).
#include <ace/xcomponent/native_interface_xcomponent.h>

#include "NKEvent/NkEventSystem.h"
#include "NKMemory/NkAllocator.h"

// =============================================================================
// Implémentation MINIMALE des méthodes par-plateforme de NkEventSystem.
//
// Chaque plateforme (Win32/XLib/Android/...) fournit Init/Shutdown/PumpOS/
// GetPlatformName/Enqueue_Public — sans elles, librenderdemo.so garde des
// symboles UNDEFINED et le dlopen NAPI échoue au chargement du HAP
// ("Error relocating ... NkEventSystem::PumpOS: symbol not found").
//
// Sur HarmonyOS il n'y a PAS de boucle d'événements à pomper côté C++ : les
// événements (surface, orientation, clavier...) sont POUSSÉS par les callbacks
// XComponent/ArkTS via Enqueue_Public (cf. NkHarmonyWindow.cpp et
// NkHarmonyOS.h). PumpOS est donc un no-op. Le routage touch/mouse/key complet
// reviendra avec la réécriture décrite dans l'en-tête d'origine (ci-dessous).
// =============================================================================

namespace nkentseu {

	bool NkEventSystem::Init() {
		if (mReady) {
			return true;
		}

		mData = memory::NkGetDefaultAllocator().New<NkEventSystemData>();
		if (mData == nullptr) {
			return false;
		}

		mTotalEventCount = 0;
		{
			NkScopedSpinLock lock(mQueueMutex);
			mEventQueue.Clear();
		}
		mPumping = false;
		mData->mInitialized = true;
		mReady = true;
		return true;
	}

	void NkEventSystem::Shutdown() {
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

		if (mData) {
			memory::NkGetDefaultAllocator().Delete(mData);
			mData = nullptr;
		}
	}

	void NkEventSystem::PumpOS() {
		// No-op : les événements HarmonyOS arrivent par callbacks (XComponent,
		// bridge ArkTS) qui appellent Enqueue_Public depuis leur propre thread.
	}

	const char *NkEventSystem::GetPlatformName() const noexcept {
		return "HarmonyOS";
	}

	void NkEventSystem::Enqueue_Public(NkEvent &evt, NkWindowId winId) {
		Enqueue(evt, winId);
	}

} // namespace nkentseu

#endif // NKENTSEU_PLATFORM_HARMONYOS
