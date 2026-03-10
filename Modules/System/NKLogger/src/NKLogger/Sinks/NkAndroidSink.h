// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/Sinks/NkAndroidSink.h
// DESCRIPTION: Sink pour les logs Android utilisant le Android Log Framework.
// AUTEUR: Rihen
// DATE: 2026
// PLATEFORME: Android
// DÉPENDANCE: <android/log.h>
// NOTE: Ce sink compile uniquement sur Android. Il utilise __android_log_print()
// pour logger directement dans le système de logs Android.
// Les logs peuvent être consultés avec: adb logcat
// USAGE:
//   auto sink = memory::NkMakeUnique<NkAndroidSink>("MyApp");
//   logger->AddSink(traits::NkMove(sink));
// TAGS: Android, Logging, JNI, NDK
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkSink.h"
#include "NKLogger/NkSync.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"


#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"

#if !defined(NK_ANDROID_SINK_HAS_ANDROID_LOG)
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define NK_ANDROID_SINK_HAS_ANDROID_LOG 1
#elif defined(__has_include)
#if __has_include(<android/log.h>)
#define NK_ANDROID_SINK_HAS_ANDROID_LOG 1
#else
#define NK_ANDROID_SINK_HAS_ANDROID_LOG 0
#endif
#else
#define NK_ANDROID_SINK_HAS_ANDROID_LOG 0
#endif
#endif

#if NK_ANDROID_SINK_HAS_ANDROID_LOG
#include <android/log.h>
#endif

// Forward declaration pour éviter les conflits
#if NK_ANDROID_SINK_HAS_ANDROID_LOG
    #ifndef ANDROID_LOG_UNKNOWN
        #define ANDROID_LOG_UNKNOWN 0
        #define ANDROID_LOG_VERBOSE 1
        #define ANDROID_LOG_DEBUG 2
        #define ANDROID_LOG_INFO 3
        #define ANDROID_LOG_WARN 4
        #define ANDROID_LOG_ERROR 5
        #define ANDROID_LOG_FATAL 6
        #define ANDROID_LOG_SILENT 7
    #endif
#endif

// Private forward declaration (ne pas inclure android/log.h sur non-Android)
namespace nkentseu::android_sink_internal {
	// Déclarations stub pour la compilation non-Android
#if !NK_ANDROID_SINK_HAS_ANDROID_LOG
	extern const int ANDROID_LOG_VERBOSE;
	extern const int ANDROID_LOG_DEBUG;
	extern const int ANDROID_LOG_INFO;
	extern const int ANDROID_LOG_WARN;
	extern const int ANDROID_LOG_ERROR;
	extern const int ANDROID_LOG_FATAL;
#endif
}

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {

	// -------------------------------------------------------------------------
	// CLASSE: NkAndroidSink
	// DESCRIPTION: Sink pour les logs Android
	// DETAILS:
	//   - Utilise le Android Log Framework (android::log.h)
	//   - Les logs sont envoyés au système de logs Android
	//   - Les logs s'affichent dans logcat: adb logcat
	//   - Support des niveaux de log (Trace, Debug, Info, Warning, Error, Fatal)
	//   - Thread-safe avec mutex
	//   - Peut compiler sur non-Android (configuration stub)
	// EXEMPLE:
	//   auto sink = memory::NkMakeUnique<NkAndroidSink>("MyApp");
	//   sink->SetFormatter(...);
	//   logger->AddSink(traits::NkMove(sink));
	// -------------------------------------------------------------------------
	class NKLOGGER_API NkAndroidSink : public NkISink {
		public:
			// ---------------------------------------------------------------
			// CONSTRUCTEURS
			// ---------------------------------------------------------------

			/**
			 * @brief Constructeur avec tag Android
			 * @param tag Tag à utiliser pour les logs Android (ex: "MyApp")
			 *            Ce tag s'affichera dans logcat
			 * EXEMPLE: NkAndroidSink("com.example.myapp")
			 */
			explicit NkAndroidSink(const NkString &tag);

			/**
			 * @brief Destructeur
			 */
			~NkAndroidSink() override;

			// ---------------------------------------------------------------
			// IMPLÉMENTATION DE NkISink
			// ---------------------------------------------------------------

			/**
			 * @brief Logge un message via le système Android
			 * @param message Message à logger
			 * DÉTAIL: Le message est formaté puis envoyé via __android_log_print()
			 */
			void Log(const NkLogMessage &message) override;

			/**
			 * @brief Force l'écriture des données en attente
			 * NOTE: Le système Android log gère automatiquement le flush
			 */
			void Flush() override;

			/**
			 * @brief Définit le formatter pour ce sink
			 */
			void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) override;

			/**
			 * @brief Définit le pattern de formatage
			 */
			void SetPattern(const NkString &pattern) override;

			/**
			 * @brief Obtient le formatter courant
			 */
			NkFormatter *GetFormatter() const override;

			/**
			 * @brief Obtient le pattern courant
			 */
			NkString GetPattern() const override;

			// ---------------------------------------------------------------
			// CONFIGURATION SPÉCIFIQUE À ANDROID
			// ---------------------------------------------------------------

			/**
			 * @brief Définit le tag Android pour les logs
			 * @param tag Nouveau tag (max 23 caractères sur certaines versions Android)
			 * EXEMPLE: SetTag("app/core")
			 */
			void SetTag(const NkString &tag);

			/**
			 * @brief Obtient le tag Android courant
			 * @return Tag utilisé pour les logs
			 */
			NkString GetTag() const;

			/**
			 * @brief Active/désactive les logs courtes (sans formatage)
			 * @param enable true pour logs courtes, false pour logs formatées
			 * DÉTAIL: Les logs courtes affichent juste le message brut
			 *         Les logs formatées incluent le timestamp, niveau, etc.
			 */
			void SetShortLogs(bool enable);

			/**
			 * @brief Vérifie si les logs courtes sont activées
			 * @return true si logs courtes activées
			 */
			bool IsShortLogsEnabled() const;

            static constexpr usize GetMaxTagLength() {
                return MAX_TAG_LENGTH;
            }
		private:
			// ---------------------------------------------------------------
			// MÉTHODES PRIVÉES
			// ---------------------------------------------------------------

			/**
			 * @brief Convertit un NkLogLevel en niveau Android
			 * @param level Niveau Nk
			 * @return Niveau Android (__android_log_print priority)
			 */
			int ConvertToAndroidLogLevel(NkLogLevel level) const;

			/**
			 * @brief Valide et tronque le tag si nécessaire
			 * @param tag Tag à valider
			 * @return Tag validé (max 24 caractères)
			 */
			NkString ValidateTag(const NkString &tag) const;

			/**
			 * @brief Écrit directement dans les logs Android
			 * @param prio Priorité Android
			 * @param tag Tag du log
			 * @param message Message à logger
			 */
			void WriteAndroidLog(int prio, const NkString &tag, const NkString &message);

			// ---------------------------------------------------------------
			// VARIABLES MEMBRE PRIVÉES
			// ---------------------------------------------------------------

			/// Formatter pour ce sink
			memory::NkUniquePtr<NkFormatter> m_Formatter;

			/// Tag Android pour les logs
			NkString m_Tag;

			/// Utiliser les logs courtes (sans formatage)
			bool m_ShortLogs;

			/// Mutex pour la synchronisation thread-safe
			mutable logger_sync::NkMutex m_Mutex;

			/// Taille maximale du tag Android (23 caractères selon spec Android)
			static constexpr usize MAX_TAG_LENGTH = 23;

			/// Message vide pour les cas d'erreur
			static constexpr const char *EMPTY_TAG = "NkAndroidLog";
	};

} // namespace nkentseu
