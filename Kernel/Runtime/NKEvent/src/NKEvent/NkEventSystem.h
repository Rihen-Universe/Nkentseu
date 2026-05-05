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

#include "NKEvent/NkWindowId.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKLogger/NkLog.h"
#include "NKCore/NkAtomic.h"
#include "NKCore/NkInvoke.h"
#include "NKCore/NkTraits.h"
#include "NKMemory/NkUniquePtr.h"

#include "NKTime/NkChrono.h"

#define NK_EVENTSYS_ANDROID_TRACE(...) logger.Infof(__VA_ARGS__)

namespace nkentseu {

    // class NkWindow;
    // class NkSystem;

    using NkGlobalEventCallback = NkEventCallback;
    using NkTypedEventCallback  = NkEventCallback;
    using NkRemoverCallback     = NkFunction<void()>;

    struct NkEventDelete {
        void operator()(NkEvent* event) const noexcept { delete event; }
    };

    using NkEventPtr = memory::NkUniquePtr<NkEvent, NkEventDelete>;

    namespace detail {
        template<typename T, typename = void>
        struct NkIsBoolTestable : traits::NkFalseType {};

        template<typename T>
        struct NkIsBoolTestable<T, traits::NkVoidT<decltype(static_cast<bool>(traits::NkDeclVal<T&>()))>>
            : traits::NkTrueType {};

        template<typename T>
        inline constexpr nk_bool NkIsBoolTestable_v = NkIsBoolTestable<T>::value;
    } // namespace detail

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
        static constexpr nk_size kHighCapacity   =  128; // critique â€” no-drop
        static constexpr nk_size kNormalCapacity =  512; // droppable

        NkEventRingBuffer()  = default;
        ~NkEventRingBuffer() = default;

        NkEventRingBuffer(const NkEventRingBuffer&)            = delete;
        NkEventRingBuffer& operator=(const NkEventRingBuffer&) = delete;

        // CORRECTION 3 : push dans la file correspondant Ã  la prioritÃ©.
        // Retourne false uniquement pour NORMAL (drop-oldest), jamais pour HIGH.
        bool Push(NkEventPtr ev, NkEventPriority prio) {
            if (prio == NkEventPriority::HIGH) {
                return PushInto(mHighSlots, kHighCapacity, mHighHead, mHighTail,
                                traits::NkMove(ev), /*allowDrop=*/false);
            } else {
                return PushInto(mNormSlots, kNormalCapacity, mNormHead, mNormTail,
                                traits::NkMove(ev), /*allowDrop=*/true);
            }
        }

        // Pop : prioritÃ© HIGH d'abord, ensuite NORMAL.
        NkEventPtr Pop() {
            if (mHighHead != mHighTail) return PopFrom(mHighSlots, kHighCapacity, mHighHead, mHighTail);
            if (mNormHead != mNormTail) return PopFrom(mNormSlots, kNormalCapacity, mNormHead, mNormTail);
            return NkEventPtr(nullptr);
        }

        bool        Empty() const { return (mHighHead == mHighTail) && (mNormHead == mNormTail); }
        nk_size     Size()  const { return QueueSize(mHighHead, mHighTail, kHighCapacity)
                                         + QueueSize(mNormHead, mNormTail, kNormalCapacity); }
        void Clear() { while (!Empty()) Pop(); }

    private:
        static nk_size QueueSize(nk_size h, nk_size t, nk_size cap) noexcept {
            if (cap == 0) return 0;
            return (h >= t) ? (h - t) : (cap - t + h);
        }

        static bool PushInto(NkVector<NkEventPtr>& slots,
                              nk_size cap,
                              nk_size& head, nk_size& tail,
                              NkEventPtr ev,
                              bool allowDrop)
        {
            if (cap == 0) return false;
            nk_size next = (head + 1) % cap;
            bool full = (next == tail);
            if (full) {
                if (!allowDrop) return false; // HIGH : on ne droppe pas
                tail = (tail + 1) % cap;      // NORMAL : drop oldest
            }
            slots[head] = traits::NkMove(ev);
            head = next;
            return !full;
        }

        static NkEventPtr PopFrom(NkVector<NkEventPtr>& slots,
                                   nk_size cap,
                                   nk_size& head, nk_size& tail)
        {
            if (cap == 0) return NkEventPtr(nullptr);
            if (head == tail) return NkEventPtr(nullptr);
            auto ev = traits::NkMove(slots[tail]);
            tail = (tail + 1) % cap;
            return ev;
        }

        // File HAUTE PRIORITÃ‰ (no-drop)
        NkVector<NkEventPtr> mHighSlots{kHighCapacity};
        nk_size mHighHead = 0;
        nk_size mHighTail = 0;

        // File NORMALE (drop-oldest)
        NkVector<NkEventPtr> mNormSlots{kNormalCapacity};
        nk_size mNormHead = 0;
        nk_size mNormTail = 0;
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
                : mRemover(traits::NkMove(remover)) {}

            ~NkCallbackGuard() { Release(); }

            // Non-copiable
            NkCallbackGuard(const NkCallbackGuard&)            = delete;
            NkCallbackGuard& operator=(const NkCallbackGuard&) = delete;

            // Movable
            NkCallbackGuard(NkCallbackGuard&& o) noexcept
                : mRemover(traits::NkMove(o.mRemover)) { o.mRemover = NkRemoverCallback{}; }
            NkCallbackGuard& operator=(NkCallbackGuard&& o) noexcept {
                if (this != &o) { Release(); mRemover = traits::NkMove(o.mRemover); o.mRemover = NkRemoverCallback{}; }
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

    class NkGamepadSystem;  // forward — défini dans NkGamepadSystem.h

    class NkEventSystem {
        public:
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
                static_assert(NkIsInvocable_v<Callback&, T*>,
                              "AddEventCallback<T>: callback must be invocable as (T*)");

                NK_EVENTSYS_ANDROID_TRACE(
                    "[AddEventCallback<T>] enter this=%p windowId=%llu",
                    static_cast<void*>(this),
                    static_cast<unsigned long long>(windowId));

                using CallbackT = traits::NkDecay_t<Callback>;
                CallbackT typedCallback(traits::NkForward<Callback>(callback));
                if constexpr (detail::NkIsBoolTestable_v<CallbackT>) {
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

                auto wrapper = [callback = traits::NkMove(typedCallback), filterId](NkEvent* ev) mutable {
                    // CORRECTION 6 : si un filtre est dÃ©fini, ignorer les events
                    // qui ne viennent pas de cette fenÃªtre.
                    if (filterId != NK_INVALID_WINDOW_ID &&
                        ev->GetWindowId() != filterId) return;
                    if (auto* typed = ev->As<T>()) callback(typed);
                };
                NK_EVENTSYS_ANDROID_TRACE(
                    "[AddEventCallback<T>] step wrapper-ready type=%u",
                    static_cast<unsigned>(type));
                AddEventCallbackRaw(type, traits::NkMove(wrapper));
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
                static_assert(NkIsInvocable_v<Callback&, T*>,
                              "AddEventCallbackGuard<T>: callback must be invocable as (T*)");

                using CallbackT = traits::NkDecay_t<Callback>;
                CallbackT typedCallback(traits::NkForward<Callback>(callback));
                if constexpr (detail::NkIsBoolTestable_v<CallbackT>) {
                    if (!static_cast<bool>(typedCallback)) return NkCallbackGuard{};
                }

                NkWindowId filterId = windowId;
                const NkEventType::Value type = T::GetStaticType();
                auto wrapper = [callback = traits::NkMove(typedCallback), filterId](NkEvent* ev) mutable {
                    if (filterId != NK_INVALID_WINDOW_ID &&
                        ev->GetWindowId() != filterId) return;
                    if (auto* typed = ev->As<T>()) callback(typed);
                };
                uint64 token = AddEventCallbackTokenRaw(type, traits::NkMove(wrapper));

                // Le guard appelle RemoveCallbackToken Ã  sa destruction
                auto remover = [this, type, token]() {
                    RemoveCallbackToken(type, token);
                };
                return NkCallbackGuard(traits::NkMove(remover));
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
            NkEventPtr               PollEventCopy();   // durÃ©e de vie contrÃ´lÃ©e par l'appelant
            void                     PollEvents();

            // --- Direct dispatch ---
            void DispatchEvent(NkEvent& event);
            template<typename T>
            void DispatchEvent(T&& event) {
                static_assert(traits::NkIsBaseOf_v<NkEvent, traits::NkDecay_t<T>>,
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

            // Injection de dependance -- appele par NkSystem::Initialise()
            void SetGamepadSystem(NkGamepadSystem* gp) noexcept { mGamepadSystem = gp; }

            void SetAutoUpdateInputState(bool e) noexcept { mAutoUpdateInputState = e; }
            bool GetAutoUpdateInputState()       const noexcept { return mAutoUpdateInputState; }
            void SetAutoGamepadPoll(bool e)      noexcept { mAutoGamepadPoll = e; }
            bool GetAutoGamepadPoll()            const noexcept { return mAutoGamepadPoll; }
            void SetQueueMode(bool e)            noexcept { mQueueMode = e; }
            bool GetQueueMode()                  const noexcept { return mQueueMode; }

            nk_size GetPendingEventCount() const noexcept;
            uint64       GetTotalEventCount()   const noexcept { return mTotalEventCount; }
            const char* GetPlatformName()      const noexcept;

        protected:
            // Platform data -- defini dans le .cpp platform-specifique.
            // Accessible aux classes derivees platform (ex. NkWin32EventSystem).
            struct NkEventSystemData *mData = nullptr;

            // Enqueue est protected pour que les classes derivees platform
            // puissent soumettre des evenements directement (evite Enqueue_Public).
            void Enqueue(NkEvent& evt, NkWindowId winId);

        private:
            void PumpOS();
            void DispatchToCallbacks(NkEvent* ev, NkWindowId winId);
            void UpdateInputState(NkEvent* ev);
            void RemoveCallbackToken(NkEventType::Value type, uint64 token);
            void AddEventCallbackRaw(NkEventType::Value type, NkEventCallback callback);
            uint64 AddEventCallbackTokenRaw(NkEventType::Value type, NkEventCallback callback);
            void ClearEventCallbacksRaw(NkEventType::Value type);

            NkUnorderedMap<NkWindowId, NkEventCallback>                      mWindowCallbacks;
            NkEventCallback                                                       mGlobalCallback;
            NkUnorderedMap<NkEventType::Value, NkVector<NkEventCallback>> mTypedCallbacks;

            // CORRECTION 4 : callbacks tokÃ©nisÃ©s pour RAII guard
            struct TokenizedCallback {
                uint64           token;
                NkEventCallback callback;
            };
            NkUnorderedMap<NkEventType::Value, NkVector<TokenizedCallback>> mTypedCallbacksWithToken;
            uint64 mCallbackTokenCounter = 0;

            NkGamepadSystem* mGamepadSystem = nullptr;  // injecte par NkSystem::Initialise()

            NkEventState mInputState;
            NkGenericHidMapper mHidMapper;
            bool         mReady = false;

            // Point 3 : ring buffer Ã  la place d'une deque dynamique
            NkEventRingBuffer mEventQueue;
            NkEventPtr        mCurrentEvent;

            bool   mAutoUpdateInputState = true;
            bool   mAutoGamepadPoll      = true;
            bool   mQueueMode            = true;
            uint64  mTotalEventCount      = 0;

            // Point 5 : deux mutex distincts
            //   mDispatchMutex : protÃ¨ge DispatchEvent() direct (appel externe)
            //   mQueueMutex    : protÃ¨ge mEventQueue pour l'accÃ¨s multi-thread
            //                    entre PumpOS() (producteur) et PollEvent() (consommateur)
            mutable NkSpinLock mDispatchMutex;
            mutable NkSpinLock mQueueMutex;

            // CORRECTION 5 : thread ID enregistrÃ© Ã  Init() pour assertions
            // PollEvent() et PumpOS() doivent Ãªtre appelÃ©s depuis ce thread.
            uint64 mPumpThreadId = 0;

            bool mPumping = false;

        friend class NkGamepadSystem; // pour accÃ©der Ã  UpdateInputState() et mInputState lors du polling gamepad auto
    };

    // Raccourci global â€” dÃ©lÃ¨gue Ã  NkSystem (dÃ©fini dans NkSystem.h)
    // NkEvents() reste utilisable partout sans inclure NkSystem.h complet.
    // La dÃ©finition inline est dans NkSystem.h pour Ã©viter la dÃ©pendance circulaire.

} // namespace nkentseu
