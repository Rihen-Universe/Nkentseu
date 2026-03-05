// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkFormatter.cpp
// DESCRIPTION: Implémentation du formatter de messages de log.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkFormatter.h"
#include "NKLogger/NkLogLevel.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <ctime>

// -----------------------------------------------------------------------------
// PATTERNS PRÉDÉFINIS
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */

const char *NkFormatter::NK_DEFAULT_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v";
const char *NkFormatter::NK_SIMPLE_PATTERN = "%v";
const char *NkFormatter::NK_DETAILED_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [thread %t] [%s:%# in %f] -> %v";
const char *NkFormatter::NK_COLOR_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] [%t] -> %v";
const char *NkFormatter::NK_JSON_PATTERN =
	R"({"time":"%Y-%m-%dT%H:%M:%S.%fZ","level":"%l","thread":%t,"logger":"%n","file":"%s","line":%#,"function":"%f","message":"%v"})";

} // namespace nkentseu

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */

// -------------------------------------------------------------------------
// IMPLÉMENTATION DE NkFormatter
// -------------------------------------------------------------------------

/**
 * @brief Constructeur par défaut
 */
NkFormatter::NkFormatter() : m_Pattern(NK_DEFAULT_PATTERN), m_TokensValid(false) {
}

/**
 * @brief Constructeur avec pattern spécifique
 */
NkFormatter::NkFormatter(const std::string &pattern) : m_Pattern(pattern), m_TokensValid(false) {
	ParsePattern(pattern);
}

/**
 * @brief Destructeur
 */
NkFormatter::~NkFormatter() {
}

/**
 * @brief Définit le pattern de formatage
 */
void NkFormatter::SetPattern(const std::string &pattern) {
	if (m_Pattern != pattern) {
		m_Pattern = pattern;
		m_TokensValid = false;
	}
}

/**
 * @brief Obtient le pattern courant
 */
const std::string &NkFormatter::GetPattern() const {
	return m_Pattern;
}

/**
 * @brief Formate un message de log
 */
std::string NkFormatter::Format(const NkLogMessage &message) {
	return Format(message, false);
}

/**
 * @brief Formate un message de log avec des couleurs
 */
std::string NkFormatter::Format(const NkLogMessage &message, bool useColors) {
	if (!m_TokensValid) {
		ParsePattern(m_Pattern);
	}

	std::string result;
	result.reserve(256); // Pré-allocation pour performance

	for (const auto &token : m_Tokens) {
		FormatToken(token, message, useColors, result);
	}

	return result;
}

/**
 * @brief Parse le pattern en tokens
 */
void NkFormatter::ParsePattern(const std::string &pattern) {
	m_Tokens.clear();
	m_Tokens.reserve(pattern.size() / 2); // Estimation

	for (core::usize i = 0; i < pattern.size(); ++i) {
		if (pattern[i] == '%' && i + 1 < pattern.size()) {
			char next = pattern[i + 1];
			NkPatternToken token;

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
					// Token littéral avec le %
					token.type = NkPatternToken::Type::NK_LITERAL;
					token.value = pattern.substr(i, 2);
					m_Tokens.push_back(token);
					++i;
					continue;
			}

			m_Tokens.push_back(token);
			++i; // Skip le caractère suivant
		} else {
			// Token littéral
			core::usize start = i;
			while (i < pattern.size() && pattern[i] != '%') {
				++i;
			}

			NkPatternToken token;
			token.type = NkPatternToken::Type::NK_LITERAL;
			token.value = pattern.substr(start, i - start);
			m_Tokens.push_back(token);

			--i; // Pour que la boucle incrémente correctement
		}
	}

	m_TokensValid = true;
}

/**
 * @brief Formate un token individuel
 */
void NkFormatter::FormatToken(const NkPatternToken &token, const NkLogMessage &message, bool useColors, std::string &result) {
	switch (token.type) {
		case NkPatternToken::Type::NK_LITERAL:
			result += token.value;
			break;

		case NkPatternToken::Type::NK_YEAR: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_year + 1900, 4);
			break;
		}

		case NkPatternToken::Type::NK_MONTH: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_mon + 1, 2);
			break;
		}

		case NkPatternToken::Type::NK_DAY: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_mday, 2);
			break;
		}

		case NkPatternToken::Type::NK_HOUR: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_hour, 2);
			break;
		}

		case NkPatternToken::Type::NK_MINUTE: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_min, 2);
			break;
		}

		case NkPatternToken::Type::NK_SECOND: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_sec, 2);
			break;
		}

		case NkPatternToken::Type::NK_MILLIS: {
			uint64 millis = message.GetMillis() % 1000;
			result += FormatNumber(static_cast<int>(millis), 3);
			break;
		}

		case NkPatternToken::Type::NK_MICROS: {
			uint64 micros = message.GetMicros() % 1000000;
			result += FormatNumber(static_cast<int>(micros), 6);
			break;
		}

		case NkPatternToken::Type::NK_LEVEL:
			result += NkLogLevelToString(message.level);
			break;

		case NkPatternToken::Type::NK_LEVEL_SHORT:
			result += NkLogLevelToShortString(message.level);
			break;

		case NkPatternToken::Type::NK_THREAD_ID: {
			std::ostringstream oss;
			oss << message.threadId;
			result += oss.str();
			break;
		}

		case NkPatternToken::Type::NK_THREAD_NAME:
			if (!message.threadName.empty()) {
				result += message.threadName;
			} else {
				std::ostringstream oss;
				oss << message.threadId;
				result += oss.str();
			}
			break;

		case NkPatternToken::Type::NK_SOURCE_FILE:
			if (!message.sourceFile.empty()) {
				// Extraire juste le nom du fichier (sans chemin)
				core::usize pos = message.sourceFile.find_last_of("/\\");
				if (pos != std::string::npos) {
					result += message.sourceFile.substr(pos + 1);
				} else {
					result += message.sourceFile;
				}
			}
			break;

		case NkPatternToken::Type::NK_SOURCE_LINE: {
			if (message.sourceLine > 0) {
				std::ostringstream oss;
				oss << message.sourceLine;
				result += oss.str();
			}
			break;
		}

		case NkPatternToken::Type::NK_FUNCTION:
			if (!message.functionName.empty()) {
				result += message.functionName;
			}
			break;

		case NkPatternToken::Type::NK_MESSAGE:
			result += message.message;
			break;

		case NkPatternToken::Type::NK_LOGGER_NAME:
			if (!message.loggerName.empty()) {
				result += message.loggerName;
			} else {
				result += "default";
			}
			break;

		case NkPatternToken::Type::NK_PERCENT:
			result += '%';
			break;

		case NkPatternToken::Type::NK_COLOR_START:
			if (useColors) {
				result += GetANSIColor(message.level);
			}
			break;

		case NkPatternToken::Type::NK_COLOR_END:
			if (useColors) {
				result += GetANSIReset();
			}
			break;
	}
}

/**
 * @brief Formate un nombre avec padding
 */
std::string NkFormatter::FormatNumber(int value, int width, char fillChar) const {
	std::ostringstream oss;
	oss << std::setw(width) << std::setfill(fillChar) << value;
	return oss.str();
}

/**
 * @brief Obtient le code couleur ANSI pour un niveau de log
 */
std::string NkFormatter::GetANSIColor(NkLogLevel level) const {
	return NkLogLevelToANSIColor(level);
}

/**
 * @brief Obtient le code de fin de couleur ANSI
 */
std::string NkFormatter::GetANSIReset() const {
	return "\033[0m";
}

} // namespace nkentseu