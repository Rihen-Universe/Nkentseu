// =============================================================================
// NKLogger/NkLogger.cpp
// Implémentation de la classe NkLogger principale.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Utilisation de NkFormatf de NKCore/NkFormat.h pour tout le formatage printf-style
//  - Sanitization UTF-8 centralisée pour sécurité des outputs
//  - Synchronisation thread-safe via NKThreading/NkMutex
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/NkLogger.h"
#include "NKLogger/NkLogMessage.h"
#include "NKLogger/NkLogLevel.h"
#include "NKContainers/String/NkFormat.h"
#include "NKContainers/String/NkString.h"
#include "NKThreading/NkMutex.h"
#include "NKThreading/NkThread.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>

#if !defined(NKENTSEU_PLATFORM_WINDOWS)
	#include <pthread.h>
	#if defined(NKENTSEU_PLATFORM_LINUX) && !defined(__ANDROID__)
		#include <sys/syscall.h>
	#endif
#endif


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE ANONYME - UTILITAIRES INTERNES
// -------------------------------------------------------------------------
// Fonctions et constantes à usage interne uniquement.
// Non exposées dans l'API publique pour encapsulation et optimisation.

namespace {


	// -------------------------------------------------------------------------
	// FONCTION : NkMapLatinAccentC3
	// DESCRIPTION : Mappe les caractères UTF-8 C3xx vers équivalents ASCII
	// PARAMS : c2 - Second octet d'une séquence UTF-8 C3 (0x80..0xBF)
	// RETURN : Caractère ASCII équivalent, ou '\0' si non mappé
	// NOTE : Utilisé par NkSanitizeLogText pour nettoyage des accents latins
	// -------------------------------------------------------------------------
	char NkMapLatinAccentC3(unsigned char c2) {
		switch (c2) {
			case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85:
				return 'A';

			case 0x87:
				return 'C';

			case 0x88: case 0x89: case 0x8A: case 0x8B:
				return 'E';

			case 0x8C: case 0x8D: case 0x8E: case 0x8F:
				return 'I';

			case 0x91:
				return 'N';

			case 0x92: case 0x93: case 0x94: case 0x95: case 0x96:
				return 'O';

			case 0x99: case 0x9A: case 0x9B: case 0x9C:
				return 'U';

			case 0x9D:
				return 'Y';

			case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5:
				return 'a';

			case 0xA6:
				return 'a';

			case 0xA7:
				return 'c';

			case 0xA8: case 0xA9: case 0xAA: case 0xAB:
				return 'e';

			case 0xAC: case 0xAD: case 0xAE: case 0xAF:
				return 'i';

			case 0xB1:
				return 'n';

			case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6:
				return 'o';

			case 0xB9: case 0xBA: case 0xBB: case 0xBC:
				return 'u';

			case 0xBD: case 0xBF:
				return 'y';

			default:
				return '\0';
		}
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkSanitizeLogText
	// DESCRIPTION : Nettoie une chaîne UTF-8 pour sécurité des outputs de log
	// PARAMS : input - Chaîne source potentiellement contenant des caractères spéciaux
	// RETURN : Nouvelle chaîne avec caractères non-safe remplacés par '?' ou équivalents ASCII
	// NOTE : Préserve les caractères imprimables ASCII, mappe les accents latins, remplace le reste
	// -------------------------------------------------------------------------
	nkentseu::NkString NkSanitizeLogText(const nkentseu::NkString& input) {
		// Cas vide : retour rapide sans allocation
		if (input.Empty()) {
			return nkentseu::NkString{};
		}

		// Pré-allocation : la sortie sera au plus de même taille que l'entrée
		nkentseu::NkString output;
		output.Reserve(input.Length());

		const nkentseu::usize length = input.Length();
		nkentseu::usize index = 0;

		// Parcours octet par octet avec détection des séquences UTF-8 multi-octets
		while (index < length) {
			const unsigned char currentByte = static_cast<unsigned char>(input[index]);

			// Cas 1 : Caractère ASCII imprimable ou whitespace autorisé
			if (currentByte < 0x80) {
				if ((currentByte >= 0x20 && currentByte <= 0x7E)  // Caractères imprimables
					|| currentByte == '\n' || currentByte == '\r' || currentByte == '\t') {  // Whitespace autorisé
					output.PushBack(static_cast<char>(currentByte));
				} else {
					// Caractère de contrôle non autorisé → remplacer par espace
					output.PushBack(' ');
				}
				++index;
				continue;
			}

			// Cas 2 : Séquence UTF-8 à 2 octets commençant par 0xC2
			if (currentByte == 0xC2 && (index + 1) < length) {
				const unsigned char secondByte = static_cast<unsigned char>(input[index + 1]);
				if (secondByte == 0xA0) {
					// NBSP (U+00A0) → espace normal
					output.PushBack(' ');
				} else {
					// Autre caractère C2xx → non mappé, remplacer par ?
					output.PushBack('?');
				}
				index += 2;
				continue;
			}

			// Cas 3 : Séquence UTF-8 à 2 octets commençant par 0xC3 (accents latins)
			if (currentByte == 0xC3 && (index + 1) < length) {
				const unsigned char secondByte = static_cast<unsigned char>(input[index + 1]);
				const char mappedChar = NkMapLatinAccentC3(secondByte);
				output.PushBack(mappedChar ? mappedChar : '?');
				index += 2;
				continue;
			}

			// Cas 4 : Ligatures Œ/œ (U+0152/U+0153) encodées en 0xC5 0x92/0x93
			if (currentByte == 0xC5 && (index + 1) < length) {
				const unsigned char secondByte = static_cast<unsigned char>(input[index + 1]);
				if (secondByte == 0x92) {
					output.Append("OE");  // Œ → "OE"
				} else if (secondByte == 0x93) {
					output.Append("oe");  // œ → "oe"
				} else {
					output.PushBack('?');  // Autre C5xx → ?
				}
				index += 2;
				continue;
			}

			// Cas 5 : Séquences UTF-8 à 3 octets commençant par 0xE2 (ponctuation avancée)
			if (currentByte == 0xE2 && (index + 2) < length) {
				const unsigned char secondByte = static_cast<unsigned char>(input[index + 1]);
				const unsigned char thirdByte = static_cast<unsigned char>(input[index + 2]);

				if (secondByte == 0x80) {
					// Mapping des caractères de ponctuation courants
					switch (thirdByte) {
						case 0x93:  // En dash U+2013
						case 0x94:  // Em dash U+2014
						case 0x95:  // Bullet U+2022
							output.PushBack('-');
							break;

						case 0x98:  // Left single quote U+2018
						case 0x99:  // Right single quote U+2019
							output.PushBack('\'');
							break;

						case 0x9C:  // Left double quote U+201C
						case 0x9D:  // Right double quote U+201D
							output.PushBack('"');
							break;

						case 0xA6:  // Ellipsis U+2026
							output.Append("...");
							break;

						default:
							output.PushBack('?');
							break;
					}
				} else {
					output.PushBack('?');  // E2xx non reconnu
				}
				index += 3;
				continue;
			}

			// Cas 6 : Autres séquences UTF-8 multi-octets → skip et remplacer par ?
			if ((currentByte & 0xE0) == 0xC0 && (index + 1) < length) {
				// Séquence 2 octets non gérée ci-dessus
				index += 2;
			} else if ((currentByte & 0xF0) == 0xE0 && (index + 2) < length) {
				// Séquence 3 octets non gérée ci-dessus
				index += 3;
			} else if ((currentByte & 0xF8) == 0xF0 && (index + 3) < length) {
				// Séquence 4 octets (emoji, etc.)
				index += 4;
			} else {
				// Octet invalide ou séquence tronquée
				++index;
			}

			// Remplacement par ? pour tout caractère non préservé
			output.PushBack('?');
		}

		return output;
	}


	// -------------------------------------------------------------------------
	// FONCTION : NkSanitizeLogText (surcharge const char*)
	// DESCRIPTION : Surcharge pour acceptation directe de C-string
	// PARAMS : input - Pointeur vers chaîne C (peut être nullptr)
	// RETURN : NkString sanitizé, ou chaîne vide si input est nullptr
	// NOTE : Délègue à la version NkString& pour implémentation unique
	// -------------------------------------------------------------------------
	nkentseu::NkString NkSanitizeLogText(const char* input) {
		return input ? NkSanitizeLogText(nkentseu::NkString(input)) : nkentseu::NkString{};
	}


} // namespace anonymous


// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques et privées de NkLogger.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur
	// DESCRIPTION : Initialise un logger avec nom et configuration par défaut
	// -------------------------------------------------------------------------
	NkLogger::NkLogger(const NkString& name)
		: m_Name(name)
		, m_Level(NkLogLevel::NK_INFO)
		, m_Enabled(true)
		, m_Formatter(memory::NkMakeUnique<NkFormatter>())
		, m_SourceLine(0) {

		// Initialisation des métadonnées de source à vide
		m_SourceFile.Clear();
		m_FunctionName.Clear();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Destructeur
	// DESCRIPTION : Flush et cleanup des ressources avant destruction
	// -------------------------------------------------------------------------
	NkLogger::~NkLogger() {
		// Flush garanti : persistance des logs en buffer avant destruction
		Flush();

		// Libération explicite des sinks (comptage de références)
		ClearSinks();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : AddSink
	// DESCRIPTION : Ajoute un sink à la liste de destinations thread-safe
	// -------------------------------------------------------------------------
	void NkLogger::AddSink(memory::NkSharedPtr<NkISink> sink) {
		// Acquisition du lock pour modification de m_Sinks
		threading::NkScopedLockMutex lock(m_Mutex);

		// Ajout en fin de vecteur : ordre d'émission préservé
		m_Sinks.PushBack(sink);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : ClearSinks
	// DESCRIPTION : Vide la liste de sinks thread-safe
	// -------------------------------------------------------------------------
	void NkLogger::ClearSinks() {
		// Acquisition du lock pour modification de m_Sinks
		threading::NkScopedLockMutex lock(m_Mutex);

		// Clear du vecteur : les NkSharedPtr décrémentent leur refcount
		m_Sinks.Clear();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetSinkCount
	// DESCRIPTION : Retourne le nombre de sinks attachés (lecture thread-safe)
	// -------------------------------------------------------------------------
	usize NkLogger::GetSinkCount() const {
		// Acquisition du lock pour lecture protégée
		threading::NkScopedLockMutex lock(m_Mutex);

		// Retour de la taille courante du vecteur
		return m_Sinks.Size();
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetFormatter
	// DESCRIPTION : Définit le formatter principal avec transfert de propriété
	// -------------------------------------------------------------------------
	void NkLogger::SetFormatter(memory::NkUniquePtr<NkFormatter> formatter) {
		// Acquisition du lock pour modification de m_Formatter
		threading::NkScopedLockMutex lock(m_Mutex);

		// Transfert de propriété via move : l'ancien formatter est détruit
		m_Formatter = traits::NkMove(formatter);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetPattern
	// DESCRIPTION : Met à jour le pattern via le formatter interne
	// -------------------------------------------------------------------------
	void NkLogger::SetPattern(const NkString& pattern) {
		// Acquisition du lock pour modification de m_Formatter
		threading::NkScopedLockMutex lock(m_Mutex);

		// Mise à jour du pattern si formatter configuré
		if (m_Formatter) {
			m_Formatter->SetPattern(pattern);
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetLevel
	// DESCRIPTION : Définit le niveau minimum de log pour filtrage
	// -------------------------------------------------------------------------
	void NkLogger::SetLevel(NkLogLevel level) {
		// Acquisition du lock pour modification de m_Level
		threading::NkScopedLockMutex lock(m_Mutex);

		// Mise à jour du seuil de filtrage
		m_Level = level;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetLevel
	// DESCRIPTION : Retourne le niveau minimum configuré (lecture thread-safe)
	// -------------------------------------------------------------------------
	NkLogLevel NkLogger::GetLevel() const {
		// Acquisition du lock pour lecture protégée
		threading::NkScopedLockMutex lock(m_Mutex);

		// Retour de la valeur courante
		return m_Level;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : ShouldLog
	// DESCRIPTION : Vérifie si un niveau passe le filtrage (activation + niveau)
	// -------------------------------------------------------------------------
	bool NkLogger::ShouldLog(NkLogLevel level) const {
		// Filtrage par activation globale du logger
		if (!m_Enabled) {
			return false;
		}

		// Filtrage par niveau : comparaison d'enum ordonnée
		return level >= m_Level;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : LogInternal (privée)
	// DESCRIPTION : Point d'entrée unique pour tous les styles de logging
	// -------------------------------------------------------------------------
	void NkLogger::LogInternal(
		NkLogLevel level,
		const NkString& message,
		const char* sourceFile,
		uint32 sourceLine,
		const char* functionName
	) {
		// Filtrage précoce : éviter tout travail si message ignoré
		if (!m_Enabled || level < m_Level) {
			return;
		}

		// Construction du message de log avec métadonnées
		NkLogMessage logMessage;

		// ID du thread courant via NKThreading
		logMessage.threadId = static_cast<uint32>(threading::NkThread::GetCurrentThreadId());

		// Niveau et contenu du message (sanitizé pour sécurité)
		logMessage.level = level;
		logMessage.message = NkSanitizeLogText(message);
		logMessage.loggerName = NkSanitizeLogText(m_Name);

		// Métadonnées de source optionnelles
		if (sourceFile) {
			logMessage.sourceFile = NkSanitizeLogText(sourceFile);
		}
		if (sourceLine > 0) {
			logMessage.sourceLine = sourceLine;
		}
		if (functionName) {
			logMessage.functionName = NkSanitizeLogText(functionName);
		}

		// Nom du thread si disponible (platform-specific)
		#if defined(__APPLE__) || (defined(__linux__) && !defined(__ANDROID__))
			char threadNameBuffer[256] = {0};
			if (pthread_getname_np(pthread_self(), threadNameBuffer, sizeof(threadNameBuffer)) == 0) {
				logMessage.threadName = NkSanitizeLogText(threadNameBuffer);
			}
		#endif

		// Acquisition du lock pour émission thread-safe vers les sinks
		threading::NkScopedLockMutex lock(m_Mutex);

		// Émission à chaque sink configuré
		for (auto& sink : m_Sinks) {
			if (sink) {
				// Propagation du pattern du logger vers le sink si nécessaire
				NkFormatter* sinkFormatter = sink->GetFormatter();
				if (sinkFormatter && m_Formatter) {
					sinkFormatter->SetPattern(m_Formatter->GetPattern());
				}

				// Appel de Log() sur le sink : filtrage et formatage appliqués côté sink
				sink->Log(logMessage);
			}
		}

		// Réinitialisation des métadonnées de source temporaires après usage
		m_SourceFile.Clear();
		m_FunctionName.Clear();
		m_SourceLine = 0;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : FormatString (privée)
	// DESCRIPTION : Formatage variadique style printf via NkFormatf
	// -------------------------------------------------------------------------
	NkString NkLogger::FormatString(const char* format, va_list args) {
		// Délégation directe à NkFormatf de NKContainers : cohérence et réutilisation
		// NkFormatf gère déjà vsnprintf, allocation, et erreurs
		return NkFormatf(format, args);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Logf (variadique simple)
	// DESCRIPTION : Log printf-style avec filtrage et formatage via NkFormatf
	// -------------------------------------------------------------------------
	void NkLogger::Logf(NkLogLevel level, const char* format, ...) {
		// Filtrage précoce : éviter formatage si message ignoré
		if (!ShouldLog(level)) {
			return;
		}

		// Empaquetage des arguments variadiques
		va_list args;
		va_start(args, format);

		// Formatage via NkFormatf (délègue à FormatString)
		NkString formattedMessage = FormatString(format, args);

		va_end(args);

		// Émission via LogInternal avec métadonnées de source courantes
		LogInternal(level, formattedMessage, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Logf (avec métadonnées de source explicites)
	// DESCRIPTION : Log printf-style avec source override temporaire
	// -------------------------------------------------------------------------
	void NkLogger::Logf(NkLogLevel level, const char* file, int line, const char* func, const char* format, ...) {
		// Filtrage précoce
		if (!ShouldLog(level)) {
			return;
		}

		// Empaquetage des arguments
		va_list args;
		va_start(args, format);

		// Formatage
		NkString formattedMessage = FormatString(format, args);

		va_end(args);

		// Émission avec métadonnées de source fournies (override)
		LogInternal(level, formattedMessage, file, line, func);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Logf (avec va_list pour chaînage)
	// DESCRIPTION : Log printf-style avec va_list pour wrappers variadiques
	// -------------------------------------------------------------------------
	void NkLogger::Logf(NkLogLevel level, const char* file, int line, const char* func, const char* format, va_list args) {
		// Filtrage précoce
		if (!ShouldLog(level)) {
			return;
		}

		// Formatage direct depuis va_list (pas de va_start/va_end ici)
		NkString formattedMessage = FormatString(format, args);

		// Émission avec métadonnées de source fournies
		LogInternal(level, formattedMessage, file, line, func);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Méthodes *f (Tracef, Debugf, etc.)
	// DESCRIPTION : Wrappers typés vers Logf pour convenance d'usage
	// -------------------------------------------------------------------------
	void NkLogger::Tracef(const char* format, ...) {
		if (!ShouldLog(NkLogLevel::NK_TRACE)) {
			return;
		}
		va_list args;
		va_start(args, format);
		NkString message = FormatString(format, args);
		va_end(args);
		LogInternal(NkLogLevel::NK_TRACE, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Debugf(const char* format, ...) {
		if (!ShouldLog(NkLogLevel::NK_DEBUG)) {
			return;
		}
		va_list args;
		va_start(args, format);
		NkString message = FormatString(format, args);
		va_end(args);
		LogInternal(NkLogLevel::NK_DEBUG, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Infof(const char* format, ...) {
		if (!ShouldLog(NkLogLevel::NK_INFO)) {
			return;
		}
		va_list args;
		va_start(args, format);
		NkString message = FormatString(format, args);
		va_end(args);
		LogInternal(NkLogLevel::NK_INFO, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Warnf(const char* format, ...) {
		if (!ShouldLog(NkLogLevel::NK_WARN)) {
			return;
		}
		va_list args;
		va_start(args, format);
		NkString message = FormatString(format, args);
		va_end(args);
		LogInternal(NkLogLevel::NK_WARN, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Errorf(const char* format, ...) {
		if (!ShouldLog(NkLogLevel::NK_ERROR)) {
			return;
		}
		va_list args;
		va_start(args, format);
		NkString message = FormatString(format, args);
		va_end(args);
		LogInternal(NkLogLevel::NK_ERROR, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Criticalf(const char* format, ...) {
		if (!ShouldLog(NkLogLevel::NK_CRITICAL)) {
			return;
		}
		va_list args;
		va_start(args, format);
		NkString message = FormatString(format, args);
		va_end(args);
		LogInternal(NkLogLevel::NK_CRITICAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Fatalf(const char* format, ...) {
		if (!ShouldLog(NkLogLevel::NK_FATAL)) {
			return;
		}
		va_list args;
		va_start(args, format);
		NkString message = FormatString(format, args);
		va_end(args);
		LogInternal(NkLogLevel::NK_FATAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Méthodes stream-style (Trace, Debug, etc.)
	// DESCRIPTION : Wrappers vers Log pour messages littéraux sans formatage
	// -------------------------------------------------------------------------
	void NkLogger::Log(NkLogLevel level, const NkString& message) {
		if (!ShouldLog(level)) {
			return;
		}
		LogInternal(level, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Trace(const NkString& message) {
		if (!ShouldLog(NkLogLevel::NK_TRACE)) {
			return;
		}
		LogInternal(NkLogLevel::NK_TRACE, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Debug(const NkString& message) {
		if (!ShouldLog(NkLogLevel::NK_DEBUG)) {
			return;
		}
		LogInternal(NkLogLevel::NK_DEBUG, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Info(const NkString& message) {
		if (!ShouldLog(NkLogLevel::NK_INFO)) {
			return;
		}
		LogInternal(NkLogLevel::NK_INFO, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Warn(const NkString& message) {
		if (!ShouldLog(NkLogLevel::NK_WARN)) {
			return;
		}
		LogInternal(NkLogLevel::NK_WARN, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Error(const NkString& message) {
		if (!ShouldLog(NkLogLevel::NK_ERROR)) {
			return;
		}
		LogInternal(NkLogLevel::NK_ERROR, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Critical(const NkString& message) {
		if (!ShouldLog(NkLogLevel::NK_CRITICAL)) {
			return;
		}
		LogInternal(NkLogLevel::NK_CRITICAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}

	void NkLogger::Fatal(const NkString& message) {
		if (!ShouldLog(NkLogLevel::NK_FATAL)) {
			return;
		}
		LogInternal(NkLogLevel::NK_FATAL, message, m_SourceFile.CStr(), m_SourceLine, m_FunctionName.CStr());
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Flush
	// DESCRIPTION : Force l'écriture des buffers de tous les sinks
	// -------------------------------------------------------------------------
	void NkLogger::Flush() {
		// Acquisition du lock pour itération thread-safe sur m_Sinks
		threading::NkScopedLockMutex lock(m_Mutex);

		// Appel de Flush() sur chaque sink valide
		for (auto& sink : m_Sinks) {
			if (sink) {
				sink->Flush();
			}
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetName
	// DESCRIPTION : Retourne le nom du logger (lecture thread-safe)
	// -------------------------------------------------------------------------
	const NkString& NkLogger::GetName() const {
		// Lecture protégée via lock (m_Name est mutable via SetName protégé)
		threading::NkScopedLockMutex lock(m_Mutex);
		return m_Name;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : IsEnabled
	// DESCRIPTION : Vérifie l'état d'activation du logger
	// -------------------------------------------------------------------------
	bool NkLogger::IsEnabled() const {
		// Lecture safe : m_Enabled n'est modifié que via SetEnabled (avec lock)
		return m_Enabled;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetEnabled
	// DESCRIPTION : Active ou désactive le logger thread-safe
	// -------------------------------------------------------------------------
	void NkLogger::SetEnabled(bool enabled) {
		// Acquisition du lock pour modification de m_Enabled
		threading::NkScopedLockMutex lock(m_Mutex);
		m_Enabled = enabled;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Source
	// DESCRIPTION : Configure les métadonnées de source temporaires
	// -------------------------------------------------------------------------
	NkLogger& NkLogger::Source(const char* sourceFile, uint32 sourceLine, const char* functionName) {
		// Acquisition du lock pour modification des métadonnées temporaires
		threading::NkScopedLockMutex lock(m_Mutex);

		// Stockage des valeurs avec gestion des nullptr
		m_SourceFile = sourceFile ? NkSanitizeLogText(sourceFile) : NkString{};
		m_SourceLine = sourceLine;
		m_FunctionName = functionName ? NkSanitizeLogText(functionName) : NkString{};

		// Retour de *this pour chaînage fluide
		return *this;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetName (protégée)
	// DESCRIPTION : Définit le nom du logger pour classes dérivées
	// -------------------------------------------------------------------------
	void NkLogger::SetName(const NkString& name) {
		// Acquisition du lock pour modification de m_Name
		threading::NkScopedLockMutex lock(m_Mutex);
		m_Name = name;
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
	1. SANITIZATION UTF-8 :
	   - NkSanitizeLogText() préserve les caractères imprimables ASCII
	   - Mappe les accents latins courants (é, è, ê, etc.) vers ASCII équivalent
	   - Remplace les caractères non-safe par '?' pour éviter corruption des logs
	   - Gère les séquences UTF-8 multi-octets : 2, 3 et 4 octets

	2. FORMATAGE VIA NKFORMATF :
	   - FormatString() délègue à nkentseu::NkFormatf() pour cohérence projet
	   - Évite la duplication de logique vsnprintf/allocation
	   - Bénéficie des optimisations et corrections de NkFormatf centralisées

	3. THREAD-SAFETY :
	   - Toutes les méthodes publiques acquièrent m_Mutex via NkScopedLockMutex
	   - Les lectures const acquièrent aussi le lock car m_Name est mutable
	   - Les sinks partagés doivent être thread-safe individuellement

	4. GESTION DES MÉTADONNÉES DE SOURCE :
	   - Source() stocke temporairement dans m_SourceFile/Line/FunctionName
	   - LogInternal() consomme ces valeurs et les réinitialise après usage
	   - Pattern fluide : logger.Source(...).Info(...) pour chaînage

	5. PERFORMANCE :
	   - Filtrage précoce dans ShouldLog() évite formatage inutile
	   - Pré-allocation dans NkSanitizeLogText() réduit les réallocations
	   - NkString utilise SSO : petites chaînes sans allocation heap

	6. EXTENSIBILITÉ :
	   - LogInternal() est le point d'extension unique pour logging personnalisé
	   - Les méthodes publiques délèguent toutes à LogInternal()
	   - Pour logging asynchrone : surcharger LogInternal() avec queue de messages

	7. COMPATIBILITÉ MULTIPLATEFORME :
	   - pthread_getname_np() disponible sur Linux/macOS, pas sur Windows/Android
	   - NkThread::GetCurrentThreadId() abstrait les différences Windows/POSIX
	   - Sanitization gère UTF-8 indépendamment de l'encodage système
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================