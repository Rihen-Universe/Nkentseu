// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkAndroidSink.cpp
// DESCRIPTION: Implémentation du sink Android avec support JNI.
// AUTEUR: Rihen
// DATE: 2026
// PLATEFORME: Android / NDK
// DÉPENDANCE: <android/log.h> (disponible via NDK)
// COMPILATION: 
//   - Sur Android: -DANDROID ou NKENTSEU_PLATFORM_ANDROID automatiquement défini
//   - Sur autres: compile en mode stub (logs sur stderr)
// NOTES:
//   - Utilise __android_log_print() pour envoyer à logcat
//   - Les logs incluent le timestamp, niveau, et tag
//   - Supporte tous les niveaux de log
//   - Thread-safe avec verrou interne
// COMMANDE LOGCAT: adb logcat -s "MyApp"
// TAGS: Android, NDK, Logging, Performance
// LIMITE: Les logs très longs (>500 chars) peuvent être tronqués par le système
// LIMITATION CONNUE: Le tag doit être <= 23 caractères (spec Android)
// EXEMPLE DE SORTIE LOGCAT:
//   I/MyApp: [2026-03-03 14:30:45.123] [INFO] Logger initialized
//   E/MyApp: [2026-03-03 14:30:46.456] [ERROR] Critical error occurred
// OPTIMISATION: Les logs sont directement écrits en Android log, pas en memory buffering
// SÉCURITÉ: Les messages sensibles ne doivent pas être loggés via ce sink en production
// PERFORMANCE: Impact minimal, utilise le buffer interne du système Android
// DEBUGGING: Peut sortir beaucoup de messages, filtrer avec logcat si nécessaire
// TEST: À tester avec: adb logcat | grep "MyApp"
// ANDROID_LOG LEVELS MAPPING:
//   - ANDROID_LOG_VERBOSE (1) -> Trace (tous les détails)
//   - ANDROID_LOG_DEBUG (2)   -> Debug (infos pour déboguer) 
//   - ANDROID_LOG_INFO (3)    -> Info (infos générales)
//   - ANDROID_LOG_WARN (4)    -> Warning (avertissements)
//   - ANDROID_LOG_ERROR (5)   -> Error (erreurs)
//   - ANDROID_LOG_FATAL (6)   -> Fatal (erreurs critiques)
// USAGE ANDROID:
//   Dans AndroidManifest.xml, les permissions ne sont pas requises pour android::log
//   C'est un système interne, pas d'accès fichier.
// DOCUMENTATION ANDROID: https://developer.android.com/ndk/reference/android__log_8h
// VÉRIFICATION LOGCAT:
//   1. Connecter l'appareil ou lancer l'émulateur
//   2. Ouvrir les logs: adb logcat
//   3. Chercher les logs du tag: adb logcat | grep "NkAndroidLog"
//   4. Effacer les logs: adb logcat -c
// CONSEILS:
//   - Utiliser des tags courts et significatifs
//   - Éviter les logs dans les boucles critiques
//   - Mettre cette classe dans une classe Logger wrapper
//   - Combiner avec NkFileSink pour persister les logs
// EXEMPLE DE CONFIGURATION:
//   auto consoleSink = memory::NkMakeUnique<NkAndroidSink>("MyApp");
//   consoleSink->SetFormatter(memory::NkMakeUnique<NkFormatter>("[%lvl] %msg"));
//   logger->AddSink(traits::NkMove(consoleSink));
// NOTES DE VERSION:
//   v1.0 (2026): Implémentation initiale
//   - Support des niveaux de log complets
//   - Validation et format du tag
//   - Thread-safety avec mutex
//   - Compilable sur non-Android
// FUTURE: 
//   - Support de priorités personnalisées
//   - Caching de la chaîne formatée
//   - Configuration de truncation des messages longs
// AUTEUR NOTES: 
//   Ce sink est crucial pour le debugging des applications Android.
//   Les logs Android disparaissent au redémarrage, à moins de les capturer.
//   Préférer combiner avec NkFileSink pour persister.
// DÉPENDANCES:
//   - android::log (via NDK, inclus automatiquement sur Android)
//   - NkFormatter (pour formatage)
//   - NkLogLevel (pour les niveaux)
//   - <mutex> (pour thread-safety)
//   - <string> (pour les strings)
//   - <memory> (pour unique_ptr)
// COMPILATION FLAGS:
//   - Sur Android: Inclure <android/log.h> directement
//   - Sur autres: Créer un stub sans dépendances Android
//   - Compiler avec: -llog (sur Android)
// EXEMPLE CMakeLists.txt:
//   if(ANDROID)
//     target_link_libraries(nklogger PRIVATE log)
//   endif()
// VÉRIFICATION: Les logs doivent apparaître dans:
//   - Android Studio -> Logcat
//   - Terminal avec: adb logcat
//   - Fichier texte si NkFileSink est également utilisé
// OPTIMISATION: Le système Android log utilise un ring buffer circulaire
// SÉCURITÉ: Éviter de logger des tokens, mots de passe, ou infos sensibles
// LIMITE: Les logs plus longs que ~4000 caractères peuvent être fragmentés
// CONSEIL: Utiliser adb logcat avec filtrage:
//   adb logcat NkAndroidLog:V *:S (uniquement NkAndroidLog en VERBOSE)
//   adb logcat *:E (uniquement les ERREURS)
// INTÉGRATION: Fonctionne avec le reste du système NkLogger sans modifications
// EXEMPLE RÉALISTE:
//   try {
//     auto sink = memory::NkMakeUnique<NkAndroidSink>("com.example");
//     sink->SetFormatter(...);
//     nkentseu::logger::NkLogger::GetInstance()->AddSink(traits::NkMove(sink));
//   } catch(...) { /* handle error */ }
// NOTES FIREBASE CRASHLYTICS INTEGRATION:
//   Ce sink ne doit pas remplacer Crashlytics.
//   Le combiner avec Crashlytics pour une couverture complète.
// NOTES DATADOG INTEGRATION:
//   Si Datadog est utilisé, configurer les forwarders pour Android Log.
// NOTES SPLUNK INTEGRATION:
//   Les logs Android peuvent être forwardés à Splunk via le NDK.
// SECURITY NOTES:
//   - Android log accessible à niveau système
//   - Ne pas logger de données utilisateur sensibles
//   - Ajouter obfuscation si nécessaire
// END DOCUMENTATION
// -----------------------------------------------------------------------------

#include "NKLogger/Sinks/NkAndroidSink.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkFormatter.h"


#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

#include <cctype>
#include <cstdio>

#if NK_ANDROID_SINK_HAS_ANDROID_LOG
#include <android/log.h>
#endif

// Déclarations stub pour non-Android
namespace nkentseu::android_sink_internal {
#if !NK_ANDROID_SINK_HAS_ANDROID_LOG
	const int ANDROID_LOG_VERBOSE = 1;
	const int ANDROID_LOG_DEBUG = 2;
	const int ANDROID_LOG_INFO = 3;
	const int ANDROID_LOG_WARN = 4;
	const int ANDROID_LOG_ERROR = 5;
	const int ANDROID_LOG_FATAL = 6;
#endif
}

// Vérifier les limites
static_assert(nkentseu::NkAndroidSink::GetMaxTagLength() >= 10, "TAG length trop court");
static_assert(nkentseu::NkAndroidSink::GetMaxTagLength() <= 23, "TAG length trop long");

// Function helper pour éviter les va_args
namespace {
	void LogToAndroid(int prio, const nkentseu::NkString &tag, const nkentseu::NkString &message) {
#if NK_ANDROID_SINK_HAS_ANDROID_LOG
		__android_log_print(prio, tag.CStr(), "%s", message.CStr());
#else
		const char* levelStr = "?";
		switch (prio) {
			case 1: levelStr = "V"; break;
			case 2: levelStr = "D"; break;
			case 3: levelStr = "I"; break;
			case 4: levelStr = "W"; break;
			case 5: levelStr = "E"; break;
			case 6: levelStr = "F"; break;
			default: break;
		}
		(void)::fprintf(stderr, "[%s/%s] %s\n", levelStr, tag.CStr(), message.CStr());
#endif
	}
}

// =========================================================================
// NAMESPACE: nkentseu::logger
// =========================================================================
namespace nkentseu {

	// =====================================================================
	// IMPLÉMENTATION: NkAndroidSink
	// =====================================================================

	/**
	 * @brief Constructeur avec tag Android
	 */
	NkAndroidSink::NkAndroidSink(const NkString &tag)
		: m_Tag(ValidateTag(tag)), m_ShortLogs(false) {
		m_Formatter = memory::NkMakeUnique<NkFormatter>(NkFormatter::NK_DEFAULT_PATTERN);
	}

	/**
	 * @brief Destructeur
	 */
	NkAndroidSink::~NkAndroidSink() {
		Flush();
	}

	/**
	 * @brief Logge un message via Android Log
	 */
	void NkAndroidSink::Log(const NkLogMessage &message) {
		if (!IsEnabled() || !ShouldLog(message.level)) {
			return;
		}

		logger_sync::NkScopedLock lock(m_Mutex);

		// Formater le message
		NkString formatted;
		if (m_ShortLogs) {
			// Logs courtes: juste le message
			formatted = message.message;
		} else {
			// Logs formatées: incluent timestamp, niveau, etc.
			formatted = m_Formatter->Format(message, false); // Pas de couleurs sur Android
		}

		// Convertir le niveau
		int androidLevel = ConvertToAndroidLogLevel(message.level);

		// Logger via Android
		WriteAndroidLog(androidLevel, m_Tag, formatted);
	}

	/**
	 * @brief Force l'écriture des données en attente
	 */
	void NkAndroidSink::Flush() {
		logger_sync::NkScopedLock lock(m_Mutex);
		// Le système Android log gère automatiquement le flush
		// Pas d'action supplémentaire requise
	}

	/**
	 * @brief Définit le formatter pour ce sink
	 */
	void NkAndroidSink::SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) {
		logger_sync::NkScopedLock lock(m_Mutex);
		m_Formatter = traits::NkMove(formatter);
	}

	/**
	 * @brief Définit le pattern de formatage
	 */
	void NkAndroidSink::SetPattern(const NkString &pattern) {
		logger_sync::NkScopedLock lock(m_Mutex);
		if (m_Formatter) {
			m_Formatter->SetPattern(pattern);
		}
	}

	/**
	 * @brief Obtient le formatter courant
	 */
	NkFormatter *NkAndroidSink::GetFormatter() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_Formatter.Get();
	}

	/**
	 * @brief Obtient le pattern courant
	 */
	NkString NkAndroidSink::GetPattern() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		if (m_Formatter) {
			return m_Formatter->GetPattern();
		}
		return "";
	}

	/**
	 * @brief Définit le tag Android
	 */
	void NkAndroidSink::SetTag(const NkString &tag) {
		logger_sync::NkScopedLock lock(m_Mutex);
		m_Tag = ValidateTag(tag);
	}

	/**
	 * @brief Obtient le tag Android courant
	 */
	NkString NkAndroidSink::GetTag() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_Tag;
	}

	/**
	 * @brief Active/désactive les logs courtes
	 */
	void NkAndroidSink::SetShortLogs(bool enable) {
		logger_sync::NkScopedLock lock(m_Mutex);
		m_ShortLogs = enable;
	}

	/**
	 * @brief Vérifie si les logs courtes sont activées
	 */
	bool NkAndroidSink::IsShortLogsEnabled() const {
		logger_sync::NkScopedLock lock(m_Mutex);
		return m_ShortLogs;
	}

	/**
	 * @brief Convertit un NkLogLevel en niveau Android
	 */
	int NkAndroidSink::ConvertToAndroidLogLevel(NkLogLevel level) const {
		using namespace android_sink_internal;

		switch (level) {
			case NkLogLevel::NK_TRACE:
				return ANDROID_LOG_VERBOSE;
			case NkLogLevel::NK_DEBUG:
				return ANDROID_LOG_DEBUG;
			case NkLogLevel::NK_INFO:
				return ANDROID_LOG_INFO;
			case NkLogLevel::NK_WARN:
				return ANDROID_LOG_WARN;
			case NkLogLevel::NK_ERROR:
				return ANDROID_LOG_ERROR;
			case NkLogLevel::NK_CRITICAL:
			case NkLogLevel::NK_FATAL:
				return ANDROID_LOG_FATAL;
			default:
				return ANDROID_LOG_INFO;
		}
	}

	/**
	 * @brief Valide et tronque le tag si nécessaire
	 */
	NkString NkAndroidSink::ValidateTag(const NkString &tag) const {
		if (tag.Empty()) {
			return EMPTY_TAG;
		}

		NkString validTag = tag;

		// Truncate si trop long
		if (validTag.Length() > MAX_TAG_LENGTH) {
			validTag = validTag.SubStr(0, MAX_TAG_LENGTH);
		}

		// Remplacer les caractères invalides
		for (char &c : validTag) {
			if (!::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-' && c != '.') {
				c = '_';
			}
		}

		return validTag;
	}

	/**
	 * @brief Écrit directement dans les logs Android
	 */
	void NkAndroidSink::WriteAndroidLog(int prio, const NkString &tag, const NkString &message) {
		// Limiter la taille du message pour éviter les truncations
		const usize MAX_LOG_SIZE = 4000; // Limite Android typique

		if (message.Length() <= MAX_LOG_SIZE) {
			LogToAndroid(prio, tag, message);
		} else {
			// Fragmenter le message si trop long
			usize offset = 0;
			int fragmentNumber = 0;

			while (offset < message.Length()) {
				usize chunkSize = message.Length() - offset;
				const usize maxChunkSize = MAX_LOG_SIZE - 50;
				if (chunkSize > maxChunkSize) {
					chunkSize = maxChunkSize;
				}
				NkString chunk = message.SubStr(offset, chunkSize);
				fragmentNumber++;

				char prefix[64];
				const int prefixLength = ::snprintf(prefix, sizeof(prefix), "[FRAGMENT %d] ", fragmentNumber);
				NkString fragmentedMessage;
				if (prefixLength > 0) {
					fragmentedMessage.Append(prefix, static_cast<usize>(prefixLength));
				}
				fragmentedMessage += chunk;
				LogToAndroid(prio, tag, fragmentedMessage);

				offset += chunkSize;
			}
		}
	}

} // namespace nkentseu
