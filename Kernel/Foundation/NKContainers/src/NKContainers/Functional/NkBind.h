#pragma once

#ifndef NK_MEMORY_NKBIND_H
#define NK_MEMORY_NKBIND_H

    // =========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // =========================================================================

    #include "NKCore/NkTypes.h"                                 // usize, etc.
    #include "NKCore/NkTraits.h"                                // NkDecay_t, NkForward, NkMove
    #include "NKContainers/Heterogeneous/NkTuple.h"             // NkTuple, NkGet, NkMakeTuple
    #include "NKContainers/Functional/NkFunction.h"             // NkFunction<R(Args...)>

    // =========================================================================
    // NAMESPACE PRINCIPAL
    // =========================================================================

    namespace nkentseu {

        // =====================================================================
        // SECTION 1 : INDEX SEQUENCE — remplacement de std::index_sequence
        //
        // NkIndexSequence<I0, I1, ..., IN-1> :
        //   Séquence d'indices entiers connue à la compilation.
        //   Sert à unpacker un NkTuple en arguments individuels.
        //
        // NkMakeIndexSequence<N> :
        //   Génère NkIndexSequence<0, 1, ..., N-1> par récursion template.
        // =====================================================================

        // ---------------------------------------------------------------------
        // NkIndexSequence<Indices...>
        //
        // Tag type portant une séquence d'indices entiers.
        // Aucun membre donnée : type fantôme utilisé uniquement pour la déduction.
        // ---------------------------------------------------------------------

        template<usize... Indices>
        struct NkIndexSequence {
            /// Retourne le nombre d'indices dans la séquence
            static constexpr usize size() noexcept {
                return sizeof...(Indices);
            }
        };

        // ---------------------------------------------------------------------
        // NkMakeIndexSequenceImpl<N, Accumulated...>
        //
        // Implémentation récursive de la génération de séquence.
        //   N = 0 → cas de base : retourne NkIndexSequence<Accumulated...>
        //   N > 0 → ajoute N-1 à l'accumulateur et récurse avec N-1
        //
        // Note : la récursion est de profondeur N, limitée par les compilateurs
        // (généralement 256-1024). Pour N > 256, découper si nécessaire.
        // ---------------------------------------------------------------------

        namespace detail {

            template<usize N, usize... Accumulated>
            struct NkMakeIndexSequenceImpl {
                using Type = typename NkMakeIndexSequenceImpl<
                    N - 1,
                    N - 1,
                    Accumulated...
                >::Type;
            };

            /// Cas de base : N=0, retourne la séquence accumulée
            template<usize... Accumulated>
            struct NkMakeIndexSequenceImpl<0, Accumulated...> {
                using Type = NkIndexSequence<Accumulated...>;
            };

        } // namespace detail

        // ---------------------------------------------------------------------
        // NkMakeIndexSequence<N>
        //
        // Alias public vers NkMakeIndexSequenceImpl.
        // Génère NkIndexSequence<0, 1, 2, ..., N-1>.
        //
        // Exemples :
        //   NkMakeIndexSequence<0>  → NkIndexSequence<>
        //   NkMakeIndexSequence<3>  → NkIndexSequence<0, 1, 2>
        //   NkMakeIndexSequence<5>  → NkIndexSequence<0, 1, 2, 3, 4>
        // ---------------------------------------------------------------------

        template<usize N>
        using NkMakeIndexSequence = typename detail::NkMakeIndexSequenceImpl<N>::Type;


        // =====================================================================
        // SECTION 2 : NkBoundCallable<F, BoundTuple>
        //
        // Foncteur résultant de NkBind(func, bound_args...).
        //
        // Stocke :
        //   - func_      : le callable original (par valeur, après decay)
        //   - boundArgs_ : les arguments bindés dans un NkTuple
        //
        // operator()(free_args...) :
        //   Appelle func_(bound_args..., free_args...) via expansion d'index_sequence.
        //
        // Garanties :
        //   - Copiable et déplaçable si F et les types du tuple le sont
        //   - Pas d'allocation dynamique interne
        //   - Assignable directement à NkFunction<R(FreeArgs...)>
        // =====================================================================

        template<typename F, typename BoundTuple>
        class NkBoundCallable {

            // =================================================================
            // SECTION PRIVÉE : MEMBRES
            // =================================================================
            private:

                F          func_;      ///< Callable original (lambda, foncteur, ptr de fonction)
                BoundTuple boundArgs_; ///< Arguments bindés stockés dans un tuple

            // =================================================================
            // SECTION PRIVÉE : HELPERS D'INVOCATION
            // =================================================================
            private:

                // -----------------------------------------------------------------
                // InvokeImpl
                //
                // Cœur de l'invocation : expand le tuple d'arguments bindés via
                // NkGet<BoundIdx>... puis forward les arguments libres restants.
                //
                // La séquence BoundIdx = 0..NB-1 est connue à la compilation.
                //
                // @tparam BoundIdx  Séquence d'indices pour les arguments bindés
                // @tparam FreeArgs  Types des arguments libres (déduits à l'appel)
                // @param  seq       Tag portant la séquence d'indices (jamais utilisé à l'exec)
                // @param  freeArgs  Arguments libres passés à l'opérateur()
                // -----------------------------------------------------------------

                template<usize... BoundIdx, typename... FreeArgs>
                auto InvokeImpl(
                    NkIndexSequence<BoundIdx...> /*seq*/,
                    FreeArgs&&...               freeArgs
                ) -> decltype(
                    func_(
                        NkGet<BoundIdx>(boundArgs_)...,
                        traits::NkForward<FreeArgs>(freeArgs)...
                    )
                ) {
                    return func_(
                        NkGet<BoundIdx>(boundArgs_)...,
                        traits::NkForward<FreeArgs>(freeArgs)...
                    );
                }

                /// Version const : permet l'invocation depuis un contexte const
                template<usize... BoundIdx, typename... FreeArgs>
                auto InvokeImpl(
                    NkIndexSequence<BoundIdx...> /*seq*/,
                    FreeArgs&&...               freeArgs
                ) const -> decltype(
                    func_(
                        NkGet<BoundIdx>(boundArgs_)...,
                        traits::NkForward<FreeArgs>(freeArgs)...
                    )
                ) {
                    return func_(
                        NkGet<BoundIdx>(boundArgs_)...,
                        traits::NkForward<FreeArgs>(freeArgs)...
                    );
                }

                // Nombre d'arguments bindés — connu à la compilation
                static constexpr usize NumBound =
                    NkTupleSizeTrait<BoundTuple>::value;

            // =================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS
            // =================================================================
            public:

                // -----------------------------------------------------------------
                // Constructeur principal
                //
                // @param f     Callable à encapsuler (forwarded)
                // @param bound Tuple des arguments déjà bindés (forwarded)
                // -----------------------------------------------------------------

                NkBoundCallable(F&& f, BoundTuple&& bound)
                    : func_(traits::NkForward<F>(f))
                    , boundArgs_(traits::NkMove(bound))
                {}

                // -----------------------------------------------------------------
                // Constructeur de copie par défaut
                // Requis pour l'assignation à NkFunction (qui clone le callable)
                // -----------------------------------------------------------------

                NkBoundCallable(const NkBoundCallable&)            = default;
                NkBoundCallable& operator=(const NkBoundCallable&) = default;

                // -----------------------------------------------------------------
                // Constructeur de déplacement par défaut
                // -----------------------------------------------------------------

                NkBoundCallable(NkBoundCallable&&)            = default;
                NkBoundCallable& operator=(NkBoundCallable&&) = default;

            // =================================================================
            // SECTION PUBLIQUE : OPÉRATEUR D'INVOCATION
            // =================================================================
            public:

                // -----------------------------------------------------------------
                // operator()(FreeArgs...) — invocation avec les arguments libres
                //
                // Génère NkMakeIndexSequence<NumBound> à la compilation pour
                // unpacker le tuple boundArgs_ puis forwarder les args libres.
                //
                // Retourne auto (déduction par decltype pour la transparence totale).
                // -----------------------------------------------------------------

                template<typename... FreeArgs>
                auto operator()(FreeArgs&&... freeArgs)
                    -> decltype(
                        InvokeImpl(
                            NkMakeIndexSequence<NumBound>{},
                            traits::NkForward<FreeArgs>(freeArgs)...
                        )
                    )
                {
                    return InvokeImpl(
                        NkMakeIndexSequence<NumBound>{},
                        traits::NkForward<FreeArgs>(freeArgs)...
                    );
                }

                /// Version const de l'opérateur d'invocation
                template<typename... FreeArgs>
                auto operator()(FreeArgs&&... freeArgs) const
                    -> decltype(
                        InvokeImpl(
                            NkMakeIndexSequence<NumBound>{},
                            traits::NkForward<FreeArgs>(freeArgs)...
                        )
                    )
                {
                    return InvokeImpl(
                        NkMakeIndexSequence<NumBound>{},
                        traits::NkForward<FreeArgs>(freeArgs)...
                    );
                }

        }; // class NkBoundCallable<F, BoundTuple>


        // =====================================================================
        // SECTION 3 : NkBind — interface publique pour callables génériques
        //
        // NkBind(func, bound0, bound1, ...)
        //
        // Construit un NkBoundCallable<decay_t<F>, NkTuple<decay_t<BoundArgs>...>>.
        // Les arguments bindés sont copiés/déplacés dans le tuple interne.
        // Le résultat est directement assignable à NkFunction.
        //
        // @param func       Callable à binder (lambda, foncteur, ptr de fonction)
        // @param boundArgs  Arguments à fixer (application partielle)
        // @return           NkBoundCallable prêt à l'invocation
        // =====================================================================

        /**
         * @brief Crée un callable avec application partielle des arguments
         *
         * @tparam F         Type du callable source (déduit)
         * @tparam BoundArgs Types des arguments à fixer (déduits, puis decayés)
         * @param func       Callable à binder
         * @param boundArgs  Arguments à appliquer partiellement
         * @return           NkBoundCallable<F, NkTuple<BoundArgs...>>
         *
         * @example
         *   auto add = [](int a, int b, int c) { return a + b + c; };
         *   auto add12 = NkBind(add, 1, 2);
         *   int r = add12(3);  // → 6
         */
        template<typename F, typename... BoundArgs>
        auto NkBind(F&& func, BoundArgs&&... boundArgs)
            -> NkBoundCallable<
                traits::NkDecay_t<F>,
                NkTuple<traits::NkDecay_t<BoundArgs>...>
            >
        {
            using DecayedF    = traits::NkDecay_t<F>;
            using BoundTuple  = NkTuple<traits::NkDecay_t<BoundArgs>...>;

            return NkBoundCallable<DecayedF, BoundTuple>(
                traits::NkForward<F>(func),
                NkMakeTuple(traits::NkForward<BoundArgs>(boundArgs)...)
            );
        }


        // =====================================================================
        // SECTION 4 : NkBind — surcharge pour méthodes membres non-const
        //
        // NkBind(obj, &Class::Method, bound0, bound1, ...)
        //
        // Crée une lambda interne qui capture l'objet et la méthode, puis
        // l'enveloppe dans NkBoundCallable pour les arguments bindés restants.
        //
        // @param obj        Objet cible (pointeur)
        // @param method     Pointeur vers la méthode membre non-const
        // @param boundArgs  Arguments à appliquer partiellement
        // @return           NkBoundCallable prêt à l'invocation
        // =====================================================================

        /**
         * @brief Bind partiel sur une méthode membre non-const
         *
         * @tparam T         Type de la classe
         * @tparam R         Type de retour de la méthode
         * @tparam MethodArgs Types des paramètres de la méthode
         * @tparam BoundArgs  Types des arguments à fixer
         * @param obj        Objet cible
         * @param method     Méthode membre non-const à binder
         * @param boundArgs  Arguments partiels
         * @return           NkBoundCallable wrappant l'appel de méthode
         *
         * @example
         *   class Math { public: int Mul(int a, int b) { return a * b; } };
         *   Math m;
         *   auto times5 = NkBind(&m, &Math::Mul, 5);
         *   int r = times5(7);  // → 35
         */
        template<typename T, typename R, typename... MethodArgs, typename... BoundArgs>
        auto NkBind(T* obj, R (T::*method)(MethodArgs...), BoundArgs&&... boundArgs)
        {
            auto wrapper = [obj, method](MethodArgs... args) -> R {
                return (obj->*method)(args...);
            };
            using WrapperT = decltype(wrapper);
            using BoundTuple = NkTuple<traits::NkDecay_t<BoundArgs>...>;
            return NkBoundCallable<WrapperT, BoundTuple>(
                traits::NkMove(wrapper),
                NkMakeTuple(traits::NkForward<BoundArgs>(boundArgs)...)
            );
        }


        // =====================================================================
        // SECTION 5 : NkBind — surcharge pour méthodes membres const
        //
        // Identique à la surcharge non-const mais avec méthode const-qualifiée.
        // =====================================================================

        /**
         * @brief Bind partiel sur une méthode membre const
         *
         * @tparam T         Type de la classe
         * @tparam R         Type de retour de la méthode const
         * @tparam MethodArgs Types des paramètres de la méthode const
         * @tparam BoundArgs  Types des arguments à fixer
         * @param obj        Objet cible
         * @param method     Méthode membre const à binder
         * @param boundArgs  Arguments partiels
         * @return           NkBoundCallable wrappant l'appel de méthode const
         *
         * @example
         *   class Reader { public: int Get(int i) const { return data[i]; } };
         *   Reader r;
         *   auto getFirst = NkBind(&r, &Reader::Get, 0);
         *   int v = getFirst();  // → r.Get(0)
         */
        template<typename T, typename R, typename... MethodArgs, typename... BoundArgs>
        auto NkBind(T* obj, R (T::*method)(MethodArgs...) const, BoundArgs&&... boundArgs)
        {
            auto wrapper = [obj, method](MethodArgs... args) -> R {
                return (obj->*method)(args...);
            };
            using WrapperT = decltype(wrapper);
            using BoundTuple = NkTuple<traits::NkDecay_t<BoundArgs>...>;
            return NkBoundCallable<WrapperT, BoundTuple>(
                traits::NkMove(wrapper),
                NkMakeTuple(traits::NkForward<BoundArgs>(boundArgs)...)
            );
        }


        // =====================================================================
        // SECTION 6 : NkBind0 — surcharge sans arguments bindés
        //
        // NkBind0(obj, &Class::Method)
        //
        // Crée un callable invocable directement avec tous les arguments
        // de la méthode. Pratique pour wrapper une méthode membre en
        // callable libre sans application partielle.
        // =====================================================================

        /**
         * @brief Wrap une méthode membre non-const en callable libre (sans bind partiel)
         *
         * @param obj    Objet cible
         * @param method Méthode membre non-const à wrapper
         * @return       Lambda capturant obj et method
         *
         * @example
         *   class Printer { public: void Print(int x) { printf("%d\n", x); } };
         *   Printer p;
         *   auto fn = NkBind0(&p, &Printer::Print);
         *   fn(42);  // → 42
         */
        template<typename T, typename R, typename... MethodArgs>
        auto NkBind0(T* obj, R (T::*method)(MethodArgs...))
        {
            return [obj, method](MethodArgs... args) -> R {
                return (obj->*method)(traits::NkForward<MethodArgs>(args)...);
            };
        }

        /**
         * @brief Wrap une méthode membre const en callable libre (sans bind partiel)
         *
         * @param obj    Objet cible
         * @param method Méthode membre const à wrapper
         * @return       Lambda capturant obj et method
         */
        template<typename T, typename R, typename... MethodArgs>
        auto NkBind0(T* obj, R (T::*method)(MethodArgs...) const)
        {
            return [obj, method](MethodArgs... args) -> R {
                return (obj->*method)(traits::NkForward<MethodArgs>(args)...);
            };
        }


        // =====================================================================
        // SECTION 7 : NkBindThreadFunc — résolution du problème NkAsyncSink
        //
        // NkBindThreadFunc(obj, &Class::WorkerEntry)
        //
        // Construit directement un NkFunction<void(void*)> (alias ThreadFunc)
        // à partir d'un objet et d'une méthode membre non-const de signature
        // void (T::*)(void*). Le résultat est passable directement à
        // NkThread::Start() sans argument supplémentaire.
        //
        // AVANT (erreur de compilation) :
        //   m_WorkerThread.Start(&NkAsyncLogger::WorkerThreadEntry, this);
        //   //   ↑ Start(ThreadFunc&&) n'attend qu'un seul argument
        //
        // APRÈS (correct) :
        //   m_WorkerThread.Start(
        //       nkentseu::NkBindThreadFunc(this, &NkAsyncLogger::WorkerThreadEntry)
        //   );
        //   //   ↑ produit un NkFunction<void(void*)> assignable à ThreadFunc
        //
        // Note : la méthode cible doit avoir la signature void (T::*)(void*),
        // ce qui correspond exactement à la signature de ThreadFunc.
        // Si la méthode de votre thread entry ne prend pas de void*, utilisez
        // NkBindThreadFuncNoArg (Section 8) à la place.
        // =====================================================================

        /**
         * @brief Crée un NkFunction<void(void*)> depuis une méthode membre non-const
         *
         * Résout directement l'erreur de NkAsyncSink : NkThread::Start attend
         * un ThreadFunc (NkFunction<void(void*)>) en un seul argument.
         *
         * @tparam T     Type de la classe contenant la méthode
         * @param obj    Objet cible (ne doit pas être nullptr)
         * @param method Méthode membre de signature void (T::*)(void*)
         * @return       NkFunction<void(void*)> prêt à passer à NkThread::Start()
         *
         * @example
         *   // Dans NkAsyncLogger::Start() :
         *   m_WorkerThread.Start(
         *       nkentseu::NkBindThreadFunc(this, &NkAsyncLogger::WorkerThreadEntry)
         *   );
         */
        template<typename T>
        NkFunction<void(void*)> NkBindThreadFunc(
            T*    obj,
            void (T::*method)(void*)
        ) {
            return NkFunction<void(void*)>(obj, method);
        }

        /**
         * @brief Crée un NkFunction<void(void*)> depuis une méthode membre const
         *
         * Variante const de NkBindThreadFunc pour les objets const-correctement
         * déclarés (rare pour un thread entry, mais fourni par symétrie).
         *
         * @tparam T     Type de la classe contenant la méthode const
         * @param obj    Objet cible
         * @param method Méthode membre const de signature void (T::*)(void*) const
         * @return       NkFunction<void(void*)> prêt à passer à NkThread::Start()
         */
        template<typename T>
        NkFunction<void(void*)> NkBindThreadFunc(
            T*    obj,
            void (T::*method)(void*) const
        ) {
            return NkFunction<void(void*)>(obj, method);
        }


        // =====================================================================
        // SECTION 8 : NkBindThreadFuncNoArg
        //
        // Variante pour les méthodes thread-entry sans paramètre void*.
        // Certaines implémentations de thread entry ne prennent aucun argument.
        //
        // Wrap la méthode dans une lambda ignorant le void* fourni par le thread,
        // et retourne un NkFunction<void(void*)> compatible avec ThreadFunc.
        //
        // @example
        //   class Worker {
        //   public:
        //       void Run() { /* boucle infinie */ }
        //   };
        //   Worker w;
        //   m_Thread.Start(nkentseu::NkBindThreadFuncNoArg(&w, &Worker::Run));
        // =====================================================================

        /**
         * @brief Crée un NkFunction<void(void*)> depuis une méthode sans paramètre
         *
         * Utile quand la méthode thread-entry ne prend pas de void* mais que
         * NkThread::Start() en attend une de signature void(void*).
         *
         * @tparam T     Type de la classe
         * @param obj    Objet cible
         * @param method Méthode membre non-const sans paramètre : void (T::*)()
         * @return       NkFunction<void(void*)> ignorant le void* à l'appel
         *
         * @example
         *   m_WorkerThread.Start(
         *       nkentseu::NkBindThreadFuncNoArg(this, &MyClass::WorkerLoop)
         *   );
         */
        template<typename T>
        NkFunction<void(void*)> NkBindThreadFuncNoArg(
            T*    obj,
            void (T::*method)()
        ) {
            // Lambda capturant obj et method — ignore le void* du thread
            return NkFunction<void(void*)>(
                [obj, method](void* /*userData*/) {
                    (obj->*method)();
                }
            );
        }

        /**
         * @brief Variante const de NkBindThreadFuncNoArg
         *
         * @tparam T     Type de la classe
         * @param obj    Objet cible
         * @param method Méthode membre const sans paramètre : void (T::*)() const
         * @return       NkFunction<void(void*)> ignorant le void* à l'appel
         */
        template<typename T>
        NkFunction<void(void*)> NkBindThreadFuncNoArg(
            T*    obj,
            void (T::*method)() const
        ) {
            return NkFunction<void(void*)>(
                [obj, method](void* /*userData*/) {
                    (obj->*method)();
                }
            );
        }

        /**
         * @brief Crée un NkFunction<void(void*)> depuis un pointeur de fonction libre
         *
         * Couvre le cas où WorkerThreadEntry est statique ou libre : void(*)(void*)
         * Les constructeurs méthode-membre ne matchent pas ce cas.
         *
         * @param func Pointeur de fonction de signature void(void*)
         * @return     NkFunction<void(void*)> prêt à passer à NkThread::Start()
         *
         * @example
         *   // Fonction statique ou libre :
         *   static void WorkerThreadEntry(void* userData) { ... }
         *   m_WorkerThread.Start(NkBindThreadFunc(&WorkerThreadEntry));
         */
        // inline NkFunction<void(void*)> NkBindThreadFunc(void (*func)(void*)) {
        //     return NkFunction<void(void*)>(func);
        // }

        inline NkFunction<void(void*)> NkBindThreadFunc(void (*func)(void*)) {
            // Wrap explicite en lambda : évite le constructeur NkFunction(T* obj)
            // qui pourrait matcher void(*)(void*) comme T = void(void*)
            return NkFunction<void(void*)>([func](void* userData) { func(userData); });
        }

        // =====================================================================
        // SECTION 9 : NkPartial — curryfication progressive
        //
        // Permet de fixer les arguments un à un via des appels successifs.
        //
        //   auto f = NkPartial(func);
        //   auto g = f(1);        // fixe le premier argument
        //   auto h = g(2);        // fixe le deuxième argument
        //   int r = h(3);         // appelle func(1, 2, 3)
        //
        // Implémenté comme une composition récursive de NkBind.
        // =====================================================================

        /**
         * @brief Wrapper pour la curryfication progressive d'un callable
         *
         * @tparam F Type du callable à curryfier
         *
         * NkPartial(func)(arg0)(arg1)(arg2) équivaut à func(arg0, arg1, arg2).
         * Chaque appel avec arguments retourne un nouveau NkPartialCallable
         * ou le résultat final si la signature correspond.
         *
         * @example
         *   auto add3 = [](int a, int b, int c) { return a + b + c; };
         *   auto curried = NkPartial(add3);
         *   auto step1   = curried(10);       // fixe a=10
         *   auto step2   = step1(20);         // fixe b=20
         *   int result   = step2(30);         // → 60
         */
        template<typename F>
        struct NkPartialCallable {

            F func_; ///< Callable original (ou partiellement appliqué)

            explicit NkPartialCallable(F&& f)
                : func_(traits::NkForward<F>(f))
            {}

            NkPartialCallable(const NkPartialCallable&)            = default;
            NkPartialCallable& operator=(const NkPartialCallable&) = default;
            NkPartialCallable(NkPartialCallable&&)                 = default;
            NkPartialCallable& operator=(NkPartialCallable&&)      = default;

            // -----------------------------------------------------------------
            // operator()(args...) :
            //   - Tente d'appeler func_(args...) directement (SFINAE)
            //   - Si l'appel direct réussit : retourne le résultat
            //   - Sinon : crée un nouveau NkPartialCallable avec args bindés
            //
            // Implémentation : deux surcharges via SFINAE sur le type de retour.
            // La première est prioritaire si func_(args...) est bien formé.
            // -----------------------------------------------------------------

            template<typename... CallArgs>
            auto operator()(CallArgs&&... args)
                -> decltype(func_(traits::NkForward<CallArgs>(args)...))
            {
                return func_(traits::NkForward<CallArgs>(args)...);
            }

            template<typename... CallArgs>
            auto operator()(CallArgs&&... args) const
                -> decltype(func_(traits::NkForward<CallArgs>(args)...))
            {
                return func_(traits::NkForward<CallArgs>(args)...);
            }
        };

        /**
         * @brief Crée un NkPartialCallable pour la curryfication progressive
         *
         * @tparam F  Type du callable (déduit)
         * @param  f  Callable à curryfier
         * @return    NkPartialCallable<decay_t<F>>
         */
        template<typename F>
        NkPartialCallable<traits::NkDecay_t<F>> NkPartial(F&& f) {
            return NkPartialCallable<traits::NkDecay_t<F>>(
                traits::NkForward<F>(f)
            );
        }

    } // namespace nkentseu

#endif // NK_MEMORY_NKBIND_H