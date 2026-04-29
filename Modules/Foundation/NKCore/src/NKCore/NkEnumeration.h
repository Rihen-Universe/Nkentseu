// =============================================================================
// NKCore/NkEnumeration.h
// Wrapper type-safe pour énumérations avec conversion string et opérations bitwise.
//
// Design :
//  - Classe template NkEnumeration<EnumType, BaseType> pour encapsulation type-safe
//  - Opérateurs bitwise (|, &, |=, &=) pour enums de type flags
//  - Conversions implicites vers BaseType et EnumType sous-jacents
//  - Méthode ToString() personnalisable via macros pour débogage
//  - Méthode HasFlag() pour test de présence de flag dans enum combiné
//  - Macros utilitaires : NK_ENUM_TO_STRING_* pour génération de ToString()
//  - Macro NkEnumeration() pour définition concise d'enum avec helpers
//  - Compatibilité avec NKENTSEU_API pour export de symboles DLL/shared
//
// Intégration :
//  - Utilise <type_traits>, <string>, <sstream> pour métaprogrammation et conversion
//  - Compatible C++11+ avec support constexpr partiel
//  - Peut être étendu avec NkTraits.h pour SFINAE/enable_if avancés
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKENUMERATION_H_INCLUDED
#define NKENTSEU_CORE_NKENUMERATION_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des en-têtes standards nécessaires.

    #include <string>
    #include <sstream>
    #include <stdexcept>
    #include <type_traits>
    #include <cstdint>

    // Inclusion optionnelle des traits NKCore si disponibles pour NkEnableIf_t, etc.
    // #include "NKCore/NkTraits.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Déclaration de l'espace de noms principal nkentseu.

    namespace nkentseu {

        // ====================================================================
        // SECTION 3 : CLASSE TEMPLATE NKENUMERATION (BASE)
        // ====================================================================
        // Wrapper générique pour énumérations avec opérations type-safe.

        /**
         * @defgroup EnumerationTemplates Templates d'Énumérations Type-Safe
         * @brief Classes et utilitaires pour manipulation sécurisée d'énumérations
         * @ingroup CoreUtilities
         *
         * Ce système fournit un wrapper autour des enum C++ pour :
         *   - Éviter les conversions implicites non désirées
         *   - Supporter les opérations bitwise pour les enums de type flags
         *   - Faciliter la conversion vers/depuis string pour logging/debug
         *   - Permettre l'extension via héritage ou spécialisation
         *
         * @note
         *   - BaseType par défaut : int (peut être uint8_t, uint32_t, etc.)
         *   - Les opérateurs bitwise sont commentés par défaut : les activer si besoin
         *   - ToString() est virtuel pour personnalisation dans les classes dérivées
         *
         * @example
         * @code
         * // Définition d'une enum simple
         * enum class Color : uint8_t { Red = 1, Green = 2, Blue = 4 };
         *
         * // Wrapper avec NkEnumeration
         * using ColorEnum = nkentseu::NkEnumeration<Color, uint8_t>;
         *
         * ColorEnum c1(Color::Red);
         * ColorEnum c2 = Color::Green;  // Conversion implicite depuis Enum
         *
         * // Conversion vers type sous-jacent
         * uint8_t raw = static_cast<uint8_t>(c1);  // ou c1.operator uint8_t()
         *
         * // Test de flag (si enum utilisée comme bitmask)
         * ColorEnum flags = Color::Red | Color::Blue;  // Nécessite opérateur| activé
         * if (flags.HasFlag(Color::Red)) { /* ... *\/ }
         * @endcode
         */
        /** @{ */

        /**
         * @brief Wrapper type-safe pour énumérations avec opérations optionnelles
         * @tparam EnumType Type de l'énumération C++ (enum class)
         * @tparam BaseType Type entier sous-jacent pour le stockage (défaut: int)
         * @ingroup EnumerationTemplates
         *
         * Cette classe encapsule une valeur d'énumération pour prévenir les
         * conversions accidentelles tout en permettant un accès contrôlé
         * aux opérations bitwise et à la sérialisation string.
         *
         * @note
         *   - Les opérateurs bitwise sont commentés par défaut pour éviter
         *     leur usage accidentel sur des enums non-flags. Décommentez-les
         *     dans votre spécialisation si vous avez besoin de combinaisons.
         *   - ToString() retourne une chaîne vide par défaut : surchargez-la
         *     ou utilisez les macros NK_ENUM_TO_STRING_* pour génération auto.
         *
         * @warning
         *   L'opérateur de conversion vers EnumType peut contourner la sécurité
         *   du wrapper. Préférer l'accès explicite via .value ou static_cast.
         */
        template <typename EnumType, typename BaseType = int>
        class NkEnumeration {
            public:
                // --------------------------------------------------------
                // Types et aliases
                // --------------------------------------------------------

                /** @brief Alias vers le type entier sous-jacent */
                using BASE_TYPE = BaseType;

                // --------------------------------------------------------
                // Constructeurs
                // --------------------------------------------------------

                /**
                 * @brief Constructeur par défaut (valeur initialisée à 0)
                 */
                NkEnumeration() noexcept
                    : value(0)
                {
                }

                /**
                 * @brief Constructeur depuis une valeur de l'enum
                 * @param e Valeur de l'énumération à encapsuler
                 */
                NkEnumeration(EnumType e) noexcept
                    : value(static_cast<BASE_TYPE>(e))
                {
                }

                /**
                 * @brief Constructeur depuis une valeur du type de base
                 * @param v Valeur entière à encapsuler
                 * @note Permet la création de combinaisons de flags non définies dans l'enum
                 */
                NkEnumeration(BASE_TYPE v) noexcept
                    : value(v)
                {
                }

                // --------------------------------------------------------
                // Opérateurs de conversion (implicites)
                // --------------------------------------------------------

                /**
                 * @brief Conversion implicite vers le type de base
                 * @return Valeur entière stockée
                 * @note Permet l'usage dans des expressions arithmétiques
                 */
                operator BASE_TYPE() const noexcept {
                    return value;
                }

                /**
                 * @brief Conversion implicite vers le type enum
                 * @return Valeur convertie en EnumType
                 * @warning Peut masquer des erreurs de type : utiliser avec précaution
                 */
                operator EnumType() const noexcept {
                    return static_cast<EnumType>(value);
                }

                // --------------------------------------------------------
                // Méthodes utilitaires
                // --------------------------------------------------------

                /**
                 * @brief Teste si un flag spécifique est présent dans la valeur
                 * @param e Flag à tester (valeur de l'enum)
                 * @return true si le bit correspondant est actif, false sinon
                 * @note Fonctionne uniquement si l'enum est utilisée comme bitmask
                 *
                 * @example
                 * @code
                 * NkEnumeration<Permissions, uint8_t> perms(Permissions::Read | Permissions::Write);
                 * if (perms.HasFlag(Permissions::Read)) { /* Accès autorisé *\/ }
                 * @endcode
                 */
                bool HasFlag(EnumType e) const noexcept {
                    return (value & static_cast<BASE_TYPE>(e)) != 0;
                }

                /**
                 * @brief Convertit la valeur en représentation string pour débogage
                 * @return Chaîne représentant la valeur (vide par défaut)
                 * @note Méthode virtuelle : à surcharger ou utiliser les macros NK_ENUM_TO_STRING_*
                 *
                 * @example de surcharge manuelle :
                 * @code
                 * std::string ToString() const override {
                 *     switch (static_cast<EnumType>(value)) {
                 *         case EnumType::ValueA: return "ValueA";
                 *         case EnumType::ValueB: return "ValueB";
                 *         default: return "Unknown(" + std::to_string(value) + ")";
                 *     }
                 * }
                 * @endcode
                 */
                virtual std::string ToString() const {
                    return "";
                }

                // --------------------------------------------------------
                // Opérateurs de comparaison
                // --------------------------------------------------------

                /** @brief Égalité avec un autre NkEnumeration */
                bool operator==(const NkEnumeration& other) const noexcept {
                    return value == other.value;
                }

                /** @brief Inégalité avec un autre NkEnumeration */
                bool operator!=(const NkEnumeration& other) const noexcept {
                    return value != other.value;
                }

                /** @brief Égalité avec une valeur de l'enum */
                bool operator==(const EnumType& other) const noexcept {
                    return value == static_cast<BASE_TYPE>(other);
                }

                /** @brief Inégalité avec une valeur de l'enum */
                bool operator!=(const EnumType& other) const noexcept {
                    return value != static_cast<BASE_TYPE>(other);
                }

                // --------------------------------------------------------
                // Opérateurs bitwise (optionnels - décommenter si besoin)
                // --------------------------------------------------------
                // Ces opérateurs sont commentés par défaut car ils ne sont
                // pertinents que pour les enums utilisées comme bitmask/flags.
                // Décommentez les blocs nécessaires dans votre code.

                /*
                // OR avec valeur enum
                NkEnumeration operator|(EnumType e) const noexcept {
                    return NkEnumeration(value | static_cast<BASE_TYPE>(e));
                }

                // AND avec valeur enum
                NkEnumeration operator&(EnumType e) const noexcept {
                    return NkEnumeration(value & static_cast<BASE_TYPE>(e));
                }

                // OR-assign avec valeur enum
                NkEnumeration& operator|=(EnumType e) noexcept {
                    value |= static_cast<BASE_TYPE>(e);
                    return *this;
                }

                // AND-assign avec valeur enum
                NkEnumeration& operator&=(EnumType e) noexcept {
                    value &= static_cast<BASE_TYPE>(e);
                    return *this;
                }

                // OR avec autre NkEnumeration
                NkEnumeration operator|(const NkEnumeration& other) const noexcept {
                    return NkEnumeration(value | other.value);
                }

                // AND avec autre NkEnumeration
                NkEnumeration operator&(const NkEnumeration& other) const noexcept {
                    return NkEnumeration(value & other.value);
                }
                */

                // --------------------------------------------------------
                // Streaming operator pour débogage
                // --------------------------------------------------------

                /**
                 * @brief Opérateur de flux pour affichage facile
                 * @param os Flux de sortie (std::ostream)
                 * @param e Instance à afficher
                 * @return Référence vers le flux pour chaînage
                 * @note Utilise ToString() pour la représentation string
                 *
                 * @example
                 * @code
                 * ColorEnum c(Color::Red);
                 * std::cout << "Color: " << c << std::endl;  // Affiche via ToString()
                 * @endcode
                 */
                friend std::ostream& operator<<(std::ostream& os, const NkEnumeration& e) {
                    return os << e.ToString();
                }

            protected:
                // --------------------------------------------------------
                // Membre de données
                // --------------------------------------------------------
                /** @brief Valeur entière stockée (représentation brute de l'enum) */
                BASE_TYPE value;
        };

        /** @} */ // End of EnumerationTemplates

        // ====================================================================
        // SECTION 4 : MACROS POUR GÉNÉRATION DE ToString()
        // ====================================================================
        // Macros utilitaires pour implémentation automatique de ToString().

        /**
         * @defgroup EnumToStringMacros Macros de Conversion Enum → String
         * @brief Macros pour génération automatique de ToString() dans les dérivés
         * @ingroup EnumerationUtilities
         *
         * Ces macros simplifient l'implémentation de ToString() pour les classes
         * dérivées de NkEnumeration. Elles génèrent du code de comparaison
         * valeur-par-valeur ou flag-par-flag selon le cas d'usage.
         *
         * @note
         *   - NK_ENUM_TO_STRING_ADD_CONTENT : pour enums de type flags (OR multiple)
         *   - NK_ENUM_TO_STRING_SET_CONTENT : pour enums simples (valeur unique)
         *   - Les macros génèrent du code dans le scope de ToString(), à utiliser
         *     entre NK_ENUM_TO_STRING_BEGIN et NK_ENUM_TO_STRING_END
         *
         * @warning
         *   Ces macros utilisent le préprocesseur : les noms de valeurs doivent
         *   être des tokens valides. Les espaces ou caractères spéciaux dans les
         *   noms d'enum causeront des erreurs de compilation.
         *
         * @example
         * @code
         * class StatusEnum : public NkEnumeration<Status, uint8_t> {
         * public:
         *     using NkEnumeration::NkEnumeration;  // Hériter des constructeurs
         *
         *     std::string ToString() const override {
         *         NK_ENUM_TO_STRING_BEGIN
         *         NK_ENUM_TO_STRING_SET_CONTENT(Status::Ok)
         *         NK_ENUM_TO_STRING_SET_CONTENT(Status::Error)
         *         NK_ENUM_TO_STRING_SET_CONTENT(Status::Pending)
         *         NK_ENUM_TO_STRING_END(Unknown)
         *     }
         * };
         * @endcode
         */
        /** @{ */

        /**
         * @brief Début du bloc de génération ToString()
         * @def NK_ENUM_TO_STRING_BEGIN
         * @ingroup EnumToStringMacros
         * @note Initialise une chaîne locale 'str' vide pour accumulation
         */
        #define NK_ENUM_TO_STRING_BEGIN \
            virtual std::string ToString() const override { \
                std::string str = "";

        /**
         * @brief Ajoute le nom d'une valeur si le flag correspondant est actif
         * @def NK_ENUM_TO_STRING_ADD_CONTENT(value_e)
         * @param value_e Nom de la valeur de l'enum à tester
         * @ingroup EnumToStringMacros
         * @note Utilise l'opérateur bitwise & : adapté aux enums de type flags
         * @warning value_e doit être accessible dans le scope (Enum::value_e ou valeur globale)
         */
        #define NK_ENUM_TO_STRING_ADD_CONTENT(value_e) \
            if (value & static_cast<BASE_TYPE>(value_e)) { str += #value_e; }

        /**
         * @brief Remplace le contenu si la valeur correspond exactement
         * @def NK_ENUM_TO_STRING_SET_CONTENT(value_e)
         * @param value_e Nom de la valeur de l'enum à comparer
         * @ingroup EnumToStringMacros
         * @note Utilise l'opérateur == : adapté aux enums simples (non-flags)
         */
        #define NK_ENUM_TO_STRING_SET_CONTENT(value_e) \
            if (value == static_cast<BASE_TYPE>(value_e)) { str = #value_e; }

        /**
         * @brief Variante avec préfixe Enum:: pour les enum class scoped
         * @def NK_ENUM_TO_STRING_ADD_CONTENT2(value_e)
         * @param value_e Nom court de la valeur (sans préfixe Enum::)
         * @ingroup EnumToStringMacros
         * @note Concatène automatiquement Enum::##value_e via token pasting
         * @warning Requiert que 'Enum' soit un alias défini dans la classe dérivée
         */
        #define NK_ENUM_TO_STRING_ADD_CONTENT2(value_e) \
            if (value & static_cast<BASE_TYPE>(Enum::value_e)) { str += #value_e; }

        /**
         * @brief Variante SET avec préfixe Enum:: pour les enum class scoped
         * @def NK_ENUM_TO_STRING_SET_CONTENT2(value_e)
         * @param value_e Nom court de la valeur (sans préfixe Enum::)
         * @ingroup EnumToStringMacros
         */
        #define NK_ENUM_TO_STRING_SET_CONTENT2(value_e) \
            if (value == static_cast<BASE_TYPE>(Enum::value_e)) { str = #value_e; }

        /**
         * @brief Finalise le bloc ToString() avec fallback pour valeur inconnue
         * @def NK_ENUM_TO_STRING_END(not_value)
         * @param not_value Nom à retourner si aucune correspondance n'est trouvée
         * @ingroup EnumToStringMacros
         * @note Retourne la chaîne accumulée, ou le fallback si vide
         */
        #define NK_ENUM_TO_STRING_END(not_value) \
            return str.empty() ? #not_value : str; \
        }

        /** @} */ // End of EnumToStringMacros

        // ====================================================================
        // SECTION 5 : MACROS POUR GÉNÉRATION DE FromString() [EXPÉRIMENTAL]
        // ====================================================================
        // Macros pour conversion inverse String → Enum (à stabiliser).

        /**
         * @defgroup EnumFromStringMacros Macros de Conversion String → Enum
         * @brief Macros expérimentales pour génération de FromString()
         * @ingroup EnumerationUtilities
         * @deprecated Fonctionnalité en développement : API susceptible de changer
         *
         * Ces macros permettent de générer une méthode FromString() pour parser
         * une chaîne et reconstruire une valeur d'enum. Utile pour la configuration
         * via fichiers texte ou arguments en ligne de commande.
         *
         * @warning
         *   - L'implémentation actuelle utilise std::string::find() qui peut
         *     matcher des sous-chaînes partielles (ex: "Read" matche "ReadOnly")
         *   - Préférer une comparaison exacte (str == "Value") pour plus de sécurité
         *   - Cette fonctionnalité est expérimentale et peut évoluer
         *
         * @example
         * @code
         * class PermissionEnum : public NkEnumeration<Permission, uint8_t> {
         * public:
         *     using NkEnumeration::NkEnumeration;
         *
         *     static PermissionEnum FromString(const std::string& input) {
         *         PermissionEnum result;
         *         NK_STRING_TO_ENUM_BEGIN()
         *         NK_STRING_TO_ENUM_SET_CONTENT(Permission::Read)
         *         NK_STRING_TO_ENUM_SET_CONTENT(Permission::Write)
         *         NK_STRING_TO_ENUM_END(Permission::None)
         *     }
         * };
         * @endcode
         */
        /** @{ */

        /**
         * @brief Début du bloc de génération FromString()
         * @def NK_STRING_TO_ENUM_BEGIN
         * @ingroup EnumFromStringMacros
         * @note Déclare une variable locale 'e' de type enum_name à retourner
         * @deprecated API expérimentale : signature susceptible de changer
         */
        #define NK_STRING_TO_ENUM_BEGIN(enum_name) \
            static enum_name FromString(const std::string& str) { \
                enum_name e;

        /**
         * @brief Ajoute un flag si la sous-chaîne est trouvée dans l'input
         * @def NK_STRING_TO_ENUM_ADD_CONTENT(value_e)
         * @param value_e Nom de la valeur à matcher
         * @ingroup EnumFromStringMacros
         * @note Utilise std::string::find() : peut matcher partiellement
         * @deprecated Préférer NK_STRING_TO_ENUM_SET_CONTENT pour matching exact
         */
        #define NK_STRING_TO_ENUM_ADD_CONTENT(value_e) \
            if (str.find(#value_e) != std::string::npos) { \
                e.value |= static_cast<BASE_TYPE>(value_e); \
            }

        /**
         * @brief Assigne la valeur si la chaîne correspond exactement
         * @def NK_STRING_TO_ENUM_SET_CONTENT(value_e)
         * @param value_e Nom de la valeur à matcher exactement
         * @ingroup EnumFromStringMacros
         * @note Utilise l'opérateur == pour comparaison exacte (recommandé)
         */
        #define NK_STRING_TO_ENUM_SET_CONTENT(value_e) \
            if (str == #value_e) { \
                e.value = static_cast<BASE_TYPE>(value_e); \
            }

        /**
         * @brief Finalise le bloc FromString() avec fallback
         * @def NK_STRING_TO_ENUM_END(fallback_value)
         * @param fallback_value Valeur à retourner si aucun match n'est trouvé
         * @ingroup EnumFromStringMacros
         * @deprecated API expérimentale
         */
        #define NK_STRING_TO_ENUM_END(fallback_value) \
            if (str.empty()) { e.value = static_cast<BASE_TYPE>(fallback_value); } \
            return e; \
        }

        /** @} */ // End of EnumFromStringMacros

        // ====================================================================
        // SECTION 6 : MACRO DE DÉFINITION RAPIDE D'ENUM
        // ====================================================================
        // Macro générant enum class + wrapper + opérateurs en une déclaration.

        /**
         * @defgroup EnumDefinitionMacro Macro de Définition Concise d'Énumération
         * @brief Macro générant enum class et wrapper NkEnumeration en une ligne
         * @ingroup EnumerationUtilities
         *
         * Cette macro simplifie la définition d'énumérations avec support
         * immédiat des conversions, comparaisons et ToString() optionnel.
         *
         * @note
         *   - Génère : enum class Enum<name> + class <name> wrapper
         *   - Inclut : constructeurs, opérateurs de comparaison, conversions
         *   - Optionnel : ToString() via paramètre 'tostring', méthodes custom via 'methods'
         *   - NKENTSEU_API : pour export DLL/shared (défini dans NkExport.h)
         *
         * @warning
         *   - La macro est complexe : débogage difficile en cas d'erreur
         *   - Les tokens ## dans les macros internes peuvent causer des problèmes
         *     si les noms de valeurs contiennent des caractères spéciaux
         *   - Préférer la définition manuelle pour les enums critiques
         *
         * @example
         * @code
         * // Définition concise d'une enum de status avec ToString()
         * NkEnumeration(
         *     Status,                    // Nom de la classe wrapper
         *     uint8_t,                   // Type de base pour le stockage
         *     NK_ENUM_TO_STRING_BEGIN    // Début de ToString() personnalisé
         *         NK_ENUM_TO_STRING_SET_CONTENT2(Ok)
         *         NK_ENUM_TO_STRING_SET_CONTENT2(Error)
         *         NK_ENUM_TO_STRING_SET_CONTENT2(Pending)
         *     NK_ENUM_TO_STRING_END(Unknown),  // Fallback si valeur inconnue
         *     ,                          // Pas de méthodes supplémentaires
         *     Ok = 0, Error = 1, Pending = 2  // Valeurs de l'enum
         * );
         *
         * // Usage :
         * Status s(Status::Ok);
         * std::cout << s.ToString() << std::endl;  // Affiche "Ok"
         * @endcode
         */
        /** @{ */

        /**
         * @brief Macro de définition complète d'une énumération avec wrapper
         * @param enum_name Nom de la classe wrapper à générer
         * @param default_type Type entier sous-jacent pour le stockage
         * @param tostring Code pour la méthode ToString() (ou vide)
         * @param methods Méthodes supplémentaires à injecter dans la classe (ou vide)
         * @param ... Liste des valeurs de l'enum class (syntaxe enum standard)
         * @ingroup EnumDefinitionMacro
         *
         * @note
         *   Cette macro génère deux entités :
         *   1. enum class Enum<enum_name> : l'énumération C++ type-safe
         *   2. class <enum_name> : le wrapper avec opérateurs et helpers
         *
         *   L'alias 'using Enum = Enum##enum_name' dans le wrapper permet
         *   d'utiliser les macros NK_ENUM_TO_STRING_*2 avec préfixe automatique.
         */
        #define NkEnumeration(enum_name, default_type, tostring, methods, ...) \
            enum class NKENTSEU_API Enum##enum_name : default_type { __VA_ARGS__ }; \
            class NKENTSEU_API enum_name { \
                protected: \
                    default_type value; \
                public: \
                    using BASE_TYPE = default_type; \
                    using Enum = Enum##enum_name; \
                    \
                    enum_name() noexcept : value(0) {} \
                    enum_name(Enum e) noexcept : value(static_cast<default_type>(e)) {} \
                    enum_name(default_type v) noexcept : value(v) {} \
                    enum_name(const enum_name& other) noexcept : value(other.value) {} \
                    \
                    tostring \
                    methods \
                    \
                    bool operator==(const enum_name& other) const noexcept { \
                        return value == other.value; \
                    } \
                    bool operator!=(const enum_name& other) const noexcept { \
                        return value != other.value; \
                    } \
                    bool operator==(const Enum& other) const noexcept { \
                        return value == static_cast<BASE_TYPE>(other); \
                    } \
                    bool operator!=(const Enum& other) const noexcept { \
                        return value != static_cast<BASE_TYPE>(other); \
                    } \
                    bool operator==(const BASE_TYPE& other) const noexcept { \
                        return value == static_cast<BASE_TYPE>(other); \
                    } \
                    bool operator!=(const BASE_TYPE& other) const noexcept { \
                        return value != static_cast<BASE_TYPE>(other); \
                    } \
                    \
                    enum_name& operator=(const enum_name& other) noexcept { \
                        this->value = other.value; \
                        return *this; \
                    } \
                    \
                    enum_name operator|(Enum e) const noexcept { \
                        return enum_name(value | static_cast<BASE_TYPE>(e)); \
                    } \
                    enum_name operator&(Enum e) const noexcept { \
                        return enum_name(value & static_cast<BASE_TYPE>(e)); \
                    } \
                    enum_name& operator|=(Enum e) noexcept { \
                        value |= static_cast<BASE_TYPE>(e); \
                        return *this; \
                    } \
                    enum_name& operator&=(Enum e) noexcept { \
                        value &= static_cast<BASE_TYPE>(e); \
                        return *this; \
                    } \
                    \
                    enum_name operator|(const enum_name& e) const noexcept { \
                        return enum_name(value | static_cast<BASE_TYPE>(e.value)); \
                    } \
                    enum_name operator&(const enum_name& e) const noexcept { \
                        return enum_name(value & static_cast<BASE_TYPE>(e.value)); \
                    } \
                    enum_name& operator|=(const enum_name& e) noexcept { \
                        value |= static_cast<BASE_TYPE>(e.value); \
                        return *this; \
                    } \
                    enum_name& operator&=(const enum_name& e) noexcept { \
                        value &= static_cast<BASE_TYPE>(e.value); \
                        return *this; \
                    } \
                    \
                    enum_name operator|(const BASE_TYPE& e) const noexcept { \
                        return enum_name(value | static_cast<BASE_TYPE>(e)); \
                    } \
                    enum_name operator&(const BASE_TYPE& e) const noexcept { \
                        return enum_name(value & static_cast<BASE_TYPE>(e)); \
                    } \
                    enum_name& operator|=(const BASE_TYPE& e) noexcept { \
                        value |= static_cast<BASE_TYPE>(e); \
                        return *this; \
                    } \
                    enum_name& operator&=(const BASE_TYPE& e) noexcept { \
                        value &= static_cast<BASE_TYPE>(e); \
                        return *this; \
                    } \
                    \
                    operator BASE_TYPE() const noexcept { \
                        return value; \
                    } \
                    operator Enum() const noexcept { \
                        return static_cast<Enum>(value); \
                    } \
                    \
                    bool HasFlag(Enum e) const noexcept { \
                        return (value & static_cast<BASE_TYPE>(e)) != 0; \
                    } \
            }

        /** @} */ // End of EnumDefinitionMacro

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKENUMERATION_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKENUMERATION.H
// =============================================================================
// Ce fichier fournit des utilitaires pour la manipulation type-safe d'énumérations.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Wrapper simple avec ToString() manuel
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkEnumeration.h"
    #include <iostream>

    // Définition d'une enum de status
    enum class ConnectionState : uint8_t {
        Disconnected = 0,
        Connecting = 1,
        Connected = 2,
        Error = 3
    };

    // Wrapper avec ToString() personnalisé
    class ConnectionStateEnum : public nkentseu::NkEnumeration<ConnectionState, uint8_t> {
    public:
        using NkEnumeration::NkEnumeration;  // Hériter des constructeurs

        std::string ToString() const override {
            switch (static_cast<ConnectionState>(value)) {
                case ConnectionState::Disconnected: return "Disconnected";
                case ConnectionState::Connecting:   return "Connecting";
                case ConnectionState::Connected:    return "Connected";
                case ConnectionState::Error:        return "Error";
                default: return "Unknown(" + std::to_string(value) + ")";
            }
        }
    };

    // Usage
    void PrintConnectionState() {
        ConnectionStateEnum state(ConnectionState::Connected);
        std::cout << "State: " << state.ToString() << std::endl;  // "State: Connected"

        // Conversion implicite vers type sous-jacent
        uint8_t raw = state;  // raw == 2

        // Comparaison avec valeur enum
        if (state == ConnectionState::Connected) {
            std::cout << "Connection established!" << std::endl;
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Enum de type flags avec opérations bitwise
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkEnumeration.h"
    #include <iostream>

    // Définition d'une enum pour permissions (bitmask)
    enum class Permission : uint8_t {
        None  = 0,
        Read  = 1 << 0,  // 0x01
        Write = 1 << 1,  // 0x02
        Exec  = 1 << 2,  // 0x04
        All   = Read | Write | Exec  // 0x07
    };

    // Wrapper avec opérateurs bitwise activés et ToString() pour flags
    class PermissionEnum : public nkentseu::NkEnumeration<Permission, uint8_t> {
    public:
        using NkEnumeration::NkEnumeration;

        // Activer les opérateurs bitwise (décommentés depuis la base)
        PermissionEnum operator|(Permission p) const noexcept {
            return PermissionEnum(value | static_cast<BASE_TYPE>(p));
        }
        PermissionEnum& operator|=(Permission p) noexcept {
            value |= static_cast<BASE_TYPE>(p);
            return *this;
        }

        // ToString() qui liste tous les flags actifs
        std::string ToString() const override {
            NK_ENUM_TO_STRING_BEGIN
            NK_ENUM_TO_STRING_ADD_CONTENT(Permission::Read)
            NK_ENUM_TO_STRING_ADD_CONTENT(Permission::Write)
            NK_ENUM_TO_STRING_ADD_CONTENT(Permission::Exec)
            if (value == 0) { return "None"; }
            NK_ENUM_TO_STRING_END(Unknown)
        }
    };

    // Usage
    void ManagePermissions() {
        // Création de combinaisons de flags
        PermissionEnum userPerms = Permission::Read | Permission::Write;

        // Test de présence de flag
        if (userPerms.HasFlag(Permission::Read)) {
            std::cout << "User can read" << std::endl;
        }

        // Ajout dynamique d'un flag
        userPerms |= Permission::Exec;
        std::cout << "Permissions: " << userPerms.ToString() << std::endl;
        // Output: "Permissions: ReadWriteExec" (ou concaténé selon l'ordre)

        // Comparaison avec valeur combinée
        if (userPerms == Permission::All) {
            std::cout << "User has all permissions" << std::endl;
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Utilisation de la macro NkEnumeration() pour définition concise
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkEnumeration.h"
    #include <iostream>

    // Définition ultra-concise d'une enum avec ToString() généré
    NkEnumeration(
        LogLevel,              // Nom de la classe wrapper
        uint8_t,               // Type de base pour stockage
        NK_ENUM_TO_STRING_BEGIN
            NK_ENUM_TO_STRING_SET_CONTENT2(Debug)
            NK_ENUM_TO_STRING_SET_CONTENT2(Info)
            NK_ENUM_TO_STRING_SET_CONTENT2(Warning)
            NK_ENUM_TO_STRING_SET_CONTENT2(Error)
            NK_ENUM_TO_STRING_SET_CONTENT2(Critical)
        NK_ENUM_TO_STRING_END(Unknown),  // Fallback si valeur hors-range
        ,                    // Pas de méthodes supplémentaires
        // Valeurs de l'enum class
        Debug = 0,
        Info = 1,
        Warning = 2,
        Error = 3,
        Critical = 4
    );

    // Usage
    void LogMessage(LogLevel level, const std::string& msg) {
        // ToString() généré automatiquement
        std::cout << "[" << level.ToString() << "] " << msg << std::endl;

        // Comparaison type-safe
        if (level >= LogLevel::Warning) {
            // Traiter les warnings et erreurs de manière prioritaire
            SendAlert(msg);
        }
    }

    // Exemple d'appel
    void AppStartup() {
        LogMessage(LogLevel::Info, "Application started");
        LogMessage(LogLevel::Debug, "Loading configuration...");
        // Output:
        // [Info] Application started
        // [Debug] Loading configuration...
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Sérialisation/Deserialization via string (config files)
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkEnumeration.h"
    #include <fstream>
    #include <unordered_map>

    // Enum pour modes de rendu graphique
    enum class RenderMode : uint8_t {
        Software = 0,
        OpenGL = 1,
        Vulkan = 2,
        DirectX12 = 3,
        Metal = 4
    };

    class RenderModeEnum : public nkentseu::NkEnumeration<RenderMode, uint8_t> {
    public:
        using NkEnumeration::NkEnumeration;

        std::string ToString() const override {
            NK_ENUM_TO_STRING_BEGIN
            NK_ENUM_TO_STRING_SET_CONTENT2(Software)
            NK_ENUM_TO_STRING_SET_CONTENT2(OpenGL)
            NK_ENUM_TO_STRING_SET_CONTENT2(Vulkan)
            NK_ENUM_TO_STRING_SET_CONTENT2(DirectX12)
            NK_ENUM_TO_STRING_SET_CONTENT2(Metal)
            NK_ENUM_TO_STRING_END(Unknown)
        }

        // Parser manuel plus robuste que les macros expérimentales
        static RenderModeEnum FromString(const std::string& str) {
            static const std::unordered_map<std::string, RenderMode> lookup = {
                {"Software", RenderMode::Software},
                {"OpenGL", RenderMode::OpenGL},
                {"Vulkan", RenderMode::Vulkan},
                {"DirectX12", RenderMode::DirectX12},
                {"Metal", RenderMode::Metal}
            };

            auto it = lookup.find(str);
            if (it != lookup.end()) {
                return RenderModeEnum(it->second);
            }
            return RenderModeEnum(RenderMode::Software);  // Fallback
        }
    };

    // Chargement de configuration depuis fichier
    RenderModeEnum LoadGraphicsConfig(const std::string& configFile) {
        std::ifstream file(configFile);
        std::string line;

        while (std::getline(file, line)) {
            if (line.find("render_mode=") == 0) {
                std::string value = line.substr(12);  // Extraire après "render_mode="
                return RenderModeEnum::FromString(value);
            }
        }
        return RenderModeEnum(RenderMode::OpenGL);  // Default si non spécifié
    }

    // Sauvegarde de configuration
    void SaveGraphicsConfig(const std::string& configFile, RenderModeEnum mode) {
        std::ofstream file(configFile);
        file << "render_mode=" << mode.ToString() << std::endl;
        // Output dans le fichier: "render_mode=Vulkan"
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Intégration avec système de logging du framework
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkEnumeration.h"
    #include "NKCore/NkFoundationLog.h"  // Système de logging NKCore

    // Niveaux de sévérité pour logs
    NkEnumeration(
        LogSeverity,
        uint8_t,
        NK_ENUM_TO_STRING_BEGIN
            NK_ENUM_TO_STRING_SET_CONTENT2(Trace)
            NK_ENUM_TO_STRING_SET_CONTENT2(Debug)
            NK_ENUM_TO_STRING_SET_CONTENT2(Info)
            NK_ENUM_TO_STRING_SET_CONTENT2(Warning)
            NK_ENUM_TO_STRING_SET_CONTENT2(Error)
            NK_ENUM_TO_STRING_SET_CONTENT2(Critical)
        NK_ENUM_TO_STRING_END(Unknown),
        // Méthode utilitaire pour vérifier si un niveau doit être loggé
        bool ShouldLog(LogSeverity minLevel) const {
            return static_cast<BASE_TYPE>(value) >= static_cast<BASE_TYPE>(minLevel.value);
        },
        Trace = 0, Debug = 1, Info = 2, Warning = 3, Error = 4, Critical = 5
    );

    // Logger contextuel avec filtrage par sévérité
    class ContextLogger {
    public:
        ContextLogger(LogSeverity minLevel) : m_minLevel(minLevel) {}

        template <typename... Args>
        void Log(LogSeverity level, const char* format, Args&&... args) {
            // Filtrage : ne logger que si niveau >= seuil configuré
            if (level.ShouldLog(m_minLevel)) {
                NK_FOUNDATION_LOG(level.ToString().c_str(), format, std::forward<Args>(args)...);
            }
        }

        // Raccourcis pour niveaux courants
        template <typename... Args>
        void Info(const char* format, Args&&... args) {
            Log(LogSeverity::Info, format, std::forward<Args>(args)...);
        }

        template <typename... Args>
        void Error(const char* format, Args&&... args) {
            Log(LogSeverity::Error, format, std::forward<Args>(args)...);
        }

    private:
        LogSeverity m_minLevel;  // Seuil de filtrage
    };

    // Usage
    void ApplicationLoop() {
        ContextLogger logger(LogSeverity::Warning);  // Ne logger que warnings+

        logger.Info("This won't appear");      // Filtré (Info < Warning)
        logger.Warning("Low memory warning");  // Affiché
        logger.Error("Critical failure!");     // Affiché
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================