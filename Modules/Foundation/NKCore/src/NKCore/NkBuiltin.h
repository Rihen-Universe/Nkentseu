// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\NkBuiltin.h
// DESCRIPTION: Macros pour fonctionnalités intégrées du compilateur
// AUTEUR: Rihen
// DATE: 2026-02-08
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKBUILTIN_H_INCLUDED
#define NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKBUILTIN_H_INCLUDED

// ============================================================
// MACROS POUR FONCTIONNALITÉS INTÉGRÉES DU COMPILATEUR
// ============================================================

/**
 * @defgroup BuiltinMacros Macros de Fonctionnalités Intégrées
 * @brief Macros pour accéder aux fonctionnalités spécifiques du compilateur
 */

/**
 * @brief Nom du fichier source courant
 * @def NKENTSEU_BUILTIN_FILE
 * @ingroup BuiltinMacros
 */
#define NKENTSEU_BUILTIN_FILE __FILE__

/**
 * @brief Numéro de ligne courante
 * @def NKENTSEU_BUILTIN_LINE
 * @ingroup BuiltinMacros
 */
#define NKENTSEU_BUILTIN_LINE __LINE__

/**
 * @brief Nom de la fonction courante (dépend du compilateur)
 * @def NKENTSEU_BUILTIN_FUNCTION
 * @note Cette macro ne peut être utilisée qu'à l'intérieur d'une fonction
 * @warning L'utilisation en dehors d'une fonction peut causer des erreurs
 * @ingroup BuiltinMacros
 */
#ifdef __cplusplus
// C++: utilise __FUNCTION__ ou équivalent
#if defined(_MSC_VER)
#define NKENTSEU_BUILTIN_FUNCTION __FUNCTION__
#elif defined(__GNUC__) || defined(__clang__)
#define NKENTSEU_BUILTIN_FUNCTION __PRETTY_FUNCTION__
#else
#define NKENTSEU_BUILTIN_FUNCTION __func__
#endif
#else
// C: utilise __func__ (standard C99)
#if __STDC_VERSION__ >= 199901L
#define NKENTSEU_BUILTIN_FUNCTION __func__
#else
// Fallback pour anciens compilateurs C
#define NKENTSEU_BUILTIN_FUNCTION ""
#endif
#endif

/**
 * @brief Date de compilation courante
 * @def NKENTSEU_BUILTIN_DATE
 * @ingroup BuiltinMacros
 */
#define NKENTSEU_BUILTIN_DATE __DATE__

/**
 * @brief Heure de compilation courante
 * @def NKENTSEU_BUILTIN_TIME
 * @ingroup BuiltinMacros
 */
#define NKENTSEU_BUILTIN_TIME __TIME__

/**
 * @brief Timestamp de compilation
 * @def NKENTSEU_BUILTIN_TIMESTAMP
 * @ingroup BuiltinMacros
 */
#define NKENTSEU_BUILTIN_TIMESTAMP __DATE__ " " __TIME__

// ============================================================
// MACROS POUR INFORMATIONS DE COMPILATION
// ============================================================

/**
 * @defgroup CompileInfoMacros Macros d'Informations de Compilation
 * @brief Macros pour obtenir des informations sur la compilation
 */

/**
 * @brief Macro de base pour créer un identifiant unique
 * @def NKENTSEU_UNIQUE_ID
 * @ingroup CompileInfoMacros
 */
#define NKENTSEU_UNIQUE_ID __LINE__

/**
 * @brief Crée un identifiant unique avec préfixe
 * @def NKENTSEU_UNIQUE_NAME(prefix)
 * @param prefix Préfixe pour l'identifiant
 * @ingroup CompileInfoMacros
 */
#define NKENTSEU_UNIQUE_NAME(prefix) NKENTSEU_CONCAT(prefix, NKENTSEU_UNIQUE_ID)

// ============================================================
// MACROS DE DÉBOGAGE AMÉLIORÉES
// ============================================================

/**
 * @defgroup DebugMacros Macros de Débogage
 * @brief Macros pour faciliter le débogage
 */

/**
 * @brief Affiche un message de compilation
 * @def NKENTSEU_COMPILE_MESSAGE(msg)
 * @param msg Message à afficher
 * @ingroup DebugMacros
 */
#if defined(_MSC_VER)
#define NKENTSEU_COMPILE_MESSAGE(msg) __pragma(message(msg))
#elif defined(__GNUC__) || defined(__clang__)
#define NKENTSEU_COMPILE_MESSAGE(msg) _Pragma(NKENTSEU_STRINGIFY(message msg))
#else
#define NKENTSEU_COMPILE_MESSAGE(msg)
#endif

/**
 * @brief Message TODO avec localisation
 * @def NKENTSEU_TODO(msg)
 * @param msg Description de la tâche TODO
 * @ingroup DebugMacros
 */
#define NKENTSEU_TODO(msg)                                                                                             \
	NKENTSEU_COMPILE_MESSAGE("TODO at " NKENTSEU_BUILTIN_FILE ":" NKENTSEU_STRINGIFY(NKENTSEU_BUILTIN_LINE) ": " msg)

/**
 * @brief Message FIXME avec localisation
 * @def NKENTSEU_FIXME(msg)
 * @param msg Description du problème à corriger
 * @ingroup DebugMacros
 */
#define NKENTSEU_FIXME(msg)                                                                                            \
	NKENTSEU_COMPILE_MESSAGE("FIXME at " NKENTSEU_BUILTIN_FILE ":" NKENTSEU_STRINGIFY(NKENTSEU_BUILTIN_LINE) ": " msg)

/**
 * @brief Message NOTE avec localisation
 * @def NKENTSEU_NOTE(msg)
 * @param msg Note informative
 * @ingroup DebugMacros
 */
#define NKENTSEU_NOTE(msg)                                                                                             \
	NKENTSEU_COMPILE_MESSAGE("NOTE at " NKENTSEU_BUILTIN_FILE ":" NKENTSEU_STRINGIFY(NKENTSEU_BUILTIN_LINE) ": " msg)

// ============================================================
// MACROS POUR ASSERTIONS AMÉLIORÉES
// ============================================================

/**
 * @defgroup AssertMacros Macros d'Assertion
 * @brief Macros pour assertions avec informations contextuelles
 */

/**
 * @brief Assertion basique avec informations contextuelles
 * @def NKENTSEU_SIMPLE_ASSERT(condition)
 * @param condition Condition à vérifier
 * @ingroup AssertMacros
 */
#define NKENTSEU_SIMPLE_ASSERT(condition)                                                                              \
	do {                                                                                                               \
		if (!(condition)) {                                                                                            \
			/* Log ou traitement d'erreur ici */                                                                       \
		}                                                                                                              \
	} while (0)

/**
 * @brief Assertion avec message personnalisé
 * @def NKENTSEU_ASSERT_MSG(condition, msg)
 * @param condition Condition à vérifier
 * @param msg Message d'erreur
 * @ingroup AssertMacros
 */
#define NKENTSEU_ASSERT_MSG(condition, msg)                                                                            \
	do {                                                                                                               \
		if (!(condition)) {                                                                                            \
			/* Log avec message */                                                                                     \
		}                                                                                                              \
	} while (0)

// ============================================================
// MACROS POUR PROFILING ET INSTRUMENTATION
// ============================================================

/**
 * @defgroup ProfileMacros Macros de Profiling
 * @brief Macros pour le profiling et l'instrumentation
 */

/**
 * @brief Marque une section de code pour le profiling
 * @def NKENTSEU_PROFILE_SCOPE(name)
 * @param name Nom de la section de profiling
 * @ingroup ProfileMacros
 */
#if defined(NKENTSEU_ENABLE_PROFILING)
#define NKENTSEU_PROFILE_SCOPE(name) nkentseu::debug::ProfileScope NKENTSEU_UNIQUE_NAME(profile_scope_)(name)
#else
#define NKENTSEU_PROFILE_SCOPE(name) ((void)0)
#endif

/**
 * @brief Marque un point d'entrée/sortie pour l'instrumentation
 * @def NKENTSEU_INSTRUMENT_FUNCTION
 * @ingroup ProfileMacros
 */
#if defined(NKENTSEU_ENABLE_INSTRUMENTATION)
#define NKENTSEU_INSTRUMENT_FUNCTION                                                                                   \
	nkentseu::debug::InstrumentFunction NKENTSEU_UNIQUE_NAME(instrument_)(NKENTSEU_BUILTIN_FUNCTION)
#else
#define NKENTSEU_INSTRUMENT_FUNCTION ((void)0)
#endif

// ============================================================
// MACROS POUR GESTION DES ERREURS
// ============================================================

/**
 * @defgroup ErrorMacros Macros de Gestion d'Erreurs
 * @brief Macros pour la gestion centralisée des erreurs
 */

/**
 * @brief Macro pour logger une erreur avec contexte
 * @def NKENTSEU_LOG_ERROR(msg)
 * @param msg Message d'erreur
 * @ingroup ErrorMacros
 */
#define NKENTSEU_LOG_ERROR(msg)                                                                                        \
	NKENTSEU_LOG_ERROR_CONTEXT(msg, NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_FUNCTION)

/**
 * @brief Macro pour logger un avertissement avec contexte
 * @def NKENTSEU_LOG_WARNING(msg)
 * @param msg Message d'avertissement
 * @ingroup ErrorMacros
 */
#define NKENTSEU_LOG_WARNING(msg)                                                                                      \
	NKENTSEU_LOG_WARNING_CONTEXT(msg, NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_FUNCTION)

/**
 * @brief Macro pour logger une information avec contexte
 * @def NKENTSEU_LOG_INFO(msg)
 * @param msg Message d'information
 * @ingroup ErrorMacros
 */
#define NKENTSEU_LOG_INFO(msg)                                                                                         \
	NKENTSEU_LOG_INFO_CONTEXT(msg, NKENTSEU_BUILTIN_FILE, NKENTSEU_BUILTIN_LINE, NKENTSEU_BUILTIN_FUNCTION)

// Ces macros doivent être implémentées dans le système de logging
#define NKENTSEU_LOG_ERROR_CONTEXT(msg, file, line, func) nkentseu::log::Error(msg, file, line, func)

#define NKENTSEU_LOG_WARNING_CONTEXT(msg, file, line, func) nkentseu::log::Warning(msg, file, line, func)

#define NKENTSEU_LOG_INFO_CONTEXT(msg, file, line, func) nkentseu::log::Info(msg, file, line, func)

// ============================================================
// MACROS UTILITAIRES AVEC CONTEXTE
// ============================================================

/**
 * @defgroup ContextMacros Macros avec Contexte
 * @brief Macros qui incluent automatiquement le contexte
 */

/**
 * @brief Définit une variable locale avec contexte de débogage
 * @def NKENTSEU_DECLARE_WITH_CONTEXT(type, name, value)
 * @param type Type de la variable
 * @param name Nom de la variable
 * @param value Valeur initiale
 * @ingroup ContextMacros
 */
#define NKENTSEU_DECLARE_WITH_CONTEXT(type, name, value)                                                               \
	type name = (value);                                                                                               \
	NKENTSEU_UNUSED(name) // Pour éviter les warnings si non utilisé

/**
 * @brief Vérifie une condition et retourne si fausse
 * @def NKENTSEU_CHECK_RETURN(condition, retval)
 * @param condition Condition à vérifier
 * @param retval Valeur de retour si condition fausse
 * @ingroup ContextMacros
 */
#define NKENTSEU_CHECK_RETURN(condition, retval)                                                                       \
	do {                                                                                                               \
		if (!(condition)) {                                                                                            \
			NKENTSEU_LOG_WARNING("Check failed: " #condition);                                                         \
			return retval;                                                                                             \
		}                                                                                                              \
	} while (0)

/**
 * @brief Vérifie une condition et continue si fausse
 * @def NKENTSEU_CHECK_CONTINUE(condition)
 * @param condition Condition à vérifier
 * @ingroup ContextMacros
 */
#define NKENTSEU_CHECK_CONTINUE(condition)                                                                             \
	do {                                                                                                               \
		if (!(condition)) {                                                                                            \
			NKENTSEU_LOG_WARNING("Check failed: " #condition);                                                         \
			continue;                                                                                                  \
		}                                                                                                              \
	} while (0)

// ============================================================
// MACROS POUR TESTS UNITAIRE
// ============================================================

/**
 * @defgroup TestMacros Macros pour Tests Unitaires
 * @brief Macros pour faciliter les tests unitaires
 */

/**
 * @brief Assertion de test avec message
 * @def NKENTSEU_TEST_ASSERT(condition, msg)
 * @param condition Condition à tester
 * @param msg Message en cas d'échec
 * @ingroup TestMacros
 */
#define NKENTSEU_TEST_ASSERT(condition, msg)                                                                           \
	do {                                                                                                               \
		if (!(condition)) {                                                                                            \
			NKENTSEU_LOG_ERROR("Test failed: " msg);                                                                   \
			return false;                                                                                              \
		}                                                                                                              \
	} while (0)

/**
 * @brief Égalité de test avec valeurs
 * @def NKENTSEU_TEST_EQUAL(actual, expected, msg)
 * @param actual Valeur actuelle
 * @param expected Valeur attendue
 * @param msg Message en cas d'échec
 * @ingroup TestMacros
 */
#define NKENTSEU_TEST_EQUAL(actual, expected, msg)                                                                     \
	NKENTSEU_TEST_ASSERT((actual) == (expected),                                                                       \
						 msg " (actual: " NKENTSEU_STRINGIFY(actual) ", expected: " NKENTSEU_STRINGIFY(expected) ")")

// ============================================================
// NAMESPACE POUR CLASSES D'AIDE
// ============================================================

namespace nkentseu {
/**
 * @brief Namespace debug.
 */
namespace debug {

#if defined(NKENTSEU_ENABLE_PROFILING)
/**
 * @class ProfileScope
 * @brief Classe utilitaire pour le profiling scope-based
 */
class ProfileScope {
public:
	explicit ProfileScope(const char *name) : m_name(name) {
		// Début du profiling
	}

	~ProfileScope() {
		// Fin du profiling
	}

private:
	const char *m_name;
};
#endif

#if defined(NKENTSEU_ENABLE_INSTRUMENTATION)
/**
 * @class InstrumentFunction
 * @brief Classe pour l'instrumentation de fonctions
 */
class InstrumentFunction {
public:
	explicit InstrumentFunction(const char *functionName) : m_functionName(functionName) {
		// Entrée dans la fonction
	}

	~InstrumentFunction() {
		// Sortie de la fonction
	}

private:
	const char *m_functionName;
};
#endif

} // namespace debug

/**
 * @brief Namespace log.
 */
namespace log {

// Fonctions de logging simplifiées
inline void Error(const char *msg, const char *file, int line, const char *func) {
	// Implémentation du logging d'erreur
}

inline void Warning(const char *msg, const char *file, int line, const char *func) {
	// Implémentation du logging d'avertissement
}

inline void Info(const char *msg, const char *file, int line, const char *func) {
	// Implémentation du logging d'information
}

} // namespace log
} // namespace nkentseu

// ============================================================
// MACROS DE COMPATIBILITÉ
// ============================================================

/**
 * @defgroup CompatibilityMacros Macros de Compatibilité
 * @brief Macros pour la compatibilité entre compilateurs
 */

/**
 * @brief Macro pour fonction actuelle (compatibilité)
 * @def NKENTSEU_CURRENT_FUNCTION
 * @note Préférer NKENTSEU_BUILTIN_FUNCTION
 * @ingroup CompatibilityMacros
 */
#define NKENTSEU_CURRENT_FUNCTION NKENTSEU_BUILTIN_FUNCTION

/**
 * @brief Macro pour fichier actuel (compatibilité)
 * @def NKENTSEU_CURRENT_FILE
 * @ingroup CompatibilityMacros
 */
#define NKENTSEU_CURRENT_FILE NKENTSEU_BUILTIN_FILE

/**
 * @brief Macro pour ligne actuelle (compatibilité)
 * @def NKENTSEU_CURRENT_LINE
 * @ingroup CompatibilityMacros
 */
#define NKENTSEU_CURRENT_LINE NKENTSEU_BUILTIN_LINE

#endif // NKENTSEU_CORE_NKCORE_SRC_NKCORE_NKBUILTIN_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Propriété intellectuelle - Libre d'utilisation et modification
// ============================================================