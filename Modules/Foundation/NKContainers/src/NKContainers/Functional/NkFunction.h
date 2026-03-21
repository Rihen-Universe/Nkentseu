/**
 * @file NkFunction.h
 * @description Conteneur polymorphe pour objets appelables — sans STL.
 *              Syntaxe identique à std::function : NkFunction<void(int, float)>.
 * @author TEUGUIA TADJUIDJE Rodolf
 * @date 2025-06-10 / v2.0.0 2026-03-14
 * @license Rihen
 *
 * ── Syntaxe d'utilisation ───────────────────────────────────────────────────
 *
 *   // Fonction libre / lambda / foncteur
 *   NkFunction<void()>          fn  = []() { puts("hello"); };
 *   NkFunction<int(int, float)> fn2 = &MyFreeFunction;
 *
 *   // Méthode membre non-const
 *   NkFunction<void(int)> fn3(&obj, &MyClass::Method);
 *
 *   // Méthode membre const
 *   NkFunction<int()> fn4(&obj, &MyClass::GetValue);
 *
 *   // Test de validité
 *   if (fn)           { fn(); }
 *   if (fn != nullptr){ fn(); }
 *   fn = nullptr;               // reset
 *
 *   // Copie / déplacement
 *   NkFunction<void()> fn5 = fn;           // copie
 *   NkFunction<void()> fn6 = NkMove(fn);   // move
 *   fn5 = fn6;                             // copy-assign (lvalue non-const: OK)
 *
 * ── Ce qui diffère de std::function ────────────────────────────────────────
 *
 *   + Binding multiple via BindMethod(obj, &Class::Method, index)
 *   + fn(index, args...) pour invoquer la méthode à l'index donné
 *   + Allocateur personnalisable (pas de small-buffer optimization intégrée)
 *   - Pas de target() / target_type() (pas de RTTI)
 *   - Pas de bad_function_call : appel sur vide retourne R() silencieusement
 *     (comportement préférable dans un moteur de jeu sans exceptions)
 *
 * ── Changements v2.0.0 ──────────────────────────────────────────────────────
 *
 *   - Syntaxe spécialisation partielle : NkFunction<R(Args...)>
 *     au lieu de l'ancienne NkFunction<R, Args...>.
 *   - Correctif operator=(F&&) : enable_if utilise NkDecay_t<F> pour exclure
 *     NkFunction lui-même quelle que soit la catégorie de valeur.
 *   - Ajout de operator=(NkFunction&) non-const pour capturer les lvalue.
 *   - Ajout de operator=(nullptr_t) et constructeur(nullptr_t) pour reset propre.
 *   - Ajout de operator==(nullptr_t) / operator!=(nullptr_t).
 *   - Ajout de NkSwap(NkFunction&, NkFunction&) non-membre (ADL).
 */
#pragma once

#include "NKCore/NkTypes.h"
#include "NKCore/NkTraits.h"
#include "NKContainers/Heterogeneous/NkPair.h"
#include "NKMemory/NkFunction.h"
#include "NKMemory/NkAllocator.h"

namespace nkentseu {

    // =========================================================================
    // nullptr_t sans <cstddef>
    // Tous les compilateurs C++11+ définissent nullptr_t comme builtin,
    // mais l'alias ci-dessous garantit l'accès sans dépendre de cstddef.
    // =========================================================================

    using NkNullptrT = decltype(nullptr);


    namespace detail {

        /// Résout l'allocateur : retourne le défaut si nul ou mal aligné.
        inline mem::NkAllocator* NkResolveFunctionAllocator(
                mem::NkAllocator* allocator) noexcept {
            mem::NkAllocator* fallback = &mem::NkGetDefaultAllocator();
            if (!allocator) { return fallback; }
            const usize addr      = reinterpret_cast<usize>(allocator);
            const usize alignMask = static_cast<usize>(alignof(void*)) - 1u;
            if ((addr & alignMask) != 0u) { return fallback; }
            return allocator;
        }

    } // namespace detail


    // =========================================================================
    // Déclaration primaire — jamais instanciée directement.
    // Force l'utilisation de la syntaxe NkFunction<R(Args...)>.
    // =========================================================================

    template<typename Signature>
    class NkFunction;   // Intentionnellement non définie — voir spécialisation


    // =========================================================================
    // NkFunction<R(Args...)>
    //
    // Spécialisation partielle sur une signature de fonction.
    // Permet la syntaxe:
    //   NkFunction<void(int, float)> fn = [](int, float) {};
    // =========================================================================

    template<typename R, typename... Args>
    class NkFunction<R(Args...)> {
    private:

        // ── Interface interne pour l'effacement de type ────────────────────────

        struct CallableBase {
            virtual ~CallableBase() = default;

            /// Invocation standard
            virtual R Invoke(Args... args) = 0;

            /// Invocation indexée — pour le binding multiple
            virtual R InvokeWithIndex(usize index, Args... args) = 0;

            /// Clonage dans un allocateur donné
            virtual CallableBase* Clone(mem::NkAllocator* allocator) const = 0;

            /// Destruction + désallocation
            virtual void Destroy(mem::NkAllocator* allocator) = 0;
        };

        // ── Callable générique (lambda, foncteur, pointeur de fonction) ────────

        template<typename T>
        struct CallableImpl : CallableBase {

            T callable;

            explicit CallableImpl(const T& c)
                noexcept(traits::NkIsTriviallyCopyable_v<T>)
                : callable(c) {}

            explicit CallableImpl(T&& c)
                noexcept(traits::NkIsTriviallyMoveConstructible_v<T>)
                : callable(traits::NkMove(c)) {}

            R Invoke(Args... args) override {
                return callable(traits::NkForward<Args>(args)...);
            }

            R InvokeWithIndex(usize, Args... args) override {
                return Invoke(traits::NkForward<Args>(args)...);
            }

            CallableBase* Clone(mem::NkAllocator* allocator) const override {
                mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                void* mem = alloc->Allocate(sizeof(CallableImpl), alignof(CallableImpl));
                if (!mem) { return nullptr; }
                return new (mem) CallableImpl(callable);
            }

            void Destroy(mem::NkAllocator* allocator) override {
                mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                this->~CallableImpl();
                alloc->Deallocate(this, sizeof(CallableImpl));
            }

        }; // struct CallableImpl


        // ── Méthode membre non-const ───────────────────────────────────────────

        template<typename T>
        struct MethodCallableImpl : CallableBase {

            T*            object;
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

            CallableBase* Clone(mem::NkAllocator* allocator) const override {
                mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                void* mem = alloc->Allocate(sizeof(MethodCallableImpl),
                                            alignof(MethodCallableImpl));
                if (!mem) { return nullptr; }
                return new (mem) MethodCallableImpl(object, method);
            }

            void Destroy(mem::NkAllocator* allocator) override {
                mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                this->~MethodCallableImpl();
                alloc->Deallocate(this, sizeof(MethodCallableImpl));
            }

        private:
            static R DefaultReturn() {
                if constexpr (traits::NkIsVoid_v<R>) { return; }
                else { return R{}; }
            }

        }; // struct MethodCallableImpl


        // ── Méthode membre const ───────────────────────────────────────────────

        template<typename T>
        struct MethodCallableConstImpl : CallableBase {

            T*            object;
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

            CallableBase* Clone(mem::NkAllocator* allocator) const override {
                mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                void* mem = alloc->Allocate(sizeof(MethodCallableConstImpl),
                                            alignof(MethodCallableConstImpl));
                if (!mem) { return nullptr; }
                return new (mem) MethodCallableConstImpl(object, method);
            }

            void Destroy(mem::NkAllocator* allocator) override {
                mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                this->~MethodCallableConstImpl();
                alloc->Deallocate(this, sizeof(MethodCallableConstImpl));
            }

        private:
            static R DefaultReturn() {
                if constexpr (traits::NkIsVoid_v<R>) { return; }
                else { return R{}; }
            }

        }; // struct MethodCallableConstImpl


        // ── Binding multiple (plusieurs méthodes indexées sur le même objet) ───

        template<typename T>
        struct MultiMethodCallableImpl : CallableBase {

            struct MethodEntry {
                R (T::*method)      (Args...);
                R (T::*const_method)(Args...) const;
                bool is_const;
            };

            T*                object;
            MethodEntry*      methods;
            usize             method_count;
            mem::NkAllocator* alloc;

            MultiMethodCallableImpl(T* obj, mem::NkAllocator* a)
                : object(obj)
                , methods(nullptr)
                , method_count(0)
                , alloc(detail::NkResolveFunctionAllocator(a)) {}

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
                    alloc->Allocate(newSize * sizeof(MethodEntry),
                                    alignof(MethodEntry)));
                if (!newM) { return; }
                for (usize i = 0; i < method_count;  ++i) { newM[i] = methods[i]; }
                for (usize i = method_count; i < newSize; ++i) {
                    newM[i] = { nullptr, nullptr, false };
                }
                if (methods) {
                    alloc->Deallocate(methods, method_count * sizeof(MethodEntry));
                }
                methods      = newM;
                method_count = newSize;
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
                    return (object->*methods[index].const_method)(
                        traits::NkForward<Args>(args)...);
                } else {
                    return (object->*methods[index].method)(
                        traits::NkForward<Args>(args)...);
                }
            }

            CallableBase* Clone(mem::NkAllocator* allocator) const override {
                mem::NkAllocator* a = detail::NkResolveFunctionAllocator(allocator);
                void* mem = a->Allocate(sizeof(MultiMethodCallableImpl),
                                        alignof(MultiMethodCallableImpl));
                if (!mem) { return nullptr; }
                auto* clone = new (mem) MultiMethodCallableImpl(object, a);
                clone->ResizeMethods(method_count);
                for (usize i = 0; i < method_count; ++i) {
                    clone->methods[i] = methods[i];
                }
                return clone;
            }

            void Destroy(mem::NkAllocator* allocator) override {
                mem::NkAllocator* a = detail::NkResolveFunctionAllocator(allocator);
                if (methods) {
                    a->Deallocate(methods, method_count * sizeof(MethodEntry));
                }
                this->~MultiMethodCallableImpl();
                a->Deallocate(this, sizeof(MultiMethodCallableImpl));
            }

        }; // struct MultiMethodCallableImpl


    public:

        // ── Type membre ────────────────────────────────────────────────────────

        using ResultType = R;

        // ── Constructeurs ──────────────────────────────────────────────────────

        /// Constructeur par défaut — crée une fonction vide
        explicit NkFunction(
                mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator()) noexcept
            : m_callable(nullptr)
            , m_allocator(detail::NkResolveFunctionAllocator(allocator)) {}

        /// Constructeur depuis nullptr — identique au défaut, syntaxe STL-like
        NkFunction(NkNullptrT, mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator()) noexcept
            : m_callable(nullptr)
            , m_allocator(detail::NkResolveFunctionAllocator(allocator)) {}

        /**
         * Constructeur depuis un callable quelconque (lambda, foncteur, fn libre).
         *
         * enable_if: NkDecay_t<F> != NkFunction<R(Args...)>
         *   NkDecay_t élimine ref + const + volatile avant la comparaison.
         *   → Invisible pour NkFunction&, const NkFunction&, NkFunction&&.
         *   → Les copy/move constructors ci-dessous prennent le relais dans ces cas.
         */
        template<
            typename F,
            typename = traits::NkEnableIf_t<
                !traits::NkIsSame_v<traits::NkDecay_t<F>, NkFunction>
            >
        >
        NkFunction(F&& f,
                   mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
            : m_callable(nullptr)
            , m_allocator(detail::NkResolveFunctionAllocator(allocator)) {
            using CallableT = traits::NkRemoveReference_t<F>;
            void* mem = m_allocator->Allocate(sizeof(CallableImpl<CallableT>),
                                               alignof(CallableImpl<CallableT>));
            if (!mem) {
#ifdef NKENTSEU_LOG_ENABLED
                NKENTSEU_LOG_ERROR("NkFunction: allocation failed.");
#endif
                return;
            }
            m_callable = new (mem) CallableImpl<CallableT>(traits::NkForward<F>(f));
        }

        /// Constructeur depuis une méthode membre non-const
        template<typename T>
        NkFunction(T* obj, R (T::*meth)(Args...),
                   mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
            : m_callable(nullptr)
            , m_allocator(detail::NkResolveFunctionAllocator(allocator)) {
            if (!obj || !meth) { return; }
            void* mem = m_allocator->Allocate(sizeof(MethodCallableImpl<T>),
                                               alignof(MethodCallableImpl<T>));
            if (!mem) { return; }
            m_callable = new (mem) MethodCallableImpl<T>(obj, meth);
        }

        /// Constructeur depuis une méthode membre const
        template<typename T>
        NkFunction(T* obj, R (T::*meth)(Args...) const,
                   mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
            : m_callable(nullptr)
            , m_allocator(detail::NkResolveFunctionAllocator(allocator)) {
            if (!obj || !meth) { return; }
            void* mem = m_allocator->Allocate(sizeof(MethodCallableConstImpl<T>),
                                               alignof(MethodCallableConstImpl<T>));
            if (!mem) { return; }
            m_callable = new (mem) MethodCallableConstImpl<T>(obj, meth);
        }

        /// Constructeur pour binding multiple
        template<typename T>
        NkFunction(T* obj,
                   mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
            : m_callable(nullptr)
            , m_allocator(detail::NkResolveFunctionAllocator(allocator)) {
            if (!obj) { return; }
            void* mem = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>),
                                               alignof(MultiMethodCallableImpl<T>));
            if (!mem) { return; }
            m_callable = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
        }

        // ── Rule of Five ──────────────────────────────────────────────────────

        /// Copy constructor — clone le callable interne
        NkFunction(const NkFunction& other) noexcept
            : m_callable(nullptr)
            , m_allocator(detail::NkResolveFunctionAllocator(other.m_allocator)) {
            if (other.m_callable) {
                m_callable = other.m_callable->Clone(m_allocator);
            }
        }

        /// Move constructor — vole le callable, aucune allocation
        NkFunction(NkFunction&& other) noexcept
            : m_callable(other.m_callable)
            , m_allocator(detail::NkResolveFunctionAllocator(other.m_allocator)) {
            other.m_callable = nullptr;
        }

        /// Destructeur
        ~NkFunction() noexcept {
            Clear();
        }

        // ── Affectation ────────────────────────────────────────────────────────

        /// Réinitialisation via nullptr — syntaxe STL-like
        NkFunction& operator=(NkNullptrT) noexcept {
            Clear();
            return *this;
        }

        /// Copy-assign (const lvalue)
        NkFunction& operator=(const NkFunction& other) noexcept {
            if (this != &other) {
                NkFunction temp(other);
                Swap(temp);
            }
            return *this;
        }

        /**
         * Copy-assign (lvalue non-const).
         *
         * Sans cette surcharge, `fn1 = fn2` sélectionne l'operator= template
         * (meilleure surcharge pour une lvalue non-const que const NkFunction&).
         * Ce template tente alors NkFunction(fn2, alloc) dont le ctor callable
         * est désactivé par enable_if → erreur de compilation.
         *
         * Cette surcharge intercepte exactement NkFunction& et délègue au
         * copy-assign const, qui lui est correct.
         */
        NkFunction& operator=(NkFunction& other) noexcept {
            return operator=(static_cast<const NkFunction&>(other));
        }

        /// Move-assign (rvalue)
        NkFunction& operator=(NkFunction&& other) noexcept {
            if (this != &other) {
                Clear();
                m_callable  = other.m_callable;
                m_allocator = detail::NkResolveFunctionAllocator(other.m_allocator);
                other.m_callable = nullptr;
            }
            return *this;
        }

        /**
         * Affectation depuis un callable quelconque.
         *
         * enable_if avec NkDecay_t<F>:
         *   NkDecay_t élimine ref + const + volatile avant la comparaison.
         *   → Ce template est INVISIBLE pour NkFunction, NkFunction&,
         *     const NkFunction&, NkFunction&&.
         *   Les surcharges dédiées ci-dessus prennent le relais.
         *
         * Implémentation: copy-and-swap → exception-safe.
         */
        template<
            typename F,
            typename = traits::NkEnableIf_t<
                !traits::NkIsSame_v<traits::NkDecay_t<F>, NkFunction>
            >
        >
        NkFunction& operator=(F&& f) {
            NkFunction temp(traits::NkForward<F>(f), m_allocator);
            Swap(temp);
            return *this;
        }

        /// Affectation depuis un binding (obj + méthode non-const)
        template<typename T>
        NkFunction& operator=(NkPair<T*, R (T::*)(Args...)> binding) {
            NkFunction temp(binding.first, binding.second, m_allocator);
            Swap(temp);
            return *this;
        }

        /// Affectation depuis un binding (obj + méthode const)
        template<typename T>
        NkFunction& operator=(NkPair<T*, R (T::*)(Args...) const> binding) {
            NkFunction temp(binding.first, binding.second, m_allocator);
            Swap(temp);
            return *this;
        }

        // ── Binding multiple ──────────────────────────────────────────────────

        /// Ajoute ou remplace la méthode non-const à l'index donné
        template<typename T>
        void BindMethod(T* obj, R (T::*meth)(Args...), usize index) {
            if (!obj || !meth) { return; }
            if (!m_callable) {
                void* mem = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>),
                                                   alignof(MultiMethodCallableImpl<T>));
                if (!mem) { return; }
                m_callable = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
            }
            static_cast<MultiMethodCallableImpl<T>*>(m_callable)->AddMethod(meth, index);
        }

        /// Ajoute ou remplace la méthode const à l'index donné
        template<typename T>
        void BindMethod(T* obj, R (T::*meth)(Args...) const, usize index) {
            if (!obj || !meth) { return; }
            if (!m_callable) {
                void* mem = m_allocator->Allocate(sizeof(MultiMethodCallableImpl<T>),
                                                   alignof(MultiMethodCallableImpl<T>));
                if (!mem) { return; }
                m_callable = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
            }
            static_cast<MultiMethodCallableImpl<T>*>(m_callable)->AddConstMethod(meth, index);
        }

        // ── Invocation ────────────────────────────────────────────────────────

        /// Invocation standard — retourne R{} silencieusement si vide
        R operator()(Args... args) const {
            if (!m_callable) {
#ifdef NKENTSEU_LOG_ENABLED
                NKENTSEU_LOG_ERROR("NkFunction::operator(): empty function call.");
#endif
                if constexpr (traits::NkIsVoid_v<R>) { return; }
                else { return R{}; }
            }
            return m_callable->Invoke(traits::NkForward<Args>(args)...);
        }

        /// Invocation indexée (binding multiple)
        R operator()(usize index, Args... args) const {
            if (!m_callable) {
#ifdef NKENTSEU_LOG_ENABLED
                NKENTSEU_LOG_ERROR("NkFunction::operator(): empty (index %zu).", index);
#endif
                if constexpr (traits::NkIsVoid_v<R>) { return; }
                else { return R{}; }
            }
            return m_callable->InvokeWithIndex(index, traits::NkForward<Args>(args)...);
        }

        // ── Test de validité ──────────────────────────────────────────────────

        /// Vrai si la fonction contient un callable
        explicit operator bool() const noexcept { return m_callable != nullptr; }

        bool IsValid() const noexcept { return m_callable != nullptr; }

        /// Comparaison avec nullptr (syntaxe STL-like)
        friend bool operator==(const NkFunction& fn, NkNullptrT) noexcept {
            return fn.m_callable == nullptr;
        }

        friend bool operator==(NkNullptrT, const NkFunction& fn) noexcept {
            return fn.m_callable == nullptr;
        }

        friend bool operator!=(const NkFunction& fn, NkNullptrT) noexcept {
            return fn.m_callable != nullptr;
        }

        friend bool operator!=(NkNullptrT, const NkFunction& fn) noexcept {
            return fn.m_callable != nullptr;
        }

        // ── Utilitaires ───────────────────────────────────────────────────────

        /// Échange le contenu de deux NkFunction (no-alloc)
        void Swap(NkFunction& other) noexcept {
            mem::NkSwap(m_callable,  other.m_callable);
            mem::NkSwap(m_allocator, other.m_allocator);
        }

        /// Vide la fonction (détruit et désalloue le callable)
        void Clear() noexcept {
            if (m_callable) {
                mem::NkAllocator* alloc =
                    detail::NkResolveFunctionAllocator(m_allocator);
                m_callable->Destroy(alloc);
                m_callable = nullptr;
            }
        }

    private:

        CallableBase*     m_callable;  ///< Callable alloué (nullptr = vide)
        mem::NkAllocator* m_allocator; ///< Allocateur du callable

    }; // class NkFunction<R(Args...)>


    // =========================================================================
    // NkSwap non-membre — permet std::swap(fn1, fn2) via ADL
    // =========================================================================

    template<typename R, typename... Args>
    void NkSwap(NkFunction<R(Args...)>& a, NkFunction<R(Args...)>& b) noexcept {
        a.Swap(b);
    }

} // namespace nkentseu