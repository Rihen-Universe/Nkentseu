// =============================================================================
// NKLogger/Sinks/NkFileSink.h
// Sink pour l'écriture des messages de log dans un fichier.
//
// Design :
//  - Implémentation concrète de NkISink pour persistance fichier
//  - Gestion thread-safe via NKThreading/NkMutex pour écritures concurrentes
//  - Création automatique des répertoires parents si inexistants
//  - Support des modes truncate/append configurable à runtime
//  - Point d'extension CheckRotation() pour implémentation de rotation
//  - Export contrôlé via NKENTSEU_LOGGER_CLASS_EXPORT sur la classe
//  - Aucune macro d'export sur les méthodes (héritée de la classe)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#pragma once

#ifndef NKENTSEU_NKFILESINK_H
#define NKENTSEU_NKFILESINK_H


	// -------------------------------------------------------------------------
	// SECTION 1 : EN-TÊTES ET DÉPENDANCES
	// -------------------------------------------------------------------------
	// Inclusions standards pour la gestion de fichiers et la synchronisation.
	// Dépendances projet pour l'interface de sink, le formatage et les utilitaires.

	#include <cstdio>
	#include <sys/stat.h>

	#include "NKCore/NkTypes.h"
	#include "NKContainers/String/NkString.h"
	#include "NKMemory/NkUniquePtr.h"
	#include "NKThreading/NkMutex.h"
	#include "NKLogger/NkSink.h"
	#include "NKLogger/NkLoggerFormatter.h"
	#include "NKLogger/NkLoggerApi.h"


	// -------------------------------------------------------------------------
	// SECTION 2 : DÉCLARATION DU NAMESPACE PRINCIPAL
	// -------------------------------------------------------------------------
	// Tous les symboles du module logger sont dans le namespace nkentseu.
	// Pas de sous-namespace pour simplifier l'usage et l'intégration.

	namespace nkentseu {


		// ---------------------------------------------------------------------
		// CLASSE : NkFileSink
		// DESCRIPTION : Implémentation de sink pour écriture dans un fichier
		// ---------------------------------------------------------------------
		/**
		 * @class NkFileSink
		 * @brief Sink pour l'écriture persistante des messages de log dans un fichier
		 * @ingroup LoggerSinks
		 *
		 * NkFileSink fournit une destination de log vers un fichier sur disque avec :
		 *  - Ouverture automatique du fichier à la construction
		 *  - Création des répertoires parents si inexistants
		 *  - Écriture thread-safe via mutex interne
		 *  - Modes truncate/append configurables à runtime
		 *  - Point d'extension virtuel CheckRotation() pour rotation de logs
		 *  - Flush explicite ou automatique via buffering contrôlé
		 *
		 * Architecture :
		 *  - Hérite de NkISink : réutilisation du filtrage par niveau et configuration
		 *  - Utilise NkFormatter pour formatage des messages avant écriture
		 *  - Gère le cycle de vie du FILE* avec RAII via constructeur/destructeur
		 *
		 * Thread-safety :
		 *  - Toutes les méthodes publiques sont thread-safe via m_Mutex
		 *  - Les méthodes Unlocked() sont protégées et appellables uniquement avec lock acquis
		 *  - Safe pour usage depuis multiples threads simultanément
		 *
		 * Gestion des erreurs :
		 *  - Échec d'ouverture : Log() devient no-op silencieux jusqu'à réouverture
		 *  - Erreur d'écriture : ignorée silencieusement pour ne pas crasher l'app
		 *  - Pour robustesse : vérifier IsOpen() après construction si critique
		 *
		 * @note Le buffering est désactivé par défaut (_IONBF) pour persistance immédiate
		 * @note Pour performance : envisager buffering avec Flush() périodique si nécessaire
		 *
		 * @example Usage basique
		 * @code
		 * // Création avec append (défaut)
		 * nkentseu::NkFileSink fileSink("logs/app.log");
		 *
		 * // Configuration optionnelle
		 * fileSink.SetLevel(nkentseu::NkLogLevel::NK_INFO);
		 * fileSink.SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
		 *
		 * // Ajout à un logger
		 * logger.AddSink(nkentseu::memory::MakeShared<nkentseu::NkFileSink>("debug.log"));
		 * @endcode
		 *
		 * @example Rotation de fichier via héritage
		 * @code
		 * class RotatingFileSink : public nkentseu::NkFileSink {
		 * public:
		 *     RotatingFileSink(const nkentseu::NkString& path, nkentseu::usize maxSize)
		 *         : nkentseu::NkFileSink(path), m_MaxSize(maxSize) {}
		 *
		 * protected:
		 *     void CheckRotation() override {
		 *         if (GetFileSize() >= m_MaxSize) {
		 *             RotateFile();  // Implémentation personnalisée
		 *         }
		 *     }
		 *
		 * private:
		 *     nkentseu::usize m_MaxSize;
		 *     void RotateFile() { /\* ... *\/ }
		 * };
		 * @endcode
		 */
		class NKENTSEU_LOGGER_CLASS_EXPORT NkFileSink : public NkISink {


			// -----------------------------------------------------------------
			// SECTION 3 : MEMBRES PUBLICS
			// -----------------------------------------------------------------
		public:


			// -----------------------------------------------------------------
			// CONSTRUCTEURS ET DESTRUCTEUR
			// -----------------------------------------------------------------
			/**
			 * @defgroup FileSinkCtors Constructeurs de NkFileSink
			 * @brief Initialisation avec configuration du fichier cible
			 */

			/**
			 * @brief Constructeur avec chemin de fichier et mode d'ouverture
			 * @ingroup FileSinkCtors
			 *
			 * @param filename Chemin du fichier de log (relatif ou absolu)
			 * @param truncate true pour écraser le fichier existant, false pour append (défaut)
			 * @post Répertoire parent créé si inexistant via NkEnsureParentDirectory()
			 * @post Fichier ouvert en mode "wb" (truncate) ou "ab" (append)
			 * @post Formatter par défaut configuré avec NK_DEFAULT_PATTERN
			 * @note Thread-safe : création du mutex interne et ouverture fichier protégées
			 *
			 * @example
			 * @code
			 * // Append mode : ajoute à la fin du fichier existant
			 * nkentseu::NkFileSink appendSink("logs/app.log", false);
			 *
			 * // Truncate mode : écrase le fichier pour nouveau démarrage
			 * nkentseu::NkFileSink freshSink("logs/app.log", true);
			 *
			 * // Chemin avec sous-répertoires : créés automatiquement
			 * nkentseu::NkFileSink nestedSink("var/log/myapp/debug.log");
			 * @endcode
			 */
			explicit NkFileSink(const NkString& filename, bool truncate = false);

			/**
			 * @brief Destructeur : fermeture garantie du fichier
			 * @ingroup FileSinkCtors
			 *
			 * @post Close() appelé pour libération du FILE* et flush des buffers
			 * @note Garantit qu'aucune donnée en buffer n'est perdue à la destruction
			 * @note Thread-safe : acquisition du mutex avant fermeture
			 */
			~NkFileSink() override;


			// -----------------------------------------------------------------
			// IMPLÉMENTATION DE L'INTERFACE NKISINK
			// -----------------------------------------------------------------
			/**
			 * @defgroup FileSinkInterface Implémentation de NkISink
			 * @brief Méthodes requises par l'interface de base des sinks
			 */

			/**
			 * @brief Écrit un message formaté dans le fichier de log
			 * @param message Message de log contenant les données et métadonnées
			 * @ingroup FileSinkInterface
			 *
			 * @note Filtrage précoce : retour immédiat si !IsEnabled() ou !ShouldLog()
			 * @note Formatage via m_Formatter->Format() si configuré
			 * @note Écriture avec newline automatique après chaque message
			 * @note Thread-safe : acquisition du mutex pour écriture exclusive
			 * @note CheckRotation() appelé après écriture pour support extension rotation
			 *
			 * @par Comportement en cas d'erreur :
			 *  - Fichier non ouvert : tentative de réouverture automatique
			 *  - Échec fwrite/fputc : ignoré silencieusement pour robustesse
			 *  - Pour debugging : vérifier IsOpen() et GetFileSize() après Log()
			 *
			 * @example
			 * @code
			 * nkentseu::NkLogMessage msg(
			 *     nkentseu::NkLogLevel::NK_INFO,
			 *     "User login successful",
			 *     "auth.cpp", 42, "HandleLogin"
			 * );
			 * fileSink.Log(msg);  // Écriture thread-safe avec formatage
			 * @endcode
			 */
			void Log(const NkLogMessage& message) override;

			/**
			 * @brief Force l'écriture immédiate des buffers vers le disque
			 * @ingroup FileSinkInterface
			 *
			 * @post fflush() appelé sur le FILE* pour vidage des buffers C stdio
			 * @note Critique pour : crash recovery, debugging temps-réel, audits
			 * @note Thread-safe : acquisition du mutex avant flush
			 * @note Peut être un no-op si buffering désactivé (défaut : _IONBF)
			 *
			 * @example
			 * @code
			 * // Flush après message critique pour persistance garantie
			 * logger.Error("Critical failure detected");
			 * fileSink.Flush();  // Force écriture disque immédiate
			 * @endcode
			 */
			void Flush() override;

			/**
			 * @brief Définit le formatter utilisé par ce sink
			 * @param formatter Pointeur unique vers le nouveau formatter à adopter
			 * @ingroup FileSinkInterface
			 *
			 * @note Transfert de propriété : le sink devient propriétaire du formatter
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Le formatter précédent est automatiquement détruit (RAII)
			 *
			 * @example
			 * @code
			 * // Formatter personnalisé pour format compact
			 * auto fmt = nkentseu::memory::MakeUnique<nkentseu::NkFormatter>("%L: %v");
			 * fileSink.SetFormatter(std::move(fmt));
			 * @endcode
			 */
			void SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) override;

			/**
			 * @brief Définit le pattern de formatage via création interne de formatter
			 * @param pattern Chaîne de pattern style spdlog à parser
			 * @ingroup FileSinkInterface
			 *
			 * @note Méthode de convenance : crée ou met à jour m_Formatter avec le pattern
			 * @note Équivalent à SetFormatter(MakeUnique<NkFormatter>(pattern))
			 * @note Thread-safe : synchronisé via m_Mutex
			 *
			 * @example
			 * @code
			 * // Pattern avec timestamp et niveau pour lisibilité
			 * fileSink.SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");
			 *
			 * // Pattern minimal pour logs machine-parseable
			 * fileSink.SetPattern("%v");
			 * @endcode
			 */
			void SetPattern(const NkString& pattern) override;

			/**
			 * @brief Obtient le formatter courant utilisé par ce sink
			 * @return Pointeur brut vers le formatter, ou nullptr si non configuré
			 * @ingroup FileSinkInterface
			 *
			 * @note Retourne un pointeur non-possédé : ne pas delete
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Le pointeur peut devenir invalide après SetFormatter() ou destruction
			 *
			 * @example
			 * @code
			 * if (auto* fmt = fileSink.GetFormatter()) {
			 *     nkentseu::NkString preview = fmt->Format(sampleMessage);
			 *     printf("Preview: %s\n", preview.CStr());
			 * }
			 * @endcode
			 */
			NkFormatter* GetFormatter() const override;

			/**
			 * @brief Obtient le pattern de formatage courant
			 * @return Chaîne NkString contenant le pattern, ou vide si non configuré
			 * @ingroup FileSinkInterface
			 *
			 * @note Retour par valeur : copie du pattern interne (safe pour usage prolongé)
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour sauvegarde de configuration ou affichage de debug
			 *
			 * @example
			 * @code
			 * // Sauvegarde de la configuration courante
			 * nkentseu::NkString currentPattern = fileSink.GetPattern();
			 * Config::SetString("logging.file.pattern", currentPattern.CStr());
			 * @endcode
			 */
			NkString GetPattern() const override;


			// -----------------------------------------------------------------
			// CONFIGURATION SPÉCIFIQUE AU FICHIER
			// -----------------------------------------------------------------
			/**
			 * @defgroup FileSinkConfig Configuration du Fichier
			 * @brief Méthodes pour gestion du fichier cible et mode d'ouverture
			 */

			/**
			 * @brief Ouvre le fichier cible (si non déjà ouvert)
			 * @return true si ouvert avec succès, false en cas d'échec
			 * @ingroup FileSinkConfig
			 *
			 * @note Thread-safe : acquisition du mutex avant opération
			 * @note Crée le répertoire parent si inexistant via NkEnsureParentDirectory()
			 * @note Mode d'ouverture déterminé par m_Truncate : "wb" ou "ab"
			 * @note Buffering désactivé (_IONBF) pour persistance immédiate
			 *
			 * @example
			 * @code
			 * nkentseu::NkFileSink sink("logs/app.log");
			 * if (!sink.IsOpen()) {
			 *     if (sink.Open()) {
			 *         logger.Info("File sink reopened successfully");
			 *     } else {
			 *         logger.Error("Failed to reopen log file");
			 *     }
			 * }
			 * @endcode
			 */
			bool Open();

			/**
			 * @brief Ferme le fichier cible et libère le FILE*
			 * @ingroup FileSinkConfig
			 *
			 * @post FILE* fermé via fclose(), m_FileStream mis à nullptr
			 * @note Thread-safe : acquisition du mutex avant fermeture
			 * @note Appelée automatiquement dans le destructeur ~NkFileSink()
			 * @note Safe à appeler multiples fois : no-op si déjà fermé
			 *
			 * @example
			 * @code
			 * // Fermeture temporaire pour rotation manuelle
			 * fileSink.Close();
			 * RotateLogFile();  // Opération externe sur le fichier
			 * fileSink.Open();  // Réouverture pour reprise du logging
			 * @endcode
			 */
			void Close();

			/**
			 * @brief Vérifie si le fichier cible est actuellement ouvert
			 * @return true si m_FileStream != nullptr, false sinon
			 * @ingroup FileSinkConfig
			 *
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour debugging ou validation avant Log() manuel
			 * @note Retourne l'état courant : peut changer après Open()/Close()
			 *
			 * @example
			 * @code
			 * if (!fileSink.IsOpen()) {
			 *     logger.Warn("Log file not open, attempting recovery");
			 *     fileSink.Open();
			 * }
			 * @endcode
			 */
			bool IsOpen() const;

			/**
			 * @brief Obtient le chemin du fichier cible
			 * @return Copie de la chaîne NkString contenant le chemin
			 * @ingroup FileSinkConfig
			 *
			 * @note Retour par valeur : copie safe pour usage prolongé
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Chemin relatif ou absolu tel que fourni au constructeur/SetFilename()
			 *
			 * @example
			 * @code
			 * nkentseu::NkString path = fileSink.GetFilename();
			 * printf("Logging to: %s\n", path.CStr());
			 * @endcode
			 */
			NkString GetFilename() const;

			/**
			 * @brief Définit un nouveau chemin de fichier cible
			 * @param filename Nouveau chemin du fichier de log
			 * @ingroup FileSinkConfig
			 *
			 * @post Ancien fichier fermé, nouveau fichier ouvert avec même mode
			 * @post Répertoire parent créé si inexistant
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Utile pour rotation manuelle ou changement dynamique de destination
			 *
			 * @example
			 * @code
			 * // Rotation quotidienne : nouveau fichier par date
			 * nkentseu::NkString today = GetDateString();  // ex: "2024-01-15"
			 * fileSink.SetFilename(nkentseu::NkString::Format("logs/app-%s.log", today.CStr()));
			 * @endcode
			 */
			void SetFilename(const NkString& filename);

			/**
			 * @brief Obtient la taille actuelle du fichier sur disque
			 * @return Taille en octets, ou 0 si fichier inexistant/inaccessible
			 * @ingroup FileSinkConfig
			 *
			 * @note Utilise stat() pour interrogation système de fichiers
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Peut retourner une taille > réalité si buffers non flushés
			 * @note Utile pour implémentation de rotation basée sur la taille
			 *
			 * @example
			 * @code
			 * // Rotation si fichier > 100 MB
			 * constexpr nkentseu::usize MAX_LOG_SIZE = 100 * 1024 * 1024;
			 * if (fileSink.GetFileSize() >= MAX_LOG_SIZE) {
			 *     RotateLogFile();
			 * }
			 * @endcode
			 */
			usize GetFileSize() const;

			/**
			 * @brief Définit le mode d'ouverture du fichier (truncate vs append)
			 * @param truncate true pour écraser (mode "wb"), false pour ajouter (mode "ab")
			 * @ingroup FileSinkConfig
			 *
			 * @note Si fichier déjà ouvert : fermeture et réouverture avec nouveau mode
			 * @note Thread-safe : synchronisé via m_Mutex
			 * @note Utile pour réinitialiser les logs au démarrage d'une session
			 *
			 * @example
			 * @code
			 * // Nouveau fichier pour chaque exécution en mode debug
			 * #ifdef NKENTSEU_DEBUG
			 *     fileSink.SetTruncate(true);  // Écrase à chaque démarrage
			 * #endif
			 * @endcode
			 */
			void SetTruncate(bool truncate);

			/**
			 * @brief Obtient le mode d'ouverture courant
			 * @return true si mode truncate ("wb"), false si mode append ("ab")
			 * @ingroup FileSinkConfig
			 *
			 * @note Retourne la valeur de m_Truncate
			 * @note Thread-safe : lecture protégée via m_Mutex
			 * @note Utile pour affichage de configuration ou validation de setup
			 *
			 * @example
			 * @code
			 * if (fileSink.GetTruncate()) {
			 *     logger.Debug("Log file configured in truncate mode");
			 * } else {
			 *     logger.Debug("Log file configured in append mode");
			 * }
			 * @endcode
			 */
			bool GetTruncate() const;


			// -----------------------------------------------------------------
			// SECTION 4 : MEMBRES PROTÉGÉS (POUR HÉRITAGE ET EXTENSION)
			// -----------------------------------------------------------------
		protected:


			// -----------------------------------------------------------------
			// MÉTHODES PROTÉGÉES UNLOCKED (APPELABLES AVEC LOCK ACQUIS)
			// -----------------------------------------------------------------
			/**
			 * @defgroup FileSinkUnlocked Méthodes Unlocked (Usage Interne)
			 * @brief Variantes sans acquisition de mutex pour usage avec lock déjà détenu
			 *
			 * Ces méthodes sont conçues pour être appelées uniquement lorsque
			 * m_Mutex est déjà acquis par l'appelant. Elles évitent la double
			 * acquisition de mutex dans les méthodes publiques qui les utilisent.
			 *
			 * @warning Ne pas appeler directement sans avoir acquis m_Mutex au préalable
			 * @warning Comportement indéfini en cas d'appel concurrent non synchronisé
			 */

			/**
			 * @brief Ouvre le fichier sans acquérir le mutex (prérequis : lock détenu)
			 * @return true si ouvert avec succès, false en cas d'échec
			 * @ingroup FileSinkUnlocked
			 *
			 * @note Délègue à OpenFile() pour logique d'ouverture réelle
			 * @note À utiliser uniquement dans des méthodes qui ont déjà locké m_Mutex
			 *
			 * @example
			 * @code
			 * void NkFileSink::SetFilename(const NkString& filename) {
			 *     threading::NkScopedLock lock(m_Mutex);  // Acquisition du lock
			 *     CloseUnlocked();  // Appel unlocked car lock déjà détenu
			 *     m_Filename = filename;
			 *     OpenUnlocked();   // Appel unlocked car lock déjà détenu
			 * }
			 * @endcode
			 */
			bool OpenUnlocked();

			/**
			 * @brief Ferme le fichier sans acquérir le mutex (prérequis : lock détenu)
			 * @ingroup FileSinkUnlocked
			 *
			 * @note Ferme m_FileStream via fclose() et met à nullptr
			 * @note À utiliser uniquement dans des méthodes qui ont déjà locké m_Mutex
			 * @note Safe à appeler multiples fois : no-op si m_FileStream == nullptr
			 */
			void CloseUnlocked();

			/**
			 * @brief Retourne le nom de fichier sans acquérir le mutex (prérequis : lock détenu)
			 * @return Copie de m_Filename
			 * @ingroup FileSinkUnlocked
			 *
			 * @note Retour par valeur : copie safe pour usage prolongé
			 * @note À utiliser uniquement dans des méthodes qui ont déjà locké m_Mutex
			 */
			NkString GetFilenameUnlocked() const;


			// -----------------------------------------------------------------
			// POINT D'EXTENSION POUR ROTATION DE FICHIER
			// -----------------------------------------------------------------
			/**
			 * @defgroup FileSinkExtension Points d'Extension
			 * @brief Méthodes virtuelles pour personnalisation par héritage
			 */

			/**
			 * @brief Vérifie et gère la rotation de fichier si nécessaire
			 * @ingroup FileSinkExtension
			 *
			 * @note Méthode appelée après chaque Log() pour vérification conditionnelle
			 * @note Implémentation par défaut : no-op (pas de rotation)
			 * @note À override dans les classes dérivées pour implémenter une stratégie de rotation
			 *
			 * @par Stratégies de rotation courantes :
			 *  - Par taille : RotateFile() si GetFileSize() >= maxSize
			 *  - Par temps : RotateFile() si dernier rotation > interval
			 *  - Par compteur : Rename vers backup.1, backup.2, etc. avec limite
			 *
			 * @example
			 * @code
			 * class SizeRotatingSink : public nkentseu::NkFileSink {
			 * protected:
			 *     void CheckRotation() override {
			 *         constexpr nkentseu::usize MAX_SIZE = 50 * 1024 * 1024;  // 50 MB
			 *         if (GetFileSize() >= MAX_SIZE) {
			 *             RotateToBackup();
			 *         }
			 *     }
			 *
			 * private:
			 *     void RotateToBackup() {
			 *         CloseUnlocked();
			 *         // Rename app.log → app.log.1, app.log.1 → app.log.2, etc.
			 *         // Recréer app.log vide
			 *         OpenUnlocked();
			 *     }
			 * };
			 * @endcode
			 */
			virtual void CheckRotation();


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PROTÉGÉES (ÉTAT PARTAGÉ AVEC LES DÉRIVÉES)
			// -----------------------------------------------------------------
			/**
			 * @defgroup FileSinkProtectedMembers Membres Protégés
			 * @brief État interne accessible aux classes dérivées pour implémentation
			 *
			 * Thread-safety :
			 *  - Ces membres sont protégés par m_Mutex pour accès concurrent
			 *  - Les dérivées doivent acquérir m_Mutex avant modification
			 *  - Lecture sans lock possible si immutable après construction
			 */

			/// @brief Mutex pour synchronisation thread-safe des opérations
			/// @ingroup FileSinkProtectedMembers
			/// @note mutable : permet modification dans les méthodes const (IsOpen, etc.)
			mutable threading::NkMutex m_Mutex;


			// -----------------------------------------------------------------
			// SECTION 5 : MEMBRES PRIVÉS (IMPLÉMENTATION INTERNE)
			// -----------------------------------------------------------------
		private:


			// -----------------------------------------------------------------
			// MÉTHODES PRIVÉES D'IMPLÉMENTATION
			// -----------------------------------------------------------------
			/**
			 * @defgroup FileSinkPrivate Méthodes Privées
			 * @brief Fonctions internes non exposées dans l'API publique
			 */

			/**
			 * @brief Ouvre le fichier avec le mode approprié basé sur m_Truncate
			 * @return true si ouvert avec succès, false en cas d'échec
			 * @ingroup FileSinkPrivate
			 *
			 * @note Mode "wb" si m_Truncate == true, "ab" sinon
			 * @note Crée le répertoire parent si inexistant via NkEnsureParentDirectory()
			 * @note Désactive le buffering via setvbuf(_IONBF) pour persistance immédiate
			 * @note Tentative de recréation du répertoire en cas d'échec initial (robustesse)
			 *
			 * @par Gestion des erreurs :
			 *  - fopen() échoue : retour false, m_FileStream reste nullptr
			 *  - setvbuf() échoue : ignoré silencieusement (non-critique)
			 *  - Pour debugging : vérifier le code d'erreur errno après échec
			 */
			bool OpenFile();


			// -----------------------------------------------------------------
			// VARIABLES MEMBRES PRIVÉES (ÉTAT INTERNE)
			// -----------------------------------------------------------------
			/**
			 * @defgroup FileSinkMembers Membres de Données
			 * @brief État interne du sink fichier (non exposé publiquement)
			 */

			/// @brief Formatter pour formatage des messages avant écriture
			/// @ingroup FileSinkMembers
			/// @note Ownership exclusif via NkUniquePtr : libération automatique
			memory::NkUniquePtr<NkFormatter> m_Formatter;

			/// @brief Handle FILE* pour écriture C stdio vers le fichier
			/// @ingroup FileSinkMembers
			/// @note nullptr si fichier fermé ou échec d'ouverture
			FILE* m_FileStream;

			/// @brief Chemin du fichier cible (relatif ou absolu)
			/// @ingroup FileSinkMembers
			/// @note Modifiable via SetFilename() avec réouverture automatique
			NkString m_Filename;

			/// @brief Mode d'ouverture : true = truncate ("wb"), false = append ("ab")
			/// @ingroup FileSinkMembers
			/// @note Modifiable via SetTruncate() avec réouverture si nécessaire
			bool m_Truncate;


		}; // class NkFileSink


	} // namespace nkentseu


#endif // NKENTSEU_NKFILESINK_H


// =============================================================================
// EXEMPLES D'UTILISATION DE NKFILESINK.H
// =============================================================================
// Ce fichier fournit l'implémentation de sink pour persistance fichier.
// Il combine thread-safety, gestion d'erreurs robuste et extensibilité.
//
// -----------------------------------------------------------------------------
// Exemple 1 : Configuration basique d'un sink fichier
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkFileSink.h>

	void SetupFileLogging() {
		// Création avec append mode (défaut)
		auto fileSink = nkentseu::memory::MakeShared<nkentseu::NkFileSink>("logs/app.log");

		// Configuration optionnelle
		fileSink->SetLevel(nkentseu::NkLogLevel::NK_INFO);  // Filtrage par niveau
		fileSink->SetPattern("[%Y-%m-%d %H:%M:%S] [%L] %v");  // Pattern lisible

		// Ajout au logger global
		nkentseu::NkLog::Instance().AddSink(fileSink);

		// Vérification d'état
		if (fileSink->IsOpen()) {
			logger.Info("File logging enabled: {0}", fileSink->GetFilename());
		}
	}
*/


// -----------------------------------------------------------------------------
// Exemple 2 : Rotation de fichier par taille via héritage
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkFileSink.h>

	class SizeRotatingFileSink : public nkentseu::NkFileSink {
	public:
		SizeRotatingFileSink(const nkentseu::NkString& path, nkentseu::usize maxSizeBytes)
			: nkentseu::NkFileSink(path)
			, m_MaxSize(maxSizeBytes) {
		}

	protected:
		void CheckRotation() override {
			// Vérification thread-safe via méthode protégée
			if (GetFileSize() >= m_MaxSize) {
				RotateFile();
			}
		}

	private:
		nkentseu::usize m_MaxSize;

		void RotateFile() {
			// Fermeture avant manipulation du fichier
			CloseUnlocked();

			// Rotation : app.log → app.log.1, app.log.1 → app.log.2, etc.
			RotateBackups();

			// Réouverture du nouveau fichier vide
			OpenUnlocked();
		}

		void RotateBackups() {
			// Décaler les backups existants
			for (int i = 9; i >= 1; --i) {
				nkentseu::NkString oldPath = nkentseu::NkString::Format("{0}.{1}", m_Filename.CStr(), i);
				nkentseu::NkString newPath = nkentseu::NkString::Format("{0}.{1}", m_Filename.CStr(), i + 1);

				if (nkentseu::NkFileExists(oldPath)) {
					if (i == 9) {
						nkentseu::NkFileDelete(oldPath);  // Supprimer le plus ancien
					} else {
						nkentseu::NkFileRename(oldPath, newPath);
					}
				}
			}

			// Renommer le courant en .1
		 if (nkentseu::NkFileExists(m_Filename)) {
				nkentseu::NkString backup = nkentseu::NkString::Format("{0}.1", m_Filename.CStr());
				nkentseu::NkFileRename(m_Filename, backup);
			}
		}
	};

	// Usage :
	// auto rotatingSink = nkentseu::memory::MakeShared<SizeRotatingFileSink>(
	//     "logs/app.log",
	//     50 * 1024 * 1024  // 50 MB max
	// );
	// logger.AddSink(rotatingSink);
*/


// -----------------------------------------------------------------------------
// Exemple 3 : Rotation quotidienne par date dans le nom de fichier
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkFileSink.h>
	#include <NKCore/NkDateTime.h>  // Hypothétique

	class DailyRotatingFileSink : public nkentseu::NkFileSink {
	public:
		DailyRotatingFileSink(const nkentseu::NkString& basePath)
			: nkentseu::NkFileSink(MakeDailyFilename(basePath))
			, m_BasePath(basePath) {
		}

	protected:
		void CheckRotation() override {
			nkentseu::NkString expectedFile = MakeDailyFilename(m_BasePath);
			if (GetFilenameUnlocked() != expectedFile) {
				// Nouveau jour : changer de fichier
				CloseUnlocked();
				SetFilenameUnlocked(expectedFile);
				OpenUnlocked();
			}
		}

	private:
		nkentseu::NkString m_BasePath;

		static nkentseu::NkString MakeDailyFilename(const nkentseu::NkString& basePath) {
			nkentseu::NkString date = nkentseu::NkDateTime::Now().ToString("%Y-%m-%d");
			return nkentseu::NkString::Format("{0}-{1}.log", basePath.CStr(), date.CStr());
		}

		void SetFilenameUnlocked(const nkentseu::NkString& filename) {
			// Helper pour modifier m_Filename sans lock (prérequis : lock détenu)
			const_cast<NkString&>(m_Filename) = filename;
		}
	};
*/


// -----------------------------------------------------------------------------
// Exemple 4 : Gestion manuelle d'ouverture/fermeture pour maintenance
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkFileSink.h>

	void MaintenanceWindow() {
		auto& logger = nkentseu::NkLog::Instance();
		auto* fileSink = dynamic_cast<nkentseu::NkFileSink*>(GetFileSink(logger));

		if (fileSink && fileSink->IsOpen()) {
			// Pause temporaire du logging fichier pour maintenance
			fileSink->Close();

			// Opérations de maintenance sur le fichier de log
			CompressLogFile(fileSink->GetFilename());
			ArchiveOldLogs();

			// Reprise du logging
			if (fileSink->Open()) {
				logger.Info("Logging resumed after maintenance");
			} else {
				logger.Error("Failed to reopen log file after maintenance");
			}
		}
	}
*/


// -----------------------------------------------------------------------------
// Exemple 5 : Configuration dynamique via fichier de config
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkFileSink.h>
	#include <NKCore/NkConfig.h>

	nkentseu::NkSinkPtr CreateFileSinkFromConfig(const nkentseu::NkString& section) {
		using namespace nkentseu;

		// Lecture des paramètres depuis configuration
		NkString filepath = core::Config::GetString(section + ".filepath", "logs/app.log");
		bool truncate = core::Config::GetBool(section + ".truncate", false);
		NkString pattern = core::Config::GetString(section + ".pattern", NkFormatter::NK_DEFAULT_PATTERN);
		NkLogLevel level = NkLogLevelFromString(core::Config::GetString(section + ".level", "info"));

		// Création et configuration du sink
		auto sink = memory::MakeShared<NkFileSink>(filepath, truncate);
		sink->SetPattern(pattern);
		sink->SetLevel(level);

		return sink;
	}

	// Usage au startup :
	// auto fileSink = CreateFileSinkFromConfig("logging.sinks.file");
	// logger.AddSink(fileSink);
*/


// -----------------------------------------------------------------------------
// Exemple 6 : Testing avec vérification d'écriture
// -----------------------------------------------------------------------------
/*
	#include <NKLogger/Sinks/NkFileSink.h>
	#include <NKLogger/NkLogMessage.h>
	#include <gtest/gtest.h>

	TEST(NkFileSinkTest, BasicWrite) {
		const nkentseu::NkString testPath = "test_logs/basic_write.log";

		{
			nkentseu::NkFileSink sink(testPath, true);  // truncate pour test propre
			ASSERT_TRUE(sink.IsOpen()) << "Failed to open test log file";

			// Message de test
			nkentseu::NkLogMessage msg(
				nkentseu::NkLogLevel::NK_INFO,
				"Test message",
				"test.cpp", 42, "TestFunction"
			);
			sink.Log(msg);
			sink.Flush();  // Force écriture disque
		}

		// Vérification : le fichier existe et contient le message
		EXPECT_TRUE(NkFileExists(testPath));
		nkentseu::NkString content = NkReadFileContents(testPath);
		EXPECT_TRUE(content.Contains("Test message"));

		// Cleanup
		NkFileDelete(testPath);
	}
*/


// =============================================================================
// NOTES DE MAINTENANCE ET BONNES PRATIQUES
// =============================================================================
/*
	1. GESTION DES ERREURS D'ÉCRITURE :
	   - fwrite/fputc échecs sont ignorés silencieusement pour robustesse
	   - Pour debugging : vérifier errno après échec d'ouverture/écriture
	   - En production : monitorer IsOpen() et GetFileSize() pour détection proactive

	2. BUFFERING ET PERFORMANCE :
	   - Buffering désactivé par défaut (_IONBF) pour persistance immédiate
	   - Pour performance : envisager _IOFBF avec Flush() périodique via timer
	   - Trade-off : persistance vs throughput → choisir selon cas d'usage

	3. ROTATION DE FICHIER :
	   - CheckRotation() appelé après chaque Log() : garder l'implémentation légère
	   - Pour rotation complexe : déléguer à un thread dédié ou timer externe
	   - Toujours Close() avant manipulation du fichier, puis Open() pour reprise

	4. CHEMINS DE FICHIERS MULTIPLATEFORME :
	   - Utiliser NKCore/NkPath pour normalisation des chemins (/ vs \)
	   - NkEnsureParentDirectory() gère déjà les deux séparateurs
	   - Pour chemins relatifs : basés sur le working directory du processus

	5. THREAD-SAFETY :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLock
	   - Les méthodes Unlocked() sont pour usage interne uniquement avec lock déjà détenu
	   - Les sinks partagés entre loggers : safe car mutex interne à chaque sink

	6. EXTENSIBILITÉ FUTURES :
	   - Pour compression : override Log() pour écriture dans buffer compressé
	   - Pour chiffrement : wrapper du FILE* avec couche de chiffrement
	   - Pour réseau : dériver vers NkNetworkSink avec envoi TCP/UDP au lieu de fichier

	7. TESTING :
	   - Utiliser des chemins temporaires uniques pour éviter les collisions
	   - Toujours cleanup les fichiers de test dans les destructeurs de test
	   - Tester les cas limites : chemin inexistant, permissions refusées, disque plein
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================