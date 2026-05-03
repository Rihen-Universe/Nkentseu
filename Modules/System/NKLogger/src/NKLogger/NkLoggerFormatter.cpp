// =============================================================================
// NKLogger/NkLoggerFormatter.cpp
// Implémentation du formateur de messages de log basé sur des patterns.
//
// Design :
//  - Inclusion de pch.h en premier pour compilation précompilée
//  - Aucune macro NKENTSEU_LOGGER_API sur les méthodes (héritée de la classe)
//  - Fonctions internes dans namespace anonyme pour encapsulation
//  - Réutilisation de NkLogLevel pour conversion niveau → texte/couleur
//  - Implémentations déterministes sans allocation dynamique imprévue
//  - Namespace unique : nkentseu (pas de sous-namespace logger)
//
// Auteur : TEUGUIA TADJUIDJE Rodolf / Rihen
// Date : 2024-2026
// License : Proprietary - Free to use and modify
// =============================================================================

#include "pch.h"

#include "NKLogger/NkLoggerFormatter.h"
#include "NKLogger/NkLogLevel.h"
#include "NKLogger/NkLogMessage.h"

#include <cstdio>
#include <ctime>


// -------------------------------------------------------------------------
// SECTION 1 : NAMESPACE ANONYME - CONSTANTES ET UTILITAIRES INTERNES
// -------------------------------------------------------------------------
// Déclarations à usage interne uniquement.
// Non exposées dans l'API publique pour encapsulation et optimisation.

namespace {


	// -------------------------------------------------------------------------
	// FONCTION : NkAppendUInt32
	// DESCRIPTION : Append un entier non-signé 32 bits à une chaîne NkString
	// PARAMS : result - Chaîne de destination à laquelle appendre
	//          value - Valeur entière à convertir et ajouter
	// NOTE : Utilise snprintf pour conversion portable, évite les dépendances STL
	// -------------------------------------------------------------------------
	inline void NkAppendUInt32(nkentseu::NkString& result, nkentseu::uint32 value) {
		char buffer[16];  // Suffisant pour uint32 max (4294967295) + null terminator

		const int length = ::snprintf(
			buffer,
			sizeof(buffer),
			"%u",
			static_cast<unsigned int>(value)
		);

		if (length > 0) {
			result.Append(buffer, static_cast<nkentseu::usize>(length));
		}
	}


} // namespace anonymous


// -------------------------------------------------------------------------
// SECTION 2 : NAMESPACE PRINCIPAL - DÉFINITIONS DES CONSTANTES STATIQUES
// -------------------------------------------------------------------------
// Les patterns prédéfinis sont définis ici pour éviter la duplication
// et permettre une référence unique dans tout le codebase.

namespace nkentseu {


	// Pattern par défaut : équilibre entre lisibilité et informations de debug
	const char* NkLoggerFormatter::NK_DEFAULT_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v";

	// Pattern minimal : uniquement le message, pour logs compacts ou production
	const char* NkLoggerFormatter::NK_SIMPLE_PATTERN = "%v";

	// Pattern détaillé : toutes les métadonnées pour debugging avancé
	const char* NkLoggerFormatter::NK_DETAILED_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [thread %t] [%s:%# in %f] -> %v";

	// Pattern NKENTSEU : avec support des couleurs via %^ et %$ autour du niveau
	const char* NkLoggerFormatter::NK_NKENTSEU_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] [%s:%# in %F] -> %v";

	// Pattern console : couleurs uniquement autour du niveau de log
	const char* NkLoggerFormatter::NK_COLOR_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] [%t] -> %v";

	// Pattern JSON : format structuré pour ingestion dans systèmes de métriques
	// Note : les champs sont échappés pour validité JSON stricte
	const char* NkLoggerFormatter::NK_JSON_PATTERN =
		R"({"time":"%Y-%m-%dT%H:%M:%S.%fZ","level":"%l","thread":%t,"logger":"%n","file":"%s","line":%#,"function":"%f","message":"%v"})";

	// Pattern court pour production : "12:34:56 INF Message"
	const char* NkLoggerFormatter::NK_SHORT_PATTERN = "%H:%M:%S %L %v";

	// Pattern ISO 8601 : format timestamp international
	const char* NkLoggerFormatter::NK_ISO8601_PATTERN = "%Y-%m-%dT%H:%M:%S.%fZ [%L] %v";

	// Pattern syslog : format compatible avec les logs système Unix
	const char* NkLoggerFormatter::NK_SYSLOG_PATTERN = "%b %d %H:%M:%S %h %n[%t]: %v";


} // namespace nkentseu


// -------------------------------------------------------------------------
// SECTION 3 : NAMESPACE PRINCIPAL - IMPLÉMENTATIONS DES MÉTHODES
// -------------------------------------------------------------------------
// Implémentation des méthodes publiques et protégées de NkLoggerFormatter.
// Aucune macro NKENTSEU_LOGGER_API : export hérité de la déclaration de classe.

namespace nkentseu {


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur par défaut
	// DESCRIPTION : Initialise avec NK_DEFAULT_PATTERN et parse les tokens
	// -------------------------------------------------------------------------
	NkLoggerFormatter::NkLoggerFormatter()
		: m_Pattern(NK_DEFAULT_PATTERN)
		, m_TokensValid(false) {

		// Parsing immédiat du pattern par défaut pour prêt-à-l'emploi
		ParsePattern(m_Pattern);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Constructeur avec pattern personnalisé
	// DESCRIPTION : Initialise avec le pattern fourni et parse les tokens
	// -------------------------------------------------------------------------
	NkLoggerFormatter::NkLoggerFormatter(const NkString& pattern)
		: m_Pattern(pattern)
		, m_TokensValid(false) {

		// Parsing immédiat du pattern personnalisé
		ParsePattern(m_Pattern);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : SetPattern
	// DESCRIPTION : Met à jour le pattern et invalide le cache de tokens
	// -------------------------------------------------------------------------
	void NkLoggerFormatter::SetPattern(const NkString& pattern) {
		// Mise à jour uniquement si changement pour éviter reparsing inutile
		if (m_Pattern != pattern) {
			m_Pattern = pattern;
			m_TokensValid = false;  // Invalidation du cache
			ParsePattern(m_Pattern);  // Reparsing avec nouveau pattern
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetPattern
	// DESCRIPTION : Retourne le pattern courant par référence constante
	// -------------------------------------------------------------------------
	const NkString& NkLoggerFormatter::GetPattern() const {
		// Retour direct : pas de copie, lecture efficace
		return m_Pattern;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Format (sans couleurs)
	// DESCRIPTION : Formate un message avec le pattern courant, sans ANSI
	// -------------------------------------------------------------------------
	NkString NkLoggerFormatter::Format(const NkLogMessage& message) {
		// Délégation à la version avec paramètre couleurs, désactivé
		return Format(message, false);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : Format (avec option couleurs)
	// DESCRIPTION : Formate un message en appliquant le pattern et les couleurs
	// -------------------------------------------------------------------------
	NkString NkLoggerFormatter::Format(const NkLogMessage& message, bool useColors) {
		// Regénération des tokens si invalides (premier appel ou après SetPattern)
		if (!m_TokensValid) {
			ParsePattern(m_Pattern);
		}

		// Pré-allocation estimée pour éviter réallocation pendant l'append
		NkString result;
		result.Reserve(256);  // Ajustable selon patterns typiques

		// Parcours séquentiel des tokens : chaque token contribue à la sortie
		for (const auto& token : m_Tokens) {
			FormatToken(token, message, useColors, result);
		}

		// Retour de la chaîne complètement formatée
		return result;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : ParsePattern (privée)
	// DESCRIPTION : Convertit une chaîne de pattern en vecteur de tokens
	// -------------------------------------------------------------------------
	void NkLoggerFormatter::ParsePattern(const NkString& pattern) {
		// Réinitialisation du cache de tokens
		m_Tokens.Clear();

		// Estimation de capacité : ~1 token tous les 2 caractères en moyenne
		m_Tokens.Reserve(pattern.Length() / 2);

		// Parcours caractère par caractère du pattern
		for (usize i = 0; i < pattern.Length(); ++i) {
			// Détection d'un placeholder : caractère % suivi d'un code
			if (pattern[i] == '%' && i + 1 < pattern.Length()) {
				const char next = pattern[i + 1];
				NkPatternToken token;

				// Mapping du code caractère vers le type de token
				switch (next) {
					case 'Y':
						token.type = NkPatternToken::Type::NK_YEAR;
						break;

					case 'm':
						token.type = NkPatternToken::Type::NK_MONTH;
						break;

					case 'd':
						token.type = NkPatternToken::Type::NK_DAY;
						break;

					case 'H':
						token.type = NkPatternToken::Type::NK_HOUR;
						break;

					case 'M':
						token.type = NkPatternToken::Type::NK_MINUTE;
						break;

					case 'S':
						token.type = NkPatternToken::Type::NK_SECOND;
						break;

					case 'e':
						token.type = NkPatternToken::Type::NK_MILLIS;
						break;

					case 'f':
						token.type = NkPatternToken::Type::NK_MICROS;
						break;

					case 'l':
						token.type = NkPatternToken::Type::NK_LEVEL;
						break;

					case 'L':
						token.type = NkPatternToken::Type::NK_LEVEL_SHORT;
						break;

					case 't':
						token.type = NkPatternToken::Type::NK_THREAD_ID;
						break;

					case 'T':
						token.type = NkPatternToken::Type::NK_THREAD_NAME;
						break;

					case 's':
						token.type = NkPatternToken::Type::NK_SOURCE_FILE;
						break;

					case '#':
						token.type = NkPatternToken::Type::NK_SOURCE_LINE;
						break;

					case 'F':
						token.type = NkPatternToken::Type::NK_FUNCTION;
						break;

					case 'v':
						token.type = NkPatternToken::Type::NK_MESSAGE;
						break;

					case 'n':
						token.type = NkPatternToken::Type::NK_LOGGER_NAME;
						break;

					case '%':
						token.type = NkPatternToken::Type::NK_PERCENT;
						break;

					case '^':
						token.type = NkPatternToken::Type::NK_COLOR_START;
						break;

					case '$':
						token.type = NkPatternToken::Type::NK_COLOR_END;
						break;

					default:
						// Code inconnu : traiter % + caractère comme littéral
						token.type = NkPatternToken::Type::NK_LITERAL;
						token.value = pattern.SubStr(i, 2);
						m_Tokens.PushBack(token);
						++i;  // Skip le caractère suivant déjà consommé
						continue;
				}

				// Token reconnu : ajout au vecteur sans valeur associée
				m_Tokens.PushBack(token);
				++i;  // Skip le code caractère déjà traité

			} else {
				// Texte littéral : accumuler jusqu'au prochain % ou fin de chaîne
				const usize start = i;

				while (i < pattern.Length() && pattern[i] != '%') {
					++i;
				}

				// Création du token littéral avec le texte accumulé
				NkPatternToken token;
				token.type = NkPatternToken::Type::NK_LITERAL;
				token.value = pattern.SubStr(start, i - start);
				m_Tokens.PushBack(token);

				// Ajustement de l'index pour la boucle for (qui fera ++i)
				--i;
			}
		}

		// Marquer les tokens comme valides pour les prochains appels Format()
		m_TokensValid = true;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : FormatToken (protégée)
	// DESCRIPTION : Applique un token individuel au message en construction
	// -------------------------------------------------------------------------
	void NkLoggerFormatter::FormatToken(
		const NkPatternToken& token,
		const NkLogMessage& message,
		bool useColors,
		NkString& result
	) {
		// Dispatch par type de token : chaque cas extrait la valeur appropriée
		switch (token.type) {
			case NkPatternToken::Type::NK_LITERAL:
				// Texte brut : copier tel quel dans la sortie
				result += token.value;
				break;

			case NkPatternToken::Type::NK_YEAR: {
				// Année sur 4 chiffres depuis la structure tm locale
				const tm timeInfo = message.GetLocalTime();
				result += FormatNumber(timeInfo.tm_year + 1900, 4);
				break;
			}

			case NkPatternToken::Type::NK_MONTH: {
				// Mois sur 2 chiffres (tm_mon est 0-based : janvier = 0)
				const tm timeInfo = message.GetLocalTime();
				result += FormatNumber(timeInfo.tm_mon + 1, 2);
				break;
			}

			case NkPatternToken::Type::NK_DAY: {
				// Jour du mois sur 2 chiffres
				const tm timeInfo = message.GetLocalTime();
				result += FormatNumber(timeInfo.tm_mday, 2);
				break;
			}

			case NkPatternToken::Type::NK_HOUR: {
				// Heure (0-23) sur 2 chiffres
				const tm timeInfo = message.GetLocalTime();
				result += FormatNumber(timeInfo.tm_hour, 2);
				break;
			}

			case NkPatternToken::Type::NK_MINUTE: {
				// Minute (0-59) sur 2 chiffres
				const tm timeInfo = message.GetLocalTime();
				result += FormatNumber(timeInfo.tm_min, 2);
				break;
			}

			case NkPatternToken::Type::NK_SECOND: {
				// Seconde (0-59) sur 2 chiffres
				const tm timeInfo = message.GetLocalTime();
				result += FormatNumber(timeInfo.tm_sec, 2);
				break;
			}

			case NkPatternToken::Type::NK_MILLIS: {
				// Millisecondes : extraire les 3 derniers chiffres du timestamp ms
				const uint64 millis = message.GetMillis() % 1000;
				result += FormatNumber(static_cast<int>(millis), 3);
				break;
			}

			case NkPatternToken::Type::NK_MICROS: {
				// Microsecondes : extraire les 6 derniers chiffres du timestamp µs
				const uint64 micros = message.GetMicros() % 1000000;
				result += FormatNumber(static_cast<int>(micros), 6);
				break;
			}

			case NkPatternToken::Type::NK_LEVEL:
				// Niveau de log en texte complet : "info", "error", etc.
				result += NkLogLevelToString(message.level);
				break;

			case NkPatternToken::Type::NK_LEVEL_SHORT:
				// Niveau de log en code court : "INF", "ERR", etc.
				result += NkLogLevelToShortString(message.level);
				break;

			case NkPatternToken::Type::NK_THREAD_ID:
				// ID numérique du thread émetteur
				NkAppendUInt32(result, message.threadId);
				break;

			case NkPatternToken::Type::NK_THREAD_NAME:
				// Nom lisible du thread, ou fallback sur ID si non défini
				if (!message.threadName.Empty()) {
					result += message.threadName;
				} else {
					NkAppendUInt32(result, message.threadId);
				}
				break;

			case NkPatternToken::Type::NK_SOURCE_FILE:
				// Chemin du fichier source : extraire uniquement le nom de fichier
				if (!message.sourceFile.Empty()) {
					// Recherche du dernier séparateur de chemin (Unix ou Windows)
					const usize pos = message.sourceFile.FindLastOf("/\\");

					if (pos != NkString::npos) {
						// Extraire le nom après le dernier séparateur
						result += message.sourceFile.SubStr(pos + 1);
					} else {
						// Pas de séparateur : utiliser le chemin tel quel
						result += message.sourceFile;
					}
				}
				break;

			case NkPatternToken::Type::NK_SOURCE_LINE:
				// Numéro de ligne source : afficher uniquement si > 0
				if (message.sourceLine > 0) {
					NkAppendUInt32(result, message.sourceLine);
				}
				break;

			case NkPatternToken::Type::NK_FUNCTION:
				// Nom de la fonction émettrice : afficher si disponible
				if (!message.functionName.Empty()) {
					result += message.functionName;
				}
				break;

			case NkPatternToken::Type::NK_MESSAGE:
				// Contenu principal du message de log
				result += message.message;
				break;

			case NkPatternToken::Type::NK_LOGGER_NAME:
				// Nom hiérarchique du logger, ou fallback "default"
				if (!message.loggerName.Empty()) {
					result += message.loggerName;
				} else {
					result += "default";
				}
				break;

			case NkPatternToken::Type::NK_PERCENT:
				// Caractère % littéral : échappement via %%
				result += '%';
				break;

			case NkPatternToken::Type::NK_COLOR_START:
				// Début de zone colorée : ajouter code ANSI si couleurs activées
				if (useColors) {
					result += GetANSIColor(message.level);
				}
				break;

			case NkPatternToken::Type::NK_COLOR_END:
				// Fin de zone colorée : ajouter code reset si couleurs activées
				if (useColors) {
					result += GetANSIReset();
				}
				break;

			default:
				// Type inconnu : ignorer silencieusement (robustesse)
				break;
		}
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : FormatNumber (privée)
	// DESCRIPTION : Formate un entier avec padding et gestion du signe
	// -------------------------------------------------------------------------
	NkString NkLoggerFormatter::FormatNumber(int value, int width, char fillChar) const {
		// Buffer temporaire pour la conversion snprintf
		char buffer[32];  // Suffisant pour int32 + signe + null

		const int length = ::snprintf(buffer, sizeof(buffer), "%d", value);

		// Gestion d'erreur snprintf : retourner chaîne vide
		if (length <= 0) {
			return {};
		}

		// Cas sans padding nécessaire : retour direct
		if (width <= length) {
			return NkString(buffer, static_cast<usize>(length));
		}

		// Calcul du nombre de caractères de remplissage nécessaires
		const int pad = width - length;

		// Pré-allocation de la chaîne de résultat
		NkString out;
		out.Reserve(static_cast<usize>(width));

		// Gestion spéciale : nombre négatif avec padding zéro
		// Le signe doit précéder les zéros : -005 et non 0-05
		if (fillChar == '0' && value < 0) {
			out.PushBack('-');                    // Signe en premier
			out.Append(static_cast<usize>(pad), fillChar);  // Zéros de padding
			out.Append(buffer + 1, static_cast<usize>(length - 1));  // Chiffres sans signe
			return out;
		}

		// Cas standard : padding puis valeur
		out.Append(static_cast<usize>(pad), fillChar);
		out.Append(buffer, static_cast<usize>(length));

		return out;
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetANSIColor (privée)
	// DESCRIPTION : Délègue à NkLogLevelToANSIColor pour cohérence globale
	// -------------------------------------------------------------------------
	NkString NkLoggerFormatter::GetANSIColor(NkLogLevel level) const {
		// Réutilisation de la fonction utilitaire centralisée
		// Évite la duplication et garantit la cohérence des couleurs
		return NkLogLevelToANSIColor(level);
	}


	// -------------------------------------------------------------------------
	// MÉTHODE : GetANSIReset (privée)
	// DESCRIPTION : Retourne la séquence ANSI de reset des attributs console
	// -------------------------------------------------------------------------
	NkString NkLoggerFormatter::GetANSIReset() const {
		// Code ANSI standard : reset tous les attributs (couleur, gras, etc.)
		return "\033[0m";
	}


} // namespace nkentseu


// =============================================================================
// NOTES D'IMPLÉMENTATION ET OPTIMISATIONS
// =============================================================================
/*
	1. PARSING DES PATTERNS :
	   - Algorithme O(n) avec n = longueur du pattern
	   - Un seul parsing par pattern : tokens mis en cache pour Format() O(1)
	   - Gestion robuste des codes inconnus : fallback vers littéral

	2. GESTION MÉMOIRE :
	   - Pré-allocation avec Reserve() pour éviter réallocation en boucle
	   - NkString utilise SSO : petites chaînes sans allocation heap
	   - Clear() + réutilisation du NkString de sortie pour logging intensif

	3. THREAD-SAFETY :
	   - ParsePattern() modifie l'état interne : appeler avant tout logging concurrent
	   - Format() en lecture seule sur les tokens : safe pour appels concurrents
	   - Les NkString retournés sont des copies : pas de partage d'état

	4. COMPATIBILITÉ MULTIPLATEFORME :
	   - Codes ANSI : standards pour terminaux Unix/Linux/macOS
	   - Windows : le backend console doit convertir ANSI → Win32 SetConsoleTextAttribute
	   - Envisager NkConsoleHelper pour abstraction des séquences de couleur

	5. EXTENSIBILITÉ :
	   - FormatToken() protégée : surchargeable pour formatage personnalisé
	   - Ajout de tokens : étendre NkPatternToken::Type + switch dans Parse/Format
	   - Patterns JSON : échappement des guillemets à gérer dans le message si nécessaire
*/


// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// ============================================================