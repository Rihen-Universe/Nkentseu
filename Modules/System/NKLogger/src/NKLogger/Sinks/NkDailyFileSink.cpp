// =============================================================================
// NKLogger/Sinks/NkDailyFileSink.cpp
// Implémentation du sink avec rotation automatique quotidienne.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Fonctions utilitaires dans namespace anonyme pour encapsulation
//  - Rotation thread-safe via mutex hérité de NkFileSink
//  - Gestion robuste des erreurs rename()/remove() sans propagation d'exceptions
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/Sinks/NkDailyFileSink.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/Sinks/NkFileSink.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkScopedLock.h"

#include <ctime>
#include <cstdio>
#include <sys/stat.h>
#include <cstring>

#if !defined(_WIN32)
	#include <time.h>
	#include <dirent.h>
#else
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
	#include <windows.h>
#endif


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE ANONYME - UTILITAIRES INTERNES
// -------------------------------------------------------------------------
// Fonctions et constantes à usage interne uniquement.
// Non exposées dans l'API publique pour encapsulation et optimisation.

namespace {


	// -------------------------------------------------------------------------
	// FONCTION : NkIsDigit
	// DESCRIPTION : Vérifie si un caractère est un chiffre décimal
	// PARAMS : ch - Caractère à tester
	// RETURN : true si ch est entre '0' et '9', false sinon
	// NOTE : Alternative portable à isdigit() sans dépendance <cctype>
	// -------------------------------------------------------------------------
	bool NkIsDigit(char ch) {
		return ch >= '0' && ch <= '9';
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkHasPrefix
	// DESCRIPTION : Vérifie si une chaîne commence par un préfixe donné
	// PARAMS : value - Chaîne à tester
	//          prefix - Préfixe recherché
	// RETURN : true si value commence par prefix, false sinon
	// NOTE : Comparaison caractère par caractère, portable sans dépendance externe
	// -------------------------------------------------------------------------
	bool NkHasPrefix(const nkentseu::NkString& value, const nkentseu::NkString& prefix) {
		// Préfixe plus long que la valeur : impossible
		if (value.Length() < prefix.Length()) {
			return false;
		}

		// Comparaison caractère par caractère du préfixe
		for (nkentseu::usize i = 0; i < prefix.Length(); ++i) {
			if (value[i] != prefix[i]) {
				return false;
			}
		}

		// Tous les caractères du préfixe correspondent
		return true;
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkIsPathSeparator
	// DESCRIPTION : Vérifie si un caractère est un séparateur de chemin
	// PARAMS : ch - Caractère à tester
	// RETURN : true si ch est '/' ou '\', false sinon
	// NOTE : Supporte les deux conventions Unix (/) et Windows (\)
	// -------------------------------------------------------------------------
	bool NkIsPathSeparator(char ch) {
		return ch == '/' || ch == '\\';
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkJoinPath
	// DESCRIPTION : Concatène un répertoire et un nom de fichier avec séparateur
	// PARAMS : directory - Chemin du répertoire (peut être vide ou ".")
	//          filename - Nom de fichier à ajouter
	// RETURN : Chaîne NkString contenant le chemin complet joint
	// NOTE : Ajoute automatiquement le séparateur si manquant, gère les cas edge
	// -------------------------------------------------------------------------
	nkentseu::NkString NkJoinPath(
		const nkentseu::NkString& directory,
		const nkentseu::NkString& filename
	) {
		// Cas directory vide ou courant : retourne filename seul
		if (directory.Empty() || directory == ".") {
			if (directory == ".") {
				nkentseu::NkString path = directory;
				path.PushBack('/');
				path.Append(filename);
				return path;
			}
			return filename;
		}

		// Construction du chemin avec séparateur si nécessaire
		nkentseu::NkString path = directory;
		if (!NkIsPathSeparator(path[path.Length() - 1])) {
			path.PushBack('/');
		}
		path.Append(filename);
		return path;
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkParseYmd8
	// DESCRIPTION : Parse une chaîne de 8 chiffres YYYYMMDD en structure tm
	// PARAMS : digits - Pointeur vers 8 caractères chiffres
	//          outDate - Référence pour stocker la date parsée
	// RETURN : true si parsing réussi et date valide, false sinon
	// NOTE : Valide année >= 1900, mois 1-12, jour 1-31 via mktime()
	// -------------------------------------------------------------------------
	bool NkParseYmd8(const char* digits, tm& outDate) {
		// Validation du pointeur d'entrée
		if (digits == nullptr) {
			return false;
		}

		// Vérification que les 8 caractères sont des chiffres
		for (int i = 0; i < 8; ++i) {
			if (!NkIsDigit(digits[i])) {
				return false;
			}
		}

		// Extraction des composantes de date
		const int year = (digits[0] - '0') * 1000
		               + (digits[1] - '0') * 100
		               + (digits[2] - '0') * 10
		               + (digits[3] - '0');
		const int month = (digits[4] - '0') * 10 + (digits[5] - '0');
		const int day = (digits[6] - '0') * 10 + (digits[7] - '0');

		// Validation des plages de valeurs
		if (year < 1900 || month < 1 || month > 12 || day < 1 || day > 31) {
			return false;
		}

		// Construction de la structure tm candidate
		tm candidate{};
		candidate.tm_year = year - 1900;  // tm_year est offset depuis 1900
		candidate.tm_mon = month - 1;      // tm_mon est 0-based (0 = janvier)
		candidate.tm_mday = day;
		candidate.tm_hour = 0;
		candidate.tm_min = 0;
		candidate.tm_sec = 0;
		candidate.tm_isdst = -1;  // Laisser mktime() déterminer DST

		// Validation finale via mktime() : détecte les dates invalides (ex: 31 février)
		if (::mktime(&candidate) == static_cast<time_t>(-1)) {
			return false;
		}

		// Succès : copie de la date validée dans l'output
		outDate = candidate;
		return true;
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkExtractRotatedDate
	// DESCRIPTION : Extrait la date d'un nom de fichier de backup rotationné
	// PARAMS : entryName - Nom de fichier complet (ex: "app.log.20240115")
	//          baseName - Nom de base sans suffixe date (ex: "app.log")
	//          outDate - Référence pour stocker la date extraite
	// RETURN : true si extraction réussie, false si format invalide
	// NOTE : Format attendu : {baseName}.YYYYMMDD (exactement 8 chiffres après le point)
	// -------------------------------------------------------------------------
	bool NkExtractRotatedDate(
		const nkentseu::NkString& entryName,
		const nkentseu::NkString& baseName,
		tm& outDate
	) {
		// Vérification de la taille attendue : base + "." + 8 chiffres
		const nkentseu::usize requiredSize = baseName.Length() + 1 + 8;
		if (entryName.Length() != requiredSize) {
			return false;
		}

		// Vérification du préfixe de base
		if (!NkHasPrefix(entryName, baseName)) {
			return false;
		}

		// Vérification du séparateur de suffixe
		const nkentseu::usize dotPos = baseName.Length();
		if (entryName[dotPos] != '.') {
			return false;
		}

		// Extraction des 8 chiffres de date
		char digits[9];  // 8 chiffres + null terminator
		for (int i = 0; i < 8; ++i) {
			digits[i] = entryName[dotPos + 1 + static_cast<nkentseu::usize>(i)];
		}
		digits[8] = '\0';

		// Parsing et validation via NkParseYmd8
		return NkParseYmd8(digits, outDate);
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkNowNs
	// DESCRIPTION : Obtient le timestamp courant en nanosecondes depuis Unix epoch
	// RETURN : uint64 représentant les nanosecondes depuis 1970-01-01 00:00:00 UTC
	// NOTE : Implémentation multiplateforme avec fallback sécurisé
	// -------------------------------------------------------------------------
	nkentseu::uint64 NkNowNs() {
		#if defined(_WIN32)
			// Windows : utilisation de GetSystemTimeAsFileTime (précision ~100ns)
			FILETIME fileTime{};
			::GetSystemTimeAsFileTime(&fileTime);

			// Conversion FILETIME (100-nanosecond intervals since 1601) → Unix epoch
			ULARGE_INTEGER ticks{};
			ticks.LowPart = fileTime.dwLowDateTime;
			ticks.HighPart = fileTime.dwHighDateTime;

			// Offset entre Windows epoch (1601) et Unix epoch (1970) en nanosecondes
			constexpr nkentseu::uint64 kWinToUnixEpochOffsetNs = 11644473600ULL * 1000000000ULL;

			// Conversion : FILETIME est en unités de 100ns → multiplication par 100
			const nkentseu::uint64 nowNs = static_cast<nkentseu::uint64>(ticks.QuadPart) * 100ULL;

			// Soustraction de l'offset avec protection contre underflow
			return (nowNs >= kWinToUnixEpochOffsetNs)
				? (nowNs - kWinToUnixEpochOffsetNs)
				: 0ULL;

		#else
			// POSIX : utilisation de clock_gettime avec CLOCK_REALTIME
			timespec ts{};

			// Appel système : retourne 0 en cas de succès, -1 en cas d'erreur
			if (clock_gettime(CLOCK_REALTIME, &ts) == 0) {
				// Combinaison secondes + nanosecondes en uint64 unique
				return (static_cast<uint64>(ts.tv_sec) * 1000000000ULL)
					+ static_cast<uint64>(ts.tv_nsec);
			}

			// Fallback en cas d'erreur système : retourne 0 (timestamp invalide)
			return 0;
		#endif
	}


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

		struct stat fileInfo{};
		return ::stat(path.CStr(), &fileInfo) == 0;
	}


} // namespace anonymous


// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques et privées de NkDailyFileSink.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur
	// DESCRIPTION : Initialise avec paramètres de rotation et date courante
	// -------------------------------------------------------------------------
	NkDailyFileSink::NkDailyFileSink(
		const NkString& filename,
		int hour,
		int minute,
		usize maxDays
	) : NkFileSink(filename, false)  // Append mode par défaut pour rotation
		, m_RotationHour(hour)
		, m_RotationMinute(minute)
		, m_MaxDays(maxDays)
		, m_LastCheck(0) {

		// Initialisation de m_CurrentDate avec la date système courante
		const time_t now = ::time(nullptr);

		#ifdef _WIN32
			// Windows : version thread-safe de localtime
			localtime_s(&m_CurrentDate, &now);
		#else
			// POSIX : version thread-safe de localtime
			localtime_r(&now, &m_CurrentDate);
		#endif

		// Initialisation de m_LastCheck pour permettre vérification immédiate
		m_LastCheck = NkNowNs();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur
	// DESCRIPTION : Cleanup via destructeur de NkFileSink
	// -------------------------------------------------------------------------
	NkDailyFileSink::~NkDailyFileSink() {
		// Destructeur de NkFileSink appelé automatiquement :
		// - Close() pour libération du FILE*
		// - Flush() pour persistance des buffers
		// Aucune logique supplémentaire nécessaire
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Log
	// DESCRIPTION : Écriture avec vérification de rotation quotidienne
	// -------------------------------------------------------------------------
	void NkDailyFileSink::Log(const NkLogMessage& message) {
		// Appel de la méthode parente pour écriture normale thread-safe
		// Note : NkFileSink::Log() appelle CheckRotation() en fin de méthode
		NkFileSink::Log(message);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetRotationTime
	// DESCRIPTION : Définit l'heure de rotation avec synchronisation thread-safe
	// -------------------------------------------------------------------------
	void NkDailyFileSink::SetRotationTime(int hour, int minute) {
		// Acquisition du mutex hérité pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour des paramètres de rotation
		m_RotationHour = hour;
		m_RotationMinute = minute;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetRotationHour
	// DESCRIPTION : Retourne l'heure de rotation configurée (lecture thread-safe)
	// -------------------------------------------------------------------------
	int NkDailyFileSink::GetRotationHour() const {
		// Acquisition du mutex hérité pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur configurée
		return m_RotationHour;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetRotationMinute
	// DESCRIPTION : Retourne la minute de rotation configurée (lecture thread-safe)
	// -------------------------------------------------------------------------
	int NkDailyFileSink::GetRotationMinute() const {
		// Acquisition du mutex hérité pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur configurée
		return m_RotationMinute;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetMaxDays
	// DESCRIPTION : Définit le nombre de jours de conservation avec synchronisation
	// -------------------------------------------------------------------------
	void NkDailyFileSink::SetMaxDays(usize maxDays) {
		// Acquisition du mutex hérité pour modification thread-safe
		threading::NkScopedLock lock(m_Mutex);

		// Mise à jour du nombre maximum de jours
		m_MaxDays = maxDays;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetMaxDays
	// DESCRIPTION : Retourne le nombre de jours configuré (lecture thread-safe)
	// -------------------------------------------------------------------------
	usize NkDailyFileSink::GetMaxDays() const {
		// Acquisition du mutex hérité pour lecture protégée
		threading::NkScopedLock lock(m_Mutex);

		// Retour de la valeur configurée
		return m_MaxDays;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Rotate
	// DESCRIPTION : Force la rotation indépendamment de la date courante
	// -------------------------------------------------------------------------
	bool NkDailyFileSink::Rotate() {
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
	// DESCRIPTION : Vérifie la condition de rotation quotidienne et déclenche si nécessaire
	// -------------------------------------------------------------------------
	void NkDailyFileSink::CheckRotation() {
		// Note : cette méthode est appelée depuis Log() qui détient déjà m_Mutex
		// Pas besoin d'acquérir le mutex ici : déjà acquis par l'appelant

		// Obtention du timestamp courant en nanosecondes
		const uint64 nowNs = NkNowNs();

		// Limitation de fréquence : vérifier max une fois par minute
		// 60000000000ULL = 60 secondes * 1e9 ns/s
		if (m_LastCheck > 0 && nowNs > m_LastCheck
			&& (nowNs - m_LastCheck) < 60000000000ULL) {
			return;
		}

		// Mise à jour du timestamp de dernière vérification
		m_LastCheck = nowNs;

		// Conversion du timestamp en time_t pour localtime
		const time_t nowTime = static_cast<time_t>(nowNs / 1000000000ULL);

		// Obtention de la date/heure locale courante
		tm nowDate{};
		#ifdef _WIN32
			localtime_s(&nowDate, &nowTime);
		#else
			localtime_r(&nowTime, &nowDate);
		#endif

		// Vérification de changement de date (année/mois/jour)
		const bool dateChanged = (nowDate.tm_year != m_CurrentDate.tm_year)
			|| (nowDate.tm_mon != m_CurrentDate.tm_mon)
			|| (nowDate.tm_mday != m_CurrentDate.tm_mday);

		if (dateChanged) {
			// Vérification de l'heure de rotation configurée
			const bool isAtRotationTime =
				(nowDate.tm_hour > m_RotationHour)
				|| (nowDate.tm_hour == m_RotationHour && nowDate.tm_min >= m_RotationMinute);

			if (isAtRotationTime) {
				// Condition remplie : exécuter la rotation
				PerformRotation();

				// Mise à jour de la date courante pour la prochaine comparaison
				m_CurrentDate = nowDate;
			}
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : PerformRotation (privée)
	// DESCRIPTION : Exécute la séquence complète de rotation quotidienne
	// -------------------------------------------------------------------------
	void NkDailyFileSink::PerformRotation() {
		// Note : doit être appelée avec m_Mutex déjà acquis (via Log() ou Rotate())

		// Étape 1 : Fermer le fichier courant avant manipulation
		CloseUnlocked();

		// Étape 2 : Générer le nom de fichier de backup avec la date courante
		const NkString rotatedFile = GetFilenameForDate(m_CurrentDate);

		// Étape 3 : Renommer le fichier courant en backup
		const NkString currentFile = GetFilenameUnlocked();
		if (NkFileExists(currentFile)) {
			// rename() retourne 0 en cas de succès, -1 en cas d'échec
			// Échec ignoré silencieusement pour robustesse en production
			(void)::rename(currentFile.CStr(), rotatedFile.CStr());
		}

		// Étape 4 : Nettoyer les anciens fichiers si conservation limitée
		if (m_MaxDays > 0) {
			CleanOldFiles();
		}

		// Étape 5 : Rouvrir un nouveau fichier courant vide
		(void)OpenUnlocked();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : CleanOldFiles (privée)
	// DESCRIPTION : Supprime les fichiers de backup plus anciens que m_MaxDays
	// -------------------------------------------------------------------------
	void NkDailyFileSink::CleanOldFiles() {
		// Note : doit être appelée avec m_Mutex déjà acquis

		// Cas spécial : conservation illimitée, rien à faire
		if (m_MaxDays == 0) {
			return;
		}

		// Extraction du répertoire et du nom de base du fichier courant
		const NkString currentFile = GetFilenameUnlocked();
		const nkentseu::usize slashPos = currentFile.FindLastOf("/\\");

		const NkString directory = (slashPos == nkentseu::NkString::npos)
			? NkString(".")
			: currentFile.SubStr(0, slashPos);

		const NkString baseName = (slashPos == nkentseu::NkString::npos)
			? currentFile
			: currentFile.SubStr(slashPos + 1);

		// Validation : nom de base non vide requis
		if (baseName.Empty()) {
			return;
		}

		#if defined(_WIN32)
			// Windows : utilisation de FindFirstFile/FindNextFile pour parcours
			NkString pattern = directory;
			if (!pattern.Empty() && !NkIsPathSeparator(pattern[pattern.Length() - 1])) {
				pattern.PushBack('\\');
			}
			pattern.Append("*");  // Pattern pour tous les fichiers du répertoire

			WIN32_FIND_DATAA findData{};
			HANDLE handle = ::FindFirstFileA(pattern.CStr(), &findData);
			if (handle == INVALID_HANDLE_VALUE) {
				return;
			}

			// Parcours de tous les fichiers correspondant au pattern
			do {
				// Ignorer les répertoires
				if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
					continue;
				}

				const NkString entryName(findData.cFileName);
				tm entryDate{};

				// Extraction et validation de la date depuis le nom de fichier
				if (!NkExtractRotatedDate(entryName, baseName, entryDate)) {
					continue;
				}

				// Vérification si la date est trop ancienne
				if (!IsDateTooOld(entryDate)) {
					continue;
				}

				// Suppression du fichier trop ancien
				const NkString fullPath = NkJoinPath(directory, entryName);
				(void)::remove(fullPath.CStr());

			} while (::FindNextFileA(handle, &findData) != 0);

			// Nettoyage du handle de recherche
			(void)::FindClose(handle);

		#else
			// POSIX : utilisation de opendir/readdir pour parcours
			DIR* dir = ::opendir(directory.CStr());
			if (dir == nullptr) {
				return;
			}

			// Parcours de toutes les entries du répertoire
			for (dirent* entry = ::readdir(dir); entry != nullptr; entry = ::readdir(dir)) {
				// Ignorer les entries vides
				if (entry->d_name[0] == '\0') {
					continue;
				}

				const NkString entryName(entry->d_name);
				tm entryDate{};

				// Extraction et validation de la date depuis le nom de fichier
				if (!NkExtractRotatedDate(entryName, baseName, entryDate)) {
					continue;
				}

				// Vérification si la date est trop ancienne
				if (!IsDateTooOld(entryDate)) {
					continue;
				}

				// Suppression du fichier trop ancien
				const NkString fullPath = NkJoinPath(directory, entryName);
				(void)::remove(fullPath.CStr());
			}

			// Fermeture du descripteur de répertoire
			(void)::closedir(dir);
		#endif
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetFilenameForDate (privée)
	// DESCRIPTION : Génère le chemin de fichier de backup pour une date donnée
	// -------------------------------------------------------------------------
	NkString NkDailyFileSink::GetFilenameForDate(const tm& date) const {
		// Note : appelée avec m_Mutex déjà acquis, lecture safe de m_Filename

		// Buffer pour le suffixe de date : ".YYYYMMDD"
		char suffixBuffer[32];

		// Formatage sûr de la date en suffixe décimal
		const int written = ::snprintf(
			suffixBuffer,
			sizeof(suffixBuffer),
			".%04d%02d%02d",
			date.tm_year + 1900,  // tm_year est offset depuis 1900
			date.tm_mon + 1,       // tm_mon est 0-based (0 = janvier)
			date.tm_mday
		);

		// Récupération du chemin de base via méthode unlocked (mutex déjà acquis)
		NkString result = GetFilenameUnlocked();

		// Append du suffixe si formatage réussi
		if (written > 0) {
			result.Append(suffixBuffer, static_cast<nkentseu::usize>(written));
		}

		return result;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : ExtractDateFromFilename (privée)
	// DESCRIPTION : Extrait la date d'un nom de fichier de backup
	// -------------------------------------------------------------------------
	tm NkDailyFileSink::ExtractDateFromFilename(const NkString& filename) const {
		// Initialisation d'une date invalide par défaut
		tm date{};
		date.tm_year = -1;  // Marqueur d'invalidité
		date.tm_mon = 0;
		date.tm_mday = 0;
		date.tm_hour = 0;
		date.tm_min = 0;
		date.tm_sec = 0;
		date.tm_isdst = -1;

		// Validation minimale : nom de fichier assez long pour contenir la date
		if (filename.Length() < 9) {
			return date;
		}

		// Position attendue du point séparateur : 9 caractères avant la fin (.YYYYMMDD)
		const nkentseu::usize dotPos = filename.Length() - 9;
		if (filename[dotPos] != '.') {
			return date;
		}

		// Extraction des 8 chiffres de date
		char digits[9];  // 8 chiffres + null terminator
		for (int i = 0; i < 8; ++i) {
			digits[i] = filename[dotPos + 1 + static_cast<nkentseu::usize>(i)];
		}
		digits[8] = '\0';

		// Parsing et validation via NkParseYmd8
		tm parsed{};
		if (NkParseYmd8(digits, parsed)) {
			return parsed;
		}

		// Échec de parsing : retour de la date invalide par défaut
		return date;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : IsDateTooOld (privée)
	// DESCRIPTION : Vérifie si une date est plus ancienne que le seuil de conservation
	// -------------------------------------------------------------------------
	bool NkDailyFileSink::IsDateTooOld(const tm& date) const {
		// Cas spéciaux : conservation illimitée ou date invalide
		if (m_MaxDays == 0 || date.tm_year < 0) {
			return false;
		}

		// Obtention de la date système courante
		const time_t now = ::time(nullptr);
		tm nowDate{};
		#ifdef _WIN32
			localtime_s(&nowDate, &now);
		#else
			localtime_r(&now, &nowDate);
		#endif

		// Normalisation de nowDate à minuit pour comparaison jour-par-jour
		nowDate.tm_hour = 0;
		nowDate.tm_min = 0;
		nowDate.tm_sec = 0;
		nowDate.tm_isdst = -1;

		// Conversion en time_t pour calcul arithmétique
		time_t threshold = ::mktime(&nowDate);
		if (threshold == static_cast<time_t>(-1)) {
			return false;
		}

		// Calcul du seuil : now - m_MaxDays jours (86400 secondes/jour)
		threshold -= static_cast<time_t>(m_MaxDays) * 86400;

		// Normalisation de la date candidate à minuit
		tm candidate = date;
		candidate.tm_hour = 0;
		candidate.tm_min = 0;
		candidate.tm_sec = 0;
		candidate.tm_isdst = -1;

		// Conversion de la candidate en time_t pour comparaison
		const time_t candidateTime = ::mktime(&candidate);
		if (candidateTime == static_cast<time_t>(-1)) {
			return false;
		}

		// Comparaison : candidate est trop ancienne si < threshold
		return candidateTime < threshold;
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
	1. PERFORMANCE DE CheckRotation() :
	   - Limitation à une vérification par minute via m_LastCheck
	   - localtime() appelé uniquement si minute écoulée depuis dernière vérification
	   - Comparaison de date : simple comparaison d'entiers (tm_year, tm_mon, tm_mday)
	   - Overhead négligeable en régime normal : < 1 µs par appel Log()

	2. ATOMICITÉ DE PerformRotation() :
	   - Séquence Close() → rename() → Open() doit être exécutée atomiquement
	   - Garantit par le fait que cette méthode est appelée uniquement depuis
	     Log() ou Rotate() qui acquièrent déjà m_Mutex
	   - Risk : crash pendant rotation → fichier backup potentiellement partiel
	   - Mitigation : rotation déclenchée uniquement après écriture complète du message

	3. GESTION DES ERREURS DE rename()/remove() :
	   - rename()/remove() peuvent échouer : permissions, disque plein, fichier verrouillé
	   - Implémentation : (void)::rename()/remove() ignore le code de retour
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