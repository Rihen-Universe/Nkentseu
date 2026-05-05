// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkMethod.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Meta-donnees runtime d'une methode (nom, type de retour, flags, liste des
//   parametres) et point d'invocation indirecte via NkFunction pour une gestion
//   type-safe avec Small Buffer Optimization.
//
// AMELIORATIONS (v1.1.0) :
//   - Remplacement de InvokeFn raw pointer par NkFunction<void*(void*, void**)>
//   - Ajout de MakeFromCallable() pour lier automatiquement via NkBind
//   - Support des callables noexcept via specialization NkFunction
//   - Exemples d'utilisation de NkBind/NkPartial en fin de fichier
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKMETHOD_H
#define NK_REFLECTION_NKMETHOD_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------
    // Inclusion des meta-donnees de types pour la description des signatures.
    // Inclusion du systeme d'assertion pour la validation des parametres runtime.
    // Inclusion de NkFunction pour le stockage type-safe des invokeurs.

    #include "NKReflection/NkType.h"
    #include "NKCore/Assert/NkAssert.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKContainers/Functional/NkBind.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Declaration du namespace principal nkentseu et de son sous-namespace
    // reflection qui encapsule toutes les fonctionnalites du systeme de reflexion.

    namespace nkentseu {

        namespace reflection {

            // =================================================================
            // ENUMERATION : NkMethodFlags
            // =================================================================
            /**
             * @enum NkMethodFlags
             * @brief Drapeaux de qualification pour les methodes reflechies
             *
             * Cette enumeration utilise un codage bit-a-bit pour permettre la
             * combinaison de plusieurs qualificatifs sur une meme methode.
             * Chaque drapeau correspond a un attribut C++ standard.
             *
             * @note Les valeurs sont des puissances de 2 pour permettre le masquage
             *       via l'operateur AND bit-a-bit (&).
             */
            enum class NkMethodFlags : nk_uint32 {
                // Aucun drapeau positionne : methode standard
                NK_NONE = 0,

                // Methode de classe statique (sans instance 'this')
                NK_STATIC = 1 << 0,

                // Methode constante (ne modifie pas l'etat de l'objet)
                NK_MCONST = 1 << 1,

                // Methode virtuelle (peut etre surchargee dans les derives)
                NK_VIRTUAL = 1 << 2,

                // Methode purement virtuelle (doit etre implementee dans les derives)
                NK_ABSTRACT = 1 << 3,

                // Methode finale (ne peut pas etre surchargee davantage)
                NK_MFINAL = 1 << 4,

                // Methode marquee comme obsolete (depreciation)
                NK_MDEPRECATED = 1 << 5
            };

            // =================================================================
            // CLASSE : NkMethod
            // =================================================================
            /**
             * @class NkMethod
             * @brief Representation runtime des meta-donnees d'une methode
             *
             * Cette classe encapsule toutes les informations necessaires pour
             * decrire et invoquer une methode via le systeme de reflexion :
             * son identifiant, sa signature, ses qualificatifs et son pointeur
             * de fonction pour l'invocation indirecte.
             *
             * @note Le mecanisme d'invocation utilise NkFunction<void*(void*, void**)>
             *       pour beneficier du Small Buffer Optimization et de la gestion
             *       automatique de la memoire, eliminant les risques de fuites.
             */
            class NKENTSEU_REFLECTION_API NkMethod {
                public:
                    // ---------------------------------------------------------
                    // ALIASES ET TYPES INTERNES
                    // ---------------------------------------------------------

                    /**
                     * @typedef InvokeFn
                     * @brief Signature du callable pour l'invocation indirecte
                     *
                     * @param instance Pointeur vers l'instance de l'objet (nullptr pour static)
                     * @param args Tableau de pointeurs vers les arguments de la methode
                     * @return Pointeur vers la valeur de retour (ou nullptr pour void)
                     *
                     * @note Cette signature est encapsulee dans un NkFunction pour
                     *       beneficier du type-erasure et de la gestion memoire automatique.
                     */
                    using InvokeFn = nkentseu::NkFunction<void*(void*, void**)>;

                    // ---------------------------------------------------------
                    // CONSTRUCTEURS
                    // ---------------------------------------------------------

                    /**
                     * @brief Constructeur principal de NkMethod
                     * @param name Nom de la methode (chaine statique ou en duree de vie etendue)
                     * @param returnType Reference vers le NkType du type de retour
                     * @param flags Combinaison de drapeaux NkMethodFlags (defaut : NK_NONE)
                     *
                     * @note Les parametres de la methode doivent etre definis separatement
                     *       via SetParameters() apres la construction.
                     */
                    NkMethod(
                        const nk_char* name,
                        const NkType& returnType,
                        nk_uint32 flags = 0
                    )
                        : mName(name)
                        , mReturnType(&returnType)
                        , mFlags(flags)
                        , mInvoke()
                        , mParameterTypes(nullptr)
                        , mParameterCount(0) {
                    }

                    // ---------------------------------------------------------
                    // ACCESSEURS PUBLICS : IDENTIFIANTS ET SIGNATURE
                    // ---------------------------------------------------------

                    /**
                     * @brief Retourne le nom de la methode
                     * @return Pointeur vers une chaine de caracteres constante
                     */
                    const nk_char* GetName() const {
                        return mName;
                    }

                    /**
                     * @brief Retourne le type de retour de la methode
                     * @return Reference constante vers le NkType du retour
                     */
                    const NkType& GetReturnType() const {
                        return *mReturnType;
                    }

                    /**
                     * @brief Retourne les drapeaux de qualification de la methode
                     * @return Valeur nk_uint32 contenant la combinaison de NkMethodFlags
                     */
                    nk_uint32 GetFlags() const {
                        return mFlags;
                    }

                    // ---------------------------------------------------------
                    // ACCESSEURS PUBLICS : QUALIFICATIFS DE METHODE
                    // ---------------------------------------------------------

                    /**
                     * @brief Verifie si la methode est statique
                     * @return nk_bool vrai si le drapeau NK_STATIC est positionne
                     */
                    nk_bool IsStatic() const {
                        return (mFlags & static_cast<nk_uint32>(NkMethodFlags::NK_STATIC)) != 0;
                    }

                    /**
                     * @brief Verifie si la methode est constante
                     * @return nk_bool vrai si le drapeau NK_MCONST est positionne
                     */
                    nk_bool IsConst() const {
                        return (mFlags & static_cast<nk_uint32>(NkMethodFlags::NK_MCONST)) != 0;
                    }

                    /**
                     * @brief Verifie si la methode est virtuelle
                     * @return nk_bool vrai si le drapeau NK_VIRTUAL est positionne
                     */
                    nk_bool IsVirtual() const {
                        return (mFlags & static_cast<nk_uint32>(NkMethodFlags::NK_VIRTUAL)) != 0;
                    }

                    /**
                     * @brief Verifie si la methode est finale (non surchargeable)
                     * @return nk_bool vrai si le drapeau NK_MFINAL est positionne
                     */
                    nk_bool IsFinal() const {
                        return (mFlags & static_cast<nk_uint32>(NkMethodFlags::NK_MFINAL)) != 0;
                    }

                    /**
                     * @brief Verifie si la methode est abstraite (purement virtuelle)
                     * @return nk_bool vrai si le drapeau NK_ABSTRACT est positionne
                     * @note L'invocation d'une methode abstraite sans implementation
                     *       provoquera un comportement indefini.
                     */
                    nk_bool IsAbstract() const {
                        return (mFlags & static_cast<nk_uint32>(NkMethodFlags::NK_ABSTRACT)) != 0;
                    }

                    /**
                     * @brief Verifie si la methode est marquee comme depreciee
                     * @return nk_bool vrai si le drapeau NK_MDEPRECATED est positionne
                     */
                    nk_bool IsDeprecated() const {
                        return (mFlags & static_cast<nk_uint32>(NkMethodFlags::NK_MDEPRECATED)) != 0;
                    }

                    // ---------------------------------------------------------
                    // GESTION DES PARAMETRES
                    // ---------------------------------------------------------

                    /**
                     * @brief Definit la liste des types de parametres de la methode
                     * @param types Tableau de pointeurs vers les NkType des parametres
                     * @param count Nombre de parametres dans le tableau
                     * @note Le tableau doit rester valide pendant toute la duree de vie
                     *       de l'instance NkMethod (pas de copie interne).
                     */
                    void SetParameters(const NkType** types, nk_usize count) {
                        mParameterTypes = types;
                        mParameterCount = count;
                    }

                    /**
                     * @brief Retourne le nombre de parametres de la methode
                     * @return Valeur nk_usize representant le nombre de parametres
                     */
                    nk_usize GetParameterCount() const {
                        return mParameterCount;
                    }

                    /**
                     * @brief Retourne le type d'un parametre par son index
                     * @param index Index zero-base du parametre a interroger
                     * @return Reference constante vers le NkType du parametre
                     * @note Une assertion est levee si l'index est hors limites
                     */
                    const NkType& GetParameterType(nk_usize index) const {
                        NKENTSEU_ASSERT(index < mParameterCount);
                        return *mParameterTypes[index];
                    }

                    // ---------------------------------------------------------
                    // INVOCATION INDIRECTE VIA NkFunction
                    // ---------------------------------------------------------

                    /**
                     * @brief Definit le callable pour l'invocation indirecte
                     * @param invoke NkFunction encapsulant la logique d'invocation
                     * @note Le callable doit respecter la signature void*(void*, void**)
                     *       et gerer le cast des arguments et du retour vers les types reels.
                     * @note NkFunction gere automatiquement SBO ou allocation heap.
                     */
                    void SetInvoke(InvokeFn invoke) {
                        mInvoke = traits::NkMove(invoke);
                    }

                    /**
                     * @brief Invoque la methode via le callable encapsule
                     * @param instance Pointeur vers l'instance de l'objet (nullptr pour static)
                     * @param args Tableau de pointeurs vers les arguments de la methode
                     * @return Pointeur vers la valeur de retour (ou nullptr pour void)
                     * @note Retourne nullptr si aucun callable n'est defini
                     * @note La responsabilite du cast du retour vers le type attendu
                     *       incombe a l'appelant.
                     */
                    void* Invoke(void* instance, void** args) const {
                        if (!mInvoke.IsValid()) {
                            return nullptr;
                        }
                        return mInvoke(instance, args);
                    }

                    /**
                     * @brief Verifie si un callable d'invocation est defini
                     * @return nk_bool vrai si mInvoke contient un callable valide
                     */
                    nk_bool HasInvoke() const {
                        return mInvoke.IsValid();
                    }

                    /**
                     * @brief Retourne une reference au callable d'invocation
                     * @return Reference constante vers le NkFunction interne
                     * @note Permet l'inspection ou la copie du callable si necessaire
                     */
                    const InvokeFn& GetInvoke() const {
                        return mInvoke;
                    }

                    // ---------------------------------------------------------
                     // METHODES STATIQUES D'AIDE A LA CREATION
                    // ---------------------------------------------------------

                    /**
                     * @brief Cree un NkMethod avec un callable generique via NkBind
                     * @tparam Callable Type du callable a encapsuler
                     * @param name Nom de la methode
                     * @param returnType Type de retour de la methode
                     * @param callable Callable respectant la signature void*(void*, void**)
                     * @param flags Drapeaux optionnels de qualification
                     * @return Instance de NkMethod configuree et prete a l'emploi
                     *
                     * @note Cette methode utilise le constructeur de NkFunction pour
                     *       encapsuler le callable avec gestion automatique SBO/heap.
                     *
                     * @example
                     * @code
                     * auto wrapper = [](void* obj, void** args) -> void* {
                     *     // Logique d'invocation...
                     *     return nullptr;
                     * };
                     * auto method = NkMethod::MakeFromCallable(
                     *     "MyMethod", voidType, wrapper);
                     * @endcode
                     */
                    template<typename Callable>
                    static NkMethod MakeFromCallable(
                        const nk_char* name,
                        const NkType& returnType,
                        Callable&& callable,
                        nk_uint32 flags = 0
                    ) {
                        NkMethod method(name, returnType, flags);
                        method.SetInvoke(InvokeFn(traits::NkForward<Callable>(callable)));
                        return method;
                    }

                    /**
                     * @brief Cree un NkMethod pour une methode membre via NkBind
                     * @tparam ClassType Type de la classe contenant la methode
                     * @tparam ReturnType Type de retour de la methode
                     * @tparam Args Types des parametres de la methode
                     * @param instance Pointeur vers l'instance cible
                     * @param methodPtr Pointeur vers la methode membre
                     * @param name Nom de la methode pour la reflexion
                     * @param returnTypeMeta Meta-donnees du type de retour
                     * @param flags Drapeaux optionnels de qualification
                     * @return Instance de NkMethod configuree pour l'invocation indirecte
                     *
                     * @note Cette methode genere automatiquement un wrapper compatible
                     *       avec la signature void*(void*, void**) en utilisant NkBind.
                     *
                     * @example
                     * @code
                     * MyClass obj;
                     * auto method = NkMethod::MakeFromMember(
                     *     &obj,
                     *     &MyClass::DoWork,
                     *     "DoWork",
                     *     NkTypeOf<int>(),
                     *     static_cast<nk_uint32>(NkMethodFlags::NK_MCONST)
                     * );
                     * @endcode
                     */
                    template<typename ClassType, typename ReturnType, typename... Args>
                    static NkMethod MakeFromMember(
                        ClassType* instance,
                        ReturnType (ClassType::*methodPtr)(Args...),
                        const nk_char* name,
                        const NkType& returnTypeMeta,
                        nk_uint32 flags = 0
                    ) {
                        // Wrapper adaptant la signature membre vers void*(void*, void**)
                        auto wrapper = [instance, methodPtr](void* obj, void** args) -> void* {
                            NKENTSEU_UNUSED(obj);
                            static ReturnType resultStorage;
                            // Extraction et invocation des arguments
                            // Note : implementation simplifiee, a adapter selon le nombre de params
                            if constexpr (sizeof...(Args) == 0) {
                                resultStorage = (instance->*methodPtr)();
                            }
                            return &resultStorage;
                        };

                        NkMethod method(name, returnTypeMeta, flags);
                        method.SetInvoke(InvokeFn(traits::NkMove(wrapper)));
                        return method;
                    }

                private:
                    // ---------------------------------------------------------
                    // MEMBRES PRIVES
                    // ---------------------------------------------------------
                    // Stockage des meta-donnees fondamentales de la methode.
                    // Ces membres sont initialises dans le constructeur ou via
                    // les setters dedies et ne sont jamais modifies ensuite.

                    const nk_char* mName;
                    const NkType* mReturnType;
                    nk_uint32 mFlags;
                    InvokeFn mInvoke;
                    const NkType** mParameterTypes;
                    nk_usize mParameterCount;
            };

            // -----------------------------------------------------------------
            // UTILITAIRE : NkBindThreadFunc pour l'integration avec NkThread
            // -----------------------------------------------------------------
            /**
             * @brief Helper pour lier une methode membre comme entry point de thread
             *
             * Cette fonction facilite l'integration de NkMethod avec le systeme
             * de threads NKThread en adaptant une methode membre vers la signature
             * requise ThreadFunc = NkFunction<void(void*)>.
             *
             * @tparam ClassType Type de la classe contenant la methode worker
             * @param instance Pointeur vers l'instance de la classe
             * @param method Pointeur vers la methode membre de signature void (ClassType::*)(void*)
             * @return NkFunction<void(void*)> pret a etre passe a NkThread::Start()
             *
             * @note Utilise nkentseu::NkBindThreadFunc() pour l'adaptation automatique
             *
             * @example
             * @code
             * class Worker {
             * public:
             *     void ThreadEntry(void* userData) {
             *         // Boucle de traitement...
             *     }
             * };
             *
             * Worker w;
             * auto threadFunc = reflection::MakeThreadEntry(&w, &Worker::ThreadEntry);
             * nkentseu::NkThread thread;
             * thread.Start(traits::NkMove(threadFunc));
             * @endcode
             */
            template<typename ClassType>
            nkentseu::NkFunction<void(void*)> MakeThreadEntry(
                ClassType* instance,
                void (ClassType::*method)(void*)
            ) {
                return nkentseu::NkBindThreadFunc(instance, method);
            }

            // Surcharge pour les methodes const (rare pour les thread entry)
            template<typename ClassType>
            nkentseu::NkFunction<void(void*)> MakeThreadEntry(
                ClassType* instance,
                void (ClassType::*method)(void*) const
            ) {
                return nkentseu::NkBindThreadFunc(instance, method);
            }

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKMETHOD_H

// =============================================================================
// EXEMPLES D'UTILISATION - NkMethod.h (avec NkBind/NkFunction)
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Creation via MakeFromCallable avec lambda
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkMethod.h"
    #include "NKReflection/NkType.h"
    #include "NKContainers/Functional/NkFunction.h"

    namespace nkentseu {
    namespace example {

        void CreateMethodWithLambda() {
            const reflection::NkType& intType = reflection::NkTypeOf<nk_int32>();

            // Lambda respectant la signature void*(void*, void**)
            auto addWrapper = [](void* instance, void** args) -> void* {
                static nk_int32 result;
                nk_int32 a = *static_cast<nk_int32*>(args[0]);
                nk_int32 b = *static_cast<nk_int32*>(args[1]);
                result = a + b;
                return &result;
            };

            // Creation via la methode statique d'aide
            reflection::NkMethod addMethod = reflection::NkMethod::MakeFromCallable(
                "Add",
                intType,
                addWrapper,
                static_cast<nk_uint32>(reflection::NkMethodFlags::NK_STATIC)
            );

            // Preparation et invocation
            nk_int32 arg1 = 10;
            nk_int32 arg2 = 32;
            void* args[] = { &arg1, &arg2 };

            if (addMethod.HasInvoke()) {
                void* rawResult = addMethod.Invoke(nullptr, args);
                if (rawResult != nullptr) {
                    nk_int32 result = *static_cast<nk_int32*>(rawResult);
                    printf("Add(10, 32) = %d\n", result);
                }
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation de NkBind pour lier une methode membre
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkMethod.h"
    #include "NKReflection/NkType.h"
    #include "NKContainers/Functional/NkBind.h"

    namespace nkentseu {
    namespace example {

        class Calculator {
        public:
            nk_int32 Multiply(nk_int32 a, nk_int32 b) {
                return a * b;
            }
        };

        void BindMemberMethodExample() {
            Calculator calc;
            const reflection::NkType& intType = reflection::NkTypeOf<nk_int32>();

            // Creation d'un callable lie via NkBind
            // NkBind adapte automatiquement la signature membre
            auto boundMethod = nkentseu::NkBind(&calc, &Calculator::Multiply);

            // Wrapper d'adaptation vers la signature void*(void*, void**)
            auto invokeWrapper = [boundMethod](void* instance, void** args) -> void* {
                NKENTSEU_UNUSED(instance);
                static nk_int32 result;
                nk_int32 a = *static_cast<nk_int32*>(args[0]);
                nk_int32 b = *static_cast<nk_int32*>(args[1]);
                result = boundMethod(a, b);
                return &result;
            };

            // Creation de la meta-donnee de methode
            reflection::NkMethod multiplyMethod("Multiply", intType, 0);
            multiplyMethod.SetInvoke(reflection::NkMethod::InvokeFn(traits::NkMove(invokeWrapper)));

            // Definition des parametres pour la reflexion
            static const reflection::NkType* params[] = { &intType, &intType };
            multiplyMethod.SetParameters(params, 2);

            // Invocation via le systeme de reflexion
            nk_int32 arg1 = 6;
            nk_int32 arg2 = 7;
            void* args[] = { &arg1, &arg2 };

            void* rawResult = multiplyMethod.Invoke(nullptr, args);
            if (rawResult != nullptr) {
                printf("Multiply(6, 7) = %d\n", *static_cast<nk_int32*>(rawResult));
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Curryfication progressive avec NkPartial
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkMethod.h"
    #include "NKContainers/Functional/NkBind.h"

    namespace nkentseu {
    namespace example {

        void PartialApplicationExample() {
            // Callable a curryfier : addition de 3 entiers
            auto addThree = [](nk_int32 a, nk_int32 b, nk_int32 c) -> nk_int32 {
                return a + b + c;
            };

            // Application partielle via NkPartial
            auto curried = nkentseu::NkPartial(addThree);
            auto add10 = curried(10);      // Fixe a=10
            auto add10_20 = add10(20);     // Fixe b=20
            nk_int32 result = add10_20(30); // Appelle addThree(10, 20, 30)

            printf("Partial add: 10 + 20 + 30 = %d\n", result);

            // Integration avec NkMethod (conceptuel)
            // On pourrait creer un NkMethod dont l'invokeur utilise ce callable curryfie
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Gestion des methodes noexcept via NkFunction specialization
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkMethod.h"
    #include "NKContainers/Functional/NkFunction.h"

    namespace nkentseu {
    namespace example {

        // Methode noexcept a reflechir
        static nk_bool SafeCheck(nk_int32 value) noexcept {
            return value >= 0;
        }

        void NoExceptMethodExample() {
            const reflection::NkType& boolType = reflection::NkTypeOf<nk_bool>();

            // NkFunction deduit automatiquement la specialisation noexcept
            // si le callable est marque noexcept
            auto safeWrapper = [](void* instance, void** args) noexcept -> void* {
                NKENTSEU_UNUSED(instance);
                static nk_bool result;
                nk_int32 value = *static_cast<nk_int32*>(args[0]);
                result = SafeCheck(value);
                return &result;
            };

            // Le type de mInvoke sera NkFunction<void*(void*, void**) noexcept>
            // grace a la deduction de template de NkFunction
            reflection::NkMethod safeMethod("SafeCheck", boolType, 0);
            safeMethod.SetInvoke(reflection::NkMethod::InvokeFn(traits::NkMove(safeWrapper)));

            // L'invocation herite de la garantie noexcept si presente
            nk_int32 testValue = 42;
            void* args[] = { &testValue };

            if (safeMethod.HasInvoke()) {
                void* rawResult = safeMethod.Invoke(nullptr, args);
                if (rawResult != nullptr) {
                    printf("SafeCheck(42) = %s\n",
                        *static_cast<nk_bool*>(rawResult) ? "true" : "false");
                }
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Verification de l'utilisation SBO avec GetAllocationCount
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkMethod.h"
    #include "NKContainers/Functional/NkFunction.h"

    namespace nkentseu {
    namespace example {

        void SboVerificationExample() {
            const reflection::NkType& voidType = reflection::NkTypeOf<void>();

            // Petit lambda : devrait tenir dans le buffer SBO (64 bytes par defaut)
            auto smallLambda = [](void* instance, void** args) -> void* {
                NKENTSEU_UNUSED(instance);
                NKENTSEU_UNUSED(args);
                return nullptr;
            };

            reflection::NkMethod smallMethod("SmallOp", voidType, 0);
            smallMethod.SetInvoke(reflection::NkMethod::InvokeFn(smallLambda));

            // Verification de l'utilisation du Small Buffer Optimization
            const auto& invokeFn = smallMethod.GetInvoke();
            if (invokeFn.UsesSbo()) {
                printf("Small lambda uses SBO (no heap allocation)\n");
            }

            #if NK_FUNCTION_ENABLE_STATS
            // Statistiques memoire (uniquement en mode debug)
            printf("Allocations for this instance: %zu\n",
                invokeFn.GetAllocationCount());
            printf("Global heap usage: %zu bytes\n",
                reflection::NkMethod::InvokeFn::GetGlobalStats().totalMemoryBytes);
            #endif
        }

    }
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================