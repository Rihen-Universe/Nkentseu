// -----------------------------------------------------------------------------
// FICHIER: NKMemory\NkFunction.h
// DESCRIPTION: Conteneur polymorphe pour objets appelables — sans dépendance STL
// AUTEUR: TEUGUIA TADJUIDJE Rodolf / Rihen
// DATE: 2025-06-10 / Mise à jour: 2026-03-14
// VERSION: 2.0.0
// LICENCE: Proprietary - Free to use and modify
// -----------------------------------------------------------------------------
//
// RÉSUMÉ:
//   Implémentation d'un conteneur de fonctions polymorphe similaire à std::function,
//   mais sans dépendance à la STL. Supporte lambdas, foncteurs, pointeurs de fonction,
//   méthodes membres (const et non-const), et binding multiple indexé.
//
// CARACTÉRISTIQUES:
//   - Syntaxe identique à std::function : NkFunction<void(int, float)>
//   - Effacement de type via interface virtuelle CallableBase
//   - Allocation personnalisable via mem::NkAllocator
//   - Binding multiple : plusieurs méthodes indexées sur un même objet
//   - Gestion noexcept et constexpr là où applicable
//   - Pas d'exceptions : retour silencieux de R{} sur appel vide
//
// DÉPENDANCES:
//   - NKCore/NkTypes.h          : Types fondamentaux (usize, etc.)
//   - NKCore/NkTraits.h         : Traits méta-programmation (NkDecay_t, NkEnableIf_t, etc.)
//   - NKContainers/Heterogeneous/NkPair.h : Paire générique pour le binding
//   - NKMemory/NkAllocator.h    : Système d'allocation mémoire personnalisable
//
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_MEMORY_NKFUNCTION_H
#define NK_MEMORY_NKFUNCTION_H

    // ========================================================================
    // INCLUSIONS DES DÉPENDANCES
    // ========================================================================

    #include "NKCore/NkTypes.h"                    // Types fondamentaux : usize, etc.
    #include "NKCore/NkTraits.h"                   // Traits méta-programmation et utilitaires
    #include "NKContainers/Heterogeneous/NkPair.h" // NkPair pour le binding méthode/objet
    #include "NKMemory/NkAllocator.h"              // Système d'allocation mémoire personnalisable

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
         * @note Permet d'utiliser nullptr dans les signatures sans dépendance système
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
             * @note Retourne l'allocateur par défaut du système en cas d'échec
             * @note Complexité O(1) : vérifications arithmétiques simples
             */
            inline mem::NkAllocator* NkResolveFunctionAllocator(
                    mem::NkAllocator* allocator) noexcept
            {
                mem::NkAllocator* fallback = &mem::NkGetDefaultAllocator();
                if (!allocator)
                {
                    return fallback;
                }
                const usize addr = reinterpret_cast<usize>(allocator);
                const usize alignMask = static_cast<usize>(alignof(void*)) - 1u;
                if ((addr & alignMask) != 0u)
                {
                    return fallback;
                }
                return allocator;
            }

        } // namespace detail


        // ====================================================================
        // DÉCLARATION PRIMAIRE : NKFUNCTION (NON DÉFINIE)
        // ====================================================================

        /**
         * @brief Déclaration primaire de NkFunction — intentionnellement non définie
         * @tparam Signature Type de signature de fonction (doit être R(Args...))
         * @note Force l'utilisation de la spécialisation partielle NkFunction<R(Args...)>
         * @note Toute tentative d'instanciation directe génère une erreur de liaison
         * @note Pattern classique pour imposer une syntaxe de template spécifique
         */
        template<typename Signature>
        class NkFunction;   // Intentionnellement non définie — voir spécialisation ci-dessous


        // ====================================================================
        // SPÉCIALISATION PARTIELLE : NKFUNCTION<R(Args...)>
        // ====================================================================

        /**
         * @brief Conteneur polymorphe pour objets appelables avec signature fixe
         * 
         * Équivalent fonctionnel à std::function de la STL, mais implémenté sans
         * dépendance externe. Permet de stocker, copier et invoquer n'importe quel
         * objet callable compatible avec la signature R(Args...).
         * 
         * PRINCIPE DE FONCTIONNEMENT :
         * - Effacement de type via interface virtuelle CallableBase
         * - Allocation dynamique du callable concret via l'allocateur fourni
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
         * 
         * GARANTIES DE PERFORMANCE :
         * - Construction : O(1) allocation + construction du callable
         * - Invocation : O(1) appel virtuel + forwarding des arguments
         * - Copie : O(n) où n = taille du callable cloné (allocation + copie)
         * - Déplacement : O(1) transfert de pointeur, aucune allocation
         * - Mémoire : sizeof(CallableBase*) + sizeof(Allocator*) + overhead virtuel
         * 
         * CAS D'USAGE TYPQUES :
         * - Callbacks et systèmes d'événements dans les moteurs de jeu
         * - Command pattern pour l'annulation/refaire d'actions
         * - Injection de dépendances fonctionnelles dans les algorithmes
         * - Wrapping de méthodes membres pour interfaces uniformes
         * - Systèmes de scripting ou de plugins avec API fonctionnelle
         * 
         * @tparam R Type de retour de la fonction encapsulée
         * @tparam Args... Types des paramètres de la fonction encapsulée
         * 
         * @note Thread-unsafe par défaut : synchronisation externe requise pour accès concurrents
         * @note Pas d'exceptions : appel sur fonction vide retourne R{} silencieusement
         * @note Allocateur personnalisable : essentiel pour les systèmes embarqués ou temps réel
         * @note Pas de small-buffer optimization : toutes les allocations sont dynamiques
         */
        template<typename R, typename... Args>
        class NkFunction<R(Args...)> {

            // ====================================================================
            // SECTION PRIVÉE : INTERFACE DE BASE POUR L'EFFACEMENT DE TYPE
            // ====================================================================
            private:

                /**
                 * @brief Interface virtuelle de base pour l'effacement de type des callables
                 * 
                 * Définit le contrat minimal que toute implémentation concrète de callable
                 * doit respecter pour être stockée dans NkFunction. Permet l'invocation
                 * polymorphe, le clonage et la destruction sans connaître le type exact.
                 * 
                 * @note Interface purement virtuelle : pas de données membres, uniquement méthodes
                 * @note Destructeur virtuel : essentiel pour la destruction polymorphe correcte
                 * @note Méthodes noexcept là où possible pour éviter les overheads d'exception
                 */
                struct CallableBase {

                    /**
                     * @brief Destructeur virtuel pour libération polymorphe correcte
                     * @note Defaulted : comportement standard de destruction C++
                     * @note Virtual : permet l'appel du destructeur dérivé via pointeur de base
                     */
                    virtual ~CallableBase() = default;

                    /**
                     * @brief Invocation standard du callable avec forwarding des arguments
                     * @param args... Arguments à forwarder vers le callable encapsulé
                     * @return Valeur de type R résultant de l'appel (ou void)
                     * @note Pure virtual : doit être implémentée par chaque type concret
                     * @note Utilise traits::NkForward pour préserver les catégories de valeur
                     */
                    virtual R Invoke(Args... args) = 0;

                    /**
                     * @brief Invocation avec index pour le binding multiple de méthodes
                     * @param index Index de la méthode à invoquer dans le tableau interne
                     * @param args... Arguments à forwarder vers la méthode sélectionnée
                     * @return Valeur de type R résultant de l'appel (ou void)
                     * @note Par défaut : délègue à Invoke() pour les callables simples
                     * @note Surchargé par MultiMethodCallableImpl pour le dispatch indexé
                     */
                    virtual R InvokeWithIndex(usize index, Args... args) = 0;

                    /**
                     * @brief Clone ce callable dans un allocateur donné
                     * @param allocator Pointeur vers l'allocateur pour la nouvelle instance
                     * @return Pointeur vers le nouveau CallableBase cloné, ou nullptr en cas d'échec
                     * @note Pure virtual : chaque type concret gère sa propre logique de copie
                     * @note Utilise placement new pour construire l'objet dans la mémoire allouée
                     * @note Retourne nullptr si l'allocation échoue (pas d'exception levée)
                     */
                    virtual CallableBase* Clone(mem::NkAllocator* allocator) const = 0;

                    /**
                     * @brief Détruit ce callable et libère sa mémoire via l'allocateur
                     * @param allocator Pointeur vers l'allocateur utilisé pour la désallocation
                     * @note Pure virtual : chaque type concret doit appeler son destructeur puis Deallocate
                     * @note Gestion safe : vérifie que l'allocateur est valide avant désallocation
                     * @note Ne lance jamais d'exception : crucial pour la sécurité en destruction
                     */
                    virtual void Destroy(mem::NkAllocator* allocator) = 0;
                };


                // ====================================================================
                // IMPLÉMENTATION GÉNÉRIQUE : CALLABLEIMPL<T>
                // ====================================================================

                /**
                 * @brief Implémentation générique pour lambdas, foncteurs et pointeurs de fonction
                 * @tparam T Type concret du callable encapsulé (après decay)
                 * @note Hérite de CallableBase pour l'effacement de type
                 * @note Stocke une copie du callable par valeur (copie ou move selon construction)
                 * @note noexcept sur les constructeurs si T est triviallement copiable/déplaçable
                 */
                template<typename T>
                struct CallableImpl : CallableBase {

                    T callable;   ///< Instance du callable concret stockée par valeur

                    /**
                     * @brief Constructeur par copie pour callables lvalue
                     * @param c Référence const vers le callable source à copier
                     * @note noexcept si T est trivialement copiable : optimisation compilation
                     * @note Initialisation directe du membre callable via liste d'initialisation
                     */
                    explicit CallableImpl(const T& c)
                        noexcept(traits::NkIsTriviallyCopyable_v<T>)
                        : callable(c)
                    {
                    }

                    /**
                     * @brief Constructeur par déplacement pour callables rvalue
                     * @param c Rvalue reference vers le callable source à déplacer
                     * @note noexcept si T est trivialement déplaçable : optimisation compilation
                     * @note Utilise traits::NkMove pour caster en rvalue et activer le move constructor
                     */
                    explicit CallableImpl(T&& c)
                        noexcept(traits::NkIsTriviallyMoveConstructible_v<T>)
                        : callable(traits::NkMove(c))
                    {
                    }

                    /**
                     * @brief Invocation standard : délègue à l'operator() du callable stocké
                     * @param args... Arguments à forwarder vers le callable
                     * @return Résultat de type R de l'appel du callable
                     * @note Override de l'interface CallableBase::Invoke()
                     * @note Utilise traits::NkForward pour préserver perfect forwarding
                     */
                    R Invoke(Args... args) override
                    {
                        return callable(traits::NkForward<Args>(args)...);
                    }

                    /**
                     * @brief Invocation indexée : ignore l'index pour les callables simples
                     * @param index Paramètre ignoré (non utilisé pour les callables non-indexés)
                     * @param args... Arguments à forwarder vers le callable
                     * @return Résultat de type R de l'appel du callable
                     * @note Override de l'interface CallableBase::InvokeWithIndex()
                     * @note Délègue simplement à Invoke() : comportement par défaut
                     */
                    R InvokeWithIndex(usize, Args... args) override
                    {
                        return Invoke(traits::NkForward<Args>(args)...);
                    }

                    /**
                     * @brief Clone ce CallableImpl dans un allocateur donné
                     * @param allocator Pointeur vers l'allocateur pour la nouvelle instance
                     * @return Pointeur vers le nouveau CallableBase cloné, ou nullptr en cas d'échec
                     * @note Alloue la mémoire brute via allocator->Allocate()
                     * @note Construit l'objet via placement new en copiant le callable membre
                     * @note Retourne nullptr silencieusement si l'allocation échoue (pas d'exception)
                     */
                    CallableBase* Clone(mem::NkAllocator* allocator) const override
                    {
                        mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                        void* mem = alloc->Allocate(sizeof(CallableImpl), alignof(CallableImpl));
                        if (!mem)
                        {
                            return nullptr;
                        }
                        return new (mem) CallableImpl(callable);
                    }

                    /**
                     * @brief Détruit ce CallableImpl et libère sa mémoire
                     * @param allocator Pointeur vers l'allocateur pour la désallocation
                     * @note Appelle explicitement le destructeur ~CallableImpl() via placement delete
                     * @note Libère la mémoire brute via allocator->Deallocate()
                     * @note Ne lance jamais d'exception : sécurisé pour la destruction
                     */
                    void Destroy(mem::NkAllocator* allocator) override
                    {
                        mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                        this->~CallableImpl();
                        alloc->Deallocate(this, sizeof(CallableImpl));
                    }

                }; // struct CallableImpl<T>


                // ====================================================================
                // IMPLÉMENTATION : MÉTHODE MEMBRE NON-CONST
                // ====================================================================

                /**
                 * @brief Implémentation pour l'appel de méthodes membres non-const
                 * @tparam T Type de la classe contenant la méthode membre
                 * @note Stocke un pointeur vers l'objet et un pointeur vers la méthode membre
                 * @note Gère le cas null object en retournant DefaultReturn() silencieusement
                 * @note Utilise la syntaxe (object->*method)(args...) pour l'invocation C++
                 */
                template<typename T>
                struct MethodCallableImpl : CallableBase {

                    T* object;                        ///< Pointeur vers l'objet cible
                    R (T::*method)(Args...);          ///< Pointeur vers la méthode membre non-const

                    /**
                     * @brief Constructeur initialisant l'objet et la méthode membre
                     * @param obj Pointeur vers l'objet sur lequel appeler la méthode
                     * @param meth Pointeur vers la méthode membre à invoquer
                     * @note Initialisation via liste d'initialisation pour performance
                     * @note Ne vérifie pas la nullité : responsabilité du caller ou de Invoke()
                     */
                    MethodCallableImpl(T* obj, R (T::*meth)(Args...))
                        : object(obj)
                        , method(meth)
                    {
                    }

                    /**
                     * @brief Invocation de la méthode membre sur l'objet stocké
                     * @param args... Arguments à forwarder vers la méthode
                     * @return Résultat de type R de l'appel de méthode, ou DefaultReturn() si null
                     * @note Vérifie la nullité de object avant l'appel pour éviter les crashs
                     * @note Utilise traits::NkForward pour préserver les catégories de valeur
                     */
                    R Invoke(Args... args) override
                    {
                        if (!object)
                        {
                            return DefaultReturn();
                        }
                        return (object->*method)(traits::NkForward<Args>(args)...);
                    }

                    /**
                     * @brief Invocation indexée : délègue à Invoke() pour méthode unique
                     * @param index Paramètre ignoré (non utilisé pour méthode unique)
                     * @param args... Arguments à forwarder vers la méthode
                     * @return Résultat de type R de l'appel de méthode
                     * @note Override de l'interface CallableBase::InvokeWithIndex()
                     * @note Comportement identique à Invoke() pour les callables non-indexés
                     */
                    R InvokeWithIndex(usize, Args... args) override
                    {
                        return Invoke(traits::NkForward<Args>(args)...);
                    }

                    /**
                     * @brief Clone ce MethodCallableImpl dans un allocateur donné
                     * @param allocator Pointeur vers l'allocateur pour la nouvelle instance
                     * @return Pointeur vers le nouveau CallableBase cloné, ou nullptr en cas d'échec
                     * @note Alloue la mémoire pour MethodCallableImpl<T> avec alignement correct
                     * @note Copie les membres object et method par valeur (pointeurs)
                     * @note Retourne nullptr silencieusement si l'allocation échoue
                     */
                    CallableBase* Clone(mem::NkAllocator* allocator) const override
                    {
                        mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                        void* mem = alloc->Allocate(
                            sizeof(MethodCallableImpl),
                            alignof(MethodCallableImpl)
                        );
                        if (!mem)
                        {
                            return nullptr;
                        }
                        return new (mem) MethodCallableImpl(object, method);
                    }

                    /**
                     * @brief Détruit ce MethodCallableImpl et libère sa mémoire
                     * @param allocator Pointeur vers l'allocateur pour la désallocation
                     * @note Appelle explicitement le destructeur ~MethodCallableImpl()
                     * @note Libère la mémoire brute via allocator->Deallocate()
                     * @note Ne désalloue pas l'objet pointé : responsabilité du propriétaire
                     */
                    void Destroy(mem::NkAllocator* allocator) override
                    {
                        mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                        this->~MethodCallableImpl();
                        alloc->Deallocate(this, sizeof(MethodCallableImpl));
                    }

                private:

                    /**
                     * @brief Retourne une valeur par défaut pour le type R
                     * @return R{} pour les types constructibles, ou rien pour void
                     * @note Utilisé quand l'objet est null ou l'appel invalide
                     * @note if constexpr : évaluation à la compilation selon le type de retour
                     * @note noexcept : ne lance jamais d'exception
                     */
                    static R DefaultReturn()
                    {
                        if constexpr (traits::NkIsVoid_v<R>)
                        {
                            return;
                        }
                        else
                        {
                            return R{};
                        }
                    }

                }; // struct MethodCallableImpl<T>


                // ====================================================================
                // IMPLÉMENTATION : MÉTHODE MEMBRE CONST
                // ====================================================================

                /**
                 * @brief Implémentation pour l'appel de méthodes membres const
                 * @tparam T Type de la classe contenant la méthode membre const
                 * @note Similaire à MethodCallableImpl mais pour méthodes const-qualifiées
                 * @note Permet d'appeler des méthodes const sur des objets const ou non
                 * @note La const-qualification fait partie du type du pointeur de méthode
                 */
                template<typename T>
                struct MethodCallableConstImpl : CallableBase {

                    T* object;                              ///< Pointeur vers l'objet cible
                    R (T::*method)(Args...) const;          ///< Pointeur vers la méthode membre const

                    /**
                     * @brief Constructeur initialisant l'objet et la méthode membre const
                     * @param obj Pointeur vers l'objet sur lequel appeler la méthode
                     * @param meth Pointeur vers la méthode membre const à invoquer
                     * @note Initialisation via liste d'initialisation pour performance
                     * @note La const-qualification de la méthode est préservée dans le type
                     */
                    MethodCallableConstImpl(T* obj, R (T::*meth)(Args...) const)
                        : object(obj)
                        , method(meth)
                    {
                    }

                    /**
                     * @brief Invocation de la méthode membre const sur l'objet stocké
                     * @param args... Arguments à forwarder vers la méthode const
                     * @return Résultat de type R de l'appel, ou DefaultReturn() si object null
                     * @note Vérifie la nullité de object avant l'appel pour sécurité
                     * @note L'appel (object->*method) respecte la const-qualification
                     */
                    R Invoke(Args... args) override
                    {
                        if (!object)
                        {
                            return DefaultReturn();
                        }
                        return (object->*method)(traits::NkForward<Args>(args)...);
                    }

                    /**
                     * @brief Invocation indexée : délègue à Invoke() pour méthode unique const
                     * @param index Paramètre ignoré (non utilisé pour méthode unique)
                     * @param args... Arguments à forwarder vers la méthode const
                     * @return Résultat de type R de l'appel de méthode const
                     * @note Override de l'interface CallableBase::InvokeWithIndex()
                     * @note Comportement identique à Invoke() pour les callables non-indexés
                     */
                    R InvokeWithIndex(usize, Args... args) override
                    {
                        return Invoke(traits::NkForward<Args>(args)...);
                    }

                    /**
                     * @brief Clone ce MethodCallableConstImpl dans un allocateur donné
                     * @param allocator Pointeur vers l'allocateur pour la nouvelle instance
                     * @return Pointeur vers le nouveau CallableBase cloné, ou nullptr en cas d'échec
                     * @note Alloue la mémoire pour MethodCallableConstImpl<T> avec alignement
                     * @note Copie les pointeurs object et method par valeur
                     * @note Retourne nullptr silencieusement si l'allocation échoue
                     */
                    CallableBase* Clone(mem::NkAllocator* allocator) const override
                    {
                        mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                        void* mem = alloc->Allocate(
                            sizeof(MethodCallableConstImpl),
                            alignof(MethodCallableConstImpl)
                        );
                        if (!mem)
                        {
                            return nullptr;
                        }
                        return new (mem) MethodCallableConstImpl(object, method);
                    }

                    /**
                     * @brief Détruit ce MethodCallableConstImpl et libère sa mémoire
                     * @param allocator Pointeur vers l'allocateur pour la désallocation
                     * @note Appelle explicitement le destructeur ~MethodCallableConstImpl()
                     * @note Libère la mémoire brute via allocator->Deallocate()
                     * @note Ne désalloue pas l'objet pointé : responsabilité du propriétaire
                     */
                    void Destroy(mem::NkAllocator* allocator) override
                    {
                        mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(allocator);
                        this->~MethodCallableConstImpl();
                        alloc->Deallocate(this, sizeof(MethodCallableConstImpl));
                    }

                private:

                    /**
                     * @brief Retourne une valeur par défaut pour le type R
                     * @return R{} pour les types constructibles, ou rien pour void
                     * @note Utilisé quand l'objet est null ou l'appel invalide
                     * @note if constexpr : évaluation à la compilation selon le type de retour
                     * @note noexcept : ne lance jamais d'exception
                     */
                    static R DefaultReturn()
                    {
                        if constexpr (traits::NkIsVoid_v<R>)
                        {
                            return;
                        }
                        else
                        {
                            return R{};
                        }
                    }

                }; // struct MethodCallableConstImpl<T>


                // ====================================================================
                // IMPLÉMENTATION : BINDING MULTIPLE DE MÉTHODES INDEXÉES
                // ====================================================================

                /**
                 * @brief Implémentation pour le binding de multiples méthodes sur un même objet
                 * @tparam T Type de la classe contenant les méthodes à binder
                 * @note Permet d'associer plusieurs méthodes (const et non-const) à des indices
                 * @note Invocation via operator()(index, args...) pour dispatch dynamique
                 * @note Gestion mémoire interne via allocateur pour le tableau de méthodes
                 * @note Thread-unsafe : synchronisation externe requise pour modifications concurrentes
                 */
                template<typename T>
                struct MultiMethodCallableImpl : CallableBase {

                    /**
                     * @brief Structure interne représentant une entrée de méthode dans le tableau
                     * @note Contient les pointeurs vers les versions const et non-const de la méthode
                     * @note Flag is_const pour savoir quelle version invoquer à l'exécution
                     * @note Une méthode ne peut être const et non-const simultanément : un seul champ actif
                     */
                    struct MethodEntry {

                        R (T::*method)(Args...);            ///< Pointeur vers méthode non-const (ou nullptr)
                        R (T::*const_method)(Args...) const;///< Pointeur vers méthode const (ou nullptr)
                        bool is_const;                      ///< true si const_method est active, false sinon

                    }; // struct MethodEntry

                    T* object;                        ///< Pointeur vers l'objet cible commun
                    MethodEntry* methods;             ///< Tableau dynamique des entrées de méthodes
                    usize method_count;               ///< Nombre d'entrées allouées dans le tableau
                    mem::NkAllocator* alloc;          ///< Allocateur utilisé pour la gestion mémoire interne

                    /**
                     * @brief Constructeur initialisant l'objet et l'allocateur interne
                     * @param obj Pointeur vers l'objet cible pour toutes les méthodes bindées
                     * @param a Pointeur vers l'allocateur pour la gestion du tableau methods
                     * @note methods initialisé à nullptr, method_count à 0 : tableau vide au départ
                     * @note Utilise NkResolveFunctionAllocator pour valider l'allocateur fourni
                     */
                    MultiMethodCallableImpl(T* obj, mem::NkAllocator* a)
                        : object(obj)
                        , methods(nullptr)
                        , method_count(0)
                        , alloc(detail::NkResolveFunctionAllocator(a))
                    {
                    }

                    /**
                     * @brief Ajoute ou remplace une méthode non-const à l'index spécifié
                     * @param meth Pointeur vers la méthode membre non-const à binder
                     * @param index Position dans le tableau où stocker cette méthode
                     * @note Redimensionne automatiquement le tableau si index >= method_count
                     * @note Écrase silencieusement toute méthode existante à cet index
                     * @note Ignore l'appel si meth est nullptr (méthode invalide)
                     */
                    void AddMethod(R (T::*meth)(Args...), usize index)
                    {
                        if (!meth)
                        {
                            return;
                        }
                        ResizeMethods(index + 1);
                        methods[index] = { meth, nullptr, false };
                    }

                    /**
                     * @brief Ajoute ou remplace une méthode const à l'index spécifié
                     * @param meth Pointeur vers la méthode membre const à binder
                     * @param index Position dans le tableau où stocker cette méthode
                     * @note Redimensionne automatiquement le tableau si index >= method_count
                     * @note Écrase silencieusement toute méthode existante à cet index
                     * @note Ignore l'appel si meth est nullptr (méthode invalide)
                     */
                    void AddConstMethod(R (T::*meth)(Args...) const, usize index)
                    {
                        if (!meth)
                        {
                            return;
                        }
                        ResizeMethods(index + 1);
                        methods[index] = { nullptr, meth, true };
                    }

                    /**
                     * @brief Redimensionne le tableau interne de méthodes si nécessaire
                     * @param newSize Nouvelle capacité souhaitée pour le tableau methods
                     * @note Ne fait rien si newSize <= method_count (déjà assez grand)
                     * @note Alloue un nouveau tableau, copie les anciennes entrées, libère l'ancien
                     * @note Initialise les nouvelles entrées avec des pointeurs nullptr
                     * @note Retourne silencieusement en cas d'échec d'allocation (pas d'exception)
                     */
                    void ResizeMethods(usize newSize)
                    {
                        if (newSize <= method_count)
                        {
                            return;
                        }
                        MethodEntry* newM = static_cast<MethodEntry*>(
                            alloc->Allocate(newSize * sizeof(MethodEntry), alignof(MethodEntry))
                        );
                        if (!newM)
                        {
                            return;
                        }
                        for (usize i = 0; i < method_count; ++i)
                        {
                            newM[i] = methods[i];
                        }
                        for (usize i = method_count; i < newSize; ++i)
                        {
                            newM[i] = { nullptr, nullptr, false };
                        }
                        if (methods)
                        {
                            alloc->Deallocate(methods, method_count * sizeof(MethodEntry));
                        }
                        methods = newM;
                        method_count = newSize;
                    }

                    /**
                     * @brief Invocation standard : délègue à l'index 0 par défaut
                     * @param args... Arguments à forwarder vers la méthode à l'index 0
                     * @return Résultat de type R de l'appel de méthode, ou valeur par défaut si invalide
                     * @note Override de CallableBase::Invoke()
                     * @note Comportement pratique : permet d'utiliser MultiMethod comme callable simple
                     */
                    R Invoke(Args... args) override
                    {
                        return InvokeWithIndex(0, traits::NkForward<Args>(args)...);
                    }

                    /**
                     * @brief Invocation indexée : dispatch vers la méthode à l'index spécifié
                     * @param index Index de la méthode à invoquer dans le tableau methods
                     * @param args... Arguments à forwarder vers la méthode sélectionnée
                     * @return Résultat de type R de l'appel, ou valeur par défaut si invalide
                     * @note Vérifie la validité de object, index, et du pointeur de méthode avant appel
                     * @note Utilise is_const pour choisir entre method et const_method
                     * @note Retourne DefaultReturn() silencieusement en cas d'erreur (pas d'exception)
                     */
                    R InvokeWithIndex(usize index, Args... args) override
                    {
                        if (!object || index >= method_count
                            || (!methods[index].method && !methods[index].const_method))
                        {
                            if constexpr (traits::NkIsVoid_v<R>)
                            {
                                return;
                            }
                            else
                            {
                                return R{};
                            }
                        }
                        if (methods[index].is_const)
                        {
                            return (object->*methods[index].const_method)(
                                traits::NkForward<Args>(args)...
                            );
                        }
                        else
                        {
                            return (object->*methods[index].method)(
                                traits::NkForward<Args>(args)...
                            );
                        }
                    }

                    /**
                     * @brief Clone ce MultiMethodCallableImpl dans un allocateur donné
                     * @param allocator Pointeur vers l'allocateur pour la nouvelle instance
                     * @return Pointeur vers le nouveau CallableBase cloné, ou nullptr en cas d'échec
                     * @note Alloue la mémoire pour MultiMethodCallableImpl<T> avec alignement
                     * @note Recrée le tableau methods et copie toutes les MethodEntry une par une
                     * @note Retourne nullptr silencieusement si n'importe quelle allocation échoue
                     */
                    CallableBase* Clone(mem::NkAllocator* allocator) const override
                    {
                        mem::NkAllocator* a = detail::NkResolveFunctionAllocator(allocator);
                        void* mem = a->Allocate(
                            sizeof(MultiMethodCallableImpl),
                            alignof(MultiMethodCallableImpl)
                        );
                        if (!mem)
                        {
                            return nullptr;
                        }
                        auto* clone = new (mem) MultiMethodCallableImpl(object, a);
                        clone->ResizeMethods(method_count);
                        for (usize i = 0; i < method_count; ++i)
                        {
                            clone->methods[i] = methods[i];
                        }
                        return clone;
                    }

                    /**
                     * @brief Détruit ce MultiMethodCallableImpl et libère toute sa mémoire
                     * @param allocator Pointeur vers l'allocateur pour la désallocation
                     * @note Libère d'abord le tableau methods si non-null
                     * @note Appelle explicitement le destructeur ~MultiMethodCallableImpl()
                     * @note Libère enfin la mémoire de l'objet MultiMethodCallableImpl lui-même
                     * @note Ne désalloue pas l'objet pointé : responsabilité du propriétaire externe
                     */
                    void Destroy(mem::NkAllocator* allocator) override
                    {
                        mem::NkAllocator* a = detail::NkResolveFunctionAllocator(allocator);
                        if (methods)
                        {
                            a->Deallocate(methods, method_count * sizeof(MethodEntry));
                        }
                        this->~MultiMethodCallableImpl();
                        a->Deallocate(this, sizeof(MultiMethodCallableImpl));
                    }

                }; // struct MultiMethodCallableImpl<T>


            // ====================================================================
            // SECTION PUBLIQUE : ALIASES ET TYPES MEMBRES
            // ====================================================================
            public:

                /**
                 * @brief Alias du type de retour de la signature fonctionnelle
                 * @note Compatibilité avec std::function::result_type (déprécié en C++17 mais utile)
                 * @note Permet d'interroger le type de retour via NkFunction<Signature>::ResultType
                 * @note Utile pour la méta-programmation et les templates génériques
                 */
                using ResultType = R;


            // ====================================================================
            // SECTION PUBLIQUE : CONSTRUCTEURS
            // ====================================================================
            public:

                /**
                 * @brief Constructeur par défaut créant une fonction vide
                 * @param allocator Pointeur vers un allocateur personnalisé (défaut : allocateur système)
                 * @note Initialise m_callable à nullptr : fonction non-invoquable jusqu'à affectation
                 * @note Utilise NkResolveFunctionAllocator pour valider et configurer l'allocateur
                 * @note noexcept : aucune allocation ni opération pouvant lever d'exception
                 * @note Complexité O(1) : simple initialisation de pointeurs
                 */
                explicit NkFunction(
                        mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator()) noexcept
                    : m_callable(nullptr)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                {
                }

                /**
                 * @brief Constructeur depuis nullptr pour initialisation explicite vide
                 * @param nullptr_t Tag type pour distinguer cette surcharge (decltype(nullptr))
                 * @param allocator Pointeur vers un allocateur personnalisé (défaut : allocateur système)
                 * @note Syntaxe STL-like : NkFunction<void()> fn = nullptr;
                 * @note Comportement identique au constructeur par défaut : fonction vide
                 * @note Utile pour la clarté sémantique et la compatibilité avec les APIs STL
                 * @note noexcept : aucune allocation ni opération risquée
                 */
                NkFunction(NkNullptrT, mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator()) noexcept
                    : m_callable(nullptr)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                {
                }

                /**
                 * @brief Constructeur template depuis n'importe quel callable compatible
                 * @tparam F Type déduit du callable source (lambda, foncteur, pointeur de fonction)
                 * @param f Callable à encapsuler (forwarded pour préserver la catégorie de valeur)
                 * @param allocator Pointeur vers un allocateur personnalisé (défaut : allocateur système)
                 * @note enable_if : exclut NkFunction lui-même pour éviter les ambiguïtés avec copy/move
                 * @note NkDecay_t<F> : normalise le type avant comparaison (enlève ref, const, volatile)
                 * @note Alloue et construit un CallableImpl<F> via placement new
                 * @note Gère silencieusement les échecs d'allocation : m_callable reste nullptr
                 * @note noexcept sur les constructeurs internes si F est triviallement copiable/déplaçable
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
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                {
                    using CallableT = traits::NkRemoveReference_t<F>;
                    void* mem = m_allocator->Allocate(
                        sizeof(CallableImpl<CallableT>),
                        alignof(CallableImpl<CallableT>)
                    );
                    if (!mem)
                    {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction: allocation failed.");
                        #endif
                        return;
                    }
                    m_callable = new (mem) CallableImpl<CallableT>(traits::NkForward<F>(f));
                }

                /**
                 * @brief Constructeur depuis une méthode membre non-const
                 * @tparam T Type de la classe contenant la méthode membre
                 * @param obj Pointeur vers l'objet sur lequel appeler la méthode
                 * @param meth Pointeur vers la méthode membre non-const à encapsuler
                 * @param allocator Pointeur vers un allocateur personnalisé (défaut : allocateur système)
                 * @note Alloue et construit un MethodCallableImpl<T> via placement new
                 * @note Retourne une fonction vide si obj ou meth sont nullptr (échec silencieux)
                 * @note Permet la syntaxe : NkFunction<void(int)> fn(&obj, &Class::Method);
                 * @note noexcept sur l'allocation : gestion d'erreur via retour nullptr
                 */
                template<typename T>
                NkFunction(T* obj, R (T::*meth)(Args...),
                           mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
                    : m_callable(nullptr)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                {
                    if (!obj || !meth)
                    {
                        return;
                    }
                    void* mem = m_allocator->Allocate(
                        sizeof(MethodCallableImpl<T>),
                        alignof(MethodCallableImpl<T>)
                    );
                    if (!mem)
                    {
                        return;
                    }
                    m_callable = new (mem) MethodCallableImpl<T>(obj, meth);
                }

                /**
                 * @brief Constructeur depuis une méthode membre const
                 * @tparam T Type de la classe contenant la méthode membre const
                 * @param obj Pointeur vers l'objet sur lequel appeler la méthode const
                 * @param meth Pointeur vers la méthode membre const à encapsuler
                 * @param allocator Pointeur vers un allocateur personnalisé (défaut : allocateur système)
                 * @note Alloue et construit un MethodCallableConstImpl<T> via placement new
                 * @note Retourne une fonction vide si obj ou meth sont nullptr (échec silencieux)
                 * @note Permet la syntaxe : NkFunction<int()> fn(&obj, &Class::GetValue);
                 * @note La const-qualification est préservée dans le type du pointeur de méthode
                 */
                template<typename T>
                NkFunction(T* obj, R (T::*meth)(Args...) const,
                           mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
                    : m_callable(nullptr)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                {
                    if (!obj || !meth)
                    {
                        return;
                    }
                    void* mem = m_allocator->Allocate(
                        sizeof(MethodCallableConstImpl<T>),
                        alignof(MethodCallableConstImpl<T>)
                    );
                    if (!mem)
                    {
                        return;
                    }
                    m_callable = new (mem) MethodCallableConstImpl<T>(obj, meth);
                }

                /**
                 * @brief Constructeur pour le binding multiple de méthodes indexées
                 * @tparam T Type de la classe contenant les méthodes à binder
                 * @param obj Pointeur vers l'objet cible commun pour toutes les méthodes
                 * @param allocator Pointeur vers un allocateur personnalisé (défaut : allocateur système)
                 * @note Alloue et construit un MultiMethodCallableImpl<T> via placement new
                 * @note Retourne une fonction vide si obj est nullptr (échec silencieux)
                 * @note Le tableau de méthodes est initialement vide : utiliser BindMethod() pour peupler
                 * @note Permet la syntaxe : NkFunction<void(int)> fn(&obj); puis fn.BindMethod(...);
                 */
                template<typename T>
                NkFunction(T* obj,
                           mem::NkAllocator* allocator = &mem::NkGetDefaultAllocator())
                    : m_callable(nullptr)
                    , m_allocator(detail::NkResolveFunctionAllocator(allocator))
                {
                    if (!obj)
                    {
                        return;
                    }
                    void* mem = m_allocator->Allocate(
                        sizeof(MultiMethodCallableImpl<T>),
                        alignof(MultiMethodCallableImpl<T>)
                    );
                    if (!mem)
                    {
                        return;
                    }
                    m_callable = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
                }


            // ====================================================================
            // SECTION PUBLIQUE : RÈGLE DES CINQ (COPY/MOVE/DTOR)
            // ====================================================================
            public:

                /**
                 * @brief Constructeur de copie : clone le callable interne via polymorphisme
                 * @param other Référence const vers le NkFunction source à copier
                 * @note Utilise la méthode virtuelle Clone() pour créer une copie du callable concret
                 * @note Préserve l'allocateur de other via NkResolveFunctionAllocator
                 * @note noexcept : Clone() retourne nullptr en cas d'échec, pas d'exception levée
                 * @note Complexité O(n) où n = taille du callable cloné (allocation + copie des données)
                 */
                NkFunction(const NkFunction& other) noexcept
                    : m_callable(nullptr)
                    , m_allocator(detail::NkResolveFunctionAllocator(other.m_allocator))
                {
                    if (other.m_callable)
                    {
                        m_callable = other.m_callable->Clone(m_allocator);
                    }
                }

                /**
                 * @brief Constructeur de déplacement : transfère la possession sans allocation
                 * @param other Rvalue reference vers le NkFunction source à déplacer
                 * @note Transfère le pointeur m_callable et l'allocateur par simple affectation
                 * @note Laisse other dans un état valide mais vide : m_callable = nullptr
                 * @note noexcept garanti : aucune allocation, copie ou opération risquée
                 * @note Complexité O(1) : simple échange de pointeurs et métadonnées
                 */
                NkFunction(NkFunction&& other) noexcept
                    : m_callable(other.m_callable)
                    , m_allocator(detail::NkResolveFunctionAllocator(other.m_allocator))
                {
                    other.m_callable = nullptr;
                }

                /**
                 * @brief Destructeur : libère proprement le callable interne via polymorphisme
                 * @note Appelle Clear() pour détruire et désallouer le callable si non-null
                 * @note Utilise la méthode virtuelle Destroy() pour la destruction polymorphe correcte
                 * @note noexcept garanti : aucune opération pouvant lever d'exception
                 * @note Garantit l'absence de fuite mémoire via RAII même en cas d'erreur antérieure
                 * @note Complexité O(1) pour le destructeur + O(n) pour la destruction du callable
                 */
                ~NkFunction() noexcept
                {
                    Clear();
                }


            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEURS D'AFFECTATION
            // ====================================================================
            public:

                /**
                 * @brief Opérateur d'affectation depuis nullptr : réinitialise à l'état vide
                 * @param nullptr_t Tag type pour distinguer cette surcharge (decltype(nullptr))
                 * @return Référence vers *this pour le chaînage d'opérations
                 * @note Appelle Clear() pour détruire et désallouer le callable courant
                 * @note Syntaxe STL-like : fn = nullptr; pour reset explicite
                 * @note noexcept : aucune allocation ni opération risquée
                 * @note Complexité O(n) pour la destruction du callable + O(1) pour le reset
                 */
                NkFunction& operator=(NkNullptrT) noexcept
                {
                    Clear();
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par copie depuis const lvalue
                 * @param other Référence const vers le NkFunction source à copier
                 * @return Référence vers *this pour le chaînage d'opérations
                 * @note Utilise le pattern copy-and-swap pour la sécurité exceptionnelle
                 * @note Crée un temporaire copié, puis swap avec *this : strong guarantee
                 * @note Gère l'auto-affectation via le check this != &other (optimisation)
                 * @note noexcept : Clone() et Swap() sont noexcept, pas d'exception possible
                 */
                NkFunction& operator=(const NkFunction& other) noexcept
                {
                    if (this != &other)
                    {
                        NkFunction temp(other);
                        Swap(temp);
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par copie depuis lvalue non-const
                 * @param other Référence non-const vers le NkFunction source à copier
                 * @return Référence vers *this pour le chaînage d'opérations
                 * @note Nécessaire pour éviter la sélection du template operator=(F&&) pour NkFunction&
                 * @note Délègue à l'operator=(const NkFunction&) via cast explicite
                 * @note Résout les ambiguïtés de surcharge pour les lvalues non-const
                 * @note noexcept : délègue à l'implémentation const qui est noexcept
                 */
                NkFunction& operator=(NkFunction& other) noexcept
                {
                    return operator=(static_cast<const NkFunction&>(other));
                }

                /**
                 * @brief Opérateur d'affectation par déplacement depuis rvalue
                 * @param other Rvalue reference vers le NkFunction source à déplacer
                 * @return Référence vers *this pour le chaînage d'opérations
                 * @note Transfère la possession des ressources sans allocation ni copie
                 * @note Détruit d'abord le contenu courant via Clear() pour éviter les fuites
                 * @note Laisse other dans un état valide mais vide après le transfert
                 * @note noexcept garanti : aucune opération pouvant lever d'exception
                 * @note Complexité O(1) pour le transfert + O(n) pour la destruction de l'ancien contenu
                 */
                NkFunction& operator=(NkFunction&& other) noexcept
                {
                    if (this != &other)
                    {
                        Clear();
                        m_callable = other.m_callable;
                        m_allocator = detail::NkResolveFunctionAllocator(other.m_allocator);
                        other.m_callable = nullptr;
                    }
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation template depuis n'importe quel callable
                 * @tparam F Type déduit du callable source (lambda, foncteur, pointeur de fonction)
                 * @param f Callable à encapsuler (forwarded pour préserver la catégorie de valeur)
                 * @return Référence vers *this pour le chaînage d'opérations
                 * @note enable_if : exclut NkFunction lui-même pour éviter les conflits avec copy/move
                 * @note Utilise le pattern copy-and-swap : construit un temporaire, puis swap
                 * @note NkDecay_t<F> normalise le type avant comparaison pour une exclusion robuste
                 * @note noexcept sur la construction du temporaire si F est trivial, swap est toujours noexcept
                 * @note Strong exception guarantee : si la construction échoue, *this reste inchangé
                 */
                template<
                    typename F,
                    typename = traits::NkEnableIf_t<
                        !traits::NkIsSame_v<traits::NkDecay_t<F>, NkFunction>
                    >
                >
                NkFunction& operator=(F&& f)
                {
                    NkFunction temp(traits::NkForward<F>(f), m_allocator);
                    Swap(temp);
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis un binding méthode non-const via NkPair
                 * @tparam T Type de la classe contenant la méthode membre
                 * @param binding NkPair<T*, R(T::*)(Args...)> contenant objet et pointeur de méthode
                 * @return Référence vers *this pour le chaînage d'opérations
                 * @note Délègue au constructeur méthode non-const via un temporaire
                 * @note Utilise le pattern copy-and-swap pour la sécurité exceptionnelle
                 * @note Permet la syntaxe : fn = NkMakePair(&obj, &Class::Method);
                 * @note noexcept sur le swap, construction peut échouer silencieusement
                 */
                template<typename T>
                NkFunction& operator=(NkPair<T*, R (T::*)(Args...)> binding)
                {
                    NkFunction temp(binding.first, binding.second, m_allocator);
                    Swap(temp);
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation depuis un binding méthode const via NkPair
                 * @tparam T Type de la classe contenant la méthode membre const
                 * @param binding NkPair<T*, R(T::*)(Args...) const> contenant objet et pointeur de méthode const
                 * @return Référence vers *this pour le chaînage d'opérations
                 * @note Délègue au constructeur méthode const via un temporaire
                 * @note Utilise le pattern copy-and-swap pour la sécurité exceptionnelle
                 * @note Permet la syntaxe : fn = NkMakePair(&obj, &Class::ConstMethod);
                 * @note noexcept sur le swap, construction peut échouer silencieusement
                 */
                template<typename T>
                NkFunction& operator=(NkPair<T*, R (T::*)(Args...) const> binding)
                {
                    NkFunction temp(binding.first, binding.second, m_allocator);
                    Swap(temp);
                    return *this;
                }


            // ====================================================================
            // SECTION PUBLIQUE : BINDING MULTIPLE DE MÉTHODES
            // ====================================================================
            public:

                /**
                 * @brief Ajoute ou remplace une méthode non-const à l'index spécifié
                 * @tparam T Type de la classe contenant la méthode membre
                 * @param obj Pointeur vers l'objet cible pour cette méthode
                 * @param meth Pointeur vers la méthode membre non-const à binder
                 * @param index Position dans le tableau interne où stocker cette méthode
                 * @note Crée un MultiMethodCallableImpl si m_callable est actuellement nullptr
                 * @note Redimensionne automatiquement le tableau interne si index >= capacité actuelle
                 * @note Écrase silencieusement toute méthode existante à cet index
                 * @note Ignore l'appel si obj ou meth sont nullptr (validation des paramètres)
                 * @note Thread-unsafe : synchronisation externe requise pour modifications concurrentes
                 */
                template<typename T>
                void BindMethod(T* obj, R (T::*meth)(Args...), usize index)
                {
                    if (!obj || !meth)
                    {
                        return;
                    }
                    if (!m_callable)
                    {
                        void* mem = m_allocator->Allocate(
                            sizeof(MultiMethodCallableImpl<T>),
                            alignof(MultiMethodCallableImpl<T>)
                        );
                        if (!mem)
                        {
                            return;
                        }
                        m_callable = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
                    }
                    static_cast<MultiMethodCallableImpl<T>*>(m_callable)->AddMethod(meth, index);
                }

                /**
                 * @brief Ajoute ou remplace une méthode const à l'index spécifié
                 * @tparam T Type de la classe contenant la méthode membre const
                 * @param obj Pointeur vers l'objet cible pour cette méthode const
                 * @param meth Pointeur vers la méthode membre const à binder
                 * @param index Position dans le tableau interne où stocker cette méthode
                 * @note Crée un MultiMethodCallableImpl si m_callable est actuellement nullptr
                 * @note Redimensionne automatiquement le tableau interne si index >= capacité actuelle
                 * @note Écrase silencieusement toute méthode existante à cet index
                 * @note Ignore l'appel si obj ou meth sont nullptr (validation des paramètres)
                 * @note Thread-unsafe : synchronisation externe requise pour modifications concurrentes
                 */
                template<typename T>
                void BindMethod(T* obj, R (T::*meth)(Args...) const, usize index)
                {
                    if (!obj || !meth)
                    {
                        return;
                    }
                    if (!m_callable)
                    {
                        void* mem = m_allocator->Allocate(
                            sizeof(MultiMethodCallableImpl<T>),
                            alignof(MultiMethodCallableImpl<T>)
                        );
                        if (!mem)
                        {
                            return;
                        }
                        m_callable = new (mem) MultiMethodCallableImpl<T>(obj, m_allocator);
                    }
                    static_cast<MultiMethodCallableImpl<T>*>(m_callable)->AddConstMethod(meth, index);
                }


            // ====================================================================
            // SECTION PUBLIQUE : OPÉRATEURS D'INVOCATION
            // ====================================================================
            public:

                /**
                 * @brief Opérateur d'appel standard : invoque le callable encapsulé
                 * @param args... Arguments à forwarder vers le callable
                 * @return Valeur de type R résultant de l'appel, ou R{} si fonction vide
                 * @note Vérifie si m_callable est nullptr avant l'invocation pour éviter les crashs
                 * @note Retourne silencieusement R{} (ou rien pour void) si la fonction est vide
                 * @note Utilise traits::NkForward pour préserver les catégories de valeur des arguments
                 * @note Pas de bad_function_call : comportement préféré dans les moteurs sans exceptions
                 * @note Thread-safe en lecture si le callable interne est lui-même thread-safe
                 */
                R operator()(Args... args) const
                {
                    if (!m_callable)
                    {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::operator(): empty function call.");
                        #endif
                        if constexpr (traits::NkIsVoid_v<R>)
                        {
                            return;
                        }
                        else
                        {
                            return R{};
                        }
                    }
                    return m_callable->Invoke(traits::NkForward<Args>(args)...);
                }

                /**
                 * @brief Opérateur d'appel indexé : invoque la méthode à l'index spécifié
                 * @param index Index de la méthode à invoquer dans le tableau interne
                 * @param args... Arguments à forwarder vers la méthode sélectionnée
                 * @return Valeur de type R résultant de l'appel, ou R{} si invalide
                 * @note Principalement utile pour les MultiMethodCallableImpl avec binding multiple
                 * @note Pour les callables simples : index ignoré, délègue à l'invocation standard
                 * @note Retourne silencieusement R{} si m_callable est nullptr ou index invalide
                 * @note Pas d'exception levée : retour de valeur par défaut en cas d'erreur
                 * @note Thread-safe en lecture si le callable interne est lui-même thread-safe
                 */
                R operator()(usize index, Args... args) const
                {
                    if (!m_callable)
                    {
                        #ifdef NKENTSEU_LOG_ENABLED
                        NKENTSEU_LOG_ERROR("NkFunction::operator(): empty (index %zu).", index);
                        #endif
                        if constexpr (traits::NkIsVoid_v<R>)
                        {
                            return;
                        }
                        else
                        {
                            return R{};
                        }
                    }
                    return m_callable->InvokeWithIndex(index, traits::NkForward<Args>(args)...);
                }


            // ====================================================================
            // SECTION PUBLIQUE : TESTS DE VALIDITÉ ET COMPARAISONS
            // ====================================================================
            public:

                /**
                 * @brief Conversion explicite en bool pour tester si la fonction est valide
                 * @return true si m_callable != nullptr, false sinon
                 * @note Syntaxe STL-like : if (fn) { fn(); }
                 * @note explicit : évite les conversions implicites accidentelles dans les expressions
                 * @note noexcept : simple comparaison de pointeur, aucune opération risquée
                 * @note Méthode const : ne modifie pas l'état de la fonction
                 */
                explicit operator bool() const noexcept
                {
                    return m_callable != nullptr;
                }

                /**
                 * @brief Méthode explicite pour tester la validité de la fonction
                 * @return true si m_callable != nullptr, false sinon
                 * @note Alternative sémantiquement claire à l'operator bool()
                 * @note Utile dans les contextes où explicit operator bool est moins lisible
                 * @note noexcept : simple comparaison de pointeur, aucune opération risquée
                 * @note Méthode const : ne modifie pas l'état de la fonction
                 */
                bool IsValid() const noexcept
                {
                    return m_callable != nullptr;
                }

                /**
                 * @brief Opérateur de comparaison avec nullptr : teste si la fonction est vide
                 * @param fn Référence const vers le NkFunction à tester
                 * @param nullptr_t Tag type pour la comparaison avec nullptr
                 * @return true si fn.m_callable == nullptr, false sinon
                 * @note Permet la syntaxe STL-like : if (fn == nullptr) { ... }
                 * @note Friend : accède au membre privé m_callable pour la comparaison
                 * @note noexcept : simple comparaison de pointeur, aucune opération risquée
                 */
                friend bool operator==(const NkFunction& fn, NkNullptrT) noexcept
                {
                    return fn.m_callable == nullptr;
                }

                /**
                 * @brief Opérateur de comparaison avec nullptr (ordre inversé)
                 * @param nullptr_t Tag type pour la comparaison avec nullptr
                 * @param fn Référence const vers le NkFunction à tester
                 * @return true si fn.m_callable == nullptr, false sinon
                 * @note Permet la syntaxe symétrique : if (nullptr == fn) { ... }
                 * @note Friend : accède au membre privé m_callable pour la comparaison
                 * @note noexcept : simple comparaison de pointeur, aucune opération risquée
                 */
                friend bool operator==(NkNullptrT, const NkFunction& fn) noexcept
                {
                    return fn.m_callable == nullptr;
                }

                /**
                 * @brief Opérateur de différence avec nullptr : teste si la fonction est non-vide
                 * @param fn Référence const vers le NkFunction à tester
                 * @param nullptr_t Tag type pour la comparaison avec nullptr
                 * @return true si fn.m_callable != nullptr, false sinon
                 * @note Permet la syntaxe STL-like : if (fn != nullptr) { ... }
                 * @note Friend : accède au membre privé m_callable pour la comparaison
                 * @note noexcept : simple comparaison de pointeur, aucune opération risquée
                 */
                friend bool operator!=(const NkFunction& fn, NkNullptrT) noexcept
                {
                    return fn.m_callable != nullptr;
                }

                /**
                 * @brief Opérateur de différence avec nullptr (ordre inversé)
                 * @param nullptr_t Tag type pour la comparaison avec nullptr
                 * @param fn Référence const vers le NkFunction à tester
                 * @return true si fn.m_callable != nullptr, false sinon
                 * @note Permet la syntaxe symétrique : if (nullptr != fn) { ... }
                 * @note Friend : accède au membre privé m_callable pour la comparaison
                 * @note noexcept : simple comparaison de pointeur, aucune opération risquée
                 */
                friend bool operator!=(NkNullptrT, const NkFunction& fn) noexcept
                {
                    return fn.m_callable != nullptr;
                }


            // ====================================================================
            // SECTION PUBLIQUE : UTILITAIRES DE GESTION
            // ====================================================================
            public:

                /**
                 * @brief Échange le contenu de deux NkFunction en temps constant
                 * @param other Référence vers le NkFunction avec lequel échanger
                 * @note Échange les pointeurs m_callable et m_allocator via mem::NkSwap
                 * @note noexcept garanti : aucun allocation, copie ou opération risquée
                 * @note Complexité O(1) : idéal pour les opérations de swap dans les algorithmes
                 * @note Utilisé par les opérateurs d'affectation pour le pattern copy-and-swap
                 */
                void Swap(NkFunction& other) noexcept
                {
                    mem::NkSwap(m_callable, other.m_callable);
                    mem::NkSwap(m_allocator, other.m_allocator);
                }

                /**
                 * @brief Libère proprement le callable interne et réinitialise à l'état vide
                 * @note Appelle Destroy() sur m_callable si non-null pour la destruction polymorphe
                 * @note Utilise NkResolveFunctionAllocator pour valider l'allocateur avant désallocation
                 * @note Réinitialise m_callable à nullptr après destruction pour éviter les dangling pointers
                 * @note noexcept garanti : aucune opération pouvant lever d'exception
                 * @note Complexité O(n) où n = taille du callable détruit + O(1) pour le reset
                 */
                void Clear() noexcept
                {
                    if (m_callable)
                    {
                        mem::NkAllocator* alloc = detail::NkResolveFunctionAllocator(m_allocator);
                        m_callable->Destroy(alloc);
                        m_callable = nullptr;
                    }
                }


            // ====================================================================
            // SECTION PRIVÉE : MEMBRES DONNÉES
            // ====================================================================
            private:

                CallableBase* m_callable;       ///< Pointeur vers le callable polymorphe encapsulé (nullptr = vide)
                mem::NkAllocator* m_allocator;  ///< Pointeur vers l'allocateur gérant la mémoire du callable

        }; // class NkFunction<R(Args...)>


        // ====================================================================
        // FONCTION NON-MEMBRE : NKSWAP POUR ADL (ARGUMENT-DEPENDENT LOOKUP)
        // ====================================================================

        /**
         * @brief Fonction swap non-membre pour permettre l'usage idiomatique avec ADL
         * @tparam R Type de retour de la signature fonctionnelle
         * @tparam Args... Types des paramètres de la signature fonctionnelle
         * @param a Premier NkFunction à échanger
         * @param b Second NkFunction à échanger
         * @note noexcept : délègue à la méthode membre Swap() qui est noexcept
         * @note Permet l'usage : using nkentseu::NkSwap; NkSwap(fn1, fn2); ou std::swap(fn1, fn2) via ADL
         * @note Complexité O(1) : échange de pointeurs uniquement, aucune allocation
         * @note Utile dans les algorithmes génériques nécessitant des opérations de swap
         */
        template<typename R, typename... Args>
        void NkSwap(NkFunction<R(Args...)>& a, NkFunction<R(Args...)>& b) noexcept
        {
            a.Swap(b);
        }

    } // namespace nkentseu

#endif // NK_MEMORY_NKFUNCTION_H

// ============================================================================
// EXEMPLES D'UTILISATION ET PATTERNS COURANTS
// ============================================================================
/*
 * @section examples Exemples pratiques d'utilisation de NkFunction
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 1 : Utilisation basique avec lambdas et fonctions libres
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkFunction.h"
 * #include <cstdio>
 * 
 * // Fonction libre simple
 * int Add(int a, int b) { return a + b; }
 * 
 * void exempleNkFunctionBasique()
 * {
 *     // Encapsuler un lambda sans capture
 *     nkentseu::NkFunction<void()> salutation = []() {
 *         printf("Bonjour depuis un lambda !\n");
 *     };
 *     salutation();  // Affiche : Bonjour depuis un lambda !
 *     
 *     // Encapsuler une fonction libre
 *     nkentseu::NkFunction<int(int, int)> somme = &Add;
 *     int result = somme(3, 4);  // result = 7
 *     printf("3 + 4 = %d\n", result);
 *     
 *     // Lambda avec capture par valeur
 *     int multiplicateur = 10;
 *     nkentseu::NkFunction<int(int)> scaler = [multiplicateur](int x) {
 *         return x * multiplicateur;
 *     };
 *     printf("5 * 10 = %d\n", scaler(5));  // Affiche : 50
 *     
 *     // Test de validité avant appel
 *     nkentseu::NkFunction<void()> vide;
 *     if (vide.IsValid())
 *     {
 *         vide();  // Ne sera pas exécuté
 *     }
 *     else
 *     {
 *         printf("Fonction vide, appel ignoré\n");
 *     }
 *     
 *     // Réinitialisation via nullptr
 *     salutation = nullptr;
 *     if (salutation == nullptr)
 *     {
 *         printf("Fonction réinitialisée avec succès\n");
 *     }
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 2 : Encapsulation de méthodes membres (const et non-const)
 * --------------------------------------------------------------------------
 * @code
 * class Calculateur {
 * private:
 *     int facteur;
 *     
 * public:
 *     Calculateur(int f) : facteur(f) {}
 *     
 *     // Méthode non-const : modifie l'état interne
 *     int MultiplierEtIncrementer(int x) {
 *         int result = x * facteur;
 *         ++facteur;  // Effet de bord
 *         return result;
 *     }
 *     
 *     // Méthode const : ne modifie pas l'état
 *     int Multiplier(int x) const {
 *         return x * facteur;
 *     }
 *     
 *     int GetFacteur() const { return facteur; }
 * };
 * 
 * void exempleMethodesMembres()
 * {
 *     Calculateur calc(5);
 *     
 *     // Encapsuler une méthode non-const
 *     nkentseu::NkFunction<int(int)> operation = 
 *         nkentseu::NkFunction<int(int)>(&calc, &Calculateur::MultiplierEtIncrementer);
 *     
 *     printf("Resultat 1: %d\n", operation(3));  // 3*5=15, facteur devient 6
 *     printf("Resultat 2: %d\n", operation(4));  // 4*6=24, facteur devient 7
 *     printf("Facteur actuel: %d\n", calc.GetFacteur());  // 7
 *     
 *     // Encapsuler une méthode const
 *     nkentseu::NkFunction<int(int)> lecture = 
 *         nkentseu::NkFunction<int(int)>(&calc, &Calculateur::Multiplier);
 *     
 *     printf("Lecture 1: %d\n", lecture(2));  // 2*7=14, facteur reste 7
 *     printf("Lecture 2: %d\n", lecture(3));  // 3*7=21, facteur reste 7
 *     printf("Facteur après const: %d\n", calc.GetFacteur());  // Toujours 7
 *     
 *     // Copie d'un NkFunction encapsulant une méthode
 *     nkentseu::NkFunction<int(int)> copie = lecture;
 *     printf("Copie: %d\n", copie(10));  // 10*7=70
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 3 : Binding multiple de méthodes indexées
 * --------------------------------------------------------------------------
 * @code
 * class Controleur {
 * public:
 *     void ActionA(int param) { printf("Action A avec %d\n", param); }
 *     void ActionB(int param) { printf("Action B avec %d\n", param); }
 *     void ActionC(int param) { printf("Action C avec %d\n", param); }
 * };
 * 
 * void exempleBindingMultiple()
 * {
 *     Controleur ctrl;
 *     
 *     // Créer un NkFunction pour binding multiple
 *     nkentseu::NkFunction<void(int)> actions(&ctrl);
 *     
 *     // Binder différentes méthodes à différents indices
 *     actions.BindMethod(&ctrl, &Controleur::ActionA, 0);
 *     actions.BindMethod(&ctrl, &Controleur::ActionB, 1);
 *     actions.BindMethod(&ctrl, &Controleur::ActionC, 2);
 *     
 *     // Invoquer par index
 *     actions(0, 42);  // Affiche : Action A avec 42
 *     actions(1, 99);  // Affiche : Action B avec 99
 *     actions(2, 7);   // Affiche : Action C avec 7
 *     
 *     // Invocation sans index : utilise l'index 0 par défaut
 *     actions(123);    // Équivalent à actions(0, 123) -> Action A avec 123
 *     
 *     // Remplacer une méthode à un index existant
 *     actions.BindMethod(&ctrl, &Controleur::ActionB, 0);  // ActionB remplace ActionA à l'index 0
 *     actions(0, 55);  // Affiche maintenant : Action B avec 55
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 4 : Gestion mémoire avec allocateur personnalisé
 * --------------------------------------------------------------------------
 * @code
 * #include "NKMemory/NkPoolAllocator.h"
 * 
 * void exempleAllocateurPersonnalise()
 * {
 *     // Créer un pool allocator pour des allocations groupées rapides
 *     mem::NkPoolAllocator poolAlloc(64 * 1024);  // Pool de 64KB
 *     
 *     // Créer plusieurs NkFunction utilisant le même pool
 *     nkentseu::NkFunction<void()> fn1(
 *         []() { printf("Fonction 1\n"); }, &poolAlloc);
 *     
 *     nkentseu::NkFunction<int(int)> fn2(
 *         [](int x) { return x * 2; }, &poolAlloc);
 *     
 *     // Utiliser les fonctions normalement
 *     fn1();              // Affiche : Fonction 1
 *     int val = fn2(21);  // val = 42
 *     
 *     // Copie : le clone utilise aussi le même allocateur
 *     nkentseu::NkFunction<void()> fn3 = fn1;
 *     fn3();  // Affiche : Fonction 1
 *     
 *     // À la destruction : libération via le pool (très rapide)
 *     // À la destruction du pool : libération en bloc de toute la mémoire
 *     printf("Pool allocator : libération en bloc à la destruction\n");
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 5 : Système de callbacks pour événements de jeu
 * --------------------------------------------------------------------------
 * @code
 * // Système simplifié de gestion d'événements
 * class EventSystem {
 * private:
 *     // Callback pour événement "PlayerDamaged"
 *     nkentseu::NkFunction<void(int damage, const char* source)> onPlayerDamaged;
 *     
 * public:
 *     // Enregistrer un callback
 *     void RegisterOnPlayerDamaged(
 *         nkentseu::NkFunction<void(int, const char*)> callback) {
 *         onPlayerDamaged = callback;
 *     }
 *     
 *     // Déclencher l'événement
 *     void TriggerPlayerDamaged(int damage, const char* source) {
 *         if (onPlayerDamaged.IsValid()) {
 *             onPlayerDamaged(damage, source);
 *         }
 *     }
 *     
 *     // Désenregistrer le callback
 *     void UnregisterOnPlayerDamaged() {
 *         onPlayerDamaged = nullptr;
 *     }
 * };
 * 
 * // Composant UI qui réagit aux dégâts du joueur
 * class DamageUI {
 * private:
 *     int totalDamage;
 *     
 * public:
 *     DamageUI() : totalDamage(0) {}
 *     
 *     void OnPlayerDamaged(int damage, const char* source) {
 *         totalDamage += damage;
 *         printf("UI: %s a infligé %d dégâts. Total: %d\n", 
 *                source, damage, totalDamage);
 *     }
 *     
 *     // Méthode helper pour s'enregistrer facilement
 *     void RegisterTo(EventSystem& events) {
 *         // Binding de méthode membre via NkFunction
 *         events.RegisterOnPlayerDamaged(
 *             nkentseu::NkFunction<void(int, const char*)>(
 *                 this, &DamageUI::OnPlayerDamaged));
 *     }
 * };
 * 
 * void exempleSystemeEvenements()
 * {
 *     EventSystem events;
 *     DamageUI ui;
 *     
 *     // Enregistrer l'UI au système d'événements
 *     ui.RegisterTo(events);
 *     
 *     // Simuler des événements de jeu
 *     events.TriggerPlayerDamaged(10, "Ennemi #1");  // UI: Ennemi #1 a infligé 10 dégâts. Total: 10
 *     events.TriggerPlayerDamaged(5, "Piège");       // UI: Piège a infligé 5 dégâts. Total: 15
 *     events.TriggerPlayerDamaged(20, "Boss");       // UI: Boss a infligé 20 dégâts. Total: 35
 *     
 *     // Désenregistrer l'UI
 *     events.UnregisterOnPlayerDamaged();
 *     events.TriggerPlayerDamaged(100, "Test");      // Aucun effet : callback désenregistré
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * EXEMPLE 6 : Command pattern avec annulation/refaire
 * --------------------------------------------------------------------------
 * @code
 * // Commande encapsulant une action et son inverse
 * struct Command {
 *     nkentseu::NkFunction<void()> execute;
 *     nkentseu::NkFunction<void()> undo;
 *     const char* name;
 *     
 *     Command(const char* n, 
 *             nkentseu::NkFunction<void()> exec,
 *             nkentseu::NkFunction<void()> un)
 *         : execute(exec), undo(un), name(n) {}
 * };
 * 
 * class CommandHistory {
 * private:
 *     nkentseu::NkVector<Command> history;  // Supposons NkVector disponible
 *     usize currentIndex;
 *     
 * public:
 *     CommandHistory() : currentIndex(0) {}
 *     
 *     void ExecuteAndStore(Command cmd) {
 *         if (cmd.execute.IsValid()) {
 *             cmd.execute();
 *             // Tronquer l'historique après la position courante si on a fait undo avant
 *             if (currentIndex < history.Size()) {
 *                 history.Resize(currentIndex);
 *             }
 *             history.PushBack(cmd);
 *             ++currentIndex;
 *         }
 *     }
 *     
 *     void Undo() {
 *         if (currentIndex > 0 && history[currentIndex - 1].undo.IsValid()) {
 *             --currentIndex;
 *             history[currentIndex].undo();
 *         }
 *     }
 *     
 *     void Redo() {
 *         if (currentIndex < history.Size() && history[currentIndex].execute.IsValid()) {
 *             history[currentIndex].execute();
 *             ++currentIndex;
 *         }
 *     }
 * };
 * 
 * void exempleCommandPattern()
 * {
 *     int valeur = 0;
 *     CommandHistory hist;
 *     
 *     // Commande : ajouter 10
 *     hist.ExecuteAndStore(Command(
 *         "Add10",
 *         [&valeur]() { valeur += 10; printf("Valeur: %d\n", valeur); },
 *         [&valeur]() { valeur -= 10; printf("Undo: %d\n", valeur); }
 *     ));  // Valeur: 10
 *     
 *     // Commande : multiplier par 2
 *     int avant = valeur;
 *     hist.ExecuteAndStore(Command(
 *         "Multiply2",
 *         [&valeur, avant]() mutable { 
 *             valeur *= 2; printf("Valeur: %d\n", valeur); 
 *         },
 *         [&valeur, avant]() mutable { 
 *             valeur = avant; printf("Undo: %d\n", valeur); 
 *         }
 *     ));  // Valeur: 20
 *     
 *     // Undo : revient à 10
 *     hist.Undo();  // Undo: 10
 *     
 *     // Redo : revient à 20
 *     hist.Redo();  // Valeur: 20
 *     
 *     // Nouvelle commande après undo : écrase l'historique futur
 *     hist.ExecuteAndStore(Command(
 *         "Add5",
 *         [&valeur]() { valeur += 5; printf("Valeur: %d\n", valeur); },
 *         [&valeur]() { valeur -= 5; printf("Undo: %d\n", valeur); }
 *     ));  // Valeur: 25
 *     
 *     // L'ancienne commande Multiply2 n'est plus accessible via redo
 * }
 * @endcode
 * 
 * --------------------------------------------------------------------------
 * BONNES PRATIQUES ET RECOMMANDATIONS
 * --------------------------------------------------------------------------
 * 
 * 1. CHOIX DE L'ALLOCATEUR :
 *    - Utiliser l'allocateur par défaut pour les cas généraux
 *    - Préférer un pool allocator pour les NkFunction créés/détruits fréquemment
 *    - Pour les systèmes temps réel : allouer tous les callbacks au démarrage pour éviter les allocations dynamiques en production
 *    - Documenter l'allocateur utilisé pour la cohérence mémoire dans les modules
 * 
 * 2. GESTION DES CAPTURES DE LAMBDAS :
 *    - Capture par valeur ([=]) : copie des variables, sûr mais peut être coûteux pour les gros objets
 *    - Capture par référence ([&]) : attention à la durée de vie des références capturées
 *    - Capture mixte ([x, &y]) : explicite et recommandée pour la clarté
 *    - Éviter les captures de this implicites dans les lambdas membres : préférer [this] ou [self = shared_from_this()]
 * 
 * 3. MÉTHODES MEMBRES ET DURÉE DE VIE :
 *    - NkFunction ne gère pas la durée de vie de l'objet pointé : garantir que l'objet reste valide pendant l'usage du callback
 *    - Pour les objets dynamiques : envisager NkFunction avec shared_ptr ou weak_ptr selon la sémantique de possession
 *    - Documenter clairement qui possède l'objet cible d'une méthode membre bindée
 * 
 * 4. PERFORMANCE ET OPTIMISATIONS :
 *    - Invocation : overhead d'un appel virtuel + forwarding des arguments (négligeable dans la plupart des cas)
 *    - Copie : allocation + clonage du callable : éviter les copies inutiles, préférer le move ou les références
 *    - Small-buffer optimization non implémentée : tous les callables sont alloués dynamiquement
 *    - Pour les callbacks très fréquents : envisager des pointeurs de fonction directs si le type est connu à la compilation
 * 
 * 5. SÉCURITÉ ET ROBUSTESSE :
 *    - Appel sur fonction vide : retourne R{} silencieusement, pas d'exception bad_function_call
 *    - Vérifier IsValid() ou utiliser operator bool() avant l'appel si le callback peut être optionnel
 *    - Thread-unsafe par défaut : protéger avec mutex si le même NkFunction est accédé/modifié depuis plusieurs threads
 *    - Pour les callbacks asynchrones : garantir que le callable reste valide jusqu'à l'exécution effective
 * 
 * 6. LIMITATIONS CONNUES (version actuelle) :
 *    - Pas de target() / target_type() : impossible d'interroger le type concret du callable encapsulé (pas de RTTI)
 *    - Pas de small-buffer optimization : overhead d'allocation même pour les petits lambdas sans capture
 *    - Binding multiple : gestion manuelle des indices, pas de nommage sémantique des méthodes bindées
 *    - Pas de composition de fonctions : chaînage de NkFunction via opérateurs non supporté nativement
 * 
 * 7. EXTENSIONS RECOMMANDÉES :
 *    - Ajouter NkFunction::target<T>() pour récupérer le callable concret si le type correspond (avec RTTI optionnel)
 *    - Implémenter un buffer interne de 32-64 bytes pour les petits callables (SBO) et éviter les allocations
 *    - Ajouter NkBind(obj, &Class::Method, args...) pour le partial application et la curryfication
 *    - Supporter les callables noexcept : NkFunction<R(Args...) noexcept> pour les garanties de non-exception
 *    - Ajouter des statistiques : GetAllocationCount(), GetTotalMemory() pour le profiling mémoire
 * 
 * 8. COMPARAISON AVEC std::function :
 *    - Avantages NkFunction : pas de dépendance STL, allocateur personnalisable, binding multiple indexé
 *    - Inconvénients : pas de SBO par défaut, pas de target(), API légèrement différente pour le binding
 *    - Compatibilité : syntaxe d'usage quasi-identique, migration facile depuis std::function
 *    - Performance : similaire en invocation, potentiellement meilleure avec pool allocator pour les créations fréquentes
 * 
 * 9. PATTERNS AVANCÉS :
 *    - Callback chaînés : NkFunction retourne un autre NkFunction pour la composition fonctionnelle
 *    - Priorité de callbacks : vector de NkFunction triés par priorité pour les systèmes d'événements complexes
 *    - Callback conditionnels : NkFunction<bool(Args...)> pour les prédicats dynamiques dans les algorithmes
 *    - Memoization : wrapper NkFunction avec cache des résultats pour les fonctions pures coûteuses
 *    - Retry logic : NkFunction avec politique de réessai en cas d'échec (utile pour les I/O réseau)
 * 
 * 10. DÉBOGAGE ET LOGGING :
 *    - Activer NKENTSEU_LOG_ENABLED pour tracer les appels sur fonctions vides et les échecs d'allocation
 *    - Ajouter un nom optionnel au NkFunction pour l'identification dans les logs : NkFunction<void(), "UpdateCallback">
 *    - Pour les fuites mémoire : valider que Clear() est appelé explicitement si nécessaire avant la destruction
 *    - Utiliser des assertions en debug pour vérifier IsValid() avant les appels critiques
 */

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
//
// Création : 2025-06-10 par TEUGUIA TADJUIDJE Rodolf
// Mise à jour v2.0.0 : 2026-03-14 par Rihen
// Dernière modification : 2026-04-26 (restructuration et documentation complète)
// ============================================================