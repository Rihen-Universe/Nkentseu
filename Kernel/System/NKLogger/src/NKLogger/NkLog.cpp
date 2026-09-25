// =============================================================================
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// NKLogger/NkLog.cpp
// Implémentation du logger singleton par défaut avec API fluide.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Initialisation lazy avec defaults raisonnables si Initialize() non appelée
//  - Sinks console et fichier configurés par défaut pour usage immédiat
//  - Synchronisation thread-safe via NKThreading/NkMutex (hérité de NkLogger)
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include "pch.h"
#include "NKCore/Text/NkSnprintf.h"

#include "NKLogger/NkLog.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkLoggerFormatter.h"

// Sinks concrets pour configuration par défaut
#include "NKLogger/Sinks/NkConsoleSink.h"
#include "NKLogger/Sinks/NkFileSink.h"

// Un journal par LANCEMENT (2026-08-18) : horodatage, PID, listage du dossier
// et purge. NKLogger ne depend PAS de NKFileSystem (voir NKLogger.jenga) — le
// listage se fait donc en natif, exactement comme NkDailyFileSink.cpp le fait
// deja dans ce meme module.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#if !defined(_WIN32)
#include <dirent.h>
#include <unistd.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS
// -------------------------------------------------------------------------
// Implémentation des méthodes statiques et membres de NkLog.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {

	// -------------------------------------------------------------------------
	// SECTION 1bis : UN JOURNAL PAR LANCEMENT (2026-08-18)
	// -------------------------------------------------------------------------
	// PROBLEME MESURE : `logs/app.log` etait ouvert en mode APPEND, donc jamais
	// remis a zero. Un fichier reel de Nogee contenait 10 692 lignes couvrant
	// SIX lancements sur DEUX jours, dont quatre en 74 secondes — deux lignes
	// consecutives y sont separees de 26 heures, sans le moindre marqueur. Il
	// fallait decouper a la main par horodatage pour savoir quelle course avait
	// produit quelle ligne, et Rodolf devait noter l'heure de ses tests pour
	// retrouver les siennes. Des mesures ont deja ete faussees par ce melange.
	//
	// POURQUOI PAR LANCEMENT ET NON PAR DATE : une granularite a la journee ne
	// separe pas deux courses du meme jour — or c'est exactement le cas qui a
	// coute du temps (quatre lancements en 74 secondes). Le PID desambigue en
	// plus deux lancements tombant dans la meme seconde.
	//
	// CE QUI NE CHANGE PAS : `logs/app.log` garde son nom et son chemin. Tout
	// outil, script ou habitude qui le lit continue de fonctionner — il ne
	// contient simplement plus que la DERNIERE course, au lieu de toutes.
	namespace {

		/// @brief Dossier des journaux, relatif au repertoire courant.
		constexpr const char *kRunLogDirectory = "logs";

		/// @brief Prefixe des journaux par lancement (distinct de "app.log").
		constexpr const char *kRunLogPrefix = "app_";

		/// @brief Extension des journaux.
		constexpr const char *kRunLogSuffix = ".log";

		/// @brief Journal de la course COURANTE : chemin historique, inchange.
		constexpr const char *kCurrentRunLogPath = "logs/app.log";

		/// @brief Nombre de journaux par lancement conserves par defaut.
		/// @note Un COMPTE, et non un age : un age ne borne rien (les six
		///       courses mesurees tiennent dans vingt minutes), alors qu'un
		///       compte borne le disque quel que soit le rythme de travail.
		constexpr int kDefaultRunLogsKept = 20;

		/// @brief Capacite des tampons de nom de fichier.
		constexpr size_t kRunLogNameCapacity = 256;

		// ---------------------------------------------------------------------
		// FONCTION : NkRunLogsToKeep
		// DESCRIPTION : Nombre de journaux a conserver (env. NKENTSEU_LOG_KEEP)
		// RETOUR : > 0 = nombre conserve ; 0 = ne jamais purger
		// ---------------------------------------------------------------------
		int NkRunLogsToKeep() {
			const char *raw = ::getenv("NKENTSEU_LOG_KEEP");

			// Variable absente ou vide : politique par defaut
			if (raw == nullptr || raw[0] == '\0') {
				return kDefaultRunLogsKept;
			}

			// Valeur illisible ou negative : on retombe sur le defaut plutot
			// que de desactiver silencieusement la purge
			const long parsed = ::strtol(raw, nullptr, 10);
			if (parsed < 0) {
				return kDefaultRunLogsKept;
			}

			return static_cast<int>(parsed);
		}

		// ---------------------------------------------------------------------
		// FONCTION : NkCurrentProcessId
		// DESCRIPTION : Identifiant du processus courant, pour desambiguiser
		//               deux lancements tombant dans la meme seconde
		// ---------------------------------------------------------------------
		unsigned long NkCurrentProcessId() {
#if defined(_WIN32)
			return static_cast<unsigned long>(::GetCurrentProcessId());
#else
			return static_cast<unsigned long>(::getpid());
#endif
		}

		// ---------------------------------------------------------------------
		// FONCTION : NkMakeRunLogPath
		// DESCRIPTION : Chemin du journal de CETTE course
		// RETOUR : "logs/app_YYYY-MM-DD_HHMMSS_<pid>.log"
		// NOTE : l'horodatage est en tete et de LARGEUR FIXE — l'ordre
		//        lexicographique des noms est donc l'ordre chronologique, ce
		//        dont la purge se sert pour trouver le plus ancien sans trier.
		// ---------------------------------------------------------------------
		NkString NkMakeRunLogPath() {
			const time_t now = ::time(nullptr);
			tm local{};

#if defined(_WIN32)
			// Windows : version thread-safe de localtime
			::localtime_s(&local, &now);
#else
			// POSIX : version thread-safe de localtime
			::localtime_r(&now, &local);
#endif

			char buffer[kRunLogNameCapacity] = {0};
			nkentseu::NkSnprintf(buffer, sizeof(buffer), "%s/%s%04d-%02d-%02d_%02d%02d%02d_%lu%s",
					   kRunLogDirectory, kRunLogPrefix, local.tm_year + 1900, local.tm_mon + 1,
					   local.tm_mday, local.tm_hour, local.tm_min, local.tm_sec,
					   NkCurrentProcessId(), kRunLogSuffix);

			return NkString(buffer);
		}

		// ---------------------------------------------------------------------
		// FONCTION : NkIsRunLogName
		// DESCRIPTION : Vrai si le nom est un journal PAR LANCEMENT
		// NOTE : "app.log" ne commence pas par "app_" — le journal courant ne
		//        peut donc JAMAIS etre purge par erreur.
		// ---------------------------------------------------------------------
		bool NkIsRunLogName(const char *name) {
			if (name == nullptr) {
				return false;
			}

			const size_t prefixLength = ::strlen(kRunLogPrefix);
			const size_t suffixLength = ::strlen(kRunLogSuffix);
			const size_t nameLength = ::strlen(name);

			// Un nom valide porte au moins un horodatage entre les deux
			if (nameLength <= prefixLength + suffixLength) {
				return false;
			}

			if (::strncmp(name, kRunLogPrefix, prefixLength) != 0) {
				return false;
			}

			return ::strcmp(name + nameLength - suffixLength, kRunLogSuffix) == 0;
		}

		// ---------------------------------------------------------------------
		// FONCTION : NkScanRunLogs
		// DESCRIPTION : Compte les journaux par lancement et depose le nom du
		//               plus ancien (plus petit au sens lexicographique)
		// PARAMS : outOldest - tampon receveur (peut etre nullptr)
		//          outSize   - capacite du tampon
		// RETOUR : nombre de journaux trouves (0 si le dossier n'existe pas)
		// NOTE : sans conteneur, donc sans allocation — un seul nom est retenu
		//        a la fois. Meme idiome de parcours que NkDailyFileSink.cpp.
		// ---------------------------------------------------------------------
		int NkScanRunLogs(char *outOldest, size_t outSize) {
			if (outOldest != nullptr && outSize > 0) {
				outOldest[0] = '\0';
			}

			int found = 0;

#if defined(_WIN32)
			// Windows : parcours via FindFirstFile/FindNextFile
			char pattern[kRunLogNameCapacity] = {0};
			nkentseu::NkSnprintf(pattern, sizeof(pattern), "%s\\%s*%s", kRunLogDirectory, kRunLogPrefix,
					   kRunLogSuffix);

			WIN32_FIND_DATAA findData{};
			HANDLE handle = ::FindFirstFileA(pattern, &findData);

			// Dossier absent ou vide : rien a compter, rien a purger
			if (handle == INVALID_HANDLE_VALUE) {
				return 0;
			}

			do {
				// Ignorer les repertoires
				if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
					continue;
				}

				// Le motif "app_*.log" est large : on revalide le nom
				if (!NkIsRunLogName(findData.cFileName)) {
					continue;
				}

				++found;

				// Retenir le plus ancien rencontre jusqu'ici
				if (outOldest != nullptr && outSize > 0 &&
					(outOldest[0] == '\0' || ::strcmp(findData.cFileName, outOldest) < 0)) {
					nkentseu::NkSnprintf(outOldest, outSize, "%s", findData.cFileName);
				}

			} while (::FindNextFileA(handle, &findData) != 0);

			// Nettoyage du handle de recherche
			(void)::FindClose(handle);
#else
			// POSIX : parcours via opendir/readdir
			DIR *dir = ::opendir(kRunLogDirectory);

			// Dossier absent : rien a compter, rien a purger
			if (dir == nullptr) {
				return 0;
			}

			for (dirent *entry = ::readdir(dir); entry != nullptr; entry = ::readdir(dir)) {
				if (!NkIsRunLogName(entry->d_name)) {
					continue;
				}

				++found;

				// Retenir le plus ancien rencontre jusqu'ici
				if (outOldest != nullptr && outSize > 0 &&
					(outOldest[0] == '\0' || ::strcmp(entry->d_name, outOldest) < 0)) {
					nkentseu::NkSnprintf(outOldest, outSize, "%s", entry->d_name);
				}
			}

			// Fermeture du descripteur de repertoire
			(void)::closedir(dir);
#endif

			return found;
		}

		// ---------------------------------------------------------------------
		// FONCTION : NkPurgeRunLogs
		// DESCRIPTION : Ne conserve que les `keep` journaux les plus recents
		// PARAMS : keep - nombre a conserver (0 ou moins = ne rien purger)
		// NOTE : appelee AVANT la creation du journal de la course courante,
		//        d'ou le budget de `keep - 1` : apres creation, on en a `keep`.
		// NOTE : sans purge, la cadence mesuree (six courses en vingt minutes)
		//        remplirait le disque en quelques semaines.
		// ---------------------------------------------------------------------
		void NkPurgeRunLogs(int keep) {
			// 0 = politique « ne jamais purger », demandee explicitement
			if (keep <= 0) {
				return;
			}

			// Place a laisser pour le journal qu'on va creer juste apres
			const int budget = keep - 1;

			char oldest[kRunLogNameCapacity] = {0};
			int remaining = NkScanRunLogs(oldest, sizeof(oldest));

			// Borne dure : jamais plus d'iterations que de fichiers comptes.
			// Sans elle, un remove() qui echouerait sans le dire ferait boucler
			// le DEMARRAGE de toutes les applications du depot.
			int guard = remaining;

			while (remaining > budget && guard > 0) {
				--guard;

				// Plus de candidat : compte et scan divergent, on sort
				if (oldest[0] == '\0') {
					break;
				}

				char fullPath[kRunLogNameCapacity * 2] = {0};
				nkentseu::NkSnprintf(fullPath, sizeof(fullPath), "%s/%s", kRunLogDirectory, oldest);

				// Echec (fichier verrouille par un autre processus, permissions)
				// : on n'insiste pas. Un journal en trop ne casse rien ; une
				// boucle d'echecs au demarrage, si.
				if (::remove(fullPath) != 0) {
					break;
				}

				remaining = NkScanRunLogs(oldest, sizeof(oldest));
			}
		}


		// ---------------------------------------------------------------------
		// SECTION 1ter : CE QUE L'UTILISATEUR PEUT REGLER SANS RECOMPILER
		// ---------------------------------------------------------------------
		// MESURE DU 2026-09-25 qui a motive ce bloc. Un utilisateur ecrivait
		// `logger.Debug()` dans sa boucle de jeu et ne voyait RIEN. Trois causes
		// etaient vraies EN MEME TEMPS, et aucune ne se disait :
		//
		//   1. le niveau du journal vaut `info`, et NK_DEBUG (1) < NK_INFO (2) :
		//      `logger.Debug()` est rejete par `NkLogger::LogInternal` AVANT
		//      d'atteindre le moindre puits. Mesure : un binaire Debug branche
		//      pourtant un puits console REGLE SUR `debug` -- il ne recoit
		//      jamais rien. Le puits n'y peut rien : le filtre est en amont.
		//
		//   2. en Release, `NKLogger.jenga` pose `NDEBUG`, donc le puits console
		//      etait compile HORS du module. Mesure : `llvm-nm` ne trouve AUCUNE
		//      reference a `NkConsoleSink` dans `src_NKLogger_NkLog.obj` de
		//      Release, et `GetSinkCount()` rend 2 au lieu de 3. Une application
		//      Release n'avait donc aucun canal visible : meme `logger.Error()`
		//      partait dans le seul fichier.
		//
		//   3. la documentation de `NkLogLevel.h` promet `NK_LOG_LEVEL`... dans
		//      un BLOC DE COMMENTAIRE. Aucune ligne de code ne lisait cette
		//      variable. Celui qui la posait ne changeait rien et ne l'apprenait
		//      jamais.
		//
		// Trois variables existent desormais POUR DE VRAI :
		//   NK_LOG_LEVEL   = trace|debug|info|warn|error|critical|fatal|off
		//   NK_LOG_CONSOLE = 0 (aucun puits console) | 1 (puits sans filtre
		//                    propre) | un nom de niveau (puits filtre a ce
		//                    niveau)
		//   NK_LOG_QUIET   = 1 pour taire la ligne de demarrage
		//   NK_LOG_BANNER  = 1 pour la forcer meme hors console

		/// @brief Comparaison de deux chaines en ignorant la casse.
		/// @note `NkEqualsIgnoreCase` existe deja, mais en `static inline` DANS
		///       NkLogLevel.cpp : elle n'est visible d'aucune autre unite de
		///       compilation. On ne la deplace pas (cela toucherait l'entete
		///       public de tout le depot) ; on en garde une ici, locale.
		bool NkMemeTexteSansCasse(const char *a, const char *b) {
			if (a == nullptr || b == nullptr) {
				return a == b;
			}
			while (*a != '\0' && *b != '\0') {
				const char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
				const char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
				if (ca != cb) {
					return false;
				}
				++a;
				++b;
			}
			return *a == '\0' && *b == '\0';
		}

		/// @brief Vrai si la variable vaut 1/on/yes/true/oui (casse ignoree).
		bool NkEnvEstVrai(const char *nom) {
			const char *v = ::getenv(nom);
			if (v == nullptr || v[0] == '\0') {
				return false;
			}
			return NkMemeTexteSansCasse(v, "1") || NkMemeTexteSansCasse(v, "on") ||
				   NkMemeTexteSansCasse(v, "yes") || NkMemeTexteSansCasse(v, "true") ||
				   NkMemeTexteSansCasse(v, "oui");
		}

		/// @brief Vrai si la variable vaut 0/off/no/false/non (casse ignoree).
		bool NkEnvEstFaux(const char *nom) {
			const char *v = ::getenv(nom);
			if (v == nullptr || v[0] == '\0') {
				return false;
			}
			return NkMemeTexteSansCasse(v, "0") || NkMemeTexteSansCasse(v, "off") ||
				   NkMemeTexteSansCasse(v, "no") || NkMemeTexteSansCasse(v, "false") ||
				   NkMemeTexteSansCasse(v, "non");
		}

		/// @brief Lit un NIVEAU et DIT s'il a ete reconnu.
		/// @note `NkStringToLogLevel` rend `info` pour toute chaine inconnue :
		///       une faute de frappe y deviendrait silencieusement le defaut.
		///       Ici on repond deux choses -- la valeur ET si elle vient du
		///       texte -- pour pouvoir NOMMER la faute au lieu de l'effacer.
		bool NkNiveauDepuisTexte(const char *texte, NkLogLevel &sortie) {
			if (texte == nullptr || texte[0] == '\0') {
				return false;
			}
			static const char *kNoms[] = {"trace", "debug", "info",	   "warn",
										  "error", "critical", "fatal", "off"};
			static const NkLogLevel kNiveaux[] = {
				NkLogLevel::NK_TRACE, NkLogLevel::NK_DEBUG,	   NkLogLevel::NK_INFO,
				NkLogLevel::NK_WARN,  NkLogLevel::NK_ERROR,	   NkLogLevel::NK_CRITICAL,
				NkLogLevel::NK_FATAL, NkLogLevel::NK_OFF};
			for (int i = 0; i < 8; ++i) {
				if (NkMemeTexteSansCasse(texte, kNoms[i])) {
					sortie = kNiveaux[i];
					return true;
				}
			}
			// Alias tolere par NkStringToLogLevel
			if (NkMemeTexteSansCasse(texte, "warning")) {
				sortie = NkLogLevel::NK_WARN;
				return true;
			}
			return false;
		}

		/// @brief Chemin ABSOLU d'un chemin relatif au repertoire courant.
		/// @note C'est le point qui coute le plus de temps a l'usage : le
		///       journal n'est pas a cote de l'executable, il est a cote du
		///       REPERTOIRE DE TRAVAIL. Deux lancements du meme binaire depuis
		///       deux dossiers ecrivent dans deux fichiers differents. On
		///       imprime donc le chemin resolu, jamais « logs/app.log ».
		void NkCheminAbsolu(const char *relatif, char *sortie, size_t capacite) {
			if (sortie == nullptr || capacite == 0) {
				return;
			}
			sortie[0] = '\0';
#if defined(_WIN32)
			const DWORD n = ::GetFullPathNameA(relatif, (DWORD)capacite, sortie, nullptr);
			if (n == 0 || n >= capacite) {
				nkentseu::NkSnprintf(sortie, capacite, "%s", relatif);
			}
#else
			char courant[512] = {0};
			if (::getcwd(courant, sizeof(courant)) != nullptr) {
				nkentseu::NkSnprintf(sortie, capacite, "%s/%s", courant, relatif);
			} else {
				nkentseu::NkSnprintf(sortie, capacite, "%s", relatif);
			}
#endif
		}

		/// @brief Vrai si stderr est une VRAIE console (pas un tube, pas un fichier).
		/// @note La ligne de demarrage n'est destinee qu'a un humain. La
		///       conditionner a la console est ce qui garantit qu'AUCUN banc
		///       ne devient bavard : un banc redirige, donc il ne la voit pas.
		bool NkStderrEstUneConsole() {
#if defined(_WIN32)
			HANDLE h = ::GetStdHandle(STD_ERROR_HANDLE);
			if (h == nullptr || h == INVALID_HANDLE_VALUE) {
				return false;
			}
			return ::GetFileType(h) == FILE_TYPE_CHAR;
#else
			return ::isatty(2) != 0;
#endif
		}

	} // namespace anonyme

	// -------------------------------------------------------------------------
	// VARIABLE STATIQUE : s_Initialized
	// DESCRIPTION : Indicateur d'initialisation explicite via Initialize()
	// -------------------------------------------------------------------------
	bool NkLog::s_Initialized = false;

	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur privé
	// DESCRIPTION : Initialisation avec configuration par défaut et sinks
	// -------------------------------------------------------------------------
	NkLog::NkLog(const NkString &name) : NkLogger(name) {
		// =====================================================================
		// LA REGLE PAR DEFAUT (revue le 2026-09-25, apres mesure)
		// =====================================================================
		// Ce qu'elle etait, et pourquoi elle piegeait :
		//   Debug   : puits console (regle sur `debug`) + deux puits fichier.
		//   Release : deux puits fichier, et RIEN de visible. Meme une erreur
		//             fatale ne laissait aucune trace a l'ecran.
		//   Niveau du journal : `info` dans les DEUX cas -- donc `logger.Debug()`
		//             etait rejete en amont, y compris en Debug, y compris avec
		//             un puits console regle sur `debug`.
		//
		// Ce qu'elle est maintenant :
		//   - un puits console dans TOUTES les configurations ;
		//   - en Debug il laisse passer `debug` (comme avant) ;
		//   - en Release il ne laisse passer que `error` et pire. Ce choix n'est
		//     pas timide, il est MESURE : `NkConsoleSink` envoie error, critical
		//     et fatal sur STDERR (GetStreamForLevel, m_UseStderrForErrors vaut
		//     vrai par defaut). La sortie STANDARD d'un binaire Release est donc
		//     rigoureusement inchangee -- aucun banc qui lit stdout ne devient
		//     bavard -- et pourtant une erreur se voit enfin.
		//   - le NIVEAU du journal reste `info` par defaut : le changer rendrait
		//     bavards tous les bancs Debug du depot. Il se regle par
		//     `NK_LOG_LEVEL`, et surtout la ligne de demarrage DIT qu'il filtre
		//     `debug`. Le piege n'etait pas la valeur, c'etait le silence.
		//
		// Sur ANDROID le puits console reste actif MÊME en Release, et ce n'est
		// pas une entorse : sur téléphone il n'y a pas de console à polluer —
		// NkConsoleSink route vers LOGCAT, qui est le journal du système et le
		// seul moyen d'observer une application. Le sink fichier, lui, ne prend
		// pas le relais : « logs/app.log » est un chemin RELATIF, or le
		// répertoire courant d'une application Android n'est pas inscriptible.
		// Sans cette exception, un build Release sur téléphone n'écrit donc
		// NULLE PART. Mesuré le 2026-08-12 sur NKARDemo : écran noir, aucune
		// trace, et des heures passées à chercher sans instrument.

		// --- 1. Le niveau du journal -----------------------------------------
		NkLogLevel niveauJournal = NkLogLevel::NK_INFO;
		const char *texteNiveau = ::getenv("NK_LOG_LEVEL");
		bool niveauVientDeLEnv = false;
		bool niveauEnvIllisible = false;
		if (texteNiveau != nullptr && texteNiveau[0] != '\0') {
			NkLogLevel lu = NkLogLevel::NK_INFO;
			if (NkNiveauDepuisTexte(texteNiveau, lu)) {
				niveauJournal = lu;
				niveauVientDeLEnv = true;
			} else {
				// On NE retombe PAS silencieusement sur `info` : la ligne de
				// demarrage nommera la valeur refusee.
				niveauEnvIllisible = true;
			}
		}

		// --- 2. Le puits console ---------------------------------------------
		// Niveau par defaut du puits, selon la configuration de compilation.
#if defined(NKENTSEU_PLATFORM_ANDROID) || defined(__ANDROID__)
		NkLogLevel niveauConsole = NkLogLevel::NK_DEBUG;
#elif defined(NDEBUG)
		NkLogLevel niveauConsole = NkLogLevel::NK_ERROR;
#else
		NkLogLevel niveauConsole = NkLogLevel::NK_DEBUG;
#endif
		// ⚠️ UNE SEULE VARIABLE DOIT SUFFIRE. Premiere version mesuree le
		//    2026-09-25 : avec `NK_LOG_LEVEL=debug` seul, en Release, le journal
		//    laissait bien passer `debug` MAIS le puits console gardait son
		//    filtre `error` -- l'ecran restait muet, et il fallait DEUX
		//    variables pour voir une ligne. C'est le meme piege que celui qu'on
		//    corrige, un cran plus loin. Poser `NK_LOG_LEVEL` explicitement, ce
		//    n'est pas regler un compteur : c'est dire « je veux VOIR ce
		//    niveau ». Le puits console suit donc, sauf si `NK_LOG_CONSOLE` dit
		//    autre chose -- l'explicite l'emporte toujours sur le deduit.
		if (niveauVientDeLEnv) {
			niveauConsole = niveauJournal;
		}
		bool consoleBranchee = true;
		if (NkEnvEstFaux("NK_LOG_CONSOLE")) {
			consoleBranchee = false;
		} else if (NkEnvEstVrai("NK_LOG_CONSOLE")) {
			// « 1 » = le puits n'ajoute AUCUN filtre propre : seul le niveau du
			// journal decide. C'est ce que veut quelqu'un qui cherche pourquoi
			// il ne voit rien.
			niveauConsole = NkLogLevel::NK_TRACE;
		} else {
			NkLogLevel lu = NkLogLevel::NK_INFO;
			if (NkNiveauDepuisTexte(::getenv("NK_LOG_CONSOLE"), lu)) {
				niveauConsole = lu;
			}
		}

		if (consoleBranchee) {
			// Sur Android : NkConsoleSink route automatiquement vers logcat
			NkConsoleSink *consoleSinkRaw = new NkConsoleSink();
			consoleSinkRaw->SetColorEnabled(true); // Activer les couleurs ANSI si supporté
			consoleSinkRaw->SetLevel(niveauConsole);
			memory::NkSharedPtr<NkISink> consoleSink(consoleSinkRaw);
			AddSink(consoleSink);
		}

		// --- 3. Les puits fichier --------------------------------------------
		// Persistance des logs. TOUJOURS actifs (debug + release) pour permettre
		// le post-mortem en prod quand un user testeur rencontre un bug. Le
		// fichier reste disponible apres crash, contrairement a la console.
		//
		// DEUX destinations depuis le 2026-08-18 (voir SECTION 1bis) :
		//   (a) logs/app_<date>_<heure>_<pid>.log — UN PAR LANCEMENT, garde
		//   (b) logs/app.log                      — la course COURANTE seule
		//
		// (b) garde son nom historique et continue d'alimenter tous les outils
		// et habitudes existants ; il est simplement TRONQUE au lancement au
		// lieu d'accumuler indefiniment.
		//
		// ⚠️ LEUR NIVEAU SUIT DESORMAIS CELUI DU JOURNAL quand on l'a baisse.
		//    Il etait fige a `info` : `NK_LOG_LEVEL=debug` aurait donc fait
		//    apparaitre les lignes a l'ecran mais PAS dans le fichier, et le
		//    fichier est justement ce qu'on envoie quand on demande de l'aide.
		const NkLogLevel niveauFichier =
			(niveauJournal < NkLogLevel::NK_INFO) ? niveauJournal : NkLogLevel::NK_INFO;

		// Purge AVANT creation : ne conserver que les N derniers journaux
		NkPurgeRunLogs(NkRunLogsToKeep());

		// (a) Journal de CETTE course : jamais ecrase par un lancement suivant
		memory::NkSharedPtr<NkISink> runSink(new NkFileSink(NkMakeRunLogPath(), /*truncate*/ true));
		runSink->SetLevel(niveauFichier);
		runSink->SetPattern(NkLoggerFormatter::NK_DEFAULT_PATTERN); // Pattern lisible
		AddSink(runSink);

		// (b) Journal courant : chemin historique, remis a zero a chaque lancement
		memory::NkSharedPtr<NkISink> fileSink(new NkFileSink(kCurrentRunLogPath, /*truncate*/ true));
		fileSink->SetLevel(niveauFichier);
		fileSink->SetPattern(NkLoggerFormatter::NK_DEFAULT_PATTERN); // Pattern lisible
		AddSink(fileSink);

		// --- 4. Configuration globale du logger -------------------------------
		SetLevel(niveauJournal);
		SetPattern(NkLoggerFormatter::NK_NKENTSEU_PATTERN); // Pattern avec support couleurs

		// --- 5. LA LIGNE QUI DIT OU EST LE JOURNAL ----------------------------
		// Elle part sur STDERR, et seulement si stderr est une vraie console (ou
		// si on la force). Deux raisons, toutes deux mesurees :
		//   - stdout est ce que lisent les bancs : on n'y touche pas ;
		//   - un banc redirige stderr aussi, donc la condition « console » suffit
		//     a garantir qu'aucun banc ne change de sortie.
		// Elle nomme le piege le plus couteux : `debug` et `trace` sont filtres.
#if !defined(NKENTSEU_PLATFORM_ANDROID) && !defined(__ANDROID__) && !defined(NKENTSEU_PLATFORM_HARMONYOS)
		const bool banniereForcee = NkEnvEstVrai("NK_LOG_BANNER");
		if (!NkEnvEstVrai("NK_LOG_QUIET") && (banniereForcee || NkStderrEstUneConsole())) {
			char chemin[1024] = {0};
			NkCheminAbsolu(kCurrentRunLogPath, chemin, sizeof(chemin));

			char ligne[2048] = {0};
			nkentseu::NkSnprintf(
				ligne, sizeof(ligne),
				"[NKLogger] niveau=%s%s | console=%s | journal=%s%s\n"
				"[NKLogger] trace/debug sont SOUS le niveau : NK_LOG_LEVEL=debug pour les voir ; "
				"NK_LOG_CONSOLE=1 pour tout mettre a l'ecran ; NK_LOG_QUIET=1 pour taire ces deux lignes.\n",
				NkLogLevelToString(niveauJournal), niveauVientDeLEnv ? " (NK_LOG_LEVEL)" : "",
				consoleBranchee ? NkLogLevelToString(niveauConsole) : "aucune", chemin,
				niveauEnvIllisible ? " | ATTENTION : NK_LOG_LEVEL illisible, ignoree" : "");

			(void)::fputs(ligne, stderr);
			(void)::fflush(stderr);
		}
#endif
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur privé
	// DESCRIPTION : Flush garanti avant destruction du singleton
	// -------------------------------------------------------------------------
	NkLog::~NkLog() {
		// Flush explicite : garantir la persistance des logs en buffer
		Flush();

		// ClearSinks() appelé automatiquement par ~NkLogger() de la base
		// Pas besoin d'appel explicite ici
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Instance (static)
	// DESCRIPTION : Retourne l'instance singleton via Meyer's singleton
	// -------------------------------------------------------------------------
	NkLog &NkLog::Instance() {
		// Meyer's singleton : static local, thread-safe en C++11+
		// L'instance est créée au premier appel, détruite à la fin du programme
		static NkLog instance;
		return instance;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Initialize (static)
	// DESCRIPTION : Configure le logger singleton avec paramètres personnalisés
	// -------------------------------------------------------------------------
	void NkLog::Initialize(const NkString &name, const NkString &pattern, NkLogLevel level) {
		// Accès à l'instance singleton (création lazy si premier appel)
		NkLog &instance = Instance();

		// Mise à jour du nom si différent et non vide
		if (!name.Empty() && instance.GetName() != name) {
			instance.SetName(name);
		}

		// Application du pattern et du niveau de log
		instance.SetPattern(pattern);
		instance.SetLevel(level);

		// Marquer comme initialisé explicitement
		s_Initialized = true;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Shutdown (static)
	// DESCRIPTION : Flush et cleanup explicite avant terminaison
	// -------------------------------------------------------------------------
	void NkLog::Shutdown() {
		// Accès à l'instance singleton
		NkLog &instance = Instance();

		// Flush explicite : garantir la persistance des logs en buffer
		instance.Flush();

		// ClearSinks : libération des références partagées vers les sinks
		// Note : les sinks partagés peuvent être utilisés ailleurs, donc pas de destruction
		instance.ClearSinks();
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Named (API fluide)
	// DESCRIPTION : Définit le nom et retourne *this pour chaînage
	// -------------------------------------------------------------------------
	NkLog &NkLog::Named(const NkString &name) {
		// Délégation à SetName() protégé de NkLogger
		SetName(name);
		return *this;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Level (API fluide)
	// DESCRIPTION : Définit le niveau de log et retourne *this pour chaînage
	// -------------------------------------------------------------------------
	NkLog &NkLog::Level(NkLogLevel level) {
		// Délégation à SetLevel() de NkLogger
		SetLevel(level);
		return *this;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Pattern (API fluide)
	// DESCRIPTION : Définit le pattern et retourne *this pour chaînage
	// -------------------------------------------------------------------------
	NkLog &NkLog::Pattern(const NkString &pattern) {
		// Délégation à SetPattern() de NkLogger
		SetPattern(pattern);
		return *this;
	}

	// -------------------------------------------------------------------------
	// MÉTHODE : Source (override fluide)
	// DESCRIPTION : Configure les métadonnées de source et retourne *this
	// -------------------------------------------------------------------------
	NkLog &NkLog::Source(const char *sourceFile, uint32 sourceLine, const char *functionName) {
		// Appel à la méthode de base pour configuration des métadonnées
		NkLogger::Source(sourceFile, sourceLine, functionName);
		return *this;
	}

} // namespace nkentseu

// =============================================================================
// NOTES D'IMPLÉMENTATION ET BONNES PRATIQUES
// =============================================================================
/*
	1. SINGLETON MEYER'S :
	   - static local dans Instance() : thread-safe en C++11+ sans mutex explicite
	   - Destruction à la fin du programme : ordre indéterminé, éviter les dépendances
	   - Pour contrôle de l'ordre de destruction : utiliser Shutdown() explicitement

	2. CONFIGURATION PAR DÉFAUT :
	   - Console + fichier : couverture des cas d'usage courants (dev + prod)
	   - Niveaux différents : DEBUG en console, INFO en fichier pour équilibre
	   - Patterns adaptés : couleurs en console, lisible en fichier

	3. THREAD-SAFETY HÉRITÉE :
	   - Toutes les méthodes de NkLogger sont thread-safe via m_Mutex
	   - NkLog ne rajoute pas de synchronisation : réutilisation de l'infrastructure
	   - Les sinks partagés doivent être thread-safe individuellement

	4. GESTION DES SINKS PAR DÉFAUT :
	   - NkConsoleSink : route vers stdout/stderr, couleurs ANSI si supporté
	   - NkFileSink : écriture dans logs/app.log, rotation à implémenter si nécessaire
	   - Pour personnalisation : ClearSinks() puis AddSink() avec sinks custom

	5. INITIALISATION LAZY VS EXPLICITE :
	   - Lazy : Instance() crée le logger au premier usage avec defaults
	   - Explicite : Initialize() permet de configurer avant tout logging
	   - Recommandation : appeler Initialize() au startup pour contrôle total

	6. EXTENSIBILITÉ :
	   - Pour ajouter des sinks par défaut : modifier le constructeur NkLog()
	   - Pour changer les defaults : modifier les valeurs dans Initialize()
	   - Pour features spécifiques : ajouter des méthodes dans NkLog uniquement

	7. COMPATIBILITÉ MULTIPLATEFORME :
	   - NkConsoleSink gère automatiquement stdout/stderr vs logcat sur Android
	   - NkFileSink utilise fopen/fwrite portable, avec fallback en cas d'erreur
	   - Les chemins de fichiers : utiliser NKCore/NkPath pour normalisation cross-platform
*/

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - All Rights Reserved (see LICENSE)
// ============================================================