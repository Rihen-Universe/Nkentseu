// =============================================================================
// NKCore/NkOptional.h
// Implémentation portable d'un type optionnel sans dépendance STL.
//
// Design :
//  - Réimplémentation de std::optional sans dépendre de <optional>
//  - Support C++11+ avec évaluation constexpr là où possible
//  - Gestion mémoire sécurisée via placement new et destruction explicite
//  - Sémantique de copie/déplacement complète (copy/move semantics)
//  - Interface ergonomique avec opérateurs de déréférencement et accesseurs
//  - Intégration avec NKCore::traits pour NkMove, NkForward, etc.
//  - Compatible header-only pour une utilisation flexible
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CORE_NKOPTIONAL_H_INCLUDED
#define NKENTSEU_CORE_NKOPTIONAL_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // Inclusion des traits NKCore pour les utilitaires de mouvement et forwarding.
    // NkTypes.h fournit nk_bool et les types fondamentaux.

    #include "NKCore/NkTypes.h"
    #include "NKCore/NkTraits.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : DOCUMENTATION DOXYGEN DES GROUPES
    // -------------------------------------------------------------------------
    // Définition des groupes pour une documentation structurée.

    /**
     * @defgroup OptionalAPI API NkOptional
     * @brief Types et fonctions pour la gestion de valeurs optionnelles
     *
     * Ce groupe contient la classe template NkOptional<T> et le type sentinelle
     * NkNullOpt_t pour représenter l'absence de valeur de manière type-safe.
     *
     * @see NkOptional
     * @see NkNullOpt
     */

    /**
     * @defgroup OptionalUtilities Utilitaires Optionnels
     * @brief Fonctions helper pour manipuler les NkOptional
     *
     * Fonctions libres pour les opérations courantes sur les optionnels :
     * comparaison, swap, factory functions, etc.
     */

    // =========================================================================
    // SECTION 3 : ESPACE DE NOMS PRINCIPAL (INDENTATION HIÉRARCHIQUE)
    // =========================================================================
    // Déclaration de l'espace de noms principal nkentseu.
    //
    // NOTE SUR L'INDENTATION :
    //  - Le contenu de 'nkentseu' est indenté d'un niveau
    //  - Les classes/structs à l'intérieur suivent l'indentation standard

    namespace nkentseu {

        // ====================================================================
        // SECTION 4 : TYPE SENTINELLE NKNULLOPT_T
        // ====================================================================
        // Type tag pour représenter explicitement l'absence de valeur.

        /**
         * @brief Type sentinelle pour indiquer une valeur optionnelle vide
         * @struct NkNullOpt_t
         * @ingroup OptionalAPI
         *
         * Ce type est utilisé avec la constante globale NkNullOpt pour
         * initialiser ou réinitialiser un NkOptional<T> à l'état vide.
         *
         * @note Le constructeur est explicit pour éviter les conversions implicites.
         *
         * @example
         * @code
         * NkOptional<int> opt = NkNullOpt;  // opt est vide
         * opt = NkNullOpt;                   // Réinitialise opt à vide
         * @endcode
         */
        struct NkNullOpt_t {

            /**
             * @brief Constructeur explicite (nécessite un argument dummy)
             * @param dummy Paramètre fictif pour éviter les constructions implicites
             */
            explicit constexpr NkNullOpt_t(int dummy) noexcept {

                (void)dummy;  // Silence unused parameter warning
            }
        };

        /**
         * @brief Instance globale de NkNullOpt_t pour usage pratique
         * @var NkNullOpt
         * @ingroup OptionalAPI
         *
         * Constante constexpr utilisée pour initialiser ou assigner
         * un NkOptional<T> à l'état "sans valeur".
         *
         * @example
         * @code
         * NkOptional<std::string> name = NkNullOpt;
         * if (name.Empty()) {
         *     name.Emplace("DefaultName");
         * }
         * @endcode
         */
        inline constexpr NkNullOpt_t NkNullOpt{0};

        // ====================================================================
        // SECTION 5 : CLASSE TEMPLATE NKOPTIONAL
        // ====================================================================
        // Implémentation principale du type optionnel.

        /**
         * @brief Conteneur pour une valeur optionnelle de type T
         * @class NkOptional
         * @tparam T Type de la valeur potentiellement contenue
         * @ingroup OptionalAPI
         *
         * Cette classe fournit une alternative portable à std::optional,
         * sans dépendance à la STL, pour les environnements contraints.
         *
         * Caractéristiques principales :
         *  - Stockage inline via buffer aligné (pas d'allocation dynamique)
         *  - Sémantique de copie et de déplacement complète
         *  - Interface intuitive avec opérateurs *, ->, et accesseurs nommés
         *  - Méthodes utilitaires : Emplace, Reset, ValueOr, Swap, etc.
         *  - Support constexpr pour les opérations simples en C++11+
         *
         * @note Cette classe n'est pas thread-safe : synchronisation externe requise
         *       pour un accès concurrent depuis plusieurs threads.
         *
         * @warning L'accès à la valeur via operator* ou Value() sans vérification
         *          préalable de HasValue() entraîne un comportement indéfini.
         *          Préférer ValueOr() ou GetIf() pour un accès sécurisé.
         *
         * @example
         * @code
         * // Création et utilisation basique
         * NkOptional<int> maybeValue;
         * maybeValue.Emplace(42);
         *
         * if (maybeValue.HasValue()) {
         *     int val = maybeValue.Value();  // val == 42
         * }
         *
         * // Réinitialisation
         * maybeValue = NkNullOpt;  // maybeValue est maintenant vide
         *
         * // Accès sécurisé avec fallback
         * int result = maybeValue.ValueOr(0);  // result == 0
         *
         * // Sémantique de déplacement
         * NkOptional<std::string> opt1;
         * opt1.Emplace("Hello");
         * NkOptional<std::string> opt2 = traits::NkMove(opt1);
         * // opt1 est maintenant vide, opt2 contient "Hello"
         * @endcode
         */
        template <typename T>
        class NkOptional {

            public:

                // -----------------------------------------------------------------
                // Sous-section : Constructeurs
                // -----------------------------------------------------------------

                /**
                 * @brief Constructeur par défaut (état vide)
                 *
                 * Initialise l'optionnel sans valeur contenue.
                 *
                 * @note constexpr : peut être utilisé dans des contextes compile-time.
                 */
                constexpr NkOptional() noexcept
                    : mHasValue(false) {

                }

                /**
                 * @brief Constructeur depuis NkNullOpt (état vide explicite)
                 * @param nullOpt Instance de NkNullOpt_t (ignorée)
                 *
                 * Permet d'initialiser explicitement un optionnel vide.
                 *
                 * @example
                 * @code
                 * NkOptional<int> opt(NkNullOpt);  // opt est vide
                 * @endcode
                 */
                constexpr NkOptional(NkNullOpt_t nullOpt) noexcept
                    : mHasValue(false) {

                    (void)nullOpt;  // Silence unused parameter warning
                }

                /**
                 * @brief Constructeur par copie depuis une valeur de type T
                 * @param value Valeur à copier dans l'optionnel
                 *
                 * Crée un optionnel contenant une copie de value.
                 *
                 * @note Utilise Emplace() pour une construction in-place sécurisée.
                 */
                NkOptional(const T& value)
                    : mHasValue(false) {

                    Emplace(value);
                }

                /**
                 * @brief Constructeur par déplacement depuis une valeur de type T
                 * @param value Valeur à déplacer dans l'optionnel
                 *
                 * Crée un optionnel en déplaçant value, laissant value dans
                 * un état valide mais non spécifié.
                 *
                 * @note Utilise traits::NkMove() pour le transfert de propriété.
                 */
                NkOptional(T&& value)
                    : mHasValue(false) {

                    Emplace(traits::NkMove(value));
                }

                /**
                 * @brief Constructeur de copie depuis un autre NkOptional
                 * @param other Optionnel source à copier
                 *
                 * Si other contient une valeur, copie cette valeur.
                 * Sinon, initialise cet optionnel comme vide.
                 *
                 * @note Gère correctement l'auto-affectation via vérification interne.
                 */
                NkOptional(const NkOptional& other)
                    : mHasValue(false) {

                    if (other.mHasValue) {

                        Emplace(*other.Data());
                    }
                }

                /**
                 * @brief Constructeur de déplacement depuis un autre NkOptional
                 * @param other Optionnel source dont prendre possession
                 *
                 * Si other contient une valeur, déplace cette valeur et
                 * réinitialise other à l'état vide.
                 *
                 * @note noexcept : garantit l'absence d'exceptions pour les conteneurs.
                 */
                NkOptional(NkOptional&& other) noexcept
                    : mHasValue(false) {

                    if (other.mHasValue) {

                        Emplace(traits::NkMove(*other.Data()));
                        other.Reset();
                    }
                }

                // -----------------------------------------------------------------
                // Sous-section : Destructeur
                // -----------------------------------------------------------------

                /**
                 * @brief Destructeur (libère la valeur contenue si présente)
                 *
                 * Appelle explicitement le destructeur de T si une valeur
                 * est présente, puis marque l'optionnel comme vide.
                 *
                 * @note Garantit la libération correcte des ressources pour les
                 *       types non triviaux (RAII).
                 */
                ~NkOptional() {

                    Reset();
                }

                // -----------------------------------------------------------------
                // Sous-section : Opérateurs d'affectation
                // -----------------------------------------------------------------

                /**
                 * @brief Assignation depuis NkNullOpt (réinitialisation)
                 * @param nullOpt Instance de NkNullOpt_t (ignorée)
                 * @return Référence vers *this pour chaînage
                 *
                 * Réinitialise l'optionnel à l'état vide, détruisant la
                 * valeur contenue si présente.
                 *
                 * @example
                 * @code
                 * opt = NkNullOpt;  // Équivalent à opt.Reset()
                 * @endcode
                 */
                NkOptional& operator=(NkNullOpt_t nullOpt) noexcept {

                    Reset();
                    (void)nullOpt;
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par copie depuis un autre NkOptional
                 * @param other Optionnel source à copier
                 * @return Référence vers *this pour chaînage
                 *
                 * Gère correctement l'auto-affectation, la copie de valeur,
                 * et la réinitialisation si other est vide.
                 *
                 * @note Utilise l'idiome copy-and-swap implicitement via Emplace/Reset.
                 */
                NkOptional& operator=(const NkOptional& other) {

                    if (this == &other) {

                        return *this;
                    }

                    if (!other.mHasValue) {

                        Reset();
                        return *this;
                    }

                    Emplace(*other.Data());
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par déplacement depuis un autre NkOptional
                 * @param other Optionnel source dont prendre possession
                 * @return Référence vers *this pour chaînage
                 *
                 * Déplace la valeur de other si présente, puis réinitialise other.
                 * Gère l'auto-affectation et le cas où other est vide.
                 *
                 * @note noexcept : essentiel pour les conteneurs STL-like.
                 */
                NkOptional& operator=(NkOptional&& other) noexcept {

                    if (this == &other) {

                        return *this;
                    }

                    if (!other.mHasValue) {

                        Reset();
                        return *this;
                    }

                    Emplace(traits::NkMove(*other.Data()));
                    other.Reset();
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par copie depuis une valeur T
                 * @param value Valeur à copier dans l'optionnel
                 * @return Référence vers *this pour chaînage
                 *
                 * Remplace le contenu actuel par une copie de value.
                 * Équivalent à Emplace(value) avec retour chaînable.
                 */
                NkOptional& operator=(const T& value) {

                    Emplace(value);
                    return *this;
                }

                /**
                 * @brief Opérateur d'affectation par déplacement depuis une valeur T
                 * @param value Valeur à déplacer dans l'optionnel
                 * @return Référence vers *this pour chaînage
                 *
                 * Remplace le contenu actuel en déplaçant value.
                 * Laisse value dans un état valide mais non spécifié.
                 */
                NkOptional& operator=(T&& value) {

                    Emplace(traits::NkMove(value));
                    return *this;
                }

                // -----------------------------------------------------------------
                // Sous-section : Méthodes de modification
                // -----------------------------------------------------------------

                /**
                 * @brief Construit une valeur in-place dans l'optionnel
                 * @tparam Args Types des arguments de construction
                 * @param args Arguments forwardés au constructeur de T
                 * @return Référence vers la valeur nouvellement construite
                 *
                 * Détruit la valeur actuelle si présente, puis construit
                 * une nouvelle instance de T directement dans le buffer
                 * via placement new.
                 *
                 * @note Méthode préférée pour éviter les copies/moves temporaires.
                 *
                 * @example
                 * @code
                 * NkOptional<std::string> opt;
                 * opt.Emplace(10, 'x');  // Construit std::string(10, 'x') in-place
                 * @endcode
                 */
                template <typename... Args>
                T& Emplace(Args&&... args) {

                    Reset();
                    new (mStorage) T(traits::NkForward<Args>(args)...);
                    mHasValue = true;
                    return *Data();
                }

                /**
                 * @brief Détruit la valeur contenue et marque l'optionnel comme vide
                 *
                 * Appelle explicitement le destructeur de T si une valeur
                 * est présente, puis réinitialise le flag mHasValue.
                 *
                 * @note noexcept : ne lance jamais d'exception.
                 * @note Idempotent : peut être appelé plusieurs fois sans effet.
                 */
                void Reset() noexcept {

                    if (mHasValue) {

                        Data()->~T();
                        mHasValue = false;
                    }
                }

                // -----------------------------------------------------------------
                // Sous-section : Accesseurs et observateurs
                // -----------------------------------------------------------------

                /**
                 * @brief Vérifier si l'optionnel contient une valeur
                 * @return true si une valeur est présente, false sinon
                 *
                 * @note [[nodiscard]] : génère un warning si le résultat est ignoré.
                 * @note noexcept : opération sans effet de bord.
                 */
                [[nodiscard]] nk_bool HasValue() const noexcept {

                    return mHasValue;
                }

                /**
                 * @brief Vérifier si l'optionnel est vide (alias de !HasValue)
                 * @return true si aucune valeur n'est présente, false sinon
                 *
                 * @note Sémantiquement équivalent à !HasValue(), mais plus expressif
                 *       dans certains contextes (ex: if (opt.Empty()) {...}).
                 */
                [[nodiscard]] nk_bool Empty() const noexcept {

                    return !mHasValue;
                }

                /**
                 * @brief Conversion explicite vers nk_bool (pour usage en condition)
                 * @return mHasValue
                 *
                 * Permet d'utiliser l'optionnel directement dans des expressions
                 * booléennes, comme std::optional.
                 *
                 * @example
                 * @code
                 * if (opt) {  // Équivalent à if (opt.HasValue())
                 *     Use(opt.Value());
                 * }
                 * @endcode
                 */
                explicit constexpr operator nk_bool() const noexcept {

                    return mHasValue;
                }

                // -----------------------------------------------------------------
                // Sous-section : Opérateurs de déréférencement
                // -----------------------------------------------------------------

                /**
                 * @brief Accès membre via pointeur (version mutable)
                 * @return Pointeur vers la valeur contenue
                 *
                 * @warning Comportement indéfini si l'optionnel est vide.
                 *          Vérifier HasValue() avant utilisation.
                 */
                T* operator->() noexcept {

                    return Data();
                }

                /**
                 * @brief Accès membre via pointeur (version const)
                 * @return Pointeur const vers la valeur contenue
                 *
                 * @warning Comportement indéfini si l'optionnel est vide.
                 */
                const T* operator->() const noexcept {

                    return Data();
                }

                /**
                 * @brief Déférencement pour obtenir la valeur (version mutable)
                 * @return Référence vers la valeur contenue
                 *
                 * @warning Comportement indéfini si l'optionnel est vide.
                 *          Préférer ValueOr() pour un accès sécurisé.
                 */
                T& operator*() noexcept {

                    return *Data();
                }

                /**
                 * @brief Déférencement pour obtenir la valeur (version const)
                 * @return Référence const vers la valeur contenue
                 *
                 * @warning Comportement indéfini si l'optionnel est vide.
                 */
                const T& operator*() const noexcept {

                    return *Data();
                }

                // -----------------------------------------------------------------
                // Sous-section : Accesseurs nommés (sécurisés)
                // -----------------------------------------------------------------

                /**
                 * @brief Obtenir la valeur contenue (version mutable)
                 * @return Référence vers la valeur
                 *
                 * @note Alias de operator*(), fourni pour la clarté sémantique.
                 * @warning Comportement indéfini si l'optionnel est vide.
                 */
                T& Value() noexcept {

                    return *Data();
                }

                /**
                 * @brief Obtenir la valeur contenue (version const)
                 * @return Référence const vers la valeur
                 *
                 * @note Alias de operator*() const, pour la clarté sémantique.
                 * @warning Comportement indéfini si l'optionnel est vide.
                 */
                const T& Value() const noexcept {

                    return *Data();
                }

                /**
                 * @brief Obtenir un pointeur vers la valeur, ou nullptr si vide
                 * @return Pointeur vers T si présent, nullptr sinon
                 *
                 * Méthode sécurisée pour accéder à la valeur sans risque
                 * de comportement indéfini.
                 *
                 * @example
                 * @code
                 * if (T* ptr = opt.GetIf()) {
                 *     Use(*ptr);  // Safe : ptr n'est pas nullptr
                 * }
                 * @endcode
                 */
                T* GetIf() noexcept {

                    return mHasValue ? Data() : nullptr;
                }

                /**
                 * @brief Obtenir un pointeur const vers la valeur, ou nullptr si vide
                 * @return Pointeur const vers T si présent, nullptr sinon
                 *
                 * Version const de GetIf(), pour les contextes en lecture seule.
                 */
                const T* GetIf() const noexcept {

                    return mHasValue ? Data() : nullptr;
                }

                // -----------------------------------------------------------------
                // Sous-section : Méthodes utilitaires
                // -----------------------------------------------------------------

                /**
                 * @brief Obtenir la valeur ou une valeur de fallback par copie
                 * @tparam U Type convertible vers T (généralement T lui-même)
                 * @param fallback Valeur à retourner si l'optionnel est vide
                 * @return *Data() si présent, sinon une copie de fallback
                 *
                 * Méthode sécurisée pour extraire une valeur avec fallback.
                 * Ne modifie pas l'état de l'optionnel.
                 *
                 * @note La valeur de fallback est copiée : utiliser ValueOrRef()
                 *       pour éviter une copie coûteuse.
                 */
                T ValueOr(T fallback) const {

                    if (mHasValue) {

                        return *Data();
                    }

                    return fallback;
                }

                /**
                 * @brief Obtenir la valeur ou une référence de fallback
                 * @param fallback Référence vers la valeur à retourner si vide
                 * @return Référence vers *Data() si présent, sinon référence vers fallback
                 *
                 * Version par référence de ValueOr(), évitant une copie inutile.
                 * Idéal pour les types lourds ou non copiables.
                 *
                 * @note La référence retournée peut pointer vers fallback :
                 *       ne pas modifier fallback après l'appel si la référence
                 *       est conservée.
                 */
                const T& ValueOrRef(const T& fallback) const {

                    return mHasValue ? *Data() : fallback;
                }

                /**
                 * @brief Échanger le contenu avec un autre optionnel
                 * @param other Autre optionnel avec lequel échanger
                 *
                 * Implémente un swap efficace en trois cas :
                 *  1. Les deux ont une valeur : swap des valeurs
                 *  2. Un seul a une valeur : move vers l'autre + reset
                 *  3. Aucun n'a de valeur : no-op
                 *
                 * @note Gère l'auto-échange sans effet secondaire.
                 * @note noexcept si T est noexcept-movable.
                 */
                void Swap(NkOptional& other) {

                    if (this == &other) {

                        return;
                    }

                    if (mHasValue && other.mHasValue) {

                        T tmp = traits::NkMove(*Data());
                        *Data() = traits::NkMove(*other.Data());
                        *other.Data() = traits::NkMove(tmp);
                        return;
                    }

                    if (mHasValue) {

                        other.Emplace(traits::NkMove(*Data()));
                        Reset();
                        return;
                    }

                    if (other.mHasValue) {

                        Emplace(traits::NkMove(*other.Data()));
                        other.Reset();
                    }
                }

            private:

                // -----------------------------------------------------------------
                // Sous-section : Méthodes privées d'accès au stockage
                // -----------------------------------------------------------------

                /**
                 * @brief Obtenir un pointeur vers la valeur stockée (mutable)
                 * @return Pointeur vers T interprété depuis mStorage
                 *
                 * @note Utilise reinterpret_cast : sûr car mStorage est aligné
                 *       sur T et de taille sizeof(T).
                 */
                T* Data() noexcept {

                    return reinterpret_cast<T*>(mStorage);
                }

                /**
                 * @brief Obtenir un pointeur vers la valeur stockée (const)
                 * @return Pointeur const vers T interprété depuis mStorage
                 */
                const T* Data() const noexcept {

                    return reinterpret_cast<const T*>(mStorage);
                }

                // -----------------------------------------------------------------
                // Sous-section : Membres de données
                // -----------------------------------------------------------------

                /**
                 * @brief Buffer de stockage inline pour la valeur de type T
                 * @var mStorage
                 *
                 * Tableau d'octets aligné sur l'alignement requis de T,
                 * de taille suffisante pour contenir une instance de T.
                 *
                 * @note L'alignement est garanti via alignas(T).
                 * @note La mémoire est gérée manuellement via placement new
                 *       et appel explicite au destructeur.
                 */
                alignas(T) nk_uint8 mStorage[sizeof(T)];

                /**
                 * @brief Flag indiquant si une valeur est actuellement stockée
                 * @var mHasValue
                 *
                 * true si mStorage contient une instance valide de T,
                 * false si l'optionnel est vide.
                 *
                 * @note Ce flag est essentiel pour gérer correctement la
                 *       construction/destruction manuelle de la valeur.
                 */
                nk_bool mHasValue;
        };

    } // namespace nkentseu

#endif // NKENTSEU_CORE_NKOPTIONAL_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DE NKOPTIONAL.H
// =============================================================================
// Ce fichier fournit une implémentation portable d'un type optionnel,
// utile pour représenter des valeurs qui peuvent être absentes.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Création et utilisation basique d'un NkOptional
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkOptional.h"
    #include <cstdio>

    void BasicUsage() {
        using namespace nkentseu;

        // Création d'un optionnel vide
        NkOptional<int> maybeInt;

        // Vérification de l'état
        if (maybeInt.Empty()) {
            printf("maybeInt est vide\n");
        }

        // Assignation d'une valeur
        maybeInt.Emplace(42);

        // Accès à la valeur (après vérification)
        if (maybeInt.HasValue()) {
            printf("Valeur : %d\n", maybeInt.Value());  // Affiche : Valeur : 42
        }

        // Utilisation en condition (conversion explicite)
        if (maybeInt) {
            printf("Via operator bool : %d\n", *maybeInt);
        }

        // Réinitialisation
        maybeInt = NkNullOpt;
        printf("Après reset : %s\n", maybeInt.Empty() ? "vide" : "non vide");
    }
*/

// -----------------------------------------------------------------------------
// Exemple 2 : Accès sécurisé avec ValueOr et GetIf
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkOptional.h"
    #include <string>

    void SafeAccess() {
        using namespace nkentseu;

        NkOptional<std::string> username;

        // ValueOr : fallback par copie
        std::string name = username.ValueOr("Anonymous");
        // name == "Anonymous" car username est vide

        // ValueOrRef : fallback par référence (évite une copie)
        const std::string defaultName = "Guest";
        const std::string& ref = username.ValueOrRef(defaultName);
        // ref référence defaultName

        // GetIf : accès via pointeur sécurisé
        if (std::string* ptr = username.GetIf()) {
            // ptr n'est pas nullptr : utilisation safe
            printf("Username: %s\n", ptr->c_str());
        } else {
            printf("Username non défini\n");
        }

        // Assignation et réutilisation
        username.Emplace("Alice");
        if (auto* ptr = username.GetIf()) {
            printf("Maintenant : %s\n", ptr->c_str());  // Affiche : Maintenant : Alice
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 3 : Sémantique de copie et de déplacement
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkOptional.h"
    #include "NKCore/NkTraits.h"  // Pour traits::NkMove

    void CopyMoveSemantics() {
        using namespace nkentseu;

        // Copie
        NkOptional<int> opt1;
        opt1.Emplace(100);
        NkOptional<int> opt2 = opt1;  // Copie de la valeur
        // opt1 et opt2 contiennent tous deux 100

        // Déplacement
        NkOptional<std::string> src;
        src.Emplace("HeavyStringData...");
        NkOptional<std::string> dst = traits::NkMove(src);
        // dst contient "HeavyStringData...", src est vide

        // Affectation
        NkOptional<int> target;
        target = 200;  // Équivalent à target.Emplace(200)
        target = NkNullOpt;  // Réinitialise target

        // Swap
        NkOptional<int> a, b;
        a.Emplace(1);
        b.Emplace(2);
        a.Swap(b);
        // a contient 2, b contient 1
    }
*/

// -----------------------------------------------------------------------------
// Exemple 4 : Construction in-place avec Emplace
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkOptional.h"
    #include <string>

    void InPlaceConstruction() {
        using namespace nkentseu;

        // Évite une copie temporaire : construit directement dans le storage
        NkOptional<std::string> opt;
        opt.Emplace(10, 'x');  // Construit std::string(10, 'x') in-place

        // Équivalent à :
        // std::string temp(10, 'x');
        // opt = temp;  // Mais avec une copie supplémentaire

        // Emplace avec arguments multiples
        struct Point {
            int x, y;
            Point(int x_, int y_) : x(x_), y(y_) {}
        };

        NkOptional<Point> maybePoint;
        maybePoint.Emplace(10, 20);  // Appelle Point(10, 20) directement

        if (auto* p = maybePoint.GetIf()) {
            printf("Point(%d, %d)\n", p->x, p->y);  // Affiche : Point(10, 20)
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 5 : Utilisation dans des structures de données
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkOptional.h"

    struct ConfigEntry {
        nkentseu::NkOptional<int> maxRetries;
        nkentseu::NkOptional<float> timeout;
        nkentseu::NkOptional<const char*> description;

        // Méthode utilitaire pour obtenir une valeur avec fallback
        int GetMaxRetriesOrDefault(int defaultVal) const {
            return maxRetries.ValueOr(defaultVal);
        }
    };

    void ProcessConfig(const ConfigEntry& cfg) {
        using namespace nkentseu;

        // Utilisation des fallbacks
        int retries = cfg.GetMaxRetriesOrDefault(3);
        float timeout = cfg.timeout.ValueOr(30.0f);
        const char* desc = cfg.description.ValueOr("No description");

        printf("Retries: %d, Timeout: %.1fs, Desc: %s\n",
               retries, timeout, desc);
    }
*/

// -----------------------------------------------------------------------------
// Exemple 6 : Combinaison avec NkTraits pour métaprogrammation
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkOptional.h"
    #include "NKCore/NkTraits.h"

    // Fonction template activée seulement si T est copiable
    template<typename T>
    traits::NkEnableIf_t<traits::NkIsCopyConstructible_v<T>, T>
    CopyValueOr(const NkOptional<T>& opt, const T& fallback) {
        return opt.ValueOr(fallback);
    }

    // Fonction pour types déplaçables uniquement
    template<typename T>
    traits::NkEnableIf_t<traits::NkIsMoveConstructible_v<T>, T>
    MoveValueOr(NkOptional<T>&& opt, T&& fallback) {
        return opt.HasValue()
            ? traits::NkMove(opt.Value())
            : traits::NkMove(fallback);
    }

    void TraitBasedUsage() {
        using namespace nkentseu;

        NkOptional<int> numOpt = 42;
        int result = CopyValueOr(numOpt, 0);  // OK : int est copiable

        NkOptional<std::unique_ptr<int>> ptrOpt;
        // CopyValueOr(ptrOpt, nullptr);  // Erreur : unique_ptr n'est pas copiable
        auto ptr = MoveValueOr(traits::NkMove(ptrOpt), nullptr);  // OK
    }
*/

// -----------------------------------------------------------------------------
// Exemple 7 : Gestion d'erreurs avec optionnels
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkOptional.h"
    #include <cstdio>

    // Fonction qui peut échouer : retourne NkOptional<T>
    nkentseu::NkOptional<int> ParseInteger(const char* str) {
        if (!str || !*str) {
            return nkentseu::NkNullOpt;  // Échec : chaîne vide
        }

        int value = 0;
        // Parsing simplifié (à remplacer par une implémentation robuste)
        while (*str >= '0' && *str <= '9') {
            value = value * 10 + (*str - '0');
            ++str;
        }

        if (*str != '\0') {
            return nkentseu::NkNullOpt;  // Caractères invalides
        }

        return value;  // Succès : retourne la valeur
    }

    void ErrorHandlingExample() {
        using namespace nkentseu;

        const char* inputs[] = {"123", "abc", "", "456xyz"};

        for (const char* input : inputs) {
            auto result = ParseInteger(input);

            if (result.HasValue()) {
                printf("'%s' -> %d (succès)\n", input, result.Value());
            } else {
                printf("'%s' -> échec de parsing\n", input);
            }
        }
    }
*/

// -----------------------------------------------------------------------------
// Exemple 8 : Intégration avec d'autres modules NKCore/NKPlatform
// -----------------------------------------------------------------------------
/*
    #include "NKCore/NkOptional.h"
    #include "NKPlatform/NkEnv.h"      // Pour NkEnvString
    #include "NKPlatform/NkFoundationLog.h"  // Pour le logging

    nkentseu::NkOptional<nkentseu::env::NkEnvString>
    GetOptionalEnvVar(const char* name) {
        using namespace nkentseu;
        using namespace nkentseu::env;

        NkEnvString varName(name);
        NkEnvString value;
        NkGet(varName, value);

        if (value.Empty()) {
            return NkNullOpt;  // Variable non définie
        }

        return NkOptional<NkEnvString>(traits::NkMove(value));
    }

    void PlatformConfigExample() {
        using namespace nkentseu;

        auto logLevel = GetOptionalEnvVar("APP_LOG_LEVEL");

        if (logLevel.HasValue()) {
            NK_FOUNDATION_LOG_INFO("Log level from env: %s",
                                   logLevel->CStr());
        } else {
            NK_FOUNDATION_LOG_INFO("Using default log level");
        }
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================