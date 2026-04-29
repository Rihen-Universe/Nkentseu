// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkClass.cpp
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Unite de compilation pour les extensions runtime de NkClass.
//   Contient les implementations non-inline et les utilitaires avances.
//
// AMELIORATIONS (v1.1.0) :
//   - Utilisation de NkFunction pour la gestion memoire automatique
//   - Exemples d'extensions avec NkBind pour l'enregistrement simplifie
//   - Documentation des patterns d'heritage et de recherche hierarchique
// =============================================================================

// -------------------------------------------------------------------------
// INCLUSION DE L'EN-TETE CORRESPONDANT
// -------------------------------------------------------------------------
// Inclusion du fichier d'en-tete pour verifier la coherence des declarations.

#include "NKReflection/NkClass.h"

// -------------------------------------------------------------------------
// DEPENDANCES COMPLEMENTAIRES
// -------------------------------------------------------------------------
// Inclusion optionnelle pour les extensions de logging et validation.

#include "NKLogger/NkLog.h"

// -------------------------------------------------------------------------
// ESPACE DE NOMS PRINCIPAL
// -------------------------------------------------------------------------
// Implementation dans le namespace reflection de nkentseu.

namespace nkentseu {

    namespace reflection {

        // -----------------------------------------------------------------
        // NOTE D'ARCHITECTURE : NkFunction vs Raw Pointer pour Cycle de Vie
        // -----------------------------------------------------------------
        // L'utilisation de NkFunction<void*(void)> et NkFunction<void(void*)>
        // pour mConstructor/mDestructor apporte :
        //
        // 1. Small Buffer Optimization (SBO) :
        //    - Les petits lambdas (< 64 bytes) sont stockes inline
        //    - Aucune allocation heap pour les constructeurs/destructeurs simples
        //
        // 2. Gestion automatique de la memoire :
        //    - Destruction automatique via le destructeur de NkFunction
        //    - Pas de risque de fuite memoire lors des copies/deplacements
        //
        // 3. Type-safety :
        //    - Verification a la compilation de la signature des callables
        //    - Erreurs de type detectees tot dans le cycle de compilation
        //
        // 4. Support noexcept :
        //    - Specialisation NkFunction<R(Args...) noexcept> pour les
        //      callables garantis sans exception
        //
        // 5. Copy/move semantics :
        //    - Les NkClass sont copiables/deplacables sans code additionnel
        //      grace aux operateurs par defaut de NkFunction

        // -----------------------------------------------------------------
        // ESPACE RESERVE POUR FUTURES IMPLEMENTATIONS
        // -----------------------------------------------------------------
        // Les fonctions suivantes pourraient etre implementees ici si leur
        // complexite justifie une separation de l'en-tete :

        /*
        // Exemple : Wrapper generique pour constructeur via NkBind
        template<typename ClassType>
        NkClass::ConstructorFn MakeDefaultConstructor() {
            return NkClass::ConstructorFn([]() -> void* {
                return new ClassType();
            });
        }

        // Exemple : Wrapper pour destructeur avec logging optionnel
        template<typename ClassType>
        NkClass::DestructorFn MakeLoggedDestructor() {
            return NkClass::DestructorFn([](void* instance) -> void {
                #ifdef NKENTSEU_DEBUG
                NK_FOUNDATION_LOG_DEBUG("Destroying instance of %s at %p",
                    typeid(ClassType).name(), instance);
                #endif
                ClassType* obj = static_cast<ClassType*>(instance);
                if (obj) {
                    delete obj;
                }
            });
        }

        // Exemple : Validation de coherence d'heritage
        nk_bool ValidateInheritanceChain(const NkClass* derived, const NkClass* base) {
            if (!derived || !base) {
                return false;
            }
            // Verification que base est bien dans la chaine d'heritage de derived
            return derived->IsSubclassOf(base);
        }
        */

        // -----------------------------------------------------------------
        // UTILITAIRE : Parcours complet de la hierarchie de classes
        // -----------------------------------------------------------------
        /**
         * @brief Applique une fonction a chaque classe de la chaine d'heritage
         * @param startClass Pointeur constant vers la classe de depart
         * @param visitor Callable accepteant const NkClass* en parametre
         * @tparam Visitor Type du callable visiteur
         *
         * @note Le visiteur est appele d'abord sur startClass, puis remonte
         *       recursivement vers les classes de base jusqu'a nullptr
         * @note Utile pour l'agregation de meta-donnees ou la validation
         *       de coherence dans une hierarchie de classes
         *
         * @example
         * @code
         * ForEachInHierarchy(playerClass, [](const NkClass* cls) {
         *     printf("Visiting class: %s\n", cls->GetName());
         * });
         * @endcode
         */
        template<typename Visitor>
        void ForEachInHierarchy(const NkClass* startClass, Visitor&& visitor) {
            const NkClass* current = startClass;
            while (current) {
                visitor(current);
                current = current->GetBaseClass();
            }
        }

        // -----------------------------------------------------------------
        // UTILITAIRE : Recherche de propriete avec filtrage par flags
        // -----------------------------------------------------------------
        /**
         * @brief Recherche une propriete par nom avec filtrage optionnel par drapeaux
         * @param classMeta Reference constant vers la classe de recherche
         * @param name Nom de la propriete a rechercher
         * @param requiredFlags Drapeaux requis (0 pour aucun filtre)
         * @param forbiddenFlags Drapeaux interdits (0 pour aucun filtre)
         * @return Pointeur constant vers le NkProperty trouve ou nullptr
         *
         * @note La recherche remonte dans l'heritage comme GetProperty()
         * @note Une propriete est retournee seulement si :
         *       (prop->GetFlags() & requiredFlags) == requiredFlags ET
         *       (prop->GetFlags() & forbiddenFlags) == 0
         *
         * @example
         * @code
         * // Trouver une propriete serialisable (ni transiente, ni write-only)
         * const NkProperty* prop = FindPropertyWithFlags(
         *     playerClass, "health",
         *     0, // aucun drapeau requis
         *     static_cast<nk_uint32>(NkPropertyFlags::NK_TRANSIENT |
         *                            NkPropertyFlags::NK_WRITE_ONLY)
         * );
         * @endcode
         */
        const NkProperty* FindPropertyWithFlags(
            const NkClass& classMeta,
            const nk_char* name,
            nk_uint32 requiredFlags = 0,
            nk_uint32 forbiddenFlags = 0
        ) {
            const NkProperty* prop = classMeta.GetProperty(name);
            if (!prop) {
                return nullptr;
            }
            nk_uint32 flags = prop->GetFlags();
            if ((flags & requiredFlags) != requiredFlags) {
                return nullptr;
            }
            if ((flags & forbiddenFlags) != 0) {
                return nullptr;
            }
            return prop;
        }

        // -----------------------------------------------------------------
        // UTILITAIRE : Agregation des proprietes de toute la hierarchie
        // -----------------------------------------------------------------
        /**
         * @brief Collecte toutes les proprietes accessibles d'une classe et de ses bases
         * @param classMeta Reference constant vers la classe racine de la collecte
         * @param outProperties Tableau de sortie pour stocker les pointeurs de proprietes
         * @param maxCount Capacite maximale du tableau de sortie
         * @return Nombre de proprietes effectivement ecrites dans outProperties
         *
         * @note Les proprietes sont collectees de la classe la plus derivee
         *       vers la classe de base (ordre inverse de l'heritage)
         * @note Les doublons de nom ne sont pas filtres : une propriete masquee
         *       dans une classe derivee apparaitra avant celle de la classe de base
         *
         * @example
         * @code
         * const NkProperty* props[128];
         * nk_usize count = CollectAllProperties(playerClass, props, 128);
         * for (nk_usize i = 0; i < count; ++i) {
         *     printf("Property %zu: %s\n", i, props[i]->GetName());
         * }
         * @endcode
         */
        nk_usize CollectAllProperties(
            const NkClass& classMeta,
            const NkProperty** outProperties,
            nk_usize maxCount
        ) {
            if (!outProperties || maxCount == 0) {
                return 0;
            }

            nk_usize written = 0;

            // Parcours de la hierarchie : classe courante d'abord, puis bases
            ForEachInHierarchy(&classMeta, [&](const NkClass* current) {
                for (nk_usize i = 0; i < current->GetPropertyCount() && written < maxCount; ++i) {
                    outProperties[written++] = current->GetPropertyAt(i);
                }
            });

            return written;
        }

        // -----------------------------------------------------------------
        // UTILITAIRE : Agregation des methodes de toute la hierarchie
        // -----------------------------------------------------------------
        /**
         * @brief Collecte toutes les methodes accessibles d'une classe et de ses bases
         * @param classMeta Reference constant vers la classe racine de la collecte
         * @param outMethods Tableau de sortie pour stocker les pointeurs de methodes
         * @param maxCount Capacite maximale du tableau de sortie
         * @return Nombre de methodes effectivement ecrites dans outMethods
         *
         * @note Les methodes sont collectees de la classe la plus derivee
         *       vers la classe de base (ordre inverse de l'heritage)
         * @note Ne gere pas la surcharge : une methode masquee dans une classe
         *       derivee apparaitra avant celle de la classe de base
         *
         * @example
         * @code
         * const NkMethod* methods[64];
         * nk_usize count = CollectAllMethods(playerClass, methods, 64);
         * for (nk_usize i = 0; i < count; ++i) {
         *     printf("Method %zu: %s\n", i, methods[i]->GetName());
         * }
         * @endcode
         */
        nk_usize CollectAllMethods(
            const NkClass& classMeta,
            const NkMethod** outMethods,
            nk_usize maxCount
        ) {
            if (!outMethods || maxCount == 0) {
                return 0;
            }

            nk_usize written = 0;

            // Parcours de la hierarchie : classe courante d'abord, puis bases
            ForEachInHierarchy(&classMeta, [&](const NkClass* current) {
                for (nk_usize i = 0; i < current->GetMethodCount() && written < maxCount; ++i) {
                    outMethods[written++] = current->GetMethodAt(i);
                }
            });

            return written;
        }

    } // namespace reflection

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'EXTENSIONS - NkClass.cpp
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Serialisation complete d'une instance via reflexion
// -----------------------------------------------------------------------------
/*
    #include "NKReflection/NkClass.h"
    #include "NKSerialization/NkJsonWriter.h" // Hypothetique

    namespace nkentseu {
    namespace reflection {
    namespace serialization {

        // Helper pour serialiser toutes les proprietes d'une instance en JSON
        void SerializeClassInstance(
            NkJsonWriter& writer,
            const NkClass& classMeta,
            void* instance
        ) {
            writer.BeginObject();

            // Collecte de toutes les proprietes de la hierarchie
            const NkProperty* props[256];
            nk_usize propCount = CollectAllProperties(classMeta, props, 256);

            for (nk_usize i = 0; i < propCount; ++i) {
                const NkProperty* prop = props[i];

                // Skip les proprietes non serialisables
                if (!IsPropertySerializable(*prop)) {
                    continue;
                }

                // Skip les proprietes statiques (pas d'instance a lire)
                if (prop->IsStatic()) {
                    continue;
                }

                // Ecriture du champ JSON
                writer.WriteKey(prop->GetName());

                // Dispatch sur le type pour le formatage (simplifie)
                const NkType& type = prop->GetType();
                if (type.IsPrimitive()) {
                    // Lecture et ecriture directe pour les types primitifs
                    switch (type.GetCategory()) {
                        case NkTypeCategory::NK_BOOL:
                            writer.WriteBool(prop->GetValue<nk_bool>(instance));
                            break;
                        case NkTypeCategory::NK_INT32:
                            writer.WriteInt(prop->GetValue<nk_int32>(instance));
                            break;
                        case NkTypeCategory::NK_FLOAT32:
                            writer.WriteFloat(prop->GetValue<nk_float32>(instance));
                            break;
                        // ... autres cas primitifs
                        default:
                            writer.WriteNull();
                            break;
                    }
                } else {
                    // Types complexes : delegation ou placeholder
                    writer.WriteNull();
                }
            }

            writer.EndObject();
        }

    } // namespace serialization
    } // namespace reflection
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Factory generique avec enregistrement de classes
// -----------------------------------------------------------------------------
/*
    namespace nkentseu {
    namespace reflection {

        // Registre global des classes par nom (simplifie, sans thread-safety)
        class ClassRegistry {
        public:
            static ClassRegistry& Get() {
                static ClassRegistry instance;
                return instance;
            }

            void RegisterClass(const NkClass* classMeta) {
                if (!classMeta || !classMeta->GetName()) {
                    return;
                }
                // Recherche de doublon par nom
                for (nk_usize i = 0; i < m_count; ++i) {
                    if (std::strcmp(m_classes[i]->GetName(), classMeta->GetName()) == 0) {
                        return; // Doublon ignore
                    }
                }
                if (m_count < NK_MAX_REGISTERED_CLASSES) {
                    m_classes[m_count++] = classMeta;
                }
            }

            const NkClass* FindClass(const nk_char* name) const {
                if (!name) {
                    return nullptr;
                }
                for (nk_usize i = 0; i < m_count; ++i) {
                    if (std::strcmp(m_classes[i]->GetName(), name) == 0) {
                        return m_classes[i];
                    }
                }
                return nullptr;
            }

            void* CreateInstance(const nk_char* className) const {
                const NkClass* cls = FindClass(className);
                return cls ? cls->CreateInstance() : nullptr;
            }

        private:
            static constexpr nk_usize NK_MAX_REGISTERED_CLASSES = 256;
            const NkClass* m_classes[NK_MAX_REGISTERED_CLASSES];
            nk_usize m_count;

            ClassRegistry() : m_count(0) {}
        };

        // Macro de convenance pour l'enregistrement automatique
        #define NK_REGISTER_CLASS(ClassType, DisplayName) \
            namespace { \
                struct NK_CONCAT(_reg_, ClassType) { \
                    NK_CONCAT(_reg_, ClassType)() { \
                        static auto& meta = \
                            ::nkentseu::reflection::NkClass::MakeFromClassType<ClassType>(DisplayName); \
                        ::nkentseu::reflection::ClassRegistry::Get().RegisterClass(&meta); \
                    } \
                } NK_CONCAT(_instance_, __LINE__); \
            }

    } // namespace reflection
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Validation de coherence d'heritage au runtime
// -----------------------------------------------------------------------------
/*
    namespace nkentseu {
    namespace reflection {
    namespace validation {

        // Verifie qu'une chaine d'heritage ne contient pas de cycles
        nk_bool ValidateInheritanceAcyclic(const NkClass* start) {
            if (!start) {
                return true;
            }

            // Ensemble simple de pointeurs vus (sans STL, tableau statique)
            const NkClass* visited[128];
            nk_usize visitedCount = 0;

            const NkClass* current = start;
            while (current) {
                // Verification de presence dans visited
                for (nk_usize i = 0; i < visitedCount; ++i) {
                    if (visited[i] == current) {
                        // Cycle detecte !
                        NK_FOUNDATION_LOG_ERROR(
                            "Inheritance cycle detected involving class '%s'",
                            current->GetName()
                        );
                        return false;
                    }
                }

                // Ajout a visited
                if (visitedCount < 128) {
                    visited[visitedCount++] = current;
                }

                current = current->GetBaseClass();
            }

            return true;
        }

        // Verifie que toutes les proprietes ont des types valides
        nk_bool ValidateClassProperties(const NkClass& classMeta) {
            nk_bool allValid = true;

            ForEachInHierarchy(&classMeta, [&](const NkClass* current) {
                for (nk_usize i = 0; i < current->GetPropertyCount(); ++i) {
                    const NkProperty* prop = current->GetPropertyAt(i);
                    if (!prop || !prop->GetName() || prop->GetName()[0] == '\0') {
                        NK_FOUNDATION_LOG_ERROR(
                            "Invalid property in class '%s' at index %zu",
                            current->GetName(), i
                        );
                        allValid = false;
                    }
                    // Autres validations possibles : type non-void, offset coherent, etc.
                }
            });

            return allValid;
        }

    } // namespace validation
    } // namespace reflection
    } // namespace nkentseu
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================