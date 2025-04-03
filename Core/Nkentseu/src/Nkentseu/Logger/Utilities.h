#ifndef __LOGGER_UTILITIES_H__
#define __LOGGER_UTILITIES_H__

#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>

#include "Nkentseu/Platform/PlatformDetection.h"
#include "Nkentseu/Platform/Platform.h"
#include "Nkentseu/Platform/Inline.h"

//
// ToString, ReplaceAll, Format, etc. 
//

/**
 * @Function ToString
 * @Description Convertit une valeur en chaîne de caractères.
 * @Param (const T&) value : La valeur à convertir.
 * @Return (std::string) : La représentation textuelle de la valeur.
 */
template<typename T>
std::string ToString(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

/**
 * @Function ReplaceAll
 * @Description Remplace toutes les occurrences de 'from' dans la chaîne s par 'to'.
 * @Param (std::string&) s : La chaîne à modifier.
 * @Param (const std::string&) from : La sous-chaîne à rechercher.
 * @Param (const std::string&) to : La sous-chaîne de remplacement.
 */
NKENTSEU_FORCE_INLINE void NKENTSEU_API ReplaceAll(std::string& s, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = s.find(from, start_pos)) != std::string::npos) {
        s.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
}

/**
 * @Function FormatImpl
 * @Description Fonction récursive qui remplace les marqueurs {0}, {1}, ... dans la chaîne par les arguments correspondants.
 * @Param (std::string&) s : La chaîne à formater.
 * @Param (size_t) index : L'indice courant.
 */
NKENTSEU_FORCE_INLINE void NKENTSEU_API FormatImpl(std::string& s, size_t) { }

template<typename T, typename... Args>
void FormatImpl(std::string& s, size_t index, const T& value, const Args&... args) {
    ReplaceAll(s, "{" + std::to_string(index) + "}", ToString(value));
    FormatImpl(s, index + 1, args...);
}

/**
 * @Function Format
 * @Description Applique le formatage à la chaîne en remplaçant les marqueurs {0}, {1}, ... par les arguments.
 * @Param (const std::string&) fmt : La chaîne de format.
 * @Param (Args...) args : Les arguments à insérer.
 * @Return (std::string) : La chaîne formatée.
 */
template<typename... Args>
std::string Format(const std::string& fmt, const Args&... args) {
    std::string result = fmt;
    FormatImpl(result, 0, args...);
    return result;
}

/**
 * @Function GetCurrentTimestamp
 * @Description Génère un timestamp précis (jusqu'aux nanosecondes) au moment de l'appel.
 * @Return (std::string) : La date et l'heure formatées.
 */
NKENTSEU_FORCE_INLINE std::string NKENTSEU_API GetCurrentTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto now_time_t = system_clock::to_time_t(now);
    auto now_ns = duration_cast<nanoseconds>(now.time_since_epoch()) % 1000000000;
    std::stringstream ss;
#if defined(_WIN32)
    std::tm tmStruct;
    localtime_s(&tmStruct, &now_time_t);
    ss << std::put_time(&tmStruct, "%Y-%m-%d %H:%M:%S");
#else
    ss << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d %H:%M:%S");
#endif
    ss << "." << std::setfill('0') << std::setw(9) << now_ns.count();
    return ss.str();
}

#endif // __LOGGER_UTILITIES_H__
