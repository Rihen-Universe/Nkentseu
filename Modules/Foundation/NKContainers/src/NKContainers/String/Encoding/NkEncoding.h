// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkEncoding.h
// DESCRIPTION: Encoding base types et détection
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKENCODING_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKENCODING_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKCore/NkCoreExport.h"
#include "NKCore/NkInline.h"

namespace nkentseu {
    namespace core {
        namespace encoding {
            
            /**
             * @brief Types d'encoding supportés
             */
            enum class NkEncodingType {
                NK_UNKNOWN,
                NK_ASCII,
                NK_UTF8,
                NK_UTF16LE,  // Little Endian
                NK_UTF16BE,  // Big Endian
                NK_UTF32LE,
                NK_UTF32BE,
                NK_LATIN1,   // ISO-8859-1
            };
            
            /**
             * @brief Résultat de conversion
             */
            enum class NkConversionResult {
                NK_SUCCESS,
                NK_SOURCE_EXHAUSTED,  // Source incomplete
                NK_TARGET_EXHAUSTED,  // Buffer destination trop petit
                NK_SOURCE_ILLEGAL     // Séquence invalide
            };
            
            /**
             * @brief Détecte l'encoding depuis BOM
             */
            NKENTSEU_CORE_API NkEncodingType NkDetectBOM(const void* data, usize size);
            
            /**
             * @brief Vérifie validité encoding
             */
            NKENTSEU_CORE_API bool NkIsValidASCII(const char* str, usize length);
            NKENTSEU_CORE_API bool NkIsValidUTF8(const char* str, usize length);
            NKENTSEU_CORE_API bool NkIsValidUTF16(const uint16* str, usize length);
            NKENTSEU_CORE_API bool NkIsValidUTF32(const uint32* str, usize length);
            
        } // namespace encoding
    } // namespace core
} // namespace nkentseu

#endif
