// =============================================================================
// NKSerialization/NkSchemaVersioning.h
// Système de migration de schéma entre versions d'assets.
//
// Design :
//  - Architecture STL-free : NkVector, pointeurs de fonction, pas d'allocations dynamiques
//  - NkTypeId : identifiant de type basé sur l'adresse de variable statique (technique EnTT)
//  - NkSchemaVersion : versionnage sémantique (major.minor.patch) avec packing 64-bit
//  - NkSchemaMigration : fonctions de migration chaînables entre versions
//  - NkSchemaRegistry : singleton global pour gestion centralisée des migrations
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Macros NK_SCHEMA_CURRENT_VERSION et NK_REGISTER_MIGRATION pour enregistrement automatique
//
// Philosophie STL-FREE :
//  - Aucun usage de std::vector, std::function, ou <memory>
//  - Gestion mémoire explicite via conteneurs maison (NkVector)
//  - Pointeurs de fonction C pour les callbacks de migration
//  - Compatibilité avec les systèmes embarqués et temps-réel
//
// Thread-safety :
//  - Le registry global est thread-safe en lecture après initialisation
//  - L'enregistrement (SetCurrentVersion/RegisterMigration) doit être fait avant accès concurrent
//  - Typiquement : enregistrer toutes les migrations au démarrage, dans un seul thread
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKSCHEMAVERSIONING_H
#define NKENTSEU_SERIALIZATION_NKSCHEMAVERSIONING_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des dépendances du module de sérialisation.
    // NkSerializationApi.h fournit les macros d'export requises.

    #include "NKSerialization/NkSerializationApi.h"
    #include "NKSerialization/NkArchive.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/Functional/NkFunction.h"
    #include "NKCore/NkTypes.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.

    namespace nkentseu {


        // =============================================================================
        // TYPE ALIAS : NkTypeId
        // DESCRIPTION : Identifiant unique de type pour le registry de migrations
        // =============================================================================
        /**
         * @typedef NkTypeId
         * @brief Identifiant numérique unique pour chaque type C++ sérialisable
         * @ingroup SerializationTypes
         *
         * Implémentation :
         *  - Alias vers nk_uint64 pour compatibilité plateforme
         *  - Généré via l'adresse d'une variable statique locale (technique type-safe)
         *  - Unique par type dans tout le programme (garanti par le linker)
         *
         * Avantages vs string :
         *  - Comparaison O(1) via opérateur entier
         *  - Pas d'allocation de chaîne pour le lookup
         *  - Type-safe : impossible de confondre deux types différents
         *
         * @note Utilisé comme clé primaire dans NkSchemaRegistry pour associer
         *       versions et migrations à chaque type sérialisable.
         *
         * @example Génération d'un NkTypeId
         * @code
         * constexpr nkentseu::NkTypeId playerTypeId = nkentseu::NkTypeOf<PlayerData>();
         * constexpr nkentseu::NkTypeId enemyTypeId  = nkentseu::NkTypeOf<EnemyData>();
         *
         * // Les deux IDs sont garantis différents à la compilation
         * static_assert(playerTypeId != enemyTypeId, "Type IDs must be unique");
         * @endcode
         */
        using NkTypeId = nk_uint64;


        // =============================================================================
        // FONCTION TEMPLATE : NkTypeOf
        // DESCRIPTION : Génération d'un NkTypeId unique pour un type C++ donné
        // =============================================================================
        /**
         * @brief Génère un identifiant unique constexpr pour un type C++
         * @tparam T Type pour lequel générer l'ID
         * @return NkTypeId unique et constant pour le type T
         * @ingroup SerializationUtilities
         *
         * Mécanisme :
         *  - Utilise l'adresse d'une variable statique locale dans une fonction template
         *  - Chaque instantiation de template a sa propre variable statique
         *  - L'adresse est convertie en entier 64-bit pour usage comme clé
         *
         * Garanties :
         *  - constexpr : évaluable à la compilation
         *  - noexcept : garantie de non-levée d'exception
         *  - Unique : deux types différents produisent toujours des IDs différents
         *  - Stable : le même type produit toujours le même ID dans une compilation
         *
         * @note Technique inspirée de EnTT et autres frameworks de réflexion type-safe.
         *       Ne dépend pas de RTTI ni de noms de types (mangled ou non).
         *
         * @warning L'ID est stable dans une compilation donnée, mais peut changer
         *          entre différentes compilations (liens statiques/dynamiques).
         *          Ne pas persister les NkTypeId dans des fichiers — utiliser les noms.
         *
         * @example
         * @code
         * class PlayerData { /* ... *\/ };
         * class EnemyData  { /* ... *\/ };
         *
         * constexpr auto playerId = nkentseu::NkTypeOf<PlayerData>();
         * constexpr auto enemyId  = nkentseu::NkTypeOf<EnemyData>();
         *
         * // Utilisation dans le registry
         * nkentseu::NkSchemaRegistry::SetCurrentVersion(playerId, {1, 0, 0});
         * @endcode
         */
        template<typename T>
        [[nodiscard]] constexpr NkTypeId NkTypeOf() noexcept {
            // Variable statique locale : adresse unique par instantiation de template
            static const char s_tag = 0;
            // Conversion de l'adresse en entier 64-bit pour usage comme clé
            return static_cast<NkTypeId>(
                reinterpret_cast<nk_size>(&s_tag));
        }


        // =============================================================================
        // STRUCTURE : NkSchemaVersion
        // DESCRIPTION : Versionnage sémantique avec packing 64-bit optimisé
        // =============================================================================
        /**
         * @struct NkSchemaVersion
         * @brief Représentation compacte d'une version sémantique (major.minor.patch)
         * @ingroup SerializationComponents
         *
         * Format sémantique :
         *  - major : changement incompatible avec les versions précédentes
         *  - minor : ajout de fonctionnalités rétro-compatibles
         *  - patch : corrections de bugs rétro-compatibles
         *
         * Représentation interne :
         *  - Packing 64-bit : [major:16bits][minor:16bits][patch:16bits][reserved:16bits]
         *  - Comparaisons via opérateurs sur la valeur packée (O(1))
         *  - Conversion string via ToString() pour logging et debugging
         *
         * Avantages du packing :
         *  - Comparaison de versions en une seule opération entière
         *  - Stockage minimal (8 bytes au lieu de 3x uint16 + padding)
         *  - Tri et recherche optimisés dans les conteneurs
         *
         * @note Les opérateurs de comparaison utilisent Pack() pour cohérence.
         *       UnorderedVersion (ex: 1.2.3 vs 1.3.2) est géré correctement.
         *
         * @example Manipulation de versions
         * @code
         * // Création
         * nkentseu::NkSchemaVersion v1(1, 0, 0);  // Version 1.0.0
         * nkentseu::NkSchemaVersion v2;            // Version 0.0.0 par défaut
         * v2.major = 1; v2.minor = 2; v2.patch = 3;  // Version 1.2.3
         *
         * // Comparaisons
         * if (v1 < v2) { /* v1 est plus ancienne *\/ }
         * if (v2 != v1) { /* versions différentes *\/ }
         *
         * // Packing/unpacking pour stockage binaire
         * nk_uint64 packed = v2.Pack();           // 0x000100020003
         * auto unpacked = nkentseu::NkSchemaVersion::Unpack(packed);  // {1,2,3}
         *
         * // Conversion string pour logging
         * nkentseu::NkString versionStr = v2.ToString();  // "1.2.3"
         * @endcode
         */
        struct NkSchemaVersion {


            // -----------------------------------------------------------------
            // MEMBRES DE DONNÉES
            // -----------------------------------------------------------------
            /// @brief Numéro de version majeure (changements incompatibles)
            nk_uint16 major = 0u;

            /// @brief Numéro de version mineure (nouvelles fonctionnalités rétro-compatibles)
            nk_uint16 minor = 0u;

            /// @brief Numéro de patch (corrections de bugs rétro-compatibles)
            nk_uint16 patch = 0u;


            // -----------------------------------------------------------------
            // CONSTRUCTEURS
            // -----------------------------------------------------------------
            /**
             * @brief Constructeur par défaut : version 0.0.0
             * @note noexcept : garantie de non-levée d'exception
             */
            NkSchemaVersion() noexcept = default;

            /**
             * @brief Constructeur avec valeurs explicites pour major/minor/patch
             * @param maj Numéro de version majeure
             * @param min Numéro de version mineure
             * @param pat Numéro de patch
             * @note constexpr : évaluable à la compilation pour les constantes
             */
            constexpr NkSchemaVersion(nk_uint16 maj, nk_uint16 min, nk_uint16 pat) noexcept
                : major(maj)
                , minor(min)
                , patch(pat)
            {}


            // -----------------------------------------------------------------
            // MÉTHODES DE PACKING/UNPACKING
            // -----------------------------------------------------------------
            /**
             * @brief Packe la version en un entier 64-bit pour comparaison/storage
             * @return Valeur 64-bit : [major:16][minor:16][patch:16][reserved:16]
             * @note constexpr : évaluable à la compilation
             * @note Utilisé en interne par les opérateurs de comparaison
             */
            [[nodiscard]] constexpr nk_uint64 Pack() const noexcept {
                return (static_cast<nk_uint64>(major) << 32u)
                    | (static_cast<nk_uint64>(minor) << 16u)
                    | static_cast<nk_uint64>(patch);
            }

            /**
             * @brief Dépacke un entier 64-bit en NkSchemaVersion
             * @param v Valeur 64-bit packée à décomposer
             * @return NkSchemaVersion avec major/minor/patch extraits
             * @note constexpr : évaluable à la compilation
             * @note Utilisé pour charger des versions depuis un format binaire
             */
            [[nodiscard]] static constexpr NkSchemaVersion Unpack(nk_uint64 v) noexcept {
                return NkSchemaVersion(
                    static_cast<nk_uint16>((v >> 32u) & 0xFFFFu),
                    static_cast<nk_uint16>((v >> 16u) & 0xFFFFu),
                    static_cast<nk_uint16>(v & 0xFFFFu));
            }


            // -----------------------------------------------------------------
            // OPÉRATEURS DE COMPARAISON
            // -----------------------------------------------------------------
            /**
             * @brief Égalité : compare les versions packées
             * @param o Autre version à comparer
             * @return true si major/minor/patch sont identiques
             */
            [[nodiscard]] constexpr bool operator==(const NkSchemaVersion& o) const noexcept {
                return Pack() == o.Pack();
            }

            /**
             * @brief Inégalité : négation de operator==
             * @param o Autre version à comparer
             * @return true si au moins un composant diffère
             */
            [[nodiscard]] constexpr bool operator!=(const NkSchemaVersion& o) const noexcept {
                return Pack() != o.Pack();
            }

            /**
             * @brief Ordre strict : compare les versions packées
             * @param o Autre version à comparer
             * @return true si cette version est strictement plus ancienne
             * @note Permet le tri et la recherche dans des conteneurs ordonnés
             */
            [[nodiscard]] constexpr bool operator<(const NkSchemaVersion& o) const noexcept {
                return Pack() < o.Pack();
            }

            /**
             * @brief Ordre large : compare les versions packées
             * @param o Autre version à comparer
             * @return true si cette version est plus ancienne ou égale
             */
            [[nodiscard]] constexpr bool operator<=(const NkSchemaVersion& o) const noexcept {
                return Pack() <= o.Pack();
            }


            // -----------------------------------------------------------------
            // UTILITAIRES DE FORMATAGE
            // -----------------------------------------------------------------
            /**
             * @brief Convertit la version en chaîne de caractères "major.minor.patch"
             * @return NkString contenant la représentation textuelle de la version
             * @note Utilise NkString::Fmtf pour cohérence avec le reste du projet
             * @note Format fixe : pas de zéros superflus, ex: "1.2.3" et non "01.02.03"
             */
            [[nodiscard]] NkString ToString() const noexcept {
                return NkString::Fmtf("%u.%u.%u",
                    static_cast<unsigned>(major),
                    static_cast<unsigned>(minor),
                    static_cast<unsigned>(patch));
            }


        }; // struct NkSchemaVersion


        // =============================================================================
        // STRUCTURE : NkSchemaMigration
        // DESCRIPTION : Fonction de migration entre deux versions de schéma
        // =============================================================================
        /**
         * @struct NkSchemaMigration
         * @brief Encapsulation d'une fonction de migration entre versions
         * @ingroup SerializationComponents
         *
         * Rôle :
         *  - Définir une migration atomique d'une version source vers une version cible
         *  - Permettre le chaînage de migrations pour mettre à jour sur plusieurs versions
         *  - Fournir un point d'extension pour les transformations de données
         *
         * Signature de MigrationFn :
         *  - Paramètres : NkArchive& (données), from/to (versions source/cible)
         *  - Retour : nk_bool (true = succès, false = échec avec rollback implicite)
         *  - noexcept : garantie de non-levée d'exception pour sécurité runtime
         *
         * Contrat d'implémentation :
         *  - La migration doit transformer les données de 'from' vers 'to' de façon réversible si possible
         *  - En cas d'échec, retourner false sans modifier partiellement les données
         *  - Les migrations doivent être idempotentes : appliquer deux fois = appliquer une fois
         *
         * @note Les migrations sont appliquées en ordre croissant de version.
         *       Le registry cherche automatiquement le chemin de migration optimal.
         *
         * @example Définition d'une fonction de migration
         * @code
         * // Fonction de migration : ajoute un champ "version" si absent
         * nk_bool Migrate_v100_to_v110(nkentseu::NkArchive& archive,
         *                              nkentseu::NkSchemaVersion from,
         *                              nkentseu::NkSchemaVersion to) noexcept {
         *     // Vérification de sécurité
         *     if (from != nkentseu::NkSchemaVersion(1, 0, 0)
         *         || to != nkentseu::NkSchemaVersion(1, 1, 0)) {
         *         return false;  // Versions inattendues
         *     }
         *
         *     // Ajout du nouveau champ avec valeur par défaut
         *     if (!archive.Has("newFeature.enabled")) {
         *         archive.SetBool("newFeature.enabled", false);
         *     }
         *
         *     return true;  // Migration réussie
         * }
         *
         * // Enregistrement via macro (dans un .cpp) :
         * // NK_REGISTER_MIGRATION(PlayerData, 1, 0, 0, 1, 1, 0, Migrate_v100_to_v110);
         * @endcode
         */
        struct NkSchemaMigration {


            // -----------------------------------------------------------------
            // TYPE ALIAS : Signature de fonction de migration
            // -----------------------------------------------------------------
            /**
             * @typedef MigrationFn
             * @brief Signature des fonctions de migration de schéma
             * @ingroup SerializationTypes
             *
             * Signature : nk_bool(*)(NkArchive&, NkSchemaVersion from, NkSchemaVersion to) noexcept
             *  - NkArchive& : données à migrer (modifiées in-place)
             *  - from : version source des données (pour validation)
             *  - to : version cible attendue après migration
             *  - Retour : true = succès, false = échec (rollback implicite)
             *  - noexcept : aucune exception ne doit être levée
             *
             * @note Utilise un pointeur de fonction C pour éviter std::function
             *       et les allocations dynamiques associées.
             */
            using MigrationFn = nk_bool(*)(NkArchive&, NkSchemaVersion from, NkSchemaVersion to) noexcept;


            // -----------------------------------------------------------------
            // MEMBRES DE DONNÉES
            // -----------------------------------------------------------------
            /// @brief Version source de la migration (inclusive)
            NkSchemaVersion from;

            /// @brief Version cible de la migration (inclusive)
            NkSchemaVersion to;

            /// @brief Pointeur vers la fonction de migration à appliquer
            MigrationFn fn = nullptr;


            // -----------------------------------------------------------------
            // CONSTRUCTEURS
            // -----------------------------------------------------------------
            /**
             * @brief Constructeur par défaut : migration nulle (fn == nullptr)
             * @note noexcept : garantie de non-levée d'exception
             */
            NkSchemaMigration() noexcept = default;

            /**
             * @brief Constructeur avec paramètres explicites
             * @param f Version source de la migration
             * @param t Version cible de la migration
             * @param fn_ Pointeur vers la fonction de migration
             * @note noexcept : garantie de non-levée d'exception
             */
            NkSchemaMigration(NkSchemaVersion f, NkSchemaVersion t, MigrationFn fn_) noexcept
                : from(f)
                , to(t)
                , fn(fn_)
            {}


            // -----------------------------------------------------------------
            // MÉTHODES D'EXÉCUTION
            // -----------------------------------------------------------------
            /**
             * @brief Applique la migration sur un archive donné
             * @param archive Archive contenant les données à migrer
             * @return true si la migration a réussi ou si fn est nullptr, false sinon
             * @note noexcept : garantie de non-levée d'exception
             * @note Si fn est nullptr, retourne true (migration no-op)
             */
            [[nodiscard]] nk_bool Apply(NkArchive& archive) const noexcept {
                return fn ? fn(archive, from, to) : true;
            }


        }; // struct NkSchemaMigration

        // =============================================================================
        // SECTION : HELPERS DE MIGRATION DE CHAMP — VERSION COMPLÈTE
        // =============================================================================
        /**
         * @defgroup SchemaFieldMigrations Helpers de Migration de Champ
         * @brief Fonctions utilitaires pour ajouter des champs avec valeur par défaut
         *
         * Ces helpers résolvent le problème de capture de lambdas dans un contexte
         * STL-free en utilisant NkFunction pour le type-erasure sécurisé.
         */

        // -------------------------------------------------------------------------
        // STRUCTURE INTERNE : NkFieldMigrationAdapter
        // -------------------------------------------------------------------------
        /**
         * @struct NkFieldMigrationAdapter
         * @brief Adaptateur interne pour encapsuler la logique d'ajout de champ
         * @ingroup SchemaFieldMigrations
         *
         * Cette structure template permet de :
         *  - Capturer un callable getDefault via NkFunction (SBO ou heap)
         *  - Stocker le nom du champ dans un buffer statique
         *  - Appliquer la migration via une fonction C-compatible
         *
         * @tparam ReturnType Type de retour de getDefault() — déduit automatiquement
         */
        template<typename ReturnType>
        struct NkFieldMigrationAdapter {

            // -----------------------------------------------------------------
            // TYPE ALIAS : Signature de la fonction de migration C-compatible
            // -----------------------------------------------------------------
            /// @brief Signature requise par NkSchemaMigration::MigrationFn
            using MigrationFnPtr = nk_bool(*)(NkArchive&, NkSchemaVersion, NkSchemaVersion) noexcept;

            // -----------------------------------------------------------------
            // MEMBRES STATIQUES : Stockage par instantiation de template
            // -----------------------------------------------------------------
            /// @brief Callable capturé pour générer la valeur par défaut
            /// @note Un par combinaison <ReturnType, DefaultFn> grâce au template
            static NkFunction<ReturnType()> s_getDefault;

            /// @brief Nom du champ à ajouter (copie statique, max 127 chars)
            static char s_fieldName[128];

            /// @brief Pointeur vers la fonction Set* appropriée pour ReturnType
            /// @note Initialisé via InitSetter() lors du premier appel
            static nk_bool (*s_setter)(NkArchive&, const char*, ReturnType) noexcept;

            // -----------------------------------------------------------------
            // MÉTHODE : Migration C-compatible
            // -----------------------------------------------------------------
            /**
             * @brief Fonction de migration appelée par le registry
             * @param archive Archive à migrer
             * @param from Version source (non utilisée ici)
             * @param to Version cible (non utilisée ici)
             * @return true si succès ou champ déjà présent, false en cas d'erreur
             *
             * Logique :
             *  1. Vérifie si le champ existe déjà dans l'archive
             *  2. Si absent : appelle s_getDefault() et applique s_setter()
             *  3. Retourne true pour indiquer que la migration est applicable
             *
             * @note noexcept : garantie requise par MigrationFn signature
             */
            static nk_bool Migrate(NkArchive& archive,
                                NkSchemaVersion /*from*/,
                                NkSchemaVersion /*to*/) noexcept {
                // Guard : vérifier que le setter est initialisé
                if (!s_setter) {
                    return false;  // Configuration incomplète
                }

                // Idempotence : ne rien faire si le champ existe déjà
                if (archive.Has(s_fieldName)) {
                    return true;
                }

                // Génération de la valeur par défaut via le callable capturé
                // NkFunction retourne ReturnType{} si vide — sécurité supplémentaire
                ReturnType defaultValue = s_getDefault ? s_getDefault() : ReturnType{};

                // Application du setter approprié pour le type
                return s_setter(archive, s_fieldName, defaultValue);
            }

            // -----------------------------------------------------------------
            // MÉTHODE : Initialisation du setter par type
            // -----------------------------------------------------------------
            /**
             * @brief Initialise s_setter avec la fonction Set* appropriée pour ReturnType
             * @note Appelée une fois par instantiation de template
             * @note Utilise if constexpr pour la résolution à la compilation
             */
            static void InitSetter() noexcept {
                // Résolution compile-time du setter via if constexpr
                if constexpr (traits::NkIsSame_v<ReturnType, nk_bool>) {
                    s_setter = [](NkArchive& arc, const char* name, ReturnType val) noexcept {
                        return arc.SetBool(name, val);
                    };
                }
                else if constexpr (traits::NkIsSame_v<ReturnType, nk_int32>) {
                    s_setter = [](NkArchive& arc, const char* name, ReturnType val) noexcept {
                        return arc.SetInt32(name, val);
                    };
                }
                else if constexpr (traits::NkIsSame_v<ReturnType, nk_uint32>) {
                    s_setter = [](NkArchive& arc, const char* name, ReturnType val) noexcept {
                        return arc.SetUInt32(name, val);
                    };
                }
                else if constexpr (traits::NkIsSame_v<ReturnType, nk_int64>) {
                    s_setter = [](NkArchive& arc, const char* name, ReturnType val) noexcept {
                        return arc.SetInt64(name, val);
                    };
                }
                else if constexpr (traits::NkIsSame_v<ReturnType, nk_uint64>) {
                    s_setter = [](NkArchive& arc, const char* name, ReturnType val) noexcept {
                        return arc.SetUInt64(name, val);
                    };
                }
                else if constexpr (traits::NkIsSame_v<ReturnType, nk_float32>) {
                    s_setter = [](NkArchive& arc, const char* name, ReturnType val) noexcept {
                        return arc.SetFloat32(name, val);
                    };
                }
                else if constexpr (traits::NkIsSame_v<ReturnType, nk_float64>) {
                    s_setter = [](NkArchive& arc, const char* name, ReturnType val) noexcept {
                        return arc.SetFloat64(name, val);
                    };
                }
                else if constexpr (traits::NkIsSame_v<ReturnType, NkString>) {
                    s_setter = [](NkArchive& arc, const char* name, ReturnType val) noexcept {
                        return arc.SetString(name, val.View());
                    };
                }
                else {
                    // Type non supporté : setter null pour échec contrôlé
                    s_setter = nullptr;
                }
            }

        }; // struct NkFieldMigrationAdapter

        // -------------------------------------------------------------------------
        // DÉFINITION DES MEMBRES STATIQUES DE NkFieldMigrationAdapter
        // -------------------------------------------------------------------------
        // Ces définitions doivent être dans le header car template, mais le linker
        // fusionnera les instances identiques (ODR) — comportement standard C++.

        template<typename ReturnType>
        NkFunction<ReturnType()> NkFieldMigrationAdapter<ReturnType>::s_getDefault;

        template<typename ReturnType>
        char NkFieldMigrationAdapter<ReturnType>::s_fieldName[128] = {};

        template<typename ReturnType>
        nk_bool (*NkFieldMigrationAdapter<ReturnType>::s_setter)(NkArchive&, const char*, ReturnType) noexcept = nullptr;


        // =============================================================================
        // CLASSE : NkSchemaRegistry
        // DESCRIPTION : Registry singleton pour gestion des versions et migrations
        // =============================================================================
        /**
         * @class NkSchemaRegistry
         * @brief Registry global pour l'enregistrement et l'application de migrations de schéma
         * @ingroup SerializationComponents
         *
         * Responsabilités :
         *  - Enregistrer la version courante de chaque type via SetCurrentVersion()
         *  - Enregistrer des fonctions de migration via RegisterMigration()
         *  - Appliquer automatiquement la chaîne de migrations via MigrateArchive()
         *  - Gérer les erreurs de migration avec messages explicites
         *
         * Pattern Singleton thread-safe :
         *  - Global() retourne une référence à une variable static locale
         *  - Garantie d'initialisation thread-safe en C++11+ (Meyer's singleton)
         *  - Accès en lecture concurrente safe après initialisation complète
         *
         * Algorithme de migration :
         *  1. Trouver la version courante du type dans mVersions
         *  2. Si version stockée == version courante : retour immédiat (no-op)
         *  3. Sinon : boucle de recherche de migrations applicables en ordre croissant
         *  4. Appliquer chaque migration trouvée, mettre à jour le curseur de version
         *  5. Si aucune migration trouvée pour avancer : erreur de chemin manquant
         *  6. Mettre à jour les méta-données "__meta__.schema_version" après succès
         *
         * @warning Ce registry n'est PAS thread-safe pour l'enregistrement.
         *          Tous les SetCurrentVersion/RegisterMigration doivent être appelés
         *          avant tout accès concurrent, typiquement au démarrage dans un seul thread.
         *
         * @note Complexité : O(n × m) où n = nombre de versions enregistrées,
         *       m = nombre de migrations enregistrées. Typiquement négligeable
         *       car appelé uniquement au chargement d'assets.
         *
         * @example Enregistrement et migration
         * @code
         * // Au démarrage : enregistrer versions et migrations
         * nkentseu::NkSchemaRegistry::SetCurrentVersion(
         *     nkentseu::NkTypeOf<PlayerData>(), {1, 2, 0});
         *
         * nkentseu::NkSchemaRegistry::RegisterMigration(
         *     nkentseu::NkTypeOf<PlayerData>(),
         *     {1, 0, 0}, {1, 1, 0}, Migrate_v100_to_v110);
         *
         * // Au chargement d'un asset : appliquer les migrations si nécessaire
         * nkentseu::NkArchive assetData;
         * // ... charger assetData depuis un fichier ...
         *
         * nkentseu::NkSchemaVersion storedVersion{1, 0, 0};  // Lu depuis les méta-données
         * nkentseu::NkString errorMsg;
         *
         * if (!nkentseu::NkSchemaRegistry::Global().MigrateArchive(
         *         nkentseu::NkTypeOf<PlayerData>(),
         *         assetData, storedVersion, &errorMsg)) {
         *     logger.Error("Migration échouée : %s\n", errorMsg.CStr());
         *     return false;
         * }
         * // assetData est maintenant à jour vers la version 1.2.0
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkSchemaRegistry {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // MÉTHODE STATIQUE : ACCÈS AU SINGLETON
            // -----------------------------------------------------------------
            /**
             * @brief Accès au singleton global du registry
             * @return Référence à l'instance unique de NkSchemaRegistry
             * @ingroup SchemaRegistry
             *
             * Thread-safety :
             *  - Initialisation thread-safe garantie par C++11 (Meyer's singleton)
             *  - Accès en lecture concurrente safe après première utilisation
             *  - L'enregistrement doit être fait avant accès concurrent
             *
             * @note Pattern classique de singleton sans overhead de mutex en lecture.
             */
            [[nodiscard]] static NkSchemaRegistry& Global() noexcept {
                static NkSchemaRegistry s_instance;
                return s_instance;
            }


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : ENREGISTREMENT DES VERSIONS
            // -----------------------------------------------------------------
            /**
             * @brief Enregistre la version courante d'un type dans le registry
             * @param typeId Identifiant du type via NkTypeOf<T>()
             * @param version Version sémantique courante du schéma du type
             * @ingroup SchemaRegistry
             *
             * Comportement :
             *  - Si typeId existe déjà : met à jour la version (override)
             *  - Sinon : ajoute une nouvelle entrée dans mVersions
             *  - La version est utilisée comme cible dans MigrateArchive()
             *
             * @warning ⚠️ Thread-safety : SetCurrentVersion() n'est PAS thread-safe.
             *          Tous les enregistrements doivent être faits avant
             *          tout accès concurrent, typiquement au démarrage.
             *
             * @note Préférer la macro NK_SCHEMA_CURRENT_VERSION(Type, MAJ, MIN, PAT)
             *       pour éviter les erreurs de typage et centraliser l'enregistrement.
             *
             * @example
             * @code
             * // Enregistrement explicite (dans un .cpp) :
             * nkentseu::NkSchemaRegistry::SetCurrentVersion(
             *     nkentseu::NkTypeOf<PlayerData>(),
             *     nkentseu::NkSchemaVersion(1, 2, 0));
             *
             * // Ou via macro (recommandé) :
             * NK_SCHEMA_CURRENT_VERSION(PlayerData, 1, 2, 0);
             * @endcode
             */
            static void SetCurrentVersion(NkTypeId typeId, NkSchemaVersion version) noexcept {
                auto& reg = Global();
                for (nk_size i = 0; i < reg.mVersions.Size(); ++i) {
                    if (reg.mVersions[i].typeId == typeId) {
                        reg.mVersions[i].version = version;
                        return;
                    }
                }
                VersionEntry e;
                e.typeId = typeId;
                e.version = version;
                reg.mVersions.PushBack(e);
            }


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : ENREGISTREMENT DES MIGRATIONS
            // -----------------------------------------------------------------
            /**
             * @brief Enregistre une fonction de migration entre deux versions
             * @param typeId Identifiant du type via NkTypeOf<T>()
             * @param from Version source de la migration
             * @param to Version cible de la migration
             * @param fn Pointeur vers la fonction de migration à appliquer
             * @ingroup SchemaRegistry
             *
             * Comportement :
             *  - Ajoute une nouvelle entrée MigrationEntry dans mMigrations
             *  - Les migrations sont appliquées en ordre de découverte lors de MigrateArchive()
             *  - Plusieurs migrations peuvent exister pour le même typeId (chaînage)
             *
             * @warning ⚠️ Thread-safety : RegisterMigration() n'est PAS thread-safe.
             *          Tous les enregistrements doivent être faits avant
             *          tout accès concurrent, typiquement au démarrage.
             *
             * @note Préférer la macro NK_REGISTER_MIGRATION pour éviter les erreurs
             *       de paramétrage et garantir la cohérence des versions.
             *
             * @example
             * @code
             * // Enregistrement explicite (dans un .cpp) :
             * nkentseu::NkSchemaRegistry::RegisterMigration(
             *     nkentseu::NkTypeOf<PlayerData>(),
             *     nkentseu::NkSchemaVersion(1, 0, 0),
             *     nkentseu::NkSchemaVersion(1, 1, 0),
             *     Migrate_v100_to_v110);
             *
             * // Ou via macro (recommandé) :
             * NK_REGISTER_MIGRATION(PlayerData, 1, 0, 0, 1, 1, 0, Migrate_v100_to_v110);
             * @endcode
             */
            static void RegisterMigration(NkTypeId typeId,
                                          NkSchemaVersion from,
                                          NkSchemaVersion to,
                                          NkSchemaMigration::MigrationFn fn) noexcept {
                MigrationEntry e;
                e.typeId = typeId;
                e.migration = NkSchemaMigration(from, to, fn);
                Global().mMigrations.PushBack(e);
            }


            // -----------------------------------------------------------------
            // MÉTHODE STATIQUE : APPLICATION DES MIGRATIONS
            // -----------------------------------------------------------------
            /**
             * @brief Applique la chaîne de migrations nécessaire pour mettre à jour un archive
             * @param typeId Identifiant du type via NkTypeOf<T>()
             * @param archive Archive contenant les données à migrer (modifié in-place)
             * @param stored Version des données telles que stockées dans l'archive
             * @param err Pointeur optionnel vers NkString pour message d'erreur détaillé
             * @return true si la migration a réussi ou était inutile, false en cas d'échec
             * @ingroup SchemaRegistry
             *
             * Algorithme détaillé :
             *  1. Recherche de la version courante du type dans mVersions
             *  2. Si non trouvée : erreur "Type not registered for versioning"
             *  3. Si stored == current : retour immédiat true (déjà à jour)
             *  4. Boucle de migration (max 256 itérations pour éviter les boucles infinies) :
             *     a. Recherche d'une migration applicable depuis la version courante du curseur
             *     b. Si trouvée : application via Apply(), mise à jour du curseur
             *     c. Si non trouvée : erreur "No migration path from X to Y"
             *  5. Mise à jour des méta-données "__meta__.schema_version" avec la version cible
             *  6. Retour true si le curseur atteint la version courante
             *
             * Gestion des erreurs :
             *  - Si err != nullptr : message détaillé formaté en cas d'échec
             *  - Les erreurs n'altèrent pas l'archive : rollback implicite par non-application
             *  - Les migrations partielles ne sont pas supportées : atomicité requise
             *
             * @note Complexité : O(guard × mMigrations.Size()) avec guard = 256 max.
             *       Typiquement négligeable car appelé uniquement au chargement.
             *
             * @example
             * @code
             * nkentseu::NkArchive assetData;
             * // ... chargement depuis fichier ...
             *
             * nkentseu::NkSchemaVersion stored{1, 0, 0};  // Lu depuis les méta-données
             * nkentseu::NkString errorMsg;
             *
             * if (!nkentseu::NkSchemaRegistry::Global().MigrateArchive(
             *         nkentseu::NkTypeOf<PlayerData>(),
             *         assetData, stored, &errorMsg)) {
             *     logger.Error("Migration failed: %s\n", errorMsg.CStr());
             *     return false;
             * }
             * // assetData est maintenant à jour
             * @endcode
             */
            [[nodiscard]] static nk_bool MigrateArchive(NkTypeId typeId,
                                                        NkArchive& archive,
                                                        NkSchemaVersion stored,
                                                        NkString* err = nullptr) noexcept {
                auto& reg = Global();

                // Étape 1 : Trouver la version courante du type
                NkSchemaVersion current;
                bool found = false;
                for (nk_size i = 0; i < reg.mVersions.Size(); ++i) {
                    if (reg.mVersions[i].typeId == typeId) {
                        current = reg.mVersions[i].version;
                        found = true;
                        break;
                    }
                }

                // Étape 2 : Gestion de l'erreur "type non enregistré"
                if (!found) {
                    if (err) {
                        *err = NkString("Type not registered for versioning");
                    }
                    return false;
                }

                // Étape 3 : No-op si déjà à jour
                if (stored == current) {
                    return true;
                }

                // Étape 4 : Boucle de migration chaînée
                NkSchemaVersion cursor = stored;
                for (int guard = 0; guard < 256 && cursor != current; ++guard) {
                    bool applied = false;
                    for (nk_size i = 0; i < reg.mMigrations.Size(); ++i) {
                        const auto& me = reg.mMigrations[i];
                        if (me.typeId == typeId && me.migration.from == cursor) {
                            // Application de la migration trouvée
                            if (!me.migration.Apply(archive)) {
                                if (err) {
                                    *err = NkString::Fmtf(
                                        "Migration %s → %s failed",
                                        cursor.ToString().CStr(),
                                        me.migration.to.ToString().CStr());
                                }
                                return false;
                            }
                            // Mise à jour du curseur après succès
                            cursor = me.migration.to;
                            applied = true;
                            break;
                        }
                    }
                    // Gestion de l'erreur "chemin de migration manquant"
                    if (!applied) {
                        if (err) {
                            *err = NkString::Fmtf(
                                "No migration path from %s to %s",
                                cursor.ToString().CStr(),
                                current.ToString().CStr());
                        }
                        return false;
                    }
                }

                // Étape 5 : Mise à jour des méta-données avec la version finale
                NkArchive metaArc;
                archive.GetObject("__meta__", metaArc);
                metaArc.SetString("schema_version", current.ToString().View());
                archive.SetObject("__meta__", metaArc);

                // Étape 6 : Vérification finale de cohérence
                return cursor == current;
            }

            // =============================================================================
            // MÉTHODE : AddFieldMigration — VERSION COMPLÈTE
            // =============================================================================
            /**
             * @brief Ajoute un champ avec valeur par défaut via migration de schéma
             * @tparam DefaultFn Type du callable retournant la valeur par défaut
             * @param typeId Identifiant du type via NkTypeOf<T>()
             * @param from Version source de la migration
             * @param to Version cible de la migration
             * @param fieldName Nom du champ à ajouter dans l'archive
             * @param getDefault Callable retournant la valeur par défaut du champ
             * @ingroup SchemaRegistry
             *
             * Fonctionnement :
             *  1. Déduction du ReturnType via decltype(getDefault())
             *  2. Instanciation de NkFieldMigrationAdapter<ReturnType> unique par type
             *  3. Capture de getDefault dans un NkFunction (SBO si ≤64 bytes, heap sinon)
             *  4. Copie sécurisée du fieldName dans le buffer statique de l'adapter
             *  5. Initialisation du setter approprié via InitSetter()
             *  6. Enregistrement de la migration via RegisterMigration()
             *
             * Avantages de cette approche :
             *  - STL-free : utilise NkFunction maison au lieu de std::function
             *  - Type-safe : résolution compile-time du setter via if constexpr
             *  - Efficace : SBO pour les petits lambdas, pas d'allocation heap inutile
             *  - Idempotent : ne modifie pas l'archive si le champ existe déjà
             *  - Thread-safe après initialisation : lecture seule des statics post-setup
             *
             * Types supportés :
             *  - nk_bool, nk_int32, nk_uint32, nk_int64, nk_uint64
             *  - nk_float32, nk_float64
             *  - NkString (copie de la vue, pas de référence temporaire)
             *
             * @warning Les types non listés ci-dessus retourneront false à la migration.
             *          Pour étendre le support : ajouter une branche else if constexpr
             *          avec le setter approprié dans InitSetter().
             *
             * @example Ajout d'un champ booléen avec valeur par défaut
             * @code
             * // Migration v1.0.0 → v1.1.0 : ajoute "settings.enableTutorial"
             * nkentseu::NkSchemaRegistry::AddFieldMigration(
             *     nkentseu::NkTypeOf<GameConfig>(),
             *     {1, 0, 0}, {1, 1, 0},
             *     "settings.enableTutorial",
             *     []() { return true; });  // Valeur par défaut : true
             * @endcode
             *
             * @example Ajout d'un champ string avec valeur dynamique
             * @code
             * // Migration : ajoute "user.defaultLanguage" avec détection plateforme
             * nkentseu::NkSchemaRegistry::AddFieldMigration(
             *     nkentseu::NkTypeOf<UserProfile>(),
             *     {2, 0, 0}, {2, 1, 0},
             *     "user.defaultLanguage",
             *     []() {
             *         #ifdef NKENTSEU_PLATFORM_FR
             *             return nkentseu::NkString("fr-FR");
             *         #else
             *             return nkentseu::NkString("en-US");
             *         #endif
             *     });
             * @endcode
             */
            template<typename DefaultFn>
            static void AddFieldMigration(NkTypeId typeId,
                                        NkSchemaVersion from,
                                        NkSchemaVersion to,
                                        const char* fieldName,
                                        DefaultFn getDefault) noexcept {
                // Guard : paramètres valides requis
                if (!fieldName || !getDefault) {
                    return;
                }

                // Déduction du type de retour du callable
                using ReturnType = decltype(getDefault());

                // Alias pour lisibilité
                using Adapter = NkFieldMigrationAdapter<ReturnType>;

                // -----------------------------------------------------------------
                // ÉTAPE 1 : Capture du callable via NkFunction (SBO ou heap)
                // -----------------------------------------------------------------
                // NkFunction gère automatiquement :
                //  - Small Buffer Optimization si sizeof(DefaultFn) ≤ 64 bytes
                //  - Allocation heap via NkAllocator sinon
                //  - Copie/move sémantique selon le type de DefaultFn
                Adapter::s_getDefault = NkFunction<ReturnType()>(traits::NkForward<DefaultFn>(getDefault));

                // -----------------------------------------------------------------
                // ÉTAPE 2 : Copie sécurisée du nom de champ
                // -----------------------------------------------------------------
                // Buffer statique de 128 bytes : suffisamment pour la plupart des noms
                // Null-termination garantie même en cas de troncature
                std::strncpy(Adapter::s_fieldName, fieldName, 127);
                Adapter::s_fieldName[127] = '\0';

                // -----------------------------------------------------------------
                // ÉTAPE 3 : Initialisation du setter approprié pour ReturnType
                // -----------------------------------------------------------------
                // Résolution compile-time via if constexpr dans InitSetter()
                // Exécuté une fois par instantiation de template <ReturnType>
                Adapter::InitSetter();

                // Guard : vérifier que le setter a pu être initialisé
                if (!Adapter::s_setter) {
                    // Type non supporté : logging optionnel en debug
                    #ifdef NKENTSEU_DEBUG
                    // NK_LOG_WARN("AddFieldMigration: unsupported return type for field '%s'", fieldName);
                    #endif
                    return;
                }

                // -----------------------------------------------------------------
                // ÉTAPE 4 : Enregistrement de la migration dans le registry
                // -----------------------------------------------------------------
                // La fonction Adapter::Migrate est C-compatible (pointeur de fonction)
                // et sera appelée par MigrateArchive() quand nécessaire
                RegisterMigration(typeId, from, to, Adapter::Migrate);
            }


            // -----------------------------------------------------------------
            // SECTION 4 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
            // -----------------------------------------------------------------
        private:


            // -----------------------------------------------------------------
            // STRUCTURES INTERNES DE STOCKAGE
            // -----------------------------------------------------------------
            /**
             * @struct VersionEntry
             * @brief Entrée de registre : association typeId → version courante
             * @ingroup SchemaRegistryInternals
             */
            struct VersionEntry {
                /// @brief Identifiant unique du type
                NkTypeId typeId = 0u;
                /// @brief Version sémantique courante du schéma du type
                NkSchemaVersion version;
            };

            /**
             * @struct MigrationEntry
             * @brief Entrée de registre : association typeId → fonction de migration
             * @ingroup SchemaRegistryInternals
             */
            struct MigrationEntry {
                /// @brief Identifiant unique du type concerné par la migration
                NkTypeId typeId = 0u;
                /// @brief Fonction de migration encapsulée avec versions source/cible
                NkSchemaMigration migration;
            };


            // -----------------------------------------------------------------
            // VARIABLES MEMBRES PRIVÉES
            // -----------------------------------------------------------------
            /// @brief Registre des versions courantes par type
            /// @note Recherche linéaire : optimisée pour <100 types enregistrés
            NkVector<VersionEntry> mVersions;

            /// @brief Registre des fonctions de migration par type
            /// @note Recherche linéaire : optimisée pour <1000 migrations enregistrées
            NkVector<MigrationEntry> mMigrations;


        }; // class NkSchemaRegistry


    } // namespace nkentseu


    // =============================================================================
    // MACRO : NK_SCHEMA_CURRENT_VERSION
    // DESCRIPTION : Enregistrement automatique de la version courante d'un type
    // =============================================================================
    /**
     * @def NK_SCHEMA_CURRENT_VERSION(Type, Major, Minor, Patch)
     * @brief Macro pour l'enregistrement automatique de la version courante d'un type
     * @ingroup SchemaMacros
     *
     * Usage :
     *  - Placer dans un fichier .cpp (jamais dans un header)
     *  - Après la définition complète du type Type
     *  - Spécifier la version sémantique courante du schéma du type
     *
     * Mécanisme :
     *  - Crée un objet static avec un constructeur qui appelle SetCurrentVersion()
     *  - L'objet est construit avant main() (initialisation statique)
     *  - Le typeId est généré via NkTypeOf<Type>() pour type-safety
     *
     * Avantages :
     *  - Évite les erreurs de typage entre Type et son NkTypeId
     *  - Garantit l'enregistrement même si le type n'est pas utilisé ailleurs
     *  - Centralise la déclaration de version près de la définition du type
     *
     * @warning ⚠️ À placer UNIQUEMENT dans un .cpp, jamais dans un header !
     *          Sinon : définitions multiples, ODR violation, comportement indéfini.
     *
     * @example
     * @code
     * // Dans PlayerData.cpp (après la définition de la classe) :
     * #include "MyGame/Data/PlayerData.h"
     * #include "NKSerialization/NkSchemaVersioning.h"
     *
     * // ... implémentation des méthodes de PlayerData ...
     *
     * // Déclaration de version — une seule fois, dans ce .cpp :
     * NK_SCHEMA_CURRENT_VERSION(PlayerData, 1, 2, 0);
     * @endcode
     */
    #define NK_SCHEMA_CURRENT_VERSION(Type, Major, Minor, Patch)              \
    namespace { struct _NkSchVer_##Type {                                     \
        _NkSchVer_##Type() noexcept {                                         \
            ::nkentseu::NkSchemaRegistry::SetCurrentVersion(                  \
                ::nkentseu::NkTypeOf<Type>(), {Major, Minor, Patch});         \
        }                                                                     \
    } _sNkSchVer_##Type; }


    // =============================================================================
    // MACRO : NK_REGISTER_MIGRATION
    // DESCRIPTION : Enregistrement automatique d'une fonction de migration
    // =============================================================================
    /**
     * @def NK_REGISTER_MIGRATION(Type, FromMaj, FromMin, FromPat, ToMaj, ToMin, ToPat, FnPtr)
     * @brief Macro pour l'enregistrement automatique d'une fonction de migration
     * @ingroup SchemaMacros
     *
     * Usage :
     *  - Placer dans un fichier .cpp (jamais dans un header)
     *  - Après la définition de la fonction de migration FnPtr
     *  - FnPtr doit avoir la signature : nk_bool(*)(NkArchive&, NkSchemaVersion, NkSchemaVersion) noexcept
     *
     * Paramètres :
     *  - Type : type C++ concerné par la migration (pour NkTypeOf<Type>())
     *  - FromMaj/Min/Pat : version source de la migration (ex: 1, 0, 0)
     *  - ToMaj/Min/Pat : version cible de la migration (ex: 1, 1, 0)
     *  - FnPtr : pointeur vers la fonction de migration à appliquer
     *
     * Mécanisme :
     *  - Crée un objet static avec un constructeur qui appelle RegisterMigration()
     *  - L'objet est construit avant main() (initialisation statique)
     *  - Le typeId est généré via NkTypeOf<Type>() pour type-safety
     *
     * @warning ⚠️ À placer UNIQUEMENT dans un .cpp, jamais dans un header !
     *          Sinon : définitions multiples, ODR violation, comportement indéfini.
     *
     * @example
     * @code
     * // Dans PlayerDataMigrations.cpp :
     * #include "MyGame/Data/PlayerData.h"
     * #include "NKSerialization/NkSchemaVersioning.h"
     *
     * // Définition de la fonction de migration
     * nk_bool Migrate_PlayerData_v100_to_v110(
     *     nkentseu::NkArchive& archive,
     *     nkentseu::NkSchemaVersion from,
     *     nkentseu::NkSchemaVersion to) noexcept {
     *     // Implémentation de la migration...
     *     return true;
     * }
     *
     * // Enregistrement automatique — une seule fois, dans ce .cpp :
     * NK_REGISTER_MIGRATION(PlayerData, 1, 0, 0, 1, 1, 0, Migrate_PlayerData_v100_to_v110);
     * @endcode
     */
    #define NK_REGISTER_MIGRATION(Type, FromMaj, FromMin, FromPat,            \
                                ToMaj, ToMin, ToPat, FnPtr)                \
    namespace { struct _NkMig_##Type##_##FromMaj##_##ToMaj {                  \
        _NkMig_##Type##_##FromMaj##_##ToMaj() noexcept {                      \
            ::nkentseu::NkSchemaRegistry::RegisterMigration(                  \
                ::nkentseu::NkTypeOf<Type>(),                                 \
                {FromMaj, FromMin, FromPat},                                  \
                {ToMaj, ToMin, ToPat},                                        \
                FnPtr);                                                       \
        }                                                                     \
    } _sNkMig_##Type##_##FromMaj##_##ToMaj; }

    // =============================================================================
    // MACROS : HELPERS DE MIGRATION DE CHAMP PAR TYPE
    // =============================================================================
    /**
     * @def NK_ADD_FIELD_MIGRATION_BOOL(Type, FromMaj, FromMin, FromPat, ToMaj, ToMin, ToPat, FieldName, DefaultValue)
     * @brief Macro pour ajouter un champ booléen avec valeur par défaut
     * @ingroup SchemaMacros
     *
     * @example
     * @code
     * NK_ADD_FIELD_MIGRATION_BOOL(GameConfig, 1, 0, 0, 1, 1, 0, "settings.enableTutorial", true);
     * @endcode
     */
    #define NK_ADD_FIELD_MIGRATION_BOOL(Type, FromMaj, FromMin, FromPat, \
                                    ToMaj, ToMin, ToPat, FieldName, DefaultValue) \
        NK_SCHEMA_ADD_FIELD_MIGRATION_IMPL(Type, FromMaj, FromMin, FromPat, \
                                        ToMaj, ToMin, ToPat, FieldName, \
                                        []() -> nk_bool { return DefaultValue; })

    /**
     * @def NK_ADD_FIELD_MIGRATION_INT32(Type, FromMaj, FromMin, FromPat, ToMaj, ToMin, ToPat, FieldName, DefaultValue)
     * @brief Macro pour ajouter un champ int32 avec valeur par défaut
     * @ingroup SchemaMacros
     */
    #define NK_ADD_FIELD_MIGRATION_INT32(Type, FromMaj, FromMin, FromPat, \
                                        ToMaj, ToMin, ToPat, FieldName, DefaultValue) \
        NK_SCHEMA_ADD_FIELD_MIGRATION_IMPL(Type, FromMaj, FromMin, FromPat, \
                                        ToMaj, ToMin, ToPat, FieldName, \
                                        []() -> nk_int32 { return DefaultValue; })

    /**
     * @def NK_ADD_FIELD_MIGRATION_FLOAT64(Type, FromMaj, FromMin, FromPat, ToMaj, ToMin, ToPat, FieldName, DefaultValue)
     * @brief Macro pour ajouter un champ float64 avec valeur par défaut
     * @ingroup SchemaMacros
     */
    #define NK_ADD_FIELD_MIGRATION_FLOAT64(Type, FromMaj, FromMin, FromPat, \
                                        ToMaj, ToMin, ToPat, FieldName, DefaultValue) \
        NK_SCHEMA_ADD_FIELD_MIGRATION_IMPL(Type, FromMaj, FromMin, FromPat, \
                                        ToMaj, ToMin, ToPat, FieldName, \
                                        []() -> nk_float64 { return DefaultValue; })

    /**
     * @def NK_ADD_FIELD_MIGRATION_STRING(Type, FromMaj, FromMin, FromPat, ToMaj, ToMin, ToPat, FieldName, DefaultValue)
     * @brief Macro pour ajouter un champ string avec valeur par défaut
     * @ingroup SchemaMacros
     */
    #define NK_ADD_FIELD_MIGRATION_STRING(Type, FromMaj, FromMin, FromPat, \
                                        ToMaj, ToMin, ToPat, FieldName, DefaultValue) \
        NK_SCHEMA_ADD_FIELD_MIGRATION_IMPL(Type, FromMaj, FromMin, FromPat, \
                                        ToMaj, ToMin, ToPat, FieldName, \
                                        []() -> nkentseu::NkString { return DefaultValue; })

    // -------------------------------------------------------------------------
    // MACRO INTERNE : Implémentation commune des macros de migration de champ
    // -------------------------------------------------------------------------
    /**
     * @def NK_SCHEMA_ADD_FIELD_MIGRATION_IMPL
     * @brief Implémentation interne des macros NK_ADD_FIELD_MIGRATION_*
     * @note Ne pas utiliser directement — préférer les macros typées ci-dessus
     */
    #define NK_SCHEMA_ADD_FIELD_MIGRATION_IMPL(Type, FromMaj, FromMin, FromPat, \
                                            ToMaj, ToMin, ToPat, FieldName, GetterLambda) \
        namespace { \
        struct _NkFieldMig_##Type##_##FromMaj##_##ToMaj##_##__LINE__ { \
            _NkFieldMig_##Type##_##FromMaj##_##ToMaj##_##__LINE__() noexcept { \
                ::nkentseu::NkSchemaRegistry::AddFieldMigration( \
                    ::nkentseu::NkTypeOf<Type>(), \
                    {FromMaj, FromMin, FromPat}, \
                    {ToMaj, ToMin, ToPat}, \
                    FieldName, \
                    GetterLambda); \
            } \
        } _sNkFieldMig_##Type##_##FromMaj##_##ToMaj##_##__LINE__; \
        }


#endif // NKENTSEU_SERIALIZATION_NKSCHEMAVERSIONING_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKSCHEMAVERSIONING.H
// =============================================================================
// Ce fichier fournit le système de migration de schéma pour assets versionnés.
// Il combine versionnage sémantique, registry de migrations et macros d'enregistrement.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Définition d'un type avec versionnage et migration simple
// -----------------------------------------------------------------------------
/*
    // PlayerData.h
    class PlayerData : public nkentseu::NkISerializable {
    public:
        nk_uint32 level = 1;
        nk_float32 health = 100.f;
        // Nouveau champ en v1.1.0 :
        nk_float32 stamina = 50.f;

        // ... Serialize/Deserialize/GetTypeName ...
    };

    // PlayerData.cpp
    #include "NKSerialization/NkSchemaVersioning.h"

    // Déclaration de la version courante
    NK_SCHEMA_CURRENT_VERSION(PlayerData, 1, 1, 0);

    // Fonction de migration v1.0.0 → v1.1.0 : ajoute le champ stamina
    nk_bool Migrate_PlayerData_v100_to_v110(
        nkentseu::NkArchive& archive,
        nkentseu::NkSchemaVersion from,
        nkentseu::NkSchemaVersion to) noexcept {

        // Validation des versions (sécurité)
        if (from != nkentseu::NkSchemaVersion(1, 0, 0)
            || to != nkentseu::NkSchemaVersion(1, 1, 0)) {
            return false;
        }

        // Ajout du nouveau champ avec valeur par défaut si absent
        if (!archive.Has("stamina")) {
            archive.SetFloat32("stamina", 50.f);
        }

        return true;
    }

    // Enregistrement de la migration
    NK_REGISTER_MIGRATION(PlayerData, 1, 0, 0, 1, 1, 0, Migrate_PlayerData_v100_to_v110);
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Chaînage de migrations sur plusieurs versions
// -----------------------------------------------------------------------------
/*
    // Supposons : version courante = 1.3.0, asset stocké en 1.0.0
    // Migrations enregistrées :
    //   1.0.0 → 1.1.0 : ajoute "stamina"
    //   1.1.0 → 1.2.0 : renomme "health" → "hitPoints"
    //   1.2.0 → 1.3.0 : ajoute "inventory.slotCount"

    // Au chargement, MigrateArchive() appliquera automatiquement :
    //   1.0.0 → 1.1.0 → 1.2.0 → 1.3.0
    // dans cet ordre, en s'arrêtant si une migration échoue.

    nkentseu::NkArchive assetData;
    // ... charger depuis fichier ...

    nkentseu::NkSchemaVersion stored{1, 0, 0};  // Lu depuis __meta__.schema_version
    nkentseu::NkString errorMsg;

    if (!nkentseu::NkSchemaRegistry::Global().MigrateArchive(
            nkentseu::NkTypeOf<PlayerData>(),
            assetData, stored, &errorMsg)) {
        logger.Error("Migration failed: %s\n", errorMsg.CStr());
        return false;
    }

    // assetData est maintenant à jour vers 1.3.0
    // Les champs "stamina", "hitPoints", "inventory.slotCount" sont présents
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Gestion des erreurs de migration
// -----------------------------------------------------------------------------
/*
    // Cas 1 : Type non enregistré pour le versionnage
    nkentseu::NkString err;
    bool ok = nkentseu::NkSchemaRegistry::Global().MigrateArchive(
        nkentseu::NkTypeOf<UnknownType>(), archive, {1, 0, 0}, &err);
    // ok == false, err == "Type not registered for versioning"

    // Cas 2 : Chemin de migration manquant
    // Si asset est en v1.0.0, version courante est v1.3.0,
    // mais seule la migration 1.2.0→1.3.0 est enregistrée :
    bool ok = nkentseu::NkSchemaRegistry::Global().MigrateArchive(
        nkentseu::NkTypeOf<PlayerData>(), archive, {1, 0, 0}, &err);
    // ok == false, err == "No migration path from 1.0.0 to 1.3.0"

    // Cas 3 : Échec d'une fonction de migration
    // Si Migrate_v100_to_v110 retourne false (ex: données corrompues) :
    bool ok = nkentseu::NkSchemaRegistry::Global().MigrateArchive(
        nkentseu::NkTypeOf<PlayerData>(), archive, {1, 0, 0}, &err);
    // ok == false, err == "Migration 1.0.0 → 1.1.0 failed"
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Bonnes pratiques pour les fonctions de migration
// -----------------------------------------------------------------------------
/*
    // ✅ TOUJOURS valider les versions from/to en début de fonction
    nk_bool SafeMigration(nkentseu::NkArchive& archive,
                          nkentseu::NkSchemaVersion from,
                          nkentseu::NkSchemaVersion to) noexcept {
        if (from != expectedFrom || to != expectedTo) {
            return false;  // Rejet silencieux ou logging selon politique
        }
        // ... logique de migration ...
        return true;
    }

    // ✅ Rendre les migrations idempotentes (appliquer 2x = appliquer 1x)
    nk_bool IdempotentMigration(nkentseu::NkArchive& archive,
                                nkentseu::NkSchemaVersion,
                                nkentseu::NkSchemaVersion) noexcept {
        // Vérifier avant d'ajouter pour éviter les doublons
        if (!archive.Has("newField")) {
            archive.SetString("newField", "defaultValue");
        }
        return true;
    }

    // ✅ Gérer les erreurs partielles : rollback ou échec total
    nk_bool AtomicMigration(nkentseu::NkArchive& archive,
                            nkentseu::NkSchemaVersion,
                            nkentseu::NkSchemaVersion) noexcept {
        // Option 1 : valider d'abord, puis appliquer
        if (!CanApplyMigration(archive)) {
            return false;  // Échec avant toute modification
        }
        ApplyMigrationChanges(archive);  // Supposé ne pas échouer
        return true;

        // Option 2 : journaliser les changements pour rollback manuel
        // (plus complexe, nécessite un système de transaction custom)
    }

    // ❌ NE JAMAIS lever d'exceptions dans une MigrationFn
    // Les fonctions doivent être noexcept et retourner false en cas d'erreur
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Migration de structure complexe (renommage + transformation)
// -----------------------------------------------------------------------------
/*
    // Migration : v2.0.0 → v2.1.0
    // Changements :
    //  - "player.health" → "player.hitPoints" (renommage)
    //  - "player.level" : int → float avec scaling x10
    //  - Ajout de "player.experience" avec valeur calculée

    nk_bool Migrate_Player_v200_to_v210(
        nkentseu::NkArchive& archive,
        nkentseu::NkSchemaVersion,
        nkentseu::NkSchemaVersion) noexcept {

        // 1. Renommage de champ avec migration de valeur
        nk_float32 health = 100.f;  // valeur par défaut
        if (archive.GetFloat32("player.health", health)) {
            archive.SetFloat32("player.hitPoints", health);
            archive.Remove("player.health");
        }

        // 2. Transformation de type avec scaling
        nk_int32 levelInt = 1;
        if (archive.GetInt32("player.level", levelInt)) {
            nk_float32 levelFloat = static_cast<nk_float32>(levelInt) * 10.f;
            archive.SetFloat32("player.level", levelFloat);
        }

        // 3. Ajout de champ calculé
        nk_float32 hitPoints = 100.f;
        nk_float32 level = 10.f;
        archive.GetFloat32("player.hitPoints", hitPoints);
        archive.GetFloat32("player.level", level);
        nk_float32 experience = (hitPoints - 50.f) * level;  // Formule arbitraire
        archive.SetFloat32("player.experience", experience);

        return true;
    }
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Test unitaire d'une fonction de migration
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkSchemaVersioning.h>
    #include <NKTest/NkUnitTest.h>  // Framework de test maison

    NK_UNIT_TEST(Migration_PlayerData_v100_to_v110) {
        // Setup : archive en version 1.0.0 sans le champ "stamina"
        nkentseu::NkArchive testArchive;
        testArchive.SetInt32("level", 5);
        testArchive.SetFloat32("health", 75.f);
        // "stamina" est absent → doit être ajouté par la migration

        // Execution : appliquer la migration
        nk_bool result = Migrate_PlayerData_v100_to_v110(
            testArchive,
            nkentseu::NkSchemaVersion(1, 0, 0),
            nkentseu::NkSchemaVersion(1, 1, 0));

        // Assertions
        NK_ASSERT_TRUE(result, "Migration should succeed");

        nk_float32 stamina = 0.f;
        NK_ASSERT_TRUE(testArchive.GetFloat32("stamina", stamina),
                       "stamina field should exist after migration");
        NK_ASSERT_FLOAT_EQ(stamina, 50.f, 0.001f,
                           "stamina should have default value 50.0");

        // Vérifier que les champs existants sont préservés
        nk_int32 level = 0;
        NK_ASSERT_TRUE(testArchive.GetInt32("level", level),
                       "level field should be preserved");
        NK_ASSERT_INT_EQ(level, 5, "level value should be unchanged");
    }
*/

// =============================================================================
// EXEMPLES D'UTILISATION — AddFieldMigration
// =============================================================================

// -----------------------------------------------------------------------------
// Exemple 1 : Ajout d'un champ booléen simple
// -----------------------------------------------------------------------------
/*
    // Dans GameConfig.cpp
    #include "NKSerialization/NkSchemaVersioning.h"

    // Déclaration de version courante
    NK_SCHEMA_CURRENT_VERSION(GameConfig, 1, 1, 0);

    // Migration v1.0.0 → v1.1.0 : ajoute "audio.enableMusic" avec défaut true
    NK_ADD_FIELD_MIGRATION_BOOL(
        GameConfig,
        1, 0, 0,  // from: 1.0.0
        1, 1, 0,  // to: 1.1.0
        "audio.enableMusic",
        true);    // valeur par défaut

    // Résultat : les configs v1.0.0 chargées auront audio.enableMusic=true
    // Les configs déjà en v1.1.0+ ne sont pas modifiées (idempotence)
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Ajout d'un champ avec valeur calculée dynamiquement
// -----------------------------------------------------------------------------
/*
    // Migration : ajoute "graphics.maxFPS" basé sur la plateforme
    nkentseu::NkSchemaRegistry::AddFieldMigration(
        nkentseu::NkTypeOf<GraphicsConfig>(),
        {2, 0, 0}, {2, 1, 0},
        "graphics.maxFPS",
        []() -> nk_int32 {
            #ifdef NKENTSEU_PLATFORM_MOBILE
                return 30;  // Mobile : limiter à 30 FPS pour batterie
            #elif defined(NKENTSEU_PLATFORM_CONSOLE)
                return 60;  // Console : 60 FPS standard
            #else
                return 144; // PC : viser haut de gamme
            #endif
        });
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Ajout d'un champ string avec localisation
// -----------------------------------------------------------------------------
/*
    // Migration : ajoute "ui.language" avec détection de locale système
    nkentseu::NkSchemaRegistry::AddFieldMigration(
        nkentseu::NkTypeOf<UserPreferences>(),
        {1, 2, 0}, {1, 3, 0},
        "ui.language",
        []() -> nkentseu::NkString {
            // Détection simplifiée — en production, utiliser une API système
            const char* envLang = std::getenv("LANG");
            if (envLang && std::strncmp(envLang, "fr", 2) == 0) {
                return nkentseu::NkString("fr-FR");
            }
            return nkentseu::NkString("en-US");  // Fallback
        });
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Chaînage de multiples ajouts de champs
// -----------------------------------------------------------------------------
/*
    // Migration v3.0.0 → v3.1.0 : ajoute 3 nouveaux champs simultanément
    NK_SCHEMA_CURRENT_VERSION(PlayerData, 3, 1, 0);

    // Champ 1 : nk_uint64 "player.totalPlayTimeSeconds"
    nkentseu::NkSchemaRegistry::AddFieldMigration(
        nkentseu::NkTypeOf<PlayerData>(),
        {3, 0, 0}, {3, 1, 0},
        "player.totalPlayTimeSeconds",
        []() -> nk_uint64 { return 0; });

    // Champ 2 : nk_float32 "player.achievementProgress"
    nkentseu::NkSchemaRegistry::AddFieldMigration(
        nkentseu::NkTypeOf<PlayerData>(),
        {3, 0, 0}, {3, 1, 0},
        "player.achievementProgress",
        []() -> nk_float32 { return 0.0f; });

    // Champ 3 : NkString "player.lastLoginPlatform"
    nkentseu::NkSchemaRegistry::AddFieldMigration(
        nkentseu::NkTypeOf<PlayerData>(),
        {3, 0, 0}, {3, 1, 0},
        "player.lastLoginPlatform",
        []() -> nkentseu::NkString { return nkentseu::NkString("unknown"); });

    // Résultat : les trois champs sont ajoutés en une seule passe de migration
    // L'ordre d'exécution est déterministe (ordre d'enregistrement)
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Migration conditionnelle avec logique complexe
// -----------------------------------------------------------------------------
/*
    // Migration : ajoute "network.useEncryption" seulement si ancienne version
    nkentseu::NkSchemaRegistry::AddFieldMigration(
        nkentseu::NkTypeOf<NetworkConfig>(),
        {1, 0, 0}, {1, 1, 0},
        "network.useEncryption",
        []() -> nk_bool {
            // Valeur par défaut dépendante de la configuration build
            #ifndef NKENTSEU_BUILD_UNSAFE
                return true;   // Production : encryption activée par défaut
            #else
                return false;  // Debug : désactivée pour faciliter le testing
            #endif
        });
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. ORDRE D'ENREGISTREMENT DES MIGRATIONS :
       - Enregistrer TOUS les SetCurrentVersion/RegisterMigration au démarrage
       - Dans un seul thread, avant tout chargement d'asset
       - Typiquement : dans une fonction InitSerialization() appelée depuis main()

    2. CONCEPTION DES FONCTIONS DE MIGRATION :
       - Toujours valider from/to en début de fonction pour sécurité
       - Rendre les migrations idempotentes (safe à appliquer multiple fois)
       - Éviter les transformations irréversibles si possible
       - Documenter clairement les changements de schéma dans les commentaires

    3. GESTION DES ERREURS :
       - Retourner false immédiatement en cas d'erreur détectée
       - Ne pas modifier partiellement l'archive en cas d'échec
       - Utiliser le paramètre `err` de MigrateArchive() pour debugging en production

    4. PERFORMANCE :
       - Recherche linéaire dans mMigrations : O(m) par étape de migration
       - Max 256 étapes de migration : évite les boucles infinies accidentelles
       - Typiquement négligeable car appelé uniquement au chargement d'assets

    5. COMPATIBILITÉ RETOUR (DOWNMIGRATION) :
       - Le système ne supporte que les migrations ascendantes (from < to)
       - Pour le downgrade : définir des migrations inverses séparément
       - Attention : toutes les migrations ne sont pas réversibles (perte de données)

    6. PERSISTANCE DES VERSIONS :
       - Stocker la version dans "__meta__.schema_version" dans chaque asset
       - Lire cette version au chargement pour déterminer les migrations à appliquer
       - Mettre à jour cette méta-donnée après migration réussie

    7. EXTENSIBILITÉ FUTURES :
       - Ajouter un système de "migration conditionnelle" (selon plateforme, config, etc.)
       - Support des migrations asynchrones pour les assets volumineux
       - Journalisation détaillée des transformations appliquées pour audit
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================