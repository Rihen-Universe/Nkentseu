// =============================================================================
// NkEventSystem.cpp — méthodes cross-plateforme
//
// Corrections appliquées :
//   CORRECTION 1 : NkGamepadSystem::Instance() → NkSystem::Gamepads()
//   CORRECTION 2 : PollEvent() pointer lifetime documenté + PollEventCopy() ajouté
//   CORRECTION 3 : ring buffer dual-priorité (HIGH no-drop / NORMAL drop-oldest)
//   CORRECTION 5 : assertions thread ID sur PollEvent() et PollEvents()
// =============================================================================

#include "NkEventSystem.h"
#include "NkGamepadSystem.h"
#include "NKWindow/Core/NkSystem.h"
#include "NKPlatform/NkPlatformDetect.h"
#include <algorithm>
#include <cassert>
#include <thread>

#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#endif

namespace nkentseu {

    // =============================================================================
    // Constructeur / Destructeur
    // =============================================================================

    NkEventSystem::NkEventSystem() {
        // Défensif: certains toolchains peuvent laisser bucket_count() à 0
        // sur map vide, ce qui peut provoquer un modulo par zéro au 1er operator[].
        mWindowCallbacks.rehash(32);
        mTypedCallbacks.rehash(static_cast<std::size_t>(NkEventType::NK_EVENT_COUNT));
        mTypedCallbacksWithToken.rehash(static_cast<std::size_t>(NkEventType::NK_EVENT_COUNT));
    }
    NkEventSystem::~NkEventSystem() { Shutdown(); }

    // =============================================================================
    // Shutdown
    // WASM override: NkWASMEventSystem.cpp fournit sa propre version de Shutdown()
    // qui désenregistre les callbacks Emscripten avant de nettoyer.
    // =============================================================================

#if !defined(NKENTSEU_PLATFORM_WEB) && !defined(NKENTSEU_PLATFORM_EMSCRIPTEN) && !defined(__EMSCRIPTEN__) && \
    !(defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_XCB) && !defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)) && \
    !(defined(NKENTSEU_PLATFORM_LINUX) && defined(NKENTSEU_WINDOWING_WAYLAND) && !defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)) && \
    !defined(NKENTSEU_PLATFORM_ANDROID)
    void NkEventSystem::Shutdown() {
        ClearAllCallbacks();
        mHidMapper.Clear();
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mEventQueue.Clear();
            mCurrentEvent.reset();
        }
        mWindowCallbacks.clear();
        mTotalEventCount = 0;
        mPumping         = false;
        mPumpThreadId    = std::thread::id{};  // reset thread ID
        mReady           = false;
    }
#endif // !WASM

    // =============================================================================
    // Callbacks
    // =============================================================================

    void NkEventSystem::SetWindowCallback(NkWindowId id, NkEventCallback cb) {
        mWindowCallbacks[id] = std::move(cb);
    }

    void NkEventSystem::RemoveWindowCallback(NkWindowId id) {
        mWindowCallbacks.erase(id);
    }

    void NkEventSystem::SetGlobalCallback(NkGlobalEventCallback cb) {
        mGlobalCallback = std::move(cb);
    }

    void NkEventSystem::AddEventCallbackRaw(NkEventType::Value type, NkEventCallback callback) {
        if (mTypedCallbacks.bucket_count() == 0) {
            mTypedCallbacks.rehash(static_cast<std::size_t>(NkEventType::NK_EVENT_COUNT));
        }
        mTypedCallbacks[type].push_back(std::move(callback));
    }

    NkU64 NkEventSystem::AddEventCallbackTokenRaw(NkEventType::Value type, NkEventCallback callback) {
        if (mTypedCallbacksWithToken.bucket_count() == 0) {
            mTypedCallbacksWithToken.rehash(static_cast<std::size_t>(NkEventType::NK_EVENT_COUNT));
        }
        const NkU64 token = ++mCallbackTokenCounter;
        mTypedCallbacksWithToken[type].push_back({token, std::move(callback)});
        return token;
    }

    void NkEventSystem::ClearEventCallbacksRaw(NkEventType::Value type) {
        mTypedCallbacks.erase(type);
        mTypedCallbacksWithToken.erase(type);
    }

    void NkEventSystem::ClearAllCallbacks() {
        mGlobalCallback = nullptr;
        mWindowCallbacks.clear();
        mTypedCallbacks.clear();
        mTypedCallbacksWithToken.clear();  // CORRECTION 4
    }

    // CORRECTION 4 : suppression d'un callback par token (appelée par NkCallbackGuard)
    void NkEventSystem::RemoveCallbackToken(NkEventType::Value type, NkU64 token) {
        auto it = mTypedCallbacksWithToken.find(type);
        if (it == mTypedCallbacksWithToken.end()) return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [token](const TokenizedCallback& tc) { return tc.token == token; }),
            vec.end());
        if (vec.empty()) mTypedCallbacksWithToken.erase(it);
    }

    // =============================================================================
    // Enqueue — Point 3 + Point 4
    //
    // Point 4 : SetWindowId() est appelé ici, avant le clone, pour que l'event
    // stocké dans le ring buffer porte déjà l'id de sa fenêtre source.
    // Pas besoin de le répéter dans DispatchToCallbacks.
    //
    // Point 3 : le clone est poussé dans le ring buffer sans alloc supplémentaire
    // (le ring buffer gère la mémoire de ses slots via unique_ptr).
    // =============================================================================

    void NkEventSystem::Enqueue(NkEvent& evt, NkWindowId winId) {
        // Point 4 : on estampille l'event avec l'id de sa fenêtre source
        evt.SetWindowId(winId);

        // Dispatch immédiat des callbacks (pas besoin de stocker pour ça)
        DispatchToCallbacks(&evt, winId);

        ++mTotalEventCount;

        if (mAutoUpdateInputState)
            UpdateInputState(&evt);

        // Point 3 : on clone et on pousse dans le ring buffer seulement si
        // le mode queue est actif. Le clone porte déjà le bon windowId.
        if (mQueueMode) {
            auto clone = std::unique_ptr<NkEvent>(evt.Clone());
            // CORRECTION 3 : push dans la file prioritaire appropriée
            NkEventPriority prio = NkGetEventPriority(evt.GetType());
            std::lock_guard<std::mutex> lock(mQueueMutex);
            mEventQueue.Push(std::move(clone), prio);
        }
    }

    // =============================================================================
    // DispatchToCallbacks
    // =============================================================================

    void NkEventSystem::DispatchToCallbacks(NkEvent* ev, NkWindowId winId) {
        if (!ev) return;

        // Per-window callback — Point 4 : winId est déjà dans ev->GetWindowId()
        // On utilise le paramètre winId (identique) pour la lookup map
        if (winId != NK_INVALID_WINDOW_ID) {
            auto it = mWindowCallbacks.find(winId);
            if (it != mWindowCallbacks.end() && it->second)
                it->second(ev);
        }

        // Typed callbacks (sans token — AddEventCallback classique)
        {
            auto it = mTypedCallbacks.find(ev->GetType());
            if (it != mTypedCallbacks.end()) {
                auto callbacks = it->second; // copie locale pour éviter invalidation
                for (auto& cb : callbacks)
                    if (cb) cb(ev);
            }
        }

        // Typed callbacks tokénisés — CORRECTION 4 (RAII guard)
        // CORRECTION 6 : le filtre windowId est encodé dans le wrapper lui-même
        {
            auto it = mTypedCallbacksWithToken.find(ev->GetType());
            if (it != mTypedCallbacksWithToken.end()) {
                auto callbacks = it->second; // copie locale
                for (auto& tc : callbacks)
                    if (tc.callback) tc.callback(ev);
            }
        }

        // Global callback
        if (mGlobalCallback)
            mGlobalCallback(ev);
    }

    void NkEventSystem::UpdateInputState(NkEvent* /*ev*/) {
        // Input state mis à jour dans ProcessMessage platform-spécifique
    }

    // =============================================================================
    // PollEvents
    // =============================================================================

    void NkEventSystem::PollEvents() {
        // CORRECTION 5 : PollEvents() doit être appelé depuis le thread pump.
        if (mPumpThreadId == std::thread::id{})
            mPumpThreadId = std::this_thread::get_id();
        assert(std::this_thread::get_id() == mPumpThreadId &&
               "PollEvents() doit etre appele depuis le thread principal (pump thread)");

        if (mAutoGamepadPoll) {
            auto& gpSys = NkSystem::Gamepads();
            if (gpSys.IsReady()) gpSys.PollGamepads();
        }

        PumpOS();

    #if (defined(NKENTSEU_PLATFORM_WEB) || defined(__EMSCRIPTEN__)) && defined(__EMSCRIPTEN__)
        emscripten_sleep(0);
    #endif
    }

    // =============================================================================
    // PollEvent — Point 3 + Point 5
    //
    // Point 3 : lecture depuis le ring buffer au lieu de la deque.
    // Point 5 : mQueueMutex protège chaque accès à mEventQueue.
    //           On prend le lock le moins longtemps possible :
    //           on extrait l'unique_ptr, on relâche, puis on utilise l'event.
    // =============================================================================

    NkEvent* NkEventSystem::PollEvent() {
        // CORRECTION 5 : PollEvent() doit être appelé depuis le thread qui a appelé Init().
        // Si mPumpThreadId n'est pas encore enregistré (Init() pas encore appelé depuis
        // un .cpp platform-specific), on l'enregistre maintenant au premier appel.
        if (mPumpThreadId == std::thread::id{})
            mPumpThreadId = std::this_thread::get_id();
        assert(std::this_thread::get_id() == mPumpThreadId &&
               "PollEvent() doit etre appele depuis le thread principal (pump thread)");

        // Tenter de dépiler un event déjà en queue
        // CORRECTION 2 : mCurrentEvent garde la propriété unique_ptr ; le pointeur
        // retourné est valide jusqu'au PROCHAIN appel de PollEvent().
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            auto ev = mEventQueue.Pop();
            if (ev) {
                mCurrentEvent = std::move(ev);
                return mCurrentEvent.get();
            }
        }

        // Queue vide — pomper l'OS pour remplir
        if (mAutoGamepadPoll) {
            auto& gpSys = NkSystem::Gamepads();
            if (gpSys.IsReady()) gpSys.PollGamepads();
        }

        PumpOS();

        // Retenter après le pump
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            auto ev = mEventQueue.Pop();
            if (ev) {
                mCurrentEvent = std::move(ev);
                return mCurrentEvent.get();
            }
        }

    #if (defined(NKENTSEU_PLATFORM_WEB) || defined(__EMSCRIPTEN__)) && defined(__EMSCRIPTEN__)
        emscripten_sleep(0);
    #endif

        return nullptr;
    }

    bool NkEventSystem::PollEvent(NkEvent*& event) {
        event = PollEvent();
        return event != nullptr;
    }

    // CORRECTION 2 : PollEventCopy() — durée de vie contrôlée par l'appelant.
    // Le unique_ptr retourné est indépendant de mCurrentEvent : peut être stocké
    // entre frames, mis en file asynchrone, ou passé à un autre thread.
    std::unique_ptr<NkEvent> NkEventSystem::PollEventCopy() {
        NkEvent* raw = PollEvent();
        if (!raw) return nullptr;
        return std::unique_ptr<NkEvent>(raw->Clone());
    }

    // =============================================================================
    // DispatchEvent (direct / immédiat) — Point 5
    //
    // mDispatchMutex protège uniquement ce chemin (appel externe direct).
    // Il est distinct de mQueueMutex pour éviter un deadlock si DispatchEvent
    // est appelé depuis un callback qui tient déjà mQueueMutex.
    // =============================================================================

    void NkEventSystem::DispatchEvent(NkEvent& event) {
        std::lock_guard<std::mutex> lock(mDispatchMutex);
        NkWindowId winId = event.GetWindowId();
        DispatchToCallbacks(&event, winId);
        ++mTotalEventCount;
        if (mAutoUpdateInputState)
            UpdateInputState(&event);
    }

    // =============================================================================
    // Input state / info
    // =============================================================================

    const NkEventState& NkEventSystem::GetInputState() const noexcept {
        return mInputState;
    }

    std::size_t NkEventSystem::GetPendingEventCount() const noexcept {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        return mEventQueue.Size();
    }

} // namespace nkentseu
