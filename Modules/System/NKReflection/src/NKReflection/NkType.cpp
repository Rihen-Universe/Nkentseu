// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkType.cpp
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.0.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Implementation du registre runtime des types.
//
// AMELIORATIONS APPLIQUEES :
//   - Documentation Doxygen pour chaque fonction publique
//   - Indentation stricte des blocs et namespaces
//   - Une instruction par ligne pour une lisibilite optimale
//   - Commentaires explicatifs pour la logique de validation
//   - Exemples d'utilisation en fin de fichier
// =============================================================================

// -------------------------------------------------------------------------
// INCLUSION DE L'EN-TETE CORRESPONDANT
// -------------------------------------------------------------------------
// Inclusion du fichier d'en-tete pour verifier la coherence des declarations.

#include "NKReflection/NkType.h"

// -------------------------------------------------------------------------
// EN-TETES STANDARD MINIMAUX
// -------------------------------------------------------------------------
// Inclusion de cstring pour l'utilisation de strcmp dans les comparaisons
// de noms de types. Aucune autre dependance externe n'est necessaire.

#include <cstring>

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------
// Implementation des methodes dans le namespace reflection de nkentseu.

namespace nkentseu {

    namespace reflection {

        // =====================================================================
        // IMPLEMENTATION : NkTypeRegistry::Get
        // =====================================================================
        /**
         * @brief Implementation de l'accesseur au singleton
         * @return Reference statique vers l'instance unique de NkTypeRegistry
         * @note Pattern Meyer's Singleton : initialisation thread-safe en C++11+
         */
        NkTypeRegistry& NkTypeRegistry::Get() {
            static NkTypeRegistry registry;
            return registry;
        }

        // =====================================================================
        // IMPLEMENTATION : NkTypeRegistry::RegisterType
        // =====================================================================
        /**
         * @brief Enregistre un type dans le registre avec validation des doublons
         * @param type Pointeur constant vers le NkType a enregistrer
         *
         * Logique d'enregistrement :
         *  1. Validation des parametres d'entree (nullptr check)
         *  2. Verification de l'absence de doublon par adresse
         *  3. Verification de l'absence de doublon par nom de type
         *  4. Verification de la capacite disponible dans le registre
         *  5. Insertion du type dans le tableau interne
         *
         * @note Complexite : O(n) ou n = nombre de types deja enregistres
         * @note Les types avec le meme nom mais des adresses differentes sont
         *       consideres comme des doublons et ignores.
         */
        void NkTypeRegistry::RegisterType(const NkType* type) {
            // Validation du parametre d'entree
            if (!type || !type->GetName()) {
                return;
            }

            // Verification des doublons par adresse ou par nom
            for (nk_usize i = 0; i < mTypeCount; ++i) {
                // Comparaison par adresse de pointeur
                if (mTypes[i] == type) {
                    return;
                }

                // Comparaison par nom de type si les deux sont valides
                if (mTypes[i] && mTypes[i]->GetName() && ::strcmp(mTypes[i]->GetName(), type->GetName()) == 0) {
                    return;
                }
            }

            // Verification de la capacite du registre
            if (mTypeCount >= mTypeCapacity) {
                return;
            }

            // Insertion du type dans le tableau et increment du compteur
            mTypes[mTypeCount++] = type;
        }

        // =====================================================================
        // IMPLEMENTATION : NkTypeRegistry::FindType
        // =====================================================================
        /**
         * @brief Recherche lineaire d'un type par son nom dans le registre
         * @param name Nom du type a rechercher (chaine C constante)
         * @return Pointeur constant vers le NkType trouve ou nullptr si absent
         *
         * Logique de recherche :
         *  1. Validation du parametre de recherche (nullptr ou chaine vide)
         *  2. Iteration lineaire sur les types enregistres
         *  3. Comparaison des noms via strcmp pour l'egalite
         *  4. Retour du premier type correspondant ou nullptr
         *
         * @note Complexite : O(n) ou n = nombre de types enregistres
         * @note La comparaison utilise strcmp pour une egalite exacte des noms
         */
        const NkType* NkTypeRegistry::FindType(const nk_char* name) const {
            // Validation du parametre de recherche
            if (!name || name[0] == '\0') {
                return nullptr;
            }

            // Recherche lineaire dans le tableau des types
            for (nk_usize i = 0; i < mTypeCount; ++i) {
                // Recuperation du type courant
                const NkType* type = mTypes[i];

                // Saut si le type ou son nom est invalide
                if (!type || !type->GetName()) {
                    continue;
                }

                // Comparaison des noms pour trouver une correspondance
                if (::strcmp(type->GetName(), name) == 0) {
                    return type;
                }
            }

            // Aucun type correspondant trouve
            return nullptr;
        }

    } // namespace reflection

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION - NkType.cpp
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Enregistrement et recherche basique
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        void BasicRegistryUsage() {
            // Reference au singleton du registre
            reflection::NkTypeRegistry& registry = reflection::NkTypeRegistry::Get();

            // Types a enregistrer
            const reflection::NkType& intType = reflection::NkTypeOf<nk_int32>();
            const reflection::NkType& floatType = reflection::NkTypeOf<nk_float32>();

            // Enregistrement des types
            registry.RegisterType(&intType);
            registry.RegisterType(&floatType);

            // Recherche par nom exact
            const reflection::NkType* foundInt = registry.FindType(intType.GetName());

            if (foundInt != nullptr) {
                printf("Found: %s (size: %zu)\n",
                    foundInt->GetName(),
                    foundInt->GetSize());
            }

            // Recherche d'un type non enregistre
            const reflection::NkType* notFound = registry.FindType("UnknownType");

            if (notFound == nullptr) {
                printf("Type not found in registry\n");
            }
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Gestion des doublons et capacite
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        void DuplicateHandlingExample() {
            reflection::NkTypeRegistry& registry = reflection::NkTypeRegistry::Get();

            // Meme type obtenu via deux appels differents
            const reflection::NkType& t1 = reflection::NkTypeOf<nk_uint64>();
            const reflection::NkType& t2 = reflection::NkTypeOf<nk_uint64>();

            // Les deux references pointent vers la meme instance statique
            // Donc le deuxieme enregistrement sera ignore comme doublon
            registry.RegisterType(&t1);  // Enregistre
            registry.RegisterType(&t2);  // Ignore (meme adresse)

            // Verification : le compteur ne doit avoir incremente qu'une fois
            // Note : mTypeCount est prive, cet exemple est conceptuel
        }

    }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Utilisation avec des types personnalisés
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkType.h"

    namespace nkentseu {
    namespace example {

        struct CustomStruct {
            nk_int32 id;
            nk_float32 value;
        };

        void CustomTypeExample() {
            // Obtention des meta-donnees pour un type utilisateur
            const reflection::NkType& customType = reflection::NkTypeOf<CustomStruct>();

            // Affichage des informations de base
            printf("CustomStruct info:\n");
            printf("  Name: %s\n", customType.GetName());
            printf("  Size: %zu bytes\n", customType.GetSize());
            printf("  Alignment: %zu bytes\n", customType.GetAlignment());
            printf("  Category: %d\n", static_cast<int>(customType.GetCategory()));

            // Enregistrement dans le registre global
            reflection::NkTypeRegistry::Get().RegisterType(&customType);

            // Recuperation via recherche par nom
            const reflection::NkType* retrieved =
                reflection::NkTypeRegistry::Get().FindType(customType.GetName());

            if (retrieved && retrieved == &customType) {
                printf("Successfully registered and retrieved custom type\n");
            }
        }

    }
    }
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================