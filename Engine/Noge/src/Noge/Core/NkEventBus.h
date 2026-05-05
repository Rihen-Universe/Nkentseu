#pragma once

// =============================================================================
// EventBus.h
// Système publish/subscribe typé pour Nkentseu.
//
// Conception :
//   - Zéro allocation dynamique dans le chemin critique (dispatch).
//   - Handlers stockés comme pointeurs de fonction + contexte (type-erasure).
//   - Jusqu'à NK_EVENTBUS_MAX_HANDLERS handlers par type d'événement.
//   - Les handlers retournent bool : true = événement consommé, stop propagation.
//   - Thread-safe en lecture (dispatch depuis plusieurs threads OK).
//     Subscribe/Unsubscribe doivent être appelés depuis le thread principal.
//
// Usage :
//   // Souscription avec lambda
//   auto id = EventBus::Subscribe<NkKeyPressEvent>(
//       [](NkKeyPressEvent* e) -> bool { ... return false; });
//
//   // Souscription avec méthode membre
//   auto id = EventBus::Subscribe<NkMouseMoveEvent>(this, &MyClass::OnMouse);
//
//   // Dispatch
//   NkKeyPressEvent evt(NkKey::NK_A, false, false);
//   EventBus::Dispatch(&evt);
//
//   // Désouscription
//   EventBus::Unsubscribe<NkKeyPressEvent>(id);
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKEvent/NkEvent.h"

namespace nkentseu {

    // Nombre maximum de handlers par type d'événement.
    // Augmenter si nécessaire (coût : mémoire statique supplémentaire).
    #ifndef NK_EVENTBUS_MAX_HANDLERS
    #   define NK_EVENTBUS_MAX_HANDLERS 32
    #endif

    // Identifiant de souscription — retourné par Subscribe, utilisé pour Unsubscribe.
    using NkEventHandlerId = nk_uint32;
    static constexpr NkEventHandlerId NK_INVALID_HANDLER_ID = 0u;

    // =============================================================================
    // Implémentation interne — ne pas utiliser directement
    // =============================================================================
    namespace internal {

        // Signature d'un handler effacé (type-erased).
        // fn  : pointeur vers la fonction dispatch
        // ctx : contexte (this pour méthode membre, nullptr pour lambda stocké)
        // lam : stockage inline du functor (évite heap)
        using NkRawDispatchFn = bool(*)(void* ctx, void* lam, NkEvent* event);

        static constexpr nk_usize kLambdaStorage = 64; // bytes inline pour le functor

        struct NkHandlerSlot {
            NkEventHandlerId id      = NK_INVALID_HANDLER_ID;
            bool             active  = false;
            NkRawDispatchFn    dispatch = nullptr;
            void*            ctx      = nullptr;
            alignas(8) nk_uint8 storage[kLambdaStorage] = {};
        };

        // Table de handlers pour un type d'événement donné (identifié par typeId).
        struct NkEventTable {
            nk_uint32   typeId                              = 0;
            nk_uint32   count                               = 0;
            NkHandlerSlot slots[NK_EVENTBUS_MAX_HANDLERS]     = {};
        };

        // Registre global — tableau fixe de tables (une par type d'événement utilisé).
    #ifndef NK_EVENTBUS_MAX_EVENT_TYPES
    #   define NK_EVENTBUS_MAX_EVENT_TYPES 64
    #endif

        struct NkBusRegistry {
            NkEventTable   tables[NK_EVENTBUS_MAX_EVENT_TYPES] = {};
            nk_uint32    tableCount  = 0;
            NkEventHandlerId nextId  = 1u; // 0 réservé pour invalide

            NkEventTable* FindOrCreate(nk_uint32 typeId) {
                for (nk_uint32 i = 0; i < tableCount; ++i) {
                    if (tables[i].typeId == typeId) return &tables[i];
                }
                if (tableCount >= NK_EVENTBUS_MAX_EVENT_TYPES) return nullptr;
                tables[tableCount].typeId = typeId;
                return &tables[tableCount++];
            }

            NkEventTable* Find(nk_uint32 typeId) {
                for (nk_uint32 i = 0; i < tableCount; ++i) {
                    if (tables[i].typeId == typeId) return &tables[i];
                }
                return nullptr;
            }
        };

        inline NkBusRegistry& NkGetRegistry() {
            static NkBusRegistry reg;
            return reg;
        }

        // ── Helpers de dispatch ───────────────────────────────────────────────────

        // Dispatch d'un functor stocké inline (lambda, std::function-like).
        template<typename TEvent, typename TFunctor>
        bool NkDispatchFunctor(void* /*ctx*/, void* lam, NkEvent* event) {
            TFunctor* fn = reinterpret_cast<TFunctor*>(lam);
            return (*fn)(static_cast<TEvent*>(event));
        }

        // Dispatch d'une méthode membre.
        template<typename TClass, typename TEvent>
        bool NkDispatchMember(void* ctx, void* lam, NkEvent* event) {
            using MemberFn = bool(TClass::*)(TEvent*);
            MemberFn fn;
            static_assert(sizeof(fn) <= kLambdaStorage,
                "Pointeur de méthode trop grand pour le stockage inline");
            __builtin_memcpy(&fn, lam, sizeof(fn));
            TClass* obj = reinterpret_cast<TClass*>(ctx);
            return (obj->*fn)(static_cast<TEvent*>(event));
        }

    } // namespace internal

    // =============================================================================
    // EventBus — API publique
    // =============================================================================
    class NkEventBus {
        public:
            // ── Subscribe avec functor (lambda, fonction libre) ───────────────────────
            // TEvent  : type de l'événement (ex: NkKeyPressEvent)
            // TFunctor : callable bool(TEvent*)
            template<typename TEvent, typename TFunctor>
            static NkEventHandlerId Subscribe(TFunctor&& functor) {
                static_assert(sizeof(TFunctor) <= internal::NkkLambdaStorage,
                    "Functor trop grand pour le stockage inline EventBus — "
                    "capturer moins de variables ou augmenter kLambdaStorage");

                auto& reg   = internal::NkGetRegistry();
                nk_uint32 typeId = TEvent::StaticTypeId();
                auto* table = reg.FindOrCreate(typeId);
                if (!table) return NK_INVALID_HANDLER_ID;

                // Chercher un slot libre
                for (nk_uint32 i = 0; i < NK_EVENTBUS_MAX_HANDLERS; ++i) {
                    auto& slot = table->slots[i];
                    if (slot.active) continue;

                    slot.id       = reg.nextId++;
                    slot.active   = true;
                    slot.dispatch = &internal::NkDispatchFunctor<TEvent, TFunctor>;
                    slot.ctx      = nullptr;
                    // Placement new du functor dans le stockage inline
                    new (slot.storage) TFunctor(static_cast<TFunctor&&>(functor));
                    ++table->count;
                    return slot.id;
                }
                return NK_INVALID_HANDLER_ID; // table pleine
            }

            // ── Subscribe avec méthode membre ────────────────────────────────────────
            // TClass  : type de la classe (ex: MyLayer)
            // TEvent  : type de l'événement
            // fn      : pointeur de méthode bool(TClass::*)(TEvent*)
            template<typename TEvent, typename TClass>
            static NkEventHandlerId Subscribe(TClass* obj,
                                            bool(TClass::*fn)(TEvent*)) {
                using MemberFn = bool(TClass::*)(TEvent*);
                static_assert(sizeof(MemberFn) <= internal::NkkLambdaStorage,
                    "Pointeur de méthode trop grand pour le stockage inline EventBus");

                auto& reg    = internal::NkGetRegistry();
                nk_uint32 typeId = TEvent::StaticTypeId();
                auto* table  = reg.FindOrCreate(typeId);
                if (!table) return NK_INVALID_HANDLER_ID;

                for (nk_uint32 i = 0; i < NK_EVENTBUS_MAX_HANDLERS; ++i) {
                    auto& slot = table->slots[i];
                    if (slot.active) continue;

                    slot.id       = reg.nextId++;
                    slot.active   = true;
                    slot.dispatch = &internal::NkDispatchMember<TClass, TEvent>;
                    slot.ctx      = reinterpret_cast<void*>(obj);
                    __builtin_memcpy(slot.storage, &fn, sizeof(fn));
                    ++table->count;
                    return slot.id;
                }
                return NK_INVALID_HANDLER_ID;
            }

            // ── Unsubscribe par ID ────────────────────────────────────────────────────
            template<typename TEvent>
            static void Unsubscribe(NkEventHandlerId id) {
                if (id == NK_INVALID_HANDLER_ID) return;
                auto& reg   = internal::NkGetRegistry();
                auto* table = reg.Find(TEvent::StaticTypeId());
                if (!table) return;

                for (nk_uint32 i = 0; i < NK_EVENTBUS_MAX_HANDLERS; ++i) {
                    auto& slot = table->slots[i];
                    if (slot.active && slot.id == id) {
                        slot.active   = false;
                        slot.id       = NK_INVALID_HANDLER_ID;
                        slot.dispatch = nullptr;
                        slot.ctx      = nullptr;
                        if (table->count > 0) --table->count;
                        return;
                    }
                }
            }

            // ── Unsubscribe toutes les souscriptions d'un objet ───────────────────────
            // Utile dans OnDetach() d'un Layer pour nettoyer d'un coup.
            static void UnsubscribeAll(void* obj) {
                auto& reg = internal::NkGetRegistry();
                for (nk_uint32 t = 0; t < reg.tableCount; ++t) {
                    auto& table = reg.tables[t];
                    for (nk_uint32 i = 0; i < NK_EVENTBUS_MAX_HANDLERS; ++i) {
                        auto& slot = table.slots[i];
                        if (slot.active && slot.ctx == obj) {
                            slot.active   = false;
                            slot.id       = NK_INVALID_HANDLER_ID;
                            slot.dispatch = nullptr;
                            slot.ctx      = nullptr;
                            if (table.count > 0) --table.count;
                        }
                    }
                }
            }

            // ── Dispatch ──────────────────────────────────────────────────────────────
            // Parcourt les handlers du type de l'événement.
            // S'arrête dès qu'un handler retourne true (événement consommé).
            // Retourne true si l'événement a été consommé.
            template<typename TEvent>
            static bool Dispatch(TEvent* event) {
                return DispatchRaw(TEvent::StaticTypeId(), static_cast<NkEvent*>(event));
            }

            // Variante sans type connu à la compilation (pour dispatch depuis NKEvent).
            static bool DispatchRaw(nk_uint32 typeId, NkEvent* event) {
                if (!event) return false;
                auto& reg   = internal::NkGetRegistry();
                auto* table = reg.Find(typeId);
                if (!table || table->count == 0) return false;

                for (nk_uint32 i = 0; i < NK_EVENTBUS_MAX_HANDLERS; ++i) {
                    auto& slot = table->slots[i];
                    if (!slot.active || !slot.dispatch) continue;
                    if (slot.dispatch(slot.ctx,
                                    reinterpret_cast<void*>(slot.storage),
                                    event)) {
                        return true; // consommé
                    }
                }
                return false;
            }

            // ── Utilitaire debug ──────────────────────────────────────────────────────
            static nk_uint32 GetHandlerCount(nk_uint32 typeId) {
                auto* table = internal::NkGetRegistry().Find(typeId);
                return table ? table->count : 0u;
            }

            static void Clear() {
                internal::NkGetRegistry() = internal::NkBusRegistry{};
            }
    };

} // namespace nkentseu
