#include "pch.h"
#include "Types.h"
#include <cstring>

namespace nkentseu
{
    // Décomposition canonique
    const std::unordered_map<uint32, std::vector<uint32>> UnicodeNormalization::decompositionMap = {
        // Lettres latines avec diacritiques
        {0x00C0, {0x0041, 0x0300}}, {0x00C1, {0x0041, 0x0301}},
        {0x00C2, {0x0041, 0x0302}}, {0x00C3, {0x0041, 0x0303}},
        {0x00C8, {0x0045, 0x0300}}, {0x00C9, {0x0045, 0x0301}},
        {0x00CA, {0x0045, 0x0302}}, {0x00CC, {0x0049, 0x0300}},
        {0x00CD, {0x0049, 0x0301}}, {0x00CE, {0x0049, 0x0302}},
        {0x00D2, {0x004F, 0x0300}}, {0x00D3, {0x004F, 0x0301}},
        {0x00D4, {0x004F, 0x0302}}, {0x00D9, {0x0055, 0x0300}},
        {0x00DA, {0x0055, 0x0301}}, {0x00DD, {0x0059, 0x0301}},
        
        // Symboles et ponctuations
        {0x00A8, {0x0020, 0x0308}}, // Tréma
        {0x00B4, {0x0020, 0x0301}}, // Accent aigu
        {0x2018, {0x0027}},         // Guillemet simple gauche
        {0x2019, {0x0027}},         // Guillemet simple droit
        
        // Caractères coréens (exemple de composition)
        {0xAC00, {0x1100, 0x1161}}, // 가
        {0xAE00, {0x1100, 0x1173}}, // 개
        
        // Symboles mathématiques
        {0x2260, {0x003D, 0x0338}}, // ≠
        {0x2248, {0x007E, 0x0303}}  // ≈
    };

    // Table de composition canonique
    const std::unordered_map<std::vector<uint32>, uint32, VectorHash> UnicodeNormalization::compositionMap = {
        {{0x0041, 0x0300}, 0x00C0}, {{0x0041, 0x0301}, 0x00C1},
        {{0x0045, 0x0301}, 0x00C9}, {{0x0055, 0x0308}, 0x00DC},
        {{0x1100, 0x1161}, 0xAC00}, {{0x1100, 0x1173}, 0xAE00}
    };

    // Classes de combinaison Unicode (extrait)
    const std::unordered_map<uint32, uint8> UnicodeNormalization::combiningClasses = {
        {0x0300, 230}, {0x0301, 230}, {0x0302, 230}, {0x0303, 230},
        {0x0308, 230}, {0x0312, 232}, {0x0315, 232}, {0x0323, 220},
        {0x0324, 220}, {0x0338, 1},   {0x0345, 240}
    };

    Encoding DetectEncodingFromBOM(const uint8* data, usize size) {
        if(size >= 3 && memcmp(data, BOM::UTF8, 3) == 0)
            return Encoding::UTF8;
        if(size >= 2 && memcmp(data, BOM::UTF16_LE, 2) == 0)
            return Encoding::UTF16_LE;
        if(size >= 2 && memcmp(data, BOM::UTF16_BE, 2) == 0)
            return Encoding::UTF16_BE;
        if(size >= 4 && memcmp(data, BOM::UTF32_LE, 4) == 0)
            return Encoding::UTF32_LE;
        if(size >= 4 && memcmp(data, BOM::UTF32_BE, 4) == 0)
            return Encoding::UTF32_BE;
        return Encoding::Unknown;
    }
} // namespace nkentseu
