// =============================================================================
// NKLogger/Sinks/NkConsoleSink.h
// Sink pour la sortie console (stdout/stderr) avec support des couleurs.
//
// Design :
//  - Implémentation concrète de NkISink pour sortie vers terminal/console
//  - Support multiplateforme : ANSI sur Unix/Linux/macOS, Win32 API sur Windows
//  - Routage Android vers logcat via __android_log_print
//  - Couleur conditionnelle : détection automatique du support terminal
//  - Routage stderr pour erreurs : option configurable pour séparation flux
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la classe
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Propriétaire - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKCONSOLESINK_H
#define NKENTSEU_NKCONSOLESINK_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour la gestion de la console et du formatage.
	// Dépendances projet pour l'interface de sink, le formatage et la synchronisation.

	#include <cstdio>

	#include "NKCore/NkTypes.h"
	#include "NKContainers/String/NkString.h"
	#include "NKMemory/NkUniquePtr.h"
	#include "NKThreading/NkMutex.h"
	#include "NKLogger/NkSink.h"
	#include "NKLogger/NkLogLevel.h"
	#include "NKLogger/NkLogMessage.h"
	#include "NKLogger/NkLoggerFormatter.h"
	#include "NKLogger/NkLoggerApi.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// ÉNUMÉRATION : NkConsoleStream
		// DESCRIPTION : Flux de console disponibles pour la sortie des logs
		// ---------------------------------------------------------------------
		/**
		 * @enum NkConsoleStream
		 * @brief Flux de sortie console supportés par NkConsoleSink
		 * @ingroup LoggerTypes
		 *
		 * Cette énumération définit les destinations de sortie possibles
		 * pour les messages de log vers la console.
		 *
		 * @note Le choix du flux affecte le buffering et la visibilité :
		 *   - stdout : buffering ligne par défaut, sortie normale
		 *   - stderr : buffering non-buffered par défaut, sortie d'erreur
		 *
		 * @example
		 * @code
		 * // Sortie normale vers stdout
		 * nkentseu::NkConsoleSink infoSink(nkentseu::NkConsoleStream::NK_STD_OUT);
		 *
		 * // Sortie d'erreur vers stderr (visible même si stdout redirigé)
		 * nkentseu::NkConsoleSink errorSink(nkentseu::NkConsoleStream::NK_STD_ERR);
		 * @endcode
		 */
		enum class NKENTSEU_LOGGER_CLASS_EXPORT NkConsoleStream : uint8 {
			/// @brief Sortie standard (stdout) : pour logs informatifs normaux
			NK_STD_OUT = 0,

			/// @brief Erreur standard (stderr) : pour logs d'erreur et diagnostics
			NK_STD_ERR = 1
		};


		// ---------------------------------------------------------------------
		// CLASSE : NkConsoleSink
		// DESCRIPTION : Sink pour sortie console avec support couleur multiplateforme
		// ---------------------------------------------------------------------
		/**
		 * @class NkConsoleSink
		 * @brief Sink pour l'écriture des messages de log vers la console avec couleurs
		 * @ingroup LoggerSinks
		 *
		 * NkConsoleSink fournit une destination de log vers la console avec :
		 *  - Support multiplateforme : ANSI (Unix) / Win32 API (Windows) / logcat (Android)
		 *  - Détection automatique du support couleur via isatty()/GetConsoleMode()
		 *  - Routage configurable : stdout pour info, stderr pour erreurs (optionnel)
		 *  - Formatage via NkLoggerFormatter avec support des marqueurs de couleur %^/%$
		 *  - Thread-safe : synchronisation via mutex interne pour écritures concurrentes
		 *
		 * Architecture :
		 *  - Hérite de NkISink : compatibilité avec l'API de logging existante
		 *  - Utilise NkLoggerFormatter pour formatage des messages avant écriture
		 *  - Gère les couleurs via NkLogLevelToANSIColor/WindowsColor centralisés
		 *  - Routage Android : redirection transparente vers __android_log_print
		 *
		 * Thread-safety :
		 *  - Toutes les méthodes publiques sont thread-safe via m_Mutex
		 *  - Les écritures console sont atomic vis-à-vis des autres threads
		 *  - Safe pour usage depuis multiples threads simultanément
		 *
		 * Gestion des couleurs :
		 *  - Détection automatique : isatty() + TERM sur Unix, GetConsoleMode sur Windows
		 *  - Désactivation sur Android : logcat ne supporte pas les codes ANSI
		 *  - Override manuel : SetColorEnabled() pour forcer activer/désactiver
		 *
		 * @note Sur Windows, ENABLE_VIRTUAL_TERMINAL_PROCESSING doit être activé
		 *       pour le support des codes ANSI. Sinon, fallback vers Win32 API.
		 *
		 * @example Usage basique avec couleurs
		 * @code
		 * // Sink console avec couleurs activées (détection automatique)
		 * auto consoleSink = nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>();
		 * consoleSink->SetPattern("[%^%L%$] %v");  // Pattern avec marqueurs couleur
		 *
		 * // Ajout au logger global
		 * nkentseu::NkLog::Instance().AddSink(consoleSink);
		 *
		 * // Logging : couleurs appliquées si terminal supporté
		 * logger.Info("Information message");    // Vert
		 * logger.Warn("Warning message");        // Jaune
		 * logger.Error("Error message");         // Rouge
		 * @endcode
		 *
		 * @example Routage stderr pour erreurs
		 * @code
		 * // Séparer flux normaux et erreurs pour scripting/shell
		 * auto consoleSink = nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>(
		 *     nkentseu::NkConsoleStream::NK_STD_OUT,  // Flux principal : stdout
		 *     true                                     // Couleurs activées
		 * );
		 * consoleSink->SetUseStderrForErrors(true);   // Erreurs vers stderr
		 *
		 * // Usage shell :
		 * // ./app > output.log 2> errors.log
		 * // → Logs normaux dans output.log, erreurs dans errors.log
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkConsoleSink : public NkISink {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup ConsoleSinkCtors Constructeurs de NkConsoleSink
			 * @brief Initialisation avec configuration du flux et des couleurs
			 */

			/**
			 * @brief Constructeur par défaut : stdout avec couleurs activées
			 * @ingroup ConsoleSinkCtors
			 *
			 * @post m_Stream = NK_STD_OUT, m_UseColors = true, m_UseStderrForErrors = true
			 * @post Formatter configuré avec NK_COLOR_PATTERN (support marqueurs %^/%$)
			 * @note Thread-safe : construction sans verrouillage requis (premier accès)
			 *
			 * @example
			 * @code
			 * // Construction par défaut pour usage standard
			 * auto consoleSink = nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>();
			 * logger.AddSink(consoleSink);
			 * @endcode
			 */
			NkConsoleSink();

			/**
			 * @brief Constructeur avec flux et option de couleurs personnalisés
			 * @ingroup ConsoleSinkCtors
			 *
			 * @param stream Flux de sortie à utiliser (NK_STD_OUT ou NK_STD_ERR)
			 * @param useColors true pour activer les couleurs si supporté, false pour désactiver
			 * @post m_Stream, m_UseColors configurés selon paramètres
			 * @post m_UseStderrForErrors = true par défaut (modifiable via SetUseStderrForErrors)
			 * @post Formatter configuré avec NK_COLOR_PATTERN si useColors=true, sinon NK_DEFAULT_PATTERN
			 * @note Thread-safe : construction sans verrouillage requis
			 *
			 * @example
			 * @code
			 * // Sink vers stderr sans couleurs (pour logs parsables)
			 * auto errorSink = nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>(
			 *     nkentseu::NkConsoleStream::NK_STD_ERR,
			 *     false  // Pas de couleurs pour parsing facile
			 * );
			 * @endcode
			 */
			explicit NkConsoleSink(NkConsoleStream stream, bool useColors = true);

			/**
			 * @brief Destructeur : flush garanti avant destruction
			 * @ingroup ConsoleSinkCtors
			 *
			 * @post Flush() appelé pour persistance des buffers console
			 * @note Garantit qu'aucun message en buffer n'est perdu à la destruction
			 * @note Thread-safe : acquisition du mutex avant flush
			 */
			~NkConsoleSink() override;


			// -----------------------------------------------------------------
			// IMPLÉMENTATION DE L'INTERFACE NKISINK
			// -----------------------------------------------------------------
			/**
			 * @defgroup ConsoleSinkInterface Implémentation de NkISink
			 * @brief Méthodes requises avec logique de sortie console
			 */

			/**
			 * @brief Écrit un message formaté vers la console appropriée
			 * @param message Message de log contenant les données et métadonnées
			 * @ingroup ConsoleSinkInterface
			 *
			 * @note Filtrage précoce : retour immédiat si !IsEnabled() ou !ShouldLog()
			 * @note Formatage via m_Formatter->Format() avec couleurs si supporté
			 * @note Routage Android : redirection vers __android_log_print via logcat
			 * @note Routage Unix/Windows : fwrite vers stdout/stderr selon niveau et config
			 * @note Flush automatique pour niveaux >= NK_ERROR pour visibilité immédiate
			 * @note Thread-safe : acquisition du mutex pour écriture exclusive
			 *
			 * @par Comportement par plateforme :
			 *  - Android : __android_log_print avec tag dérivé de loggerName
			 *  - Windows : fwrite + Win32 SetConsoleTextAttribute si couleurs activées
			 *  - Unix : fwrite + codes ANSI si terminal supporte les couleurs
			 *
			 * @example
			 * @code
			 * nkentseu::NkLogMessage msg(
			 *     nkentseu::NkLogLevel::NK_INFO,
			 *     "Console output with optional colors"
			 * );
			 * consoleSink.Log(msg);  // Écriture thread-safe avec formatage
			 * @endcode
			 */
			void Log(const NkLogMessage& message) override;

			/**
			 * @brief Force l'écriture immédiate des buffers console
			 * @ingroup ConsoleSinkInterface
			 *
			 * @post fflush() appelé sur stdout et/ou stderr selon configuration
			 * @note Critique pour : debugging temps-réel, visibilité immédiate des erreurs
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Sur Android : no-op car logcat gère son propre buffering
			 *
			 * @example
			 * @code
			 * // Flush après message critique pour visibilité immédiate
			 * logger.Error("Critical failure detected");
			 * consoleSink.Flush();  // Force écriture console immédiate
			 * @endcode
			 */
			void Flush() override;

			/**
			 * @brief Définit le formatter utilisé par ce sink
			 * @param formatter Pointeur unique vers le nouveau formatter à adopter
			 * @ingroup ConsoleSinkInterface
			 *
			 * @note Transfert de propriété : le sink devient propriétaire du formatter
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Le formatter précédent est automatiquement détruit (RAII)
			 *
			 * @example
			 * @code
			 * // Formatter personnalisé pour console compacte
			 * auto fmt = nkentseu::memory::MakeUnique<nkentseu::NkLoggerFormatter>("%L: %v");
			 * consoleSink.SetFormatter(std::move(fmt));
			 * @endcode
			 */
			void SetFormatter(memory::NkUniquePtr<NkLoggerFormatter> formatter) override;

			/**
			 * @brief Définit le pattern de formatage via création interne de formatter
			 * @param pattern Chaîne de pattern style spdlog à parser
			 * @ingroup ConsoleSinkInterface
			 *
			 * @note Méthode de convenance : crée ou met à jour m_Formatter avec le pattern
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Équivalent à SetFormatter(MakeUnique<NkLoggerFormatter>(pattern))
			 *
			 * @example
			 * @code
			 * // Pattern avec couleurs pour console
			 * consoleSink.SetPattern("[%^%L%$] %v");
			 *
			 * // Pattern sans couleurs pour logs parsables
			 * consoleSink.SetPattern("%L: %v");
			 * @endcode
			 */
			void SetPattern(const NkString& pattern) override;

			/**
			 * @brief Obtient le formatter courant utilisé par ce sink
			 * @return Pointeur brut vers le formatter, ou nullptr si non configuré
			 * @ingroup ConsoleSinkInterface
			 *
			 * @note Retourne un pointeur non-possédé : ne pas delete
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Le pointeur peut devenir invalide après SetFormatter() ou destruction
			 *
			 * @example
			 * @code
			 * if (auto* fmt = consoleSink.GetFormatter()) {
			 *     nkentseu::NkString preview = fmt->Format(sampleMessage, true);
			 *     printf("Preview: %s\n", preview.CStr());
			 * }
			 * @endcode
			 */
			NkLoggerFormatter* GetFormatter() const override;

			/**
			 * @brief Obtient le pattern de formatage courant
			 * @return Chaîne NkString contenant le pattern, ou vide si non configuré
			 * @ingroup ConsoleSinkInterface
			 *
			 * @note Retour par valeur : copie safe pour usage prolongé
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour sauvegarde de configuration ou affichage de debug
			 *
			 * @example
			 * @code
			 * // Sauvegarde de la configuration courante
			 * nkentseu::NkString currentPattern = consoleSink.GetPattern();
			 * Config::SetString("logging.console.pattern", currentPattern.CStr());
			 * @endcode
			 */
			NkString GetPattern() const override;


			// -----------------------------------------------------------------
			// CONFIGURATION SPÉCIFIQUE À LA CONSOLE
			// -----------------------------------------------------------------
			/**
			 * @defgroup ConsoleSinkConfig Configuration Console-Specific
			 * @brief Méthodes pour ajuster le comportement de sortie console
			 */

			/**
			 * @brief Active ou désactive l'usage des couleurs dans la sortie console
			 * @param enable true pour activer les couleurs (si supporté), false pour désactiver
			 * @ingroup ConsoleSinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note L'activation n'a d'effet que si SupportsColors() retourne true
			 * @note Sur Android : toujours ignoré car logcat ne supporte pas ANSI
			 * @note Utile pour forcer désactivation dans des environnements non-interactifs
			 *
			 * @example
			 * @code
			 * // Désactiver les couleurs pour logs redirigés vers fichier
			 * if (IsOutputRedirected()) {
			 *     consoleSink.SetColorEnabled(false);
			 * }
			 * @endcode
			 */
			void SetColorEnabled(bool enable);

			/**
			 * @brief Vérifie si les couleurs sont activées pour ce sink
			 * @return true si m_UseColors == true, false sinon
			 * @ingroup ConsoleSinkConfig
			 *
			 * @note Retourne la configuration, pas le support réel du terminal
			 * @note Pour vérifier le support effectif : appeler SupportsColors()
			 * @note Thread-safe : lecture protégée via m_Mutex
			 *
			 * @example
			 * @code
			 * if (consoleSink.IsColorEnabled() && consoleSink.SupportsColors()) {
			 *     printf("Colored console output enabled\n");
			 * }
			 * @endcode
			 */
			bool IsColorEnabled() const;

			/**
			 * @brief Définit le flux de console principal pour ce sink
			 * @param stream Nouveau flux à utiliser (NK_STD_OUT ou NK_STD_ERR)
			 * @ingroup ConsoleSinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Affecte tous les messages sauf si SetUseStderrForErrors(true) et niveau erreur
			 * @note Modification effective immédiatement pour les prochains Log()
			 *
			 * @example
			 * @code
			 * // Router tous les logs vers stderr pour capture d'erreur shell
			 * consoleSink.SetStream(nkentseu::NkConsoleStream::NK_STD_ERR);
			 * @endcode
			 */
			void SetStream(NkConsoleStream stream);

			/**
			 * @brief Obtient le flux de console configuré pour ce sink
			 * @return NkConsoleStream courant (NK_STD_OUT ou NK_STD_ERR)
			 * @ingroup ConsoleSinkConfig
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour affichage de configuration ou validation de setup
			 *
			 * @example
			 * @code
			 * if (consoleSink.GetStream() == nkentseu::NkConsoleStream::NK_STD_ERR) {
			 *     logger.Debug("Console sink configured for stderr output");
			 * }
			 * @endcode
			 */
			NkConsoleStream GetStream() const;

			/**
			 * @brief Configure l'usage de stderr pour les niveaux d'erreur
			 * @param enable true pour router Error/Critical/Fatal vers stderr, false pour tout vers m_Stream
			 * @ingroup ConsoleSinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Utile pour séparation des flux dans les scripts shell
			 * @note Affecte uniquement les niveaux NK_ERROR, NK_CRITICAL, NK_FATAL
			 *
			 * @example
			 * @code
			 * // Séparer flux normaux et erreurs pour monitoring
			 * consoleSink.SetUseStderrForErrors(true);
			 *
			 * // Usage shell :
			 * // ./app > normal.log 2> errors.log
			 * @endcode
			 */
			void SetUseStderrForErrors(bool enable);

			/**
			 * @brief Vérifie si le sink route les erreurs vers stderr
			 * @return true si m_UseStderrForErrors == true, false sinon
			 * @ingroup ConsoleSinkConfig
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour affichage de configuration ou validation de routing
			 *
			 * @example
			 * @code
			 * if (consoleSink.IsUsingStderrForErrors()) {
			 *     logger.Debug("Error-level logs will be routed to stderr");
			 * }
			 * @endcode
			 */
			bool IsUsingStderrForErrors() const;


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
			// -----------------------------------------------------------------
		private:


			// -----------------------------------------------------------------
			// MÉTHODES PRIVÉES D'IMPLÉMENTATION
			// -----------------------------------------------------------------
			/**
			 * @defgroup ConsoleSinkPrivate Méthodes Privées
			 * @brief Fonctions internes pour gestion de la sortie console
			 */

			/**
			 * @brief Sélectionne le flux C approprié pour un niveau de log donné
			 * @param level Niveau de log du message à écrire
			 * @return Pointeur FILE* vers stdout ou stderr selon configuration
			 * @ingroup ConsoleSinkPrivate
			 *
			 * @note Logique de sélection :
			 *   - Si m_UseStderrForErrors && level >= NK_ERROR → stderr
			 *   - Sinon → m_Stream (stdout ou stderr selon configuration)
			 * @note Thread-safe : lecture des membres protégée par mutex appelant
			 * @note Retourne nullptr en cas d'erreur (rare, pour robustesse)
			 *
			 * @example
			 * @code
			 * // Avec m_UseStderrForErrors = true :
			 * GetStreamForLevel(NK_INFO)   → stdout
			 * GetStreamForLevel(NK_ERROR)  → stderr
			 * @endcode
			 */
			FILE* GetStreamForLevel(NkLogLevel level);

			/**
			 * @brief Vérifie si la console courante supporte les codes couleur
			 * @return true si les couleurs sont supportées, false sinon
			 * @ingroup ConsoleSinkPrivate
			 *
			 * @note Détection multiplateforme :
			 *   - Android : toujours false (logcat ne supporte pas ANSI)
			 *   - Windows : GetConsoleMode + ENABLE_VIRTUAL_TERMINAL_PROCESSING
			 *   - Unix : isatty() + vérification variable d'environnement TERM
			 * @note Résultat mis en cache après première vérification pour performance
			 * @note Thread-safe : lecture/écriture du cache protégée par mutex
			 *
			 * @example
			 * @code
			 * if (consoleSink.SupportsColors()) {
			 *     logger.Debug("Terminal supports ANSI colors");
			 * } else {
			 *     logger.Debug("Terminal does not support colors, using plain output");
			 * }
			 * @endcode
			 */
			bool SupportsColors() const;

			/**
			 * @brief Obtient le code couleur ANSI pour un niveau de log donné
			 * @param level Niveau de log pour lequel obtenir la couleur
			 * @return Chaîne NkString contenant le code ANSI (ex: "\033[32m")
			 * @ingroup ConsoleSinkPrivate
			 *
			 * @note Délègue à NkLogLevelToANSIColor() pour cohérence globale
			 * @note Retourne chaîne vide si level inconnu ou non coloré
			 * @note Thread-safe : aucune mutation d'état partagé
			 *
			 * @example
			 * @code
			 * // Codes retournés :
			 * GetColorCode(NK_INFO)     → "\033[32m"  // Vert
			 * GetColorCode(NK_ERROR)    → "\033[31m"  // Rouge
			 * GetColorCode(NK_UNKNOWN)  → ""          // Vide
			 * @endcode
			 */
			NkString GetColorCode(NkLogLevel level) const;

			/**
			 * @brief Obtient le code ANSI de reset pour terminer une zone colorée
			 * @return Chaîne NkString contenant le code de reset "\033[0m"
			 * @ingroup ConsoleSinkPrivate
			 *
			 * @note Code ANSI standard : reset tous les attributs (couleur, gras, etc.)
			 * @note Thread-safe : retour de constante, aucune mutation d'état
			 * @note À utiliser après chaque message coloré pour éviter la pollution
			 *
			 * @example
			 * @code
			 * // Usage typique dans un message coloré :
			 * printf("%s[INFO]%s Message text\n",
			 *     GetColorCode(NK_INFO).CStr(),
			 *     GetResetCode().CStr());
			 * @endcode
			 */
			NkString GetResetCode() const;

			/**
			 * @brief Configure la couleur de console Windows via Win32 API
			 * @param level Niveau de log pour lequel appliquer la couleur
			 * @ingroup ConsoleSinkPrivate
			 *
			 * @note Utilise SetConsoleTextAttribute() avec code dérivé de NkLogLevelToWindowsColor
			 * @note Cible STD_OUTPUT_HANDLE ou STD_ERROR_HANDLE selon m_Stream
			 * @note Thread-safe : appel Win32 API thread-safe par conception
			 * @note No-op si handle invalide ou plateforme non-Windows
			 *
			 * @example
			 * @code
			 * // Avant écriture d'un message d'erreur sur Windows :
			 * #ifdef _WIN32
			 *     SetWindowsColor(NkLogLevel::NK_ERROR);  // Rouge clair
			 * #endif
			 * fprintf(stderr, "Error message\n");
			 * ResetWindowsColor();  // Reset après écriture
			 * @endcode
			 */
			void SetWindowsColor(NkLogLevel level);

			/**
			 * @brief Réinitialise la couleur de console Windows aux attributs par défaut
			 * @ingroup ConsoleSinkPrivate
			 *
			 * @note Utilise SetConsoleTextAttribute() avec valeur 0x07 (gris sur noir)
			 * @note Cible STD_OUTPUT_HANDLE ou STD_ERROR_HANDLE selon m_Stream
			 * @note Thread-safe : appel Win32 API thread-safe par conception
			 * @note No-op si handle invalide ou plateforme non-Windows
			 *
			 * @example
			 * @code
			 * // Après écriture d'un message coloré sur Windows :
			 * #ifdef _WIN32
			 *     ResetWindowsColor();  // Retour à gris/noir par défaut
			 * #endif
			 * @endcode
			 */
			void ResetWindowsColor();


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PRIVÉES (ÉTAT INTERNE)
			// -----------------------------------------------------------------
			/**
			 * @defgroup ConsoleSinkMembers Membres de Données
			 * @brief État interne du sink console (non exposé publiquement)
			 */

			/// @brief Formatter pour formatage des messages avant écriture console
			/// @ingroup ConsoleSinkMembers
			/// @note Ownership exclusif via NkUniquePtr : libération automatique
			memory::NkUniquePtr<NkLoggerFormatter> m_Formatter;

			/// @brief Flux de console principal pour ce sink (stdout ou stderr)
			/// @ingroup ConsoleSinkMembers
			/// @note Modifiable via SetStream(), lecture via GetStream()
			NkConsoleStream m_Stream;

			/// @brief Indicateur d'activation des couleurs pour ce sink
			/// @ingroup ConsoleSinkMembers
			/// @note true = utiliser couleurs si SupportsColors(), false = toujours plain
			/// @note Modifiable via SetColorEnabled(), lecture via IsColorEnabled()
			bool m_UseColors;

			/// @brief Indicateur de routage des erreurs vers stderr
			/// @ingroup ConsoleSinkMembers
			/// @note true = Error/Critical/Fatal → stderr, false = tout vers m_Stream
			/// @note Modifiable via SetUseStderrForErrors(), lecture via IsUsingStderrForErrors()
			bool m_UseStderrForErrors;

			/// @brief Mutex pour synchronisation thread-safe des opérations
			/// @ingroup ConsoleSinkMembers
			/// @note mutable : permet modification dans les méthodes const (SupportsColors, etc.)
			/// @note Protège l'accès aux membres et les écritures console concurrentes
			mutable threading::NkMutex m_Mutex;


		}; // class NkConsoleSink


	} // namespace nkentseu


#endif // NKENTSEU_NKCONSOLESINK_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKCONSOLESINK.H
// =============================================================================
// Ce fichier fournit l'implémentation de sink pour sortie console avec couleurs.
// Il combine détection automatique, multiplateforme et configuration flexible.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration basique avec couleurs automatiques
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkConsoleSink.h>

	void SetupConsoleLogging() {
		// Sink console avec détection automatique des couleurs
		auto consoleSink = nkentseu::memory::MakeShared<nkentseu::NkConsoleSink>();

		// Pattern avec marqueurs de couleur pour niveaux
		consoleSink->SetPattern("[%^%L%$] %v");

		// Ajout au logger global
		nkentseu::NkLog::Instance().AddSink(consoleSink);

		// Logging : couleurs appliquées si terminal supporté
		logger.Info("Application started");      // Vert
		logger.Warn("Low memory warning");        // Jaune
		logger.Error("Connection failed");        // Rouge
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Routage stderr pour séparation des flux shell
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkConsoleSink.h>

	void SetupSeparateErrorStream() {
		using namespace nkentseu;

		// Sink principal vers stdout avec couleurs
		auto consoleSink = memory::MakeShared<NkConsoleSink>(
			NkConsoleStream::NK_STD_OUT,  // Flux principal : stdout
			true                           // Couleurs activées
		);

		// Router les erreurs vers stderr pour capture séparée
		consoleSink->SetUseStderrForErrors(true);

		// Pattern différent pour erreurs si souhaité
		consoleSink->SetPattern("[%L] %v");

		NkLog::Instance().AddSink(consoleSink);

		// Usage shell :
		// ./app > output.log 2> errors.log
		// → Logs normaux dans output.log, erreurs dans errors.log
	}
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Désactivation des couleurs pour logs parsables
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkConsoleSink.h>

	void SetupParsableConsoleOutput() {
		using namespace nkentseu;

		// Sink sans couleurs pour output machine-parsable
		auto consoleSink = memory::MakeShared<NkConsoleSink>(
			NkConsoleStream::NK_STD_OUT,
			false  // Désactiver les couleurs pour parsing facile
		);

		// Pattern simple sans marqueurs couleur
		consoleSink->SetPattern("%L: %v");

		NkLog::Instance().AddSink(consoleSink);

		// Output : "INFO: Application started" (sans codes ANSI)
		// Facile à parser avec grep, awk, etc.
	}
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Configuration dynamique via environnement
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkConsoleSink.h>
	#include <NKCore/NkConfig.h>

	void SetupEnvironmentAwareConsole() {
		using namespace nkentseu;

		// Lecture de la configuration depuis env/CLI
		bool enableColors = core::Config::GetBool("logging.console.colors", true);
		bool stderrForErrors = core::Config::GetBool("logging.console.stderrErrors", true);
		NkConsoleStream stream = core::Config::GetString("logging.console.stream", "stdout") == "stderr"
			? NkConsoleStream::NK_STD_ERR
			: NkConsoleStream::NK_STD_OUT;

		// Création avec configuration lue
		auto consoleSink = memory::MakeShared<NkConsoleSink>(stream, enableColors);
		consoleSink->SetUseStderrForErrors(stderrForErrors);
		consoleSink->SetPattern(core::Config::GetString("logging.console.pattern", "[%^%L%$] %v"));

		NkLog::Instance().AddSink(consoleSink);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Testing avec vérification de sortie console
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkConsoleSink.h>
	#include <NKLogger/NkLogMessage.h>
	#include <gtest/gtest.h>

	// Note : tester la sortie console réelle est complexe
	// Alternative : utiliser NkMemorySink pour capture en tests unitaires

	TEST(NkConsoleSinkTest, BasicConfiguration) {
		using namespace nkentseu;

		// Test de configuration par défaut
		NkConsoleSink defaultSink;
		EXPECT_EQ(defaultSink.GetStream(), NkConsoleStream::NK_STD_OUT);
		EXPECT_TRUE(defaultSink.IsColorEnabled());
		EXPECT_TRUE(defaultSink.IsUsingStderrForErrors());

		// Test de configuration personnalisée
		NkConsoleSink customSink(NkConsoleStream::NK_STD_ERR, false);
		EXPECT_EQ(customSink.GetStream(), NkConsoleStream::NK_STD_ERR);
		EXPECT_FALSE(customSink.IsColorEnabled());
		EXPECT_TRUE(customSink.IsUsingStderrForErrors());

		// Test de modification dynamique
		customSink.SetColorEnabled(true);
		EXPECT_TRUE(customSink.IsColorEnabled());
	}
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Détection et logging du support couleur
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkConsoleSink.h>

	void LogColorSupportStatus() {
		using namespace nkentseu;

		NkConsoleSink consoleSink;

		// Vérification du support effectif des couleurs
		bool configured = consoleSink.IsColorEnabled();
		bool supported = consoleSink.SupportsColors();

		logger.Info("Console color support: configured={0}, supported={1}",
			configured ? "yes" : "no",
			supported ? "yes" : "no");

		if (configured && !supported) {
			logger.Warn("Colors configured but terminal does not support them");
		}
	}
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. DÉTECTION DES COULEURS :
	   - SupportsColors() met en cache le résultat après première vérification
	   - Pour forcer re-vérification : recréer le sink ou ajouter méthode RefreshColorSupport()
	   - Sur Windows : ENABLE_VIRTUAL_TERMINAL_PROCESSING doit être activé pour ANSI
	   - Alternative Windows : utiliser SetWindowsColor() via Win32 API si ANSI non supporté

	2. ROUTAGE STDOUT/STDERR :
	   - m_UseStderrForErrors permet séparation logique sans changer m_Stream
	   - Utile pour scripting shell : ./app > out.log 2> err.log
	   - Attention : buffering différent entre stdout (line-buffered) et stderr (unbuffered)

	3. SUPPORT ANDROID / LOGCAT :
	   - Sur Android, toute sortie console est redirigée vers __android_log_print
	   - Les codes ANSI sont ignorés : SetColorEnabled() n'a pas d'effet
	   - Le tag logcat est dérivé de loggerName (tronqué à 23 caractères max)

	4. THREAD-SAFETY DES ÉCRITURES CONSOLE :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLock
	   - fwrite/fputc sont thread-safe sur la plupart des implémentations C stdio
	   - Pour garantie stricte : envisager mutex global console si écritures depuis multiples sinks

	5. PERFORMANCE ET BUFFERING :
	   - Flush automatique pour niveaux >= NK_ERROR pour visibilité immédiate
	   - Pour haute fréquence : désactiver flush auto et appeler Flush() périodiquement
	   - stdout est line-buffered par défaut, stderr est unbuffered : impact sur performance

	6. COMPATIBILITÉ MULTIPLATEFORME :
	   - ANSI : supporté sur la plupart des terminaux Unix/Linux/macOS modernes
	   - Windows 10+ : nécessite ENABLE_VIRTUAL_TERMINAL_PROCESSING activé
	   - Windows ancien : fallback vers SetConsoleTextAttribute via Win32 API
	   - Android : redirection vers logcat, pas de support ANSI natif

	7. EXTENSIBILITÉ FUTURES :
	   - Support des hyperliens terminal : ajouter SetHyperlinkEnabled()
	   - Progress bars/animations : ajouter méthode pour écriture sans newline
	   - Couleurs 24-bit (truecolor) : étendre NkLogLevelToANSIColor pour codes RGB

	8. TESTING ET VALIDATION :
	   - Tester SupportsColors() dans différents environnements (TTY, pipe, file redirection)
	   - Valider le routage stderr avec tests de redirection shell
	   - Pour Android : tester via adb logcat pour vérification de sortie
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================