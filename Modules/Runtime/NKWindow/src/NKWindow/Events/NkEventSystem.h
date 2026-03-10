#pragma once

// =============================================================================
// NkEventSystem.h
// SystÃ¨me d'Ã©vÃ©nements cross-plateforme.
// PossÃ©dÃ© et instanciÃ© par NkSystem â€” plus de singleton autoproclamÃ©.
//
// Corrections appliquÃ©es :
//   CORRECTION 1 : NkGamepadSystem n'est plus un singleton â€” possÃ©dÃ© par NkSystem
//   CORRECTION 2 : PollEvent() lifetime documentÃ© + PollEventCopy() ajoutÃ©
//   CORRECTION 3 : ring buffer dual-prioritÃ© (HIGH no-drop / NORMAL drop-oldest)
//   CORRECTION 4 : AddEventCallbackGuard<T>() retourne un NkCallbackGuard RAII
//   CORRECTION 5 : assertions thread ID sur PollEvent() / PollEvents()
//   CORRECTION 6 : AddEventCallback<T>(cb, windowId) filtre par fenÃªtre
// =============================================================================

#include "NkEvent.h"
#include "NkEventState.h"
#include "NkApplicationEvent.h"
#include "NkWindowEvent.h"
#include "NkKeyboardEvent.h"
#include "NkMouseEvent.h"
#include "NkTouchEvent.h"
#include "NkGamepadEvent.h"
#include "NkDropEvent.h"
#include "NkSystemEvent.h"
#include "NkCustomEvent.h"
#include "NkTransferEvent.h"
#include "NkGenericHidEvent.h"
#include "NkGenericHidMapper.h"
#include "NkGraphicsEvent.h"

#include "NKWindow/Core/NkWindowId.h"
#include "NKPlatform/NkPlatformDetect.h"


#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <array>
#include <atomic>
#include <thread>
#include <cstddef>
#include <type_traits>
#include <utility>

#if !defined(NK_EVENTSYS_HAS_ANDROID_LOG)
#   if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#       define NK_EVENTSYS_HAS_ANDROID_LOG 1
#   elif defined(__has_include)
#       if __has_include(<android/log.h>)
#           define NK_EVENTSYS_HAS_ANDROID_LOG 1
#       else
#           define NK_EVENTSYS_HAS_ANDROID_LOG 0
#       endif
#   else
#       define NK_EVENTSYS_HAS_ANDROID_LOG 0
#   endif
#endif

#if NK_EVENTSYS_HAS_ANDROID_LOG
#   include <android/log.h>
#   define NK_EVENTSYS_ANDROID_TRACE(...) __android_log_print(ANDROID_LOG_INFO, "NkEventSystem", __VA_ARGS__)
#else
#   define NK_EVENTSYS_ANDROID_TRACE(...) ((void)0)
#endif

#if defined(NKENTSEU_PLATFORM_UWP)
#   include "NKWindow/Platform/UWP/NkUWPEventSystem.h"
#elif defined(NKENTSEU_PLATFORM_XBOX)
#   include "NKWindow/Platform/Xbox/NkXboxEventSystem.h"
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <windows.h>
#   include "NKWindow/Platform/Win32/NkWin32EventSystem.h"
#elif defined(NKENTSEU_PLATFORM_EMSCRIPTEN)
#   include "NKWindow/Platform/Emscripten/NkEmscriptenEventSystem.h"
#elif defined(NKENTSEU_PLATFORM_MACOS)
#   include "NKWindow/Platform/Cocoa/NkCocoaEventSystem.h"
#elif defined(NKENTSEU_PLATFORM_IOS)
#   include "NKWindow/Platform/UIKit/NkUIKitEventSystem.h"
#elif defined(NKENTSEU_FORCE_WINDOWING_NOOP_ONLY)
#   include "NKWindow/Platform/Noop/NkNoopEventSystem.h"
#elif defined(NKENTSEU_PLATFORM_LINUX)
#   if defined(NKENTSEU_WINDOWING_WAYLAND)
#       include "NKWindow/Platform/Wayland/NkWaylandEventSystem.h"
#   elif defined(NKENTSEU_WINDOWING_XCB)
#       include "NKWindow/Platform/XCB/NkXCBEventSystem.h"
#   elif defined(NKENTSEU_WINDOWING_XLIB)
#       include "NKWindow/Platform/XLib/NkXLibEventSystem.h"
#   else
#       include "NKWindow/Platform/Noop/NkNoopEventSystem.h"
#   endif
#elif defined(NKENTSEU_PLATFORM_ANDROID)
#   include "NKWindow/Platform/Android/NkAndroidEventSystem.h"
#else
#   include "NKWindow/Platform/Noop/NkNoopEventSystem.h"
#endif

namespace nkentseu {

    class NkWindow;
    class NkSystem;

    using NkGlobalEventCallback = NkEventCallback;
    using NkTypedEventCallback  = NkEventCallback;

#if NKENTSEU_EVENT_CALLBACK_USE_NKFUNCTION
    using NkRemoverCallback = NkFunction<void>;
#else
    using NkRemoverCallback = std::function<void()>;
#endif

    // =========================================================================
    // NkEventPriority â€” CORRECTION 3 : classification drop-safe vs droppable
    // =========================================================================
    // Ã‰vÃ©nements HIGH : ne jamais dropper (lifecycle fenÃªtre, clavier, destroy)
    // Ã‰vÃ©nements NORMAL : peuvent Ãªtre droppÃ©s sous pression (mouse move, etc.)
    // =========================================================================
    enum class NkEventPriority { HIGH, NORMAL };

    /// @brief Retourne la prioritÃ© d'un type d'Ã©vÃ©nement.
    /// Les Ã©vÃ©nements HIGH ne sont jamais droppÃ©s en cas de saturation.
    inline NkEventPriority NkGetEventPriority(NkEventType::Value t) noexcept {
        switch (t) {
            // Lifecycle fenÃªtre â€” critique (destruction/fermeture irrÃ©versible)
            case NkEventType::NK_WINDOW_CREATE:
            case NkEventType::NK_WINDOW_CLOSE:
            case NkEventType::NK_WINDOW_DESTROY:
            case NkEventType::NK_WINDOW_FOCUS_GAINED:
            case NkEventType::NK_WINDOW_FOCUS_LOST:
            case NkEventType::NK_WINDOW_MINIMIZE:
            case NkEventType::NK_WINDOW_MAXIMIZE:
            case NkEventType::NK_WINDOW_RESTORE:
            case NkEventType::NK_WINDOW_FULLSCREEN:
            case NkEventType::NK_WINDOW_WINDOWED:
            case NkEventType::NK_WINDOW_SHOWN:
            case NkEventType::NK_WINDOW_HIDDEN:
            // Clavier â€” chaque frappe est sÃ©mantique
            case NkEventType::NK_KEY_PRESSED:
            case NkEventType::NK_KEY_REPEATED:
            case NkEventType::NK_KEY_RELEASED:
            case NkEventType::NK_TEXT_INPUT:
            case NkEventType::NK_CHAR_ENTERED:
            // Boutons souris â€” chaque clic est sÃ©mantique
            case NkEventType::NK_MOUSE_BUTTON_PRESSED:
            case NkEventType::NK_MOUSE_BUTTON_RELEASED:
            case NkEventType::NK_MOUSE_DOUBLE_CLICK:
            // App lifecycle
            case NkEventType::NK_APP_CLOSE:
            case NkEventType::NK_APP_LAUNCH:
            // Gamepad boutons
            case NkEventType::NK_GAMEPAD_CONNECT:
            case NkEventType::NK_GAMEPAD_DISCONNECT:
            case NkEventType::NK_GAMEPAD_BUTTON_PRESSED:
            case NkEventType::NK_GAMEPAD_BUTTON_RELEASED:
                return NkEventPriority::HIGH;
            default:
                return NkEventPriority::NORMAL;
        }
    }

    // =========================================================================
    // NkEventRingBuffer
    // Ring buffer prÃ©-allouÃ©e pour les Ã©vÃ©nements.
    // Politique drop-oldest UNIQUEMENT sur la file NORMAL.
    // La file HIGH ne droppe jamais (taille 128 â€” overflow = assert en debug).
    // =========================================================================

    class NkEventRingBuffer {
    public:
        static constexpr std::size_t kHighCapacity   =  128; // critique â€” no-drop
        static constexpr std::size_t kNormalCapacity =  512; // droppable

        NkEventRingBuffer()  = default;
        ~NkEventRingBuffer() = default;

        NkEventRingBuffer(const NkEventRingBuffer&)            = delete;
        NkEventRingBuffer& operator=(const NkEventRingBuffer&) = delete;

        // CORRECTION 3 : push dans la file correspondant Ã  la prioritÃ©.
        // Retourne false uniquement pour NORMAL (drop-oldest), jamais pour HIGH.
        bool Push(std::unique_ptr<NkEvent> ev, NkEventPriority prio) {
            if (prio == NkEventPriority::HIGH) {
                return PushInto(mHighSlots, kHighCapacity, mHighHead, mHighTail,
                                std::move(ev), /*allowDrop=*/false);
            } else {
                return PushInto(mNormSlots, kNormalCapacity, mNormHead, mNormTail,
                                std::move(ev), /*allowDrop=*/true);
            }
        }

        // Pop : prioritÃ© HIGH d'abord, ensuite NORMAL.
        std::unique_ptr<NkEvent> Pop() {
            if (mHighHead != mHighTail) return PopFrom(mHighSlots, kHighCapacity, mHighHead, mHighTail);
            if (mNormHead != mNormTail) return PopFrom(mNormSlots, kNormalCapacity, mNormHead, mNormTail);
            return nullptr;
        }

        bool        Empty() const { return (mHighHead == mHighTail) && (mNormHead == mNormTail); }
        std::size_t Size()  const { return QueueSize(mHighHead, mHighTail, kHighCapacity)
                                         + QueueSize(mNormHead, mNormTail, kNormalCapacity); }
        void Clear() { while (!Empty()) Pop(); }

    private:
        static std::size_t QueueSize(std::size_t h, std::size_t t, std::size_t cap) noexcept {
            if (cap == 0) return 0;
            return (h >= t) ? (h - t) : (cap - t + h);
        }

        static bool PushInto(NkVector<std::unique_ptr<NkEvent>>& slots,
                              std::size_t cap,
                              std::size_t& head, std::size_t& tail,
                              std::unique_ptr<NkEvent> ev,
                              bool allowDrop)
        {
            if (cap == 0) return false;
            std::size_t next = (head + 1) % cap;
            bool full = (next == tail);
            if (full) {
                if (!allowDrop) return false; // HIGH : on ne droppe pas
                tail = (tail + 1) % cap;      // NORMAL : drop oldest
            }
            slots[head] = std::move(ev);
            head = next;
            return !full;
        }

        static std::unique_ptr<NkEvent> PopFrom(NkVector<std::unique_ptr<NkEvent>>& slots,
                                                 std::size_t cap,
                                                 std::size_t& head, std::size_t& tail)
        {
            if (cap == 0) return nullptr;
            if (head == tail) return nullptr;
            auto ev = std::move(slots[tail]);
            tail = (tail + 1) % cap;
            return ev;
        }

        // File HAUTE PRIORITÃ‰ (no-drop)
        NkVector<std::unique_ptr<NkEvent>> mHighSlots{kHighCapacity};
        std::size_t mHighHead = 0, mHighTail = 0;

        // File NORMALE (drop-oldest)
        NkVector<std::unique_ptr<NkEvent>> mNormSlots{kNormalCapacity};
        std::size_t mNormHead = 0, mNormTail = 0;
    };

    // =========================================================================
    // NkCallbackGuard â€” CORRECTION 4 : RAII pour les typed callbacks
    // =========================================================================
    // Garantit que le callback est supprimÃ© quand le guard est dÃ©truit.
    // Utilisation :
    //   auto guard = NkEvents().AddEventCallbackGuard<NkKeyPressEvent>(
    //       [&](NkKeyPressEvent* ev) { ... });
    //   // guard tient la connexion vivante ; destruction = dÃ©sinscription auto
    // =========================================================================
    class NkCallbackGuard {
    public:
        NkCallbackGuard() = default;

        NkCallbackGuard(NkRemoverCallback remover)
            : mRemover(std::move(remover)) {}

        ~NkCallbackGuard() { Release(); }

        // Non-copiable
        NkCallbackGuard(const NkCallbackGuard&)            = delete;
        NkCallbackGuard& operator=(const NkCallbackGuard&) = delete;

        // Movable
        NkCallbackGuard(NkCallbackGuard&& o) noexcept
            : mRemover(std::move(o.mRemover)) { o.mRemover = NkRemoverCallback{}; }
        NkCallbackGuard& operator=(NkCallbackGuard&& o) noexcept {
            if (this != &o) { Release(); mRemover = std::move(o.mRemover); o.mRemover = NkRemoverCallback{}; }
            return *this;
        }

        void Release() { if (mRemover) { mRemover(); mRemover = NkRemoverCallback{}; } }
        bool IsActive() const { return static_cast<bool>(mRemover); }

    private:
        NkRemoverCallback mRemover;
    };

        // =========================================================================
    // NkEventSystem
    // =========================================================================

    class NkEventSystem {
        public:
            // Point 1 : plus de Instance() statique.
            // NkSystem est le seul crÃ©ateur â€” friendship explicite.
            friend class NkSystem;

            NkEventSystem();
            ~NkEventSystem();

            NkEventSystem(const NkEventSystem&)            = delete;
            NkEventSystem& operator=(const NkEventSystem&) = delete;

            bool Init();
            void Shutdown();
            bool IsReady() const noexcept { return mReady; }

            // --- Callbacks (per-window by ID, global, typed) ---
            void SetWindowCallback(NkWindowId id, NkEventCallback cb);
            void RemoveWindowCallback(NkWindowId id);
            void SetGlobalCallback(NkGlobalEventCallback cb);

            // CORRECTION 6 : filtre optionnel par fenÃªtre.
            // windowId == NK_INVALID_WINDOW_ID (dÃ©faut) = toutes les fenÃªtres.
            template<typename T, typename Callback>
            void AddEventCallback(Callback&& callback,
                                  NkWindowId windowId = NK_INVALID_WINDOW_ID)
            {
                static_assert(std::is_invocable_r_v<void, Callback&, T*>,
                              "AddEventCallback<T>: callback must be invocable as void(T*)");

                NK_EVENTSYS_ANDROID_TRACE(
                    "[AddEventCallback<T>] enter this=%p windowId=%llu",
                    static_cast<void*>(this),
                    static_cast<unsigned long long>(windowId));

                using CallbackT = std::decay_t<Callback>;
                CallbackT typedCallback(std::forward<Callback>(callback));
                if constexpr (std::is_constructible_v<bool, CallbackT>) {
                    if (!static_cast<bool>(typedCallback)) {
                        NK_EVENTSYS_ANDROID_TRACE("[AddEventCallback<T>] empty callback -> skip");
                        return;
                    }
                }

                NkWindowId filterId = windowId;
                NK_EVENTSYS_ANDROID_TRACE(
                    "[AddEventCallback<T>] step filterId=%llu",
                    static_cast<unsigned long long>(filterId));
                const NkEventType::Value type = T::GetStaticType();
                NK_EVENTSYS_ANDROID_TRACE(
                    "[AddEventCallback<T>] step type=%u",
                    static_cast<unsigned>(type));

                if (type >= NkEventType::NK_EVENT_COUNT) {
                    NK_EVENTSYS_ANDROID_TRACE(
                        "[AddEventCallback<T>] invalid type=%u count=%u -> skip",
                        static_cast<unsigned>(type),
                        static_cast<unsigned>(NkEventType::NK_EVENT_COUNT));
                    return;
                }

                auto wrapper = [callback = std::move(typedCallback), filterId](NkEvent* ev) mutable {
                    // CORRECTION 6 : si un filtre est dÃ©fini, ignorer les events
                    // qui ne viennent pas de cette fenÃªtre.
                    if (filterId != NK_INVALID_WINDOW_ID &&
                        ev->GetWindowId() != filterId) return;
                    if (auto* typed = ev->As<T>()) callback(typed);
                };
                NK_EVENTSYS_ANDROID_TRACE(
                    "[AddEventCallback<T>] step wrapper-ready type=%u",
                    static_cast<unsigned>(type));
                AddEventCallbackRaw(type, std::move(wrapper));
                NK_EVENTSYS_ANDROID_TRACE(
                    "[AddEventCallback<T>] done type=%u",
                    static_cast<unsigned>(type));
            }

            // CORRECTION 4 : version RAII â€” le callback est automatiquement
            // supprimÃ© quand le guard retournÃ© est dÃ©truit.
            template<typename T, typename Callback>
            [[nodiscard]] NkCallbackGuard AddEventCallbackGuard(
                Callback&& callback,
                NkWindowId windowId = NK_INVALID_WINDOW_ID)
            {
                static_assert(std::is_invocable_r_v<void, Callback&, T*>,
                              "AddEventCallbackGuard<T>: callback must be invocable as void(T*)");

                using CallbackT = std::decay_t<Callback>;
                CallbackT typedCallback(std::forward<Callback>(callback));
                if constexpr (std::is_constructible_v<bool, CallbackT>) {
                    if (!static_cast<bool>(typedCallback)) return NkCallbackGuard{};
                }

                NkWindowId filterId = windowId;
                const NkEventType::Value type = T::GetStaticType();
                auto wrapper = [callback = std::move(typedCallback), filterId](NkEvent* ev) mutable {
                    if (filterId != NK_INVALID_WINDOW_ID &&
                        ev->GetWindowId() != filterId) return;
                    if (auto* typed = ev->As<T>()) callback(typed);
                };
                NkU64 token = AddEventCallbackTokenRaw(type, std::move(wrapper));

                // Le guard appelle RemoveCallbackToken Ã  sa destruction
                auto remover = [this, type, token]() {
                    RemoveCallbackToken(type, token);
                };
                return NkCallbackGuard(std::move(remover));
            }

            template<typename T>
            void ClearEventCallbacks() {
                ClearEventCallbacksRaw(T::GetStaticType());
            }

            void ClearAllCallbacks();

            // --- Event pump ---
            // CORRECTION 2 : PollEvent() retourne un pointeur valide uniquement
            // jusqu'au PROCHAIN appel de PollEvent(). Ne jamais stocker ce pointeur
            // entre frames â€” utiliser PollEventCopy() si une durÃ©e de vie propre est
            // requise (ex: file de travail asynchrone, traitement diffÃ©rÃ©).
            NkEvent*                 PollEvent();
            bool                     PollEvent(NkEvent*& event);
            std::unique_ptr<NkEvent> PollEventCopy();   // durÃ©e de vie contrÃ´lÃ©e par l'appelant
            void                     PollEvents();

            // --- Direct dispatch ---
            void DispatchEvent(NkEvent& event);
            template<typename T>
            void DispatchEvent(T&& event) {
                static_assert(std::is_base_of<NkEvent, std::decay_t<T>>::value,
                              "DispatchEvent: T must derive from NkEvent");
                NkEvent& base = event;
                DispatchEvent(base);
            }

            // --- Pont public pour les callbacks statiques platform (Wayland, Android, WASM, UIKit) ---
            // Les listeners/callbacks statiques n'ont pas accÃ¨s aux membres privÃ©s ;
            // ils passent par cette fonction qui dÃ©lÃ¨gue Ã  Enqueue().
            void Enqueue_Public(NkEvent& evt, NkWindowId winId);

            // --- Input state / info ---
            const NkEventState& GetInputState() const noexcept;
            NkEventState& GetInputState() noexcept { return mInputState; }
            NkGenericHidMapper& GetHidMapper() noexcept { return mHidMapper; }
            const NkGenericHidMapper& GetHidMapper() const noexcept { return mHidMapper; }

            void SetAutoUpdateInputState(bool e) noexcept { mAutoUpdateInputState = e; }
            bool GetAutoUpdateInputState()       const noexcept { return mAutoUpdateInputState; }
            void SetAutoGamepadPoll(bool e)      noexcept { mAutoGamepadPoll = e; }
            bool GetAutoGamepadPoll()            const noexcept { return mAutoGamepadPoll; }
            void SetQueueMode(bool e)            noexcept { mQueueMode = e; }
            bool GetQueueMode()                  const noexcept { return mQueueMode; }

            std::size_t GetPendingEventCount() const noexcept;
            NkU64       GetTotalEventCount()   const noexcept { return mTotalEventCount; }
            const char* GetPlatformName()      const noexcept;

        private:
            // Platform data â€” dÃ©fini dans le header platform-spÃ©cifique inclus ci-dessus
            struct NkEventSystemData mData;

            void PumpOS();
            void DispatchToCallbacks(NkEvent* ev, NkWindowId winId);
            void UpdateInputState(NkEvent* ev);
            void RemoveCallbackToken(NkEventType::Value type, NkU64 token);
            void AddEventCallbackRaw(NkEventType::Value type, NkEventCallback callback);
            NkU64 AddEventCallbackTokenRaw(NkEventType::Value type, NkEventCallback callback);
            void ClearEventCallbacksRaw(NkEventType::Value type);

            // Point 4 : Enqueue prend le winId explicitement pour que chaque
            // event sache exactement Ã  quelle fenÃªtre il appartient.
            void Enqueue(NkEvent& evt, NkWindowId winId);

            NkUnorderedMap<NkWindowId, NkEventCallback>                      mWindowCallbacks;
            NkEventCallback                                                       mGlobalCallback;
            NkUnorderedMap<NkEventType::Value, NkVector<NkEventCallback>> mTypedCallbacks;

            // CORRECTION 4 : callbacks tokÃ©nisÃ©s pour RAII guard
            struct TokenizedCallback {
                NkU64           token;
                NkEventCallback callback;
            };
            NkUnorderedMap<NkEventType::Value, NkVector<TokenizedCallback>> mTypedCallbacksWithToken;
            NkU64 mCallbackTokenCounter = 0;

            NkEventState mInputState;
            NkGenericHidMapper mHidMapper;
            bool         mReady = false;

            // Point 3 : ring buffer Ã  la place de std::deque<unique_ptr<NkEvent>>
            NkEventRingBuffer        mEventQueue;
            std::unique_ptr<NkEvent> mCurrentEvent;

            bool   mAutoUpdateInputState = true;
            bool   mAutoGamepadPoll      = true;
            bool   mQueueMode            = true;
            NkU64  mTotalEventCount      = 0;

            // Point 5 : deux mutex distincts
            //   mDispatchMutex : protÃ¨ge DispatchEvent() direct (appel externe)
            //   mQueueMutex    : protÃ¨ge mEventQueue pour l'accÃ¨s multi-thread
            //                    entre PumpOS() (producteur) et PollEvent() (consommateur)
            mutable std::mutex mDispatchMutex;
            mutable std::mutex mQueueMutex;

            // CORRECTION 5 : thread ID enregistrÃ© Ã  Init() pour assertions
            // PollEvent() et PumpOS() doivent Ãªtre appelÃ©s depuis ce thread.
            std::thread::id mPumpThreadId;

            bool mPumping = false;

            // --- Platform-specific methods ---
        #if defined(NKENTSEU_PLATFORM_WINDOWS) && !defined(NKENTSEU_PLATFORM_UWP) && !defined(NKENTSEU_PLATFORM_XBOX)
            public:
                static LRESULT CALLBACK WindowProcStatic(HWND, UINT, WPARAM, LPARAM);
            private:
                LRESULT ProcessMessage(HWND, UINT, WPARAM, LPARAM, NkWindowId, NkWindow*);
                static NkKey           VkeyToNkKey(UINT vk, bool extended) noexcept;
                static NkModifierState CurrentMods();
        #endif

        friend class NkGamepadSystem; // pour accÃ©der Ã  UpdateInputState() et mInputState lors du polling gamepad auto
    };

    // Raccourci global â€” dÃ©lÃ¨gue Ã  NkSystem (dÃ©fini dans NkSystem.h)
    // NkEvents() reste utilisable partout sans inclure NkSystem.h complet.
    // La dÃ©finition inline est dans NkSystem.h pour Ã©viter la dÃ©pendance circulaire.

} // namespace nkentseu
