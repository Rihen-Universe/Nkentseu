// =============================================================================
// NKLogger/Sinks/NkFileSink.cpp
// Implémentation du sink pour écriture des logs dans un fichier.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Fonctions utilitaires dans namespace anonyme pour encapsulation
//  - Gestion robuste des erreurs d'E/S sans propagation d'exceptions
//  - Synchronisation thread-safe via NKThreading/NkMutex
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/Sinks/NkFileSink.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkLoggerFormatter.h"
#include "NKContainers/String/NkString.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"

#include <cerrno>
#include <cstdio>
#include <sys/stat.h>

#if defined(_WIN32)
	#include <direct.h>
#endif


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE ANONYME - UTILITAIRES INTERNES
// -------------------------------------------------------------------------
// Fonctions et constantes à usage interne uniquement.
// Non exposées dans l'API publique pour encapsulation et optimisation.

namespace {


	// -------------------------------------------------------------------------
	// FONCTION : NkPathExists
	// DESCRIPTION : Vérifie si un chemin de fichier/répertoire existe
	// PARAMS : path - Chaîne contenant le chemin à tester
	// RETURN : true si le chemin existe et est accessible, false sinon
	// NOTE : Utilise stat() portable, gère les chemins vides
	// -------------------------------------------------------------------------
	bool NkPathExists(const nkentseu::NkString& path) {
		if (path.Empty()) {
			return false;
		}

		struct stat fileInfo {};
		return ::stat(path.CStr(), &fileInfo) == 0;
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkGetPathFileSize
	// DESCRIPTION : Obtient la taille d'un fichier via interrogation système
	// PARAMS : path - Chemin du fichier à interroger
	// RETURN : Taille en octets, ou 0 si fichier inexistant/inaccessible
	// NOTE : Utilise stat().st_size, retourne 0 en cas d'erreur
	// -------------------------------------------------------------------------
	nkentseu::usize NkGetPathFileSize(const nkentseu::NkString& path) {
		struct stat fileInfo {};

		if (::stat(path.CStr(), &fileInfo) != 0) {
			return 0;
		}

		return static_cast<nkentseu::usize>(fileInfo.st_size);
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkCreateDirectory
	// DESCRIPTION : Crée un répertoire unique (pas récursif)
	// PARAMS : path - Chemin du répertoire à créer
	// RETURN : true si créé ou déjà existant, false en cas d'échec
	// NOTE : Windows : _mkdir(), POSIX : mkdir() avec permissions 0755
	// -------------------------------------------------------------------------
	bool NkCreateDirectory(const char* path) {
		#if defined(_WIN32)
			// Windows : _mkdir() retourne 0 en cas de succès
			const int result = ::_mkdir(path);
		#else
			// POSIX : mkdir() avec permissions rwxr-xr-x (0755)
			const int result = ::mkdir(path, 0755);
		#endif

		// Succès si créé (0) ou déjà existant (EEXIST)
		return (result == 0 || errno == EEXIST);
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkCreateDirectories
	// DESCRIPTION : Crée récursivement tous les répertoires d'un chemin
	// PARAMS : directory - Chemin complet des répertoires à créer
	// RETURN : true si tous créés ou déjà existants, false en cas d'échec
	// NOTE : Parcours caractère par caractère, crée à chaque séparateur / ou \
	// -------------------------------------------------------------------------
	bool NkCreateDirectories(const nkentseu::NkString& directory) {
		// Cas vide : rien à faire, considéré comme succès
		if (directory.Empty()) {
			return true;
		}

		// Déjà existant : retour immédiat
		if (NkPathExists(directory)) {
			return true;
		}

		// Construction progressive du chemin avec création à chaque étape
		nkentseu::NkString currentPath;
		currentPath.Reserve(directory.Length());

		for (nkentseu::usize i = 0; i < directory.Length(); ++i) {
			const char currentChar = directory[i];
			currentPath.PushBack(currentChar);

			// Détection d'un séparateur de chemin
			const bool isSeparator = (currentChar == '/' || currentChar == '\\');

			if (!isSeparator) {
				continue;
			}

			// Ignorer les séparateurs en début de chemin (racine Unix ou drive Windows)
			if (currentPath.Length() <= 1) {
				continue;
			}

			// Tentative de création du répertoire courant
			(void)NkCreateDirectory(currentPath.CStr());
		}

		// Création finale du répertoire cible complet
		return NkCreateDirectory(directory.CStr());
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkEnsureParentDirectory
	// DESCRIPTION : Crée le répertoire parent d'un chemin de fichier si nécessaire
	// PARAMS : filename - Chemin complet du fichier (pour extraire le parent)
	// NOTE : Extrait le chemin jusqu'au dernier / ou \, puis crée récursivement
	// -------------------------------------------------------------------------
	void NkEnsureParentDirectory(const nkentseu::NkString& filename) {
		// Recherche du dernier séparateur de chemin
		const nkentseu::usize lastSeparatorPos = filename.FindLastOf("/\\");

		// Pas de séparateur : fichier dans le répertoire courant, rien à créer
		if (lastSeparatorPos == nkentseu::NkString::npos) {
			return;
		}

		// Extraction du chemin parent (exclu le nom de fichier)
		nkentseu::NkString parentDirectory = filename.SubStr(0, lastSeparatorPos);

		// Création récursive si nécessaire
		(void)NkCreateDirectories(parentDirectory);
	}


} // namespace anonymous


// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques et protégées de NkFileSink.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur
	// DESCRIPTION : Initialise avec chemin, mode, et ouverture automatique
	// -------------------------------------------------------------------------
	NkFileSink::NkFileSink(const NkString& filename, bool truncate)
		: m_Formatter(memory::NkMakeUnique<NkLoggerFormatter>(NkLoggerFormatter::NK_DEFAULT_PATTERN))
		, m_FileStream(nullptr)
		, m_Filename(filename)
		, m_Truncate(truncate) {

		// Création du répertoire parent si inexistant
		NkEnsureParentDirectory(filename);

		// Tentative d'ouverture immédiate du fichier
		Open();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur
	// DESCRIPTION : Fermeture garantie du fichier avant destruction
	// -------------------------------------------------------------------------
	NkFileSink::~NkFileSink() {
		// Fermeture explicite : libération du FILE* et flush des buffers
		Close();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Log
	// DESCRIPTION : Écriture thread-safe d'un message formaté dans le fichier
	// -------------------------------------------------------------------------
	void NkFileSink::Log(const NkLogMessage& message) {
		// Filtrage précoce : éviter tout traitement si message ignoré
		if (!IsEnabled() || !ShouldLog(message.level)) {
			return;
		}

		// Acquisition du mutex pour écriture exclusive thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Vérification de l'état du fichier : réouverture si nécessaire
		if (m_FileStream == nullptr) {
			if (!OpenFile()) {
				// Échec d'ouverture : retour silencieux pour robustesse
				return;
			}
		}

		// Formatage du message via le formatter configuré
		// Note : couleurs désactivées pour fichier (paramètre false)
		NkString formattedMessage;
		if (m_Formatter) {
			formattedMessage = m_Formatter->Format(message, false);
		} else {
			// Fallback minimal si aucun formatter configuré
			formattedMessage = message.message;
		}

		// Écriture du message formaté dans le fichier
		if (!formattedMessage.Empty()) {
			// fwrite pour le contenu principal
			(void)::fwrite(
				formattedMessage.CStr(),
				sizeof(char),
				static_cast<size_t>(formattedMessage.Length()),
				m_FileStream
			);
		}

		// Newline automatique après chaque message pour lisibilité
		(void)::fputc('\n', m_FileStream);

		// Vérification de rotation après écriture (extension point)
		CheckRotation();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Flush
	// DESCRIPTION : Force l'écriture des buffers C stdio vers le disque
	// -------------------------------------------------------------------------
	void NkFileSink::Flush() {
		// Acquisition du mutex pour opération thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// fflush uniquement si fichier ouvert
		if (m_FileStream != nullptr) {
			(void)::fflush(m_FileStream);
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetFormatter
	// DESCRIPTION : Définit le formatter avec transfert de propriété thread-safe
	// -------------------------------------------------------------------------
	void NkFileSink::SetFormatter(memory::NkUniquePtr<NkLoggerFormatter> formatter) {
		// Acquisition du mutex pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Transfert de propriété via move : ancien formatter détruit automatiquement
		m_Formatter = traits::NkMove(formatter);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetPattern
	// DESCRIPTION : Met à jour le pattern via le formatter interne
	// -------------------------------------------------------------------------
	void NkFileSink::SetPattern(const NkString& pattern) {
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
	NkLoggerFormatter* NkFileSink::GetFormatter() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour du pointeur brut via get() de NkUniquePtr
		return m_Formatter.Get();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetPattern
	// DESCRIPTION : Retourne le pattern courant via le formatter (lecture thread-safe)
	// -------------------------------------------------------------------------
	NkString NkFileSink::GetPattern() const {
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
	// MÉTHODE : Open
	// DESCRIPTION : Ouvre le fichier avec acquisition de mutex
	// -------------------------------------------------------------------------
	bool NkFileSink::Open() {
		// Acquisition du mutex pour opération thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Délégation à la version unlocked car mutex déjà acquis
		return OpenUnlocked();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Close
	// DESCRIPTION : Ferme le fichier avec acquisition de mutex
	// -------------------------------------------------------------------------
	void NkFileSink::Close() {
		// Acquisition du mutex pour opération thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Délégation à la version unlocked car mutex déjà acquis
		CloseUnlocked();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : IsOpen
	// DESCRIPTION : Vérifie l'état d'ouverture du fichier (lecture thread-safe)
	// -------------------------------------------------------------------------
	bool NkFileSink::IsOpen() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de l'état du FILE*
		return m_FileStream != nullptr;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetFilename
	// DESCRIPTION : Retourne le chemin du fichier (lecture thread-safe)
	// -------------------------------------------------------------------------
	NkString NkFileSink::GetFilename() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Délégation à la version unlocked car mutex déjà acquis
		return GetFilenameUnlocked();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetFilename
	// DESCRIPTION : Change le fichier cible avec réouverture automatique
	// -------------------------------------------------------------------------
	void NkFileSink::SetFilename(const NkString& filename) {
		// Acquisition du mutex pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour uniquement si changement pour éviter réouverture inutile
		if (m_Filename != filename) {
			// Fermeture de l'ancien fichier
			CloseUnlocked();

			// Mise à jour du chemin cible
			m_Filename = filename;

			// Création du répertoire parent si nécessaire pour le nouveau chemin
			NkEnsureParentDirectory(filename);

			// Ouverture du nouveau fichier
			OpenUnlocked();
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetFileSize
	// DESCRIPTION : Retourne la taille du fichier sur disque (lecture thread-safe)
	// -------------------------------------------------------------------------
	usize NkFileSink::GetFileSize() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Délégation à l'utilitaire stat()-based
		return NkGetPathFileSize(m_Filename);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetTruncate
	// DESCRIPTION : Change le mode d'ouverture avec réouverture si nécessaire
	// -------------------------------------------------------------------------
	void NkFileSink::SetTruncate(bool truncate) {
		// Acquisition du mutex pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour uniquement si changement pour éviter réouverture inutile
		if (m_Truncate != truncate) {
			// Mise à jour du mode
			m_Truncate = truncate;

			// Réouverture avec le nouveau mode si fichier actuellement ouvert
			if (m_FileStream != nullptr) {
				CloseUnlocked();
				OpenUnlocked();
			}
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetTruncate
	// DESCRIPTION : Retourne le mode d'ouverture courant (lecture thread-safe)
	// -------------------------------------------------------------------------
	bool NkFileSink::GetTruncate() const {
		// Acquisition du mutex pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur du flag
		return m_Truncate;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : OpenFile (privée)
	// DESCRIPTION : Logique réelle d'ouverture du fichier avec gestion d'erreurs
	// -------------------------------------------------------------------------
	bool NkFileSink::OpenFile() {
		// Validation : chemin non vide requis
		if (m_Filename.Empty()) {
			return false;
		}

		// Création du répertoire parent si inexistant (robustesse)
		NkEnsureParentDirectory(m_Filename);

		// Détermination du mode d'ouverture basé sur m_Truncate
		const char* openMode = m_Truncate ? "wb" : "ab";

		// Première tentative d'ouverture
		m_FileStream = ::fopen(m_Filename.CStr(), openMode);

		// Seconde tentative après recréation du répertoire (cas edge : race condition)
		if (m_FileStream == nullptr) {
			NkEnsureParentDirectory(m_Filename);
			m_FileStream = ::fopen(m_Filename.CStr(), openMode);
		}

		// Échec définitif si toujours nullptr
		if (m_FileStream == nullptr) {
			return false;
		}

		// Désactivation du buffering C stdio pour persistance immédiate
		// _IONBF : no buffering, chaque fwrite va directement au système
		(void)::setvbuf(m_FileStream, nullptr, _IONBF, 0);

		// Succès : fichier ouvert et configuré
		return true;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : OpenUnlocked (protégée)
	// DESCRIPTION : Wrapper vers OpenFile() pour usage avec lock déjà acquis
	// -------------------------------------------------------------------------
	bool NkFileSink::OpenUnlocked() {
		// Délégation directe à la méthode privée d'ouverture
		return OpenFile();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : CloseUnlocked (protégée)
	// DESCRIPTION : Fermeture du FILE* sans acquisition de mutex
	// -------------------------------------------------------------------------
	void NkFileSink::CloseUnlocked() {
		// Fermeture uniquement si fichier ouvert
		if (m_FileStream != nullptr) {
			// Flush implicite dans fclose(), mais explicite pour clarté
			(void)::fflush(m_FileStream);

			// Fermeture et libération du FILE*
			(void)::fclose(m_FileStream);
			m_FileStream = nullptr;
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetFilenameUnlocked (protégée)
	// DESCRIPTION : Retour du chemin sans acquisition de mutex
	// -------------------------------------------------------------------------
	NkString NkFileSink::GetFilenameUnlocked() const {
		// Retour par copie de la chaîne interne
		return m_Filename;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : CheckRotation (protégée virtuelle)
	// DESCRIPTION : Point d'extension pour rotation de fichier
	// -------------------------------------------------------------------------
	void NkFileSink::CheckRotation() {
		// Implémentation par défaut : no-op (pas de rotation)
		// Les classes dérivées peuvent override pour implémenter une stratégie
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
	1. GESTION DES ERREURS D'OUVERTURE :
	   - Deux tentatives fopen() avec NkEnsureParentDirectory() intermédiaire
	   - Gère les race conditions où le répertoire est créé entre les tentatives
	   - Retour false silencieux : Log() devient no-op jusqu'à réouverture manuelle

	2. BUFFERING ET PERSISTANCE :
	   - setvbuf(_IONBF) désactive le buffering C stdio pour écriture immédiate
	   - Avantage : persistance garantie même en cas de crash immédiat
	   - Inconvénient : overhead système pour chaque fwrite → envisager buffering pour haute fréquence

	3. THREAD-SAFETY :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLock
	   - Les méthodes Unlocked() sont pour usage interne avec lock déjà détenu
	   - Safe pour partage entre multiples loggers via NkSharedPtr

	4. CRÉATION DE RÉPERTOIRES :
	   - NkCreateDirectories() crée récursivement à chaque séparateur / ou \
	   - Gère les chemins absolus et relatifs, Windows et POSIX
	   - Ignore les erreurs EEXIST pour idempotence

	5. EXTENSIBILITÉ VIA CHECKROTATION :
	   - Appelée après chaque Log() : garder l'implémentation légère
	   - Pour rotation complexe : déléguer à un thread dédié ou timer externe
	   - Toujours Close() avant manipulation du fichier, puis Open() pour reprise

	6. COMPATIBILITÉ MULTIPLATEFORME :
	   - fopen/fwrite/fclose : standards C, portables partout
	   - mkdir/_mkdir : abstraction via NkCreateDirectory()
	   - stat : disponible sur Windows (via <sys/stat.h>) et POSIX

	7. PERFORMANCE :
	   - Filtrage précoce dans Log() évite formatage si message ignoré
	   - NkString utilise SSO : petites chaînes sans allocation heap
	   - Pour très haute fréquence : envisager buffering avec Flush() périodique
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================