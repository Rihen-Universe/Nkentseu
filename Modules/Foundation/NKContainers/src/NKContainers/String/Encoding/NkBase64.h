// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\Encoding\NkBase64.h
// DESCRIPTION: Base64 encoding/decoding
// AUTEUR: Rihen
// DATE: 2026-02-07
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_ENCODING_NKBASE64_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_ENCODING_NKBASE64_H_INCLUDED

#include "NKCore/NkTypes.h"
#include "NKContainers/String/NkString.h"

namespace nkentseu {
    
        namespace encoding {
            namespace base64 {
                
                /**
                 * @brief Encode data to Base64
                 */
                NKENTSEU_CONTAINERS_API NkString NkEncode(const uint8* data, usize length);
                
                /**
                 * @brief Decode Base64 to data
                 */
                NKENTSEU_CONTAINERS_API bool NkDecode(NkStringView base64, uint8* out, usize* outLength);
                
                /**
                 * @brief Decode Base64 to string
                 */
                NKENTSEU_CONTAINERS_API NkString NkDecodeToString(NkStringView base64);
                
                /**
                 * @brief Encode string to Base64
                 */
                NKENTSEU_CONTAINERS_API NkString NkEncodeString(NkStringView str);
                
                /**
                 * @brief URL-safe Base64 encode
                 */
                NKENTSEU_CONTAINERS_API NkString NkEncodeURLSafe(const uint8* data, usize length);
                
                /**
                 * @brief URL-safe Base64 decode
                 */
                NKENTSEU_CONTAINERS_API bool NkDecodeURLSafe(NkStringView base64, uint8* out, usize* outLength);
                
                /**
                 * @brief Check if string is valid Base64
                 */
                NKENTSEU_CONTAINERS_API bool NkIsValid(NkStringView base64);
                
            } // namespace base64
        } // namespace encoding
    
} // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_ENCODING_NKBASE64_H_INCLUDED