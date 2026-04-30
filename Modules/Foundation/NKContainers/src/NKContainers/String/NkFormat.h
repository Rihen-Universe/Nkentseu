// -----------------------------------------------------------------------------
// FICHIER: Containers/NKString/NkFormat.h
// DESCRIPTION: Moteur de formatage de chaînes unifié
//              - Syntaxe accolades : NkFormat("{0:>10.2f} {1:,}", 3.14, 1000000)
//              - Syntaxe printf    : NkPrintf("%-15s %08.3f", "hello", 3.14)
//              Points d'extension :
//                1. Fonction libre ADL NkToString(const MyType&, const NkFormatProps&)
//                2. Spécialisation de nkentseu::NkFormatter<MyType>
//                3. Macro NK_FORMATTER / NK_FORMATTER_END
//              Compatible avec les conventions du framework Nkentseu.
// AUTEUR: Rihen
// DATE: 2026
// VERSION: 4.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_FORMAT_NKFORMAT_H_INCLUDED
#define NK_FORMAT_NKFORMAT_H_INCLUDED

// ============================================================
// INCLUSIONS
// ============================================================

#include "NkString.h"
#include "NkStringView.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/Iterators/NkIterator.h"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <type_traits>

// ============================================================
// DÉCLARATIONS PRINCIPALES (namespace nkentseu)
// ============================================================

namespace nkentseu {

    /// Propriétés de formatage, issues du mini-langage de spécification.
    struct NkFormatProps {
        char         fill      = ' ';   ///< Caractère de remplissage
        char         align     = '\0';  ///< Alignement : '<' '>' '^' '='
        char         sign      = '\0';  ///< Signe explicite : '+' '-' ' '
        bool         alt       = false; ///< '#' (préfixe 0x, 0b, …)
        bool         zeroPad   = false; ///< '0' (remplissage par zéros)
        int          width     = -1;    ///< Largeur de champ (-1 = non spécifié)
        char         grouping  = '\0';  ///< Séparateur de milliers : ',' ou '_'
        int          precision = -1;    ///< Précision (-1 = non spécifié)
        char         type      = '\0';  ///< Caractère de conversion (d, x, f, …)
        NkStringView raw;               ///< Spécification brute restante (extensions custom)

        bool HasWidth()     const noexcept { return width >= 0; }
        bool HasPrecision() const noexcept { return precision >= 0; }

        /// Applique largeur et alignement à une chaîne déjà formatée.
        /// @param content Chaîne à ajuster
        /// @param numeric Vrai si la valeur est numérique (gère le zéro-padding)
        NkString ApplyWidth(const NkStringView& content, bool numeric = false) const;
    };

    // ── Point d'extension principal : NkFormatter<T> ─────────────────┐
    template<typename T, typename Enable = void>
    struct NkFormatter;  // Spécialisation utilisateur possible

    // ── API publique ─────────────────────────────────────────────────┐

    /// Formatage avec accolades {i:props}
    template<typename... Args>
    NkString NkFormat(NkStringView fmt, const Args&... args);

    /// Formatage style printf (%…)
    template<typename... Args>
    NkString NkPrintf(NkStringView fmt, const Args&... args);

    /// Ajoute le résultat d'un NkFormat dans une chaîne existante
    template<typename... Args>
    void NkFormatTo(NkString& out, NkStringView fmt, const Args&... args);

    /// Ajoute le résultat d'un NkPrintf dans une chaîne existante
    template<typename... Args>
    void NkPrintfTo(NkString& out, NkStringView fmt, const Args&... args);

    // Sorties pratiques
    template<typename... Args>
    void NkPrint  (NkStringView fmt, const Args&... args);
    template<typename... Args>
    void NkPrintln(NkStringView fmt, const Args&... args);
    template<typename... Args>
    void NkEPrint  (NkStringView fmt, const Args&... args);
    template<typename... Args>
    void NkEPrintln(NkStringView fmt, const Args&... args);

} // namespace nkentseu

// ============================================================
// IMPLÉMENTATIONS (détail, placé dans nkentseu car requis par les templates)
// ============================================================

namespace nkentseu {
    // ── Alignement / padding (membre de NkFormatProps) ──────────────────

    inline NkString NkFormatProps::ApplyWidth(const NkStringView& content, bool numeric) const {
        if (!HasWidth()) return NkString(content);

        int len = static_cast<int>(content.Size());
        int pad = width - len;
        if (pad <= 0) return NkString(content);

        char effectiveAlign = align ? align : (numeric ? '>' : '<');
        char fillChar       = fill;

        // Zéro-padding numérique : insère les '0' après le signe/préfixe
        if (zeroPad && numeric && effectiveAlign == '>') {
            int prefixLen = 0;
            if (content.Size() > 0 && (content[0] == '-' || content[0] == '+' || content[0] == ' '))
                prefixLen = 1;
            if (prefixLen + 1 < len && content[prefixLen] == '0' &&
                (content[prefixLen + 1] == 'x' || content[prefixLen + 1] == 'X' ||
                content[prefixLen + 1] == 'b' || content[prefixLen + 1] == 'B' ||
                content[prefixLen + 1] == 'o'))
                prefixLen += 2;

            NkString result;
            result.Append(content.SubStr(0, prefixLen));
            result.Append(static_cast<NkString::SizeType>(pad), '0');
            result.Append(content.SubStr(prefixLen));
            return result;
        }

        NkString result;
        result.Reserve(static_cast<NkString::SizeType>(width));

        switch (effectiveAlign) {
            case '<':
                result.Append(content);
                result.Append(static_cast<NkString::SizeType>(pad), fillChar);
                break;
            case '>':
                result.Append(static_cast<NkString::SizeType>(pad), fillChar);
                result.Append(content);
                break;
            case '^': {
                int left  = pad / 2;
                int right = pad - left;
                result.Append(static_cast<NkString::SizeType>(left), fillChar);
                result.Append(content);
                result.Append(static_cast<NkString::SizeType>(right), fillChar);
                break;
            }
            case '=': {
                if (content.Size() > 0 &&
                    (content[0] == '-' || content[0] == '+' || content[0] == ' ')) {
                    result.Append(content.SubStr(0, 1));
                    result.Append(static_cast<NkString::SizeType>(pad), fillChar);
                    result.Append(content.SubStr(1));
                } else {
                    result.Append(static_cast<NkString::SizeType>(pad), fillChar);
                    result.Append(content);
                }
                break;
            }
            default:
                result.Append(static_cast<NkString::SizeType>(pad), fillChar);
                result.Append(content);
                break;
        }
        return result;
    }

    namespace detail {

        // ── Groupement des milliers ──────────────────────────────────────────

        inline NkString NkApplyGrouping(const NkStringView& digits, char sep) {
            int signLen = 0;
            if (digits.Size() > 0 && (digits[0] == '-' || digits[0] == '+' || digits[0] == ' '))
                signLen = 1;

            NkStringView signPart = digits.SubStr(0, signLen);
            NkStringView digitPart = digits.SubStr(signLen);
            int n = static_cast<int>(digitPart.Size());
            if (n <= 3) return NkString(digits);

            NkString result;
            int rem = n % 3;
            int i   = 0;
            if (rem > 0) {
                result.Append(digitPart.SubStr(0, rem));
                i = rem;
            }
            while (i < n) {
                if (!result.Empty()) result.Append(sep);
                result.Append(digitPart.SubStr(i, 3));
                i += 3;
            }
            return NkString(signPart) + result;
        }

        // ── Formatage entier ─────────────────────────────────────────────────

        inline NkString NkFmtInteger(unsigned long long uval, bool isSigned, bool negative, const NkFormatProps& p) {
            char type = p.type ? p.type : 'd';

            NkString result;
            char buf[72];

            switch (type) {
                case 'd': case 'i':
                    snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(negative ? -static_cast<long long>(uval) : uval));
                    result = buf;
                    break;
                case 'u':
                    snprintf(buf, sizeof(buf), "%llu", uval);
                    result = buf;
                    break;
                case 'x':
                    snprintf(buf, sizeof(buf), "%llx", uval);
                    result = buf;
                    if (p.alt) result = NkString("0x") + result;
                    break;
                case 'X':
                    snprintf(buf, sizeof(buf), "%llX", uval);
                    result = buf;
                    if (p.alt) result = NkString("0X") + result;
                    break;
                case 'o':
                    snprintf(buf, sizeof(buf), "%llo", uval);
                    result = buf;
                    if (p.alt && result.Length() > 0 && result[0] != '0') result = NkString("0") + result;
                    break;
                case 'b': case 'B': {
                    if (uval == 0) {
                        result = "0";
                    } else {
                        NkString bin;
                        for (auto v = uval; v; v >>= 1)
                            bin = NkString(1, static_cast<char>('0' + (v & 1))) + bin;
                        result = std::move(bin);
                    }
                    if (p.alt) result = (type == 'b' ? NkString("0b") : NkString("0B")) + result;
                    break;
                }
                case 'c':
                    return p.ApplyWidth(NkStringView(NkString(1, static_cast<char>(uval))), false);
                case '%': {
                    snprintf(buf, sizeof(buf), "%.6g%%", static_cast<double>(uval) * 100.0);
                    return p.ApplyWidth(NkStringView(NkString(buf)), true);
                }
                default:
                    snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(negative ? -static_cast<long long>(uval) : uval));
                    result = buf;
                    break;
            }

            // Signe explicite
            if (!negative) {
                if (p.sign == '+') result = NkString("+") + result;
                else if (p.sign == ' ') result = NkString(" ") + result;
            }

            // Groupement (décimal uniquement)
            if (p.grouping && (type == 'd' || type == 'i' || type == 'u'))
                result = NkApplyGrouping(NkStringView(result), p.grouping);

            return p.ApplyWidth(NkStringView(result), true);
        }

        inline NkString NkFmtInteger(long long val, const NkFormatProps& p) {
            bool neg = val < 0;
            unsigned long long uval = neg ? static_cast<unsigned long long>(-val) : static_cast<unsigned long long>(val);
            return NkFmtInteger(uval, true, neg, p);
        }

        inline NkString NkFmtInteger(unsigned long long val, const NkFormatProps& p) {
            return NkFmtInteger(val, false, false, p);
        }

        // ── Formatage flottant ───────────────────────────────────────────────

        inline NkString NkFmtFloat(double val, const NkFormatProps& p) {
            char type = p.type ? p.type : 'g';
            int  prec = p.HasPrecision() ? p.precision : 6;
            bool neg  = std::signbit(val);

            double fval      = val;
            bool   appendPct = false;
            char   typeChar;

            switch (type) {
                case 'f': case 'F': typeChar = type; break;
                case 'e': case 'E': typeChar = type; break;
                case 'g': case 'G': typeChar = type; break;
                case '%': fval *= 100.0; appendPct = true; typeChar = 'f'; break;
                default:  typeChar = 'g'; break;
            }

            char spec[32];
            snprintf(spec, sizeof(spec), "%%%s.%d%c", p.alt ? "#" : "", prec, typeChar);

            char buf[128];
            snprintf(buf, sizeof(buf), spec, fval);
            NkString result(buf);
            if (appendPct) result.Append('%');

            if (!neg) {
                if (p.sign == '+') result = NkString("+") + result;
                else if (p.sign == ' ') result = NkString(" ") + result;
            }

            if (p.grouping) {
                // Utiliser NkString (propriétaire) pour éviter les vues sur temporaires
                NkString::SizeType dotPos = result.Find('.');
                NkString intStr  = (dotPos == NkString::npos) ? result     : result.SubStr(0, dotPos);
                NkString fracStr = (dotPos == NkString::npos) ? NkString() : result.SubStr(dotPos);
                result = NkApplyGrouping(NkStringView(intStr), p.grouping) + fracStr;
            }

            return p.ApplyWidth(NkStringView(result), true);
        }

        // ── Parseur de spécifications ───────────────────────────────────────

        inline NkFormatProps NkParseBraceSpec(NkStringView spec) {
            NkFormatProps p;
            p.raw = spec;
            if (spec.Empty()) return p;

            const char* s   = spec.Data();
            const char* end = s + spec.Size();

            auto IsAlign = [](char c) { return c == '<' || c == '>' || c == '^' || c == '='; };

            // [[fill]align]
            if (s + 1 < end && IsAlign(s[1])) { p.fill = s[0]; p.align = s[1]; s += 2; }
            else if (s < end && IsAlign(s[0])) { p.align = s[0]; s++; }

            // [sign]
            if (s < end && (s[0] == '+' || s[0] == '-' || s[0] == ' ')) p.sign = *s++;

            // [#]
            if (s < end && s[0] == '#') { p.alt = true; s++; }

            // [0]
            if (s < end && s[0] == '0') { p.zeroPad = true; s++; }

            // [width]
            if (s < end && s[0] >= '1' && s[0] <= '9') {
                p.width = 0;
                while (s < end && s[0] >= '0' && s[0] <= '9')
                    p.width = p.width * 10 + (*s++ - '0');
            }

            // [grouping]
            if (s < end && (s[0] == ',' || s[0] == '_')) p.grouping = *s++;

            // [.precision]
            if (s < end && s[0] == '.') {
                ++s;
                p.precision = 0;
                while (s < end && s[0] >= '0' && s[0] <= '9')
                    p.precision = p.precision * 10 + (*s++ - '0');
            }

            // [type]
            if (s < end) p.type = *s;

            return p;
        }

        inline NkFormatProps NkParsePrintfSpec(const char* specStart, const char* typePos) {
            NkFormatProps p;
            p.raw = NkStringView(specStart, static_cast<size_t>(typePos - specStart + 1));
            const char* s = specStart;

            for (bool more = true; more && s < typePos;) {
                switch (*s) {
                    case '-': p.align   = '<';  ++s; break;
                    case '+': p.sign    = '+';  ++s; break;
                    case ' ': if (p.sign != '+') p.sign = ' '; ++s; break;
                    case '#': p.alt     = true; ++s; break;
                    case '0': p.zeroPad = true; ++s; break;
                    default:  more = false; break;
                }
            }

            if (s < typePos && *s >= '1' && *s <= '9') {
                p.width = 0;
                while (s < typePos && *s >= '0' && *s <= '9')
                    p.width = p.width * 10 + (*s++ - '0');
            }

            if (s < typePos && *s == '.') {
                ++s;
                p.precision = 0;
                while (s < typePos && *s >= '0' && *s <= '9')
                    p.precision = p.precision * 10 + (*s++ - '0');
            }

            while (s < typePos &&
                (*s == 'l' || *s == 'h' || *s == 'z' || *s == 't' || *s == 'j'))
                ++s;

            p.type = *typePos;
            if (p.align == '\0') p.align = '>'; // printf aligne par défaut à droite
            return p;
        }

        // ── Couche d'extension ADL ──────────────────────────────────────────

        namespace adl {

            // Fallback générique : erreur de compilation si rien n'est trouvé
            template<typename T>
            NkString NkToString(const T&, const NkFormatProps&) {
                static_assert(sizeof(T) == 0,
                    "Aucune spécialisation de NkFormatter<T> ni de fonction libre NkToString(T, NkFormatProps) "
                    "trouvée. Veuillez fournir une surcharge ADL ou une spécialisation de NkFormatter<T>.");
                return {};
            }

            // Appel non qualifié – ADL préfère la version de l'utilisateur à fallback
            template<typename T>
            NkString Invoke(const T& val, const NkFormatProps& props) {
                return NkToString(val, props);
            }

        } // namespace adl

        // ── Détection de NkFormatter<T>::format() ───────────────────────────

        template<typename T, typename = void>
        struct NkHasFormatter : std::false_type {};

        template<typename T>
        struct NkHasFormatter<T, std::void_t<decltype(NkFormatter<T>::Convert(
            std::declval<const T&>(), std::declval<const NkFormatProps&>()))>>
            : std::true_type {};

        // ── NkFmtDispatch principal ──────────────────────────────────────────────

        template<typename T>
        NkString NkFmtDispatch(const T& val, const NkFormatProps& p) {
            if constexpr (NkHasFormatter<T>::value)
                return NkFormatter<T>::Convert(val, p);
            else
                return adl::Invoke(val, p);
        }

        // ── Effacement de type : NkAnyArg ─────────────────────────────────────

        struct NkAnyArg {
            using Fn = NkString (*)(const void*, const NkFormatProps&);
            const void* ptr = nullptr;
            Fn          fn  = nullptr;

            template<typename T>
            static NkAnyArg Make(const T& val) {
                NkAnyArg a;
                a.ptr = static_cast<const void*>(std::addressof(val));
                a.fn  = [](const void* p, const NkFormatProps& props) -> NkString {
                    return NkFmtDispatch(*static_cast<const T*>(p), props);
                };
                return a;
            }

            NkString Format(const NkFormatProps& p) const { return fn(ptr, p); }
        };

        // Gestion des tableaux : stocke l'adresse du premier élément
        template<typename T>
        NkAnyArg NkMakeArg(const T& val) {
            if constexpr (std::is_array_v<T>) {
                using Elem = std::remove_extent_t<T>;
                NkAnyArg a;
                a.ptr = static_cast<const void*>(&val[0]);
                a.fn  = [](const void* p, const NkFormatProps& props) -> NkString {
                    const Elem* ptr = static_cast<const Elem*>(p);
                    return NkFmtDispatch(ptr, props);
                };
                return a;
            } else {
                return NkAnyArg::Make(val);
            }
        }

        // ── Moteur d'exécution pour accolades ───────────────────────────────

        inline NkString NkRunBrace(NkStringView fmt, const NkVector<NkAnyArg>& args) {
            NkString out;
            out.Reserve(fmt.Size() * 2);

            const char* s   = fmt.Data();
            const char* end = s + fmt.Size();

            while (s < end) {
                if (*s == '{') {
                    if (s + 1 < end && s[1] == '{') { out.Append('{'); s += 2; continue; }

                    const char* close = s + 1;
                    while (close < end && *close != '}') ++close;
                    if (close >= end)
                        throw std::runtime_error("nkformat: accolade '{' non fermée dans la chaîne de format");

                    NkStringView inner(s + 1, static_cast<size_t>(close - s - 1));

                    // Index obligatoire
                    int idx = 0;
                    const char* ip = inner.Data();
                    const char* ie = ip + inner.Size();
                    while (ip < ie && *ip >= '0' && *ip <= '9')
                        idx = idx * 10 + (*ip++ - '0');

                    // Spécification optionnelle après ':'
                    NkStringView spec;
                    if (ip < ie && *ip == ':')
                        spec = NkStringView(ip + 1, static_cast<size_t>(ie - (ip + 1)));

                    if (idx < 0 || idx >= static_cast<int>(args.Size()))
                        throw std::out_of_range("nkformat: index d'argument hors limites");

                    out.Append(args[idx].Format(NkParseBraceSpec(spec)));
                    s = close + 1;

                } else if (*s == '}') {
                    if (s + 1 < end && s[1] == '}') { out.Append('}'); s += 2; continue; }
                    throw std::runtime_error("nkformat: accolade '}' inattendue");
                } else {
                    out.Append(*s++);
                }
            }
            return out;
        }

        // ── Moteur d'exécution pour printf ─────────────────────────────────

        inline NkString NkRunPrintf(NkStringView fmt, const NkVector<NkAnyArg>& args) {
            NkString out;
            out.Reserve(fmt.Size() * 2);

            const char* s   = fmt.Data();
            const char* end = s + fmt.Size();
            int argIdx = 0;

            while (s < end) {
                if (*s != '%') { out.Append(*s++); continue; }
                if (s + 1 < end && s[1] == '%') { out.Append('%'); s += 2; continue; }

                const char* specStart = s + 1;
                const char* p = specStart;

                // Flags
                while (p < end && (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0')) ++p;
                // Width
                while (p < end && *p >= '0' && *p <= '9') ++p;
                // Precision
                if (p < end && *p == '.') { ++p; while (p < end && *p >= '0' && *p <= '9') ++p; }
                // Length modifiers
                while (p < end && (*p == 'l' || *p == 'h' || *p == 'z' || *p == 't' || *p == 'j')) ++p;
                // Type char
                if (p >= end)
                    throw std::runtime_error("nkformat: spécificateur printf non terminé");

                if (argIdx >= static_cast<int>(args.Size()))
                    throw std::out_of_range("nkformat: trop peu d'arguments pour le format printf");

                out.Append(args[argIdx++].Format(NkParsePrintfSpec(specStart, p)));
                s = p + 1;
            }
            return out;
        }

    } // namespace detail

    // ============================================================
    // SPÉCIALISATIONS INTÉGRÉES DE NkFormatter<T>
    // ============================================================

    // ── Entiers signés ─────────────────────────────────────────────────

    #define NK_FORMATTER_SIGNED(Type)                                           \
    template<> struct NkFormatter<Type> {                                       \
        static NkString Convert(const Type& v, const NkFormatProps& p) {        \
            return detail::NkFmtInteger(static_cast<long long>(v), p);            \
        }                                                                       \
    };

    NK_FORMATTER_SIGNED(signed char)
    NK_FORMATTER_SIGNED(short)
    NK_FORMATTER_SIGNED(int)
    NK_FORMATTER_SIGNED(long)
    NK_FORMATTER_SIGNED(long long)

    // ── Entiers non signés ──────────────────────────────────────────────

    #define NK_FORMATTER_UNSIGNED(Type)                                         \
    template<> struct NkFormatter<Type> {                                       \
        static NkString Convert(const Type& v, const NkFormatProps& p) {        \
            return detail::NkFmtInteger(static_cast<unsigned long long>(v), p);   \
        }                                                                       \
    };

    NK_FORMATTER_UNSIGNED(unsigned char)
    NK_FORMATTER_UNSIGNED(unsigned short)
    NK_FORMATTER_UNSIGNED(unsigned int)
    NK_FORMATTER_UNSIGNED(unsigned long)
    NK_FORMATTER_UNSIGNED(unsigned long long)

    // ── Flottants ───────────────────────────────────────────────────────

    template<> struct NkFormatter<float> {
        static NkString Convert(const float& v, const NkFormatProps& p) {
            return detail::NkFmtFloat(static_cast<double>(v), p);
        }
    };
    template<> struct NkFormatter<double> {
        static NkString Convert(const double& v, const NkFormatProps& p) {
            return detail::NkFmtFloat(v, p);
        }
    };
    template<> struct NkFormatter<long double> {
        static NkString Convert(const long double& v, const NkFormatProps& p) {
            return detail::NkFmtFloat(static_cast<double>(v), p);
        }
    };

    // ── bool ────────────────────────────────────────────────────────────

    template<> struct NkFormatter<bool> {
        static NkString Convert(const bool& v, const NkFormatProps& p) {
            if (p.type == 'd' || p.type == 'i' || p.type == 'u')
                return detail::NkFmtInteger(static_cast<unsigned long long>(v ? 1 : 0), p);
            // Utiliser NkStringView(const char*) pour éviter l'ambiguïté de conversion
            return p.ApplyWidth(NkStringView(v ? "true" : "false"), false);
        }
    };

    // ── char ────────────────────────────────────────────────────────────

    template<> struct NkFormatter<char> {
        static NkString Convert(const char& v, const NkFormatProps& p) {
            if (p.type && p.type != 'c')
                return detail::NkFmtInteger(static_cast<unsigned long long>(static_cast<unsigned char>(v)), p);
            // NkString temporaire puis vue explicite pour éviter l'ambiguïté
            NkString charStr(1, v);
            return p.ApplyWidth(NkStringView(charStr), false);
        }
    };

    // ── Chaînes C ───────────────────────────────────────────────────────

    template<> struct NkFormatter<const char*> {
        static NkString Convert(const char* const& v, const NkFormatProps& p) {
            if (!v)
                return p.ApplyWidth(NkStringView("<null>"), false);
            NkStringView sv(v, std::strlen(v));
            if (p.HasPrecision() && static_cast<int>(sv.Size()) > p.precision)
                sv = sv.SubStr(0, p.precision);
            // sv est déjà NkStringView : pas de conversion implicite ambiguë
            return p.ApplyWidth(sv, false);
        }
    };

    template<> struct NkFormatter<char*> {
        static NkString Convert(char* const& v, const NkFormatProps& p) {
            return NkFormatter<const char*>::Convert(v, p);
        }
    };

    // ── NkString / NkStringView ────────────────────────────────────────

    template<> struct NkFormatter<NkString> {
        static NkString Convert(const NkString& v, const NkFormatProps& p) {
            // Initialisation directe : résout l'ambiguïté (appelle le constructeur)
            NkStringView sv(v);
            if (p.HasPrecision() && static_cast<int>(sv.Size()) > p.precision)
                sv = sv.SubStr(0, p.precision);
            return p.ApplyWidth(sv, false);
        }
    };

    template<> struct NkFormatter<NkStringView> {
        static NkString Convert(const NkStringView& v, const NkFormatProps& p) {
            NkStringView sv(v);
            if (p.HasPrecision() && static_cast<int>(sv.Size()) > p.precision)
                sv = sv.SubStr(0, p.precision);
            return p.ApplyWidth(sv, false);
        }
    };

    // ── nullptr_t ───────────────────────────────────────────────────────

    template<> struct NkFormatter<std::nullptr_t> {
        static NkString Convert(const std::nullptr_t&, const NkFormatProps& p) {
            return p.ApplyWidth(NkStringView("nullptr"), false);
        }
    };

    // ── Pointeurs génériques (hors char déjà traité) ───────────────────

    template<typename T>
    struct NkFormatter<T*, std::enable_if_t<!std::is_same_v<std::remove_cv_t<T>, char>>> {
        static NkString Convert(T* const& v, const NkFormatProps& p) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%p", static_cast<const void*>(v));
            // NkStringView(const char*) : résout l'ambiguïté sans allocation NkString
            return p.ApplyWidth(NkStringView(buf), false);
        }
    };

    // ============================================================
    // API PUBLIQUE (implémentations inline)
    // ============================================================

    template<typename... Args>
    NkString NkFormat(NkStringView fmt, const Args&... args) {
        NkVector<detail::NkAnyArg> a;
        a.Reserve(sizeof...(args));
        (a.PushBack(detail::NkMakeArg(args)), ...);
        return detail::NkRunBrace(fmt, a);
    }

    template<typename... Args>
    NkString NkPrintf(NkStringView fmt, const Args&... args) {
        NkVector<detail::NkAnyArg> a;
        a.Reserve(sizeof...(args));
        (a.PushBack(detail::NkMakeArg(args)), ...);
        return detail::NkRunPrintf(fmt, a);
    }

    template<typename... Args>
    void NkFormatTo(NkString& out, NkStringView fmt, const Args&... args) {
        out.Append(NkFormat(fmt, args...));
    }

    template<typename... Args>
    void NkPrintfTo(NkString& out, NkStringView fmt, const Args&... args) {
        out.Append(NkPrintf(fmt, args...));
    }

    template<typename... Args>
    void NkPrint(NkStringView fmt, const Args&... args) {
        std::fputs(NkFormat(fmt, args...).Data(), stdout);
    }

    template<typename... Args>
    void NkPrintln(NkStringView fmt, const Args&... args) {
        std::puts(NkFormat(fmt, args...).Data());
    }

    template<typename... Args>
    void NkEPrint(NkStringView fmt, const Args&... args) {
        std::fputs(NkFormat(fmt, args...).Data(), stderr);
    }

    template<typename... Args>
    void NkEPrintln(NkStringView fmt, const Args&... args) {
        std::fputs((NkFormat(fmt, args...) + "\n").Data(), stderr);
    }

    // Surcharge pour les conteneurs séquentiels
    template <typename Container>
    traits::NkEnableIf_t<traits::NkIsSequentialContainer_v<Container>, NkString>
    NkToString(const Container& c, const NkFormatProps& props = {}) {
        NkString result = "[";
        bool first = true;
        for (auto it = nkentseu::GetBegin(c); it != nkentseu::GetEnd(c); ++it) {
            if (!first) result += ", ";
            result += NkToString(*it, props);
            first = false;
        }
        result += "]";
        return result;
    }

    // Surcharge pour les conteneurs associatifs (NkHashMap, NkUnorderedMap, NkMap)
    // L'itérateur retourne un NkPair<const Key, Value> : accès via .First et .Second
    template <typename Container>
    traits::NkEnableIf_t<traits::NkIsAssociativeContainer_v<Container>, NkString>
    NkToString(const Container& c, const NkFormatProps& props = {}) {
        NkString result = "{";
        bool first = true;
        for (auto it = nkentseu::GetBegin(c); it != nkentseu::GetEnd(c); ++it) {
            if (!first) result += ", ";
            result += NkToString((*it).First, props);
            result += ": ";
            result += NkToString((*it).Second, props);
            first = false;
        }
        result += "}";
        return result;
    }

    // ============================================================
    // CORPS DE NkString::Fmt — défini ici car dépend de NkFormat
    // ============================================================

    template<typename... Args>
    NkString NkString::Fmt(const char* format, const Args&... args) {
        return NkFormat(NkStringView(format), args...);
    }

} // namespace nkentseu

// ============================================================
// MACRO DE CONVENANCE (dans l'espace global, préfixée NK_)
// ============================================================

/// Définit une spécialisation de NkFormatter<Type>.
/// Dans le bloc, la variable 'val' (const Type&) et 'props' (const NkFormatProps&) sont disponibles.
/// @example
///   NK_FORMATTER(Vec3) {
///       return nkentseu::NkFormat("({0}, {1}, {2})", val.x, val.y, val.z);
///   } NK_FORMATTER_END
#define NK_FORMATTER(Type)                                                   \
    template<> struct nkentseu::NkFormatter<Type> {                           \
        static nkentseu::NkString Convert(const Type& val,                    \
                                          const nkentseu::NkFormatProps& props)

#define NK_FORMATTER_END };

#endif // NK_FORMAT_NKFORMAT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================