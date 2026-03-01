// -----------------------------------------------------------------------------
// FICHIER: Core/Logger/src/Logger/Formatter.cpp
// DESCRIPTION: Implémentation du formatter de messages de log.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "Logger/Formatter.h"
#include "Logger/LogLevel.h"
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
namespace logger {

const char *Formatter::DEFAULT_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v";
const char *Formatter::SIMPLE_PATTERN = "%v";
const char *Formatter::DETAILED_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [thread %t] [%s:%# in %f] -> %v";
const char *Formatter::COLOR_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] [%t] -> %v";
const char *Formatter::JSON_PATTERN =
	R"({"time":"%Y-%m-%dT%H:%M:%S.%fZ","level":"%l","thread":%t,"logger":"%n","file":"%s","line":%#,"function":"%f","message":"%v"})";

} // namespace logger
} // namespace nkentseu

// -----------------------------------------------------------------------------
// NAMESPACE: nkentseu::logger
// -----------------------------------------------------------------------------
namespace nkentseu {
/**
 * @brief Namespace logger.
 */
namespace logger {

// -------------------------------------------------------------------------
// IMPLÉMENTATION DE Formatter
// -------------------------------------------------------------------------

/**
 * @brief Constructeur par défaut
 */
Formatter::Formatter() : m_Pattern(DEFAULT_PATTERN), m_TokensValid(false) {
}

/**
 * @brief Constructeur avec pattern spécifique
 */
Formatter::Formatter(const std::string &pattern) : m_Pattern(pattern), m_TokensValid(false) {
	ParsePattern(pattern);
}

/**
 * @brief Destructeur
 */
Formatter::~Formatter() {
}

/**
 * @brief Définit le pattern de formatage
 */
void Formatter::SetPattern(const std::string &pattern) {
	if (m_Pattern != pattern) {
		m_Pattern = pattern;
		m_TokensValid = false;
	}
}

/**
 * @brief Obtient le pattern courant
 */
const std::string &Formatter::GetPattern() const {
	return m_Pattern;
}

/**
 * @brief Formate un message de log
 */
std::string Formatter::Format(const LogMessage &message) {
	return Format(message, false);
}

/**
 * @brief Formate un message de log avec des couleurs
 */
std::string Formatter::Format(const LogMessage &message, bool useColors) {
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
void Formatter::ParsePattern(const std::string &pattern) {
	m_Tokens.clear();
	m_Tokens.reserve(pattern.size() / 2); // Estimation

	for (size_t i = 0; i < pattern.size(); ++i) {
		if (pattern[i] == '%' && i + 1 < pattern.size()) {
			char next = pattern[i + 1];
			PatternToken token;

			switch (next) {
				case 'Y':
					token.type = PatternToken::Type::Year;
					break;
				case 'm':
					token.type = PatternToken::Type::Month;
					break;
				case 'd':
					token.type = PatternToken::Type::Day;
					break;
				case 'H':
					token.type = PatternToken::Type::Hour;
					break;
				case 'M':
					token.type = PatternToken::Type::Minute;
					break;
				case 'S':
					token.type = PatternToken::Type::Second;
					break;
				case 'e':
					token.type = PatternToken::Type::Millis;
					break;
				case 'f':
					token.type = PatternToken::Type::Micros;
					break;
				case 'l':
					token.type = PatternToken::Type::Level;
					break;
				case 'L':
					token.type = PatternToken::Type::LevelShort;
					break;
				case 't':
					token.type = PatternToken::Type::ThreadId;
					break;
				case 'T':
					token.type = PatternToken::Type::ThreadName;
					break;
				case 's':
					token.type = PatternToken::Type::SourceFile;
					break;
				case '#':
					token.type = PatternToken::Type::SourceLine;
					break;
				case 'F':
					token.type = PatternToken::Type::Function;
					break;
				case 'v':
					token.type = PatternToken::Type::Message;
					break;
				case 'n':
					token.type = PatternToken::Type::LoggerName;
					break;
				case '%':
					token.type = PatternToken::Type::Percent;
					break;
				case '^':
					token.type = PatternToken::Type::ColorStart;
					break;
				case '$':
					token.type = PatternToken::Type::ColorEnd;
					break;
				default:
					// Token littéral avec le %
					token.type = PatternToken::Type::Literal;
					token.value = pattern.substr(i, 2);
					m_Tokens.push_back(token);
					++i;
					continue;
			}

			m_Tokens.push_back(token);
			++i; // Skip le caractère suivant
		} else {
			// Token littéral
			size_t start = i;
			while (i < pattern.size() && pattern[i] != '%') {
				++i;
			}

			PatternToken token;
			token.type = PatternToken::Type::Literal;
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
void Formatter::FormatToken(const PatternToken &token, const LogMessage &message, bool useColors, std::string &result) {
	switch (token.type) {
		case PatternToken::Type::Literal:
			result += token.value;
			break;

		case PatternToken::Type::Year: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_year + 1900, 4);
			break;
		}

		case PatternToken::Type::Month: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_mon + 1, 2);
			break;
		}

		case PatternToken::Type::Day: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_mday, 2);
			break;
		}

		case PatternToken::Type::Hour: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_hour, 2);
			break;
		}

		case PatternToken::Type::Minute: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_min, 2);
			break;
		}

		case PatternToken::Type::Second: {
			auto tm = message.GetLocalTime();
			result += FormatNumber(tm.tm_sec, 2);
			break;
		}

		case PatternToken::Type::Millis: {
			uint64 millis = message.GetMillis() % 1000;
			result += FormatNumber(static_cast<int>(millis), 3);
			break;
		}

		case PatternToken::Type::Micros: {
			uint64 micros = message.GetMicros() % 1000000;
			result += FormatNumber(static_cast<int>(micros), 6);
			break;
		}

		case PatternToken::Type::Level:
			result += LogLevelToString(message.level);
			break;

		case PatternToken::Type::LevelShort:
			result += LogLevelToShortString(message.level);
			break;

		case PatternToken::Type::ThreadId: {
			std::ostringstream oss;
			oss << message.threadId;
			result += oss.str();
			break;
		}

		case PatternToken::Type::ThreadName:
			if (!message.threadName.empty()) {
				result += message.threadName;
			} else {
				std::ostringstream oss;
				oss << message.threadId;
				result += oss.str();
			}
			break;

		case PatternToken::Type::SourceFile:
			if (!message.sourceFile.empty()) {
				// Extraire juste le nom du fichier (sans chemin)
				size_t pos = message.sourceFile.find_last_of("/\\");
				if (pos != std::string::npos) {
					result += message.sourceFile.substr(pos + 1);
				} else {
					result += message.sourceFile;
				}
			}
			break;

		case PatternToken::Type::SourceLine: {
			if (message.sourceLine > 0) {
				std::ostringstream oss;
				oss << message.sourceLine;
				result += oss.str();
			}
			break;
		}

		case PatternToken::Type::Function:
			if (!message.functionName.empty()) {
				result += message.functionName;
			}
			break;

		case PatternToken::Type::Message:
			result += message.message;
			break;

		case PatternToken::Type::LoggerName:
			if (!message.loggerName.empty()) {
				result += message.loggerName;
			} else {
				result += "default";
			}
			break;

		case PatternToken::Type::Percent:
			result += '%';
			break;

		case PatternToken::Type::ColorStart:
			if (useColors) {
				result += GetANSIColor(message.level);
			}
			break;

		case PatternToken::Type::ColorEnd:
			if (useColors) {
				result += GetANSIReset();
			}
			break;
	}
}

/**
 * @brief Formate un nombre avec padding
 */
std::string Formatter::FormatNumber(int value, int width, char fillChar) const {
	std::ostringstream oss;
	oss << std::setw(width) << std::setfill(fillChar) << value;
	return oss.str();
}

/**
 * @brief Obtient le code couleur ANSI pour un niveau de log
 */
std::string Formatter::GetANSIColor(LogLevel level) const {
	return LogLevelToANSIColor(level);
}

/**
 * @brief Obtient le code de fin de couleur ANSI
 */
std::string Formatter::GetANSIReset() const {
	return "\033[0m";
}

} // namespace logger
} // namespace nkentseu