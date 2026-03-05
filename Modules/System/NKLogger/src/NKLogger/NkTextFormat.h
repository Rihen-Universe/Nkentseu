#pragma once

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nkentseu {

namespace detail {

template <typename T>
struct NkDependentFalse : std::false_type {};

inline std::string NkToLowerCopy(std::string_view input) {
    std::string out(input.begin(), input.end());
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

inline bool NkTryParsePrecision(std::string_view props, int& precision) {
    if (props.size() < 2 || props.front() != '.') {
        return false;
    }

    int parsed = 0;
    for (std::size_t i = 1; i < props.size(); ++i) {
        const char c = props[i];
        if (c < '0' || c > '9') {
            return false;
        }
        parsed = (parsed * 10) + (c - '0');
        if (parsed > 32) {
            parsed = 32;
            break;
        }
    }

    precision = parsed;
    return true;
}

inline std::string NkApplyStringProperties(std::string value, std::string_view props) {
    const std::string p = NkToLowerCopy(props);
    if (p == "upper") {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        return value;
    }
    if (p == "lower") {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }
    if (p == "q" || p == "quote") {
        return "\"" + value + "\"";
    }
    return value;
}

template <typename T, typename = void>
struct NkHasToStringWithProps : std::false_type {};

template <typename T>
struct NkHasToStringWithProps<
    T,
    std::void_t<decltype(NKToString(std::declval<const T&>(), std::declval<std::string_view>()))>>
    : std::true_type {};

template <typename T, typename = void>
struct NkHasToString : std::false_type {};

template <typename T>
struct NkHasToString<T, std::void_t<decltype(NKToString(std::declval<const T&>()))>> : std::true_type {};

template <typename T, typename = void>
struct NkHasOStream : std::false_type {};

template <typename T>
struct NkHasOStream<T, std::void_t<decltype(std::declval<std::ostringstream&>() << std::declval<const T&>())>>
    : std::true_type {};

template <typename T>
std::string NkFormatValueImpl(const T& value, std::string_view props) {
    using D = std::decay_t<T>;

    if constexpr (std::is_same_v<D, std::string>) {
        return NkApplyStringProperties(value, props);
    } else if constexpr (std::is_same_v<D, std::string_view>) {
        return NkApplyStringProperties(std::string(value), props);
    } else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>) {
        return NkApplyStringProperties(value ? std::string(value) : std::string(), props);
    } else if constexpr (std::is_same_v<D, bool>) {
        const std::string p = NkToLowerCopy(props);
        if (p == "d" || p == "int" || p == "num") {
            return value ? "1" : "0";
        }
        return value ? "true" : "false";
    } else if constexpr (std::is_integral_v<D>) {
        std::ostringstream oss;
        const std::string p = NkToLowerCopy(props);
        if (p == "x" || p == "hex") {
            oss << std::hex << std::nouppercase;
        } else if (p == "x2" || p == "hex2") {
            oss << std::hex << std::setfill('0') << std::setw(2) << std::nouppercase;
        } else if (p == "x8" || p == "hex8") {
            oss << std::hex << std::setfill('0') << std::setw(8) << std::nouppercase;
        } else if (p == "x16" || p == "hex16") {
            oss << std::hex << std::setfill('0') << std::setw(16) << std::nouppercase;
        } else if (p == "xupper" || p == "hexupper" || p == "X" || p == "HEX") {
            oss << std::hex << std::uppercase;
        } else if (p == "bin" || p == "b") {
            using U = std::make_unsigned_t<D>;
            U u = static_cast<U>(value);
            std::string bits;
            bits.reserve(sizeof(U) * 8);
            for (int i = static_cast<int>(sizeof(U) * 8) - 1; i >= 0; --i) {
                bits.push_back((u & (U{1} << i)) ? '1' : '0');
            }
            const std::size_t firstOne = bits.find('1');
            if (firstOne == std::string::npos) {
                return "0";
            }
            return bits.substr(firstOne);
        }
        oss << value;
        return oss.str();
    } else if constexpr (std::is_floating_point_v<D>) {
        std::ostringstream oss;
        int precision = 6;
        if (NkTryParsePrecision(props, precision)) {
            oss << std::fixed << std::setprecision(precision);
        } else {
            const std::string p = NkToLowerCopy(props);
            if (p == "e" || p == "exp") {
                oss << std::scientific;
            } else if (p == "g") {
                oss << std::defaultfloat;
            }
        }
        oss << value;
        return oss.str();
    } else if constexpr (NkHasToStringWithProps<D>::value) {
        return NKToString(value, props);
    } else if constexpr (NkHasToString<D>::value) {
        return NKToString(value);
    } else if constexpr (NkHasOStream<D>::value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    } else {
        static_assert(NkDependentFalse<D>::value,
                      "No formatter found for type. Provide NKToString(const T&) or "
                      "NKToString(const T&, std::string_view).");
        return {};
    }
}

template <typename Resolver>
std::string NkFormatIndexedRuntime(std::string_view format, Resolver&& resolver) {
    std::string out;
    out.reserve(format.size() + 32);

    std::size_t i = 0;
    while (i < format.size()) {
        const char c = format[i];

        if (c == '{') {
            if (i + 1 < format.size() && format[i + 1] == '{') {
                out.push_back('{');
                i += 2;
                continue;
            }

            const std::size_t close = format.find('}', i + 1);
            if (close == std::string_view::npos) {
                out.push_back('{');
                ++i;
                continue;
            }

            const std::string_view placeholder = format.substr(i + 1, close - i - 1);
            const std::size_t colonPos = placeholder.find(':');
            std::string_view indexPart = (colonPos == std::string_view::npos)
                                             ? placeholder
                                             : placeholder.substr(0, colonPos);
            const std::string_view props = (colonPos == std::string_view::npos)
                                               ? std::string_view{}
                                               : placeholder.substr(colonPos + 1);

            while (!indexPart.empty() && std::isspace(static_cast<unsigned char>(indexPart.front()))) {
                indexPart.remove_prefix(1);
            }
            while (!indexPart.empty() && std::isspace(static_cast<unsigned char>(indexPart.back()))) {
                indexPart.remove_suffix(1);
            }

            std::size_t index = 0;
            bool validIndex = !indexPart.empty();
            for (const char digit : indexPart) {
                if (digit < '0' || digit > '9') {
                    validIndex = false;
                    break;
                }
                const std::size_t next = index * 10 + static_cast<std::size_t>(digit - '0');
                if (next < index) {
                    validIndex = false;
                    break;
                }
                index = next;
            }

            std::string replacement;
            if (validIndex && resolver(index, props, replacement)) {
                out += replacement;
            } else {
                out.append(format.substr(i, close - i + 1));
            }

            i = close + 1;
            continue;
        }

        if (c == '}' && i + 1 < format.size() && format[i + 1] == '}') {
            out.push_back('}');
            i += 2;
            continue;
        }

        out.push_back(c);
        ++i;
    }

    return out;
}

template <typename Tuple, std::size_t... I>
std::string NkFormatIndexedTuple(std::string_view format, Tuple&& tuple, std::index_sequence<I...>) {
    auto resolver = [&](std::size_t index, std::string_view props, std::string& out) -> bool {
        bool handled = false;
        (void)((index == I ? (out = NkFormatValueImpl(std::get<I>(tuple), props), handled = true, true) : false) || ...);
        return handled;
    };
    return NkFormatIndexedRuntime(format, resolver);
}

} // namespace detail

template <typename T>
std::string NkFormatValue(const T& value, std::string_view properties = {}) {
    return detail::NkFormatValueImpl(value, properties);
}

template <typename... Args>
std::string NkFormatIndexed(std::string_view format, Args&&... args) {
    if constexpr (sizeof...(Args) == 0) {
        auto resolver = [](std::size_t, std::string_view, std::string&) {
            return false;
        };
        return detail::NkFormatIndexedRuntime(format, resolver);
    } else {
        auto tuple = std::forward_as_tuple(std::forward<Args>(args)...);
        return detail::NkFormatIndexedTuple(format, tuple, std::index_sequence_for<Args...>{});
    }
}

} // namespace nkentseu
