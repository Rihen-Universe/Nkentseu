// =============================================================================
// NKCore/Text/NkSnprintf.cpp
// Moteur printf pilote par va_list : analyse du format, extraction des
// arguments selon la longueur (hh h l ll j z t L), emission par NkNumberToText.
//
// Aucune fonction de la libc n'est appelee ici : ni snprintf, ni strlen, ni
// memcpy. Seul <cstdarg> (compilateur) est requis.
//
// AUTEUR : TEUGUIA TADJUIDJE Rodolf Séderis — Rihen
// DATE : 2026-09-04
// LICENCE : Proprietary - All Rights Reserved (see LICENSE)
// =============================================================================

#include "pch.h"

#include "NKCore/Text/NkSnprintf.h"
#include "NKCore/Text/NkNumberToText.h"

namespace nkentseu {

	namespace {

		enum class NkArgLen { None, HH, H, L, LL, J, Z, T, LD };

		/// Lit un entier signe selon la longueur ; rend (negatif, magnitude).
		void NkReadSigned(va_list &ap, NkArgLen len, bool &negative, uint64 &mag) {
			int64 v;
			switch (len) {
				case NkArgLen::HH:
					v = static_cast<signed char>(va_arg(ap, int));
					break;
				case NkArgLen::H:
					v = static_cast<short>(va_arg(ap, int));
					break;
				case NkArgLen::L:
					v = va_arg(ap, long);
					break;
				case NkArgLen::LL:
					v = va_arg(ap, long long);
					break;
				case NkArgLen::J:
					v = static_cast<int64>(va_arg(ap, long long));
					break;
				case NkArgLen::Z:
					v = static_cast<int64>(va_arg(ap, usize));
					break;
				case NkArgLen::T:
					v = static_cast<int64>(va_arg(ap, isize));
					break;
				default:
					v = va_arg(ap, int);
					break;
			}
			negative = v < 0;
			// -INT64_MIN n'existe pas : passer par le non signe evite le debordement
			mag = negative ? (static_cast<uint64>(0) - static_cast<uint64>(v)) : static_cast<uint64>(v);
		}

		uint64 NkReadUnsigned(va_list &ap, NkArgLen len) {
			switch (len) {
				case NkArgLen::HH:
					return static_cast<unsigned char>(va_arg(ap, unsigned int));
				case NkArgLen::H:
					return static_cast<unsigned short>(va_arg(ap, unsigned int));
				case NkArgLen::L:
					return va_arg(ap, unsigned long);
				case NkArgLen::LL:
					return va_arg(ap, unsigned long long);
				case NkArgLen::J:
					return static_cast<uint64>(va_arg(ap, unsigned long long));
				case NkArgLen::Z:
					return static_cast<uint64>(va_arg(ap, usize));
				case NkArgLen::T:
					return static_cast<uint64>(va_arg(ap, isize));
				default:
					return va_arg(ap, unsigned int);
			}
		}

	} // namespace

	int NkVsnprintf(char *buf, usize cap, const char *fmt, va_list apIn) {
		numtext::NkCharSink out(buf, cap);
		if (!fmt) {
			out.Terminate();
			return 0;
		}
		va_list ap;
		va_copy(ap, apIn);

		const char *s = fmt;
		while (*s) {
			if (*s != '%') {
				out.Put(*s++);
				continue;
			}
			const char *specStart = s;
			++s;
			if (*s == '%') {
				out.Put('%');
				++s;
				continue;
			}

			numtext::NkNumSpec sp;
			// drapeaux
			for (bool more = true; more;) {
				switch (*s) {
					case '-':
						sp.left = true;
						++s;
						break;
					case '+':
						sp.plus = true;
						++s;
						break;
					case ' ':
						sp.space = true;
						++s;
						break;
					case '#':
						sp.alt = true;
						++s;
						break;
					case '0':
						sp.zero = true;
						++s;
						break;
					case '\'': // groupement : ignore, comme la CRT de reference
						++s;
						break;
					default:
						more = false;
						break;
				}
			}
			// largeur
			if (*s == '*') {
				const int w = va_arg(ap, int);
				if (w < 0) {
					sp.left = true;
					sp.width = -w;
				} else {
					sp.width = w;
				}
				++s;
			} else if (*s >= '1' && *s <= '9') {
				int w = 0;
				while (*s >= '0' && *s <= '9')
					w = w * 10 + (*s++ - '0');
				sp.width = w;
			}
			// precision
			if (*s == '.') {
				++s;
				if (*s == '*') {
					const int p = va_arg(ap, int);
					sp.precision = p < 0 ? -1 : p;
					++s;
				} else {
					int p = 0;
					while (*s >= '0' && *s <= '9')
						p = p * 10 + (*s++ - '0');
					sp.precision = p;
				}
			}
			// longueur
			NkArgLen len = NkArgLen::None;
			switch (*s) {
				case 'h':
					++s;
					if (*s == 'h') {
						len = NkArgLen::HH;
						++s;
					} else {
						len = NkArgLen::H;
					}
					break;
				case 'l':
					++s;
					if (*s == 'l') {
						len = NkArgLen::LL;
						++s;
					} else {
						len = NkArgLen::L;
					}
					break;
				case 'j':
					len = NkArgLen::J;
					++s;
					break;
				case 'z':
					len = NkArgLen::Z;
					++s;
					break;
				case 't':
					len = NkArgLen::T;
					++s;
					break;
				case 'L':
					len = NkArgLen::LD;
					++s;
					break;
				default:
					break;
			}
			// conversion
			const char conv = *s;
			if (conv == '\0') {
				// specificateur non termine : recopie tel quel et s'arrete
				out.Put(specStart, static_cast<usize>(s - specStart));
				break;
			}
			++s;
			sp.conv = conv;

			switch (conv) {
				case 'd':
				case 'i': {
					bool neg;
					uint64 mag;
					NkReadSigned(ap, len, neg, mag);
					numtext::NkEmitInteger(out, sp, neg, mag);
					break;
				}
				case 'u':
				case 'x':
				case 'X':
				case 'o':
				case 'b':
				case 'B':
					numtext::NkEmitInteger(out, sp, false, NkReadUnsigned(ap, len));
					break;
				case 'c': {
					const char c = static_cast<char>(va_arg(ap, int));
					sp.precision = -1;
					numtext::NkEmitText(out, sp, &c, 1u);
					break;
				}
				case 's': {
					const char *str = va_arg(ap, const char *);
					if (!str)
						str = "(null)";
					numtext::NkEmitText(out, sp, str, numtext::NkCStrLen(str));
					break;
				}
				case 'p': {
					const void *p = va_arg(ap, const void *);
					numtext::NkEmitPointer(out, sp, reinterpret_cast<uintptr>(p));
					break;
				}
				case 'f':
				case 'F':
				case 'e':
				case 'E':
				case 'g':
				case 'G':
				case 'a':
				case 'A': {
					double v;
					if (len == NkArgLen::LD)
						v = static_cast<double>(va_arg(ap, long double));
					else
						v = va_arg(ap, double);
					numtext::NkEmitFloat(out, sp, v);
					break;
				}
				case 'n': {
					int *dst = va_arg(ap, int *);
					if (dst)
						*dst = static_cast<int>(out.len);
					break;
				}
				default:
					// conversion inconnue : recopiee telle quelle (comportement
					// non defini chez printf ; ici, visible et non destructif)
					out.Put(specStart, static_cast<usize>(s - specStart));
					break;
			}
		}

		va_end(ap);
		out.Terminate();
		return static_cast<int>(out.len);
	}

	int NkSnprintf(char *buf, usize cap, const char *fmt, ...) {
		va_list ap;
		va_start(ap, fmt);
		const int n = NkVsnprintf(buf, cap, fmt, ap);
		va_end(ap);
		return n;
	}

} // namespace nkentseu
