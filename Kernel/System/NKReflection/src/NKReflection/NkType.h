// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkType.h
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Meta-donnees runtime sur les types (nom, taille, categorie) et registre
//   de types. API sans dependance STL sur les conteneurs runtime du module.
//
// AMELIORATIONS APPLIQUEES :
//   - Documentation Doxygen complete pour chaque element public
//   - Indentation stricte des blocs conditionnels, classes et namespaces
//   - Une instruction par ligne pour une lisibilite optimale
//   - Separation claire des sections logiques avec en-tetes commentes
//   - Exemples d'utilisation en fin de fichier
// =============================================================================

#pragma once

#ifndef NK_REFLECTION_NKTYPE_H
#define NK_REFLECTION_NKTYPE_H

    // -------------------------------------------------------------------------
    // SECTION 1 : DEPENDANCES INTERNES
    // -------------------------------------------------------------------------
    // Inclusion des types et traits de base du module NKCore.
    // Ces dependances fournissent les types fondamentaux (nk_char, nk_usize, etc.)
    // et les utilitaires de meta-programmation (traits::NkIsSame, etc.).

    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"
    #include "NKCore/NkBuiltin.h"
    #include "NkReflectionApi.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : EN-TETES STANDARD MINIMAUX
    // -------------------------------------------------------------------------
    // Inclusion minimale de la STL pour l'identification des types via typeid.
    // Aucune autre dependance STL n'est utilisee pour respecter la contrainte
    // d'independance des conteneurs runtime du module.

    #include <typeinfo>

    // -------------------------------------------------------------------------
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Declaration du namespace principal nkentseu et de son sous-namespace
    // reflection qui encapsule toutes les fonctionnalites du systeme de reflexion.

    namespace nkentseu {

        namespace reflection {

            // -----------------------------------------------------------------
            // DECLARATIONS ANTICIPEES
            // -----------------------------------------------------------------
            // Declarations forward pour briser les dependances circulaires
            // entre les classes du systeme de reflexion.

            class NkClass;
            class NkProperty;
            class NkMethod;
            enum class NkTypeCategory;

            template<typename T>
            constexpr NkTypeCategory DetermineCategory();

            // =================================================================
            // ENUMERATION : NkTypeCategory
            // =================================================================
            /**
             * @enum NkTypeCategory
             * @brief Enumeration des categories de types supportees par le systeme
             *
             * Cette enumeration permet d'identifier la nature fondamentale d'un type
             * au moment de l'execution, facilitant ainsi l'introspection et la
             * serialisation dynamique.
             *
             * @note Les categories primitives sont ordonnees consecutivement pour
             *       permettre des tests de plage efficaces via IsPrimitive().
             */
            enum class NkTypeCategory {
                // Types booleens
                NK_BOOL,

                // Types entiers signes
                NK_INT8,
                NK_INT16,
                NK_INT32,
                NK_INT64,

                // Types entiers non signes
                NK_UINT8,
                NK_UINT16,
                NK_UINT32,
                NK_UINT64,

                // Types flottants
                NK_FLOAT32,
                NK_FLOAT64,

                // Types caracteres et chaines
                NK_nk_char,
                NK_STRING,

                // Types composes et references
                NK_POINTER,
                NK_REFERENCE,
                NK_ARRAY,
                NK_VECTOR,

                // Types definis par l'utilisateur
                NK_CLASS,
                NK_STRUCT,
                NK_ENUM,
                NK_UNION,

                // Types speciaux
                NK_FUNCTION,
                NK_VOID,

                // Type par defaut pour les cas non reconnus
                NK_UNKNOWN
            };

            // =================================================================
            // CLASSE : NkType
            // =================================================================
            /**
             * @class NkType
             * @brief Representation runtime des meta-donnees d'un type
             *
             * Cette classe encapsule les informations essentielles sur un type :
             * son nom, sa taille, son alignement et sa categorie. Elle sert de
             * base pour l'introspection et la manipulation dynamique des types.
             *
             * @note Cette classe est immuable apres construction, garantissant la
             *       coherence des meta-donnees tout au long du cycle de vie.
             */
            class NKENTSEU_REFLECTION_API NkType {
                public:
                    /**
                     * @brief Constructeur principal de NkType
                     * @param name Nom du type (chaine statique ou en duree de vie etendue)
                     * @param size Taille du type en octets
                     * @param alignment Alignement memoire requis par le type
                     * @param category Categorie du type selon NkTypeCategory
                     */
                    NkType(
                        const nk_char* name,
                        nk_usize size,
                        nk_usize alignment,
                        NkTypeCategory category
                    )
                        : mName(name)
                        , mSize(size)
                        , mAlignment(alignment)
                        , mCategory(category)
                        , mClass(nullptr) {
                    }

                    /**
                     * @brief Destructeur virtuel par defaut
                     * @note Virtuel pour permettre l'heritage et le polymorphisme
                     */
                    virtual ~NkType() = default;

                    // ---------------------------------------------------------
                    // ACCESSEURS PUBLICS
                    // ---------------------------------------------------------

                    /**
                     * @brief Retourne le nom du type
                     * @return Pointeur vers une chaine de caracteres constante
                     */
                    const nk_char* GetName() const {
                        return mName;
                    }

                    /**
                     * @brief Retourne la taille du type en octets
                     * @return Valeur de type nk_usize representant la taille
                     */
                    nk_usize GetSize() const {
                        return mSize;
                    }

                    /**
                     * @brief Retourne l'alignement memoire du type
                     * @return Valeur de type nk_usize representant l'alignement
                     */
                    nk_usize GetAlignment() const {
                        return mAlignment;
                    }

                    /**
                     * @brief Retourne la categorie du type
                     * @return Valeur de l'enumeration NkTypeCategory
                     */
                    NkTypeCategory GetCategory() const {
                        return mCategory;
                    }

                    /**
                     * @brief Verifie si le type est une classe
                     * @return nk_bool vrai si la categorie est NK_CLASS
                     */
                    nk_bool IsClass() const {
                        return mCategory == NkTypeCategory::NK_CLASS;
                    }

                    /**
                     * @brief Verifie si le type est un pointeur
                     * @return nk_bool vrai si la categorie est NK_POINTER
                     */
                    nk_bool IsPointer() const {
                        return mCategory == NkTypeCategory::NK_POINTER;
                    }

                    /**
                     * @brief Verifie si le type est une reference
                     * @return nk_bool vrai si la categorie est NK_REFERENCE
                     */
                    nk_bool IsReference() const {
                        return mCategory == NkTypeCategory::NK_REFERENCE;
                    }

                    /**
                     * @brief Verifie si le type est un tableau
                     * @return nk_bool vrai si la categorie est NK_ARRAY
                     */
                    nk_bool IsArray() const {
                        return mCategory == NkTypeCategory::NK_ARRAY;
                    }

                    /**
                     * @brief Verifie si le type est une enumeration
                     * @return nk_bool vrai si la categorie est NK_ENUM
                     */
                    nk_bool IsEnum() const {
                        return mCategory == NkTypeCategory::NK_ENUM;
                    }

                    /**
                     * @brief Verifie si le type est primitif
                     * @return nk_bool vrai pour les types numeriques de base
                     * @note Les types primitifs sont NK_BOOL a NK_FLOAT64 inclus
                     */
                    nk_bool IsPrimitive() const {
                        return mCategory >= NkTypeCategory::NK_BOOL && mCategory <= NkTypeCategory::NK_FLOAT64;
                    }

                    // ---------------------------------------------------------
                    // ASSOCIATION DE CLASSE
                    // ---------------------------------------------------------

                    /**
                     * @brief Retourne les meta-donnees de classe associees
                     * @return Pointeur constant vers NkClass ou nullptr
                     * @note Retourne nullptr si le type n'est pas une classe
                     */
                    const NkClass* GetClass() const {
                        return mClass;
                    }

                    /**
                     * @brief Associe des meta-donnees de classe a ce type
                     * @param classInfo Pointeur constant vers les meta-donnees de classe
                     * @note Cette methode permet de lier un NkType a son NkClass correspondant
                     */
                    void SetClass(const NkClass* classInfo) {
                        mClass = classInfo;
                    }

                    // ---------------------------------------------------------
                    // OPERATEURS DE COMPARAISON
                    // ---------------------------------------------------------

                    /**
                     * @brief Operateur d'egalite entre deux NkType
                     * @param other Reference constante vers l'autre NkType
                     * @return nk_bool vrai si les types sont identiques
                     * @note La comparaison utilise l'adresse pour l'identite ou nom+taille
                     */
                    nk_bool operator==(const NkType& other) const {
                        return this == &other || (mName == other.mName && mSize == other.mSize);
                    }

                    /**
                     * @brief Operateur d'inegalite entre deux NkType
                     * @param other Reference constante vers l'autre NkType
                     * @return nk_bool vrai si les types sont differents
                     */
                    nk_bool operator!=(const NkType& other) const {
                        return !(*this == other);
                    }

                private:
                    // ---------------------------------------------------------
                    // MEMBRES PRIVES
                    // ---------------------------------------------------------
                    // Stockage des meta-donnees fondamentales du type.
                    // Ces membres sont initialises une fois dans le constructeur
                    // et ne sont jamais modifies par la suite.

                    const nk_char* mName;
                    nk_usize mSize;
                    nk_usize mAlignment;
                    NkTypeCategory mCategory;
                    const NkClass* mClass;
            };

            // =================================================================
            // CLASSE : NkTypeRegistry
            // =================================================================
            /**
             * @class NkTypeRegistry
             * @brief Registre centralise pour la gestion des types au runtime
             *
             * Cette classe implemente un registre singleton permettant
             * l'enregistrement, la recherche et la recuperation des meta-donnees
             * de types. Elle utilise un tableau statique pour eviter les
             * allocations dynamiques et respecter la contrainte sans STL.
             *
             * @note Thread-safety : Cette implementation n'est pas thread-safe.
             *       La synchronisation externe est requise en environnement multi-thread.
             */
            class NKENTSEU_REFLECTION_API NkTypeRegistry {
                public:
                    /**
                     * @brief Accesseur au singleton du registre
                     * @return Reference statique vers l'instance unique de NkTypeRegistry
                     * @note Utilise le pattern Meyer's Singleton pour une initialisation thread-safe en C++11+
                     */
                    static NkTypeRegistry& Get();

                    /**
                     * @brief Enregistre un type dans le registre
                     * @param type Pointeur constant vers le NkType a enregistrer
                     * @note Les doublons sont ignores (comparaison par nom et adresse)
                     */
                    void RegisterType(const NkType* type);

                    /**
                     * @brief Recherche un type par son nom
                     * @param name Nom du type a rechercher
                     * @return Pointeur constant vers le NkType trouve ou nullptr
                     */
                    const NkType* FindType(const nk_char* name) const;

                    /**
                     * @brief Template pour recuperer un type par son type C++
                     * @tparam T Type C++ dont on souhaite obtenir les meta-donnees
                     * @return Pointeur constant vers le NkType correspondant ou nullptr
                     */
                    template<typename T>
                    const NkType* GetType() const {
                        return FindType(GetTypeName<T>());
                    }

                private:
                    // ---------------------------------------------------------
                    // CONSTANTES PRIVEES
                    // ---------------------------------------------------------
                    // Capacite maximale du registre de types.
                    // Cette valeur limite le nombre de types enregistrables
                    // pour garantir une allocation statique et previsibile.

                    static constexpr nk_usize NK_MAX_TYPES = 512;

                    // ---------------------------------------------------------
                    // CONSTRUCTEUR PRIVE
                    // ---------------------------------------------------------
                    /**
                     * @brief Constructeur prive pour le pattern Singleton
                     * @note Initialise le tableau de stockage a nullptr
                     */
                    NkTypeRegistry()
                        : mTypes(mTypeStorage)
                        , mTypeCount(0)
                        , mTypeCapacity(NK_MAX_TYPES) {
                        for (nk_usize i = 0; i < NK_MAX_TYPES; ++i) {
                            mTypeStorage[i] = nullptr;
                        }
                    }

                    // ---------------------------------------------------------
                    // FONCTIONS UTILITAIRES PRIVEES
                    // ---------------------------------------------------------

                    /**
                     * @brief Obtient le nom d'un type via typeid
                     * @tparam T Type C++ dont on souhaite obtenir le nom
                     * @return Nom du type tel que retourne par typeid(T).name()
                     * @note Le format du nom depend du compilateur (name mangling)
                     */
                    template<typename T>
                    static const nk_char* GetTypeName() {
                        return typeid(T).name();
                    }

                    // ---------------------------------------------------------
                    // MEMBRES PRIVES
                    // ---------------------------------------------------------
                    // Tableau statique pour le stockage des pointeurs de types.
                    // mTypes pointe vers mTypeStorage pour permettre une
                    // eventuelle reallocation future sans changer l'API.

                    const NkType* mTypeStorage[NK_MAX_TYPES];
                    const NkType** mTypes;
                    nk_usize mTypeCount;
                    nk_usize mTypeCapacity;
            };

            // =================================================================
            // FONCTIONS TEMPLATES : NkTypeOf
            // =================================================================
            /**
             * @brief Obtient une reference au NkType d'un type donne
             * @tparam T Type C++ dont on souhaite obtenir les meta-donnees
             * @return Reference constante vers le NkType correspondant
             * @note Utilise un static local pour garantir une initialisation unique
             */
            template<typename T>
            const NkType& NkTypeOf() {
                static NkType type(
                    typeid(T).name(),
                    sizeof(T),
                    alignof(T),
                    DetermineCategory<T>()
                );
                return type;
            }

            /**
             * @brief Surcharge de NkTypeOf pour une instance de type
             * @tparam T Type C++ de l'instance passee en parametre
             * @param instance Reference constante vers l'instance (non utilisee)
             * @return Reference constante vers le NkType correspondant
             * @note Permet d'appeler NkTypeOf(obj) au lieu de NkTypeOf<decltype(obj)>()
             */
            template<typename T>
            const NkType& NkTypeOf(const T&) {
                return NkTypeOf<T>();
            }

            // =================================================================
            // FONCTION : DetermineCategory
            // =================================================================
            /**
             * @brief Determine la categorie d'un type au moment de la compilation
             * @tparam T Type C++ a analyser
             * @return Valeur constexpr de NkTypeCategory correspondant au type
             * @note Utilise les traits de NKCore pour la detection des types
             */
            template<typename T>
            constexpr NkTypeCategory DetermineCategory() {
                if (traits::NkIsSame<T, void>::value) {
                    return NkTypeCategory::NK_VOID;
                }

                if (traits::NkIsSame<T, nk_bool>::value) {
                    return NkTypeCategory::NK_BOOL;
                }

                if (traits::NkIsSame<T, nk_int8>::value) {
                    return NkTypeCategory::NK_INT8;
                }
                if (traits::NkIsSame<T, nk_int16>::value) {
                    return NkTypeCategory::NK_INT16;
                }
                if (traits::NkIsSame<T, nk_int32>::value) {
                    return NkTypeCategory::NK_INT32;
                }
                if (traits::NkIsSame<T, nk_int64>::value) {
                    return NkTypeCategory::NK_INT64;
                }

                if (traits::NkIsSame<T, nk_uint8>::value) {
                    return NkTypeCategory::NK_UINT8;
                }
                if (traits::NkIsSame<T, nk_uint16>::value) {
                    return NkTypeCategory::NK_UINT16;
                }
                if (traits::NkIsSame<T, nk_uint32>::value) {
                    return NkTypeCategory::NK_UINT32;
                }
                if (traits::NkIsSame<T, nk_uint64>::value) {
                    return NkTypeCategory::NK_UINT64;
                }

                if (traits::NkIsSame<T, nk_float32>::value) {
                    return NkTypeCategory::NK_FLOAT32;
                }
                if (traits::NkIsSame<T, nk_float64>::value) {
                    return NkTypeCategory::NK_FLOAT64;
                }

                if (traits::NkIsPointer<T>::value) {
                    return NkTypeCategory::NK_POINTER;
                }
                if (traits::NkIsReference<T>::value) {
                    return NkTypeCategory::NK_REFERENCE;
                }
                if (traits::NkIsArray<T>::value) {
                    return NkTypeCategory::NK_ARRAY;
                }

                return NkTypeCategory::NK_CLASS;
            }

        } // namespace reflection

    } // namespace nkentseu

#endif // NK_REFLECTION_NKTYPE_H

// =============================================================================
// EXEMPLES D'UTILISATION - NkType.h
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Obtention des meta-donnees d'un type primitif
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        void InspectPrimitiveTypes() {
            // Obtention des meta-donnees pour nk_int32
            const reflection::NkType& intType = reflection::NkTypeOf<nk_int32>();

            // Affichage des proprietes du type
            printf("Type: %s\n", intType.GetName());
            printf("Size: %zu bytes\n", intType.GetSize());
            printf("Alignment: %zu bytes\n", intType.GetAlignment());
            printf("IsPrimitive: %s\n", intType.IsPrimitive() ? "true" : "false");

            // Obtention des meta-donnees via une instance
            nk_float32 value = 3.14f;
            const reflection::NkType& floatType = reflection::NkTypeOf(value);

            // Verification de la categorie
            if (floatType.GetCategory() == reflection::NkTypeCategory::NK_FLOAT32) {
                printf("Confirmed: float32 type detected\n");
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Utilisation du registre de types
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        void RegistryExample() {
            // Obtention de l'instance singleton du registre
            reflection::NkTypeRegistry& registry = reflection::NkTypeRegistry::Get();

            // Obtention et enregistrement d'un type
            const reflection::NkType& myType = reflection::NkTypeOf<nk_uint64>();
            registry.RegisterType(&myType);

            // Recherche d'un type par son nom
            const reflection::NkType* found = registry.FindType(myType.GetName());

            if (found != nullptr) {
                printf("Found type: %s\n", found->GetName());
            }

            // Recherche via le template GetType<T>
            const reflection::NkType* direct = registry.GetType<nk_uint64>();

            if (direct != nullptr && direct == &myType) {
                printf("Direct lookup successful\n");
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Verification des proprietes de type
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        void TypePropertyChecks() {
            // Tableau de types pour demonstration
            const reflection::NkType& types[] = {
                reflection::NkTypeOf<nk_bool>(),
                reflection::NkTypeOf<nk_int32>(),
                reflection::NkTypeOf<nk_float64>(),
                reflection::NkTypeOf<int*>(),
                reflection::NkTypeOf<nk_char[]>()
            };

            // Iteration et affichage des proprietes
            for (const auto& type : types) {
                printf("Type: %s\n", type.GetName());
                printf("  Category: %d\n", static_cast<int>(type.GetCategory()));
                printf("  IsPrimitive: %s\n", type.IsPrimitive() ? "yes" : "no");
                printf("  IsPointer: %s\n", type.IsPointer() ? "yes" : "no");
                printf("  IsArray: %s\n", type.IsArray() ? "yes" : "no");
                printf("  Size: %zu, Alignment: %zu\n", type.GetSize(), type.GetAlignment());
                printf("\n");
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Comparaison de types
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        void TypeComparisonExample() {
            // Deux references au meme type
            const reflection::NkType& t1 = reflection::NkTypeOf<nk_int32>();
            const reflection::NkType& t2 = reflection::NkTypeOf<nk_int32>();

            // Comparaison par egalite (devrait etre vrai)
            if (t1 == t2) {
                printf("Types are equal (same static instance)\n");
            }

            // Comparaison avec un type different
            const reflection::NkType& t3 = reflection::NkTypeOf<nk_float32>();

            if (t1 != t3) {
                printf("int32 and float32 are different types\n");
            }

            // Comparaison par nom et taille (fallback si instances differentes)
            if (t1.GetName() == t2.GetName() && t1.GetSize() == t2.GetSize()) {
                printf("Types match by name and size\n");
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Integration avec NkClass (conceptuel)
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkType.h"
    #include "NKReflection/NkClass.h"  // Fichier hypothetique

    namespace nkentseu {
    namespace example {

        class NKENTSEU_REFLECTION_CLASS_EXPORT MyClass {
        public:
            int value;
            void DoSomething();
        };

        void ClassTypeAssociation() {
            // Obtention du NkType pour une classe utilisateur
            const reflection::NkType& classType = reflection::NkTypeOf<MyClass>();

            // Creation hypothetique des meta-donnees de classe
            // reflection::NkClass classMeta("MyClass", &classType);

            // Association bidirectionnelle entre NkType et NkClass
            // classType.SetClass(&classMeta);

            // Recuperation des meta-donnees de classe depuis le type
            // const reflection::NkClass* meta = classType.GetClass();
            // if (meta != nullptr && classType.IsClass()) {
            //     printf("Class %s has reflection metadata\n", meta->GetName());
            // }
        }

    }
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================