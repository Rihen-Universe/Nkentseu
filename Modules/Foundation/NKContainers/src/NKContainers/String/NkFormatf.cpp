// =============================================================================
// @File    NkFormatf.cpp
// @Brief   Implémentation du moteur printf NkFormatf
// @Author  TEUGUIA TADJUIDJE Rodolf Séderis
// @Date    2026-03-06
// @License Apache-2.0
//
// @Algorithme
//  On parse la chaîne de format caractère par caractère.
//  Quand on rencontre '%', on extrait les flags/largeur/précision/longueur
//  et le specifier, on reconstruit un sous-format printf valide, on appelle
//  snprintf dans un buffer temporaire, et on appende au NkString résultat.
//  Le specifier 'b' (binaire) est géré manuellement (non standard C).
// =============================================================================

#include "pch.h"
#include "NKContainers/String/NkFormatf.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace nkentseu {
    

        // ---------------------------------------------------------------------
        // Helpers internes
        // ---------------------------------------------------------------------

        // Taille du buffer temporaire pour snprintf.
        static constexpr int FMTBUF_SIZE = 1024;

        // Extrait la sous-chaîne de format entre '%' et le specifier.
        // subfmt reçoit "[flags][width][.prec]" sans modificateur de longueur.
        // lenMod reçoit le modificateur de longueur ("", "l", "ll", "h", etc.)
        // Retourne le specifier, avance *pp après lui.
        static char ExtractSubFmt(const char** pp,
                                   char* subfmt, int subfmtSize,
                                   char* lenMod,  int lenModSize) {
            const char* p = *pp;
            int si = 0;
            int li = 0;

            // flags : - + 0 espace #
            while (*p == '-' || *p == '+' || *p == '0' ||
                   *p == ' ' || *p == '#') {
                if (si < subfmtSize - 2) subfmt[si++] = *p;
                ++p;
            }

            // largeur (* ignoré)
            if (*p == '*') {
                ++p;
            } else {
                while (*p >= '0' && *p <= '9') {
                    if (si < subfmtSize - 2) subfmt[si++] = *p;
                    ++p;
                }
            }

            // précision
            if (*p == '.') {
                if (si < subfmtSize - 2) subfmt[si++] = '.';
                ++p;
                if (*p == '*') {
                    ++p;
                } else {
                    while (*p >= '0' && *p <= '9') {
                        if (si < subfmtSize - 2) subfmt[si++] = *p;
                        ++p;
                    }
                }
            }

            // modificateur de longueur : l ll h hh L z — stocké séparément
            if (*p == 'l') {
                if (li < lenModSize - 2) lenMod[li++] = *p; ++p;
                if (*p == 'l') {
                    if (li < lenModSize - 2) lenMod[li++] = *p; ++p;
                }
            } else if (*p == 'h') {
                if (li < lenModSize - 2) lenMod[li++] = *p; ++p;
                if (*p == 'h') {
                    if (li < lenModSize - 2) lenMod[li++] = *p; ++p;
                }
            } else if (*p == 'L' || *p == 'z') {
                if (li < lenModSize - 2) lenMod[li++] = *p; ++p;
            }

            subfmt[si] = '\0';
            lenMod[li] = '\0';
            char spec = *p ? *p++ : '\0';
            *pp = p;
            return spec;
        }

        // Convertit un entier non-signé en représentation binaire
        NkString NkToBinaryString(unsigned long long value, int width, char fill) {
            char tmp[65];
            int len = 0;

            if (value == 0) {
                tmp[len++] = '0';
            } else {
                unsigned long long v = value;
                while (v) {
                    tmp[len++] = (char)('0' + (v & 1));
                    v >>= 1;
                }
            }

            // Inverser
            for (int a = 0, b = len - 1; a < b; ++a, --b) {
                char t = tmp[a]; tmp[a] = tmp[b]; tmp[b] = t;
            }
            tmp[len] = '\0';

            // Padding
            if (width > len) {
                int pad = width - len;
                NkString result(static_cast<NkString::SizeType>(pad), fill);
                result.Append(tmp, static_cast<NkString::SizeType>(len));
                return result;
            }

            return NkString(tmp, static_cast<NkString::SizeType>(len));
        }

        // Construit le format final "%<subfmt>ll<specReplacement>"
        static void BuildFinalFmt(char* out, int outSize,
                                   const char* subfmt, const char* spec) {
            ::snprintf(out, static_cast<size_t>(outSize), "%%%s%s", subfmt, spec);
        }

        // ---------------------------------------------------------------------
        // NkVFormatf — moteur principal
        // ---------------------------------------------------------------------

        NkString NkVFormatf(const char* fmt, va_list args) {
            NkString result;
            if (!fmt) return result;

            result.Reserve(static_cast<NkString::SizeType>(::strlen(fmt) * 2));

            const char* p = fmt;
            char buf[FMTBUF_SIZE];
            char subfmt[24];   // flags + width + prec (sans length mod)
            char lenMod[8];    // length modifier ("", "l", "ll", "h", "hh")

            while (*p) {
                if (*p != '%') {
                    result.Append(*p++);
                    continue;
                }
                ++p; // saute '%'

                if (*p == '%') {
                    result.Append('%');
                    ++p;
                    continue;
                }

                char spec = ExtractSubFmt(&p, subfmt, sizeof(subfmt),
                                                       lenMod, sizeof(lenMod));

                switch (spec) {
                    // ----- Entiers signés -----
                    case 'd': case 'i': {
                        long long v;
                        if (lenMod[0] == 'l' && lenMod[1] == 'l')
                            v = va_arg(args, long long);
                        else if (lenMod[0] == 'l')
                            v = (long long)va_arg(args, long);
                        else
                            v = (long long)va_arg(args, int);
                        char finalFmt[32];
                        BuildFinalFmt(finalFmt, sizeof(finalFmt), subfmt, "lld");
                        ::snprintf(buf, sizeof(buf), finalFmt, v);
                        result.Append(buf);
                        break;
                    }

                    // ----- Entiers non-signés -----
                    case 'u': {
                        unsigned long long v;
                        if (lenMod[0] == 'l' && lenMod[1] == 'l')
                            v = va_arg(args, unsigned long long);
                        else if (lenMod[0] == 'l')
                            v = (unsigned long long)va_arg(args, unsigned long);
                        else
                            v = (unsigned long long)va_arg(args, unsigned int);
                        char finalFmt[32];
                        BuildFinalFmt(finalFmt, sizeof(finalFmt), subfmt, "llu");
                        ::snprintf(buf, sizeof(buf), finalFmt, v);
                        result.Append(buf);
                        break;
                    }

                    // ----- Hexadécimal -----
                    case 'x': {
                        unsigned long long v;
                        if (lenMod[0] == 'l' && lenMod[1] == 'l')
                            v = va_arg(args, unsigned long long);
                        else if (lenMod[0] == 'l')
                            v = (unsigned long long)va_arg(args, unsigned long);
                        else
                            v = (unsigned long long)va_arg(args, unsigned int);
                        char finalFmt[32];
                        BuildFinalFmt(finalFmt, sizeof(finalFmt), subfmt, "llx");
                        ::snprintf(buf, sizeof(buf), finalFmt, v);
                        result.Append(buf);
                        break;
                    }
                    case 'X': {
                        unsigned long long v;
                        if (lenMod[0] == 'l' && lenMod[1] == 'l')
                            v = va_arg(args, unsigned long long);
                        else if (lenMod[0] == 'l')
                            v = (unsigned long long)va_arg(args, unsigned long);
                        else
                            v = (unsigned long long)va_arg(args, unsigned int);
                        char finalFmt[32];
                        BuildFinalFmt(finalFmt, sizeof(finalFmt), subfmt, "llX");
                        ::snprintf(buf, sizeof(buf), finalFmt, v);
                        result.Append(buf);
                        break;
                    }

                    // ----- Octal -----
                    case 'o': {
                        unsigned long long v;
                        if (lenMod[0] == 'l' && lenMod[1] == 'l')
                            v = va_arg(args, unsigned long long);
                        else if (lenMod[0] == 'l')
                            v = (unsigned long long)va_arg(args, unsigned long);
                        else
                            v = (unsigned long long)va_arg(args, unsigned int);
                        char finalFmt[32];
                        BuildFinalFmt(finalFmt, sizeof(finalFmt), subfmt, "llo");
                        ::snprintf(buf, sizeof(buf), finalFmt, v);
                        result.Append(buf);
                        break;
                    }

                    // ----- Binaire (extension) -----
                    case 'b': {
                        unsigned long long v;
                        if (lenMod[0] == 'l' && lenMod[1] == 'l')
                            v = va_arg(args, unsigned long long);
                        else if (lenMod[0] == 'l')
                            v = (unsigned long long)va_arg(args, unsigned long);
                        else
                            v = (unsigned long long)va_arg(args, unsigned int);
                        int w = 0;
                        char fillChar = ' ';
                        const char* sf = subfmt;
                        if (*sf == '0') { fillChar = '0'; ++sf; }
                        while (*sf >= '0' && *sf <= '9') { w = w * 10 + (*sf - '0'); ++sf; }
                        result.Append(NkToBinaryString(v, w, fillChar));
                        break;
                    }

                    // ----- Flottants — float est promu en double, pas de correction -----
                    case 'f': case 'F':
                    case 'e': case 'E':
                    case 'g': case 'G': {
                        double v = va_arg(args, double);
                        // Reconstruit le format complet avec le length mod d'origine
                        char finalFmt[32];
                        ::snprintf(finalFmt, sizeof(finalFmt), "%%%s%s%c", subfmt, lenMod, spec);
                        ::snprintf(buf, sizeof(buf), finalFmt, v);
                        result.Append(buf);
                        break;
                    }

                    // ----- Caractère -----
                    case 'c': {
                        int v = va_arg(args, int);
                        result.Append(static_cast<char>(v));
                        break;
                    }

                    // ----- Chaîne -----
                    case 's': {
                        const char* v = va_arg(args, const char*);
                        if (!v) v = "(null)";
                        if (!subfmt[0]) {
                            result.Append(v);
                        } else {
                            int slen = static_cast<int>(::strlen(v));
                            if (slen < FMTBUF_SIZE - 32) {
                                char finalFmt[32];
                                BuildFinalFmt(finalFmt, sizeof(finalFmt), subfmt, "s");
                                ::snprintf(buf, sizeof(buf), finalFmt, v);
                                result.Append(buf);
                            } else {
                                result.Append(v);
                            }
                        }
                        break;
                    }

                    // ----- Pointeur -----
                    case 'p': {
                        void* v = va_arg(args, void*);
                        ::snprintf(buf, sizeof(buf), "%p", v);
                        result.Append(buf);
                        break;
                    }

                    // ----- Inconnu → passe brut -----
                    default: {
                        result.Append('%');
                        if (spec) result.Append(spec);
                        break;
                    }
                }
            }

            return result;
        }

        NkString NkFormatf(const char* fmt, ...) {
            va_list args;
            va_start(args, fmt);
            NkString result = NkVFormatf(fmt, args);
            va_end(args);
            return result;
        }

    
} // namespace nkentseu
