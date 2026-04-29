// =============================================================================
// NKLogger/Sinks/NkRotatingFileSink.h
// Sink pour l'écriture dans un fichier avec rotation automatique basée sur la taille.
//
// Design :
//  - Héritage de NkFileSink : réutilisation de toute l'infrastructure fichier
//  - Rotation automatique : basculement vers backup quand taille max atteinte
//  - Gestion de l'historique : conservation d'un nombre configurable de fichiers
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

#ifndef NKENTSEU_NKROTATINGFILESINK_H
#define NKENTSEU_NKROTATINGFILESINK_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour la gestion de fichiers et la synchronisation.
	// Dépendances projet pour le sink fichier de base et les utilitaires.

	#include "NKCore/NkTypes.h"
	#include "NKContainers/String/NkString.h"
	#include "NKLogger/Sinks/NkFileSink.h"
	#include "NKLogger/NkLogMessage.h"
	#include "NKLogger/NkLoggerApi.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// CLASSE : NkRotatingFileSink
		// DESCRIPTION : Sink fichier avec rotation automatique par taille
		// ---------------------------------------------------------------------
		/**
		 * @class NkRotatingFileSink
		 * @brief Sink pour écriture persistante avec rotation automatique basée sur la taille
		 * @ingroup LoggerSinks
		 *
		 * NkRotatingFileSink étend NkFileSink avec une stratégie de rotation :
		 *  - Rotation déclenchée quand la taille du fichier atteint m_MaxSize
		 *  - Fichiers renommés : app.log → app.log.0 → app.log.1 → ... → app.log.N
		 *  - Conservation de m_MaxFiles fichiers historiques (le plus ancien est supprimé)
		 *  - Rotation thread-safe via mutex hérité de NkFileSink
		 *
		 * Architecture :
		 *  - Hérite de NkFileSink : réutilisation complète de l'infrastructure fichier
		 *  - Override de CheckRotation() : point d'extension pour logique de rotation
		 *  - Cache de taille m_CurrentSize : évite les appels stat() fréquents
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
		 * @note La rotation est déclenchée après chaque Log() si m_CurrentSize >= m_MaxSize
		 * @note m_CurrentSize est mis à jour via GetFileSize() : peut être légèrement obsolète
		 *
		 * @example Usage basique
		 * @code
		 * // Rotation à 50 MB, conservation de 5 fichiers historiques
		 * auto rotatingSink = nkentseu::memory::MakeShared<nkentseu::NkRotatingFileSink>(
		 *     "logs/app.log",      // Chemin de base
		 *     50 * 1024 * 1024,    // 50 MB max par fichier
		 *     5                    // Conserver app.log.0 à app.log.4
		 * );
		 *
		 * rotatingSink->SetLevel(nkentseu::NkLogLevel::NK_INFO);
		 * rotatingSink->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
		 *
		 * logger.AddSink(rotatingSink);
		 * @endcode
		 *
		 * @example Rotation manuelle forcée
		 * @code
		 * // Forcer la rotation avant une opération de maintenance
		 * if (auto* rotSink = dynamic_cast<nkentseu::NkRotatingFileSink*>(fileSink.get())) {
		 *     rotSink->Rotate();  // Rotation immédiate indépendamment de la taille
		 * }
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkRotatingFileSink : public NkFileSink {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup RotatingSinkCtors Constructeurs de NkRotatingFileSink
			 * @brief Initialisation avec configuration de rotation
			 */

			/**
			 * @brief Constructeur avec paramètres de rotation
			 * @ingroup RotatingSinkCtors
			 *
			 * @param filename Chemin du fichier de log de base (ex: "logs/app.log")
			 * @param maxSize Taille maximum en octets avant déclenchement de rotation
			 * @param maxFiles Nombre maximum de fichiers historiques à conserver (0 = désactivé)
			 * @post Hérite de NkFileSink : fichier ouvert en mode append par défaut
			 * @post m_MaxSize, m_MaxFiles initialisés, m_CurrentSize = 0
			 * @note Thread-safe : construction via mutex hérité de NkFileSink
			 *
			 * @par Schéma de rotation :
			 * @code
			 * État initial : app.log (courant)
			 * Après 1ère rotation : app.log.0 (ancien), app.log (nouveau)
			 * Après 2ème rotation : app.log.1, app.log.0, app.log (nouveau)
			 * ...
			 * Après Nème rotation (N >= maxFiles) : suppression de app.log.(maxFiles-1)
			 * @endcode
			 *
			 * @example
			 * @code
			 * // Rotation à 100 MB, 10 fichiers historiques
			 * nkentseu::NkRotatingFileSink sink(
			 *     "var/log/myapp/service.log",
			 *     100 * 1024 * 1024,  // 100 MB
			 *     10                   // Conserver .0 à .9
			 * );
			 * @endcode
			 */
			NkRotatingFileSink(const NkString& filename, usize maxSize, usize maxFiles);

			/**
			 * @brief Destructeur : cleanup via NkFileSink
			 * @ingroup RotatingSinkCtors
			 *
			 * @post Destructeur de NkFileSink appelé : fermeture garantie du fichier
			 * @note Aucune logique supplémentaire : rotation gérée uniquement dans Log()
			 */
			~NkRotatingFileSink() override;


			// -----------------------------------------------------------------
			// IMPLÉMENTATION DE L'INTERFACE NKISINK
			// -----------------------------------------------------------------
			/**
			 * @defgroup RotatingSinkInterface Implémentation de NkISink/NkFileSink
			 * @brief Méthodes requises avec logique de rotation ajoutée
			 */

			/**
			 * @brief Écrit un message et vérifie la rotation si nécessaire
			 * @param message Message de log à écrire
			 * @ingroup RotatingSinkInterface
			 *
			 * @note Appel d'abord NkFileSink::Log() pour écriture normale
			 * @note Puis mise à jour de m_CurrentSize via GetFileSize()
			 * @note CheckRotation() appelé implicitement via override dans NkFileSink::Log()
			 * @note Thread-safe : synchronisation via mutex hérité
			 *
			 * @par Performance :
			 *  - GetFileSize() utilise stat() : appel système potentiellement coûteux
			 *  - Pour haute fréquence : envisager estimation incrémentale de la taille
			 *  - Cache m_CurrentSize réduit mais n'élimine pas totalement les stat()
			 *
			 * @example
			 * @code
			 * nkentseu::NkLogMessage msg(
			 *     nkentseu::NkLogLevel::NK_INFO,
			 *     "High volume log entry"
			 * );
			 * rotatingSink.Log(msg);  // Écriture + vérification rotation automatique
			 * @endcode
			 */
			void Log(const NkLogMessage& message) override;


			// -----------------------------------------------------------------
			// CONFIGURATION DE LA ROTATION
			// -----------------------------------------------------------------
			/**
			 * @defgroup RotatingSinkConfig Configuration des Paramètres de Rotation
			 * @brief Méthodes pour ajuster la stratégie de rotation à runtime
			 */

			/**
			 * @brief Définit la taille maximum déclenchant la rotation
			 * @param maxSize Nouvelle limite en octets
			 * @ingroup RotatingSinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex hérité
			 * @note Modification effective immédiatement pour les prochains Log()
			 * @note Ne déclenche pas de rotation rétroactive si taille actuelle > nouveau max
			 *
			 * @example
			 * @code
			 * // Réduire la limite en mode debug pour tester la rotation fréquemment
			 * #ifdef NKENTSEU_DEBUG
			 *     rotatingSink.SetMaxSize(1 * 1024 * 1024);  // 1 MB pour tests
			 * #endif
			 * @endcode
			 */
			void SetMaxSize(usize maxSize);

			/**
			 * @brief Obtient la taille maximum configurée pour la rotation
			 * @return Limite en octets
			 * @ingroup RotatingSinkConfig
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex hérité
			 * @note Utile pour affichage de configuration ou validation de setup
			 *
			 * @example
			 * @code
			 * printf("Rotation threshold: %zu bytes (%.2f MB)\n",
			 *     rotatingSink.GetMaxSize(),
			 *     rotatingSink.GetMaxSize() / (1024.0 * 1024.0));
			 * @endcode
			 */
			usize GetMaxSize() const;

			/**
			 * @brief Définit le nombre maximum de fichiers historiques à conserver
			 * @param maxFiles Nouveau nombre maximum (0 = désactiver la conservation)
			 * @ingroup RotatingSinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex hérité
			 * @note Modification effective à la prochaine rotation
			 * @note Réduction de maxFiles ne supprime pas immédiatement les fichiers excédentaires
			 *
			 * @example
			 * @code
			 * // Conserver plus d'historique en production
			 * #ifndef NKENTSEU_DEBUG
			 *     rotatingSink.SetMaxFiles(20);  // 20 fichiers au lieu de 5
			 * #endif
			 * @endcode
			 */
			void SetMaxFiles(usize maxFiles);

			/**
			 * @brief Obtient le nombre maximum de fichiers historiques configuré
			 * @return Nombre maximum de fichiers à conserver
			 * @ingroup RotatingSinkConfig
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex hérité
			 * @note Utile pour affichage de configuration ou calcul d'espace disque requis
			 *
			 * @example
			 * @code
			 * // Estimation de l'espace disque maximum utilisé
			 * usize maxSpace = rotatingSink.GetMaxFiles() * rotatingSink.GetMaxSize();
			 * printf("Max disk usage: %zu bytes (%.2f GB)\n",
			 *     maxSpace, maxSpace / (1024.0 * 1024.0 * 1024.0));
			 * @endcode
			 */
			usize GetMaxFiles() const;

			/**
			 * @brief Force la rotation du fichier indépendamment de la taille courante
			 * @return true si rotation effectuée avec succès, false en cas d'échec
			 * @ingroup RotatingSinkConfig
			 *
			 * @note Thread-safe : synchronisé via m_Mutex hérité
			 * @note Utile pour rotation manuelle avant maintenance ou backup
			 * @note Réinitialise m_CurrentSize à 0 après rotation réussie
			 *
			 * @example
			 * @code
			 * // Rotation avant un déploiement pour archiver les logs de la version précédente
			 * if (rotatingSink.Rotate()) {
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
			 * @defgroup RotatingSinkPrivate Méthodes Privées
			 * @brief Fonctions internes pour logique de rotation
			 */

			/**
			 * @brief Vérifie si la rotation est nécessaire et l'effectue si oui
			 * @ingroup RotatingSinkPrivate
			 *
			 * @note Override de NkFileSink::CheckRotation() : point d'extension principal
			 * @note Comparaison : m_CurrentSize >= m_MaxSize pour déclenchement
			 * @note Appel PerformRotation() si condition remplie
			 * @note Thread-safe : appelé depuis Log() qui détient déjà le mutex
			 *
			 * @warning Ne pas appeler directement sans mutex acquis : comportement indéfini
			 */
			void CheckRotation() override;

			/**
			 * @brief Effectue la rotation complète des fichiers
			 * @ingroup RotatingSinkPrivate
			 *
			 * @note Algorithme :
			 *   1. Close() : fermer le fichier courant
			 *   2. Décaler les backups : .N-1 → .N, ..., .0 → .1
			 *   3. Renommer courant → .0
			 *   4. Supprimer .maxFiles si existe (dépassement de limite)
			 *   5. Open() : rouvrir un nouveau fichier courant vide
			 *   6. Réinitialiser m_CurrentSize à 0
			 * @note Thread-safe : doit être appelé avec m_Mutex déjà acquis
			 * @note Gestion d'erreurs : rename() échecs ignorés silencieusement pour robustesse
			 *
			 * @warning Ne pas appeler directement sans mutex acquis : risque de corruption
			 */
			void PerformRotation();

			/**
			 * @brief Génère le chemin de fichier pour un index de rotation donné
			 * @param index Index du backup (0 = plus récent, maxFiles-1 = plus ancien)
			 * @return Chaîne NkString contenant le chemin complet (ex: "app.log.2")
			 * @ingroup RotatingSinkPrivate
			 *
			 * @note Format : {base_filename}.{index}
			 * @note Thread-safe : lecture de m_Filename protégée par mutex appelant
			 * @note Utilise snprintf pour formatage sûr de l'index
			 *
			 * @example
			 * @code
			 * // Pour filename = "logs/app.log" :
			 * GetFilenameForIndex(0) → "logs/app.log.0"
			 * GetFilenameForIndex(3) → "logs/app.log.3"
			 * @endcode
			 */
			NkString GetFilenameForIndex(usize index) const;


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PRIVÉES (ÉTAT DE ROTATION)
			// -----------------------------------------------------------------
			/**
			 * @defgroup RotatingSinkMembers Membres de Données
			 * @brief État interne pour gestion de la rotation (non exposé publiquement)
			 */

			/// @brief Taille maximum en octets déclenchant la rotation
			/// @ingroup RotatingSinkMembers
			/// @note Modifiable via SetMaxSize(), lecture via GetMaxSize()
			usize m_MaxSize;

			/// @brief Nombre maximum de fichiers historiques à conserver
			/// @ingroup RotatingSinkMembers
			/// @note 0 = désactiver la conservation (rotation sans backup)
			/// @note Modifiable via SetMaxFiles(), lecture via GetMaxFiles()
			usize m_MaxFiles;

			/// @brief Estimation de la taille courante du fichier en octets
			/// @ingroup RotatingSinkMembers
			/// @note Mis à jour après chaque Log() via GetFileSize()
			/// @note Peut être légèrement obsolète : utilisé pour décision de rotation uniquement
			usize m_CurrentSize;


		}; // class NkRotatingFileSink


	} // namespace nkentseu


#endif // NKENTSEU_NKROTATINGFILESINK_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKROTATINGFILESINK.H
// =============================================================================
// Ce fichier fournit l'implémentation de sink avec rotation automatique.
// Il combine persistance fichier et gestion d'historique pour production.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration basique avec rotation
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkRotatingFileSink.h>

	void SetupRotatingLogging() {
		// Rotation à 100 MB, conservation de 10 fichiers historiques
		auto rotatingSink = nkentseu::memory::MakeShared<nkentseu::NkRotatingFileSink>(
			"logs/app.log",              // Chemin de base
			100 * 1024 * 1024,           // 100 MB max par fichier
			10                           // Conserver .0 à .9
		);

		// Configuration optionnelle
		rotatingSink->SetLevel(nkentseu::NkLogLevel::NK_INFO);
		rotatingSink->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");

		// Ajout au logger global
		nkentseu::NkLog::Instance().AddSink(rotatingSink);

		// Logging normal : rotation automatique quand taille atteinte
		logger.Info("Application started with rotating file logging");
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Rotation manuelle avant opération critique
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkRotatingFileSink.h>

	void PreDeploymentLogRotation() {
		auto& logger = nkentseu::NkLog::Instance();

		// Recherche du sink rotatif parmi les sinks configurés
		nkentseu::NkRotatingFileSink* rotatingSink = nullptr;
		for (usize i = 0; i < logger.GetSinkCount(); ++i) {
			// Note : accès aux sinks nécessite API supplémentaire ou casting
			// Cet exemple suppose un accès direct pour illustration
		}

		if (rotatingSink && rotatingSink->Rotate()) {
			logger.Info("Logs rotated before deployment - old logs archived");
		} else {
			logger.Warn("Failed to rotate logs before deployment");
		}
	}
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Configuration dynamique via fichier de config
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkRotatingFileSink.h>
	#include <NKCore/NkConfig.h>

	nkentseu::NkSinkPtr CreateRotatingSinkFromConfig(const nkentseu::NkString& section) {
		using namespace nkentseu;

		// Lecture des paramètres depuis configuration
		NkString filepath = core::Config::GetString(section + ".filepath", "logs/app.log");
		usize maxSize = core::Config::GetUint64(section + ".maxSize", 50 * 1024 * 1024);
		usize maxFiles = core::Config::GetUint64(section + ".maxFiles", 5);
		NkString pattern = core::Config::GetString(section + ".pattern", NkFormatter::NK_DEFAULT_PATTERN);
		NkLogLevel level = NkLogLevelFromString(core::Config::GetString(section + ".level", "info"));

		// Création et configuration du sink
		auto sink = memory::MakeShared<NkRotatingFileSink>(filepath, maxSize, maxFiles);
		sink->SetPattern(pattern);
		sink->SetLevel(level);

		return sink;
	}

	// Usage au startup :
	// auto rotatingSink = CreateRotatingSinkFromConfig("logging.sinks.rotating");
	// logger.AddSink(rotatingSink);
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Monitoring de l'état de rotation
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkRotatingFileSink.h>

	void LogRotationStatus(const nkentseu::NkRotatingFileSink& sink) {
		using namespace nkentseu;

		// Informations de configuration
		NkString filename = sink.GetFilename();
		usize maxSize = sink.GetMaxSize();
		usize maxFiles = sink.GetMaxFiles();
		usize currentSize = sink.GetFileSize();  // Taille réelle sur disque

		// Calculs dérivés
		double usagePercent = (maxSize > 0)
			? (100.0 * currentSize / maxSize)
			: 0.0;
		usize estimatedMaxSpace = maxSize * maxFiles;

		// Logging de statut
		logger.Info("Rotating sink status: file={0}, size={1:.2}MB/{2:.2}MB ({3:.1}%), history={4} files, maxSpace={5:.2}GB",
			filename,
			currentSize / (1024.0 * 1024.0),
			maxSize / (1024.0 * 1024.0),
			usagePercent,
			maxFiles,
			estimatedMaxSpace / (1024.0 * 1024.0 * 1024.0));
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Testing de la rotation avec génération de logs
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkRotatingFileSink.h>
	#include <NKLogger/NkLogMessage.h>
	#include <gtest/gtest.h>

	TEST(NkRotatingFileSinkTest, RotationTrigger) {
		const nkentseu::NkString testPath = "test_logs/rotation_test.log";
		constexpr nkentseu::usize TEST_MAX_SIZE = 1024;  // 1 KB pour test rapide
		constexpr nkentseu::usize TEST_MAX_FILES = 3;

		{
			nkentseu::NkRotatingFileSink sink(testPath, TEST_MAX_SIZE, TEST_MAX_FILES);
			ASSERT_TRUE(sink.IsOpen());

			// Générer des logs jusqu'à déclenchement de rotation
			nkentseu::NkLogMessage msg(nkentseu::NkLogLevel::NK_INFO, nkentseu::NkString(200, 'X'));
			for (int i = 0; i < 10; ++i) {
				sink.Log(msg);
			}
			sink.Flush();

			// Vérifier que des fichiers de backup ont été créés
			EXPECT_TRUE(NkFileExists(testPath));  // Fichier courant
			// Note : vérification précise des backups dépend du timing exact de rotation
		}

		// Cleanup
		for (usize i = 0; i <= TEST_MAX_FILES; ++i) {
			nkentseu::NkString path = testPath + "." + nkentseu::NkString::Format("%zu", i);
			if (NkFileExists(path)) {
				NkFileDelete(path);
			}
		}
		NkFileDelete(testPath);
	}
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Rotation avec estimation de taille incrémentale (optimisation)
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkRotatingFileSink.h>

	class OptimizedRotatingSink : public nkentseu::NkRotatingFileSink {
	public:
		OptimizedRotatingSink(const nkentseu::NkString& path, nkentseu::usize maxSize, nkentseu::usize maxFiles)
			: nkentseu::NkRotatingFileSink(path, maxSize, maxFiles) {
		}

	protected:
		// Override pour estimation incrémentale au lieu de stat() à chaque Log()
		void Log(const nkentseu::NkLogMessage& message) override {
			// Écrire le message via la méthode parente
			nkentseu::NkFileSink::Log(message);

			// Estimation incrémentale : ajouter la taille approximative du message
			// Note : approximation car formatage peut varier légèrement
			const nkentseu::usize approxSize = message.message.Length() + 64;  // + metadata
			m_CurrentSize += approxSize;

			// Vérifier la rotation avec l'estimation
			CheckRotation();

			// Recalibrer périodiquement avec la taille réelle (tous les 100 logs)
			static nkentseu::usize logCount = 0;
			if (++logCount >= 100) {
				m_CurrentSize = GetFileSize();  // Recalage via stat()
				logCount = 0;
			}
		}
	};
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. PERFORMANCE DE GetFileSize() :
	   - Utilise stat() : appel système potentiellement coûteux (~1-10 µs)
	   - Appelé après chaque Log() : peut impacter le throughput en haute fréquence
	   - Optimisation : estimation incrémentale avec recalage périodique (voir Exemple 6)
	   - Alternative : buffer de taille avec flush périodique vers stat()

	2. ATOMICITÉ DE LA ROTATION :
	   - Séquence Close() → rename() → Open() doit être atomic vis-à-vis des autres threads
	   - Garantit par m_Mutex hérité de NkFileSink : toutes les méthodes publiques lockent
	   - Risk : si crash pendant rotation, fichier .0 peut être partiellement écrit
	   - Mitigation : rotation déclenchée uniquement après écriture complète du message

	3. GESTION DES ERREURS DE RENAME() :
	   - rename() peut échouer (permissions, disque plein, fichier verrouillé)
	   - Implémentation actuelle : échec ignoré silencieusement pour robustesse
	   - Pour production critique : logger l'erreur et tenter fallback (copie+suppression)

	4. SCHÉMA DE NOMMAGE DES BACKUPS :
	   - Format : {base}.{index} où 0 = plus récent, maxFiles-1 = plus ancien
	   - Avantage : ordre chronologique prévisible, facile à parser pour outils externes
	   - Alternative : timestamp dans le nom pour archivage externe (ex: app.log.20240115-143022)

	5. ESPACE DISQUE ET NETTOYAGE :
	   - Espace maximum utilisé : m_MaxSize * m_MaxFiles (plus fichier courant)
	   - Pour gestion proactive : monitorer GetFileSize() et alerte si > seuil
	   - Nettoyage externe possible : scripts cron pour archiver/supprimer les .N anciens

	6. THREAD-SAFETY HÉRITÉE :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLock (hérité)
	   - CheckRotation() et PerformRotation() appelés depuis Log() qui détient déjà le lock
	   - Safe pour partage entre multiples loggers via NkSharedPtr

	7. EXTENSIBILITÉ FUTURES :
	   - Rotation par temps : override CheckRotation() avec comparaison de timestamps
	   - Compression : compresser les backups .N après rotation pour économiser l'espace
	   - Upload cloud : envoyer les fichiers .N vers S3/autre après rotation pour archivage distant

	8. TESTING :
	   - Utiliser de petites valeurs maxSize pour tests unitaires (1 KB au lieu de 50 MB)
	   - Tester les cas limites : maxSize = 0, maxFiles = 0, chemin inexistant, permissions refusées
	   - Valider le thread-safety avec tests concurrents (TSan, helgrind, etc.)
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================