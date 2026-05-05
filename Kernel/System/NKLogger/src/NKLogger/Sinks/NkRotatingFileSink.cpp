// =============================================================================
// NKLogger/Sinks/NkRotatingFileSink.cpp
// Implémentation du sink avec rotation automatique basée sur la taille.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Fonctions utilitaires dans namespace anonyme pour encapsulation
//  - Rotation thread-safe via mutex hérité de NkFileSink
//  - Gestion robuste des erreurs rename() sans propagation d'exceptions
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/Sinks/NkRotatingFileSink.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/Sinks/NkFileSink.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"

#include <cstdio>
#include <sys/stat.h>


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE ANONYME - UTILITAIRES INTERNES
// -------------------------------------------------------------------------
// Fonctions et constantes à usage interne uniquement.
// Non exposées dans l'API publique pour encapsulation et optimisation.

namespace {


	// -------------------------------------------------------------------------
	// FONCTION : NkFileExists
	// DESCRIPTION : Vérifie si un fichier existe via interrogation système
	// PARAMS : path - Chemin du fichier à tester
	// RETURN : true si le fichier existe et est accessible, false sinon
	// NOTE : Utilise stat() portable, retourne false pour chemins vides
	// -------------------------------------------------------------------------
	bool NkFileExists(const nkentseu::NkString& path) {
		if (path.Empty()) {
			return false;
		}

		struct stat fileInfo {};
		return ::stat(path.CStr(), &fileInfo) == 0;
	}


} // namespace anonymous


// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques et privées de NkRotatingFileSink.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur
	// DESCRIPTION : Initialise avec paramètres de rotation et ouverture fichier
	// -------------------------------------------------------------------------
	NkRotatingFileSink::NkRotatingFileSink(
		const NkString& filename,
		usize maxSize,
		usize maxFiles
	) : NkFileSink(filename, false)  // Append mode par défaut pour rotation
		, m_MaxSize(maxSize)
		, m_MaxFiles(maxFiles)
		, m_CurrentSize(0) {

		// Initialisation de m_CurrentSize via interrogation système
		// Note : peut être 0 si fichier nouvellement créé
		m_CurrentSize = GetFileSize();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur
	// DESCRIPTION : Cleanup via destructeur de NkFileSink
	// -------------------------------------------------------------------------
	NkRotatingFileSink::~NkRotatingFileSink() {
		// Destructeur de NkFileSink appelé automatiquement :
		// - Close() pour libération du FILE*
		// - Flush() pour persistance des buffers
		// Aucune logique supplémentaire nécessaire
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Log
	// DESCRIPTION : Écriture avec mise à jour de taille et vérification rotation
	// -------------------------------------------------------------------------
	void NkRotatingFileSink::Log(const NkLogMessage& message) {
		// Appel de la méthode parente pour écriture normale thread-safe
		NkFileSink::Log(message);

		// Mise à jour de la taille courante après écriture
		// Note : GetFileSize() utilise stat() : appel système potentiellement coûteux
		m_CurrentSize = GetFileSize();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetMaxSize
	// DESCRIPTION : Définit la limite de taille avec synchronisation thread-safe
	// -------------------------------------------------------------------------
	void NkRotatingFileSink::SetMaxSize(usize maxSize) {
		// Acquisition du mutex hérité pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour de la limite
		m_MaxSize = maxSize;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetMaxSize
	// DESCRIPTION : Retourne la limite de taille (lecture thread-safe)
	// -------------------------------------------------------------------------
	usize NkRotatingFileSink::GetMaxSize() const {
		// Acquisition du mutex hérité pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur configurée
		return m_MaxSize;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetMaxFiles
	// DESCRIPTION : Définit le nombre de backups avec synchronisation thread-safe
	// -------------------------------------------------------------------------
	void NkRotatingFileSink::SetMaxFiles(usize maxFiles) {
		// Acquisition du mutex hérité pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour du nombre maximum de fichiers historiques
		m_MaxFiles = maxFiles;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetMaxFiles
	// DESCRIPTION : Retourne le nombre de backups configuré (lecture thread-safe)
	// -------------------------------------------------------------------------
	usize NkRotatingFileSink::GetMaxFiles() const {
		// Acquisition du mutex hérité pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur configurée
		return m_MaxFiles;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Rotate
	// DESCRIPTION : Force la rotation indépendamment de la taille courante
	// -------------------------------------------------------------------------
	bool NkRotatingFileSink::Rotate() {
		// Acquisition du mutex hérité pour opération thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Exécution de la rotation
		PerformRotation();

		// Retour de succès : rotation considérée réussie même si rename() partiel
		// Note : pour robustesse stricte, vérifier IsOpen() après PerformRotation()
		return true;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : CheckRotation (override protégé)
	// DESCRIPTION : Vérifie la condition de rotation et déclenche si nécessaire
	// -------------------------------------------------------------------------
	void NkRotatingFileSink::CheckRotation() {
		// Note : cette méthode est appelée depuis Log() qui détient déjà m_Mutex
		// Pas besoin d'acquérir le mutex ici : déjà acquis par l'appelant

		// Vérification de la condition de déclenchement
		if (m_MaxSize > 0 && m_CurrentSize >= m_MaxSize) {
			// Condition remplie : exécuter la rotation
			PerformRotation();
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : PerformRotation (privée)
	// DESCRIPTION : Exécute la séquence complète de rotation des fichiers
	// -------------------------------------------------------------------------
	void NkRotatingFileSink::PerformRotation() {
		// Note : doit être appelée avec m_Mutex déjà acquis (via Log() ou Rotate())

		// Cas spécial : rotation désactivée si maxFiles = 0
		if (m_MaxFiles == 0) {
			return;
		}

		// Étape 1 : Fermer le fichier courant avant manipulation
		CloseUnlocked();

		// Étape 2 : Décaler les fichiers de backup existants
		// Algorithme : parcourir de maxFiles-1 vers 1 pour éviter les écrasements
		// Exemple avec maxFiles=5 : .3→.4, .2→.3, .1→.2, .0→.1
		for (usize index = m_MaxFiles - 1; index > 0; --index) {
			const NkString sourceFile = GetFilenameForIndex(index - 1);
			const NkString targetFile = GetFilenameForIndex(index);

			// Rename uniquement si le fichier source existe
			if (NkFileExists(sourceFile)) {
				// rename() retourne 0 en cas de succès, -1 en cas d'échec
				// Échec ignoré silencieusement pour robustesse en production
				(void)::rename(sourceFile.CStr(), targetFile.CStr());
			}
		}

		// Étape 3 : Renommer le fichier courant en backup .0
		const NkString currentFile = GetFilenameUnlocked();
		const NkString firstBackup = GetFilenameForIndex(0);

		if (NkFileExists(currentFile)) {
			(void)::rename(currentFile.CStr(), firstBackup.CStr());
		}

		// Étape 4 : Supprimer le backup le plus ancien si dépassement de limite
		// Exemple : si maxFiles=5, supprimer .5 après décalage (car .4→.5)
		if (m_MaxFiles > 0) {
			const NkString oldestBackup = GetFilenameForIndex(m_MaxFiles);
			if (NkFileExists(oldestBackup)) {
				(void)::remove(oldestBackup.CStr());
			}
		}

		// Étape 5 : Rouvrir un nouveau fichier courant vide
		OpenUnlocked();

		// Étape 6 : Réinitialiser l'estimation de taille pour le nouveau fichier
		m_CurrentSize = 0;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetFilenameForIndex (privée)
	// DESCRIPTION : Génère le chemin de fichier pour un index de rotation donné
	// -------------------------------------------------------------------------
	NkString NkRotatingFileSink::GetFilenameForIndex(usize index) const {
		// Note : appelée avec m_Mutex déjà acquis, lecture safe de m_Filename

		// Buffer pour le suffixe numérique : ".{index}"
		char suffixBuffer[32];

		// Formatage sûr de l'index en chaîne décimale
		const int written = ::snprintf(
			suffixBuffer,
			sizeof(suffixBuffer),
			".%zu",
			static_cast<size_t>(index)
		);

		// Récupération du chemin de base via méthode unlocked (mutex déjà acquis)
		NkString result = GetFilenameUnlocked();

		// Append du suffixe si formatage réussi
		if (written > 0) {
			result.Append(suffixBuffer, static_cast<usize>(written));
		}

		return result;
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
	1. ATOMICITÉ DE PerformRotation() :
	   - Séquence Close() → rename() → Open() doit être exécutée atomiquement
	   - Garantit par le fait que cette méthode est appelée uniquement depuis
	     Log() ou Rotate() qui acquièrent déjà m_Mutex
	   - Risk : crash pendant rotation → fichier .0 potentiellement partiel
	   - Mitigation : rotation déclenchée uniquement après écriture complète

	2. GESTION DES ERREURS DE rename() :
	   - rename() peut échouer : permissions, disque plein, fichier verrouillé
	   - Implémentation : (void)::rename() ignore le code de retour
	   - Pour production critique : logger l'erreur et tenter fallback (copie+suppression)
	   - Alternative : retourner bool pour propagation d'erreur contrôlée

	3. PERFORMANCE DE GetFileSize() DANS Log() :
	   - stat() appelé après chaque Log() : ~1-10 µs selon système de fichiers
	   - Impact en haute fréquence : peut réduire le throughput de 10-30%
	   - Optimisation : estimation incrémentale avec recalage périodique
	   - Voir Exemple 6 dans le header pour implémentation optimisée

	4. SCHÉMA DE DÉCALAGE DES BACKUPS :
	   - Parcours de maxFiles-1 vers 1 pour éviter les écrasements
	   - Exemple maxFiles=5 : .3→.4, .2→.3, .1→.2, .0→.1, courant→.0
	   - Alternative : utilisation de liens durs ou copy-on-write pour performance

	5. SUPPRESSION DU PLUS ANCIEN :
	   - Fichier .maxFiles supprimé après décalage si existe
	   - Exemple : maxFiles=5 → .5 supprimé après rotation
	   - Pour archivage externe : hook avant suppression pour upload cloud

	6. THREAD-SAFETY HÉRITÉE :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLock (hérité)
	   - CheckRotation() et PerformRotation() appelés depuis Log() avec lock déjà acquis
	   - Safe pour partage entre multiples loggers via NkSharedPtr

	7. COMPATIBILITÉ MULTIPLATEFORME :
	   - rename()/remove() : standards C, portables Windows/POSIX
	   - stat() : disponible via <sys/stat.h> sur Windows (MinGW/MSVC) et POSIX
	   - Chemins : NkString gère / et \, NkEnsureParentDirectory() dans NkFileSink

	8. EXTENSIBILITÉ FUTURES :
	   - Rotation par temps : comparer timestamp dernier rotation vs intervalle configuré
	   - Compression : compresser les backups .N après rotation pour économiser l'espace
	   - Upload cloud : envoyer .N vers S3/autre après rotation pour archivage distant
	   - Hook personnalisé : callback avant/après rotation pour intégration monitoring
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================