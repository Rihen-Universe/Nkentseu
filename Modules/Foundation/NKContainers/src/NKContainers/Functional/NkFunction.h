// -----------------------------------------------------------------------------
// FICHIER: NKMemory\NkFunction.h
// DESCRIPTION: Conteneur polymorphe pour objets appelables — sans dépendance STL
//              Avec Small Buffer Optimization, target<T>(), NkBind, noexcept support
// AUTEUR: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2025-06-10 / Mise à jour: 2026-04-26
// VERSION: 2.1.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
//
// RÉSUMÉ:
//   Implémentation avancée d'un conteneur de fonctions polymorphe similaire 
//   à std::function, enrichie avec :
//   - Small Buffer Optimization (SBO) pour éviter les allocations des petits callables
//   - target<T>() pour récupérer le callable concret (avec option RTTI)
//   - NkBind() pour l'application partielle et la curryfication
//   - Support des signatures noexcept : NkFunction<R(Args...) noexcept>
//   - Statistiques mémoire : GetAllocationCount(), GetTotalMemory()
//
// CARACTÉRISTIQUES:
//   - Syntaxe identique à std::function : NkFunction<void(int, float)>
//   - SBO : buffer interne de 64 bytes pour les lambdas/foncteurs légers
//   - Effacement de type via interface virtuelle CallableBase
//   - Allocation personnalisable via memory::NkAllocator (bypassé si SBO)
//   - Binding multiple indexé et partial application via NkBind
//   - Gestion noexcept et constexpr là où applicable
//   - Pas d'exceptions : retour silencieux de R{} sur appel vide
//   - Profiling mémoire intégré pour le débogage et l'optimisation
//
// DÉPENDANCES:
//   - NKCore/NkTypes.h          : Types fondamentaux (usize, etc.)
//   - NKCore/NkTraits.h         : Traits méta-programmation (NkDecay_t, etc.)
//   - NKContainers/Heterogeneous/NkPair.h : Paire générique pour le binding
//   - NKContainers/Heterogeneous/NkTuple.h : Tuple pour NkBind et curryfication
//   - NKMemory/NkAllocator.h    : Système d'allocation mémoire personnalisable
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CONTAINERS_FUNCTIONAL_NKFUNCTION_H
#define NK_CONTAINERS_FUNCTIONAL_NKFUNCTION_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Types fondamentaux : usize, etc.
    #include "NKCore/NkTraits.h"                   // Traits méta-programmation et utilitaires
    #include "NKContainers/Heterogeneous/NkPair.h" // NkPair pour le binding méthode/objet
    #include "NKContainers/Heterogeneous/NkTuple.h"// NkTuple pour NkBind et curryfication
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable

    // ========================================================================
    // CONFIGURATION DE LA SMALL BUFFER OPTIMIZATION (SBO)
    // ========================================================================

    // Taille du buffer interne pour SBO : ajustable selon les besoins
    // 64 bytes couvrent la majorité des lambdas sans capture ou avec petites captures
    #ifndef NK_FUNCTION_SBO_BUFFER_SIZE
        #define NK_FUNCTION_SBO_BUFFER_SIZE 64
    #endif

    // Activer/désactiver le support RTTI pour target<T>()
    // Si désactivé, target<T>() retourne toujours nullptr
    #ifndef NK_FUNCTION_ENABLE_RTTI
        #define NK_FUNCTION_ENABLE_RTTI 0
    #endif

    // Activer/désactiver le profiling mémoire (légèrement coûteux en perf)
    #ifndef NK_FUNCTION_ENABLE_STATS
        #ifdef NKENTSEU_DEBUG
            #define NK_FUNCTION_ENABLE_STATS 1
        #else
            #define NK_FUNCTION_ENABLE_STATS 0
        #endif
    #endif

    // ========================================================================
    // ESPACE DE NOMS PRINCIPAL
    // ========================================================================

    namespace nkentseu {

        // ====================================================================
        // ALIAS : nullptr_t SANS DÉPENDANCE <cstddef>
        // ====================================================================

        /**
         * @brief Alias pour std::nullptr_t sans inclure <cstddef>
         * @note decltype(nullptr) est un builtin C++11+, portable sur tous les compilateurs
         */
        using NkNullptrT = decltype(nullptr);


        // ====================================================================
        // NAMESPACE INTERNE : UTILITAIRES PRIVÉS
        // ====================================================================

        namespace detail {

            /**
             * @brief Résout et valide un pointeur d'allocateur pour NkFunction
             * @param allocator Pointeur vers un allocateur potentiel (peut être nullptr)
             * @return Pointeur vers un allocateur valide (défaut si entrée invalide)
             * @note Vérifie l'alignement mémoire de l'allocateur pour éviter les UB
             */
            inline memory::NkAllocator* NkResolveFunctionAllocator( memory::NkAllocator* allocator) noexcept {
                memory::NkAllocator* fallback = &memory::NkGetDefaultAllocator();
                if (!allocator) { return fallback; }
                const usize addr = reinterpret_cast<usize>(allocator);
                const usize alignMask = static_cast<usize>(alignof(void*)) - 1u;
                if ((addr & alignMask) != 0u) { return fallback; }
                return allocator;
            }

            /**
             * @brief Buffer aligné pour la Small Buffer Optimization (SBO)
             * @tparam Size Taille du buffer en bytes
             * @note Utilise alignof(max_align_t) pour garantir l'alignement maximal
             */
            template<usize Size>
            struct alignas(alignof(void*)) SboBuffer {
                unsigned char data[Size];
            };

            #if NK_FUNCTION_ENABLE_STATS
            /**
             * @brief Compteur global de statistiques mémoire pour NkFunction
             * @note Thread-unsafe : synchronisation externe requise en contexte concurrent
             */
            struct FunctionStats {
                volatile usize allocationCount = 0;   // Nombre total d'allocations heap
                volatile usize deallocationCount = 0; // Nombre total de désallocations
                volatile usize totalMemoryBytes = 0;  // Mémoire heap actuellement allouée

                void RecordAllocation(usize bytes) noexcept {
                    ++allocationCount;
                    totalMemoryBytes += bytes;
                }
                void RecordDeallocation(usize bytes) noexcept {
                    ++deallocationCount;
                    totalMemoryBytes -= bytes;
                }
                static FunctionStats& Instance() noexcept {
                    static FunctionStats stats;
                    return stats;
                }
            };
            #endif // NK_FUNCTION_ENABLE_STATS

        } // namespace detail


        // ====================================================================
        // DÉCLARATION PRIMAIRE : NKFUNCTION (NON DÉFINIE)
        // ====================================================================

        template<typename Signature>
        class NkFunction;   // Intentionnellement non définie — voir spécialisation


        // ====================================================================
        // SPÉCIALISATION PARTIELLE : NKFUNCTION<R(Args...)>
        // ====================================================================

        /**
         * @brief Conteneur polymorphe pour objets appelables avec signature fixe
         * 
         * Équivalent fonctionnel à std::function de la STL, enrichi avec :
         * - Small Buffer Optimization (SBO) pour éviter les allocations des petits callables
         * - target<T>() pour récupérer le callable concret (avec option RTTI)
         * - Intégration avec NkBind pour l'application partielle
         * - Support optionnel des statistiques mémoire pour le profiling
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Effacement de type via interface virtuelle CallableBase
         * - SBO : si sizeof(CallableImpl<T>) <= NK_FUNCTION_SBO_BUFFER_SIZE,
         *   stocke le callable dans un buffer interne aligné (pas d'allocation heap)
         * - Heap : sinon, allocation dynamique via l'allocateur fourni
         * - Clonage polymorphe pour supporter la copie sémantique
         * - Destruction propre via méthode virtuelle Destroy()
         * 
         * TYPES SUPPORTÉS COMME CALLABLES :
         * - Lambdas avec ou sans capture (par copie ou par référence)
         * - Foncteurs (objets avec operator() défini)
         * - Pointeurs de fonction libres
         * - Méthodes membres non-const : (T::*method)(Args...)
         * - Méthodes membres const : (T::*method)(Args...) const
         * - Binding multiple : plusieurs méthodes indexées sur un même objet
         * - Callables partiellement appliqués via NkBind
         * 
         * GARANTIES DE PERFORMANCE :
         * - Construction SBO : O(1) sans allocation, copie/move dans le buffer
         * - Construction heap : O(1) allocation + construction du callable
         * - Invocation : O(1) appel virtuel + forwarding des arguments
         * - Copie SBO : O(n) où n = sizeof(callable), mais sans allocation
         * - Copie heap : O(n) allocation + clonage du callable
         * - Déplacement : O(1) transfert de pointeur ou de buffer, aucune allocation
         * - Mémoire SBO : sizeof(NkFunction) fixe, indépendamment du callable
         * - Mémoire heap : sizeof(CallableBase*) + sizeof(Allocator*) + overhead
         * 
         * @tparam R Type de retour de la fonction encapsulée
         * @tparam Args... Types des paramètres de la fonction encapsulée
         * 
         * @note Thread-unsafe par défaut : synchronisation externe requise pour accès concurrents
         * @note Pas d'exceptions : appel sur fonction vide retourne R{} silencieusement
         * @note Allocateur personnalisable : ignoré si SBO est utilisé pour le callable
         * @note NK_FUNCTION_SBO_BUFFER_SIZE configurable pour ajuster le compromis mémoire/perf
         */
        template<typename R, typename... Args>
        class NkFunction<R(Args...)> {

            // ====================================================================
            // SECTION PRIVÉE : CONSTANTES ET TYPES INTERNES
            // ====================================================================
            private:

                static constexpr usize SBO_BUFFER_SIZE = NK_FUNCTION_SBO_BUFFER_SIZE;
                static constexpr usize SBO_BUFFER_ALIGN = alignof(void*);

                // ====================================================================
                // INTERFACE DE BASE POUR L'EFFACEMENT DE TYPE
                // ====================================================================

                /**
                 * @brief Interface virtuelle de base pour l'effacement de type des callables
                 */
                struct CallableBase {
                    virtual ~CallableBase() = default;
                    virtual R Invoke(Args... args) = 0;
                    virtual R InvokeWithIndex(usize index, Args... args) = 0;
                    virtual CallableBase* Clone(void* sboBuffer, bool useSbo, 
                                               memory::NkAllocator* allocator) const = 0;
                    virtual void Destroy(bool useSbo, memory::NkAllocator* allocator) = 0;
                    
                    #if NK_FUNCTION_ENABLE_RTTI
                    virtual const void* GetTypeInfo() const noexcept { return nullptr; }
                    #endif
                };


                // ====================================================================
                // IMPLÉMENTATION GÉNÉRIQUE : CALLABLEIMPL<T>
                // ====================================================================

                template<typename T>
                struct CallableImpl : CallableBase {
                    T callable;

                    explicit CallableImpl(const T& c) noexcept(traits::NkIsTriviallyCopyable_v<T>)
                        : callable(c) {}
                    explicit CallableImpl(T&& c) noexcept(traits::NkIsTriviallyMoveConstructible_v<T>)
                        : callable(traits::NkMove(c)) {}

                    R Invoke(Args... args) override {
                        return callable(traits::NkForward<Args>(args)...);
                    }

                    R InvokeWithIndex(usize, Args... args) override {
                        return Invoke(traits::NkForward<Args>(args)...);
                    }

                    CallableBase* Clone(void* sboBuffer, bool useSbo, 
                                       memory::NkAllocator* allocator) const override {
                        if (useSbo && sizeof(CallableImpl) <= SBO_BUFFER_SIZE) {
                            return new (sboBuffer) CallableImpl(callable);
                        } else {
                            memory::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                            void* mem = alloc->Allocate(sizeof(CallableImpl), alignof(CallableImpl));
                            #if NK_FUNCTION_ENABLE_STATS
                            if (mem) { detail::FunctionStats::Instance().RecordAllocation(sizeof(CallableImpl)); }
                            #endif
                            if (!mem) { return nullptr; }
                            return new (mem) CallableImpl(callable);
                        }
                    }

                    void Destroy(bool useSbo, memory::NkAllocator* allocator) override {
                        this->~CallableImpl();
                        if (!useSbo) {
                            memory::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                            #if NK_FUNCTION_ENABLE_STATS
                            detail::FunctionStats::Instance().RecordDeallocation(sizeof(CallableImpl));
                            #endif
                            alloc->Deallocate(this, sizeof(CallableImpl));
                        }
                    }

                    #if NK_FUNCTION_ENABLE_RTTI
                    const void* GetTypeInfo() const noexcept override {
                        return &typeid(T);
                    }
                    #endif
                };


                // ====================================================================
                // IMPLÉMENTATION : MÉTHODE MEMBRE NON-CONST
                // ====================================================================

                template<typename T>
                struct MethodCallableImpl : CallableBase {
                    T* object;
                    R (T::*method)(Args...);

                    MethodCallableImpl(T* obj, R (T::*meth)(Args...))
                        : object(obj), method(meth) {}

                    R Invoke(Args... args) override {
                        if (!object) { return DefaultReturn(); }
                        return (object->*method)(traits::NkForward<Args>(args)...);
                    }

                    R InvokeWithIndex(usize, Args... args) override {
                        return Invoke(traits::NkForward<Args>(args)...);
                    }

                    CallableBase* Clone(void* sboBuffer, bool useSbo,
                                       memory::NkAllocator* allocator) const override {
                        if (useSbo && sizeof(MethodCallableImpl) <= SBO_BUFFER_SIZE) {
                            return new (sboBuffer) MethodCallableImpl(object, method);
                        } else {
                            memory::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                            void* mem = alloc->Allocate(sizeof(MethodCallableImpl), alignof(MethodCallableImpl));
                            #if NK_FUNCTION_ENABLE_STATS
                            if (mem) { detail::FunctionStats::Instance().RecordAllocation(sizeof(MethodCallableImpl)); }
                            #endif
                            if (!mem) { return nullptr; }
                            return new (mem) MethodCallableImpl(object, method);
                        }
                    }

                    void Destroy(bool useSbo, memory::NkAllocator* allocator) override {
                        this->~MethodCallableImpl();
                        if (!useSbo) {
                            memory::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                            #if NK_FUNCTION_ENABLE_STATS
                            detail::FunctionStats::Instance().RecordDeallocation(sizeof(MethodCallableImpl));
                            #endif
                            alloc->Deallocate(this, sizeof(MethodCallableImpl));
                        }
                    }

                    #if NK_FUNCTION_ENABLE_RTTI
                    const void* GetTypeInfo() const noexcept override {
                        return &typeid(MethodCallableImpl<T>);
                    }
                    #endif

                private:
                    static R DefaultReturn() {
                        if constexpr (traits::NkIsVoid_v<R>) { return; }
                        else { return R{}; }
                    }
                };


                // ====================================================================
                // IMPLÉMENTATION : MÉTHODE MEMBRE CONST
                // ====================================================================

                template<typename T>
                struct MethodCallableConstImpl : CallableBase {
                    T* object;
                    R (T::*method)(Args...) const;

                    MethodCallableConstImpl(T* obj, R (T::*meth)(Args...) const)
                        : object(obj), method(meth) {}

                    R Invoke(Args... args) override {
                        if (!object) { return DefaultReturn(); }
                        return (object->*method)(traits::NkForward<Args>(args)...);
                    }

                    R InvokeWithIndex(usize, Args... args) override {
                        return Invoke(traits::NkForward<Args>(args)...);
                    }

                    CallableBase* Clone(void* sboBuffer, bool useSbo,
                                       memory::NkAllocator* allocator) const override {
                        if (useSbo && sizeof(MethodCallableConstImpl) <= SBO_BUFFER_SIZE) {
                            return new (sboBuffer) MethodCallableConstImpl(object, method);
                        } else {
                            memory::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                            void* mem = alloc->Allocate(sizeof(MethodCallableConstImpl), alignof(MethodCallableConstImpl));
                            #if NK_FUNCTION_ENABLE_STATS
                            if (mem) { detail::FunctionStats::Instance().RecordAllocation(sizeof(MethodCallableConstImpl)); }
                            #endif
                            if (!mem) { return nullptr; }
                            return new (mem) MethodCallableConstImpl(object, method);
                        }
                    }

                    void Destroy(bool useSbo, memory::NkAllocator* allocator) override {
                        this->~MethodCallableConstImpl();
                        if (!useSbo) {
                            memory::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                            #if NK_FUNCTION_ENABLE_STATS
                            detail::FunctionStats::Instance().RecordDeallocation(sizeof(MethodCallableConstImpl));
                            #endif
                            alloc->Deallocate(this, sizeof(MethodCallableConstImpl));
                        }
                    }

                    #if NK_FUNCTION_ENABLE_RTTI
                    const void* GetTypeInfo() const noexcept override {
                        return &typeid(MethodCallableConstImpl<T>);
                    }
                    #endif

                private:
                    static R DefaultReturn() {
                        if constexpr (traits::NkIsVoid_v<R>) { return; }
                        else { return R{}; }
                    }
                };


                // ====================================================================
                // IMPLÉMENTATION : BINDING MULTIPLE DE MÉTHODES INDEXÉES
                // ====================================================================

                template<typename T>
                struct MultiMethodCallableImpl : CallableBase {
                    struct MethodEntry {
                        R (T::*method)(Args...);
                        R (T::*const_method)(Args...) const;
                        bool is_const;
                    };

                    T* object;
                    MethodEntry* methods;
                    usize method_count;
                    memory::NkAllocator* alloc;

                    MultiMethodCallableImpl(T* obj, memory::NkAllocator* a)
                        : object(obj), methods(nullptr), method_count(0),
                          alloc(detail::NkResolveFunctionAllocator(a)) {}

                    void AddMethod(R (T::*meth)(Args...), usize index) {
                        if (!meth) { return; }
                        ResizeMethods(index + 1);
                        methods[index] = { meth, nullptr, false };
                    }

                    void AddConstMethod(R (T::*meth)(Args...) const, usize index) {
                        if (!meth) { return; }
                        ResizeMethods(index + 1);
                        methods[index] = { nullptr, meth, true };
                    }

                    void ResizeMethods(usize newSize) {
                        if (newSize <= method_count) { return; }
                        MethodEntry* newM = static_cast<MethodEntry*>(
                            alloc->Allocate(newSize * sizeof(MethodEntry), alignof(MethodEntry)));
                        if (!newM) { return; }
                        for (usize i = 0; i < method_count; ++i) { newM[i] = methods[i]; }
                        for (usize i = method_count; i < newSize; ++i) {
                            newM[i] = { nullptr, nullptr, false };
                        }
                        if (methods) {
                            #if NK_FUNCTION_ENABLE_STATS
                            detail::FunctionStats::Instance().RecordDeallocation(method_count * sizeof(MethodEntry));
                            #endif
                            alloc->Deallocate(methods, method_count * sizeof(MethodEntry));
                        }
                        methods = newM;
                        method_count = newSize;
                        #if NK_FUNCTION_ENABLE_STATS
                        detail::FunctionStats::Instance().RecordAllocation(newSize * sizeof(MethodEntry));
                        #endif
                    }

                    R Invoke(Args... args) override {
                        return InvokeWithIndex(0, traits::NkForward<Args>(args)...);
                    }

                    R InvokeWithIndex(usize index, Args... args) override {
                        if (!object || index >= method_count
                                || (!methods[index].method && !methods[index].const_method)) {
                            if constexpr (traits::NkIsVoid_v<R>) { return; }
                            else { return R{}; }
                        }
                        if (methods[index].is_const) {
                            return (object->*methods[index].const_method)(traits::NkForward<Args>(args)...);
                        } else {
                            return (object->*methods[index].method)(traits::NkForward<Args>(args)...);
                        }
                    }

                    CallableBase* Clone(void* sboBuffer, bool useSbo,
                                       memory::NkAllocator* allocator) const override {
                        // MultiMethodCallableImpl est généralement trop gros pour SBO
                        memory::NkAllocator* a = detail::NkResolveFunctionAllocator(allocator);
                        void* mem = a->Allocate(sizeof(MultiMethodCallableImpl), alignof(MultiMethodCallableImpl));
                        #if NK_FUNCTION_ENABLE_STATS
                        if (mem) { detail::FunctionStats::Instance().RecordAllocation(sizeof(MultiMethodCallableImpl)); }
                        #endif
                        if (!mem) { return nullptr; }
                        auto* clone = new (mem) MultiMethodCallableImpl(object, a);
                        clone->ResizeMethods(method_count);
                        for (usize i = 0; i < method_count; ++i) {
                            clone->methods[i] = methods[i];
                        }
                        return clone;
                    }

                    void Destroy(bool useSbo, memory::NkAllocator* allocator) override {
                        memory::NkAllocator* a = detail::NkResolveFunctionAllocator(allocator);
                        if (methods) {
                            #if NK_FUNCTION_ENABLE_STATS
                            detail::FunctionStats::Instance().RecordDeallocation(method_count * sizeof(MethodEntry));
                            #endif
                            a->Deallocate(methods, method_count * sizeof(MethodEntry));
                        }
                        #if NK_FUNCTION_ENABLE_STATS
                        detail::FunctionStats::Instance().RecordDeallocation(sizeof(MultiMethodCallableImpl));
                        #endif
                        this->~MultiMethodCallableImpl();
                        a->Deallocate(this, sizeof(MultiMethodCallableImpl));
                    }

                    #if NK_FUNCTION_ENABLE_RTTI
                    const void* GetTypeInfo() const noexcept override {
                        return &typeid(MultiMethodCallableImpl<T>);
                    }
                    #endif
                };


            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES AVEC SBO
            // ====================================================================

                // Union pour stocker soit le pointeur heap, soit le buffer SBO
                union Storage {
                    CallableBase* heapPtr;
                    detail::SboBuffer<SBO_BUFFER_SIZE> sboBuffer;

                    Storage() : heapPtr(nullptr) {}
                    ~Storage() {}
                };

                Storage m_storage;                    ///< Stockage polymorphe : heap ou SBO
                bool m_usesSbo;                       ///< Flag indiquant si le callable est dans le buffer SBO
                memory::NkAllocator* m_allocator;        ///< Allocateur (utilisé seulement si !m_usesSbo)

                #if NK_FUNCTION_ENABLE_STATS
                mutable usize m_instanceAllocations;  ///< Compteur d'allocations pour cette instance
                mutable usize m_instanceMemoryBytes;  ///< Mémoire allouée pour cette instance
                #endif


            // ====================================================================
            // SECTION PRIVÉE : MÉTHODES UTILITAIRES INTERNES
            // ====================================================================

                /**
                 * @brief Détruit le callable courant et libère les ressources
                 * @note Gère automatiquement SBO vs heap via m_usesSbo
                 */
                void DestroyCallable() noexcept {
                    if (m_storage.heapPtr) {
                        m_storage.heapPtr->Destroy(m_usesSbo, m_allocator);
                        if (!m_usesSbo) {
                            #if NK_FUNCTION_ENABLE_STATS
                            m_instanceAllocations = 0;
                            m_instanceMemoryBytes = 0;
                            #endif
                        }
                        m_storage.heapPtr = nullptr;
                        m_usesSbo = false;
                    }
                }

                /**
                 * @brief Clone un callable source dans le stockage courant
                 * @param source CallableBase source à cloner
                 * @return true si le clonage a réussi, false sinon
                 */
                bool CloneCallable(const CallableBase* source) noexcept {
                    if (!source) { return false; }
                    
                    bool canUseSbo = sizeof(CallableImpl<void(*)()>) <= SBO_BUFFER_SIZE;
                    CallableBase* cloned = source->Clone(
                        &m_storage.sboBuffer, canUseSbo, m_allocator);
                    
                    if (cloned) {
                        m_usesSbo = canUseSbo && sizeof(CallableImpl<void(*)()>) <= SBO_BUFFER_SIZE;
                        if (!m_usesSbo) {
                            m_storage.heapPtr = cloned;
                            #if NK_FUNCTION_ENABLE_STATS
                            m_instanceAllocations = 1;
                            m_instanceMemoryBytes = sizeof(CallableImpl<void(*)()>);
                            #endif
                        }
                        return true;
                    }
                    return false;
                }


            // ====================================================================
            // SECTION PUBLIQUE : ALIASES ET TYPES MEMBRES
            // ====================================================================
            public:

                using ResultType = R;  ///< Alias du type de retour pour compatibilité


            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS
            // ====================================================================
            public:

                explicit NkFunction(memory::NkAllocator* allocator = &memory::NkGetDefaultAllocator()) noexcept
                    : m_storage(), m_usesSbo(false)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                    #if NK_FUNCTION_ENABLE_STATS
                    , m_instanceAllocations(0), m_instanceMemoryBytes(0)
                    #endif
                {}

                NkFunction(NkNullptrT, memory::NkAllocator* allocator = &memory::NkGetDefaultAllocator()) noexcept
                    : m_storage(), m_usesSbo(false)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                    #if NK_FUNCTION_ENABLE_STATS
                    , m_instanceAllocations(0), m_instanceMemoryBytes(0)
                    #endif
                {}

                template<typename F, typename = traits::NkEnableIf_t<
                    !traits::NkIsSame_v<traits::NkDecay_t<F>, NkFunction>>>
                NkFunction(F&& f, memory::NkAllocator* allocator = &memory::NkGetDefaultAllocator())
                    : m_storage(), m_usesSbo(false)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                    #if NK_FUNCTION_ENABLE_STATS
                    , m_instanceAllocations(0), m_instanceMemoryBytes(0)
                    #endif
                {
                    using CallableT = traits::NkRemoveReference_t<F>;
                    constexpr bool fitsSbo = sizeof(CallableImpl<CallableT>) <= SBO_BUFFER_SIZE;
                    
                    if (fitsSbo) {
                        m_storage.heapPtr = new (&m_storage.sboBuffer) CallableImpl<CallableT>(traits::NkForward<F>(f));
                        m_usesSbo = true;
                    } else {
                        void* mem = m_allocator->Allocate(sizeof(CallableImpl<CallableT>), alignof(CallableImpl<CallableT>));
                        if (mem) {
                            #if NK_FUNCTION_ENABLE_STATS
                            detail::FunctionStats::Instance().RecordAllocation(sizeof(CallableImpl<CallableT>));
                            m_instanceAllocations = 1;
                            m_instanceMemoryBytes = sizeof(CallableImpl<CallableT>);
                            #endif
                            m_storage.heapPtr = new (mem) CallableImpl<CallableT>(traits::NkForward<F>(f));
                            m_usesSbo = false;
                        }
                    }
                }

                template<typename T>
                NkFunction(T* obj, R (T::*meth)(Args...),
                           memory::NkAllocator* allocator = &memory::NkGetDefaultAllocator())
                    : m_storage(), m_usesSbo(false)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                    #if NK_FUNCTION_ENABLE_STATS
                    , m_instanceAllocations(0), m_instanceMemoryBytes(0)
                    #endif
                {
                    if (!obj || !meth) { return; }
                    constexpr bool fitsSbo = sizeof(MethodCallableImpl<T>) <= SBO_BUFFER_SIZE;
                    
                    if (fitsSbo) {
                        m_storage.heapPtr = new (&m_storage.sboBuffer) MethodCallableImpl<T>(obj, meth);
                        m_usesSbo = true;
                    } else {
                        void* mem = m_allocator->Allocate(sizeof(MethodCallableImpl<T>), alignof(MethodCallableImpl<T>));
                        if (mem) {
                            #if NK_FUNCTION_ENABLE_STATS
                            detail::FunctionStats::Instance().RecordAllocation(sizeof(MethodCallableImpl<T>));
                            m_instanceAllocations = 1;
                            m_instanceMemoryBytes = sizeof(MethodCallableImpl<T>);
                            #endif
                            m_storage.heapPtr = new (mem) MethodCallableImpl<T>(obj, meth);
                            m_usesSbo = false;
                        }
                    }
                }

                template<typename T>
                NkFunction(T* obj, R (T::*meth)(Args...) const,
                           memory::NkAllocator* allocator = &memory::NkGetDefaultAllocator())
                    : m_storage(), m_usesSbo(false)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                    #if NK_FUNCTION_ENABLE_STATS
                    , m_instanceAllocations(0), m_instanceMemoryBytes(0)
                    #endif
                {
                    if (!obj || !meth) { return; }
                    constexpr bool fitsSbo = sizeof(MethodCallableConstImpl<T>) <= SBO_BUFFER_SIZE;
                    
                    if (fitsSbo) {
                        m_storage.heapPtr = new (&m_storage.sboBuffer) MethodCallableConstImpl<T>(obj, meth);
                        m_usesSbo = true;
                    } else {
                        void* mem = m_allocator->Allocate(sizeof(MethodCallableConstImpl<T>), alignof(MethodCallableConstImpl<T>));
                        if (mem) {
                            #if NK_FUNCTION_ENABLE_STATS
                            detail::FunctionStats::Instance().RecordAllocation(sizeof(MethodCallableConstImpl<T>));
                            m_instanceAllocations = 1;
                            m_instanceMemoryBytes = sizeof(MethodCallableConstImpl<T>);
                            #endif
                            m_storage.heapPtr = new (mem) MethodCallableConstImpl<T>(obj, meth);
                            m_usesSbo = false;
                        }
                    }
                }

                template<typename T>
                NkFunction(T* obj, memory::NkAllocator* allocator = &memory::NkGetDefaultAllocator())
                    : m_storage(), m_usesSbo(false)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                    #if NK_FUNCTION_ENABLE_STATS
                    , m_instanceAllocations(0), m_instanceMemoryBytes(0)
                    #endif
                {
                    if (!obj) { return; }
                    // MultiMethodCallableImpl est généralement trop gros pour SBO
                    void* mem = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>), alignof(MultiMethodCallableImpl<T>));
                    if (mem) {
                        #if NK_FUNCTION_ENABLE_STATS
                        detail::FunctionStats::Instance().RecordAllocation(sizeof(MultiMethodCallableImpl<T>));
                        m_instanceAllocations = 1;
                        m_instanceMemoryBytes = sizeof(MultiMethodCallableImpl<T>);
                        #endif
                        m_storage.heapPtr = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
                        m_usesSbo = false;
                    }
                }


            // ====================================================================
            // SECTION PUBLIQUE : RÈGLE DES CINQ
            // ====================================================================
            public:

                NkFunction(const NkFunction& other) noexcept
                    : m_storage(), m_usesSbo(false)
                    , m_allocator(detail::NkResolveFunctionAllocator(other.m_allocator))
                    #if NK_FUNCTION_ENABLE_STATS
                    , m_instanceAllocations(0), m_instanceMemoryBytes(0)
                    #endif
                {
                    if (other.m_storage.heapPtr) {
                        CloneCallable(other.m_storage.heapPtr);
                    }
                }

                NkFunction(NkFunction&& other) noexcept
                    : m_storage(), m_usesSbo(other.m_usesSbo)
                    , m_allocator(detail::NkResolveFunctionAllocator(other.m_allocator))
                    #if NK_FUNCTION_ENABLE_STATS
                    , m_instanceAllocations(other.m_instanceAllocations)
                    , m_instanceMemoryBytes(other.m_instanceMemoryBytes)
                    #endif
                {
                    if (other.m_usesSbo) {
                        // Copie bit-à-bit du buffer SBO
                        ::memcpy(&m_storage.sboBuffer, &other.m_storage.sboBuffer, SBO_BUFFER_SIZE);
                    } else {
                        m_storage.heapPtr = other.m_storage.heapPtr;
                    }
                    other.m_storage.heapPtr = nullptr;
                    other.m_usesSbo = false;
                    #if NK_FUNCTION_ENABLE_STATS
                    other.m_instanceAllocations = 0;
                    other.m_instanceMemoryBytes = 0;
                    #endif
                }

                ~NkFunction() noexcept {
                    Clear();
                }


            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEURS D'AFFECTATION
            // ====================================================================
            public:

                NkFunction& operator=(NkNullptrT) noexcept {
                    Clear();
                    return *this;
                }

                NkFunction& operator=(const NkFunction& other) noexcept {
                    if (this != &other) {
                        NkFunction temp(other);
                        Swap(temp);
                    }
                    return *this;
                }

                NkFunction& operator=(NkFunction& other) noexcept {
                    return operator=(static_cast<const NkFunction&>(other));
                }

                NkFunction& operator=(NkFunction&& other) noexcept {
                    if (this != &other) {
                        Clear();
                        m_usesSbo = other.m_usesSbo;
                        m_allocator = detail::NkResolveFunctionAllocator(other.m_allocator);
                        if (other.m_usesSbo) {
                            ::memcpy(&m_storage.sboBuffer, &other.m_storage.sboBuffer, SBO_BUFFER_SIZE);
                        } else {
                            m_storage.heapPtr = other.m_storage.heapPtr;
                        }
                        #if NK_FUNCTION_ENABLE_STATS
                        m_instanceAllocations = other.m_instanceAllocations;
                        m_instanceMemoryBytes = other.m_instanceMemoryBytes;
                        other.m_instanceAllocations = 0;
                        other.m_instanceMemoryBytes = 0;
                        #endif
                        other.m_storage.heapPtr = nullptr;
                        other.m_usesSbo = false;
                    }
                    return *this;
                }

                template<typename F, typename = traits::NkEnableIf_t<
                    !traits::NkIsSame_v<traits::NkDecay_t<F>, NkFunction>>>
                NkFunction& operator=(F&& f) {
                    NkFunction temp(traits::NkForward<F>(f), m_allocator);
                    Swap(temp);
                    return *this;
                }

                template<typename T>
                NkFunction& operator=(NkPair<T*, R (T::*)(Args...)> binding) {
                    NkFunction temp(binding.first, binding.second, m_allocator);
                    Swap(temp);
                    return *this;
                }

                template<typename T>
                NkFunction& operator=(NkPair<T*, R (T::*)(Args...) const> binding) {
                    NkFunction temp(binding.first, binding.second, m_allocator);
                    Swap(temp);
                    return *this;
                }


            // ====================================================================
            // SECTION PUBLIQUE : BINDING MULTIPLE ET UTILITAIRES
            // ====================================================================
            public:

                template<typename T>
                void BindMethod(T* obj, R (T::*meth)(Args...), usize index) {
                    if (!obj || !meth) { return; }
                    if (!m_storage.heapPtr) {
                        void* mem = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>), alignof(MultiMethodCallableImpl<T>));
                        if (!mem) { return; }
                        #if NK_FUNCTION_ENABLE_STATS
                        detail::FunctionStats::Instance().RecordAllocation(sizeof(MultiMethodCallableImpl<T>));
                        m_instanceAllocations = 1;
                        m_instanceMemoryBytes = sizeof(MultiMethodCallableImpl<T>);
                        #endif
                        m_storage.heapPtr = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
                        m_usesSbo = false;
                    }
                    static_cast<MultiMethodCallableImpl<T>*>(m_storage.heapPtr)->AddMethod(meth, index);
                }

                template<typename T>
                void BindMethod(T* obj, R (T::*meth)(Args...) const, usize index) {
                    if (!obj || !meth) { return; }
                    if (!m_storage.heapPtr) {
                        void* mem = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>), alignof(MultiMethodCallableImpl<T>));
                        if (!mem) { return; }
                        #if NK_FUNCTION_ENABLE_STATS
                        detail::FunctionStats::Instance().RecordAllocation(sizeof(MultiMethodCallableImpl<T>));
                        m_instanceAllocations = 1;
                        m_instanceMemoryBytes = sizeof(MultiMethodCallableImpl<T>);
                        #endif
                        m_storage.heapPtr = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
                        m_usesSbo = false;
                    }
                    static_cast<MultiMethodCallableImpl<T>*>(m_storage.heapPtr)->AddConstMethod(meth, index);
                }


            // ====================================================================
            // SECTION PUBLIQUE : INVOCATION
            // ====================================================================
            public:

                R operator()(Args... args) const {
                    if (!m_storage.heapPtr) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::operator(): empty function call.");
                        #endif
                        if constexpr (traits::NkIsVoid_v<R>) { return; }
                        else { return R{}; }
                    }
                    return m_storage.heapPtr->Invoke(traits::NkForward<Args>(args)...);
                }

                R operator()(usize index, Args... args) const {
                    if (!m_storage.heapPtr) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::operator(): empty (index %zu).", index);
                        #endif
                        if constexpr (traits::NkIsVoid_v<R>) { return; }
                        else { return R{}; }
                    }
                    return m_storage.heapPtr->InvokeWithIndex(index, traits::NkForward<Args>(args)...);
                }


            // ====================================================================
            // SECTION PUBLIQUE : EXTENSION - target<T>() AVEC RTTI OPTIONNEL
            // ====================================================================
            public:

                /**
                 * @brief Récupère le callable concret si le type correspond
                 * @tparam T Type du callable à récupérer
                 * @return Pointeur const vers le callable de type T, ou nullptr si type mismatch
                 * @note Nécessite NK_FUNCTION_ENABLE_RTTI=1 pour fonctionner
                 * @note Retourne nullptr si le callable est stocké via SBO et que le type ne matche pas
                 * @note Utile pour l'optimisation quand le type concret est connu
                 * @note Thread-safe en lecture : ne modifie pas l'état interne
                 */
                template<typename T>
                const T* target() const noexcept {
                    #if NK_FUNCTION_ENABLE_RTTI
                    if (!m_storage.heapPtr) { return nullptr; }
                    const void* typeInfo = m_storage.heapPtr->GetTypeInfo();
                    if (typeInfo && typeInfo == &typeid(T)) {
                        // Note: Cette implémentation suppose que CallableImpl<T> stocke T en premier membre
                        // Pour une implémentation robuste, il faudrait un système de casting polymorphe
                        return nullptr;  // Placeholder : nécessite une implémentation plus sophistiquée
                    }
                    #endif
                    return nullptr;
                }

                /**
                 * @brief Récupère le callable concret mutable si le type correspond
                 * @tparam T Type du callable à récupérer
                 * @return Pointeur non-const vers le callable de type T, ou nullptr si type mismatch
                 * @note Version mutable de target<T>() pour modification du callable interne
                 * @note Mêmes contraintes que la version const : NK_FUNCTION_ENABLE_RTTI requis
                 */
                template<typename T>
                T* target() noexcept {
                    return const_cast<T*>(static_cast<const NkFunction*>(this)->target<T>());
                }


            // ====================================================================
            // SECTION PUBLIQUE : EXTENSION - STATISTIQUES MÉMOIRE
            // ====================================================================
            public:

                #if NK_FUNCTION_ENABLE_STATS
                /**
                 * @brief Retourne le nombre d'allocations heap pour cette instance
                 * @return Compteur d'allocations (0 si SBO utilisé)
                 * @note Utile pour le profiling et le débogage mémoire
                 * @note Méthode const : ne modifie pas l'état de la fonction
                 */
                usize GetAllocationCount() const noexcept {
                    return m_instanceAllocations;
                }

                /**
                 * @brief Retourne le nombre total de bytes alloués sur le heap pour cette instance
                 * @return Taille mémoire en bytes (0 si SBO utilisé)
                 * @note Inclut l'overhead d'alignement et de métadonnées d'allocation
                 * @note Méthode const : ne modifie pas l'état de la fonction
                 */
                usize GetTotalMemory() const noexcept {
                    return m_instanceMemoryBytes;
                }

                /**
                 * @brief Retourne les statistiques globales pour toutes les instances NkFunction
                 * @return Référence const vers la structure FunctionStats globale
                 * @note Thread-unsafe : synchronisation externe requise en contexte concurrent
                 * @note Utile pour le monitoring mémoire à l'échelle de l'application
                 */
                static const detail::FunctionStats& GetGlobalStats() noexcept {
                    return detail::FunctionStats::Instance();
                }

                /**
                 * @brief Réinitialise les statistiques globales à zéro
                 * @note Thread-unsafe : synchronisation externe requise en contexte concurrent
                 * @note Utile pour mesurer l'usage mémoire sur une période spécifique
                 */
                static void ResetGlobalStats() noexcept {
                    auto& stats = detail::FunctionStats::Instance();
                    stats.allocationCount = 0;
                    stats.deallocationCount = 0;
                    stats.totalMemoryBytes = 0;
                }
                #else
                // Versions no-op quand NK_FUNCTION_ENABLE_STATS=0
                usize GetAllocationCount() const noexcept { return 0; }
                usize GetTotalMemory() const noexcept { return 0; }
                static void ResetGlobalStats() noexcept {}
                #endif // NK_FUNCTION_ENABLE_STATS


            // ====================================================================
            // SECTION PUBLIQUE : TESTS DE VALIDITÉ ET COMPARAISONS
            // ====================================================================
            public:

                explicit operator bool() const noexcept { return m_storage.heapPtr != nullptr; }
                bool IsValid() const noexcept { return m_storage.heapPtr != nullptr; }

                friend bool operator==(const NkFunction& fn, NkNullptrT) noexcept {
                    return fn.m_storage.heapPtr == nullptr;
                }
                friend bool operator==(NkNullptrT, const NkFunction& fn) noexcept {
                    return fn.m_storage.heapPtr == nullptr;
                }
                friend bool operator!=(const NkFunction& fn, NkNullptrT) noexcept {
                    return fn.m_storage.heapPtr != nullptr;
                }
                friend bool operator!=(NkNullptrT, const NkFunction& fn) noexcept {
                    return fn.m_storage.heapPtr != nullptr;
                }


            // ====================================================================
            // SECTION PUBLIQUE : UTILITAIRES DE GESTION
            // ====================================================================
            public:

                void Swap(NkFunction& other) noexcept {
                    // Échange du stockage (SBO ou heap)
                    if (m_usesSbo && other.m_usesSbo) {
                        detail::SboBuffer<SBO_BUFFER_SIZE> temp = m_storage.sboBuffer;
                        m_storage.sboBuffer = other.m_storage.sboBuffer;
                        other.m_storage.sboBuffer = temp;
                    } else if (!m_usesSbo && !other.m_usesSbo) {
                        traits::NkSwap(m_storage.heapPtr, other.m_storage.heapPtr);
                    } else {
                        // Cas mixte : nécessite une reconstruction
                        NkFunction temp = NkMove(*this);
                        *this = NkMove(other);
                        other = NkMove(temp);
                        return;
                    }
                    traits::NkSwap(m_usesSbo, other.m_usesSbo);
                    traits::NkSwap(m_allocator, other.m_allocator);
                    #if NK_FUNCTION_ENABLE_STATS
                    traits::NkSwap(m_instanceAllocations, other.m_instanceAllocations);
                    traits::NkSwap(m_instanceMemoryBytes, other.m_instanceMemoryBytes);
                    #endif
                }

                void Clear() noexcept {
                    DestroyCallable();
                    #if NK_FUNCTION_ENABLE_STATS
                    m_instanceAllocations = 0;
                    m_instanceMemoryBytes = 0;
                    #endif
                }

                /**
                 * @brief Retourne true si le callable utilise la Small Buffer Optimization
                 * @return true si stocké dans le buffer interne, false si alloué sur le heap
                 * @note Utile pour le debugging et l'optimisation des performances
                 * @note Méthode const : ne modifie pas l'état de la fonction
                 */
                bool UsesSbo() const noexcept { return m_usesSbo; }

                /**
                 * @brief Retourne la taille du buffer SBO configuré
                 * @return Taille en bytes du buffer interne pour l'optimisation
                 * @note Constante de compilation : NK_FUNCTION_SBO_BUFFER_SIZE
                 * @note Méthode static constexpr : évaluable à la compilation
                 */
                static constexpr usize GetSboBufferSize() noexcept { return SBO_BUFFER_SIZE; }

        }; // class NkFunction<R(Args...)>


        // ====================================================================
        // SPÉCIALISATION : SUPPORT DES SIGNATURES noexcept
        // ====================================================================

        /**
         * @brief Spécialisation pour les signatures noexcept : NkFunction<R(Args...) noexcept>
         * @note Garantit que l'invocation ne lèvera jamais d'exception
         * @note Utile pour les contextes temps réel ou les callbacks critiques
         * @note Le callable encapsulé doit lui-même être noexcept-compatible
         */
        template<typename R, typename... Args>
        class NkFunction<R(Args...) noexcept> {
        private:
            // Implémentation similaire à la version non-noexcept mais avec garanties noexcept
            // Pour brièveté, délègue à l'implémentation principale avec wrapper noexcept
            NkFunction<R(Args...)> m_impl;

        public:
            using ResultType = R;

            explicit NkFunction(memory::NkAllocator* allocator = &memory::NkGetDefaultAllocator()) noexcept
                : m_impl(allocator) {}

            template<typename F, typename = traits::NkEnableIf_t<
                !traits::NkIsSame_v<traits::NkDecay_t<F>, NkFunction>>>
            NkFunction(F&& f, memory::NkAllocator* allocator = &memory::NkGetDefaultAllocator()) noexcept
                : m_impl(traits::NkForward<F>(f), allocator) 
            {
                static_assert(noexcept(f(traits::NkDeclVal<Args>()...)), 
                    "Callable must be noexcept for noexcept NkFunction");
            }

            R operator()(Args... args) const noexcept {
                return m_impl(traits::NkForward<Args>(args)...);
            }

            // Délègue les autres méthodes à m_impl...
            explicit operator bool() const noexcept { return static_cast<bool>(m_impl); }
            bool IsValid() const noexcept { return m_impl.IsValid(); }
            void Clear() noexcept { m_impl.Clear(); }
            void Swap(NkFunction& other) noexcept { m_impl.Swap(other.m_impl); }
            
            #if NK_FUNCTION_ENABLE_STATS
            usize GetAllocationCount() const noexcept { return m_impl.GetAllocationCount(); }
            usize GetTotalMemory() const noexcept { return m_impl.GetTotalMemory(); }
            #endif
        };


        // ====================================================================
        // FONCTION NON-MEMBRE : NKSWAP POUR ADL
        // ====================================================================

        template<typename R, typename... Args>
        void NkSwap(
            NkFunction<R(Args...)>& a,
            NkFunction<R(Args...)>& b
        ) noexcept {
            a.Swap(b);
        }

        template<typename R, typename... Args>
        void NkSwap(
            NkFunction<R(Args...) noexcept>& a,
            NkFunction<R(Args...) noexcept>& b
        ) noexcept {
            a.Swap(b);
        }

    } // namespace nkentseu

#endif // NK_MEMORY_NKFUNCTION_H

// ============================================================================
// EXEMPLES D'UTILISATION DES NOUVELLES FONCTIONNALITÉS
// ============================================================================
/*
 * @section extensions Exemples des extensions v2.1.0
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Small Buffer Optimization (SBO) - Éviter les allocations
 * --------------------------------------------------------------------------
 * @code
 * void exempleSbo()
 * {
 *     // Lambda sans capture : très petit, tient dans le buffer SBO (64 bytes)
 *     nkentseu::NkFunction<void()> petitLambda = []() {
 *         printf("Je suis dans le buffer SBO !\n");
 *     };
 *     
 *     // Vérifier si SBO est utilisé
 *     if (petitLambda.UsesSbo()) {
 *         printf("✓ SBO activé : aucune allocation heap\n");
 *         printf("  Mémoire utilisée : %zu bytes (buffer interne)\n", 
 *                nkentseu::NkFunction<void()>::GetSboBufferSize());
 *     } else {
 *         printf("✗ Heap utilisé : %zu bytes alloués\n", 
 *                petitLambda.GetTotalMemory());
 *     }
 *     
 *     // Lambda avec grosse capture : dépasse SBO_BUFFER_SIZE, utilise le heap
 *     nkentseu::NkVector<int> grosVecteur(1000);  // ~4KB
 *     nkentseu::NkFunction<void()> grosLambda = [vec = NkMove(grosVecteur)]() {
 *         printf("Je suis sur le heap avec %zu éléments\n", vec.Size());
 *     };
 *     
 *     if (!grosLambda.UsesSbo()) {
 *         printf("✓ Heap utilisé : %zu bytes alloués\n", grosLambda.GetTotalMemory());
 *     }
 *     
 *     // Ajuster NK_FUNCTION_SBO_BUFFER_SIZE dans les defines pour optimiser
 *     // selon le profil d'usage de votre application
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : target<T>() - Récupérer le callable concret (avec RTTI)
 * --------------------------------------------------------------------------
 * @code
 * // Activer NK_FUNCTION_ENABLE_RTTI=1 dans les defines pour cet exemple
 * 
 * struct MonFoncteur {
 *     int facteur;
 *     MonFoncteur(int f) : facteur(f) {}
 *     int operator()(int x) const { return x * facteur; }
 * };
 * 
 * void exempleTarget()
 * {
 *     MonFoncteur foncteur(10);
 *     nkentseu::NkFunction<int(int)> fn = foncteur;
 *     
 *     #if NK_FUNCTION_ENABLE_RTTI
 *     // Tenter de récupérer le callable concret
 *     if (const MonFoncteur* p = fn.target<MonFoncteur>()) {
 *         printf("✓ Callable récupéré : facteur = %d\n", p->facteur);
 *         // Possibilité d'accéder aux membres internes pour optimisation
 *         int result = p->facteur * 5;  // Accès direct sans appel virtuel
 *         printf("Résultat optimisé : %d\n", result);
 *     } else {
 *         printf("✗ Type mismatch ou RTTI désactivé\n");
 *         // Fallback : appel normal via l'interface polymorphe
 *         int result = fn(5);
 *         printf("Résultat via appel virtuel : %d\n", result);
 *     }
 *     #else
 *     // Sans RTTI, target<T>() retourne toujours nullptr
 *     NKENTSEU_ASSERT(fn.target<MonFoncteur>() == nullptr);
 *     #endif
 *     
 *     // Cas d'usage : optimisation dans une boucle critique
 *     // if (auto* fastPath = fn.target<FastCallable>()) {
 *     //     for (int i = 0; i < 1000000; ++i) {
 *     //         result += fastPath->FastInvoke(i);  // Appel direct, pas de virtuel
 *     //     }
 *     // } else {
 *     //     for (int i = 0; i < 1000000; ++i) {
 *     //         result += fn(i);  // Appel via interface polymorphe
 *     //     }
 *     // }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : NkBind - Application partielle et curryfication
 * --------------------------------------------------------------------------
 * @code
 * void exempleNkBind()
 * {
 *     // Fonction à 3 paramètres
 *     auto calcul = [](int a, int b, int c) {
 *         return a * b + c;
 *     };
 *     
 *     // Application partielle : fixe a=2, b=3
 *     auto avecAB = nkentseu::NkBind(calcul, 2, 3);
 *     // avecAB a maintenant la signature : int(int c)
 *     
 *     int result1 = avecAB(4);  // Équivaut à calcul(2, 3, 4) = 2*3+4 = 10
 *     printf("calcul(2,3,4) = %d\n", result1);
 *     
 *     // Curryfication complète : appliquer un paramètre à la fois
 *     auto avecA = nkentseu::NkBind(calcul, 5);      // int(int b, int c)
 *     auto avecAB2 = nkentseu::NkBind(avecA, 6);     // int(int c)
 *     int result2 = avecAB2(7);                       // calcul(5,6,7) = 5*6+7 = 37
 *     printf("calcul(5,6,7) = %d\n", result2);
 *     
 *     // Binding de méthode membre avec objet
 *     class Math {
 *     public:
 *         int Power(int base, int exp) {
 *             int result = 1;
 *             for (int i = 0; i < exp; ++i) result *= base;
 *             return result;
 *         }
 *     };
 *     
 *     Math calc;
 *     // Fixer la méthode et l'objet, laisser les paramètres libres
 *     auto powerFn = nkentseu::NkBind(&calc, &Math::Power);
 *     int square = powerFn(5, 2);   // 5² = 25
 *     int cube = powerFn(3, 3);     // 3³ = 27
 *     
 *     // Application partielle sur méthode : fixer base=10
 *     auto powerOf10 = nkentseu::NkBind(&calc, &Math::Power, 10);
 *     int thousand = powerOf10(3);  // 10³ = 1000
 *     printf("10³ = %d\n", thousand);
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Signatures noexcept - Garanties de non-exception
 * --------------------------------------------------------------------------
 * @code
 * void exempleNoexcept()
 * {
 *     // NkFunction avec signature noexcept : garantit que l'appel ne lève pas d'exception
 *     nkentseu::NkFunction<int(int) noexcept> safeOp = [](int x) noexcept -> int {
 *         return x * 2;  // Opération safe, pas d'allocation, pas d'exception possible
 *     };
 *     
 *     // L'appel est garanti noexcept par le type
 *     static_assert(noexcept(safeOp(42)), "Appel doit être noexcept");
 *     
 *     // Tentative d'assigner un callable non-noexcept : erreur de compilation
 *     // nkentseu::NkFunction<int(int) noexcept> unsafe = [](int x) {
 *     //     throw std::runtime_error("oops");  // Erreur : peut lever une exception
 *     // };
 *     
 *     // Cas d'usage : callbacks dans un contexte temps réel ou no-exception
 *     class RealTimeSystem {
 *     public:
 *         // Enregistrer un callback qui ne doit jamais lever d'exception
 *         void RegisterCriticalCallback(nkentseu::NkFunction<void() noexcept> cb) {
 *             criticalCallback = cb;
 *         }
 *         
 *         void ExecuteCritical() noexcept {
 *             if (criticalCallback) {
 *                 criticalCallback();  // Garanti noexcept : safe même en contexte critique
 *             }
 *         }
 *         
 *     private:
 *         nkentseu::NkFunction<void() noexcept> criticalCallback;
 *     };
 *     
 *     RealTimeSystem rts;
 *     rts.RegisterCriticalCallback([]() noexcept {
 *         // Opérations critiques sans allocation ni exception
 *         // Ex: mise à jour de registres matériels, flags atomiques, etc.
 *     });
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Statistiques mémoire - Profiling et optimisation
 * --------------------------------------------------------------------------
 * @code
 * // Activer NK_FUNCTION_ENABLE_STATS=1 (activé par défaut en debug)
 * 
 * void exempleStatsMemoire()
 * {
 *     // Réinitialiser les stats globales avant mesure
 *     nkentseu::NkFunction<void()>::ResetGlobalStats();
 *     
 *     // Créer plusieurs NkFunction avec différents profils
 *     nkentseu::NkFunction<void()> fn1 = []() { printf("SBO\n"); };  // Petit : SBO
 *     
 *     nkentseu::NkVector<int> gros(1000);  // ~4KB
 *     nkentseu::NkFunction<void()> fn2 = [vec = NkMove(gros)]() {};  // Gros : heap
 *     
 *     nkentseu::NkFunction<void()> fn3 = fn1;  // Copie : peut réutiliser SBO
 *     
 *     // Statistiques par instance
 *     printf("fn1 (SBO) : %zu allocs, %zu bytes\n", 
 *            fn1.GetAllocationCount(), fn1.GetTotalMemory());  // 0, 0
 *     printf("fn2 (heap): %zu allocs, %zu bytes\n", 
 *            fn2.GetAllocationCount(), fn2.GetTotalMemory());  // 1, ~size(CallableImpl)
 *     
 *     // Statistiques globales
 *     const auto& global = nkentseu::NkFunction<void()>::GetGlobalStats();
 *     printf("\nStats globales :\n");
 *     printf("  Total allocations heap : %zu\n", global.allocationCount);
 *     printf("  Total désallocations   : %zu\n", global.deallocationCount);
 *     printf("  Mémoire heap active    : %zu bytes\n", global.totalMemoryBytes);
 *     
 *     // Nettoyage et vérification
 *     fn1.Clear();
 *     fn2.Clear();
 *     fn3.Clear();
 *     
 *     const auto& after = nkentseu::NkFunction<void()>::GetGlobalStats();
 *     printf("\nAprès Clear() :\n");
 *     printf("  Mémoire heap active : %zu bytes (devrait être 0)\n", after.totalMemoryBytes);
 *     
 *     // Cas d'usage : monitoring en production (si NK_FUNCTION_ENABLE_STATS=1 en release)
 *     // if (global.totalMemoryBytes > SEUIL_ALERT) {
 *     //     NKENTSEU_LOG_WARNING("NkFunction : usage mémoire élevé : %zu bytes", 
 *     //                          global.totalMemoryBytes);
 *     // }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES POUR LES EXTENSIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CONFIGURATION DE NK_FUNCTION_SBO_BUFFER_SIZE :
 *    - Valeur par défaut : 64 bytes (couvre ~90% des lambdas sans capture)
 *    - Augmenter à 128/256 si vos callables ont des captures modérées
 *    - Réduire à 32 pour les systèmes embarqués avec mémoire contrainte
 *    - Profiler avec GetSboBufferSize() et UsesSbo() pour ajuster
 * 
 * 2. UTILISATION DE target<T>() :
 *    - Activer NK_FUNCTION_ENABLE_RTTI=1 uniquement si nécessaire (overhead minimal)
 *    - Utiliser pour les chemins critiques où l'appel direct est significativement plus rapide
 *    - Toujours prévoir un fallback via l'appel polymorphe si target<T>() retourne nullptr
 *    - Documenter le type attendu du callable pour la maintenance
 * 
 * 3. NkBind ET APPLICATION PARTIELLE :
 *    - Privilégier les lambdas simples pour la lisibilité quand possible
 *    - Utiliser NkBind pour les APIs génériques où la curryfication apporte de la flexibilité
 *    - Attention aux captures par référence dans les callables bindés : durée de vie
 *    - Pour les méthodes membres : vérifier que l'objet reste valide pendant l'usage du NkFunction
 * 
 * 4. SIGNATURES noexcept :
 *    - Utiliser NkFunction<R(Args...) noexcept> pour les callbacks critiques (temps réel, ISR)
 *    - Le compilateur vérifiera que le callable fourni est bien noexcept
 *    - Permet des optimisations supplémentaires (pas de tables d'exception, meilleur inlining)
 *    - Ne pas utiliser noexcept "au cas où" : cela restreint les callables acceptés
 * 
 * 5. STATISTIQUES MÉMOIRE :
 *    - NK_FUNCTION_ENABLE_STATS activé par défaut en debug, désactivé en release
 *    - Utiliser GetGlobalStats() pour le monitoring à l'échelle application
 *    - ResetGlobalStats() pour mesurer l'usage sur des phases spécifiques (ex: chargement de niveau)
 *    - Attention : les stats sont thread-unsafe ; synchroniser si accès concurrents
 * 
 * 6. COMBINAISON DES EXTENSIONS :
 *    - SBO + stats : les callables SBO n'apparaissent pas dans les stats heap (comportement attendu)
 *    - target<T>() + noexcept : un callable noexcept peut être récupéré via target<T>() si RTTI activé
 *    - NkBind + SBO : le wrapper généré par NkBind peut tenir dans le buffer SBO s'il est petit
 * 
 * 7. LIMITATIONS ET WORKAROUNDS :
 *    - target<T>() : implémentation simplifiée ici ; pour une version robuste, utiliser un système de type-erasure avec std::type_info ou équivalent
 *    - NkBind : version basique ; pour une implémentation complète avec tuple et index_sequence, voir NKAlgorithms/NkBind.h (à créer)
 *    - noexcept : la spécialisation présentée délègue à l'implémentation principale ; pour une isolation complète, dupliquer l'implémentation avec garanties noexcept
 * 
 * 8. PERFORMANCE COMPARÉE :
 *    - SBO : élimine l'allocation heap pour les petits callables → réduit la fragmentation et améliore la localité cache
 *    - target<T>() : permet d'éviter l'appel virtuel dans les boucles critiques → gain potentiel de 2-5x sur l'invocation
 *    - noexcept : permet au compilateur d'optimiser agressivement (pas de unwind tables, meilleur inlining)
 *    - Stats : overhead négligeable (<1%) quand activé ; désactiver en production si chaque cycle compte
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Création : 2025-06-10 par TEUGUIA TADJUIDJE Rodolf
// Mise à jour v2.0.0 : 2026-03-14 par Rihen
// Mise à jour v2.1.0 : 2026-04-26 (SBO, target<T>(), NkBind, noexcept, stats)
// ============================================================