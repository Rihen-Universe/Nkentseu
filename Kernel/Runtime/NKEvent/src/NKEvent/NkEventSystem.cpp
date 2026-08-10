#include "pch.h"
// =============================================================================
// NkEventSystem.cpp â€” mÃ©thodes cross-plateforme
//
// Corrections appliquÃ©es :
//   CORRECTION 1 : NkGamepadSystem::Instance() â†’ NkSystem::Gamepads()
//   CORRECTION 2 : PollEvent() pointer lifetime documentÃ© + PollEventCopy() ajoutÃ©
//   CORRECTION 3 : ring buffer dual-prioritÃ© (HIGH no-drop / NORMAL drop-oldest)
//   CORRECTION 5 : assertions thread ID sur PollEvent() et PollEvents()
// =============================================================================

#include "NkEventSystem.h"
#include "NkGamepadSystem.h"
#include "NKPlatform/NkPlatformDetect.h"
#include <cassert>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

namespace nkentseu {

	namespace {
		// Thread identity without STL thread wrapper: each thread has its own TLS address.
		uint64 NkCurrentThreadTag() noexcept {
			thread_local uint8 sThreadLocalTag = 0;
			return static_cast<uint64>(reinterpret_cast<uintptr>(&sThreadLocalTag));
		}
	} // namespace

	// =============================================================================
	// Constructeur / Destructeur
	// =============================================================================

	NkEventSystem::NkEventSystem() {
		// DÃ©fensif: certains toolchains peuvent laisser bucket_count() Ã  0
		// sur map vide, ce qui peut provoquer un modulo par zÃ©ro au 1er operator[].
		mWindowCallbacks.Rehash(32);
		mTypedCallbacks.Rehash(static_cast<nk_size>(NkEventType::NK_EVENT_COUNT));
		mTypedCallbacksWithToken.Rehash(static_cast<nk_size>(NkEventType::NK_EVENT_COUNT));

		// PrÃ©-crÃ©e les buckets pour tous les types d'Ã©vÃ©nements afin d'Ã©viter
		// des insertions ultÃ©rieures qui peuvent copier/dÃ©placer des callables
		// enregistrÃ©s (chemin instable observÃ© sur Android x86).
		for (uint32 i = 0; i < static_cast<uint32>(NkEventType::NK_EVENT_COUNT); ++i) {
			const auto type = static_cast<NkEventType::Value>(i);
			mTypedCallbacks.Insert(type, NkVector<NkEventCallback>{});
			mTypedCallbacksWithToken.Insert(type, NkVector<TokenizedCallback>{});
		}
	}

	NkEventSystem::~NkEventSystem() {
		Shutdown();
	}

	// =============================================================================
	// Shutdown
	// WASM override: NkEmscriptenEventSystem.cpp fournit sa propre version de Shutdown()
	// qui dÃ©senregistre les callbacks Emscripten avant de nettoyer.
	// =============================================================================

#if !defined(NKENTSEU_PLATFORM_EMSCRIPTEN) && !defined(__EMSCRIPTEN__) &&                                              \
	!(defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XCB) &&                                           \
	  !defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)) &&                                                                 \
	!(defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND) &&                                       \
	  !defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)) &&                                                                 \
	!(defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XLIB) &&                                          \
	  !defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)) &&                                                                 \
	!defined(NKENTSEU_PLATFORM_ANDROID) && !defined(NKENTSEU_PLATFORM_MACOS) && !defined(NKENTSEU_PLATFORM_IOS) &&    \
	!defined(NKENTSEU_PLATFORM_HARMONYOS)
	// HarmonyOS : Shutdown() est fourni par NkHarmonyEventSystem.cpp (libère
	// aussi mData, alloué par son Init()) — éviter le doublon au link.
	// macOS/iOS : Shutdown() est fourni par le backend Cocoa/UIKit
	// (NkCocoaEventSystem.mm / NkUIKitEventSystem.mm) — éviter le doublon au link.
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
		mPumpThreadId = 0; // reset thread ID
		mReady = false;
	}
#endif // !WASM

	// =============================================================================
	// Callbacks
	// =============================================================================

	void NkEventSystem::SetWindowCallback(NkWindowId id, NkEventCallback cb) {
		mWindowCallbacks[id] = traits::NkMove(cb);
	}

	void NkEventSystem::RemoveWindowCallback(NkWindowId id) {
		mWindowCallbacks.Erase(id);
	}

	void NkEventSystem::SetGlobalCallback(NkGlobalEventCallback cb) {
		mGlobalCallback = traits::NkMove(cb);
	}

	void NkEventSystem::AddEventCallbackRaw(NkEventType::Value type, NkEventCallback callback) {
		NK_EVENTSYS_ANDROID_TRACE("[AddEventCallbackRaw] enter this=%p type=%u buckets=%llu map_size=%llu",
								  static_cast<void *>(this), static_cast<unsigned>(type),
								  static_cast<unsigned long long>(mTypedCallbacks.BucketCount()),
								  static_cast<unsigned long long>(mTypedCallbacks.Size()));

		if (!callback) {
			NK_EVENTSYS_ANDROID_TRACE("[AddEventCallbackRaw] empty callback ignored type=%u",
									  static_cast<unsigned>(type));
			return;
		}

		if (mTypedCallbacks.BucketCount() == 0) {
			NK_EVENTSYS_ANDROID_TRACE("[AddEventCallbackRaw] bucket_count=0 -> rehash");
			mTypedCallbacks.Rehash(static_cast<nk_size>(NkEventType::NK_EVENT_COUNT));
			NK_EVENTSYS_ANDROID_TRACE("[AddEventCallbackRaw] rehash done buckets=%llu",
									  static_cast<unsigned long long>(mTypedCallbacks.BucketCount()));
		}

		auto *callbacks = mTypedCallbacks.Find(type);
		if (!callbacks) {
			NK_EVENTSYS_ANDROID_TRACE("[AddEventCallbackRaw] no vector for type=%u -> insert",
									  static_cast<unsigned>(type));
			NkVector<NkEventCallback> initial;
			mTypedCallbacks.Insert(type, initial);
			callbacks = mTypedCallbacks.Find(type);
			if (!callbacks) {
				NK_EVENTSYS_ANDROID_TRACE("[AddEventCallbackRaw] insert failed type=%u", static_cast<unsigned>(type));
				return;
			}
		}

		NK_EVENTSYS_ANDROID_TRACE("[AddEventCallbackRaw] before push type=%u vec_size=%llu vec_capacity=%llu",
								  static_cast<unsigned>(type), static_cast<unsigned long long>(callbacks->Size()),
								  static_cast<unsigned long long>(callbacks->Capacity()));

		callbacks->Resize(callbacks->Size() + 1);
		(*callbacks)[callbacks->Size() - 1] = traits::NkMove(callback);

		NK_EVENTSYS_ANDROID_TRACE("[AddEventCallbackRaw] after push type=%u vec_size=%llu vec_capacity=%llu",
								  static_cast<unsigned>(type), static_cast<unsigned long long>(callbacks->Size()),
								  static_cast<unsigned long long>(callbacks->Capacity()));
	}

	uint64 NkEventSystem::AddEventCallbackTokenRaw(NkEventType::Value type, NkEventCallback callback) {
		if (mTypedCallbacksWithToken.BucketCount() == 0) {
			mTypedCallbacksWithToken.Rehash(static_cast<nk_size>(NkEventType::NK_EVENT_COUNT));
		}
		const uint64 token = ++mCallbackTokenCounter;
		auto &vec = mTypedCallbacksWithToken[type];
		vec.Resize(vec.Size() + 1);
		auto &slot = vec[vec.Size() - 1];
		slot.token = token;
		slot.callback = traits::NkMove(callback);
		return token;
	}

	void NkEventSystem::ClearEventCallbacksRaw(NkEventType::Value type) {
		mTypedCallbacks.Erase(type);
		mTypedCallbacksWithToken.Erase(type);
	}

	void NkEventSystem::ClearAllCallbacks() {
		mGlobalCallback = NkGlobalEventCallback{};
		mWindowCallbacks.Clear();
		mTypedCallbacks.Clear();
		mTypedCallbacksWithToken.Clear(); // CORRECTION 4
	}

	// CORRECTION 4 : suppression d'un callback par token (appelÃ©e par NkCallbackGuard)
	void NkEventSystem::RemoveCallbackToken(NkEventType::Value type, uint64 token) {
		auto *vec = mTypedCallbacksWithToken.Find(type);
		if (!vec)
			return;
		for (nk_size i = 0; i < vec->Size(); ++i) {
			if ((*vec)[i].token == token) {
				vec->Erase(vec->begin() + i);
				break;
			}
		}
		if (vec->Empty())
			mTypedCallbacksWithToken.Erase(type);
	}

	// =============================================================================
	// Enqueue â€” Point 3 + Point 4
	//
	// Point 4 : SetWindowId() est appelÃ© ici, avant le clone, pour que l'event
	// stockÃ© dans le ring buffer porte dÃ©jÃ  l'id de sa fenÃªtre source.
	// Pas besoin de le rÃ©pÃ©ter dans DispatchToCallbacks.
	//
	// Point 3 : le clone est poussÃ© dans le ring buffer sans alloc supplÃ©mentaire
	// (le ring buffer gÃ¨re la mÃ©moire de ses slots via unique_ptr).
	// =============================================================================

	void NkEventSystem::Enqueue(NkEvent &evt, NkWindowId winId) {
		// Point 4 : on estampille l'event avec l'id de sa fenÃªtre source
		evt.SetWindowId(winId);

		// Evenement produit par un THREAD ETRANGER (callback systeme mobile) :
		// on le met de cote au lieu de le livrer ici. Le livrer sur ce thread
		// ferait tourner du code applicatif — donc potentiellement du code GPU —
		// sans contexte graphique courant.  Cf. mForeignEvents dans l'en-tete.
		// mPumpThreadId vaut 0 tant que la boucle n'a pas demarre : pendant ce
		// bootstrap on livre normalement, sinon rien ne partirait jamais.
		if (mPumpThreadId != 0 && NkCurrentThreadTag() != mPumpThreadId) {
			NkEventPtr clone(evt.Clone());
			if (clone) {
				NkScopedSpinLock lock(mForeignMutex);
				mForeignEvents.PushBack(traits::NkMove(clone));
			}
			return;
		}

		DeliverOnPumpThread(evt, winId);
	}

	// =============================================================================
	// DeliverOnPumpThread — corps historique de Enqueue, desormais garanti
	// de s'executer sur le thread qui pompe les evenements.
	// =============================================================================

	void NkEventSystem::DeliverOnPumpThread(NkEvent &evt, NkWindowId winId) {

		// Dispatch immÃ©diat des callbacks (pas besoin de stocker pour Ã§a)
		DispatchToCallbacks(&evt, winId);

		++mTotalEventCount;

		if (mAutoUpdateInputState)
			UpdateInputState(&evt);

		// Point 3 : on clone et on pousse dans le ring buffer seulement si
		// le mode queue est actif. Le clone porte dÃ©jÃ  le bon windowId.
		if (mQueueMode) {
			NkEventPtr clone(evt.Clone());
			// CORRECTION 3 : push dans la file prioritaire appropriÃ©e
			NkEventPriority prio = NkGetEventPriority(evt.GetType());
			NkScopedSpinLock lock(mQueueMutex);
			mEventQueue.Push(traits::NkMove(clone), prio);
		}
	}

	// =============================================================================
	// DrainForeignEvents — rejoue sur le thread pump ce qui vient d'ailleurs
	// =============================================================================

	void NkEventSystem::DrainForeignEvents() {
		// On sort les evenements SOUS verrou, puis on les livre HORS verrou : un
		// callback applicatif peut lui-meme produire un evenement, et le spinlock
		// n'est pas reentrant.
		NkVector<NkEventPtr> aLivrer;
		{
			NkScopedSpinLock lock(mForeignMutex);
			if (mForeignEvents.Empty())
				return;
			aLivrer = traits::NkMove(mForeignEvents);
			mForeignEvents.Clear();
		}

		for (usize i = 0; i < aLivrer.Size(); ++i) {
			NkEvent *ev = aLivrer[i].Get();
			if (ev)
				DeliverOnPumpThread(*ev, ev->GetWindowId());
		}
	}

	// =============================================================================
	// DispatchToCallbacks
	// =============================================================================

	void NkEventSystem::DispatchToCallbacks(NkEvent *ev, NkWindowId winId) {
		if (!ev)
			return;

		// Per-window callback â€” Point 4 : winId est dÃ©jÃ  dans ev->GetWindowId()
		// On utilise le paramÃ¨tre winId (identique) pour la lookup map
		if (winId != NK_INVALID_WINDOW_ID) {
			auto *cb = mWindowCallbacks.Find(winId);
			if (cb && *cb)
				(*cb)(ev);
		}

		// Typed callbacks (sans token â€” AddEventCallback classique)
		{
			auto *vecT = mTypedCallbacks.Find(ev->GetType());
			if (vecT) {
				// NOTE:
				// Evite la copie des callables (function wrapper) qui peut
				// Ãªtre coÃ»teuse et fragile sur certains runtimes Android x86.
				for (nk_size i = 0; i < vecT->Size(); ++i) {
					auto &cb = (*vecT)[i];
					if (cb)
						cb(ev);
				}
			}
		}

		// Typed callbacks tokÃ©nisÃ©s â€” CORRECTION 4 (RAII guard)
		// CORRECTION 6 : le filtre windowId est encodÃ© dans le wrapper lui-mÃªme
		{
			auto *vecTT = mTypedCallbacksWithToken.Find(ev->GetType());
			if (vecTT) {
				for (nk_size i = 0; i < vecTT->Size(); ++i) {
					auto &tc = (*vecTT)[i];
					if (tc.callback)
						tc.callback(ev);
				}
			}
		}

		// Global callback
		if (mGlobalCallback)
			mGlobalCallback(ev);
	}

	void NkEventSystem::UpdateInputState(NkEvent *ev) {
		// Met a jour l'etat d'input persistant (poll-based : NkInput.IsKeyDown /
		// IsMouseDown / MouseX...) a partir de CHAQUE event, de facon CROSS-PLATFORM.
		// Tous les backends (Win32/XLib/XCB/Wayland/Cocoa/Android/...) passent par
		// Enqueue() -> ici, donc le poll fonctionne partout. Avant : corps vide (le
		// state n'etait rempli que par certains ProcessMessage platform) -> IsKeyDown
		// casse sur Win32.
		if (!ev)
			return;

		// -- Clavier ----------------------------------------------------------
		if (ev->Is<NkKeyPressEvent>()) {
			auto &e = *static_cast<NkKeyPressEvent *>(ev);
			mInputState.keyboard.OnKeyPress(e.GetKey(), e.GetScancode(), e.GetModifiers());
		} else if (ev->Is<NkKeyRepeatEvent>()) {
			auto &e = *static_cast<NkKeyRepeatEvent *>(ev);
			mInputState.keyboard.OnKeyRepeat(e.GetKey(), e.GetScancode(), e.GetModifiers());
		} else if (ev->Is<NkKeyReleaseEvent>()) {
			auto &e = *static_cast<NkKeyReleaseEvent *>(ev);
			mInputState.keyboard.OnKeyRelease(e.GetKey(), e.GetModifiers());
		}
		// -- Souris -----------------------------------------------------------
		else if (ev->Is<NkMouseMoveEvent>()) {
			auto &e = *static_cast<NkMouseMoveEvent *>(ev);
			mInputState.mouse.OnMove(e.GetX(), e.GetY(), e.GetScreenX(), e.GetScreenY());
		} else if (ev->Is<NkMouseRawEvent>()) {
			auto &e = *static_cast<NkMouseRawEvent *>(ev);
			mInputState.mouse.OnRaw(e.GetDeltaX(), e.GetDeltaY());
		} else if (ev->Is<NkMouseButtonPressEvent>()) {
			auto &e = *static_cast<NkMouseButtonPressEvent *>(ev);
			mInputState.mouse.OnButtonPress(e.GetButton(), e.GetModifiers());
		} else if (ev->Is<NkMouseButtonReleaseEvent>()) {
			auto &e = *static_cast<NkMouseButtonReleaseEvent *>(ev);
			mInputState.mouse.OnButtonRelease(e.GetButton(), e.GetModifiers());
		}
		return;
		// (ancien corps : vide — l'input state n'etait pas rempli de facon portable)
		// Input state mis Ã  jour dans ProcessMessage platform-spÃ©cifique
	}

	// =============================================================================
	// PollEvents
	// =============================================================================

	void NkEventSystem::PollEvents() {
		// CORRECTION 5 : PollEvents() doit Ãªtre appelÃ© depuis le thread pump.
		if (mPumpThreadId == 0)
			mPumpThreadId = NkCurrentThreadTag();
		assert(NkCurrentThreadTag() == mPumpThreadId &&
			   "PollEvents() doit etre appele depuis le thread principal (pump thread)");

		if (mAutoGamepadPoll) {
			if (mGamepadSystem && mGamepadSystem->IsReady())
				mGamepadSystem->PollGamepads();
		}

		// Avant toute chose : livrer ce que les callbacks systeme ont pousse
		// depuis leur propre thread. C'est ICI que l'application les recoit,
		// avec le contexte graphique courant.
		DrainForeignEvents();

		PumpOS();

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) && defined(__EMSCRIPTEN__)
		// Web cooperative yielding is done by the application frame loop.
		// Avoid yielding here to keep PollEvents synchronous.
#endif
	}

	// =============================================================================
	// PollEvent â€” Point 3 + Point 5
	//
	// Point 3 : lecture depuis le ring buffer au lieu de la deque.
	// Point 5 : mQueueMutex protÃ¨ge chaque accÃ¨s Ã  mEventQueue.
	//           On prend le lock le moins longtemps possible :
	//           on extrait l'unique_ptr, on relÃ¢che, puis on utilise l'event.
	// =============================================================================

	NkEvent *NkEventSystem::PollEvent() {
		// CORRECTION 5 : PollEvent() doit Ãªtre appelÃ© depuis le thread qui a appelÃ© Init().
		// Si mPumpThreadId n'est pas encore enregistrÃ© (Init() pas encore appelÃ© depuis
		// un .cpp platform-specific), on l'enregistre maintenant au premier appel.
		if (mPumpThreadId == 0)
			mPumpThreadId = NkCurrentThreadTag();
		assert(NkCurrentThreadTag() == mPumpThreadId &&
			   "PollEvent() doit etre appele depuis le thread principal (pump thread)");

		// Tenter de dÃ©piler un event dÃ©jÃ  en queue
		// CORRECTION 2 : mCurrentEvent garde la propriÃ©tÃ© unique_ptr ; le pointeur
		// retournÃ© est valide jusqu'au PROCHAIN appel de PollEvent().
		{
			NkScopedSpinLock lock(mQueueMutex);
			auto ev = mEventQueue.Pop();
			if (ev) {
				mCurrentEvent = traits::NkMove(ev);
				return mCurrentEvent.Get();
			}
		}

		// Queue vide â€” pomper l'OS pour remplir
		if (mAutoGamepadPoll) {
			if (mGamepadSystem && mGamepadSystem->IsReady())
				mGamepadSystem->PollGamepads();
		}

		PumpOS();

		// Retenter aprÃ¨s le pump
		{
			NkScopedSpinLock lock(mQueueMutex);
			auto ev = mEventQueue.Pop();
			if (ev) {
				mCurrentEvent = traits::NkMove(ev);
				return mCurrentEvent.Get();
			}
		}

#if defined(NKENTSEU_PLATFORM_EMSCRIPTEN) && defined(__EMSCRIPTEN__)
		// Web cooperative yielding is done by the application frame loop.
		// Avoid async unwind from inside PollEvent().
#endif

		return nullptr;
	}

	bool NkEventSystem::PollEvent(NkEvent *&event) {
		event = PollEvent();
		return event != nullptr;
	}

	// CORRECTION 2 : PollEventCopy() â€” durÃ©e de vie contrÃ´lÃ©e par l'appelant.
	// Le unique_ptr retournÃ© est indÃ©pendant de mCurrentEvent : peut Ãªtre stockÃ©
	// entre frames, mis en file asynchrone, ou passÃ© Ã  un autre thread.
	NkEventPtr NkEventSystem::PollEventCopy() {
		NkEvent *raw = PollEvent();
		if (!raw)
			return NkEventPtr(nullptr);
		return NkEventPtr(raw->Clone());
	}

	// =============================================================================
	// DispatchEvent (direct / immÃ©diat) â€” Point 5
	//
	// mDispatchMutex protÃ¨ge uniquement ce chemin (appel externe direct).
	// Il est distinct de mQueueMutex pour Ã©viter un deadlock si DispatchEvent
	// est appelÃ© depuis un callback qui tient dÃ©jÃ  mQueueMutex.
	// =============================================================================

	void NkEventSystem::DispatchEvent(NkEvent &event) {
		NkScopedSpinLock lock(mDispatchMutex);
		NkWindowId winId = event.GetWindowId();
		DispatchToCallbacks(&event, winId);
		++mTotalEventCount;
		if (mAutoUpdateInputState)
			UpdateInputState(&event);
	}

	// =============================================================================
	// Input state / info
	// =============================================================================

	const NkEventState &NkEventSystem::GetInputState() const noexcept {
		return mInputState;
	}

	nk_size NkEventSystem::GetPendingEventCount() const noexcept {
		NkScopedSpinLock lock(mQueueMutex);
		return mEventQueue.Size();
	}

} // namespace nkentseu
