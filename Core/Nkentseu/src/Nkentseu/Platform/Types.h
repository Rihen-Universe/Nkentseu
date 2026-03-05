#pragma once

#include "Export.h"
#include "Inline.h"

#include <istream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>

namespace nkentseu
{
    
    ///////////////////////////////////////////////////////////////////////////////
    // Types fondamentaux
    ///////////////////////////////////////////////////////////////////////////////

    // Entiers non signés
    using uint8         = unsigned char;
    using uint16        = unsigned short;
    using uint32        = unsigned int;
    using uintl32       = unsigned long int;
    using uint64        = unsigned long long int;
    using usize         = unsigned long long;

    // Entiers signés
    using int8          = signed char;
    using int16         = signed short int;
    using int32         = signed int;
    using int64         = signed long long int;
    using usize_gpu     = signed long long;

    // Flottants
    using float32       = float;
    using float64       = double;
    using float80       = long double; // Support limité

    // Booléens
    using Boolean       = uint8;
    using bool32        = int32;
    static constexpr Boolean True  = 1;
    static constexpr Boolean False = 0;

    using charb         = char;
    using char8         = char8_t;
    using char16        = char16_t;
    using char32        = char32_t;
    #if defined(NKENTSEU_PLATFORM_WINDOWS)
    using wchar         = wchar_t;    // UTF-16 sur Windows (2 octets)
    #else
    using wchar         = char32;     // Alias vers char32_t sur Unix
    #endif

    ///////////////////////////////////////////////////////////////////////////////
    // Constantes globales
    ///////////////////////////////////////////////////////////////////////////////
    namespace constants {
        static constexpr uint64 INVALID_ID_UINT64 = 18446744073709551615UL;
        static constexpr uint32 INVALID_ID        = 4294967295U;
        static constexpr uint16 INVALID_ID_UINT16 = 65535U;
        static constexpr uint8  INVALID_ID_UINT8  = 255U;
        static constexpr void*  Null              = nullptr;
        static constexpr usize  USIZE_MAX         = std::numeric_limits<usize>::max();
        
        static constexpr usize INT_BUFFER_SIZE    = 32;
        static constexpr usize FLOAT_BUFFER_SIZE  = 64;
        static constexpr usize BOOL_BUFFER_SIZE   = 8;

        constexpr usize STRING_BUFFER_SIZE = 1024;
    }

    namespace unicode {
        constexpr uint32 REPLACEMENT_CHAR = 0xFFFD;
    }

    ///////////////////////////////////////////////////////////////////////////////
    // Macros utilitaires
    ///////////////////////////////////////////////////////////////////////////////
    #define BIT(x)          (1 << (x))
    #define STDPH(v)        std::placeholders::_##v
    #define STR_BOOL(b)     ((b) ? "True" : "False")
    #define KCLAMP(v,mi,ma) (((v) <= (mi)) ? (mi) : (((v) >= (ma)) ? (ma) : (v)))
    #define ARRAY_SIZE(a)   (sizeof(a) / sizeof((a)[0]))

    // Ajoutez cette structure avant la définition de compositionMap
    struct VectorHash {
        size_t operator()(const std::vector<uint32>& vec) const {
            size_t seed = vec.size();
            for(auto& i : vec) {
                seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };

    // Tables de normalisation Unicode (extrait significatif)
    struct UnicodeNormalization {
        // Décomposition canonique
        static const std::unordered_map<uint32, std::vector<uint32>> decompositionMap;
        // Table de composition canonique
        static const std::unordered_map<std::vector<uint32>, uint32, VectorHash> compositionMap;
        // Classes de combinaison Unicode (extrait)
        static const std::unordered_map<uint32, uint8> combiningClasses;
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Enumération des encodages supportés
    ///////////////////////////////////////////////////////////////////////////////

    enum class Encoding {
        Unknown,
        ASCII,
        UTF8,
        UTF16_LE,     // Little Endian (Windows standard)
        UTF16_BE,     // Big Endian
        UTF32_LE,
        UTF32_BE,
        Latin1,
        SystemDefault // Encodage du système local
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Gestion des BOM (Byte Order Mark)
    ///////////////////////////////////////////////////////////////////////////////

    namespace BOM {
        constexpr uint8 UTF8[] = {0xEF, 0xBB, 0xBF};
        constexpr uint8 UTF16_LE[] = {0xFF, 0xFE};
        constexpr uint8 UTF16_BE[] = {0xFE, 0xFF};
        constexpr uint8 UTF32_LE[] = {0xFF, 0xFE, 0x00, 0x00};
        constexpr uint8 UTF32_BE[] = {0x00, 0x00, 0xFE, 0xFF};
    }

    enum class NormalizationForm {
        NFC, NFD, NFKC, NFKD
    };

    ///////////////////////////////////////////////////////////////////////////////
    // Détection automatique d'encodage
    ///////////////////////////////////////////////////////////////////////////////

    Encoding DetectEncodingFromBOM(const uint8* data, usize size);

} // namespace nkentseu
