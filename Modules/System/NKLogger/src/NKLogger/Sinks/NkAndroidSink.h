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
//   auto sink = std::make_unique<NkAndroidSink>("MyApp");
//   logger->AddSink(std::move(sink));
// TAGS: Android, Logging, JNI, NDK
// -----------------------------------------------------------------------------

#pragma once

#include "NKLogger/NkSink.h"
#include "NKPlatform/NkPlatformDetect.h"
#include "NKCore/NkTypes.h"
#include <string>
#include <mutex>

#ifdef NKENTSEU_PLATFORM_ANDROID
#include <android/log.h>
#endif

// Forward declaration pour éviter les conflits
#ifdef NKENTSEU_PLATFORM_ANDROID
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
#ifndef NKENTSEU_PLATFORM_ANDROID
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
	//   auto sink = std::make_unique<NkAndroidSink>("MyApp");
	//   sink->SetFormatter(...);
	//   logger->AddSink(std::move(sink));
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
			explicit NkAndroidSink(const std::string &tag);

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
			void SetFormatter(std::unique_ptr<NkFormatter> formatter) override;

			/**
			 * @brief Définit le pattern de formatage
			 */
			void SetPattern(const std::string &pattern) override;

			/**
			 * @brief Obtient le formatter courant
			 */
			NkFormatter *GetFormatter() const override;

			/**
			 * @brief Obtient le pattern courant
			 */
			std::string GetPattern() const override;

			// ---------------------------------------------------------------
			// CONFIGURATION SPÉCIFIQUE À ANDROID
			// ---------------------------------------------------------------

			/**
			 * @brief Définit le tag Android pour les logs
			 * @param tag Nouveau tag (max 23 caractères sur certaines versions Android)
			 * EXEMPLE: SetTag("app/core")
			 */
			void SetTag(const std::string &tag);

			/**
			 * @brief Obtient le tag Android courant
			 * @return Tag utilisé pour les logs
			 */
			std::string GetTag() const;

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

            static constexpr core::usize GetMaxTagLength() {
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
			std::string ValidateTag(const std::string &tag) const;

			/**
			 * @brief Écrit directement dans les logs Android
			 * @param prio Priorité Android
			 * @param tag Tag du log
			 * @param message Message à logger
			 */
			void WriteAndroidLog(int prio, const std::string &tag, const std::string &message);

			// ---------------------------------------------------------------
			// VARIABLES MEMBRE PRIVÉES
			// ---------------------------------------------------------------

			/// Formatter pour ce sink
			std::unique_ptr<NkFormatter> m_Formatter;

			/// Tag Android pour les logs
			std::string m_Tag;

			/// Utiliser les logs courtes (sans formatage)
			bool m_ShortLogs;

			/// Mutex pour la synchronisation thread-safe
			mutable std::mutex m_Mutex;

			/// Taille maximale du tag Android (23 caractères selon spec Android)
			static constexpr core::usize MAX_TAG_LENGTH = 23;

			/// Message vide pour les cas d'erreur
			static constexpr const char *EMPTY_TAG = "NkAndroidLog";
	};

} // namespace nkentseu