// =============================================================================
// NKLogger/Sinks/NkDailyFileSink.h
// Sink pour l'écriture dans un fichier avec rotation automatique quotidienne.
//
// Design :
//  - Héritage de NkFileSink : réutilisation de toute l'infrastructure fichier
//  - Rotation par date : basculement vers backup quand la date change
//  - Heure de rotation configurable : déclenchement à H:M spécifié (défaut: 00:00)
//  - Conservation configurable : suppression automatique des backups trop anciens
//  - Thread-safe : synchronisation via mutex hérité de NkFileSink
//  - Point d'extension CheckRotation() override pour logique personnalisée
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la classe
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKDAILYFILESINK_H
#define NKENTSEU_NKDAILYFILESINK_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour la gestion du temps et des fichiers.
	// Dépendances projet pour le sink fichier de base et les utilitaires.

	#include <ctime>

	#include "NKCore/NkTypes.h"
	#include "NKContainers/String/NkString.h"
	#include "NKLogger/Sinks/NkFileSink.h"
	#include "NKLogger/NkLogMessage.h"
	#include "NKLogger/NkLogLevel.h"
	#include "NKLogger/NkLoggerApi.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// CLASSE : NkDailyFileSink
		// DESCRIPTION : Sink fichier avec rotation automatique par date
		// ---------------------------------------------------------------------
		/**
		 * @class NkDailyFileSink
		 * @brief Sink pour écriture persistante avec rotation automatique quotidienne
		 * @ingroup LoggerSinks
		 *
		 * NkDailyFileSink étend NkFileSink avec une stratégie de rotation temporelle :
		 *  - Rotation déclenchée quand la date système change ET l'heure de rotation est atteinte
		 *  - Fichiers renommés avec suffixe date : app.log → app.log.20240115
		 *  - Conservation configurable : suppression automatique des backups > m_MaxDays
		 *  - Rotation thread-safe via mutex hérité de NkFileSink
		 *
		 * Architecture :
		 *  - Hérite de NkFileSink : réutilisation complète de l'infrastructure fichier
		 *  - Override de CheckRotation() : point d'extension pour logique de rotation
		 *  - Cache de date m_CurrentDate : évite les recalculs fréquents de localtime
		 *  - Timestamp m_LastCheck : limite la vérification à une fois par minute
		 *
		 * Thread-safety :
		 *  - Toutes les méthodes publiques sont thread-safe via m_Mutex (hérité)
		 *  - La rotation est atomic : Close() → rename() → Open() protégés par mutex
		 *  - Safe pour usage depuis multiples threads simultanément
		 *
		 * Gestion des erreurs :
		 *  - Échec rename() : ignoré silencieusement, rotation partielle possible
		 *  - Échec Open() après rotation : fichier courant perdu, logging suspendu
		 *  - Pour robustesse : vérifier IsOpen() après Rotate() si critique
		 *
		 * @note La rotation est vérifiée après chaque Log() mais exécutée max une fois/minute
		 * @note Le format de suffixe est fixe : .YYYYMMDD (ex: .20240115)
		 *
		 * @example Usage basique
		 * @code
		 * // Rotation quotidienne à minuit, conservation de 7 jours
		 * auto dailySink = nkentseu::memory::MakeShared<nkentseu::NkDailyFileSink>(
		 *     "logs/app.log",   // Chemin de base
		 *     0,                // Heure de rotation: 00:00 (minuit)
		 *     0,                // Minute de rotation
		 *     7                 // Conserver 7 jours de backups
		 * );
		 *
		 * dailySink->SetLevel(nkentseu::NkLogLevel::NK_INFO);
		 * dailySink->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
		 *
		 * logger.AddSink(dailySink);
		 * @endcode
		 *
		 * @example Rotation manuelle forcée
		 * @code
		 * // Forcer la rotation avant une opération de maintenance
		 * if (auto* dailySink = dynamic_cast<nkentseu::NkDailyFileSink*>(fileSink.get())) {
		 *     dailySink->Rotate();  // Rotation immédiate indépendamment de la date
		 * }
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkDailyFileSink : public NkFileSink {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup DailySinkCtors Constructeurs de NkDailyFileSink
			 * @brief Initialisation avec configuration de rotation quotidienne
			 */

			/**
			 * @brief Constructeur avec paramètres de rotation quotidienne
			 * @ingroup DailySinkCtors
			 *
			 * @param filename Chemin du fichier de log de base (ex: "logs/app.log")
			 * @param hour Heure de rotation (0-23), défaut 0 pour minuit
			 * @param minute Minute de rotation (0-59), défaut 0
			 * @param maxDays Nombre maximum de jours à conserver (0 = illimité), défaut 0
			 * @post Hérite de NkFileSink : fichier ouvert en mode append par défaut
			 * @post m_RotationHour, m_RotationMinute, m_MaxDays initialisés
			 * @post m_CurrentDate initialisé à la date système courante
			 * @post m_LastCheck initialisé pour permettre vérification immédiate
			 * @note Thread-safe : construction via mutex hérité de NkFileSink
			 *
			 * @par Schéma de rotation :
			 * @code
			 * État initial : app.log (courant, date=2024-01-15)
			 * Après rotation à 2024-01-16 00:00 :
			 *   - app.log.20240115 (backup de la veille)
			 *   - app.log (nouveau fichier courant)
			 * Après 7 jours avec maxDays=7 :
			 *   - app.log.20240108 et plus anciens sont supprimés automatiquement
			 * @endcode
			 *
			 * @example
			 * @code
			 * // Rotation à 23:59, conservation illimitée
			 * nkentseu::NkDailyFileSink sink(
			 *     "var/log/myapp/service.log",
			 *     23,   // Heure: 23h
			 *     59,   // Minute: 59
			 *     0     // maxDays=0: conservation illimitée
			 * );
			 * @endcode
			 */
			NkDailyFileSink(const NkString& filename, int hour = 0, int minute = 0, usize maxDays = 0);

			/**
			 * @brief Destructeur : cleanup via NkFileSink
			 * @ingroup DailySinkCtors
			 *
			 * @post Destructeur de NkFileSink appelé : fermeture garantie du fichier
			 * @note Aucune logique supplémentaire : rotation gérée uniquement dans Log()
			 */
			~NkDailyFileSink() override;


			// -----------------------------------------------------------------
			// IMPLÉMENTATION DE L'INTERFACE NKISINK
			// -----------------------------------------------------------------
			/**
			 * @defgroup DailySinkInterface Implémentation de NkISink/NkFileSink
			 * @brief Méthodes requises avec logique de rotation quotidienne ajoutée
			 */

			/**
			 * @brief Écrit un message et vérifie la rotation quotidienne si nécessaire
			 * @param message Message de log à écrire
			 * @ingroup DailySinkInterface
			 *
			 * @note Appel d'abord NkFileSink::Log() pour écriture normale
			 * @note Puis CheckRotation() appelé implicitement via override dans NkFileSink::Log()
			 * @note Thread-safe : synchronisation via mutex hérité
			 *
			 * @par Performance :
			 *  - Vérification de date limitée à une fois par minute via m_LastCheck
			 *  - localtime() appelé uniquement si minute écoulée depuis dernière vérification
			 *  - Overhead minimal en régime normal : comparaison d'entiers uniquement
			 *
			 * @example
			 * @code
			 * nkentseu::NkLogMessage msg(
			 *     nkentseu::NkLogLevel::NK_INFO,
			 *     "Daily log entry"
			 * );
			 * dailySink.Log(msg);  // Écriture + vérification rotation automatique
			 * @endcode
			 */
			void Log(const NkLogMessage& message) override;


			// -----------------------------------------------------------------
			// CONFIGURATION DE LA ROTATION QUOTIDIENNE
			// -----------------------------------------------------------------
			/**
			 * @defgroup DailySinkConfig Configuration des Paramètres de Rotation
			 * @brief Méthodes pour ajuster la stratégie de rotation à runtime
			 */

			/**
			 * @brief Définit l'heure et la minute de déclenchement de la rotation
			 * @param hour Nouvelle heure de rotation (0-23)
			 * @param minute Nouvelle minute de rotation (0-59)
			 * @ingroup DailySinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex hérité
			 * @note Modification effective immédiatement pour les prochains Log()
			 * @note Ne déclenche pas de rotation rétroactive si l'heure est déjà passée
			 *
			 * @example
			 * @code
			 * // Changer la rotation pour 6h du matin au lieu de minuit
			 * dailySink.SetRotationTime(6, 0);
			 *
			 * // Utile pour aligner la rotation avec les fenêtres de maintenance
			 * @endcode
			 */
			void SetRotationTime(int hour, int minute);

			/**
			 * @brief Obtient l'heure de rotation configurée
			 * @return Heure de rotation (0-23)
			 * @ingroup DailySinkConfig
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex hérité
			 * @note Utile pour affichage de configuration ou validation de setup
			 *
			 * @example
			 * @code
			 * printf("Rotation scheduled at %02d:%02d daily\n",
			 *     dailySink.GetRotationHour(),
			 *     dailySink.GetRotationMinute());
			 * @endcode
			 */
			int GetRotationHour() const;

			/**
			 * @brief Obtient la minute de rotation configurée
			 * @return Minute de rotation (0-59)
			 * @ingroup DailySinkConfig
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex hérité
			 * @note Utile pour affichage de configuration ou validation de setup
			 *
			 * @example
			 * @code
			 * // Vérification que la rotation est bien configurée à minuit
			 * if (dailySink.GetRotationHour() == 0 && dailySink.GetRotationMinute() == 0) {
			 *     logger.Debug("Daily rotation configured at midnight");
			 * }
			 * @endcode
			 */
			int GetRotationMinute() const;

			/**
			 * @brief Définit le nombre maximum de jours de backups à conserver
			 * @param maxDays Nouveau nombre maximum (0 = conservation illimitée)
			 * @ingroup DailySinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex hérité
			 * @note Modification effective à la prochaine rotation ou CleanOldFiles()
			 * @note Réduction de maxDays ne supprime pas immédiatement les fichiers excédentaires
			 *
			 * @example
			 * @code
			 * // Conserver plus d'historique en période d'audit
			 * #ifdef AUDIT_MODE
			 *     dailySink.SetMaxDays(30);  // 30 jours au lieu de 7
			 * #endif
			 * @endcode
			 */
			void SetMaxDays(usize maxDays);

			/**
			 * @brief Obtient le nombre maximum de jours configuré pour la conservation
			 * @return Nombre maximum de jours à conserver (0 = illimité)
			 * @ingroup DailySinkConfig
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex hérité
			 * @note Utile pour affichage de configuration ou calcul d'espace disque requis
			 *
			 * @example
			 * @code
			 * // Estimation de l'espace disque maximum utilisé
			 * usize avgLogSizePerDay = EstimateDailyLogSize();  // Fonction métier
			 * usize maxSpace = dailySink.GetMaxDays() * avgLogSizePerDay;
			 * printf("Max disk usage: %zu bytes (%.2f GB)\n",
			 *     maxSpace, maxSpace / (1024.0 * 1024.0 * 1024.0));
			 * @endcode
			 */
			usize GetMaxDays() const;

			/**
			 * @brief Force la rotation du fichier indépendamment de la date courante
			 * @return true si rotation effectuée avec succès, false en cas d'échec
			 * @ingroup DailySinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex hérité
			 * @note Utile pour rotation manuelle avant maintenance ou backup externe
			 * @note Ne met pas à jour m_CurrentDate : la prochaine vérification utilisera la date système
			 *
			 * @example
			 * @code
			 * // Rotation avant un déploiement pour archiver les logs de la version précédente
			 * if (dailySink.Rotate()) {
			 *     logger.Info("Logs rotated before deployment");
			 * } else {
			 *     logger.Error("Failed to rotate logs before deployment");
			 * }
			 * @endcode
			 */
			bool Rotate();


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
			// -----------------------------------------------------------------
		private:


			// -----------------------------------------------------------------
			// MÉTHODES PRIVÉES D'IMPLÉMENTATION
			// -----------------------------------------------------------------
			/**
			 * @defgroup DailySinkPrivate Méthodes Privées
			 * @brief Fonctions internes pour logique de rotation quotidienne
			 */

			/**
			 * @brief Vérifie si la rotation est nécessaire et l'effectue si oui
			 * @ingroup DailySinkPrivate
			 *
			 * @note Override de NkFileSink::CheckRotation() : point d'extension principal
			 * @note Comparaison : date système vs m_CurrentDate + heure de rotation
			 * @note Limitation : vérification exécutée max une fois par minute via m_LastCheck
			 * @note Appel PerformRotation() si condition de rotation remplie
			 * @note Thread-safe : appelé depuis Log() qui détient déjà le mutex
			 *
			 * @warning Ne pas appeler directement sans mutex acquis : comportement indéfini
			 */
			void CheckRotation() override;

			/**
			 * @brief Effectue la rotation complète des fichiers pour la date courante
			 * @ingroup DailySinkPrivate
			 *
			 * @note Algorithme :
			 *   1. CloseUnlocked() : fermer le fichier courant
			 *   2. Générer le nom de backup : {base}.YYYYMMDD
			 *   3. Renommer courant → backup via rename()
			 *   4. CleanOldFiles() : supprimer les backups > m_MaxDays si configuré
			 *   5. OpenUnlocked() : rouvrir un nouveau fichier courant vide
			 * @note Thread-safe : doit être appelé avec m_Mutex déjà acquis
			 * @note Gestion d'erreurs : rename() échecs ignorés silencieusement pour robustesse
			 *
			 * @warning Ne pas appeler directement sans mutex acquis : risque de corruption
			 */
			void PerformRotation();

			/**
			 * @brief Supprime les fichiers de backup plus anciens que m_MaxDays
			 * @ingroup DailySinkPrivate
			 *
			 * @note Parcours du répertoire contenant le fichier courant
			 * @note Filtrage par pattern : {baseName}.YYYYMMDD uniquement
			 * @note Extraction de date via NkExtractRotatedDate() pour validation
			 * @note Suppression via remove() des fichiers dont la date < (now - m_MaxDays)
			 * @note Thread-safe : doit être appelé avec m_Mutex déjà acquis
			 * @note Multiplateforme : implémentation Windows (FindFirstFile) et POSIX (opendir)
			 *
			 * @warning Ne pas appeler directement sans mutex acquis : risque de race condition
			 */
			void CleanOldFiles();

			/**
			 * @brief Génère le chemin de fichier de backup pour une date donnée
			 * @param date Structure tm contenant la date cible
			 * @return Chaîne NkString contenant le chemin complet (ex: "app.log.20240115")
			 * @ingroup DailySinkPrivate
			 *
			 * @note Format de suffixe fixe : .%04d%02d%02d → .YYYYMMDD
			 * @note Thread-safe : lecture de m_Filename via GetFilenameUnlocked() (mutex déjà acquis)
			 * @note Utilise snprintf pour formatage sûr de la date
			 *
			 * @example
			 * @code
			 * // Pour filename = "logs/app.log" et date = 2024-01-15 :
			 * GetFilenameForDate(date) → "logs/app.log.20240115"
			 * @endcode
			 */
			NkString GetFilenameForDate(const tm& date) const;

			/**
			 * @brief Extrait la date d'un nom de fichier de backup
			 * @param filename Nom de fichier à parser (ex: "app.log.20240115")
			 * @return Structure tm avec la date extraite, ou tm invalide si échec
			 * @ingroup DailySinkPrivate
			 *
			 * @note Format attendu : {baseName}.YYYYMMDD (8 chiffres après le point)
			 * @note Validation : année >= 1900, mois 1-12, jour 1-31 via mktime()
			 * @note Retourne tm avec tm_year = -1 en cas d'échec de parsing
			 * @note Thread-safe : aucune mutation d'état partagé
			 *
			 * @example
			 * @code
			 * // Parsing réussi :
			 * ExtractDateFromFilename("app.log.20240115") → tm{tm_year=124, tm_mon=0, tm_mday=15, ...}
			 *
			 * // Parsing échoué (format invalide) :
			 * ExtractDateFromFilename("app.log.invalid") → tm{tm_year=-1, ...}
			 * @endcode
			 */
			tm ExtractDateFromFilename(const NkString& filename) const;

			/**
			 * @brief Vérifie si une date est plus ancienne que le seuil de conservation
			 * @param date Structure tm contenant la date à évaluer
			 * @return true si date < (now - m_MaxDays), false sinon
			 * @ingroup DailySinkPrivate
			 *
			 * @note Comparaison via mktime() pour conversion en time_t portable
			 * @note Gestion de m_MaxDays = 0 : retourne toujours false (conservation illimitée)
			 * @note Gestion de date invalide (tm_year < 0) : retourne false par sécurité
			 * @note Thread-safe : aucune mutation d'état partagé
			 *
			 * @example
			 * @code
			 * // Avec m_MaxDays = 7 et date système = 2024-01-15 :
			 * IsDateTooOld(2024-01-10) → false (encore dans la fenêtre)
			 * IsDateTooOld(2024-01-07) → true (à supprimer)
			 * @endcode
			 */
			bool IsDateTooOld(const tm& date) const;


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PRIVÉES (ÉTAT DE ROTATION QUOTIDIENNE)
			// -----------------------------------------------------------------
			/**
			 * @defgroup DailySinkMembers Membres de Données
			 * @brief État interne pour gestion de la rotation quotidienne (non exposé publiquement)
			 */

			/// @brief Heure de rotation configurée (0-23)
			/// @ingroup DailySinkMembers
			/// @note Déclenche la rotation quand l'heure système atteint cette valeur
			/// @note Modifiable via SetRotationTime(), lecture via GetRotationHour()
			int m_RotationHour;

			/// @brief Minute de rotation configurée (0-59)
			/// @ingroup DailySinkMembers
			/// @note Déclenche la rotation quand la minute système atteint cette valeur
			/// @note Modifiable via SetRotationTime(), lecture via GetRotationMinute()
			int m_RotationMinute;

			/// @brief Nombre maximum de jours de backups à conserver
			/// @ingroup DailySinkMembers
			/// @note 0 = conservation illimitée (aucun cleanup automatique)
			/// @note Modifiable via SetMaxDays(), lecture via GetMaxDays()
			usize m_MaxDays;

			/// @brief Date du fichier courant (année/mois/jour uniquement)
			/// @ingroup DailySinkMembers
			/// @note Utilisé pour comparaison avec la date système dans CheckRotation()
			/// @note Mis à jour après chaque rotation réussie
			tm m_CurrentDate;

			/// @brief Timestamp de la dernière vérification de rotation (nanosecondes)
			/// @ingroup DailySinkMembers
			/// @note Limitation : vérification exécutée max une fois par minute
			/// @note Mis à jour à chaque appel de CheckRotation()
			uint64 m_LastCheck;


		}; // class NkDailyFileSink


	} // namespace nkentseu


#endif // NKENTSEU_NKDAILYFILESINK_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKDAILYFILESINK.H
// =============================================================================
// Ce fichier fournit l'implémentation de sink avec rotation automatique quotidienne.
// Il combine persistance fichier et gestion d'historique temporel pour production.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration basique avec rotation quotidienne
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDailyFileSink.h>

	void SetupDailyRotatingLogging() {
		// Rotation quotidienne à minuit, conservation de 7 jours
		auto dailySink = nkentseu::memory::MakeShared<nkentseu::NkDailyFileSink>(
			"logs/app.log",              // Chemin de base
			0,                           // Heure de rotation: 00:00 (minuit)
			0,                           // Minute de rotation
			7                            // Conserver 7 jours de backups
		);

		// Configuration optionnelle
		dailySink->SetLevel(nkentseu::NkLogLevel::NK_INFO);
		dailySink->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");

		// Ajout au logger global
		nkentseu::NkLog::Instance().AddSink(dailySink);

		// Logging normal : rotation automatique quand la date change à minuit
		logger.Info("Application started with daily rotating file logging");
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Rotation manuelle avant opération critique
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDailyFileSink.h>

	void PreDeploymentLogRotation() {
		auto& logger = nkentseu::NkLog::Instance();

		// Recherche du sink quotidien parmi les sinks configurés
		nkentseu::NkDailyFileSink* dailySink = nullptr;
		for (usize i = 0; i < logger.GetSinkCount(); ++i) {
			// Note : accès aux sinks nécessite API supplémentaire ou casting
			// Cet exemple suppose un accès direct pour illustration
		}

		if (dailySink && dailySink->Rotate()) {
			logger.Info("Logs rotated before deployment - old logs archived with date suffix");
		} else {
			logger.Warn("Failed to rotate logs before deployment");
		}
	}
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Configuration dynamique via fichier de config
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDailyFileSink.h>
	#include <NKCore/NkConfig.h>

	nkentseu::NkSinkPtr CreateDailySinkFromConfig(const nkentseu::NkString& section) {
		using namespace nkentseu;

		// Lecture des paramètres depuis configuration
		NkString filepath = core::Config::GetString(section + ".filepath", "logs/app.log");
		int rotationHour = static_cast<int>(core::Config::GetInt64(section + ".rotationHour", 0));
		int rotationMinute = static_cast<int>(core::Config::GetInt64(section + ".rotationMinute", 0));
		usize maxDays = core::Config::GetUint64(section + ".maxDays", 7);
		NkString pattern = core::Config::GetString(section + ".pattern", NkFormatter::NK_DEFAULT_PATTERN);
		NkLogLevel level = NkLogLevelFromString(core::Config::GetString(section + ".level", "info"));

		// Validation des paramètres de rotation
		if (rotationHour < 0 || rotationHour > 23) rotationHour = 0;
		if (rotationMinute < 0 || rotationMinute > 59) rotationMinute = 0;

		// Création et configuration du sink
		auto sink = memory::MakeShared<NkDailyFileSink>(filepath, rotationHour, rotationMinute, maxDays);
		sink->SetPattern(pattern);
		sink->SetLevel(level);

		return sink;
	}

	// Usage au startup :
	// auto dailySink = CreateDailySinkFromConfig("logging.sinks.daily");
	// logger.AddSink(dailySink);
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Monitoring de l'état de rotation
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDailyFileSink.h>

	void LogDailyRotationStatus(const nkentseu::NkDailyFileSink& sink) {
		using namespace nkentseu;

		// Informations de configuration
		NkString filename = sink.GetFilename();
		int rotHour = sink.GetRotationHour();
		int rotMinute = sink.GetRotationMinute();
		usize maxDays = sink.GetMaxDays();

		// Logging de statut
		logger.Info("Daily rotating sink status: file={0}, rotation={1:02d}:{2:02d}, maxDays={3}",
			filename,
			rotHour,
			rotMinute,
			maxDays == 0 ? "unlimited" : NkString::Format("%zu", maxDays));
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Testing de la rotation avec manipulation de date
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDailyFileSink.h>
	#include <NKLogger/NkLogMessage.h>
	#include <gtest/gtest.h>

	TEST(NkDailyFileSinkTest, RotationTrigger) {
		const nkentseu::NkString testPath = "test_logs/daily_rotation_test.log";

		{
			// Rotation à 00:00, conservation de 3 jours pour test
			nkentseu::NkDailyFileSink sink(testPath, 0, 0, 3);
			ASSERT_TRUE(sink.IsOpen()) << "Failed to open test log file";

			// Message de test
			nkentseu::NkLogMessage msg(nkentseu::NkLogLevel::NK_INFO, "Test daily entry");
			sink.Log(msg);
			sink.Flush();

			// Vérification que le fichier courant existe
			EXPECT_TRUE(NkFileExists(testPath));

			// Note : tester la rotation automatique nécessite de manipuler l'heure système
			// ou d'appeler Rotate() manuellement pour validation
		}

		// Cleanup
		NkFileDelete(testPath);
		// Supprimer aussi les backups potentiels : testPath.YYYYMMDD
	}
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Rotation avec heure personnalisée pour fenêtre de maintenance
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkDailyFileSink.h>

	void SetupMaintenanceWindowRotation() {
		using namespace nkentseu;

		// Rotation à 4h du matin pendant la fenêtre de maintenance
		auto dailySink = memory::MakeShared<NkDailyFileSink>(
			"logs/app.log",
			4,   // Heure: 04:00
			0,   // Minute: 00
			14   // Conserver 2 semaines de backups
		);

		// Configuration pour lisibilité
		dailySink->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] [%n] %v");

		logger.Info("Daily rotation configured at 04:00 with 14-day retention");
	}
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. PERFORMANCE DE CheckRotation() :
	   - Vérification limitée à une fois par minute via m_LastCheck
	   - localtime() appelé uniquement si minute écoulée depuis dernière vérification
	   - Comparaison de date : simple comparaison d'entiers (tm_year, tm_mon, tm_mday)
	   - Overhead négligeable en régime normal : < 1 µs par appel Log()

	2. ATOMICITÉ DE PerformRotation() :
	   - Séquence Close() → rename() → Open() doit être exécutée atomiquement
	   - Garantit par le fait que cette méthode est appelée uniquement depuis
	     Log() ou Rotate() qui acquièrent déjà m_Mutex
	   - Risk : crash pendant rotation → fichier backup potentiellement partiel
	   - Mitigation : rotation déclenchée uniquement après écriture complète du message

	3. GESTION DES ERREURS DE rename() :
	   - rename() peut échouer : permissions, disque plein, fichier verrouillé
	   - Implémentation : (void)::rename() ignore le code de retour
	   - Pour production critique : logger l'erreur et tenter fallback (copie+suppression)
	   - Alternative : retourner bool pour propagation d'erreur contrôlée

	4. FORMAT DE NOMMAGE DES BACKUPS :
	   - Format fixe : {base_filename}.YYYYMMDD (ex: app.log.20240115)
	   - Avantage : tri chronologique naturel, facile à parser pour outils externes
	   - Alternative : timestamp dans le nom pour précision sub-jour (ex: app.log.20240115-043022)

	5. NETTOYAGE DES ANCIENS FICHIERS :
	   - CleanOldFiles() appelé après chaque rotation si m_MaxDays > 0
	   - Parcours du répertoire : O(n) avec n = nombre de fichiers dans le dossier
	   - Pour grands répertoires : envisager cache ou index pour optimisation
	   - Suppression via remove() : échecs ignorés silencieusement pour robustesse

	6. THREAD-SAFETY HÉRITÉE :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLock (hérité)
	   - CheckRotation() et PerformRotation() appelés depuis Log() avec lock déjà acquis
	   - Safe pour partage entre multiples loggers via NkSharedPtr

	7. COMPATIBILITÉ MULTIPLATEFORME :
	   - rename()/remove() : standards C, portables Windows/POSIX
	   - localtime_r/localtime_s : abstraction via #ifdef pour thread-safety
	   - Directory listing : FindFirstFile (Windows) vs opendir/readdir (POSIX)
	   - Chemins : NkString gère / et \, NkEnsureParentDirectory() dans NkFileSink

	8. EXTENSIBILITÉ FUTURES :
	   - Rotation par semaine/mois : étendre CheckRotation() avec logique calendrier
	   - Compression : compresser les backups après rotation pour économiser l'espace
	   - Upload cloud : envoyer les backups vers S3/autre après rotation pour archivage distant
	   - Hook personnalisé : callback avant/après rotation pour intégration monitoring

	9. TESTING :
	   - Utiliser de petites valeurs maxDays pour tests unitaires (1-3 jours)
	   - Tester les cas limites : maxDays = 0, heure de rotation invalide, chemin inexistant
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
	   - Pour tester la rotation automatique : mock de localtime() ou manipulation d'horloge
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================