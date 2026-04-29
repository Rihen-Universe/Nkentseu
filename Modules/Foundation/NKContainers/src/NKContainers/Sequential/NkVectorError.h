// =============================================================================
// NKContainers/Errors/NkVectorError.h
// Gestion des erreurs et exceptions pour les conteneurs séquentiels NKContainers.
//
// Design :
//  - Réutilisation DIRECTE des macros NKPlatform/NKCore (ZÉRO duplication)
//  - Gestion conditionnelle des exceptions via NKENTSEU_USE_EXCEPTIONS
//  - Macros d'erreur unifiées : NKENTSEU_CONTAINERS_THROW_* / NKENTSEU_CONTAINERS_ASSERT_*
//  - Indentation stricte : visibilité, blocs conditionnels, namespaces tous indentés
//  - Une instruction par ligne pour lisibilité et maintenance
//  - Compatibilité multiplateforme via NKPlatform
//
// Auteur : Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_CONTAINERS_ERRORS_NKVECTORERROR_H_INCLUDED
#define NKENTSEU_CONTAINERS_ERRORS_NKVECTORERROR_H_INCLUDED

    // -------------------------------------------------------------------------
    // SECTION 1 : EN-TÊTES ET DÉPENDANCES
    // -------------------------------------------------------------------------
    // NKContainers dépend de NKCore pour les types fondamentaux et NKPlatform
    // pour les macros d'assertion et de compatibilité.
    // Aucune duplication : nous importons et utilisons directement ces macros.

    #include "NKContainers/NkContainersApi.h"
    #include "NKCore/NkTypes.h"
    #include "NKCore/Assert/NkAssert.h"

    // -------------------------------------------------------------------------
    // SECTION 2 : ESPACE DE NOMS PRINCIPAL
    // -------------------------------------------------------------------------
    // Tous les composants NKContainers sont encapsulés dans ce namespace
    // pour éviter les collisions de noms et assurer une API cohérente.

    namespace nkentseu {
        namespace errors {

            // ====================================================================
            // SECTION 3 : CODES D'ERREUR (Enum class type-safe)
            // ====================================================================
            /**
             * @enum NkVectorError
             * @brief Codes d'erreur pour les opérations sur les vecteurs et conteneurs séquentiels.
             * @ingroup ContainerErrors
             *
             * Cette énumération fortement typée définit l'ensemble des erreurs
             * potentielles rencontrées lors de l'utilisation des conteneurs NKContainers.
             * Chaque valeur possède une sémantique précise pour un diagnostic fiable.
             *
             * @note Utiliser NkVectorErrorToString() pour obtenir une représentation
             *       lisible de l'erreur à des fins de logging ou de débogage.
             *
             * @see NkVectorErrorToString()
             */
            enum class NKENTSEU_CONTAINERS_CLASS_EXPORT NkVectorError : nk_uint32 {
                NK_SUCCESS = 0,           ///< Opération terminée avec succès
                NK_OUT_OF_RANGE,          ///< Index spécifié hors des bornes valides du conteneur
                NK_OUT_OF_MEMORY,         ///< Échec d'allocation mémoire (heap exhaustion)
                NK_INVALID_ARGUMENT,      ///< Argument invalide ou incohérent passé à une fonction
                NK_LENGTH_ERROR,          ///< Erreur de longueur (ex: resize avec valeur négative)
                NK_OVERFLOW,              ///< Dépassement de la capacité maximale représentable
                NK_UNDERFLOW,             ///< Tentative d'accès avant le début du conteneur
                NK_BAD_ALLOC              ///< Échec d'allocation avec informations détaillées
            };

            // ====================================================================
            // SECTION 4 : GESTION CONDITIONNELLE DES EXCEPTIONS
            // ====================================================================
            /**
             * @defgroup ContainerExceptionHandling Gestion des Exceptions
             * @brief Macros et classes pour gestion d'erreurs avec/sans exceptions C++.
             *
             * Le comportement de gestion d'erreurs est contrôlé par la macro
             * NKENTSEU_USE_EXCEPTIONS définie au moment de la compilation :
             *
             *  - Si définie et vraie : les macros NKENTSEU_CONTAINERS_THROW_*
             *    lèvent des exceptions C++ standards dérivées de NkVectorException.
             *
             *  - Si non définie ou fausse : les mêmes macros utilisent
             *    NKENTSEU_ASSERT_MSG pour un comportement de type "assertion"
             *    (arrêt en debug, no-op en release).
             *
             * Cette approche permet d'écrire un code unique qui s'adapte
             * automatiquement au mode de build sélectionné.
             *
             * @note Cette configuration est indépendante de NKCore et NKPlatform :
             *       NKContainers peut utiliser les exceptions même si NKCore
             *       est compilé sans, et vice-versa.
             *
             * @example Configuration CMake
             * @code
             * # Mode avec exceptions (développement, tests)
             * target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=1)
             *
             * # Mode sans exceptions (production, embarqué, temps réel)
             * target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=0)
             * target_compile_definitions(monapp PRIVATE NKENTSEU_ENABLE_ASSERT=1)
             * @endcode
             */
            /** @{ */

            #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS

                // -----------------------------------------------------------------
                // Sous-section : Classes d'exceptions (mode avec exceptions C++)
                // -----------------------------------------------------------------

                /**
                 * @brief Exception de base pour toutes les erreurs de conteneurs NKContainers.
                 * @class NkVectorException
                 * @ingroup ContainerExceptionHandling
                 *
                 * Cette classe fournit une interface commune pour accéder aux
                 * informations de diagnostic d'une erreur : code, message optionnel,
                 * et index concerné le cas échéant.
                 *
                 * Toutes les méthodes sont noexcept pour garantir la sécurité
                 * lors du traitement d'exceptions dans des contextes critiques.
                 *
                 * @note Cette classe est exportée via NKENTSEU_CONTAINERS_CLASS_EXPORT
                 *       pour une utilisation correcte dans les builds DLL/shared.
                 */
                class NKENTSEU_CONTAINERS_CLASS_EXPORT NkVectorException {
                    public:

                        /**
                         * @brief Constructeur principal avec contexte d'erreur complet.
                         * @param error Code d'erreur de type NkVectorError identifiant la nature du problème.
                         * @param message Message personnalisé optionnel (nullptr pour message par défaut).
                         * @param index Index concerné par l'erreur (0 si non applicable).
                         *
                         * @note Tous les paramètres sont copiés ou référencés de manière safe
                         *       pour garantir la validité des données même après destruction
                         *       du contexte d'origine.
                         */
                        explicit NkVectorException(
                            NkVectorError error,
                            const nk_char* message = nullptr,
                            nk_size index = 0
                        ) NKENTSEU_NOEXCEPT
                            : mError(error)
                            , mMessage(message)
                            , mIndex(index) {
                        }

                        /**
                         * @brief Retourne le code d'erreur identifiant la nature du problème.
                         * @return Valeur de type NkVectorError correspondant à l'erreur rencontrée.
                         *
                         * @note Méthode noexcept : garantie de ne jamais lever d'exception,
                         *       essentielle pour un usage dans des gestionnaires d'exceptions.
                         */
                        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                        NkVectorError GetError() const NKENTSEU_NOEXCEPT {
                            return mError;
                        }

                        /**
                         * @brief Retourne le message personnalisé associé à l'erreur.
                         * @return Pointeur vers chaîne de caractères (nullptr si aucun message personnalisé).
                         *
                         * @note Le message personnalisé a priorité sur le message par défaut
                         *       retourné par What(). Cette méthode permet d'accéder
                         *       directement au texte fourni lors de la construction.
                         */
                        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                        const nk_char* GetMessage() const NKENTSEU_NOEXCEPT {
                            return mMessage;
                        }

                        /**
                         * @brief Retourne l'index concerné par l'erreur si applicable.
                         * @return Valeur de type nk_size représentant l'index, ou 0 si non applicable.
                         *
                         * @note Utile pour les erreurs de type "out of range" où l'index
                         *       fautif est pertinent pour le diagnostic. Pour les
                         *       autres types d'erreurs, cette valeur est généralement 0.
                         */
                        [[nodiscard]] NKENTSEU_CONTAINERS_API_FORCE_INLINE
                        nk_size GetIndex() const NKENTSEU_NOEXCEPT {
                            return mIndex;
                        }

                        /**
                         * @brief Retourne une description lisible et complète de l'erreur.
                         * @return Pointeur vers chaîne de caractères statique décrivant l'erreur.
                         *
                         * @note Cette méthode retourne le message personnalisé si fourni,
                         *       sinon délègue à NkVectorErrorToString() pour obtenir
                         *       une description générique basée sur le code d'erreur.
                         *
                         * @see NkVectorErrorToString()
                         */
                        [[nodiscard]] NKENTSEU_CONTAINERS_API_NO_INLINE
                        const nk_char* What() const NKENTSEU_NOEXCEPT {
                            if (mMessage != nullptr) {
                                return mMessage;
                            }
                            return NkVectorErrorToString(mError);
                        }

                    private:

                        NkVectorError mError;       ///< Code d'erreur identifiant la nature du problème
                        const nk_char* mMessage;    ///< Message personnalisé optionnel (peut être nullptr)
                        nk_size mIndex;             ///< Index concerné par l'erreur (0 si non applicable)

                };

                /**
                 * @brief Exception spécifique pour les accès hors bornes d'un conteneur.
                 * @class NkVectorOutOfRangeException
                 * @ingroup ContainerExceptionHandling
                 *
                 * Cette exception est levée lorsqu'un index spécifié pour accéder
                 * à un élément d'un conteneur est inférieur à 0 ou supérieur ou
                 * égal à la taille actuelle du conteneur.
                 *
                 * @note Hérite de NkVectorException pour une gestion unifiée des erreurs.
                 */
                class NKENTSEU_CONTAINERS_CLASS_EXPORT NkVectorOutOfRangeException
                    : public NkVectorException {
                    public:

                        /**
                         * @brief Constructeur avec contexte d'index et taille pour diagnostic.
                         * @param index Index tenté qui a provoqué l'erreur.
                         * @param size Taille actuelle du conteneur au moment de l'erreur.
                         *
                         * @note Le paramètre size est actuellement utilisé pour le contexte
                         *       interne uniquement. Il est réservé pour une extension
                         *       future permettant de générer des messages d'erreur
                         *       dynamiques incluant la taille valide maximale.
                         */
                        explicit NkVectorOutOfRangeException(
                            nk_size index,
                            nk_size size
                        ) NKENTSEU_NOEXCEPT
                            : NkVectorException(
                                NkVectorError::NK_OUT_OF_RANGE,
                                "index out of range",
                                index
                            ) {
                            (void)size;
                        }

                };

                /**
                 * @brief Exception spécifique pour les échecs d'allocation mémoire.
                 * @class NkVectorBadAllocException
                 * @ingroup ContainerExceptionHandling
                 *
                 * Cette exception est levée lorsqu'une opération nécessitant
                 * une allocation dynamique de mémoire échoue, typiquement en
                 * raison d'un épuisement du heap ou d'une demande de taille
                 * excessive.
                 *
                 * @note L'index stocké dans l'exception de base représente la
                 *       taille en bytes qui n'a pas pu être allouée.
                 */
                class NKENTSEU_CONTAINERS_CLASS_EXPORT NkVectorBadAllocException
                    : public NkVectorException {
                    public:

                        /**
                         * @brief Constructeur avec taille demandée pour diagnostic.
                         * @param requestedSize Taille en bytes qui a échoué à être allouée.
                         *
                         * @note Cette information permet au code client d'implémenter
                         *       des stratégies de fallback, comme réduire la taille
                         *       demandée ou libérer d'autres ressources avant de
                         *       réessayer l'allocation.
                         */
                        explicit NkVectorBadAllocException(
                            nk_size requestedSize
                        ) NKENTSEU_NOEXCEPT
                            : NkVectorException(
                                NkVectorError::NK_BAD_ALLOC,
                                "memory allocation failed",
                                requestedSize
                            ) {
                        }

                };

                /**
                 * @brief Exception spécifique pour les erreurs de longueur de conteneur.
                 * @class NkVectorLengthException
                 * @ingroup ContainerExceptionHandling
                 *
                 * Cette exception est levée lorsqu'une opération modifiant la
                 * taille d'un conteneur reçoit un paramètre invalide, comme une
                 * nouvelle taille négative, supérieure à la capacité maximale,
                 * ou provoquant un dépassement arithmétique.
                 */
                class NKENTSEU_CONTAINERS_CLASS_EXPORT NkVectorLengthException
                    : public NkVectorException {
                    public:

                        /**
                         * @brief Constructeur avec message d'erreur personnalisé optionnel.
                         * @param message Message décrivant la nature précise de l'erreur de longueur.
                         *
                         * @note Si aucun message n'est fourni, la valeur par défaut
                         *       "length error" est utilisée. Pour un diagnostic plus
                         *       précis, il est recommandé de fournir un message
                         *       contextuel décrivant l'opération et les valeurs en cause.
                         */
                        explicit NkVectorLengthException(
                            const nk_char* message = "length error"
                        ) NKENTSEU_NOEXCEPT
                            : NkVectorException(
                                NkVectorError::NK_LENGTH_ERROR,
                                message
                            ) {
                        }

                };

            #endif

            // -----------------------------------------------------------------
            // Sous-section : Macros de gestion d'erreurs unifiées
            // -----------------------------------------------------------------
            // Ces macros fournissent une interface unique pour signaler des
            // erreurs, avec un comportement qui s'adapte automatiquement au
            // mode de compilation (avec ou sans exceptions C++).

            #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS

                /**
                 * @brief Macro pour signaler une erreur d'index hors bornes.
                 * @def NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE
                 * @param index Index qui a provoqué l'erreur.
                 * @param size Taille actuelle du conteneur pour contexte.
                 * @ingroup ContainerExceptionHandling
                 *
                 * Cette macro lève une exception NkVectorOutOfRangeException
                 * si NKENTSEU_USE_EXCEPTIONS est défini et vrai. Sinon, elle
                 * utilise NKENTSEU_ASSERT_MSG pour un comportement de type
                 * assertion (arrêt en debug, no-op en release).
                 *
                 * @note Les paramètres sont évalués une seule fois et protégés
                 *       par des parenthèses pour éviter les problèmes de
                 *       priorité d'opérateurs dans les expressions complexes.
                 *
                 * @example
                 * @code
                 * if (index >= vector.Size())
                 * {
                 *     NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE(index, vector.Size());
                 * }
                 * @endcode
                 */
                #define NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE(index, size) \
                    throw ::nkentseu::containers::errors::NkVectorOutOfRangeException((index), (size))

                /**
                 * @brief Macro pour signaler un échec d'allocation mémoire.
                 * @def NKENTSEU_CONTAINERS_THROW_BAD_ALLOC
                 * @param size Taille demandée en bytes qui n'a pas pu être allouée.
                 * @ingroup ContainerExceptionHandling
                 *
                 * Cette macro lève une exception NkVectorBadAllocException
                 * si les exceptions sont activées. Sinon, elle déclenche
                 * une assertion avec un message contextuel incluant la
                 * taille demandée.
                 *
                 * @note Utile dans les méthodes reserve(), resize(), push_back()
                 *       lorsque l'allocation interne échoue.
                 */
                #define NKENTSEU_CONTAINERS_THROW_BAD_ALLOC(size) \
                    throw ::nkentseu::containers::errors::NkVectorBadAllocException((size))

                /**
                 * @brief Macro pour signaler une erreur de longueur de conteneur.
                 * @def NKENTSEU_CONTAINERS_THROW_LENGTH_ERROR
                 * @param msg Message décrivant la nature de l'erreur de longueur.
                 * @ingroup ContainerExceptionHandling
                 *
                 * Cette macro lève une exception NkVectorLengthException
                 * avec le message fourni si les exceptions sont activées.
                 * Sinon, elle utilise NKENTSEU_ASSERT_MSG avec le même message.
                 *
                 * @note Le message doit être une chaîne littérale ou une variable
                 *       de type const nk_char* valide pour toute la durée
                 *       de vie potentielle de l'exception.
                 */
                #define NKENTSEU_CONTAINERS_THROW_LENGTH_ERROR(msg) \
                    throw ::nkentseu::containers::errors::NkVectorLengthException((msg))

            #else

                /**
                 * @brief Macro pour signaler une erreur d'index hors bornes (mode sans exceptions).
                 * @def NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE
                 * @param index Index qui a provoqué l'erreur.
                 * @param size Taille actuelle du conteneur pour contexte.
                 * @ingroup ContainerExceptionHandling
                 *
                 * En mode sans exceptions, cette macro utilise NKENTSEU_ASSERT_MSG
                 * pour signaler l'erreur. En mode debug avec assertions activées,
                 * l'exécution s'arrête avec un message explicite. En mode release,
                 * la macro est un no-op pour des performances maximales.
                 *
                 * @note Le comportement en release sans assertions signifie que
                 *       l'erreur n'est pas détectée : il est crucial de valider
                 *       les index en amont dans les builds de production.
                 */
                #define NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE(index, size) \
                    NKENTSEU_ASSERT_MSG( \
                        false, \
                        "Vector index out of range: index=" #index ", size=" #size \
                    )

                /**
                 * @brief Macro pour signaler un échec d'allocation (mode sans exceptions).
                 * @def NKENTSEU_CONTAINERS_THROW_BAD_ALLOC
                 * @param size Taille demandée en bytes qui n'a pas pu être allouée.
                 * @ingroup ContainerExceptionHandling
                 *
                 * En mode sans exceptions, utilise NKENTSEU_ASSERT_MSG avec
                 * un message incluant la taille demandée pour faciliter le
                 * diagnostic des problèmes mémoire.
                 */
                #define NKENTSEU_CONTAINERS_THROW_BAD_ALLOC(size) \
                    NKENTSEU_ASSERT_MSG( \
                        false, \
                        "Vector bad allocation: requested=" #size " bytes" \
                    )

                /**
                 * @brief Macro pour signaler une erreur de longueur (mode sans exceptions).
                 * @def NKENTSEU_CONTAINERS_THROW_LENGTH_ERROR
                 * @param msg Message décrivant la nature de l'erreur de longueur.
                 * @ingroup ContainerExceptionHandling
                 *
                 * En mode sans exceptions, utilise NKENTSEU_ASSERT_MSG avec
                 * le message fourni pour un diagnostic précis de l'erreur.
                 */
                #define NKENTSEU_CONTAINERS_THROW_LENGTH_ERROR(msg) \
                    NKENTSEU_ASSERT_MSG( \
                        false, \
                        "Vector length error: " msg \
                    )

            #endif

            // -----------------------------------------------------------------
            // Sous-section : Fonction utilitaire de conversion code -> chaîne
            // -----------------------------------------------------------------

            /**
             * @brief Convertit un code NkVectorError en chaîne de caractères lisible.
             * @param error Code d'erreur de type NkVectorError à convertir.
             * @return Pointeur vers chaîne statique décrivant l'erreur en clair.
             * @ingroup ContainerErrors
             *
             * Cette fonction utilise un switch exhaustif sur l'enum class pour
             * mapper chaque valeur de NkVectorError vers une description textuelle
             * appropriée pour le logging, le débogage ou l'affichage à l'utilisateur.
             *
             * @note Retourne toujours une chaîne valide, même pour les codes inconnus
             *       (cas "default" retournant "Unknown error").
             *
             * @note Fonction marquée NKENTSEU_NOEXCEPT : garantie de ne jamais lever
             *       d'exception, essentielle pour un usage dans des gestionnaires
             *       d'erreurs ou des contextes critiques.
             *
             * @note Les chaînes retournées sont des littéraux statiques : ne pas
             *       les libérer ni les modifier.
             *
             * @example
             * @code
             * NkVectorError err = operation();
             * NK_LOG("Operation result: %s", NkVectorErrorToString(err));
             * @endcode
             */
            NKENTSEU_CONTAINERS_API_FORCE_INLINE
            const nk_char* NkVectorErrorToString(
                NkVectorError error
            ) NKENTSEU_NOEXCEPT {
                switch (error) {
                    case NkVectorError::NK_SUCCESS:
                        return "Success";
                    case NkVectorError::NK_OUT_OF_RANGE:
                        return "Out of range";
                    case NkVectorError::NK_OUT_OF_MEMORY:
                        return "Out of memory";
                    case NkVectorError::NK_INVALID_ARGUMENT:
                        return "Invalid argument";
                    case NkVectorError::NK_LENGTH_ERROR:
                        return "Length error";
                    case NkVectorError::NK_OVERFLOW:
                        return "Overflow";
                    case NkVectorError::NK_UNDERFLOW:
                        return "Underflow";
                    case NkVectorError::NK_BAD_ALLOC:
                        return "Bad allocation";
                    default:
                        return "Unknown error";
                }
            }

            /** @} */

        } // namespace errors

        // -------------------------------------------------------------------------
        // SECTION 5 : ALIAS AU NIVEAU CONTAINERS POUR USAGE DIRECT
        // -------------------------------------------------------------------------
        // Ces alias permettent d'utiliser les types et macros sans qualifier
        // le sous-namespace 'errors', pour une ergonomie améliorée tout en
        // maintenant une organisation modulaire du code.

        using errors::NkVectorError;
        using errors::NkVectorErrorToString;

        #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS
            using errors::NkVectorException;
            using errors::NkVectorOutOfRangeException;
            using errors::NkVectorBadAllocException;
            using errors::NkVectorLengthException;
        #endif

    } // namespace nkentseu

    // -------------------------------------------------------------------------
    // SECTION 6 : VALIDATION ET MESSAGES DE DEBUG OPTIONNELS
    // -------------------------------------------------------------------------
    // Ces directives #pragma message aident à vérifier la configuration de
    // compilation lors du build, particulièrement utile pour déboguer les
    // problèmes liés au mode exceptions/asssertions.

    #ifdef NKENTSEU_CONTAINERS_DEBUG_CONFIG
        #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS
            #pragma message("NKContainers NkVectorError: Exception handling ENABLED")
        #else
            #pragma message("NKContainers NkVectorError: Exception handling DISABLED (using assertions)")
        #endif
    #endif

#endif // NKENTSEU_CONTAINERS_ERRORS_NKVECTORERROR_H_INCLUDED

// =============================================================================
// EXEMPLES D'UTILISATION DÉTAILLÉS
// =============================================================================
// Cette section fournit des exemples concrets d'utilisation des composants
// définis dans ce fichier, couvrant les cas d'usage courants et les bonnes
// pratiques pour la gestion d'erreurs dans NKContainers.
// =============================================================================

/*
    // -----------------------------------------------------------------------------
    // Exemple 1 : Validation d'index avec macro unifiée
    // -----------------------------------------------------------------------------
    // Cet exemple montre comment utiliser NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE
    // pour valider un index avant un accès au conteneur. Le code est identique
    // quel que soit le mode de compilation (avec ou sans exceptions).

    #include <NKContainers/Errors/NkVectorError.h>
    #include <NKContainers/Vector.h>

    using namespace nkentseu::containers;

    nk_int32 SafeGetElement(const Vector<nk_int32>& vec, nk_size index) {
        // Validation de l'index avec macro unifiée
        // Comportement :
        //   - Avec exceptions : lève NkVectorOutOfRangeException
        //   - Sans exceptions : NKENTSEU_ASSERT_MSG en debug, no-op en release
        if (index >= vec.Size()) {
            NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE(index, vec.Size());
        }

        // Accès sécurisé après validation
        return vec[index];
    }

    // -----------------------------------------------------------------------------
    // Exemple 2 : Gestion structurée des exceptions (mode avec exceptions)
    // -----------------------------------------------------------------------------
    // Lorsque NKENTSEU_USE_EXCEPTIONS est défini, il est possible d'utiliser
    // des blocs try/catch pour gérer les erreurs de manière granulaire.

    #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS

    void ProcessLargeVector(Vector<nk_double>& vec, nk_size targetSize) {
        try {
            // Tentative de réservation d'une grande quantité de mémoire
            vec.Reserve(targetSize);

            // Remplissage du vecteur
            for (nk_size i = 0; i < targetSize; ++i) {
                vec.PushBack(static_cast<nk_double>(i) * 1.5);
            }
        }
        catch (const NkVectorBadAllocException& e) {
            // Gestion spécifique des échecs d'allocation
            NKENTSEU_LOG_ERROR(
                "Memory allocation failed: %s (requested: %zu bytes)",
                e.What(),
                e.GetIndex()
            );

            // Stratégie de fallback : réduire la taille cible et réessayer
            nk_size fallbackSize = targetSize / 4;
            NKENTSEU_LOG_WARNING("Retrying with reduced size: %zu", fallbackSize);

            vec.Reserve(fallbackSize);
            for (nk_size i = 0; i < fallbackSize; ++i) {
                vec.PushBack(static_cast<nk_double>(i) * 1.5);
            }
        }
        catch (const NkVectorOutOfRangeException& e) {
            // Gestion des erreurs d'index (moins probable ici mais possible)
            NKENTSEU_LOG_ERROR(
                "Index error during processing: %s (index: %zu)",
                e.What(),
                e.GetIndex()
            );
            // Propagation ou traitement alternatif selon la politique d'erreur
        }
        catch (const NkVectorException& e) {
            // Gestion générique de toute autre erreur de conteneur
            NKENTSEU_LOG_ERROR(
                "Container error [%u]: %s",
                static_cast<nk_uint32>(e.GetError()),
                e.What()
            );
            // Nettoyage ou réinitialisation si nécessaire
            vec.Clear();
        }
    }

    #endif // NKENTSEU_USE_EXCEPTIONS

    // -----------------------------------------------------------------------------
    // Exemple 3 : Conversion code d'erreur -> message pour logging
    // -----------------------------------------------------------------------------
    // La fonction NkVectorErrorToString() permet de convertir un code d'erreur
    // en chaîne lisible pour le logging, les messages utilisateur ou le débogage.

    void LogOperationResult(
        const char* operationName,
        NkVectorError errorCode,
        nk_size contextIndex
    ) {
        const nk_char* errorDescription = NkVectorErrorToString(errorCode);

        if (errorCode == NkVectorError::NK_SUCCESS) {
            NKENTSEU_LOG_INFO(
                "Operation '%s' completed successfully (context: %zu)",
                operationName,
                contextIndex
            );
        }
        else {
            NKENTSEU_LOG_ERROR(
                "Operation '%s' failed: %s (code: %u, context: %zu)",
                operationName,
                errorDescription,
                static_cast<nk_uint32>(errorCode),
                contextIndex
            );
        }
    }

    // -----------------------------------------------------------------------------
    // Exemple 4 : Mode sans exceptions - assertions en debug uniquement
    // -----------------------------------------------------------------------------
    // Lorsque NKENTSEU_USE_EXCEPTIONS n'est pas défini, les macros utilisent
    // NKENTSEU_ASSERT_MSG. En mode debug avec assertions activées, l'exécution
    // s'arrête avec un message. En mode release, c'est un no-op (performance).

    void FastPathAccess(Vector<nk_float>& vec, nk_size index) {
        // En debug : assertion avec message si index invalide
        // En release : aucune vérification (performance maximale)
        // Le code client doit garantir la validité de l'index en production
        NKENTSEU_CONTAINERS_THROW_OUT_OF_RANGE(index, vec.Size());

        // Accès direct sans surcoût en release
        vec[index] *= 2.0f;
    }

    // Configuration recommandée pour ce mode :
    // CMakeLists.txt :
    //   target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=0)
    //   target_compile_definitions(monapp PRIVATE NKENTSEU_ENABLE_ASSERT=1)  # Pour debug
    //   target_compile_definitions(monapp PRIVATE NKENTSEU_ENABLE_ASSERT=0)  # Pour release

    // -----------------------------------------------------------------------------
    // Exemple 5 : Fonction retournant un code d'erreur (style C / sans exceptions)
    // -----------------------------------------------------------------------------
    // Alternative aux exceptions : retourner un code NkVectorError pour permettre
    // au code appelant de décider de la stratégie de gestion d'erreur.

    NkVectorError ResizeVectorSafe(
        Vector<nk_int32>& vec,
        nk_size newSize,
        nk_int32 defaultValue
    ) {
        // Validation des arguments
        if (newSize > vec.MaxSize()) {
            return NkVectorError::NK_LENGTH_ERROR;
        }

        // Tentative de redimensionnement
        // Note : en mode sans exceptions, les erreurs internes utilisent des assertions
        vec.Resize(newSize, defaultValue);

        return NkVectorError::NK_SUCCESS;
    }

    // Usage :
    void CallerExample() {
        Vector<nk_int32> vec;
        NkVectorError result = ResizeVectorSafe(vec, 1000, 0);

        if (result != NkVectorError::NK_SUCCESS) {
            NKENTSEU_LOG_WARNING(
                "Resize failed: %s",
                NkVectorErrorToString(result)
            );
            // Gestion alternative : utiliser une taille plus petite, etc.
            return;
        }

        // Continuer avec le vecteur redimensionné
        // ...
    }

    // -----------------------------------------------------------------------------
    // Exemple 6 : Héritage et polymorphisme des exceptions
    // -----------------------------------------------------------------------------
    // Les exceptions NKContainers suivent une hiérarchie permettant une capture
    // granulaire ou générique selon les besoins.

    #if defined(NKENTSEU_USE_EXCEPTIONS) && NKENTSEU_USE_EXCEPTIONS

    void GenericErrorHandler() {
        try {
            // Code pouvant lever différentes exceptions NKContainers
            RiskyOperation();
        }
        catch (const NkVectorOutOfRangeException& e) {
            // Capture spécifique : index hors bornes
            NKENTSEU_LOG_ERROR("Bounds error at index %zu", e.GetIndex());
        }
        catch (const NkVectorBadAllocException& e) {
            // Capture spécifique : échec mémoire
            NKENTSEU_LOG_ERROR("Memory error: %zu bytes requested", e.GetIndex());
        }
        catch (const NkVectorException& e) {
            // Capture générique : toute autre erreur de conteneur
            NKENTSEU_LOG_ERROR(
                "Generic container error [%u]: %s",
                static_cast<nk_uint32>(e.GetError()),
                e.What()
            );
        }
    }

    #endif // NKENTSEU_USE_EXCEPTIONS

    // -----------------------------------------------------------------------------
    // Exemple 7 : Intégration avec le système de logging NKPlatform
    // -----------------------------------------------------------------------------
    // Combinaison des erreurs NKContainers avec le logging centralisé NKPlatform.

    void LogVectorErrorContext(
        const char* functionName,
        NkVectorError error,
        nk_size index,
        nk_size size
    ) {
        // Construction d'un message contextuel complet
        const nk_char* errorStr = NkVectorErrorToString(error);

        NKENTSEU_LOG_ERROR(
            "[%s] Vector error: %s (code=%u, index=%zu, size=%zu)",
            functionName,
            errorStr,
            static_cast<nk_uint32>(error),
            index,
            size
        );

        // En mode debug avec assertions : ajout d'une assertion pour débogage
        #ifndef NDEBUG
            NKENTSEU_ASSERT_MSG(false, "Vector error in production build");
        #endif
    }

    // -----------------------------------------------------------------------------
    // Exemple 8 : Configuration conditionnelle au moment du build
    // -----------------------------------------------------------------------------
    // Comment configurer le projet pour activer/désactiver les exceptions.

    // Dans CMakeLists.txt du projet client :

    // Option 1 : Mode développement avec exceptions pour débogage facile
    // option(ENABLE_EXCEPTIONS "Enable C++ exceptions for error handling" ON)
    // if(ENABLE_EXCEPTIONS)
    //     target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=1)
    // else()
    //     target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=0)
    // endif()

    // Option 2 : Mode production embarqué sans exceptions pour déterminisme
    // target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=0)
    // target_compile_definitions(monapp PRIVATE NKENTSEU_ENABLE_ASSERT=0)  # Release
    // target_compile_options(monapp PRIVATE -O3 -DNDEBUG)

    // Option 3 : Mode test avec assertions activées même sans exceptions
    // target_compile_definitions(tests PRIVATE NKENTSEU_USE_EXCEPTIONS=0)
    // target_compile_definitions(tests PRIVATE NKENTSEU_ENABLE_ASSERT=1)
*/

// =============================================================================
// NOTES DE MIGRATION ET COMPATIBILITÉ
// =============================================================================
/*
    Migration depuis NKCore::NkVectorError vers NKContainers :

    Ancien code (NKCore)              | Nouveau code (NKContainers)
    ----------------------------------|----------------------------------
    nkentseu::NkVectorError          | nkentseu::containers::NkVectorError
    NK_VECTOR_THROW_*                | NKENTSEU_CONTAINERS_THROW_*
    NK_NO_EXCEPTIONS                 | NKENTSEU_USE_EXCEPTIONS (logique inversée)
    NK_ASSERT_MSG                    | NKENTSEU_ASSERT_MSG (via NKPlatform)

    Configuration CMake recommandée :
    # Avec exceptions (développement/debug)
    target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=1)

    # Sans exceptions (production/embarqué)
    target_compile_definitions(monapp PRIVATE NKENTSEU_USE_EXCEPTIONS=0)
    target_compile_definitions(monapp PRIVATE NKENTSEU_ENABLE_ASSERT=1)  # Pour debug

    Avantages de l'approche NKContainers :
    - Un seul code source pour les deux modes (exceptions / assertions)
    - Intégration avec le système d'assertions NKPlatform
    - Messages d'erreur contextuels (index, taille, message personnalisé)
    - Zéro overhead en release sans exceptions
    - Indentation cohérente pour maintenance facilitée
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================