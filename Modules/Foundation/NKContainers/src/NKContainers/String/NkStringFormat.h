// ============================================================================
// FICHIER: NkStringFormat.h
// DESCRIPTION: Formatage de chaînes unifié (Printf-style et Placeholder-style)
// AUTEUR: Rihen
// DATE: 2026-03-06
// ============================================================================

#pragma once

#ifndef NK_CORE_NKCONTAINERS_SRC_NKCONTAINERS_STRING_NKSTRINGFORMAT_H_INCLUDED
#define NK_CORE_NKCONTAINERS_SRC_NKCONTAINERS_STRING_NKSTRINGFORMAT_H_INCLUDED

#include "NkString.h"
#include "NkStringView.h"
#include "NKCore/NkTraits.h"

#include <cctype>
#include <cstdio>
#include <cstdarg>

namespace nkentseu {
    
        namespace string {
            namespace detail {
                
                // ============================================================
                // HELPERS POUR FORMATAGE
                // ============================================================
                
                template <typename T>
                struct NkDependentFalse : traits::NkFalseType {};

                inline NkString NkToLowerCopy(NkStringView input) {
                    NkString out(input);
                    for (char& c : out) {
                        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
                    }
                    return out;
                }

                inline bool NkTryParsePrecision(NkStringView props, int& precision) {
                    if (props.Length() < 2 || props[0] != '.') {
                        return false;
                    }

                    int parsed = 0;
                    for (usize i = 1; i < props.Length(); ++i) {
                        const char c = props[i];
                        if (c < '0' || c > '9') {
                            return false;
                        }

                        parsed = (parsed * 10) + static_cast<int>(c - '0');
                        if (parsed > 32) {
                            parsed = 32;
                            break;
                        }
                    }

                    precision = parsed;
                    return true;
                }

                inline NkString NkApplyStringProperties(NkString value, NkStringView props) {
                    const NkString p = NkToLowerCopy(props);
                    if (p == "upper") {
                        for (char& c : value) {
                            c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
                        }
                        return value;
                    }

                    if (p == "lower") {
                        for (char& c : value) {
                            c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
                        }
                        return value;
                    }

                    if (p == "q" || p == "quote") {
                        return "\"" + value + "\"";
                    }

                    return value;
                }

                template <typename T>
                struct NkMakeUnsigned;

                template <>
                struct NkMakeUnsigned<signed char> { using Type = unsigned char; };
                template <>
                struct NkMakeUnsigned<unsigned char> { using Type = unsigned char; };
                template <>
                struct NkMakeUnsigned<char> { using Type = unsigned char; };
                template <>
                struct NkMakeUnsigned<wchar_t> { using Type = unsigned int; };
                template <>
                struct NkMakeUnsigned<short> { using Type = unsigned short; };
                template <>
                struct NkMakeUnsigned<unsigned short> { using Type = unsigned short; };
                template <>
                struct NkMakeUnsigned<int> { using Type = unsigned int; };
                template <>
                struct NkMakeUnsigned<unsigned int> { using Type = unsigned int; };
                template <>
                struct NkMakeUnsigned<long> { using Type = unsigned long; };
                template <>
                struct NkMakeUnsigned<unsigned long> { using Type = unsigned long; };
                template <>
                struct NkMakeUnsigned<long long> { using Type = unsigned long long; };
                template <>
                struct NkMakeUnsigned<unsigned long long> { using Type = unsigned long long; };

                template <typename T>
                using NkMakeUnsigned_t = typename NkMakeUnsigned<traits::NkRemoveCV_t<T>>::Type;

                inline NkString NkUnsignedToBase(unsigned long long value,
                                                unsigned base,
                                                bool uppercase,
                                                usize minWidth = 0) {
                    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
                    char buffer[65];
                    usize pos = sizeof(buffer);

                    do {
                        const unsigned idx = static_cast<unsigned>(value % base);
                        buffer[--pos] = digits[idx];
                        value /= base;
                    } while (value > 0 && pos > 0);

                    while ((sizeof(buffer) - pos) < minWidth && pos > 0) {
                        buffer[--pos] = '0';
                    }

                    return NkString(buffer + pos, sizeof(buffer) - pos);
                }

                template <typename T>
                NkString NkFormatDecimal(T value) {
                    char buffer[64];
                    int length = 0;

                    if constexpr (traits::NkIsSigned_v<T>) {
                        length = ::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
                    } else {
                        length = ::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
                    }

                    return length > 0 ? NkString(buffer, static_cast<usize>(length)) : NkString{};
                }

                template <typename T>
                NkString NkFormatFloating(T value, NkStringView props) {
                    char buffer[128];
                    int length = 0;

                    int precision = 0;
                    if (NkTryParsePrecision(props, precision)) {
                        length = ::snprintf(buffer, sizeof(buffer), "%.*f", precision, static_cast<double>(value));
                        return length > 0 ? NkString(buffer, static_cast<usize>(length)) : NkString{};
                    }

                    const NkString p = NkToLowerCopy(props);
                    if (p == "e" || p == "exp") {
                        length = ::snprintf(buffer, sizeof(buffer), "%e", static_cast<double>(value));
                    } else if (p == "g" || p.Empty()) {
                        length = ::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(value));
                    } else {
                        length = ::snprintf(buffer, sizeof(buffer), "%f", static_cast<double>(value));
                    }

                    return length > 0 ? NkString(buffer, static_cast<usize>(length)) : NkString{};
                }

                template <typename T>
                NkString NkFormatValueImpl(const T& value, NkStringView props) {
                    using D = traits::NkDecay_t<T>;

                    if constexpr (traits::NkIsSame_v<D, NkString>) {
                        return NkApplyStringProperties(value, props);
                    } else if constexpr (traits::NkIsSame_v<D, NkStringView>) {
                        return NkApplyStringProperties(NkString(value), props);
                    } else if constexpr (traits::NkIsSame_v<D, const char*> ||
                                         traits::NkIsSame_v<D, char*>) {
                        return NkApplyStringProperties(value ? NkString(value) : NkString(), props);
                    } else if constexpr (traits::NkIsSame_v<D, bool>) {
                        const NkString p = NkToLowerCopy(props);
                        if (p == "d" || p == "int" || p == "num") {
                            return value ? "1" : "0";
                        }
                        return value ? "true" : "false";
                    } else if constexpr (traits::NkIsIntegral_v<D>) {
                        const NkString p = NkToLowerCopy(props);
                        if (p == "x" || p == "hex") {
                            using U = NkMakeUnsigned_t<D>;
                            return NkUnsignedToBase(static_cast<unsigned long long>(static_cast<U>(value)), 16, false);
                        } else if (p == "x2" || p == "hex2") {
                            using U = NkMakeUnsigned_t<D>;
                            return NkUnsignedToBase(static_cast<unsigned long long>(static_cast<U>(value)), 16, false, 2);
                        } else if (p == "x8" || p == "hex8") {
                            using U = NkMakeUnsigned_t<D>;
                            return NkUnsignedToBase(static_cast<unsigned long long>(static_cast<U>(value)), 16, false, 8);
                        } else if (p == "x16" || p == "hex16") {
                            using U = NkMakeUnsigned_t<D>;
                            return NkUnsignedToBase(static_cast<unsigned long long>(static_cast<U>(value)), 16, false, 16);
                        } else if (p == "xupper" || p == "hexupper") {
                            using U = NkMakeUnsigned_t<D>;
                            return NkUnsignedToBase(static_cast<unsigned long long>(static_cast<U>(value)), 16, true);
                        } else if (p == "bin" || p == "b") {
                            using U = NkMakeUnsigned_t<D>;
                            return NkUnsignedToBase(static_cast<unsigned long long>(static_cast<U>(value)), 2, false);
                        }
                        return NkFormatDecimal(value);
                    } else if constexpr (traits::NkIsFloatingPoint_v<D>) {
                        return NkFormatFloating(value, props);
                    } else {
                        static_assert(NkDependentFalse<D>::value,
                                      "No formatter found for type. Please override or provide custom converter."
                        );
                        return {};
                    }
                }

                template <typename Resolver>
                NkString NkFormatIndexedRuntime(NkStringView format, Resolver&& resolver) {
                    NkString out;
                    out.Reserve(format.Length() + 32);

                    usize i = 0;
                    while (i < format.Length()) {
                        const char c = format[i];

                        if (c == '{') {
                            if (i + 1 < format.Length() && format[i + 1] == '{') {
                                out.Append('{');
                                i += 2;
                                continue;
                            }

                            const usize close = format.Find('}', i + 1);
                            if (close == NkStringView::npos) {
                                out.Append('{');
                                ++i;
                                continue;
                            }

                            const NkStringView placeholder = format.SubStr(i + 1, close - i - 1);
                            const usize colonPos = placeholder.Find(':');
                            NkStringView indexPart = (colonPos == NkStringView::npos)
                                ? placeholder
                                : placeholder.SubStr(0, colonPos);
                            const NkStringView props = (colonPos == NkStringView::npos)
                                ? NkStringView{}
                                : placeholder.SubStr(colonPos + 1);

                            while (!indexPart.Empty() && ::isspace(static_cast<unsigned char>(indexPart[0]))) {
                                indexPart = indexPart.SubStr(1);
                            }
                            while (!indexPart.Empty() && ::isspace(static_cast<unsigned char>(indexPart[indexPart.Length() - 1]))) {
                                indexPart = indexPart.SubStr(0, indexPart.Length() - 1);
                            }

                            usize index = 0;
                            bool validIndex = !indexPart.Empty();
                            for (usize j = 0; j < indexPart.Length(); ++j) {
                                const char digit = indexPart[j];
                                if (digit < '0' || digit > '9') {
                                    validIndex = false;
                                    break;
                                }

                                const usize next = index * 10 + static_cast<usize>(digit - '0');
                                if (next < index) {
                                    validIndex = false;
                                    break;
                                }

                                index = next;
                            }

                            NkString replacement;
                            if (validIndex && resolver(index, props, replacement)) {
                                out.Append(replacement);
                            } else {
                                out.Append(format.SubStr(i, close - i + 1));
                            }

                            i = close + 1;
                            continue;
                        }

                        if (c == '}' && i + 1 < format.Length() && format[i + 1] == '}') {
                            out.Append('}');
                            i += 2;
                            continue;
                        }

                        out.Append(c);
                        ++i;
                    }

                    return out;
                }

                inline bool NkResolveIndexed(usize,
                                             usize,
                                             NkStringView,
                                             NkString&) {
                    return false;
                }

                template <typename First, typename... Rest>
                bool NkResolveIndexed(usize index,
                                      usize current,
                                      NkStringView props,
                                      NkString& out,
                                      First&& first,
                                      Rest&&... rest) {
                    if (index == current) {
                        out = NkFormatValueImpl(first, props);
                        return true;
                    }

                    if constexpr (sizeof...(Rest) == 0) {
                        return false;
                    } else {
                        return NkResolveIndexed(index,
                                                current + 1,
                                                props,
                                                out,
                                                traits::NkForward<Rest>(rest)...);
                    }
                }

            } // namespace detail

            // ============================================================
            // FORMATAGE PUBLIC
            // ============================================================

            /**
             * @brief Format une valeur avec propriétés
             * Exemple: NkFormatValue(42, "hex") -> "2a"
             */
            template <typename T>
            NkString NkFormatValue(const T& value, NkStringView properties = {}) {
                return detail::NkFormatValueImpl(value, properties);
            }

            /**
             * @brief Format avec placeholders {i:p}
             * Exemple: NkFormat("{0} + {1} = {2}", 2, 3, 5)
             * Propriétés supportées: hex, bin, upper, lower, .prec, e, g, d, etc.
             */
            template <typename... Args>
            NkString NkFormat(NkStringView format, Args&&... args) {
                if constexpr (sizeof...(Args) == 0) {
                    auto resolver = [](usize, NkStringView, NkString&) {
                        return false;
                    };
                    return detail::NkFormatIndexedRuntime(format, resolver);
                } else {
                    auto resolver = [&](usize index, NkStringView props, NkString& out) -> bool {
                        return detail::NkResolveIndexed(index,
                                                        0,
                                                        props,
                                                        out,
                                                        traits::NkForward<Args>(args)...);
                    };
                    return detail::NkFormatIndexedRuntime(format, resolver);
                }
            }

        } // namespace string
    
} // namespace nkentseu

#endif // NK_CORE_NKCONTAINERS_SRC_NKCONTAINERS_STRING_NKSTRINGFORMAT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// Proprietary License - Free to use and modify
// Generated by Rihen on 2026-03-06
// ============================================================
