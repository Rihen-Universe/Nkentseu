// =============================================================================
// NKSerialization/NkArchive.h
// Archive centrale clé/valeur servant d'intermédiaire universel entre formats.
//
// Design :
//  - Architecture hiérarchique : NkArchiveValue → NkArchiveNode → NkArchive
//  - Support multi-format : JSON, YAML, XML, Binary, NkNative via interface unifiée
//  - Double API : plate (clés simples) ET hiérarchique (chemins "a.b.c")
//  - Sérialisation canonique : représentation textuelle + union binaire optimisée
//  - Compatibilité Unreal-style : méta-données via clé "__meta__"
//  - Export contrôlé via NKENTSEU_SERIALIZATION_CLASS_EXPORT
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Enums en UPPER_SNAKE_CASE strict pour cohérence projet
//
// Hiérarchie des types :
//   NkArchiveValue  → scalaire portable (null, bool, int64, uint64, float64, string)
//   NkArchiveNode   → conteneur polymorphe (scalaire | objet | array)
//   NkArchive       → table associative ordonnée clé → NkArchiveNode
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_SERIALIZATION_NKARCHIVE_H
#define NKENTSEU_SERIALIZATION_NKARCHIVE_H

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusions des types de base et conteneurs du projet.
    // Dépendances minimales pour éviter les cycles de compilation.

    #include "NKCore/NkTypes.h"
    #include "NKContainers/Sequential/NkVector.h"
    #include "NKContainers/String/NkString.h"
    #include "NKContainers/String/NkStringView.h"
    #include "NKSerialization/NkSerializationApi.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les symboles du module serialization sont dans le namespace nkentseu.
    // Pas de sous-namespace pour simplifier l'usage et l'intégration.

    namespace nkentseu {


        // ---------------------------------------------------------------------
        // ÉNUMÉRATION : NkArchiveValueType
        // DESCRIPTION : Types scalaires supportés par l'archive
        // ---------------------------------------------------------------------
        /**
         * @enum NkArchiveValueType
         * @brief Types de valeurs scalaires portables dans NkArchive
         * @ingroup SerializationTypes
         *
         * Convention : UPPER_SNAKE_CASE avec préfixe NK_VALUE_ pour unicité globale.
         *
         * Ces types définissent le discriminant de stockage pour NkArchiveValue.
         * Chaque type possède une représentation binaire (union Raw) et textuelle (NkString).
         *
         * @note NK_VALUE_NULL représente l'absence de valeur (équivalent JSON null).
         * @note Les conversions entre types numériques sont gérées automatiquement dans les getters.
         */
        enum class NkArchiveValueType : nk_uint8 {

            /// @brief Valeur nulle (absence de donnée)
            NK_VALUE_NULL    = 0,

            /// @brief Valeur booléenne (true/false)
            NK_VALUE_BOOL    = 1,

            /// @brief Entier signé 64 bits
            NK_VALUE_INT64   = 2,

            /// @brief Entier non-signé 64 bits
            NK_VALUE_UINT64  = 3,

            /// @brief Flottant double précision 64 bits
            NK_VALUE_FLOAT64 = 4,

            /// @brief Chaîne de caractères UTF-8
            NK_VALUE_STRING  = 5,

        }; // enum class NkArchiveValueType


        // ---------------------------------------------------------------------
        // ÉNUMÉRATION : NkNodeKind
        // DESCRIPTION : Discriminant de type pour NkArchiveNode
        // ---------------------------------------------------------------------
        /**
         * @enum NkNodeKind
         * @brief Types de nœuds dans l'arbre hiérarchique de l'archive
         * @ingroup SerializationTypes
         *
         * Convention : UPPER_SNAKE_CASE avec préfixe NK_NODE_ pour cohérence.
         *
         * Ce discriminant permet à NkArchiveNode de stocker polymorphiquement :
         *  - Un scalaire (NkArchiveValue)
         *  - Un objet imbriqué (NkArchive*)
         *  - Un tableau de nœuds (NkVector<NkArchiveNode>)
         *
         * @note Seul un variant est actif à la fois (mémoire optimisée).
         * @note La gestion manuelle de l'union est encapsulée dans les méthodes de NkArchiveNode.
         */
        enum class NkNodeKind : nk_uint8 {

            /// @brief Nœud contenant une valeur scalaire
            NK_NODE_SCALAR = 0,

            /// @brief Nœud contenant un objet imbriqué (table clé/valeur)
            NK_NODE_OBJECT = 1,

            /// @brief Nœud contenant un tableau ordonné de nœuds
            NK_NODE_ARRAY  = 2,

        }; // enum class NkNodeKind


        // ---------------------------------------------------------------------
        // FORWARD DECLARATIONS
        // ---------------------------------------------------------------------
        // Déclarations anticipées pour briser les dépendances circulaires.

        struct NkArchiveValue;
        struct NkArchiveEntry;
        class  NkArchive;
        struct NkArchiveNode;


        // =============================================================================
        // STRUCTURE : NkArchiveValue
        // DESCRIPTION : Valeur scalaire portable avec double représentation
        // =============================================================================
        /**
         * @struct NkArchiveValue
         * @brief Représentation canonique d'une valeur scalaire dans l'archive
         * @ingroup SerializationComponents
         *
         * Architecture double-représentation :
         *  - Union `raw` : stockage binaire natif pour performance (formats binaires)
         *  - Champ `text` : représentation textuelle canonique (formats texte JSON/YAML/XML)
         *
         * Avantages :
         *  - Sérialisation directe sans parsing pour les formats binaires
         *  - Formatage lazy pour les formats texte (text généré à la demande si besoin)
         *  - Comparaison rapide via texte pour l'unicité sémantique
         *
         * Thread-safety :
         *  - Structure POD-like : copie bitwise safe
         *  - Aucune mutation interne après construction (immutabilité sémantique)
         *
         * @note Les factory methods garantissent la cohérence raw/text à la création.
         * @note L'opérateur== compare uniquement le type et le texte (abstraction binaire).
         *
         * @example Création de valeurs
         * @code
         * auto nullVal = nkentseu::NkArchiveValue::Null();
         * auto boolVal = nkentseu::NkArchiveValue::FromBool(true);
         * auto intVal  = nkentseu::NkArchiveValue::FromInt64(42);
         * auto strVal  = nkentseu::NkArchiveValue::FromString("hello");
         * @endcode
         */
        struct NKENTSEU_SERIALIZATION_API NkArchiveValue {


            // -----------------------------------------------------------------
            // MEMBRES DE DONNÉES
            // -----------------------------------------------------------------
            /**
             * @brief Type discriminant de la valeur scalaire
             * @note Défaut : NK_VALUE_NULL pour sécurité (valeur indéfinie)
             */
            NkArchiveValueType type = NkArchiveValueType::NK_VALUE_NULL;

            /**
             * @brief Union de stockage binaire natif pour performance
             * @ingroup ArchiveValueInternals
             *
             * Stockage little-endian natif pour sérialisation directe.
             * Initialisé à zéro par défaut pour éviter les lectures non-initialisées.
             */
            union Raw {

                /// @brief Représentation booléenne
                nk_bool b;

                /// @brief Représentation entier signé 64 bits
                nk_int64 i;

                /// @brief Représentation entier non-signé 64 bits
                nk_uint64 u;

                /// @brief Représentation flottant 64 bits
                nk_float64 f;

                /**
                 * @brief Constructeur par défaut initialisant à zéro
                 * @note Évite les valeurs indéfinies en mémoire
                 */
                Raw() noexcept : u(0) {}

            } raw;  // Union de stockage binaire

            /**
             * @brief Représentation textuelle canonique de la valeur
             * @note Toujours synchronisée avec `raw` après construction/modification
             * @note Utilisée pour : comparaison, sérialisation texte, debugging
             */
            NkString text;


            // -----------------------------------------------------------------
            // MÉTHODES STATIQUES : FACTORIES
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveValueFactories Méthodes de Création
             * @brief Factory methods type-safe pour construction de NkArchiveValue
             *
             * Ces méthodes garantissent :
             *  - Initialisation correcte de `type`, `raw` et `text`
             *  - Formatage canonique du texte (ex: "%.17g" pour les floats)
             *  - Aucune allocation dynamique inutile
             */

            /**
             * @brief Crée une valeur null (absence de donnée)
             * @return NkArchiveValue initialisé à NK_VALUE_NULL
             * @note Méthode noexcept : garantie de non-levée d'exception
             */
            [[nodiscard]] static NkArchiveValue Null() noexcept;

            /**
             * @brief Crée une valeur booléenne
             * @param v Valeur booléenne à encapsuler
             * @return NkArchiveValue avec type NK_VALUE_BOOL et text "true"/"false"
             */
            [[nodiscard]] static NkArchiveValue FromBool(nk_bool v) noexcept;

            /**
             * @brief Crée une valeur entier signé 32 bits (convertie en 64 bits interne)
             * @param v Valeur int32 à encapsuler
             * @return NkArchiveValue avec type NK_VALUE_INT64
             * @note Conversion implicite vers int64 pour unicité du type interne
             */
            [[nodiscard]] static NkArchiveValue FromInt32(nk_int32 v) noexcept;

            /**
             * @brief Crée une valeur entier signé 64 bits
             * @param v Valeur int64 à encapsuler
             * @return NkArchiveValue avec type NK_VALUE_INT64 et formatage décimal
             */
            [[nodiscard]] static NkArchiveValue FromInt64(nk_int64 v) noexcept;

            /**
             * @brief Crée une valeur entier non-signé 32 bits (convertie en 64 bits interne)
             * @param v Valeur uint32 à encapsuler
             * @return NkArchiveValue avec type NK_VALUE_UINT64
             */
            [[nodiscard]] static NkArchiveValue FromUInt32(nk_uint32 v) noexcept;

            /**
             * @brief Crée une valeur entier non-signé 64 bits
             * @param v Valeur uint64 à encapsuler
             * @return NkArchiveValue avec type NK_VALUE_UINT64 et formatage décimal
             */
            [[nodiscard]] static NkArchiveValue FromUInt64(nk_uint64 v) noexcept;

            /**
             * @brief Crée une valeur flottant 32 bits (convertie en 64 bits interne)
             * @param v Valeur float32 à encapsuler
             * @return NkArchiveValue avec type NK_VALUE_FLOAT64
             * @note Conversion vers double pour précision maximale en stockage
             */
            [[nodiscard]] static NkArchiveValue FromFloat32(nk_float32 v) noexcept;

            /**
             * @brief Crée une valeur flottant 64 bits avec précision maximale
             * @param v Valeur float64 à encapsuler
             * @return NkArchiveValue avec type NK_VALUE_FLOAT64 et formatage "%.17g"
             * @note "%.17g" garantit la réversibilité float→string→float sans perte
             */
            [[nodiscard]] static NkArchiveValue FromFloat64(nk_float64 v) noexcept;

            /**
             * @brief Crée une valeur chaîne de caractères
             * @param v Vue de chaîne à copier (NkStringView pour efficacité)
             * @return NkArchiveValue avec type NK_VALUE_STRING et texte copié
             */
            [[nodiscard]] static NkArchiveValue FromString(NkStringView v) noexcept;


            // -----------------------------------------------------------------
            // MÉTHODES : PREDICATS DE TYPE
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveValuePredicates Prédicats de Type
             * @brief Méthodes de query type-safe pour interrogation rapide
             *
             * Toutes ces méthodes sont noexcept et inline-friendly.
             * Utilisables dans des conditions de filtrage sans overhead.
             */

            /// @brief Vérifie si la valeur est null
            /// @return true si type == NK_VALUE_NULL
            [[nodiscard]] bool IsNull() const noexcept { return type == NkArchiveValueType::NK_VALUE_NULL; }

            /// @brief Vérifie si la valeur est booléenne
            /// @return true si type == NK_VALUE_BOOL
            [[nodiscard]] bool IsBool() const noexcept { return type == NkArchiveValueType::NK_VALUE_BOOL; }

            /// @brief Vérifie si la valeur est entier signé
            /// @return true si type == NK_VALUE_INT64
            [[nodiscard]] bool IsInt() const noexcept { return type == NkArchiveValueType::NK_VALUE_INT64; }

            /// @brief Vérifie si la valeur est entier non-signé
            /// @return true si type == NK_VALUE_UINT64
            [[nodiscard]] bool IsUInt() const noexcept { return type == NkArchiveValueType::NK_VALUE_UINT64; }

            /// @brief Vérifie si la valeur est flottant
            /// @return true si type == NK_VALUE_FLOAT64
            [[nodiscard]] bool IsFloat() const noexcept { return type == NkArchiveValueType::NK_VALUE_FLOAT64; }

            /// @brief Vérifie si la valeur est chaîne de caractères
            /// @return true si type == NK_VALUE_STRING
            [[nodiscard]] bool IsString() const noexcept { return type == NkArchiveValueType::NK_VALUE_STRING; }

            /// @brief Vérifie si la valeur est numérique (int/uint/float)
            /// @return true pour tout type permettant des opérations arithmétiques
            [[nodiscard]] bool IsNumeric() const noexcept {
                return type == NkArchiveValueType::NK_VALUE_INT64
                    || type == NkArchiveValueType::NK_VALUE_UINT64
                    || type == NkArchiveValueType::NK_VALUE_FLOAT64;
            }


            // -----------------------------------------------------------------
            // OPÉRATEURS DE COMPARAISON
            // -----------------------------------------------------------------
            /**
             * @brief Égalité sémantique : compare type et représentation textuelle
             * @param o Autre valeur à comparer
             * @return true si même type et même texte canonique
             * @note Ne compare pas l'union `raw` : abstraction de la représentation binaire
             */
            bool operator==(const NkArchiveValue& o) const noexcept {
                return type == o.type && text == o.text;
            }

            /**
             * @brief Inégalité : négation de operator==
             * @param o Autre valeur à comparer
             * @return true si les valeurs diffèrent en type ou texte
             */
            bool operator!=(const NkArchiveValue& o) const noexcept {
                return !(*this == o);
            }


        }; // struct NkArchiveValue


        // =============================================================================
        // STRUCTURE : NkArchiveNode
        // DESCRIPTION : Nœud polymorphe de l'arbre hiérarchique
        // =============================================================================
        /**
         * @struct NkArchiveNode
         * @brief Conteneur polymorphe pour valeur scalaire, objet ou tableau
         * @ingroup SerializationComponents
         *
         * Pattern Variant manuel optimisé :
         *  - Discriminant `kind` pour identification rapide du type actif
         *  - Union sémantique : `value` (scalaire), `object` (NkArchive*), `array` (vector)
         *  - Gestion manuelle de la mémoire pour éviter le coût de std::variant
         *
         * Règles de possession :
         *  - `object` : owning raw pointer (détruite dans le destructeur)
         *  - `array` : valeur sémantique (copie/move via NkVector)
         *  - `value` : valeur sémantique (copie/move trivial)
         *
         * Thread-safety :
         *  - Non thread-safe par conception : synchronisation déléguée au conteneur parent
         *  - Copie profonde via constructeur copy/move pour isolation des données
         *
         * @note Les méthodes Make*() permettent la mutation de type avec cleanup automatique.
         * @note Is*() methods sont noexcept et utilisables en conditions de filtrage.
         *
         * @example Manipulation de nœuds
         * @code
         * // Nœud scalaire
         * nkentseu::NkArchiveNode scalar(nkentseu::NkArchiveValue::FromInt64(42));
         *
         * // Nœud objet
         * nkentseu::NkArchive obj;
         * obj.SetInt32("age", 30);
         * node.SetObject(obj);  // Transfert de propriété
         *
         * // Nœud tableau
         * node.MakeArray();
         * node.array.PushBack(nkentseu::NkArchiveNode(nkentseu::NkArchiveValue::FromString("item1")));
         * @endcode
         */
        struct NKENTSEU_SERIALIZATION_API NkArchiveNode {


            // -----------------------------------------------------------------
            // MEMBRES DE DONNÉES
            // -----------------------------------------------------------------
            /**
             * @brief Discriminant de type du nœud
             * @note Défaut : NK_NODE_SCALAR pour sécurité
             */
            NkNodeKind kind = NkNodeKind::NK_NODE_SCALAR;

            /**
             * @brief Valeur scalaire (actif si kind == NK_NODE_SCALAR)
             * @note Stockage par valeur : copie/move triviaux
             */
            NkArchiveValue value;

            /**
             * @brief Pointeur vers objet imbriqué (actif si kind == NK_NODE_OBJECT)
             * @note Owning pointer : détruite dans FreeObject() et le destructeur
             * @note nullptr si non actif : vérification via IsObject() requise
             */
            NkArchive* object = nullptr;

            /**
             * @brief Tableau de nœuds (actif si kind == NK_NODE_ARRAY)
             * @note Stockage par valeur : gestion automatique via NkVector
             */
            NkVector<NkArchiveNode> array;


            // -----------------------------------------------------------------
            // CONSTRUCTEURS ET DESTRUCTEUR (BIG FIVE)
            // -----------------------------------------------------------------
            /**
             * @brief Constructeur par défaut : nœud scalaire null
             * @note noexcept : garantie de non-levée d'exception
             */
            NkArchiveNode() noexcept = default;

            /**
             * @brief Constructeur depuis valeur scalaire
             * @param v Valeur à encapsuler dans un nœud scalaire
             * @post kind = NK_NODE_SCALAR, value = v, object = nullptr, array vidé
             */
            explicit NkArchiveNode(const NkArchiveValue& v) noexcept
                : kind(NkNodeKind::NK_NODE_SCALAR)
                , value(v)
            {}

            /**
             * @brief Constructeur de copie : duplication profonde
             * @param o Nœud source à dupliquer
             * @post Copie profonde de l'objet si kind == NK_NODE_OBJECT
             */
            NkArchiveNode(const NkArchiveNode& o) noexcept;

            /**
             * @brief Constructeur de move : transfert de propriété
             * @param o Nœud source dont les ressources sont transférées
             * @post o.kind réinitialisé à NK_NODE_SCALAR, o.object = nullptr
             */
            NkArchiveNode(NkArchiveNode&& o) noexcept;

            /**
             * @brief Opérateur d'affectation par copie
             * @param o Nœud source à copier
             * @return Référence vers ce nœud après copie profonde
             * @note Auto-assignation protégée, cleanup préalable des ressources existantes
             */
            NkArchiveNode& operator=(const NkArchiveNode& o) noexcept;

            /**
             * @brief Opérateur d'affectation par move
             * @param o Nœud source dont les ressources sont transférées
             * @return Référence vers ce nœud après transfert
             * @note Auto-assignation protégée, cleanup préalable des ressources existantes
             */
            NkArchiveNode& operator=(NkArchiveNode&& o) noexcept;

            /**
             * @brief Destructeur : libération des ressources possédées
             * @post FreeObject() appelé pour détruire l'objet imbriqué si présent
             */
            ~NkArchiveNode() noexcept;


            // -----------------------------------------------------------------
            // MÉTHODES DE MUTATION DE TYPE
            // -----------------------------------------------------------------
            /**
             * @brief Configure ce nœud comme objet imbriqué
             * @param arc Archive à copier dans le nouveau sous-objet
             * @post kind = NK_NODE_OBJECT, object = new NkArchive(arc), array vidé
             * @note Copie profonde de l'archive : pas de partage de pointeur
             */
            void SetObject(const NkArchive& arc) noexcept;

            /**
             * @brief Reconfigure ce nœud comme tableau vide
             * @post kind = NK_NODE_ARRAY, object détruite si présente, array vidé
             * @note Méthode noexcept : garantie de non-levée d'exception
             */
            void MakeArray() noexcept {
                FreeObject();
                kind = NkNodeKind::NK_NODE_ARRAY;
                array.Clear();
            }

            /**
             * @brief Reconfigure ce nœud comme scalaire null
             * @post kind = NK_NODE_SCALAR, object détruite si présente, array vidé, value = Null()
             * @note Méthode noexcept : garantie de non-levée d'exception
             */
            void MakeScalar() noexcept {
                FreeObject();
                kind = NkNodeKind::NK_NODE_SCALAR;
                array.Clear();
                value = NkArchiveValue::Null();
            }


            // -----------------------------------------------------------------
            // PRÉDICATS DE TYPE
            // -----------------------------------------------------------------
            /// @brief Vérifie si ce nœud contient un scalaire
            /// @return true si kind == NK_NODE_SCALAR
            [[nodiscard]] bool IsScalar() const noexcept { return kind == NkNodeKind::NK_NODE_SCALAR; }

            /// @brief Vérifie si ce nœud contient un objet valide
            /// @return true si kind == NK_NODE_OBJECT ET object != nullptr
            [[nodiscard]] bool IsObject() const noexcept { return kind == NkNodeKind::NK_NODE_OBJECT && object; }

            /// @brief Vérifie si ce nœud contient un tableau
            /// @return true si kind == NK_NODE_ARRAY
            [[nodiscard]] bool IsArray() const noexcept { return kind == NkNodeKind::NK_NODE_ARRAY; }


            // -----------------------------------------------------------------
            // MÉTHODES PRIVÉES D'IMPLÉMENTATION
            // -----------------------------------------------------------------
        private:

            /**
             * @brief Libère l'objet imbriqué si présent
             * @post object = nullptr, mémoire libérée via delete
             * @note Méthode noexcept : delete sur nullptr est safe en C++
             */
            void FreeObject() noexcept;


        }; // struct NkArchiveNode


        // =============================================================================
        // STRUCTURE : NkArchiveEntry
        // DESCRIPTION : Paire clé/valeur pour le stockage dans NkArchive
        // =============================================================================
        /**
         * @struct NkArchiveEntry
         * @brief Élément de stockage : association clé NkString → NkArchiveNode
         * @ingroup SerializationComponents
         *
         * Structure POD-like pour performance :
         *  - Copie/move triviaux via les membres NkString et NkArchiveNode
         *  - Pas de logique métier : simple conteneur de données
         *
         * @note Utilisé exclusivement dans NkVector<NkArchiveEntry> pour mEntries.
         * @note L'ordre d'insertion est préservé : itération déterministe garantie.
         */
        struct NKENTSEU_SERIALIZATION_API NkArchiveEntry {

            /// @brief Clé d'identification unique dans l'archive
            NkString key;

            /// @brief Nœud associé contenant la valeur (scalaire/objet/array)
            NkArchiveNode node;

        }; // struct NkArchiveEntry


        // =============================================================================
        // CLASSE : NkArchive
        // DESCRIPTION : Table associative ordonnée clé → NkArchiveNode
        // =============================================================================
        /**
         * @class NkArchive
         * @brief Conteneur hiérarchique clé/valeur pour sérialisation multi-format
         * @ingroup SerializationComponents
         *
         * Fonctionnalités principales :
         *  - API plate : Set/Get scalaires par clé simple (compatibilité legacy)
         *  - API hiérarchique : SetPath/GetPath avec chemins "parent.child.leaf"
         *  - Support objets imbriqués : SetObject/GetObject pour sous-archives
         *  - Support tableaux : SetArray/GetArray pour listes ordonnées
         *  - Méta-données : système "__meta__" style Unreal Engine assets
         *  - Fusion : Merge() pour combinaison d'archives avec contrôle d'écrasement
         *
         * Design decisions :
         *  - Stockage : NkVector<NkArchiveEntry> pour ordre d'insertion préservé
         *  - Recherche : linéaire O(n) optimisée pour petits dictionnaires (<100 entrées)
         *  - Extensibilité : méthodes virtuelles préparées pour héritage futur
         *
         * Thread-safety :
         *  - Non thread-safe par conception : synchronisation déléguée au caller
         *  - Copie profonde via constructeur copy pour isolation des données
         *
         * @note Export via NKENTSEU_SERIALIZATION_CLASS_EXPORT pour visibilité DLL.
         * @note Aucune macro d'export sur les méthodes : héritée de la déclaration de classe.
         *
         * @example Usage basique
         * @code
         * nkentseu::NkArchive config;
         * config.SetString("app.name", "MyApplication");
         * config.SetInt32("app.version", 1);
         * config.SetBool("debug.enabled", true);
         *
         * nkentseu::NkString appName;
         * if (config.GetString("app.name", appName)) {
         *     printf("App: %s\n", appName.CStr());
         * }
         * @endcode
         */
        class NKENTSEU_SERIALIZATION_CLASS_EXPORT NkArchive {


            // -----------------------------------------------------------------
            // SECTION 3 : MEMBRES PUBLICS
            // -----------------------------------------------------------------
        public:


            // -----------------------------------------------------------------
            // CONSTRUCTEURS ET DESTRUCTEUR
            // -----------------------------------------------------------------
            /**
             * @brief Constructeur par défaut : archive vide
             * @note noexcept : allocation différée via NkVector
             */
            NkArchive() noexcept = default;

            /**
             * @brief Constructeur de copie : duplication profonde
             * @note Défaut : copie membre-à-membre via NkVector::operator=
             */
            NkArchive(const NkArchive&) noexcept = default;

            /**
             * @brief Constructeur de move : transfert de propriété
             * @note Défaut : move sémantique via NkVector::operator=
             */
            NkArchive(NkArchive&&) noexcept = default;

            /**
             * @brief Opérateur d'affectation par copie
             * @note Défaut : copie profonde via NkVector
             */
            NkArchive& operator=(const NkArchive&) noexcept = default;

            /**
             * @brief Opérateur d'affectation par move
             * @note Défaut : transfert via NkVector
             */
            NkArchive& operator=(NkArchive&&) noexcept = default;

            /**
             * @brief Destructeur : libération automatique des ressources
             * @note Défaut : NkVector gère la destruction des NkArchiveEntry
             */
            ~NkArchive() noexcept = default;


            // -----------------------------------------------------------------
            // SETTERS SCALAIRES : API PLATE
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveSettersScalar Setters pour Valeurs Scalaires
             * @brief Méthodes d'écriture type-safe pour valeurs primitives
             *
             * Toutes ces méthodes :
             *  - Retournent nk_bool pour indication de succès/échec
             *  - Valident la clé via IsValidKey() avant traitement
             *  - Créent ou mettent à jour l'entrée correspondante dans mEntries
             *  - Sont noexcept : garanties de non-levée d'exception
             *
             * Pattern : SetValue() est la méthode centrale, les autres sont des wrappers.
             */

            /// @brief Définit une valeur null pour la clé spécifiée
            /// @param key Clé d'identification (doit être non-vide)
            /// @return true si l'opération a réussi, false si clé invalide
            nk_bool SetNull(NkStringView key) noexcept;

            /// @brief Définit une valeur booléenne pour la clé spécifiée
            /// @param key Clé d'identification
            /// @param v Valeur booléenne à stocker
            /// @return true si succès, false si clé invalide
            nk_bool SetBool(NkStringView key, nk_bool v) noexcept;

            /// @brief Définit une valeur entier signé 32 bits
            /// @param key Clé d'identification
            /// @param v Valeur int32 à stocker (convertie en int64 interne)
            /// @return true si succès, false si clé invalide
            nk_bool SetInt32(NkStringView key, nk_int32 v) noexcept;

            /// @brief Définit une valeur entier signé 64 bits
            /// @param key Clé d'identification
            /// @param v Valeur int64 à stocker
            /// @return true si succès, false si clé invalide
            nk_bool SetInt64(NkStringView key, nk_int64 v) noexcept;

            /// @brief Définit une valeur entier non-signé 32 bits
            /// @param key Clé d'identification
            /// @param v Valeur uint32 à stocker (convertie en uint64 interne)
            /// @return true si succès, false si clé invalide
            nk_bool SetUInt32(NkStringView key, nk_uint32 v) noexcept;

            /// @brief Définit une valeur entier non-signé 64 bits
            /// @param key Clé d'identification
            /// @param v Valeur uint64 à stocker
            /// @return true si succès, false si clé invalide
            nk_bool SetUInt64(NkStringView key, nk_uint64 v) noexcept;

            /// @brief Définit une valeur flottant 32 bits
            /// @param key Clé d'identification
            /// @param v Valeur float32 à stocker (convertie en float64 interne)
            /// @return true si succès, false si clé invalide
            nk_bool SetFloat32(NkStringView key, nk_float32 v) noexcept;

            /// @brief Définit une valeur flottant 64 bits
            /// @param key Clé d'identification
            /// @param v Valeur float64 à stocker
            /// @return true si succès, false si clé invalide
            nk_bool SetFloat64(NkStringView key, nk_float64 v) noexcept;

            /// @brief Définit une valeur chaîne de caractères
            /// @param key Clé d'identification
            /// @param v Vue de chaîne à copier (NkStringView pour efficacité)
            /// @return true si succès, false si clé invalide
            nk_bool SetString(NkStringView key, NkStringView v) noexcept;

            /// @brief Définit une valeur scalaire générique
            /// @param key Clé d'identification
            /// @param v Valeur NkArchiveValue pré-construite
            /// @return true si succès, false si clé invalide
            /// @note Méthode centrale : tous les autres Set* l'utilisent
            nk_bool SetValue(NkStringView key, const NkArchiveValue& v) noexcept;


            // -----------------------------------------------------------------
            // GETTERS SCALAIRES : API PLATE
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveGettersScalar Getters pour Valeurs Scalaires
             * @brief Méthodes de lecture type-safe avec coercition automatique
             *
             * Toutes ces méthodes :
             *  - Retournent nk_bool pour indication de présence/absence
             *  - Utilisent un paramètre de sortie `out` pour la valeur lue
             *  - Supportent la coercition entre types numériques compatibles
             *  - Tentent le parsing depuis le texte si le type ne matche pas exactement
             *
             * Coercition supportée :
             *  - int64 ↔ uint64 ↔ float64 (avec perte de précision pour float→int)
             *  - bool ↔ string ("true"/"false" case-insensitive)
             *  - Tout type numérique → string (via représentation textuelle canonique)
             */

            /// @brief Récupère une valeur scalaire générique
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la valeur trouvée
            /// @return true si la clé existe et contient un scalaire, false sinon
            [[nodiscard]] nk_bool GetValue(NkStringView key, NkArchiveValue& out) const noexcept;

            /// @brief Récupère une valeur booléenne avec coercition
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la valeur booléenne
            /// @return true si conversion réussie, false si clé absente ou type incompatible
            /// @note Tente le parsing depuis string si la valeur stockée n'est pas bool
            [[nodiscard]] nk_bool GetBool(NkStringView key, nk_bool& out) const noexcept;

            /// @brief Récupère un entier signé 32 bits avec coercition
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la valeur int32
            /// @return true si conversion réussie, false sinon
            /// @note Délègue à GetInt64() puis cast vers int32 avec vérification de bounds
            [[nodiscard]] nk_bool GetInt32(NkStringView key, nk_int32& out) const noexcept;

            /// @brief Récupère un entier signé 64 bits avec coercition
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la valeur int64
            /// @return true si conversion réussie, false sinon
            /// @note Supporte coercition depuis uint64, float64, ou parsing depuis string
            [[nodiscard]] nk_bool GetInt64(NkStringView key, nk_int64& out) const noexcept;

            /// @brief Récupère un entier non-signé 32 bits avec coercition
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la valeur uint32
            /// @return true si conversion réussie, false sinon
            [[nodiscard]] nk_bool GetUInt32(NkStringView key, nk_uint32& out) const noexcept;

            /// @brief Récupère un entier non-signé 64 bits avec coercition
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la valeur uint64
            /// @return true si conversion réussie, false sinon
            /// @note Supporte coercition depuis int64, float64, ou parsing depuis string
            [[nodiscard]] nk_bool GetUInt64(NkStringView key, nk_uint64& out) const noexcept;

            /// @brief Récupère un flottant 32 bits avec coercition
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la valeur float32
            /// @return true si conversion réussie, false sinon
            /// @note Délègue à GetFloat64() puis cast vers float32
            [[nodiscard]] nk_bool GetFloat32(NkStringView key, nk_float32& out) const noexcept;

            /// @brief Récupère un flottant 64 bits avec coercition
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la valeur float64
            /// @return true si conversion réussie, false sinon
            /// @note Supporte coercition depuis int64, uint64, ou parsing depuis string
            [[nodiscard]] nk_bool GetFloat64(NkStringView key, nk_float64& out) const noexcept;

            /// @brief Récupère une chaîne de caractères
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour la chaîne copiée
            /// @return true si la clé existe et contient un scalaire, false sinon
            /// @note Retourne toujours la représentation textuelle canonique
            [[nodiscard]] nk_bool GetString(NkStringView key, NkString& out) const noexcept;


            // -----------------------------------------------------------------
            // OBJETS IMBRIQUÉS : API HIÉRARCHIQUE
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveObjects Gestion des Objets Imbriqués
             * @brief Méthodes pour manipulation de sous-archives
             *
             * Pattern Composite : un NkArchive peut contenir d'autres NkArchive
             * via des nœuds de type NK_NODE_OBJECT.
             *
             * @note SetObject() effectue une copie profonde de l'archive source.
             * @note GetObject() effectue une copie vers l'archive de sortie.
             */

            /// @brief Définit un objet imbriqué pour la clé spécifiée
            /// @param key Clé d'identification
            /// @param arc Archive à copier comme sous-objet
            /// @return true si succès, false si clé invalide
            nk_bool SetObject(NkStringView key, const NkArchive& arc) noexcept;

            /// @brief Récupère un objet imbriqué pour la clé spécifiée
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour l'archive copiée
            /// @return true si la clé existe et contient un objet valide, false sinon
            [[nodiscard]] nk_bool GetObject(NkStringView key, NkArchive& out) const noexcept;


            // -----------------------------------------------------------------
            // TABLEAUX : SUPPORT DES LISTES ORDONNÉES
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveArrays Gestion des Tableaux
             * @brief Méthodes pour manipulation de listes de valeurs ou de nœuds
             *
             * Deux variantes :
             *  - SetArray/GetArray : pour tableaux de valeurs scalaires uniquement
             *  - SetNodeArray/GetNodeArray : pour tableaux de nœuds polymorphes
             *
             * @note Les tableaux préservent l'ordre d'insertion.
             * @note GetArray() filtre automatiquement les éléments non-scalaires.
             */

            /// @brief Définit un tableau de valeurs scalaires
            /// @param key Clé d'identification
            /// @param arr Vector de valeurs à copier dans le nouveau tableau
            /// @return true si succès, false si clé invalide
            nk_bool SetArray(NkStringView key, const NkVector<NkArchiveValue>& arr) noexcept;

            /// @brief Récupère un tableau de valeurs scalaires
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour le vector copié
            /// @return true si la clé existe et contient un tableau, false sinon
            /// @note Seuls les éléments scalaires sont copiés dans `out`
            [[nodiscard]] nk_bool GetArray(NkStringView key, NkVector<NkArchiveValue>& out) const noexcept;

            /// @brief Définit un tableau de nœuds polymorphes
            /// @param key Clé d'identification
            /// @param arr Vector de nœuds à copier (scalaires/objets/arrays)
            /// @return true si succès, false si clé invalide
            nk_bool SetNodeArray(NkStringView key, const NkVector<NkArchiveNode>& arr) noexcept;

            /// @brief Récupère un tableau de nœuds polymorphes
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour le vector copié
            /// @return true si la clé existe et contient un tableau, false sinon
            [[nodiscard]] nk_bool GetNodeArray(NkStringView key, NkVector<NkArchiveNode>& out) const noexcept;


            // -----------------------------------------------------------------
            // ACCÈS AUX NŒUDS BRUTS : API BAS NIVEAU
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveRawNodes Accès Direct aux Nœuds
             * @brief Méthodes pour manipulation avancée des NkArchiveNode
             *
             * Pour les cas nécessitant un contrôle fin du type de nœud :
             *  - SetNode() : insertion directe d'un nœud pré-construit
             *  - GetNode() : récupération par copie d'un nœud existant
             *  - FindNode() : accès par pointeur (const et non-const) sans copie
             *
             * @note FindNode() retourne nullptr si la clé n'existe pas.
             * @note Les pointeurs retournés sont invalidés par toute mutation de l'archive.
             */

            /// @brief Définit un nœud brut pour la clé spécifiée
            /// @param key Clé d'identification
            /// @param node Nœud à copier (scalaire/objet/array)
            /// @return true si succès, false si clé invalide
            nk_bool SetNode(NkStringView key, const NkArchiveNode& node) noexcept;

            /// @brief Récupère un nœud brut par copie
            /// @param key Clé à rechercher
            /// @param out Référence de sortie pour le nœud copié
            /// @return true si la clé existe, false sinon
            [[nodiscard]] nk_bool GetNode(NkStringView key, NkArchiveNode& out) const noexcept;

            /// @brief Recherche un nœud par clé (version const)
            /// @param key Clé à rechercher
            /// @return Pointeur vers le nœud trouvé, ou nullptr si absent
            /// @note Pointeur non-possédé : ne pas delete, valide jusqu'à prochaine mutation
            [[nodiscard]] const NkArchiveNode* FindNode(NkStringView key) const noexcept;

            /// @brief Recherche un nœud par clé (version mutable)
            /// @param key Clé à rechercher
            /// @return Pointeur mutable vers le nœud trouvé, ou nullptr si absent
            /// @note Permet la modification directe du nœud trouvé
            [[nodiscard]] NkArchiveNode* FindNode(NkStringView key) noexcept;


            // -----------------------------------------------------------------
            // ACCÈS PAR CHEMIN HIÉRARCHIQUE : STYLE "a.b.c"
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchivePathAccess Accès par Chemin Hiérarchique
             * @brief Méthodes pour navigation dans l'arbre via chemins dot-separated
             *
             * Syntaxe des chemins :
             *  - "key" : accès direct à une clé de premier niveau
             *  - "parent.child" : accès à `child` dans l'objet sous `parent`
             *  - "a.b.c.d" : navigation récursive arbitrairement profonde
             *
             * Comportement :
             *  - SetPath() : crée automatiquement les objets intermédiaires manquants
             *  - GetPath() : retourne nullptr si un maillon de la chaîne est absent
             *
             * @note Les chemins vides ou contenant des séparateurs consécutifs sont invalides.
             * @note Performance : O(depth × entries_per_level) pour recherche linéaire.
             *
             * @example
             * @code
             * // Définition hiérarchique
             * archive.SetPath("database.host", nkentseu::NkArchiveNode(
             *     nkentseu::NkArchiveValue::FromString("localhost")));
             * archive.SetPath("database.port", nkentseu::NkArchiveNode(
             *     nkentseu::NkArchiveValue::FromInt32(5432)));
             *
             * // Lecture hiérarchique
             * const auto* hostNode = archive.GetPath("database.host");
             * if (hostNode && hostNode->IsScalar()) {
             *     printf("Host: %s\n", hostNode->value.text.CStr());
             * }
             * @endcode
             */

            /// @brief Définit un nœud via un chemin hiérarchique
            /// @param path Chemin dot-separated vers la cible (ex: "a.b.c")
            /// @param node Nœud à placer à l'emplacement spécifié
            /// @return true si succès, false si chemin invalide ou création échouée
            nk_bool SetPath(NkStringView path, const NkArchiveNode& node) noexcept;

            /// @brief Récupère un nœud via un chemin hiérarchique
            /// @param path Chemin dot-separated vers la cible
            /// @return Pointeur vers le nœud trouvé, ou nullptr si chemin invalide
            /// @note Pointeur non-possédé : valide jusqu'à prochaine mutation de l'archive
            [[nodiscard]] const NkArchiveNode* GetPath(NkStringView path) const noexcept;


            // -----------------------------------------------------------------
            // MÉTA-DONNÉES : SYSTÈME STYLE UNREAL ENGINE
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveMetadata Gestion des Méta-données
             * @brief API pour stockage de métadonnées d'asset via clé "__meta__"
             *
             * Convention :
             *  - Toutes les méta-données sont stockées sous l'objet "__meta__"
             *  - Accès via SetMeta/GetMeta : abstraction du chemin interne
             *  - Valeurs toujours de type string pour flexibilité de parsing
             *
             * Cas d'usage :
             *  - UUID d'asset, timestamps, dépendances, flags de build
             *  - Informations non-sérialisées dans le contenu principal
             *
             * @example
             * @code
             * archive.SetMeta("uuid", "550e8400-e29b-41d4-a716-446655440000");
             * archive.SetMeta("imported_by", "AssetPipeline v2.1");
             *
             * nkentseu::NkString uuid;
             * if (archive.GetMeta("uuid", uuid)) {
             *     // Traitement de l'UUID...
             * }
             * @endcode
             */

            /// @brief Définit une méta-donnée string sous la clé "__meta__.metaKey"
            /// @param metaKey Nom de la méta-donnée (sans préfixe)
            /// @param value Valeur string à stocker
            /// @return true si succès, false si échec de SetPath interne
            nk_bool SetMeta(NkStringView metaKey, NkStringView value) noexcept;

            /// @brief Récupère une méta-donnée depuis "__meta__.metaKey"
            /// @param metaKey Nom de la méta-donnée à récupérer
            /// @param out Référence de sortie pour la valeur string
            /// @return true si la méta-donnée existe et est un scalaire string, false sinon
            [[nodiscard]] nk_bool GetMeta(NkStringView metaKey, NkString& out) const noexcept;


            // -----------------------------------------------------------------
            // MÉTHODES DE GESTION GÉNÉRALE
            // -----------------------------------------------------------------
            /**
             * @defgroup ArchiveManagement Gestion de l'Archive
             * @brief Méthodes utilitaires pour interrogation et mutation globale
             */

            /// @brief Vérifie si une clé existe dans l'archive
            /// @param key Clé à rechercher
            /// @return true si la clé est présente, false sinon
            /// @note Recherche linéaire O(n) : optimisée pour petits dictionnaires
            [[nodiscard]] nk_bool Has(NkStringView key) const noexcept;

            /// @brief Supprime une entrée par clé
            /// @param key Clé à supprimer
            /// @return true si la clé existait et a été supprimée, false sinon
            /// @note Libération automatique des ressources du nœud supprimé
            nk_bool Remove(NkStringView key) noexcept;

            /// @brief Vide complètement l'archive
            /// @post mEntries vidé, toutes les ressources libérées
            /// @note Méthode noexcept : garantie de non-levée d'exception
            void Clear() noexcept;

            /// @brief Fusionne une autre archive dans celle-ci
            /// @param other Archive source à fusionner
            /// @param overwrite Si true, écrase les clés existantes ; sinon, les préserve
            /// @note Itération sur mEntries de `other` : O(m × n) worst-case
            void Merge(const NkArchive& other, bool overwrite = true) noexcept;

            /// @brief Obtient le nombre d'entrées dans l'archive
            /// @return Nombre d'éléments dans mEntries
            [[nodiscard]] nk_size Size() const noexcept { return mEntries.Size(); }

            /// @brief Vérifie si l'archive est vide
            /// @return true si Size() == 0, false sinon
            [[nodiscard]] nk_bool Empty() const noexcept { return mEntries.Empty(); }

            /// @brief Accès en lecture aux entrées internes
            /// @return Référence const vers le vector d'entrées
            /// @note Pour itération : ne pas modifier pendant l'itération
            [[nodiscard]] const NkVector<NkArchiveEntry>& Entries() const noexcept { return mEntries; }

            /// @brief Accès en écriture aux entrées internes
            /// @return Référence mutable vers le vector d'entrées
            /// @note Attention : toute mutation peut invalider les pointeurs FindNode()
            [[nodiscard]] NkVector<NkArchiveEntry>& Entries() noexcept { return mEntries; }


            // -----------------------------------------------------------------
            // SECTION 4 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
            // -----------------------------------------------------------------
        private:


            // -----------------------------------------------------------------
            // MÉTHODES PRIVÉES D'IMPLÉMENTATION
            // -----------------------------------------------------------------
            /**
             * @brief Recherche linéaire d'une clé dans mEntries
             * @param key Clé à rechercher (comparaison via NkString::Compare)
             * @return Index de l'entrée trouvée, ou NPOS si absente
             * @note Complexité O(n) : acceptable pour n < 100 ; envisager hashmap pour plus
             */
            [[nodiscard]] nk_size FindIndex(NkStringView key) const noexcept;

            /**
             * @brief Valide qu'une clé est utilisable pour l'archive
             * @param key Clé à valider
             * @return true si key non-vide et ne contient pas de caractères interdits
             * @note Règles de validation : !Empty(), pas de '.' en première position pour API plate
             */
            [[nodiscard]] static bool IsValidKey(NkStringView key) noexcept;


            // -----------------------------------------------------------------
            // VARIABLES MEMBRES PRIVÉES
            // -----------------------------------------------------------------
            /**
             * @brief Conteneur principal des entrées clé/valeur
             * @note NkVector préserve l'ordre d'insertion pour itération déterministe
             * @note Recherche linéaire : optimisée pour petits dictionnaires (<100 entrées)
             */
            NkVector<NkArchiveEntry> mEntries;

            /**
             * @brief Constante représentant "non trouvé" pour FindIndex()
             * @note Valeur : max(nk_size) pour distinction claire des indices valides [0, Size()-1]
             */
            static constexpr nk_size NPOS = static_cast<nk_size>(-1);


        }; // class NkArchive


    } // namespace nkentseu


#endif // NKENTSEU_SERIALIZATION_NKARCHIVE_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKARCHIVE.H
// =============================================================================
// Ce fichier fournit l'archive centrale pour sérialisation multi-format.
// Il combine API plate (clés simples) et API hiérarchique (chemins "a.b.c").
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration d'application simple (API plate)
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkArchive.h>

    void SaveAppConfig() {
        nkentseu::NkArchive config;

        // Valeurs scalaires basiques
        config.SetString("app.name", "MyApplication");
        config.SetInt32("app.version.major", 2);
        config.SetInt32("app.version.minor", 1);
        config.SetBool("app.debug_mode", true);
        config.SetFloat64("app.timeout_seconds", 30.5);

        // Sauvegarde via un sérialiseur (ex: JSON)
        // nkentseu::NkJsonSerializer::SaveToFile(config, "config.json");
    }
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Structure hiérarchique avec SetPath/GetPath
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkArchive.h>

    void SetupDatabaseConfig() {
        nkentseu::NkArchive dbConfig;

        // Chemins hiérarchiques pour organisation logique
        dbConfig.SetPath("database.primary.host",
            nkentseu::NkArchiveNode(nkentseu::NkArchiveValue::FromString("db1.example.com")));
        dbConfig.SetPath("database.primary.port",
            nkentseu::NkArchiveNode(nkentseu::NkArchiveValue::FromInt32(5432)));
        dbConfig.SetPath("database.replica.host",
            nkentseu::NkArchiveNode(nkentseu::NkArchiveValue::FromString("db2.example.com")));

        // Lecture avec fallback
        nkentseu::NkString primaryHost;
        if (!dbConfig.GetString("database.primary.host", primaryHost)) {
            primaryHost = "localhost";  // Valeur par défaut
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Manipulation de tableaux de valeurs
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkArchive.h>
    #include <NKContainers/Sequential/NkVector.h>

    void StoreTagList() {
        nkentseu::NkArchive metadata;

        // Création d'un tableau de strings
        nkentseu::NkVector<nkentseu::NkArchiveValue> tags;
        tags.PushBack(nkentseu::NkArchiveValue::FromString("production"));
        tags.PushBack(nkentseu::NkArchiveValue::FromString("web"));
        tags.PushBack(nkentseu::NkArchiveValue::FromString("api"));

        metadata.SetArray("asset.tags", tags);

        // Lecture et itération
        nkentseu::NkVector<nkentseu::NkArchiveValue> retrievedTags;
        if (metadata.GetArray("asset.tags", retrievedTags)) {
            for (nk_size i = 0; i < retrievedTags.Size(); ++i) {
                printf("Tag %zu: %s\n", i, retrievedTags[i].text.CStr());
            }
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Objet imbriqué avec GetObject/SetObject
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkArchive.h>

    void NestedObjectExample() {
        nkentseu::NkArchive root;

        // Création d'un sous-objet "user"
        nkentseu::NkArchive user;
        user.SetString("name", "Alice");
        user.SetInt32("age", 30);
        user.SetBool("active", true);

        // Insertion dans l'archive parente
        root.SetObject("user", user);

        // Extraction et modification
        nkentseu::NkArchive extractedUser;
        if (root.GetObject("user", extractedUser)) {
            extractedUser.SetString("email", "alice@example.com");
            root.SetObject("user", extractedUser);  // Mise à jour
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Méta-données style Unreal Engine
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkArchive.h>

    void AssetMetadataExample() {
        nkentseu::NkArchive asset;

        // Contenu principal de l'asset
        asset.SetString("mesh.path", "models/character.fbx");
        asset.SetInt32("mesh.lod_count", 4);

        // Méta-données (stockées sous "__meta__")
        asset.SetMeta("uuid", "550e8400-e29b-41d4-a716-446655440000");
        asset.SetMeta("import_timestamp", "2024-01-15T10:30:00Z");
        asset.SetMeta("importer_version", "Pipeline/2.1.0");

        // Lecture des méta-données
        nkentseu::NkString uuid;
        if (asset.GetMeta("uuid", uuid)) {
            // Vérification d'intégrité ou cache lookup...
        }
    }
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Fusion d'archives avec contrôle d'écrasement
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkArchive.h>

    void MergeConfigsExample() {
        nkentseu::NkArchive baseConfig;
        baseConfig.SetString("log.level", "info");
        baseConfig.SetBool("log.file_output", true);

        nkentseu::NkArchive overrideConfig;
        overrideConfig.SetString("log.level", "debug");  // Valeur à écraser
        overrideConfig.SetString("log.custom_path", "/var/log/app/");  // Nouvelle clé

        // Fusion avec écrasement des clés existantes
        baseConfig.Merge(overrideConfig, true);

        // Résultat : log.level = "debug", log.custom_path ajoutée
        nkentseu::NkString level;
        baseConfig.GetString("log.level", level);  // "debug"
    }
*/


// -----------------------------------------------------------------------------
// Exemple 7 : Coercition de types dans les getters
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkArchive.h>

    void TypeCoercionExample() {
        nkentseu::NkArchive config;

        // Stockage en int64
        config.SetInt64("server.port", 8080);

        // Lecture avec coercition vers différents types
        nk_int32 port32;
        nk_uint64 portU64;
        nk_float64 portFloat;
        nkentseu::NkString portStr;

        config.GetInt32("server.port", port32);    // 8080 (cast safe)
        config.GetUInt64("server.port", portU64);  // 8080 (conversion sign→unsigned)
        config.GetFloat64("server.port", portFloat); // 8080.0 (int→float)
        config.GetString("server.port", portStr);  // "8080" (via text canonique)

        // Coercition depuis string (parsing)
        config.SetString("timeout", "45.5");
        nk_float64 timeout;
        config.GetFloat64("timeout", timeout);  // 45.5 via parsing du texte
    }
*/


// -----------------------------------------------------------------------------
// Exemple 8 : Navigation hiérarchique avancée avec FindNode
// -----------------------------------------------------------------------------
/*
    #include <NKSerialization/NkArchive.h>

    void AdvancedNavigationExample() {
        nkentseu::NkArchive doc;

        // Construction d'une structure complexe
        doc.SetPath("users.alice.roles",
            nkentseu::NkArchiveNode(nkentseu::NkArchiveValue::FromString("admin")));
        doc.SetPath("users.bob.roles",
            nkentseu::NkArchiveNode(nkentseu::NkArchiveValue::FromString("user")));

        // Accès direct via FindNode pour performance (pas de copie)
        const auto* aliceRoles = doc.FindNode("users.alice.roles");
        if (aliceRoles && aliceRoles->IsScalar()) {
            printf("Alice role: %s\n", aliceRoles->value.text.CStr());
        }

        // Attention : les pointeurs FindNode sont invalidés par toute mutation
        // Ne pas stocker ces pointeurs au-delà de la portée d'utilisation immédiate.
    }
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
    1. CHOIX API PLATE vs HIÉRARCHIQUE :
       - API plate (SetString/GetString) : pour configurations simples, clés uniques
       - API hiérarchique (SetPath/GetPath) : pour structures imbriquées style JSON
       - Éviter de mélanger les deux styles sur les mêmes clés pour éviter les conflits

    2. PERFORMANCE DE RECHERCHE :
       - FindIndex() utilise une recherche linéaire O(n)
       - Optimisé pour n < 100 : cas typique des configurations
       - Pour grands dictionnaires : envisager une version avec hashmap interne

    3. GESTION DE LA MÉMOIRE DANS NkArchiveNode :
       - object est un owning raw pointer : toujours libéré dans FreeObject()
       - Ne jamais delete manuellement un pointeur obtenu via FindNode()
       - Les copies/moves gèrent automatiquement la profondeur via CloneNode()

    4. COERCITION DE TYPES DANS LES GETTERS :
       - GetInt64() accepte uint64, float64, ou string parseable
       - La coercition float→int tronque (pas d'arrondi) : documenter ce comportement
       - En cas d'échec de parsing, retourner false sans modifier `out`

    5. THREAD-SAFETY :
       - NkArchive n'est PAS thread-safe par conception
       - Synchronisation déléguée au caller via mutex externe si nécessaire
       - Les copies sont profondes : isolation garantie entre threads après copie

    6. EXTENSIBILITÉ FUTURES :
       - Ajouter un type NK_VALUE_BINARY pour données blob (base64 en texte)
       - Support des tableaux associatifs (dictionnaires dans tableaux)
       - Méthodes de validation de schéma via NkSchema externe

    7. SÉRIALISATION/DÉSÉRIALISATION :
       - Les sérialiseurs (JSON, YAML, etc.) doivent respecter :
         * Représentation textuelle canonique pour round-trip fidelity
         * Gestion des types via discriminant NkArchiveValueType
         * Préservation de l'ordre d'insertion pour déterminisme
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================