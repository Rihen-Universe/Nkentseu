// -----------------------------------------------------------------------------
// FICHIER: Core/NkLogger/src/NkLogger/NkFormatter.cpp
// DESCRIPTION: Implémentation du formatter de messages de log.
// AUTEUR: Rihen
// DATE: 2026
// -----------------------------------------------------------------------------

#include "NKLogger/NkFormatter.h"
#include "NKLogger/NkLogLevel.h"
#include <cstdio>
#include <ctime>

// -----------------------------------------------------------------------------
// PATTERNS PRÉDÉFINIS
// -----------------------------------------------------------------------------
namespace nkentseu {
	namespace {

		inline void NkAppendUInt32(NkString &result, uint32 value) {
			char buffer[16];
			const int length = ::snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned int>(value));
			if (length > 0) {
				result.Append(buffer, static_cast<usize>(length));
			}
		}

	} // namespace

	const char *NkFormatter::NK_DEFAULT_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [%t] -> %v";
	const char *NkFormatter::NK_SIMPLE_PATTERN = "%v";
	const char *NkFormatter::NK_DETAILED_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%L] [%n] [thread %t] [%s:%# in %f] -> %v";
	const char *NkFormatter::NK_NKENTSEU_PATTERN = "[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] [%n] [%s:%# in %F] -> %v";
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
	NkFormatter::NkFormatter(const NkString &pattern) : m_Pattern(pattern), m_TokensValid(false) {
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
	void NkFormatter::SetPattern(const NkString &pattern) {
		if (m_Pattern != pattern) {
			m_Pattern = pattern;
			m_TokensValid = false;
		}
	}

	/**
	 * @brief Obtient le pattern courant
	 */
	const NkString &NkFormatter::GetPattern() const {
		return m_Pattern;
	}

	/**
	 * @brief Formate un message de log
	 */
	NkString NkFormatter::Format(const NkLogMessage &message) {
		return Format(message, false);
	}

	/**
	 * @brief Formate un message de log avec des couleurs
	 */
	NkString NkFormatter::Format(const NkLogMessage &message, bool useColors) {
		if (!m_TokensValid) {
			ParsePattern(m_Pattern);
		}

		NkString result;
		result.Reserve(256); // Pré-allocation pour performance

		for (const auto &token : m_Tokens) {
			FormatToken(token, message, useColors, result);
		}

		return result;
	}

	/**
	 * @brief Parse le pattern en tokens
	 */
	void NkFormatter::ParsePattern(const NkString &pattern) {
		m_Tokens.Clear();
		m_Tokens.Reserve(pattern.Size() / 2); // Estimation

		for (usize i = 0; i < pattern.Size(); ++i) {
			if (pattern[i] == '%' && i + 1 < pattern.Size()) {
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
						token.value = pattern.SubStr(i, 2);
						m_Tokens.PushBack(token);
						++i;
						continue;
				}

				m_Tokens.PushBack(token);
				++i; // Skip le caractère suivant
			} else {
				// Token littéral
				usize start = i;
				while (i < pattern.Size() && pattern[i] != '%') {
					++i;
				}

				NkPatternToken token;
				token.type = NkPatternToken::Type::NK_LITERAL;
				token.value = pattern.SubStr(start, i - start);
				m_Tokens.PushBack(token);

				--i; // Pour que la boucle incrémente correctement
			}
		}

		m_TokensValid = true;
	}

	/**
	 * @brief Formate un token individuel
	 */
	void NkFormatter::FormatToken(const NkPatternToken &token, const NkLogMessage &message, bool useColors, NkString &result) {
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
				NkAppendUInt32(result, message.threadId);
				break;
			}

			case NkPatternToken::Type::NK_THREAD_NAME:
				if (!message.threadName.Empty()) {
					result += message.threadName;
				} else {
					NkAppendUInt32(result, message.threadId);
				}
				break;

			case NkPatternToken::Type::NK_SOURCE_FILE:
				if (!message.sourceFile.Empty()) {
					// Extraire juste le nom du fichier (sans chemin)
					usize pos = message.sourceFile.FindLastOf("/\\");
					if (pos != NkString::npos) {
						result += message.sourceFile.SubStr(pos + 1);
					} else {
						result += message.sourceFile;
					}
				}
				break;

			case NkPatternToken::Type::NK_SOURCE_LINE: {
				if (message.sourceLine > 0) {
					NkAppendUInt32(result, message.sourceLine);
				}
				break;
			}

			case NkPatternToken::Type::NK_FUNCTION:
				if (!message.functionName.Empty()) {
					result += message.functionName;
				}
				break;

			case NkPatternToken::Type::NK_MESSAGE:
				result += message.message;
				break;

			case NkPatternToken::Type::NK_LOGGER_NAME:
				if (!message.loggerName.Empty()) {
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
	NkString NkFormatter::FormatNumber(int value, int width, char fillChar) const {
		char buffer[32];
		const int length = ::snprintf(buffer, sizeof(buffer), "%d", value);
		if (length <= 0) {
			return {};
		}

		if (width <= length) {
			return NkString(buffer, static_cast<usize>(length));
		}

		const int pad = width - length;
		NkString out;
		out.Reserve(static_cast<usize>(width));

		// Conserve le signe devant les zéros: -01 au lieu de 0-1.
		if (fillChar == '0' && value < 0) {
			out.PushBack('-');
			out.Append(static_cast<usize>(pad), fillChar);
			out.Append(buffer + 1, static_cast<usize>(length - 1));
			return out;
		}

		out.Append(static_cast<usize>(pad), fillChar);
		out.Append(buffer, static_cast<usize>(length));
		return out;
	}

	/**
	 * @brief Obtient le code couleur ANSI pour un niveau de log
	 */
	NkString NkFormatter::GetANSIColor(NkLogLevel level) const {
		return NkLogLevelToANSIColor(level);
	}

	/**
	 * @brief Obtient le code de fin de couleur ANSI
	 */
	NkString NkFormatter::GetANSIReset() const {
		return "\033[0m";
	}

} // namespace nkentseu
