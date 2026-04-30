// =============================================================================
// NKCore/NkInvoke.h
// Implémentation portable de std::invoke avec détection compile-time.
//
// Design :
//  - Fonction NkInvoke() inspirée de std::invoke (C++17) pour appel uniforme
//  - Support des fonctions libres, pointeurs de fonction, lambdas, functors
//  - Support des pointeurs de membre (fonctions et données) avec résolution d'objet
//  - Helper detail::NkInvokeObject() pour normalisation référence/pointeur
//  - Alias NkInvokeResult_t pour déduction du type de retour
//  - Trait NkIsInvocable<F, Args...> pour vérification compile-time
//  - Variable template NkIsInvocable_v pour syntaxe concise C++17+
//  - Intégration avec NkTraits.h pour NkEnableIf_t, NkForward, etc.
//
// Intégration :
//  - Utilise NKCore/NkTraits.h pour les utilitaires de métaprogrammation
//  - Utilise <type_traits> pour std::is_member_function_pointer_v, etc.
//  - Compatible C++14+ avec support constexpr et noexcept propagation
//  - Header-only : aucune implémentation .cpp requise
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKINVOKE_H_INCLUDED
#define NKENTSEU_CORE_NKINVOKE_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des modules de traits et des en-têtes standards.

    #include "NkTraits.h"
    #include <type_traits>

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : NAMESPACE DETAIL - HELPERS INTERNES
        // ====================================================================
        // Fonctions utilitaires non destinées à un usage direct par l'utilisateur.

        /**
         * @defgroup InvokeDetailHelpers Helpers Internes pour NkInvoke
         * @brief Fonctions de support pour la résolution d'objets dans NkInvoke
         * @ingroup InvokeUtilities
         * @internal
         *
         * Ces fonctions normalisent l'accès à un objet qu'il soit passé
         * par référence ou par pointeur, facilitant l'implémentation des
         * surcharges de NkInvoke pour les pointeurs de membre.
         *
         * @note
         *   - Ne pas utiliser directement : API interne sujette à changement
         *   - Toutes les fonctions sont constexpr et noexcept pour optimisation
         *   - La surcharge const préserve la constance de l'objet d'origine
         */
        /** @{ */

        namespace detail {

            /**
             * @brief Retourne la référence vers un objet passé par référence
             * @tparam T Type de l'objet (déduit automatiquement)
             * @param object Référence vers l'objet à retourner
             * @return Référence vers l'objet (identité)
             * @ingroup InvokeDetailHelpers
             */
            template <typename T>
            constexpr T& NkInvokeObject(T& object) noexcept {
                return object;
            }

            /**
             * @brief Déréférence un pointeur vers un objet non-const
             * @tparam T Type pointé (déduit automatiquement)
             * @param object Pointeur vers l'objet à déréférencer
             * @return Référence non-const vers l'objet pointé
             * @warning Comportement indéfini si object est nullptr
             * @ingroup InvokeDetailHelpers
             */
            template <typename T>
            constexpr T& NkInvokeObject(T* object) noexcept {
                return *object;
            }

            /**
             * @brief Déréférence un pointeur vers un objet const
             * @tparam T Type pointé (déduit automatiquement)
             * @param object Pointeur const vers l'objet à déréférencer
             * @return Référence const vers l'objet pointé
             * @warning Comportement indéfini si object est nullptr
             * @ingroup InvokeDetailHelpers
             */
            template <typename T>
            constexpr const T& NkInvokeObject(const T* object) noexcept {
                return *object;
            }

        } // namespace detail

        /** @} */ // End of InvokeDetailHelpers

        // ====================================================================
        // SECTION 4 : FONCTION NKINVOKE - SURCHARGES PRINCIPALES
        // ====================================================================
        // Implémentation de l'appel uniforme pour différents types d'invocables.

        /**
         * @defgroup NkInvokeFunction Fonction d'Invocation Uniforme
         * @brief Surcharge de NkInvoke pour différents types d'appelables
         * @ingroup InvokeUtilities
         *
         * NkInvoke fournit une interface unique pour appeler :
         *   - Fonctions libres, pointeurs de fonction, lambdas, functors
         *   - Pointeurs de membre fonction avec objet ou pointeur d'objet
         *   - Pointeurs de membre données avec objet ou pointeur d'objet
         *
         * @note
         *   - Inspiré de std::invoke (C++17) mais portable C++14+
         *   - Utilise SFINAE via NkEnableIf_t pour sélectionner la bonne surcharge
         *   - Propagation noexcept : l'appel est noexcept si l'appel sous-jacent l'est
         *   - Perfect forwarding via NkForward pour préserver les catégories de valeur
         *
         * @warning
         *   Pour les pointeurs de membre, l'objet doit être valide (non-null).
         *   Aucun check runtime n'est effectué : comportement indéfini sinon.
         *
         * @example
         * @code
         * // Appel de fonction libre
         * int Add(int a, int b) { return a + b; }
         * auto result = nkentseu::NkInvoke(Add, 3, 4);  // result == 7
         *
         * // Appel de lambda avec capture
         * auto lambda = [factor = 2](int x) { return x * factor; };
         * auto doubled = nkentseu::NkInvoke(lambda, 5);  // doubled == 10
         *
         * // Appel de méthode membre via pointeur d'objet
         * class Calculator {
         * public:
         *     int Multiply(int x, int y) { return x * y; }
         * };
         * Calculator calc;
         * auto product = nkentseu::NkInvoke(&Calculator::Multiply, &calc, 6, 7);
         * // product == 42
         *
         * // Accès à un membre de données via référence
         * struct Point { int x = 10, y = 20; };
         * Point p;
         * auto& xRef = nkentseu::NkInvoke(&Point::x, p);  // xRef == p.x
         * xRef = 100;  // Modifie p.x
         * @endcode
         */
        /** @{ */

        /**
         * @brief Appel d'un callable non-membre (fonction, lambda, functor)
         * @tparam F Type du callable (déduit)
         * @tparam Args Types des arguments (variadiques, déduits)
         * @param function Callable à invoquer
         * @param args Arguments à forwarder au callable
         * @return Résultat de l'appel, forwardé avec decltype(auto)
         * @note
         *   - Activé uniquement si F n'est PAS un pointeur de membre (fonction ou objet)
         *   - noexcept conditionnel : propagé depuis l'appel sous-jacent
         *   - Utilise NkForward pour perfect forwarding des arguments
         * @ingroup NkInvokeFunction
         */
        template <
            typename F,
            typename... Args,
            typename traits::NkEnableIf_t<
                !std::is_member_function_pointer_v<traits::NkRemoveReference_t<F>> &&
                !std::is_member_object_pointer_v<traits::NkRemoveReference_t<F>>,
                int
            > = 0
        >
        constexpr decltype(auto) NkInvoke(F&& function, Args&&... args)
            noexcept(noexcept(traits::NkForward<F>(function)(traits::NkForward<Args>(args)...)))
        {
            return traits::NkForward<F>(function)(traits::NkForward<Args>(args)...);
        }

        /**
         * @brief Appel d'un pointeur de membre fonction avec objet cible
         * @tparam MemFn Type du pointeur de membre fonction (déduit)
         * @tparam Obj Type de l'objet cible (référence ou pointeur, déduit)
         * @tparam Args Types des arguments de la méthode (variadiques, déduits)
         * @param memberFunction Pointeur de membre fonction à invoquer
         * @param object Objet ou pointeur vers l'objet sur lequel appeler
         * @param args Arguments à forwarder à la méthode membre
         * @return Résultat de l'appel de la méthode, forwardé avec decltype(auto)
         * @note
         *   - Activé uniquement si MemFn EST un pointeur de membre fonction
         *   - Utilise detail::NkInvokeObject pour normaliser objet/pointeur
         *   - noexcept conditionnel : propagé depuis l'appel de méthode sous-jacent
         * @ingroup NkInvokeFunction
         */
        template <
            typename MemFn,
            typename Obj,
            typename... Args,
            typename traits::NkEnableIf_t<
                std::is_member_function_pointer_v<traits::NkRemoveReference_t<MemFn>>,
                int
            > = 0
        >
        constexpr decltype(auto) NkInvoke(
            MemFn&& memberFunction,
            Obj&& object,
            Args&&... args
        ) noexcept(
            noexcept(
                (detail::NkInvokeObject(traits::NkForward<Obj>(object)).*memberFunction)(
                    traits::NkForward<Args>(args)...
                )
            )
        ) {
            return (detail::NkInvokeObject(traits::NkForward<Obj>(object)).*memberFunction)(
                traits::NkForward<Args>(args)...
            );
        }

        /**
         * @brief Accès à un pointeur de membre objet (donnée) avec objet cible
         * @tparam MemObj Type du pointeur de membre objet (déduit)
         * @tparam Obj Type de l'objet cible (référence ou pointeur, déduit)
         * @param memberObject Pointeur de membre objet à accéder
         * @param object Objet ou pointeur vers l'objet contenant le membre
         * @return Référence vers le membre de données accédé
         * @note
         *   - Activé uniquement si MemObj EST un pointeur de membre objet (donnée)
         *   - Retourne une référence : permet lecture et modification du membre
         *   - noexcept garanti : l'accès à un membre ne peut pas lever d'exception
         * @ingroup NkInvokeFunction
         */
        template <
            typename MemObj,
            typename Obj,
            typename traits::NkEnableIf_t<
                std::is_member_object_pointer_v<traits::NkRemoveReference_t<MemObj>>,
                int
            > = 0
        >
        constexpr decltype(auto) NkInvoke(
            MemObj&& memberObject,
            Obj&& object
        ) noexcept(
            noexcept(detail::NkInvokeObject(traits::NkForward<Obj>(object)).*memberObject)
        ) {
            return detail::NkInvokeObject(traits::NkForward<Obj>(object)).*memberObject;
        }

        /** @} */ // End of NkInvokeFunction

        // ====================================================================
        // SECTION 5 : ALIAS ET TRAITS ASSOCIÉS
        // ====================================================================
        // Utilitaires pour déduction de type et vérification compile-time.

        /**
         * @defgroup InvokeTraits Traits et Alias pour NkInvoke
         * @brief Utilitaires de métaprogrammation associés à NkInvoke
         * @ingroup InvokeUtilities
         *
         * Ces composants facilitent l'écriture de code générique utilisant NkInvoke :
         *   - NkInvokeResult_t : déduit le type de retour d'un appel NkInvoke
         *   - NkIsInvocable : vérifie compile-time si un appel est valide
         *   - NkIsInvocable_v : variable template pour syntaxe concise (C++17+)
         *
         * @note
         *   - Basé sur le pattern SFINAE avec fonctions Test() surchargées
         *   - Utilise traits::NkDeclVal pour simulation d'instances sans construction
         *   - Compatible avec les expressions noexcept et decltype(auto)
         *
         * @example
         * @code
         * // Déduction du type de retour d'un appel
         * template <typename F, typename... Args>
         * void Wrapper(F&& f, Args&&... args) {
         *     using Result = nkentseu::NkInvokeResult_t<F, Args...>;
         *     Result r = nkentseu::NkInvoke(f, args...);
         *     // ... utilisation de r ...
         * }
         *
         * // Vérification compile-time avant appel
         * template <typename F, typename... Args>
         * constexpr bool CanCall = nkentseu::NkIsInvocable_v<F, Args...>;
         *
         * static_assert(CanCall<decltype(&MyClass::Method), MyClass*, int>,
         *               "Method signature mismatch");
         * @endcode
         */
        /** @{ */

        /**
         * @brief Alias pour le type de retour de NkInvoke<F, Args...>
         * @tparam F Type du callable
         * @tparam Args Types des arguments
         * @ingroup InvokeTraits
         *
         * @note
         *   - Utilise decltype + NkInvoke + traits::NkDeclVal pour déduction sans évaluation
         *   - Résultat : type exact retourné par l'appel (avec références, cv-qualifiers)
         *   - Utile dans les templates pour déclarer des variables ou return type
         */
        template <typename F, typename... Args>
        using NkInvokeResult_t = decltype(
            NkInvoke(traits::NkDeclVal<F>(), traits::NkDeclVal<Args>()...)
        );

        /**
         * @brief Trait compile-time pour vérifier si F est invocable avec Args...
         * @tparam F Type du callable à tester
         * @tparam Args Types des arguments à tester
         * @ingroup InvokeTraits
         *
         * Ce trait utilise SFINAE pour détecter si l'expression
         * NkInvoke(declval<F>(), declval<Args>()...) est bien formée.
         *
         * @note
         *   - value = true si l'appel est valide, false sinon
         *   - Ne vérifie PAS les exceptions : uniquement la validité syntaxique
         *   - Utilise le pattern classique Test(int) / Test(...) pour SFINAE
         *
         * @warning
         *   Ce trait peut échouer à compiler si F ou Args contiennent des types
         *   incomplets. Assurez-vous que tous les types sont complets au point d'usage.
         */
        template <typename F, typename... Args>
        class NkIsInvocable {
            private:
                // Surcharge sélectionnée si NkInvoke<F, Args...> est bien formé
                template <typename Dummy = void>
                static auto Test(int)
                    -> decltype(
                        NkInvoke(traits::NkDeclVal<F>(), traits::NkDeclVal<Args>()...),
                        traits::NkTrueType{}
                    );

                // Fallback sélectionné si la surcharge principale est SFINAE-out
                template <typename Dummy = void>
                static auto Test(...) -> traits::NkFalseType;

            public:
                /** @brief true si F est invocable avec Args..., false sinon */
                static constexpr nk_bool value = decltype(Test<>(0))::value;
        };

        /**
         * @brief Variable template pour NkIsInvocable (syntaxe concise C++17+)
         * @tparam F Type du callable
         * @tparam Args Types des arguments
         * @ingroup InvokeTraits
         *
         * @note
         *   - Équivalent à NkIsInvocable<F, Args...>::value
         *   - Nécessite C++17 pour les variable templates
         *   - Pour C++14, utiliser NkIsInvocable<F, Args...>::value directement
         *
         * @example
         * @code
         * // C++17+ : syntaxe concise
         * if constexpr (nkentseu::NkIsInvocable_v<F, int, std::string>) {
         *     // F peut être appelé avec (int, std::string)
         * }
         *
         * // C++14 : syntaxe classique
         * typedef nkentseu::NkIsInvocable<F, int, std::string> Trait;
         * if (Trait::value) { /\* ... *\/ }
         * @endcode
         */
        template <typename F, typename... Args>
        inline constexpr nk_bool NkIsInvocable_v = NkIsInvocable<F, Args...>::value;

        /** @} */ // End of InvokeTraits

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKINVOKE_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKINVOKE.H
// =============================================================================
// Ce fichier fournit une implémentation portable de std::invoke pour l'appel
// uniforme de fonctions, méthodes et accès aux membres.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Appel de fonctions libres et lambdas
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkInvoke.h"
    #include <iostream>

    // Fonction libre simple
    int Add(int a, int b) {
        return a + b;
    }

    // Functor avec état
    struct Multiplier {
        int factor;
        int operator()(int x) const { return x * factor; }
    };

    void TestFreeFunctions() {
        using namespace nkentseu;

        // Appel de fonction libre par nom
        int sum = NkInvoke(Add, 10, 20);  // sum == 30
        std::cout << "Sum: " << sum << std::endl;

        // Appel de pointeur de fonction
        int (*funcPtr)(int, int) = &Add;
        int sum2 = NkInvoke(funcPtr, 5, 7);  // sum2 == 12

        // Appel de lambda avec capture
        auto lambda = [offset = 100](int x) { return x + offset; };
        int shifted = NkInvoke(lambda, 42);  // shifted == 142

        // Appel de functor
        Multiplier times3{3};
        int product = NkInvoke(times3, 15);  // product == 45
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Appel de méthodes membres via pointeur de membre
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkInvoke.h"
    #include <iostream>
    #include <string>

    class Person {
    public:
        Person(std::string name, int age) : m_name(std::move(name)), m_age(age) {}

        std::string GetGreeting() const {
            return "Hello, I'm " + m_name + ", age " + std::to_string(m_age);
        }

        void SetAge(int newAge) {
            if (newAge >= 0) { m_age = newAge; }
        }

        int GetAge() const { return m_age; }

    private:
        std::string m_name;
        int m_age;
    };

    void TestMemberFunctions() {
        using namespace nkentseu;

        Person alice("Alice", 30);

        // Appel de méthode const via pointeur d'objet
        auto greeting = NkInvoke(&Person::GetGreeting, &alice);
        std::cout << greeting << std::endl;  // "Hello, I'm Alice, age 30"

        // Appel de méthode non-const via référence
        NkInvoke(&Person::SetAge, alice, 31);  // Modifie l'âge d'Alice

        // Lecture du membre de données via pointeur de membre objet
        const int& ageRef = NkInvoke(&Person::m_age, alice);
        std::cout << "Age by reference: " << ageRef << std::endl;  // 31

        // Modification via référence retournée par NkInvoke sur membre objet
        int& ageMod = NkInvoke(&Person::m_age, alice);
        ageMod = 32;  // Modifie directement alice.m_age
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Code générique avec NkInvokeResult_t et NkIsInvocable_v
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkInvoke.h"
    #include <type_traits>
    #include <utility>

    namespace nkentseu {

        // Wrapper générique qui appelle un callable et gère le résultat
        template <typename F, typename... Args>
        auto SafeInvoke(F&& f, Args&&... args)
            -> std::enable_if_t<NkIsInvocable_v<F, Args...>, NkInvokeResult_t<F, Args...>>
        {
            // Vérification compile-time : ne compile que si invocable
            static_assert(NkIsInvocable_v<F, Args...>,
                "Provided callable cannot be invoked with given arguments");

            // Appel effectif avec forwarding parfait
            return NkInvoke(
                traits::NkForward<F>(f),
                traits::NkForward<Args>(args)...
            );
        }

        // Surcharge pour cas non-invocable : retourne un optional vide
        template <typename F, typename... Args>
        auto SafeInvoke(F&&, Args&&...)
            -> std::enable_if_t<!NkIsInvocable_v<F, Args...>, std::nullptr_t>
        {
            // Fallback : callable incompatible avec les arguments fournis
            return nullptr;
        }

        // Exemple d'utilisation avec détection de type de retour
        template <typename Callable>
        void InspectCallable(Callable&& c) {
            using namespace nkentseu;

            // Inspection à la compilation
            constexpr bool noArgs = NkIsInvocable_v<Callable>;
            constexpr bool withInt = NkIsInvocable_v<Callable, int>;
            constexpr bool withIntStr = NkIsInvocable_v<Callable, int, std::string>;

            if constexpr (noArgs) {
                using Result0 = NkInvokeResult_t<Callable>;
                // ... traitement pour appel sans args ...
            }
            if constexpr (withInt) {
                using Result1 = NkInvokeResult_t<Callable, int>;
                // ... traitement pour appel avec int ...
            }
        }

    } // namespace nkentseu

    // Usage concret
    void GenericExample() {
        auto lambda = [](int x, const std::string& s) -> double {
            return x * static_cast<double>(s.size());
        };

        // SafeInvoke déduit automatiquement le type de retour (double ici)
        double result = nkentseu::SafeInvoke(lambda, 5, std::string("hello"));
        // result == 5 * 5.0 == 25.0

        // Appel incompatible retourne nullptr (via enable_if fallback)
        auto bad = nkentseu::SafeInvoke(lambda, "wrong", "types");  // bad == nullptr
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Factory pattern avec NkInvoke pour construction polymorphique
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkInvoke.h"
    #include <memory>
    #include <unordered_map>
    #include <string>
    #include <functional>

    // Interface de base pour un système de plugins
    class IPlugin {
    public:
        virtual ~IPlugin() = default;
        virtual void Initialize() = 0;
        virtual std::string GetName() const = 0;
    };

    // Plugin concret
    class LoggerPlugin : public IPlugin {
    public:
        LoggerPlugin(std::string name, int level)
            : m_name(std::move(name)), m_level(level) {}

        void Initialize() override {
            // Initialisation du logger...
        }

        std::string GetName() const override { return m_name; }

    private:
        std::string m_name;
        int m_level;
    };

    // Registry de factories utilisant NkInvoke pour construction générique
    class PluginRegistry {
    public:
        // Type pour une factory : fonction retournant unique_ptr<IPlugin>
        using FactoryFn = std::function<std::unique_ptr<IPlugin>()>;

        // Enregistrement d'un plugin avec factory arbitraire
        template <typename Factory>
        void Register(const std::string& id, Factory&& factory) {
            static_assert(
                nkentseu::NkIsInvocable_v<Factory>,
                "Factory must be callable with no arguments"
            );
            static_assert(
                std::is_convertible_v<
                    nkentseu::NkInvokeResult_t<Factory>,
                    std::unique_ptr<IPlugin>
                >,
                "Factory must return std::unique_ptr<IPlugin> or convertible"
            );

            // Stockage avec type-erasure via std::function
            m_factories[id] = [f = nkentseu::traits::NkForward<Factory>(factory)]() mutable
                -> std::unique_ptr<IPlugin>
            {
                // NkInvoke appelle la factory et retourne le résultat
                return nkentseu::NkInvoke(f);
            };
        }

        // Création d'une instance de plugin par ID
        std::unique_ptr<IPlugin> Create(const std::string& id) const {
            auto it = m_factories.find(id);
            if (it != m_factories.end()) {
                // Appel de la factory enregistrée via NkInvoke
                return nkentseu::NkInvoke(it->second);
            }
            return nullptr;  // Plugin non trouvé
        }

    private:
        std::unordered_map<std::string, FactoryFn> m_factories;
    };

    // Usage du registry
    void SetupPlugins() {
        PluginRegistry registry;

        // Enregistrement avec lambda de construction
        registry.Register("logger", []() {
            return std::make_unique<LoggerPlugin>("AppLogger", 3);
        });

        // Enregistrement avec fonction libre
        auto CreateAudioPlugin = []() -> std::unique_ptr<IPlugin> {
            // return std::make_unique<AudioPlugin>(...);
            return nullptr;  // Placeholder
        };
        registry.Register("audio", CreateAudioPlugin);

        // Création à la demande
        auto logger = registry.Create("logger");
        if (logger) {
            logger->Initialize();
            std::cout << "Loaded: " << logger->GetName() << std::endl;
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Adaptateur de callbacks avec NkInvoke pour flexibilité maximale
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkInvoke.h"
    #include <vector>
    #include <functional>

    namespace nkentseu {

        // Callback générique acceptant n'importe quel invocable
        template <typename... CallbackArgs>
        class Callback {
        public:
            // Construction depuis n'importe quel invocable compatible
            template <typename F>
            explicit Callback(F&& callback)
                : m_callback([cb = traits::NkForward<F>(callback)](CallbackArgs... args) mutable {
                    // NkInvoke gère tous les cas : fonction, lambda, membre, etc.
                    return NkInvoke(cb, traits::NkForward<CallbackArgs>(args)...);
                })
            {
                static_assert(NkIsInvocable_v<F, CallbackArgs...>,
                    "Callback must be invocable with the specified argument types");
            }

            // Appel du callback avec les arguments fournis
            template <typename... CallArgs>
            auto operator()(CallArgs&&... args) const
                -> decltype(NkInvoke(m_callback, traits::NkForward<CallArgs>(args)...))
            {
                return NkInvoke(m_callback, traits::NkForward<CallArgs>(args)...);
            }

            // Vérification si le callback est valide (non-null pour std::function)
            explicit operator bool() const noexcept {
                return static_cast<bool>(m_callback);
            }

        private:
            // Type-erasure via std::function pour stockage uniforme
            std::function<void(CallbackArgs...)> m_callback;
        };

        // Helper pour déduction de template (C++17 CTAD ou fonction factory)
        template <typename F, typename... Args>
        auto MakeCallback(F&& f, Args&&... sampleArgs)
            -> Callback<std::decay_t<Args>...>
        {
            using CB = Callback<std::decay_t<Args>...>;
            return CB(traits::NkForward<F>(f));
        }

    } // namespace nkentseu

    // Usage : callbacks flexibles avec différents types d'invocables
    void EventSystemExample() {
        using namespace nkentseu;

        // Callback acceptant (int, std::string)
        Callback<int, std::string> onEvent;

        // Enregistrement d'une fonction libre
        auto HandleEvent = [](int code, const std::string& msg) {
            printf("Event %d: %s\n", code, msg.c_str());
        };
        onEvent = Callback<int, std::string>(HandleEvent);

        // Enregistrement d'un lambda avec capture
        int eventCount = 0;
        onEvent = Callback<int, std::string>([&eventCount](int, const std::string&) {
            ++eventCount;
        });

        // Enregistrement d'une méthode membre (via std::bind ou lambda wrapper)
        class Handler {
        public:
            void OnEvent(int code, const std::string& msg) {
                // Traitement...
            }
        };
        Handler h;
        onEvent = Callback<int, std::string>([&h](int c, const std::string& m) {
            // NkInvoke pourrait être utilisé ici pour appeler directement :
            // NkInvoke(&Handler::OnEvent, &h, c, m);
            h.OnEvent(c, m);
        });

        // Déclenchement de l'événement
        if (onEvent) {
            onEvent(42, "Test event");  // Appelle le callback enregistré
        }
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================