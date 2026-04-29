// =============================================================================
// NKLogger/Sinks/NkConsoleSink.cpp
// Implémentation du sink pour sortie console avec support couleur multiplateforme.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Fonctions utilitaires dans namespace anonyme pour encapsulation
//  - Routage Android vers logcat via __android_log_print
//  - Détection couleur : isatty/TERM sur Unix, GetConsoleMode sur Windows
//  - Synchronisation thread-safe via NKThreading/NkMutex
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Propriétaire - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/Sinks/NkConsoleSink.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkLoggerFormatter.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"

#include <cstdio>
#include <cstdlib>

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
	#include <android/log.h>
#endif

#if defined(_WIN32)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#else
	#include <unistd.h>   // Pour isatty(), fileno()
	#include <cstring>    // Pour strstr()
#endif


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE ANONYME - UTILITAIRES INTERNES (ANDROID)
// -------------------------------------------------------------------------
// Fonctions spécifiques à Android pour routage vers logcat.
// Non exposées dans l'API publique pour encapsulation.

#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
namespace {


	// -------------------------------------------------------------------------
	// FONCTION : NkToAndroidPriority
	// DESCRIPTION : Mappe un NkLogLevel vers un priority android_log
	// PARAMS : level - Niveau de log NKLogger à convertir
	// RETURN : Constante ANDROID_LOG_* correspondante pour __android_log_print
	// NOTE : Mapping direct avec fallback ANDROID_LOG_INFO pour niveaux inconnus
	// -------------------------------------------------------------------------
	int NkToAndroidPriority(nkentseu::NkLogLevel level) {
		switch (level) {
			case nkentseu::NkLogLevel::NK_TRACE:
				return ANDROID_LOG_VERBOSE;

			case nkentseu::NkLogLevel::NK_DEBUG:
				return ANDROID_LOG_DEBUG;

			case nkentseu::NkLogLevel::NK_INFO:
				return ANDROID_LOG_INFO;

			case nkentseu::NkLogLevel::NK_WARN:
				return ANDROID_LOG_WARN;

			case nkentseu::NkLogLevel::NK_ERROR:
				return ANDROID_LOG_ERROR;

			case nkentseu::NkLogLevel::NK_CRITICAL:
			case nkentseu::NkLogLevel::NK_FATAL:
				return ANDROID_LOG_FATAL;

			default:
				// Fallback sécurisé pour niveaux inconnus ou futurs
				return ANDROID_LOG_INFO;
		}
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkMakeAndroidTag
	// DESCRIPTION : Génère un tag logcat valide depuis un loggerName
	// PARAMS : loggerName - Nom du logger source (peut être vide ou long)
	// RETURN : Chaîne NkString tronquée à 23 caractères max pour compatibilité logcat
	// NOTE : logcat limite les tags à 23 caractères : tronquer silencieusement si nécessaire
	// -------------------------------------------------------------------------
	nkentseu::NkString NkMakeAndroidTag(const nkentseu::NkString& loggerName) {
		// Cas vide : fallback vers tag générique
		if (loggerName.Empty()) {
			return "NkConsole";
		}

		// Troncature si trop long : logcat limite à 23 caractères
		if (loggerName.Length() > 23) {
			return loggerName.SubStr(0, 23);
		}

		// Nom valide : retour tel quel
		return loggerName;
	}


} // namespace anonymous
#endif  // Android-specific utilities


// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques et privées de NkConsoleSink.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur par défaut
	// DESCRIPTION : Initialisation avec stdout, couleurs activées, pattern couleur
	// -------------------------------------------------------------------------
	NkConsoleSink::NkConsoleSink()
		: m_Stream(NkConsoleStream::NK_STD_OUT)
		, m_UseColors(true)
		, m_UseStderrForErrors(true) {

		// Configuration du formatter avec pattern supportant les marqueurs de couleur
		m_Formatter = memory::NkMakeUnique<NkFormatter>(NkFormatter::NK_COLOR_PATTERN);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur avec flux et couleurs personnalisés
	// DESCRIPTION : Initialisation avec paramètres explicites
	// -------------------------------------------------------------------------
	NkConsoleSink::NkConsoleSink(NkConsoleStream stream, bool useColors)
		: m_Stream(stream)
		, m_UseColors(useColors)
		, m_UseStderrForErrors(true) {

		// Choix du pattern basé sur l'activation des couleurs
		const char* pattern = useColors
			? NkFormatter::NK_COLOR_PATTERN    // Pattern avec marqueurs %^/%$
			: NkFormatter::NK_DEFAULT_PATTERN; // Pattern standard sans couleurs

		m_Formatter = memory::NkMakeUnique<NkFormatter>(pattern);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur
	// DESCRIPTION : Flush garanti avant destruction pour persistance des buffers
	// -------------------------------------------------------------------------
	NkConsoleSink::~NkConsoleSink() {
		// Flush explicite : garantir que les messages en buffer sont écrits
		Flush();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Log
	// DESCRIPTION : Écriture thread-safe d'un message formaté vers la console
	// -------------------------------------------------------------------------
	void NkConsoleSink::Log(const NkLogMessage& message) {
		// Filtrage précoce : éviter tout traitement si message ignoré
		if (!IsEnabled() || !ShouldLog(message.level)) {
			return;
		}

		// Acquisition du mutex pour écriture thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Formatage du message avec couleurs si activées et supportées
		const bool applyColors = m_UseColors && SupportsColors();
		NkString formattedMessage = m_Formatter->Format(message, applyColors);

		#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
			// Android : redirection vers logcat via __android_log_print
			// Les codes ANSI sont ignorés par logcat : pas de traitement couleur

			// Génération du tag logcat depuis le loggerName (tronqué si nécessaire)
			const NkString tag = NkMakeAndroidTag(message.loggerName);

			// Mapping du niveau NKLogger vers priority android_log
			const int androidPriority = NkToAndroidPriority(message.level);

			// Écriture vers logcat avec formatage appliqué
			__android_log_print(androidPriority, tag.CStr(), "%s", formattedMessage.CStr());

			// Retour immédiat : pas d'écriture vers stdout/stderr sur Android
			return;
		#endif

		// Sélection du flux C approprié selon niveau et configuration
		FILE* outputStream = GetStreamForLevel(message.level);
		if (outputStream == nullptr) {
			// Échec de sélection de flux : retour silencieux pour robustesse
			return;
		}

		// Écriture du message formaté vers le flux sélectionné
		if (!formattedMessage.Empty()) {
			(void)::fwrite(
				formattedMessage.CStr(),
				sizeof(char),
				static_cast<size_t>(formattedMessage.Length()),
				outputStream
			);
		}

		// Newline automatique après chaque message pour lisibilité
		(void)::fputc('\n', outputStream);

		// Flush automatique pour niveaux critiques pour visibilité immédiate
		if (message.level >= NkLogLevel::NK_ERROR) {
			(void)::fflush(outputStream);
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Flush
	// DESCRIPTION : Force l'écriture des buffers C stdio vers la console
	// -------------------------------------------------------------------------
	void NkConsoleSink::Flush() {
		// Acquisition du mutex pour opération thread-safe
		threading::NkScopedLock lock(m_Mutex);

		#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
			// Android : logcat gère son propre buffering, pas de flush requis
			return;
		#endif

		// Flush de stdout si configuré comme flux principal ou si erreurs routées vers stderr
		if (m_Stream == NkConsoleStream::NK_STD_OUT) {
			(void)::fflush(stdout);
		}

		// Flush de stderr si configuré comme flux principal ou si erreurs routées vers stderr
		if (m_Stream == NkConsoleStream::NK_STD_ERR || m_UseStderrForErrors) {
			(void)::fflush(stderr);
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetFormatter
	// DESCRIPTION : Définit le formatter avec transfert de propriété thread-safe
	// -------------------------------------------------------------------------
	void NkConsoleSink::SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) {
		// Acquisition du mutex pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Transfert de propriété via move : ancien formatter détruit automatiquement
		m_Formatter = traits::NkMove(formatter);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetPattern
	// DESCRIPTION : Met à jour le pattern via le formatter interne
	// -------------------------------------------------------------------------
	void NkConsoleSink::SetPattern(const NkString& pattern) {
		// Acquisition du mutex pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour du pattern si formatter configuré
		if (m_Formatter) {
			m_Formatter->SetPattern(pattern);
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetFormatter
	// DESCRIPTION : Retourne le formatter courant (lecture thread-safe)
	// -------------------------------------------------------------------------
	NkFormatter* NkConsoleSink::GetFormatter() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour du pointeur brut via get() de NkUniquePtr
		return m_Formatter.Get();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetPattern
	// DESCRIPTION : Retourne le pattern courant via le formatter (lecture thread-safe)
	// -------------------------------------------------------------------------
	NkString NkConsoleSink::GetPattern() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Délégation au formatter si configuré
		if (m_Formatter) {
			return m_Formatter->GetPattern();
		}

		// Fallback : chaîne vide si aucun formatter
		return NkString{};
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetColorEnabled
	// DESCRIPTION : Active/désactive l'usage des couleurs thread-safe
	// -------------------------------------------------------------------------
	void NkConsoleSink::SetColorEnabled(bool enable) {
		// Acquisition du mutex pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour du flag de configuration
		m_UseColors = enable;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : IsColorEnabled
	// DESCRIPTION : Retourne l'état d'activation des couleurs (lecture thread-safe)
	// -------------------------------------------------------------------------
	bool NkConsoleSink::IsColorEnabled() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur configurée
		return m_UseColors;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetStream
	// DESCRIPTION : Définit le flux de console principal thread-safe
	// -------------------------------------------------------------------------
	void NkConsoleSink::SetStream(NkConsoleStream stream) {
		// Acquisition du mutex pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour du flux configuré
		m_Stream = stream;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetStream
	// DESCRIPTION : Retourne le flux configuré (lecture thread-safe)
	// -------------------------------------------------------------------------
	NkConsoleStream NkConsoleSink::GetStream() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur configurée
		return m_Stream;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetUseStderrForErrors
	// DESCRIPTION : Configure le routage des erreurs vers stderr thread-safe
	// -------------------------------------------------------------------------
	void NkConsoleSink::SetUseStderrForErrors(bool enable) {
		// Acquisition du mutex pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour du flag de routage
		m_UseStderrForErrors = enable;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : IsUsingStderrForErrors
	// DESCRIPTION : Retourne l'état du routage des erreurs (lecture thread-safe)
	// -------------------------------------------------------------------------
	bool NkConsoleSink::IsUsingStderrForErrors() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur configurée
		return m_UseStderrForErrors;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetStreamForLevel (privée)
	// DESCRIPTION : Sélectionne stdout ou stderr selon niveau et configuration
	// -------------------------------------------------------------------------
	FILE* NkConsoleSink::GetStreamForLevel(NkLogLevel level) {
		// Routage des erreurs vers stderr si configuré
		if (m_UseStderrForErrors) {
			if (level == NkLogLevel::NK_ERROR
				|| level == NkLogLevel::NK_CRITICAL
				|| level == NkLogLevel::NK_FATAL) {
				return stderr;
			}
		}

		// Retour du flux configuré par défaut
		return (m_Stream == NkConsoleStream::NK_STD_OUT) ? stdout : stderr;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SupportsColors (privée)
	// DESCRIPTION : Détecte si la console courante supporte les codes couleur
	// -------------------------------------------------------------------------
	bool NkConsoleSink::SupportsColors() const {
		#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
			// Android : logcat ne supporte pas les codes ANSI
			// Les couleurs sont gérées par l'IDE/adb logcat si configuré
			return false;

		#elif defined(_WIN32)
			// Windows : vérification via GetConsoleMode + ENABLE_VIRTUAL_TERMINAL_PROCESSING
			HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
			if (consoleHandle == INVALID_HANDLE_VALUE) {
				return false;
			}

			DWORD consoleMode = 0;
			if (!GetConsoleMode(consoleHandle, &consoleMode)) {
				return false;
			}

			// Support ANSI si ENABLE_VIRTUAL_TERMINAL_PROCESSING est activé
			return (consoleMode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;

		#else
			// Unix/Linux/macOS : vérification via isatty() + variable d'environnement TERM
			// Cache statique pour éviter les vérifications répétées coûteuses
			static bool checked = false;
			static bool supports = false;

			if (!checked) {
				// Vérification si stdout est un terminal interactif
				const int fileDescriptor = fileno(stdout);
				if (fileDescriptor < 0) {
					supports = false;
				} else if (isatty(fileDescriptor)) {
					// Vérification de la variable d'environnement TERM
					const char* termEnv = getenv("TERM");
					if (termEnv != nullptr) {
						// Recherche de terminaux connus pour supporter les couleurs
						supports = (strstr(termEnv, "xterm") != nullptr)
							|| (strstr(termEnv, "color") != nullptr)
							|| (strstr(termEnv, "ansi") != nullptr)
							|| (strstr(termEnv, "256color") != nullptr);
					}
				}
				checked = true;
			}

			return supports;
		#endif
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetColorCode (privée)
	// DESCRIPTION : Retourne le code ANSI pour un niveau de log
	// -------------------------------------------------------------------------
	NkString NkConsoleSink::GetColorCode(NkLogLevel level) const {
		// Délégation à la fonction utilitaire centralisée pour cohérence
		return NkLogLevelToANSIColor(level);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetResetCode (privée)
	// DESCRIPTION : Retourne le code ANSI de reset des attributs console
	// -------------------------------------------------------------------------
	NkString NkConsoleSink::GetResetCode() const {
		// Code ANSI standard : reset tous les attributs (couleur, gras, souligné, etc.)
		return "\033[0m";
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetWindowsColor (privée)
	// DESCRIPTION : Configure la couleur de console Windows via Win32 API
	// -------------------------------------------------------------------------
	void NkConsoleSink::SetWindowsColor(NkLogLevel level) {
		#if defined(_WIN32)
			// Sélection du handle console selon le flux configuré
			HANDLE consoleHandle = GetStdHandle(
				(m_Stream == NkConsoleStream::NK_STD_OUT)
					? STD_OUTPUT_HANDLE
					: STD_ERROR_HANDLE
			);

			// Application de la couleur uniquement si handle valide
			if (consoleHandle != INVALID_HANDLE_VALUE) {
				// Mapping du niveau vers code couleur Windows via fonction centralisée
				const WORD colorCode = NkLogLevelToWindowsColor(level);
				SetConsoleTextAttribute(consoleHandle, colorCode);
			}
		#endif
		// No-op sur plateformes non-Windows
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : ResetWindowsColor (privée)
	// DESCRIPTION : Réinitialise la couleur de console Windows aux attributs par défaut
	// -------------------------------------------------------------------------
	void NkConsoleSink::ResetWindowsColor() {
		#if defined(_WIN32)
			// Sélection du handle console selon le flux configuré
			HANDLE consoleHandle = GetStdHandle(
				(m_Stream == NkConsoleStream::NK_STD_OUT)
					? STD_OUTPUT_HANDLE
					: STD_ERROR_HANDLE
			);

			// Réinitialisation uniquement si handle valide
			if (consoleHandle != INVALID_HANDLE_VALUE) {
				// Code 0x07 : foreground gris clair (0x07) sur fond noir (0x00)
				// Équivalent à FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
				SetConsoleTextAttribute(consoleHandle, 0x07);
			}
		#endif
		// No-op sur plateformes non-Windows
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
	1. DÉTECTION DES COULEURS MULTIPLATEFORME :
	   - Android : toujours false, logcat ne supporte pas ANSI
	   - Windows : GetConsoleMode + ENABLE_VIRTUAL_TERMINAL_PROCESSING requis
	   - Unix : isatty() + vérification TERM pour terminaux connus
	   - Cache statique sur Unix pour éviter vérifications répétées

	2. ROUTAGE ANDROID VERS LOGCAT :
	   - __android_log_print utilisé avec priority mappé depuis NkLogLevel
	   - Tag dérivé de loggerName, tronqué à 23 caractères max pour compatibilité
	   - Les codes ANSI sont ignorés : pas de traitement couleur sur Android

	3. GESTION DES COULEURS WINDOWS :
	   - Deux approches : ANSI via ENABLE_VIRTUAL_TERMINAL_PROCESSING ou Win32 API
	   - SetWindowsColor/ResetWindowsColor utilisent NkLogLevelToWindowsColor centralisé
	   - Fallback vers attributs par défaut (0x07) après chaque message coloré

	4. THREAD-SAFETY DES ÉCRITURES CONSOLE :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLock
	   - fwrite/fputc sont thread-safe sur la plupart des implémentations C stdio
	   - Pour garantie stricte : mutex global console si écritures depuis multiples sinks

	5. BUFFERING ET FLUSH :
	   - stdout : line-buffered par défaut sur terminaux, fully-buffered si redirigé
	   - stderr : unbuffered par défaut pour visibilité immédiate des erreurs
	   - Flush automatique pour niveaux >= NK_ERROR pour visibilité garantie

	6. COMPATIBILITÉ MULTIPLATEFORME :
	   - fwrite/fputc/fflush : standards C, portables partout
	   - isatty/fileno : POSIX, disponibles sur Windows via <io.h> si nécessaire
	   - getenv : standard C, portable pour lecture variables d'environnement

	7. EXTENSIBILITÉ FUTURES :
	   - Support des hyperliens terminal : ajouter méthode SetHyperlinkEnabled()
	   - Progress bars/animations : méthode WriteNoNewline() pour mise à jour in-place
	   - Couleurs 24-bit : étendre NkLogLevelToANSIColor pour codes RGB si terminal supporte

	8. TESTING ET VALIDATION :
	   - Tester SupportsColors() dans différents contextes : TTY interactif, pipe, redirection fichier
	   - Valider le routage stderr avec tests shell : ./app > out.log 2> err.log
	   - Pour Android : tester via adb logcat pour vérification de sortie et tag
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================