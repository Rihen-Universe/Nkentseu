#pragma once

#include "NKCore/NkTraits.h"
#include "NKContainers/String/NkString.h"
#include "NKContainers/String/NkStringUtils.h"
#include "NKContainers/String/NkStringView.h"

#include <cctype>
#include <cstdio>

namespace nkentseu {

namespace detail {

template <typename T>
struct NkDependentFalse : traits::NkFalseType {};

inline NkString NkToLowerCopy(NkStringView input) {
    NkString out(input.Data(), input.Size());
    for (char& c : out) {
        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

inline bool NkTryParsePrecision(NkStringView props, int& precision) {
    if (props.Size() < 2 || props.Front() != '.') {
        return false;
    }

    int parsed = 0;
    for (usize i = 1; i < props.Size(); ++i) {
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

template <typename T, typename = void>
struct NkHasToStringWithProps : traits::NkFalseType {};

template <typename T>
struct NkHasToStringWithProps<
    T,
    traits::NkVoidT<decltype(NKToString(*static_cast<const T*>(nullptr), NkStringView{}))>>
    : traits::NkTrueType {};

template <typename T, typename = void>
struct NkHasToString : traits::NkFalseType {};

template <typename T>
struct NkHasToString<T, traits::NkVoidT<decltype(NKToString(*static_cast<const T*>(nullptr)))>>
    : traits::NkTrueType {};

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
struct NkMakeUnsigned<char16_t> { using Type = unsigned short; };
template <>
struct NkMakeUnsigned<char32_t> { using Type = unsigned int; };
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
    } else if constexpr (NkHasToStringWithProps<D>::value) {
        return NKToString(value, props);
    } else if constexpr (NkHasToString<D>::value) {
        return NKToString(value);
    } else {
        static_assert(NkDependentFalse<D>::value,
                      "No formatter found for type. Provide NKToString(const T&) or "
                      "NKToString(const T&, NkStringView)."
        );
        return {};
    }
}

template <typename Resolver>
NkString NkFormatIndexedRuntime(NkStringView format, Resolver&& resolver) {
    NkString out;
    out.Reserve(format.Size() + 32);

    usize i = 0;
    while (i < format.Size()) {
        const char c = format[i];

        if (c == '{') {
            if (i + 1 < format.Size() && format[i + 1] == '{') {
                out.PushBack('{');
                i += 2;
                continue;
            }

            const usize close = format.Find('}', i + 1);
            if (close == NkStringView::npos) {
                out.PushBack('{');
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

            while (!indexPart.Empty() && ::isspace(static_cast<unsigned char>(indexPart.Front()))) {
                indexPart.RemovePrefix(1);
            }
            while (!indexPart.Empty() && ::isspace(static_cast<unsigned char>(indexPart.Back()))) {
                indexPart.RemoveSuffix(1);
            }

            usize index = 0;
            bool validIndex = !indexPart.Empty();
            for (const char digit : indexPart) {
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
                out += replacement;
            } else {
                out.Append(format.SubStr(i, close - i + 1));
            }

            i = close + 1;
            continue;
        }

        if (c == '}' && i + 1 < format.Size() && format[i + 1] == '}') {
            out.PushBack('}');
            i += 2;
            continue;
        }

        out.PushBack(c);
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

template <typename T>
NkString NkFormatValue(const T& value, NkStringView properties = {}) {
    return detail::NkFormatValueImpl(value, properties);
}

template <typename... Args>
NkString NkFormatIndexed(NkStringView format, Args&&... args) {
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
                                            args...);
        };
        return detail::NkFormatIndexedRuntime(format, resolver);
    }
}

} // namespace nkentseu
