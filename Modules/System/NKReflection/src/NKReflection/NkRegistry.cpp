// =============================================================================
// FICHIER  : Modules/System/NKReflection/src/NKReflection/NkRegistry.cpp
// MODULE   : NKReflection
// AUTEUR   : Rihen
// DATE     : 2026-02-07
// VERSION  : 1.1.0
// LICENCE  : Proprietaire - libre d'utilisation et de modification
// =============================================================================
// DESCRIPTION :
//   Unite de compilation pour les extensions runtime de NkRegistry.
//   Contient les implementations non-inline et les utilitaires avances.
//
// AMELIORATIONS (v1.1.0) :
//   - Documentation des patterns d'enregistrement batch
//   - Exemples d'integration avec NkBind pour les callbacks
//   - Helpers pour la validation et le debugging du registre
// =============================================================================

// -------------------------------------------------------------------------
// INCLUSION DE L'EN-TETE CORRESPONDANT
// -------------------------------------------------------------------------
// Inclusion du fichier d'en-tete pour verifier la coherence des declarations.

#include "NKReflection/NkRegistry.h"

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
        // NOTE D'ARCHITECTURE : Thread-Safety du Registre
        // -----------------------------------------------------------------
        // NkRegistry utilise un pattern Meyer's Singleton pour l'instance :
        //   - Initialisation thread-safe garantie par C++11 (magic statics)
        //   - Destruction automatique a la fin du programme (ordre non garanti)
        //
        // Operations thread-safe :
        //   - Get() : acces au singleton
        //   - FindType()/FindClass() : lecture seule (si pas d'enregistrement concurrent)
        //   - GetTypeCount()/GetClassCount() : lecture seule
        //
        // Operations thread-UNSAFE (requierent synchronisation externe) :
        //   - RegisterType()/RegisterClass() : modification des tableaux internes
        //   - SetOnRegisterCallback() : modification du callback
        //
        // Recommendation pour usage multi-thread :
        //   1. Enregistrer toutes les classes/types pendant l'initialisation
        //      (avant le lancement des threads de travail)
        //   2. Utiliser un mutex si des enregistrements dynamiques sont requis
        //   3. Preferer les lectures concurrentes aux ecritures concurrentes

        // -----------------------------------------------------------------
        // ESPACE RESERVE POUR FUTURES IMPLEMENTATIONS
        // -----------------------------------------------------------------
        // Les fonctions suivantes pourraient etre implementees ici si leur
        // complexite justifie une separation de l'en-tete :

        /*
        // Exemple : Enregistrement batch avec validation
        nk_usize RegisterClassesBatch(const NkClass* const* classes, nk_usize count) {
            auto& registry = NkRegistry::Get();
            nk_usize registered = 0;

            for (nk_usize i = 0; i < count; ++i) {
                if (classes[i] && classes[i]->GetName()) {
                    // Verification de doublon avant enregistrement
                    if (!registry.FindClass(classes[i]->GetName())) {
                        registry.RegisterClass(classes[i]);
                        ++registered;
                    }
                }
            }

            return registered;
        }

        // Exemple : Export du registre pour debugging
        void DumpRegistryToLog() {
            const auto& registry = NkRegistry::Get();

            NK_FOUNDATION_LOG_INFO("=== NKReflection Registry Dump ===");
            NK_FOUNDATION_LOG_INFO("Types: %zu / %zu",
                registry.GetTypeCount(), NkRegistry::NK_MAX_TYPES);
            NK_FOUNDATION_LOG_INFO("Classes: %zu / %zu",
                registry.GetClassCount(), NkRegistry::NK_MAX_CLASSES);

            for (nk_usize i = 0; i < registry.GetClassCount(); ++i) {
                const auto* cls = registry.GetClassAt(i);
                if (cls) {
                    NK_FOUNDATION_LOG_INFO("Class[%zu]: %s (size=%zu, base=%s)",
                        i,
                        cls->GetName(),
                        cls->GetSize(),
                        cls->GetBaseClass() ? cls->GetBaseClass()->GetName() : "none");
                }
            }
            NK_FOUNDATION_LOG_INFO("=== End Dump ===");
        }

        // Exemple : Validation de coherence des enregistrements
        nk_bool ValidateRegistryIntegrity() {
            const auto& registry = NkRegistry::Get();
            nk_bool allValid = true;

            // Verification des types
            for (nk_usize i = 0; i < registry.GetTypeCount(); ++i) {
                const auto* type = registry.GetTypeAt(i);
                if (!type || !type->GetName() || type->GetName()[0] == '\0') {
                    NK_FOUNDATION_LOG_ERROR("Invalid type at index %zu", i);
                    allValid = false;
                }
            }

            // Verification des classes
            for (nk_usize i = 0; i < registry.GetClassCount(); ++i) {
                const auto* cls = registry.GetClassAt(i);
                if (!cls || !cls->GetName() || cls->GetName()[0] == '\0') {
                    NK_FOUNDATION_LOG_ERROR("Invalid class at index %zu", i);
                    allValid = false;
                    continue;
                }

                // Verification de l'heritage (pas de cycles)
                const NkClass* cursor = cls;
                nk_usize depth = 0;
                while (cursor && depth < 128) {
                    cursor = cursor->GetBaseClass();
                    ++depth;
                }
                if (depth >= 128) {
                    NK_FOUNDATION_LOG_ERROR("Possible inheritance cycle involving class '%s'",
                        cls->GetName());
                    allValid = false;
                }
            }

            return allValid;
        }
        */

        // -----------------------------------------------------------------
        // UTILITAIRE : Helper pour l'enregistrement avec callback NkBind
        // -----------------------------------------------------------------
        /**
         * @brief Cree un callback d'enregistrement lie a un objet via NkBind
         * @tparam HandlerType Type de la classe contenant la methode handler
         * @param handlerObj Pointeur vers l'objet handler
         * @param handlerMethod Methode membre de signature void(const nk_char*, nk_bool)
         * @return NkRegistry::OnRegisterCallback pret a etre passe a SetOnRegisterCallback
         *
         * @note Utilise nkentseu::NkBind pour lier l'objet a la methode
         * @note Le callback resultant est copiable/deplacable via NkFunction
         *
         * @example
         * @code
         * class RegistryLogger {
         * public:
         *     void OnRegistered(const nk_char* name, nk_bool isClass) {
         *         printf("Registered: %s (%s)\n", name, isClass ? "class" : "type");
         *     }
         * };
         *
         * RegistryLogger logger;
         * auto callback = CreateBoundRegisterCallback(&logger, &RegistryLogger::OnRegistered);
         * NkRegistry::Get().SetOnRegisterCallback(callback);
         * @endcode
         */
        template<typename HandlerType>
        NkRegistry::OnRegisterCallback CreateBoundRegisterCallback(
            HandlerType* handlerObj,
            void (HandlerType::*handlerMethod)(const nk_char*, nk_bool)
        ) {
            // Adaptation de la signature membre vers la signature du callback
            auto wrapper = [handlerObj, handlerMethod](
                const nk_char* name,
                nk_bool isClass
            ) -> void {
                (handlerObj->*handlerMethod)(name, isClass);
            };

            return NkRegistry::OnRegisterCallback(traits::NkMove(wrapper));
        }

        // -----------------------------------------------------------------
        // UTILITAIRE : Recherche de classe avec filtrage par heritage
        // -----------------------------------------------------------------
        /**
         * @brief Recherche une classe qui derive d'une classe de base donnee
         * @param baseClassName Nom de la classe de base requise
         * @param derivedClassName Nom de la classe derivee a rechercher
         * @return Pointeur constant vers le NkClass trouve ou nullptr
         *
         * @note Combine FindClass() et IsSubclassOf() pour une recherche hierarchique
         * @note Utile pour les factories qui ne doivent retourner que des sous-types
         *
         * @example
         * @code
         * // Trouver une classe "Enemy" qui derive de "GameObject"
         * const NkClass* enemyClass = FindDerivedClass("GameObject", "Enemy");
         * if (enemyClass) {
         *     void* instance = enemyClass->CreateInstance();
         * }
         * @endcode
         */
        const NkClass* FindDerivedClass(
            const nk_char* baseClassName,
            const nk_char* derivedClassName
        ) {
            auto& registry = NkRegistry::Get();
            const NkClass* baseClass = registry.FindClass(baseClassName);
            const NkClass* derivedClass = registry.FindClass(derivedClassName);

            if (!baseClass || !derivedClass) {
                return nullptr;
            }

            return derivedClass->IsSubclassOf(baseClass) ? derivedClass : nullptr;
        }

        // -----------------------------------------------------------------
        // UTILITAIRE : Collecte de toutes les classes derivant d'une base
        // -----------------------------------------------------------------
        /**
         * @brief Collecte toutes les classes enregistrees derivant d'une classe de base
         * @param baseClassName Nom de la classe de base
         * @param outClasses Tableau de sortie pour stocker les pointeurs de classes
         * @param maxCount Capacite maximale du tableau de sortie
         * @return Nombre de classes effectivement ecrites dans outClasses
         *
         * @note Parcours lineaire de toutes les classes enregistrees
         * @note Les classes sont retournees dans l'ordre d'enregistrement (non garanti)
         *
         * @example
         * @code
         * const NkClass* entities[64];
         * nk_usize count = CollectDerivedClasses("Entity", entities, 64);
         * for (nk_usize i = 0; i < count; ++i) {
         *     printf("Entity subtype: %s\n", entities[i]->GetName());
         * }
         * @endcode
         */
        nk_usize CollectDerivedClasses(
            const nk_char* baseClassName,
            const NkClass** outClasses,
            nk_usize maxCount
        ) {
            if (!outClasses || maxCount == 0 || !baseClassName) {
                return 0;
            }

            auto& registry = NkRegistry::Get();
            const NkClass* baseClass = registry.FindClass(baseClassName);
            if (!baseClass) {
                return 0;
            }

            nk_usize written = 0;
            for (nk_usize i = 0; i < registry.GetClassCount() && written < maxCount; ++i) {
                const NkClass* candidate = registry.GetClassAt(i);
                if (candidate && candidate->IsSubclassOf(baseClass)) {
                    outClasses[written++] = candidate;
                }
            }

            return written;
        }

    } // namespace reflection

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'EXTENSIONS - NkRegistry.cpp
// =============================================================================
//
// -----------------------------------------------------------------------------
// Exemple 1 : Factory generique avec filtrage par heritage
// -----------------------------------------------------------------------------
/*
    namespace nkentseu {
    namespace reflection {
    namespace factory {

        // Helper pour creer une instance d'une classe derivee d'une base
        template<typename BaseType>
        BaseType* CreateDerivedInstance(const nk_char* derivedClassName) {
            const NkClass* derivedClass =
                FindDerivedClass(typeid(BaseType).name(), derivedClassName);

            if (!derivedClass || !derivedClass->HasConstructor()) {
                return nullptr;
            }

            void* rawInstance = derivedClass->CreateInstance();
            return static_cast<BaseType*>(rawInstance);
        }

        // Helper pour iterer sur tous les sous-types d'une base
        template<typename BaseType, typename Visitor>
        void ForEachDerivedType(Visitor&& visitor) {
            const NkClass* baseClass = NkRegistry::Get().FindClass(typeid(BaseType).name());
            if (!baseClass) {
                return;
            }

            const NkClass* derived[128];
            nk_usize count = CollectDerivedClasses(typeid(BaseType).name(), derived, 128);

            for (nk_usize i = 0; i < count; ++i) {
                visitor(derived[i]);
            }
        }

    } // namespace factory
    } // namespace reflection
    } // namespace nkentseu

    // Usage :
    // auto* enemy = factory::CreateDerivedInstance<GameObject>("Zombie");
    // factory::ForEachDerivedType<Entity>([](const NkClass* cls) {
    //     printf("Available entity type: %s\n", cls->GetName());
    // });
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Serialisation reflexive via parcours du registre
// -----------------------------------------------------------------------------
/*
    namespace nkentseu {
    namespace reflection {
    namespace serialization {

        // Helper pour serialiser toutes les instances d'un type donne
        template<typename T>
        nk_bool SerializeAllInstances(
            NkJsonWriter& writer, // Hypothetique
            const nk_char* outputKey
        ) {
            auto& registry = NkRegistry::Get();
            const NkClass* classMeta = registry.GetClass<T>();

            if (!classMeta) {
                return false;
            }

            writer.WriteKey(outputKey);
            writer.BeginArray();

            // Note : le registre ne stocke pas les instances, seulement les meta-donnees
            // Ceci est un exemple conceptuel : en pratique, il faudrait un gestionnaire
            // d'instances separe qui maintient une liste des objets crees par type

            writer.EndArray();
            return true;
        }

    } // namespace serialization
    } // namespace reflection
    } // namespace nkentseu
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Plugin system avec enregistrement dynamique de classes
// -----------------------------------------------------------------------------
/*
    namespace nkentseu {
    namespace reflection {
    namespace plugins {

        // Interface pour les plugins reflechis
        class IReflectivePlugin {
        public:
            virtual ~IReflectivePlugin() = default;
            virtual const nk_char* GetPluginName() const = 0;
            virtual void RegisterTypes() = 0; // Enregistre ses classes dans NkRegistry
        };

        // Helper pour charger et enregistrer un plugin
        nk_bool LoadAndRegisterPlugin(IReflectivePlugin* plugin) {
            if (!plugin) {
                return false;
            }

            // Synchronisation requise pour l'enregistrement concurrent
            // std::lock_guard<std::mutex> lock(g_registryMutex);

            plugin->RegisterTypes();

            NK_FOUNDATION_LOG_INFO("Plugin '%s' registered its types",
                plugin->GetPluginName());

            return true;
        }

        // Helper pour lister tous les plugins charges via leurs classes
        void ListLoadedPlugins() {
            auto& registry = NkRegistry::Get();

            NK_FOUNDATION_LOG_INFO("Loaded reflective plugins:");
            for (nk_usize i = 0; i < registry.GetClassCount(); ++i) {
                const auto* cls = registry.GetClassAt(i);
                if (cls && cls->IsSubclassOf(
                    NkRegistry::Get().FindClass(typeid(IReflectivePlugin).name())
                )) {
                    NK_FOUNDATION_LOG_INFO("  - %s", cls->GetName());
                }
            }
        }

    } // namespace plugins
    } // namespace reflection
    } // namespace nkentseu
*/

// ============================================================
// Copyright (c) 2024-2026 Rihen. Tous droits reserves.
// ============================================================