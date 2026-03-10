/**
 * @file NkFunction.h
 * @description Fournit une classe pour stocker des objets appelables dans le framework Nkentseu, similaire à std::function.
 * @author TEUGUIA TADJUIDJE Rodolf
 * @date 2025-06-10
 * @license Rihen
 */
#pragma once

#include "NKCore/NkTypes.h"
#include "NKCore/NkTraits.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKMemory/NkFunction.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {

    namespace detail {
        inline mem::NkAllocator* NkResolveFunctionAllocator(mem::NkAllocator* allocator) noexcept {
            mem::NkAllocator* fallback = &mem::NkGetDefaultAllocator();
            if (!allocator) {
                return fallback;
            }
            const usize addr = reinterpret_cast<usize>(allocator);
            const usize alignMask = static_cast<usize>(alignof(void*)) - 1u;
            if ((addr & alignMask) != 0u) {
                return fallback;
            }
            return allocator;
        }
    } // namespace detail

    /**
     * @class NkFunction
     * @brief Conteneur polymorphe pour objets appelables, similaire à std::function.
     * @tparam R Type de retour de l'appel.
     * @tparam Args Types des arguments de l'appel.
     */
    template<typename R, typename... Args>
    class NkFunction {
        private:
            // Classe de base abstraite pour l'effacement de type
            struct CallableBase {
                virtual ~CallableBase() = default;
                virtual R Invoke(Args... args) = 0;
                virtual R InvokeWithIndex(usize index, Args... args) = 0; // Pour binding multiple
                virtual CallableBase* Clone(mem::NkAllocator* allocator) const = 0;
                virtual void Destroy(mem::NkAllocator* allocator) = 0;
            };

            // Implémentation pour un callable générique
            template<typename T>
            struct CallableImpl : CallableBase {
                T callable;

                explicit CallableImpl(const T& c) noexcept(traits::NkIsTriviallyCopyable_v<T>) : callable(c) {}
                explicit CallableImpl(T&& c) noexcept(traits::NkIsTriviallyMoveConstructible_v<T>) : callable(traits::NkMove(c)) {}

                R Invoke(Args... args) override {
                    return callable(traits::NkForward<Args>(args)...);
                }

                R InvokeWithIndex(usize, Args... args) override {
                    return Invoke(traits::NkForward<Args>(args)...);
                }

                CallableBase* Clone(mem::NkAllocator* allocator) const override {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    void* memory = alloc->Allocate(sizeof(CallableImpl), alignof(CallableImpl));
                    if (!memory) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::Clone: Allocation failed.");
                        #endif
                        return nullptr;
                    }
                    return new (memory) CallableImpl(callable);
                }

                void Destroy(mem::NkAllocator* allocator) override {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    this->~CallableImpl();
                    alloc->Deallocate(this, sizeof(CallableImpl));
                }
            };

            // Implémentation pour une méthode membre non-const
            template<typename T>
            struct MethodCallableImpl : CallableBase {
                T* object;
                R (T::*method)(Args...);

                MethodCallableImpl(T* obj, R (T::*meth)(Args...)) : object(obj), method(meth) {}

                R Invoke(Args... args) override {
                    if (!object) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::Invoke: Null object pointer.");
                        #endif
                        if constexpr (!traits::NkIsVoid_v<R>) {
                            return R();
                        } else {
                            return;
                        }
                    }
                    return (object->*method)(traits::NkForward<Args>(args)...);
                }

                R InvokeWithIndex(usize, Args... args) override {
                    return Invoke(traits::NkForward<Args>(args)...);
                }

                CallableBase* Clone(mem::NkAllocator* allocator) const override {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    void* memory = alloc->Allocate(sizeof(MethodCallableImpl), alignof(MethodCallableImpl));
                    if (!memory) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::Clone: Allocation failed for method.");
                        #endif
                        return nullptr;
                    }
                    return new (memory) MethodCallableImpl(object, method);
                }

                void Destroy(mem::NkAllocator* allocator) override {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    this->~MethodCallableImpl();
                    alloc->Deallocate(this, sizeof(MethodCallableImpl));
                }
            };

            // Implémentation pour une méthode membre const
            template<typename T>
            struct MethodCallableConstImpl : CallableBase {
                T* object;
                R (T::*method)(Args...) const;

                MethodCallableConstImpl(T* obj, R (T::*meth)(Args...) const) : object(obj), method(meth) {}

                R Invoke(Args... args) override {
                    if (!object) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::Invoke: Null object pointer.");
                        #endif
                        if constexpr (!traits::NkIsVoid_v<R>) {
                            return R();
                        } else {
                            return;
                        }
                    }
                    return (object->*method)(traits::NkForward<Args>(args)...);
                }

                R InvokeWithIndex(usize, Args... args) override {
                    return Invoke(traits::NkForward<Args>(args)...);
                }

                CallableBase* Clone(mem::NkAllocator* allocator) const override {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    void* memory = alloc->Allocate(sizeof(MethodCallableConstImpl), alignof(MethodCallableConstImpl));
                    if (!memory) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::Clone: Allocation failed for const method.");
                        #endif
                        return nullptr;
                    }
                    return new (memory) MethodCallableConstImpl(object, method);
                }

                void Destroy(mem::NkAllocator* allocator) override {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    this->~MethodCallableConstImpl();
                    alloc->Deallocate(this, sizeof(MethodCallableConstImpl));
                }
            };

            // Implémentation pour binding multiple
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
                mem::NkAllocator* allocator;

                MultiMethodCallableImpl(T* obj, mem::NkAllocator* alloc) 
                    : object(obj), methods(nullptr), method_count(0), allocator(detail::NkResolveFunctionAllocator(alloc)) {}

                void AddMethod(R (T::*meth)(Args...), usize index) {
                    if (!meth) return;
                    resizeMethods(index + 1);
                    methods[index] = { meth, nullptr, false };
                }

                void AddConstMethod(R (T::*meth)(Args...) const, usize index) {
                    if (!meth) return;
                    resizeMethods(index + 1);
                    methods[index] = { nullptr, meth, true };
                }

                void resizeMethods(usize new_size) {
                    if (new_size <= method_count) return;
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    MethodEntry* new_methods = static_cast<MethodEntry*>(
                        alloc->Allocate(new_size * sizeof(MethodEntry), alignof(MethodEntry)));
                    if (!new_methods) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::MultiMethod: Allocation failed.");
                        #endif
                        return;
                    }
                    for (usize i = 0; i < method_count; ++i) {
                        new_methods[i] = methods[i];
                    }
                    for (usize i = method_count; i < new_size; ++i) {
                        new_methods[i] = { nullptr, nullptr, false };
                    }
                    if (methods) {
                        alloc->Deallocate(methods, method_count * sizeof(MethodEntry));
                    }
                    methods = new_methods;
                    method_count = new_size;
                    allocator = alloc;
                }

                R Invoke(Args... args) override {
                    return InvokeWithIndex(0, traits::NkForward<Args>(args)...);
                }

                R InvokeWithIndex(usize index, Args... args) override {
                    if (!object || index >= method_count || (!methods[index].method && !methods[index].const_method)) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::InvokeWithIndex: Invalid object or method index %zu.", index);
                        #endif
                        if constexpr (!traits::NkIsVoid_v<R>) {
                            return R();
                        } else {
                            return;
                        }
                    }
                    if (methods[index].is_const) {
                        return (object->*methods[index].const_method)(traits::NkForward<Args>(args)...);
                    } else {
                        return (object->*methods[index].method)(traits::NkForward<Args>(args)...);
                    }
                }

                CallableBase* Clone(mem::NkAllocator* allocator) const override {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    void* memory = alloc->Allocate(sizeof(MultiMethodCallableImpl), alignof(MultiMethodCallableImpl));
                    if (!memory) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::Clone: Allocation failed for multi-method.");
                        #endif
                        return nullptr;
                    }
                    auto* clone = new (memory) MultiMethodCallableImpl(object, alloc);
                    clone->resizeMethods(method_count);
                    for (usize i = 0; i < method_count; ++i) {
                        clone->methods[i] = methods[i];
                    }
                    return clone;
                }

                void Destroy(mem::NkAllocator* allocator) override {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                    if (methods) {
                        alloc->Deallocate(methods, method_count * sizeof(MethodEntry));
                    }
                    this->~MultiMethodCallableImpl();
                    alloc->Deallocate(this, sizeof(MultiMethodCallableImpl));
                }
            };

        public:
            // Types membres
            using ResultType = R;

            // Constructeur par défaut (vide)
            explicit NkFunction(mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator()) noexcept
                : m_callable(nullptr), m_allocator(detail::NkResolveFunctionAllocator(allocator)) {}

            // Constructeur à partir d'un objet callable
            template<typename F, typename = traits::NkEnableIf_t<!traits::NkIsSame_v<traits::NkRemoveReference_t<F>, NkFunction>>>
            NkFunction(F&& f, mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
                : m_callable(nullptr), m_allocator(detail::NkResolveFunctionAllocator(allocator)) {
                using CallableT = traits::NkRemoveReference_t<F>;
                void* memory = m_allocator->Allocate(sizeof(CallableImpl<CallableT>), alignof(CallableImpl<CallableT>));
                if (!memory) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::Constructor: Allocation failed for callable.");
                    #endif
                    return;
                }
                m_callable = new (memory) CallableImpl<CallableT>(traits::NkForward<F>(f));
            }

            // Constructeur pour méthode membre non-const
            template<typename T>
            NkFunction(T* obj, R (T::*meth)(Args...), mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
                : m_callable(nullptr), m_allocator(detail::NkResolveFunctionAllocator(allocator)) {
                if (!obj || !meth) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::Constructor: Null object or method pointer.");
                    #endif
                    return;
                }
                void* memory = m_allocator->Allocate(sizeof(MethodCallableImpl<T>), alignof(MethodCallableImpl<T>));
                if (!memory) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::Constructor: Allocation failed for method callable.");
                    #endif
                    return;
                }
                m_callable = new (memory) MethodCallableImpl<T>(obj, meth);
            }

            // Constructeur pour méthode membre const
            template<typename T>
            NkFunction(T* obj, R (T::*meth)(Args...) const, mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
                : m_callable(nullptr), m_allocator(detail::NkResolveFunctionAllocator(allocator)) {
                if (!obj || !meth) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::Constructor: Null object or const method pointer.");
                    #endif
                    return;
                }
                void* memory = m_allocator->Allocate(sizeof(MethodCallableConstImpl<T>), alignof(MethodCallableConstImpl<T>));
                if (!memory) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::Constructor: Allocation failed for const method callable.");
                    #endif
                    return;
                }
                m_callable = new (memory) MethodCallableConstImpl<T>(obj, meth);
            }

            // Constructeur pour binding multiple
            template<typename T>
            NkFunction(T* obj, mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
                : m_callable(nullptr), m_allocator(detail::NkResolveFunctionAllocator(allocator)) {
                if (!obj) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::Constructor: Null object for multi-method.");
                    #endif
                    return;
                }
                void* memory = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>), alignof(MultiMethodCallableImpl<T>));
                if (!memory) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::Constructor: Allocation failed for multi-method callable.");
                    #endif
                    return;
                }
                m_callable = new (memory) MultiMethodCallableImpl<T>(obj, allocator);
            }

            // Constructeur par copie
            NkFunction(const NkFunction& other) noexcept
                : m_callable(nullptr), m_allocator(detail::NkResolveFunctionAllocator(other.m_allocator)) {
                if (other.m_callable) {
                    m_callable = other.m_callable->Clone(m_allocator);
                }
            }

            // Constructeur par déplacement
            NkFunction(NkFunction&& other) noexcept
                : m_callable(other.m_callable), m_allocator(detail::NkResolveFunctionAllocator(other.m_allocator)) {
                other.m_callable = nullptr;
            }

            // Destructeur
            ~NkFunction() noexcept {
                Clear();
            }

            // Affectation par copie
            NkFunction& operator=(const NkFunction& other) noexcept {
                if (this != &other) {
                    NkFunction temp(other);
                    Swap(temp);
                }
                return *this;
            }

            // Affectation par déplacement
            NkFunction& operator=(NkFunction&& other) noexcept {
                if (this != &other) {
                    Clear();
                    m_callable = other.m_callable;
                    m_allocator = detail::NkResolveFunctionAllocator(other.m_allocator);
                    other.m_callable = nullptr;
                }
                return *this;
            }

            // Affectation à partir d'un objet callable
            template<typename F>
            NkFunction& operator=(F&& f) {
                NkFunction temp(traits::NkForward<F>(f), m_allocator);
                Swap(temp);
                return *this;
            }

            // Affectation pour méthode membre
            template<typename T>
            NkFunction& operator=(NkPair<T*, R (T::*)(Args...)> binding) {
                NkFunction temp(binding.first, binding.second, m_allocator);
                Swap(temp);
                return *this;
            }

            // Affectation pour méthode membre const
            template<typename T>
            NkFunction& operator=(NkPair<T*, R (T::*)(Args...) const> binding) {
                NkFunction temp(binding.first, binding.second, m_allocator);
                Swap(temp);
                return *this;
            }

            // Ajout d'une méthode non-const pour binding multiple
            template<typename T>
            void BindMethod(T* obj, R (T::*meth)(Args...), usize index) {
                if (!obj || !meth) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::BindMethod: Null object or method pointer.");
                    #endif
                    return;
                }
                if (!m_callable || !dynamic_cast<MultiMethodCallableImpl<T>*>(m_callable)) {
                    Clear();
                    void* memory = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>), alignof(MultiMethodCallableImpl<T>));
                    if (!memory) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::BindMethod: Allocation failed.");
                        #endif
                        return;
                    }
                    m_callable = new (memory) MultiMethodCallableImpl<T>(obj, m_allocator);
                }
                static_cast<MultiMethodCallableImpl<T>*>(m_callable)->AddMethod(meth, index);
            }

            // Ajout d'une méthode const pour binding multiple
            template<typename T>
            void BindMethod(T* obj, R (T::*meth)(Args...) const, usize index) {
                if (!obj || !meth) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::BindMethod: Null object or const method pointer.");
                    #endif
                    return;
                }
                if (!m_callable || !dynamic_cast<MultiMethodCallableImpl<T>*>(m_callable)) {
                    Clear();
                    void* memory = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>), alignof(MultiMethodCallableImpl<T>));
                    if (!memory) {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::BindMethod: Allocation failed.");
                        #endif
                        return;
                    }
                    m_callable = new (memory) MultiMethodCallableImpl<T>(obj, m_allocator);
                }
                static_cast<MultiMethodCallableImpl<T>*>(m_callable)->AddConstMethod(meth, index);
            }

            // Invocation standard
            R operator()(Args... args) const {
                if (!m_callable) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::operator(): Attempt to call empty function.");
                    #endif
                    if constexpr (!traits::NkIsVoid_v<R>) {
                        return R();
                    } else {
                        return;
                    }
                }
                return m_callable->Invoke(traits::NkForward<Args>(args)...);
            }

            // Invocation avec index pour binding multiple
            R operator()(usize index, Args... args) const {
                if (!m_callable) {
                    #ifdef NKENTSEU_LOG_ENABLED
                    NKENTSEU_LOG_ERROR("NkFunction::operator(): Attempt to call empty function with index %zu.", index);
                    #endif
                    if constexpr (!traits::NkIsVoid_v<R>) {
                        return R();
                    } else {
                        return;
                    }
                }
                return m_callable->InvokeWithIndex(index, traits::NkForward<Args>(args)...);
            }

            // Vérifie si la fonction est valide
            explicit operator bool() const noexcept {
                return m_callable != nullptr;
            }

            // Échange
            void Swap(NkFunction& other) noexcept {
                mem::NkSwap(m_callable, other.m_callable);
                mem::NkSwap(m_allocator, other.m_allocator);
            }

            // Nettoyage
            void Clear() noexcept {
                if (m_callable) {
                    mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(m_allocator);
                    m_callable->Destroy(alloc);
                    m_callable = nullptr;
                }
                m_allocator = detail::NkResolveFunctionAllocator(m_allocator);
            }

        private:
            CallableBase* m_callable;
            mem::NkAllocator* m_allocator;
    };

} // namespace nkentseu
