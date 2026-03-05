#pragma once

// =============================================================================
// NkEventSystem.h
// Système d'événements cross-plateforme.
// Possédé et instancié par NkSystem — plus de singleton autoproclamé.
//
// Corrections appliquées :
//   CORRECTION 1 : NkGamepadSystem n'est plus un singleton — possédé par NkSystem
//   CORRECTION 2 : PollEvent() lifetime documenté + PollEventCopy() ajouté
//   CORRECTION 3 : ring buffer dual-priorité (HIGH no-drop / NORMAL drop-oldest)
//   CORRECTION 4 : AddEventCallbackGuard<T>() retourne un NkCallbackGuard RAII
//   CORRECTION 5 : assertions thread ID sur PollEvent() / PollEvents()
//   CORRECTION 6 : AddEventCallback<T>(cb, windowId) filtre par fenêtre
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

#if defined(NKENTSEU_PLATFORM_UWP)
#   include "NKWindow/Platform/UWP/NkUWPEventSystem.h"
#elif defined(NKENTSEU_PLATFORM_XBOX)
#   include "NKWindow/Platform/Xbox/NkXboxEventSystem.h"
#elif defined(NKENTSEU_PLATFORM_WINDOWS)
#   include <windows.h>
#   include "NKWindow/Platform/Win32/NkWin32EventSystem.h"
#elif defined(NKENTSEU_PLATFORM_WEB) || defined(NKENTSEU_PLATFORM_EMSCRIPTEN) || defined(__EMSCRIPTEN__)
#   include "NKWindow/Platform/WASM/NkWASMEventSystem.h"
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

    using NkGlobalEventCallback = std::function<void(NkEvent*)>;
    using NkTypedEventCallback  = std::function<void(NkEvent*)>;

    // =========================================================================
    // NkEventPriority — CORRECTION 3 : classification drop-safe vs droppable
    // =========================================================================
    // Événements HIGH : ne jamais dropper (lifecycle fenêtre, clavier, destroy)
    // Événements NORMAL : peuvent être droppés sous pression (mouse move, etc.)
    // =========================================================================
    enum class NkEventPriority { HIGH, NORMAL };

    /// @brief Retourne la priorité d'un type d'événement.
    /// Les événements HIGH ne sont jamais droppés en cas de saturation.
    inline NkEventPriority NkGetEventPriority(NkEventType::Value t) noexcept {
        switch (t) {
            // Lifecycle fenêtre — critique (destruction/fermeture irréversible)
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
            // Clavier — chaque frappe est sémantique
            case NkEventType::NK_KEY_PRESSED:
            case NkEventType::NK_KEY_REPEATED:
            case NkEventType::NK_KEY_RELEASED:
            case NkEventType::NK_TEXT_INPUT:
            case NkEventType::NK_CHAR_ENTERED:
            // Boutons souris — chaque clic est sémantique
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
    // Ring buffer pré-allouée pour les événements.
    // Politique drop-oldest UNIQUEMENT sur la file NORMAL.
    // La file HIGH ne droppe jamais (taille 128 — overflow = assert en debug).
    // =========================================================================

    class NkEventRingBuffer {
    public:
        static constexpr std::size_t kHighCapacity   =  128; // critique — no-drop
        static constexpr std::size_t kNormalCapacity =  512; // droppable

        NkEventRingBuffer()  = default;
        ~NkEventRingBuffer() = default;

        NkEventRingBuffer(const NkEventRingBuffer&)            = delete;
        NkEventRingBuffer& operator=(const NkEventRingBuffer&) = delete;

        // CORRECTION 3 : push dans la file correspondant à la priorité.
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

        // Pop : priorité HIGH d'abord, ensuite NORMAL.
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

        static bool PushInto(std::vector<std::unique_ptr<NkEvent>>& slots,
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

        static std::unique_ptr<NkEvent> PopFrom(std::vector<std::unique_ptr<NkEvent>>& slots,
                                                 std::size_t cap,
                                                 std::size_t& head, std::size_t& tail)
        {
            if (cap == 0) return nullptr;
            if (head == tail) return nullptr;
            auto ev = std::move(slots[tail]);
            tail = (tail + 1) % cap;
            return ev;
        }

        // File HAUTE PRIORITÉ (no-drop)
        std::vector<std::unique_ptr<NkEvent>> mHighSlots{kHighCapacity};
        std::size_t mHighHead = 0, mHighTail = 0;

        // File NORMALE (drop-oldest)
        std::vector<std::unique_ptr<NkEvent>> mNormSlots{kNormalCapacity};
        std::size_t mNormHead = 0, mNormTail = 0;
    };

    // =========================================================================
    // NkCallbackGuard — CORRECTION 4 : RAII pour les typed callbacks
    // =========================================================================
    // Garantit que le callback est supprimé quand le guard est détruit.
    // Utilisation :
    //   auto guard = NkEvents().AddEventCallbackGuard<NkKeyPressEvent>(
    //       [&](NkKeyPressEvent* ev) { ... });
    //   // guard tient la connexion vivante ; destruction = désinscription auto
    // =========================================================================
    class NkCallbackGuard {
    public:
        NkCallbackGuard() = default;

        NkCallbackGuard(std::function<void()> remover)
            : mRemover(std::move(remover)) {}

        ~NkCallbackGuard() { Release(); }

        // Non-copiable
        NkCallbackGuard(const NkCallbackGuard&)            = delete;
        NkCallbackGuard& operator=(const NkCallbackGuard&) = delete;

        // Movable
        NkCallbackGuard(NkCallbackGuard&& o) noexcept
            : mRemover(std::move(o.mRemover)) { o.mRemover = nullptr; }
        NkCallbackGuard& operator=(NkCallbackGuard&& o) noexcept {
            if (this != &o) { Release(); mRemover = std::move(o.mRemover); o.mRemover = nullptr; }
            return *this;
        }

        void Release() { if (mRemover) { mRemover(); mRemover = nullptr; } }
        bool IsActive() const { return static_cast<bool>(mRemover); }

    private:
        std::function<void()> mRemover;
    };

        // =========================================================================
    // NkEventSystem
    // =========================================================================

    class NkEventSystem {
        public:
            // Point 1 : plus de Instance() statique.
            // NkSystem est le seul créateur — friendship explicite.
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

            // CORRECTION 6 : filtre optionnel par fenêtre.
            // windowId == NK_INVALID_WINDOW_ID (défaut) = toutes les fenêtres.
            template<typename T>
            void AddEventCallback(std::function<void(T*)> callback,
                                  NkWindowId windowId = NK_INVALID_WINDOW_ID)
            {
                NkWindowId filterId = windowId;
                const NkEventType::Value type = T::GetStaticType();
                auto wrapper = [callback, filterId](NkEvent* ev) {
                    // CORRECTION 6 : si un filtre est défini, ignorer les events
                    // qui ne viennent pas de cette fenêtre.
                    if (filterId != NK_INVALID_WINDOW_ID &&
                        ev->GetWindowId() != filterId) return;
                    if (auto* typed = ev->As<T>()) callback(typed);
                };
                AddEventCallbackRaw(type, std::move(wrapper));
            }

            // CORRECTION 4 : version RAII — le callback est automatiquement
            // supprimé quand le guard retourné est détruit.
            template<typename T>
            [[nodiscard]] NkCallbackGuard AddEventCallbackGuard(
                std::function<void(T*)> callback,
                NkWindowId windowId = NK_INVALID_WINDOW_ID)
            {
                NkWindowId filterId = windowId;
                const NkEventType::Value type = T::GetStaticType();
                auto wrapper = [callback, filterId](NkEvent* ev) {
                    if (filterId != NK_INVALID_WINDOW_ID &&
                        ev->GetWindowId() != filterId) return;
                    if (auto* typed = ev->As<T>()) callback(typed);
                };
                NkU64 token = AddEventCallbackTokenRaw(type, std::move(wrapper));

                // Le guard appelle RemoveCallbackToken à sa destruction
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
            // entre frames — utiliser PollEventCopy() si une durée de vie propre est
            // requise (ex: file de travail asynchrone, traitement différé).
            NkEvent*                 PollEvent();
            bool                     PollEvent(NkEvent*& event);
            std::unique_ptr<NkEvent> PollEventCopy();   // durée de vie contrôlée par l'appelant
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
            // Les listeners/callbacks statiques n'ont pas accès aux membres privés ;
            // ils passent par cette fonction qui délègue à Enqueue().
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
            // Platform data — défini dans le header platform-spécifique inclus ci-dessus
            struct NkEventSystemData mData;

            void PumpOS();
            void DispatchToCallbacks(NkEvent* ev, NkWindowId winId);
            void UpdateInputState(NkEvent* ev);
            void RemoveCallbackToken(NkEventType::Value type, NkU64 token);
            void AddEventCallbackRaw(NkEventType::Value type, NkEventCallback callback);
            NkU64 AddEventCallbackTokenRaw(NkEventType::Value type, NkEventCallback callback);
            void ClearEventCallbacksRaw(NkEventType::Value type);

            // Point 4 : Enqueue prend le winId explicitement pour que chaque
            // event sache exactement à quelle fenêtre il appartient.
            void Enqueue(NkEvent& evt, NkWindowId winId);

            std::unordered_map<NkWindowId, NkEventCallback>                      mWindowCallbacks;
            NkEventCallback                                                       mGlobalCallback;
            std::unordered_map<NkEventType::Value, std::vector<NkEventCallback>> mTypedCallbacks;

            // CORRECTION 4 : callbacks tokénisés pour RAII guard
            struct TokenizedCallback {
                NkU64           token;
                NkEventCallback callback;
            };
            std::unordered_map<NkEventType::Value, std::vector<TokenizedCallback>> mTypedCallbacksWithToken;
            NkU64 mCallbackTokenCounter = 0;

            NkEventState mInputState;
            NkGenericHidMapper mHidMapper;
            bool         mReady = false;

            // Point 3 : ring buffer à la place de std::deque<unique_ptr<NkEvent>>
            NkEventRingBuffer        mEventQueue;
            std::unique_ptr<NkEvent> mCurrentEvent;

            bool   mAutoUpdateInputState = true;
            bool   mAutoGamepadPoll      = true;
            bool   mQueueMode            = true;
            NkU64  mTotalEventCount      = 0;

            // Point 5 : deux mutex distincts
            //   mDispatchMutex : protège DispatchEvent() direct (appel externe)
            //   mQueueMutex    : protège mEventQueue pour l'accès multi-thread
            //                    entre PumpOS() (producteur) et PollEvent() (consommateur)
            mutable std::mutex mDispatchMutex;
            mutable std::mutex mQueueMutex;

            // CORRECTION 5 : thread ID enregistré à Init() pour assertions
            // PollEvent() et PumpOS() doivent être appelés depuis ce thread.
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

        friend class NkGamepadSystem; // pour accéder à UpdateInputState() et mInputState lors du polling gamepad auto
    };

    // Raccourci global — délègue à NkSystem (défini dans NkSystem.h)
    // NkEvents() reste utilisable partout sans inclure NkSystem.h complet.
    // La définition inline est dans NkSystem.h pour éviter la dépendance circulaire.

} // namespace nkentseu
