// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkUTF8.cpp
// DESCRIPTION: Implémentation des fonctions d'encodage/décodage UTF-8
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Inclusions locales
// -------------------------------------------------------------------------
#include "NkUTF8.h"
#include "NkUTF16.h"
#include "NkUTF32.h"

// -------------------------------------------------------------------------
// Namespace principal
// -------------------------------------------------------------------------
namespace nkentseu {

    // -------------------------------------------------------------------------
    // Namespace encoding
    // -------------------------------------------------------------------------
    namespace encoding {

        // -------------------------------------------------------------------------
        // Sous-namespace utf8
        // -------------------------------------------------------------------------
        namespace utf8 {

            // =====================================================================
            // IMPLÉMENTATION : DÉCODAGE DE CODEPOINTS
            // =====================================================================

            usize NkDecodeChar(const char* str, uint32& outCodepoint) {

                // Validation du pointeur d'entrée
                if (!str) {
                    return 0;
                }

                // Extraction du premier byte pour analyse du pattern
                uint8 byte1 = static_cast<uint8>(str[0]);

                // -----------------------------------------------------------------
                // Cas 1 : Séquence 1-byte (0xxxxxxx) - ASCII compatible
                // -----------------------------------------------------------------
                if ((byte1 & 0x80) == 0) {
                    outCodepoint = static_cast<uint32>(byte1);
                    return 1;
                }

                // -----------------------------------------------------------------
                // Cas 2 : Séquence 2-byte (110xxxxx 10xxxxxx)
                // -----------------------------------------------------------------
                if ((byte1 & 0xE0) == 0xC0) {

                    // Vérification du byte de continuation
                    if (!NkIsContinuationByte(str[1])) {
                        return 0;
                    }

                    // Reconstruction du codepoint depuis les bits significatifs
                    outCodepoint = ((static_cast<uint32>(byte1) & 0x1F) << 6) | 
                                   (static_cast<uint32>(str[1]) & 0x3F);

                    // Rejet des encodages sur-longueur (overlong encoding)
                    return (outCodepoint >= 0x80) ? 2 : 0;
                }

                // -----------------------------------------------------------------
                // Cas 3 : Séquence 3-byte (1110xxxx 10xxxxxx 10xxxxxx)
                // -----------------------------------------------------------------
                if ((byte1 & 0xF0) == 0xE0) {

                    // Vérification des deux bytes de continuation
                    if (!NkIsContinuationByte(str[1]) || !NkIsContinuationByte(str[2])) {
                        return 0;
                    }

                    // Reconstruction du codepoint
                    outCodepoint = ((static_cast<uint32>(byte1) & 0x0F) << 12) | 
                                   ((static_cast<uint32>(str[1]) & 0x3F) << 6) | 
                                   (static_cast<uint32>(str[2]) & 0x3F);

                    // Validation : pas de sur-encodage, pas de surrogate range
                    if (outCodepoint < 0x800) {
                        return 0;
                    }
                    if (outCodepoint >= 0xD800 && outCodepoint <= 0xDFFF) {
                        return 0;
                    }

                    return 3;
                }

                // -----------------------------------------------------------------
                // Cas 4 : Séquence 4-byte (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
                // -----------------------------------------------------------------
                if ((byte1 & 0xF8) == 0xF0) {

                    // Vérification des trois bytes de continuation
                    if (!NkIsContinuationByte(str[1]) || 
                        !NkIsContinuationByte(str[2]) || 
                        !NkIsContinuationByte(str[3])) {
                        return 0;
                    }

                    // Reconstruction du codepoint
                    outCodepoint = ((static_cast<uint32>(byte1) & 0x07) << 18) | 
                                   ((static_cast<uint32>(str[1]) & 0x3F) << 12) | 
                                   ((static_cast<uint32>(str[2]) & 0x3F) << 6) | 
                                   (static_cast<uint32>(str[3]) & 0x3F);

                    // Validation : plage Unicode valide (U+10000 à U+10FFFF)
                    if (outCodepoint < 0x10000 || outCodepoint > 0x10FFFF) {
                        return 0;
                    }

                    return 4;
                }

                // Pattern de byte initial invalide
                return 0;
            }

            // =====================================================================
            // IMPLÉMENTATION : ENCODAGE DE CODEPOINTS
            // =====================================================================

            usize NkEncodeChar(uint32 codepoint, char* outBuffer) {

                // Validation des paramètres d'entrée
                if (!outBuffer || !NkIsValidCodepoint(codepoint)) {
                    return 0;
                }

                // -----------------------------------------------------------------
                // Cas 1 : Codepoint ASCII (0x0000 - 0x007F) -> 1 byte
                // -----------------------------------------------------------------
                if (codepoint < 0x80) {
                    outBuffer[0] = static_cast<char>(codepoint);
                    return 1;
                }

                // -----------------------------------------------------------------
                // Cas 2 : Codepoint 2-byte (0x0080 - 0x07FF) -> 2 bytes
                // -----------------------------------------------------------------
                if (codepoint < 0x800) {
                    outBuffer[0] = static_cast<char>(0xC0 | (codepoint >> 6));
                    outBuffer[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
                    return 2;
                }

                // -----------------------------------------------------------------
                // Cas 3 : Codepoint 3-byte (0x0800 - 0xFFFF, excluant surrogates) -> 3 bytes
                // -----------------------------------------------------------------
                if (codepoint < 0x10000) {
                    outBuffer[0] = static_cast<char>(0xE0 | (codepoint >> 12));
                    outBuffer[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                    outBuffer[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
                    return 3;
                }

                // -----------------------------------------------------------------
                // Cas 4 : Codepoint 4-byte (0x10000 - 0x10FFFF) -> 4 bytes
                // -----------------------------------------------------------------
                outBuffer[0] = static_cast<char>(0xF0 | (codepoint >> 18));
                outBuffer[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
                outBuffer[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                outBuffer[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
                return 4;
            }

            // =====================================================================
            // IMPLÉMENTATION : ANALYSE DE LONGUEUR
            // =====================================================================

            usize NkCharLength(char firstByte) {

                // Cast vers unsigned pour manipulations bit-à-bit sûres
                uint8 byte = static_cast<uint8>(firstByte);

                // Détermination de la longueur via le pattern du premier byte
                if ((byte & 0x80) == 0) {
                    return 1;
                }
                if ((byte & 0xE0) == 0xC0) {
                    return 2;
                }
                if ((byte & 0xF0) == 0xE0) {
                    return 3;
                }
                if ((byte & 0xF8) == 0xF0) {
                    return 4;
                }

                // Byte initial invalide
                return 0;
            }

            usize NkCodepointLength(uint32 codepoint) {

                // Calcul de la longueur encodée selon la plage du codepoint
                if (codepoint < 0x80) {
                    return 1;
                }
                if (codepoint < 0x800) {
                    return 2;
                }
                if (codepoint < 0x10000) {
                    return 3;
                }
                if (codepoint <= 0x10FFFF) {
                    return 4;
                }

                // Codepoint hors de l'espace Unicode valide
                return 0;
            }

            // =====================================================================
            // IMPLÉMENTATION : PARCOURS ET COMPTAGE
            // =====================================================================

            usize NkCountChars(const char* str, usize byteLength) {

                // Gestion du cas de pointeur nul
                if (!str) {
                    return 0;
                }

                // Initialisation des compteurs
                usize count = 0;
                usize pos = 0;

                // Parcours séquentiel de la séquence UTF-8
                while (pos < byteLength) {

                    // Tentative de décodage du caractère courant
                    uint32 codepoint = 0;
                    usize bytes = NkDecodeChar(str + pos, codepoint);

                    // Arrêt en cas de séquence invalide
                    if (bytes == 0) {
                        break;
                    }

                    // Avancement et incrémentation du compteur
                    pos += bytes;
                    ++count;
                }

                return count;
            }

            const char* NkNextChar(const char* str) {

                // Validation des paramètres
                if (!str || *str == '\0') {
                    return str;
                }

                // Détermination de la longueur du caractère courant
                usize len = NkCharLength(*str);

                // Avancement sécurisé du pointeur
                return (len > 0) ? (str + len) : (str + 1);
            }

            const char* NkPrevChar(const char* str, const char* start) {

                // Validation des bornes
                if (!str || str <= start) {
                    return str;
                }

                // Recul d'un byte pour commencer l'analyse
                const char* ptr = str - 1;

                // Remonter les bytes de continuation vers le début du caractère
                while (ptr > start && NkIsContinuationByte(*ptr)) {
                    --ptr;
                }

                return ptr;
            }

            // =====================================================================
            // IMPLÉMENTATION : VALIDATION
            // =====================================================================

            bool NkIsValid(const char* str, usize length) {

                // Gestion du cas de pointeur nul
                if (!str) {
                    return false;
                }

                // Parcours complet de la séquence
                usize pos = 0;
                while (pos < length) {

                    // Tentative de décodage à la position courante
                    uint32 codepoint = 0;
                    usize bytes = NkDecodeChar(str + pos, codepoint);

                    // Échec immédiat en cas de séquence invalide
                    if (bytes == 0) {
                        return false;
                    }

                    // Avancement vers le caractère suivant
                    pos += bytes;
                }

                // Toute la séquence est valide
                return true;
            }

            bool NkIsValidCodepoint(uint32 codepoint) {

                // Validation : plage Unicode + exclusion des surrogates
                return codepoint <= 0x10FFFF && 
                       !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
            }

            // =====================================================================
            // IMPLÉMENTATION : CONVERSIONS VERS AUTRES ENCODAGES
            // =====================================================================

            NkConversionResult NkToUTF16(
                const char* src, usize srcLen, 
                uint16* dst, usize dstLen,
                usize& bytesRead, usize& charsWritten) {

                // Validation des pointeurs d'entrée
                if (!src || !dst) {
                    return NkConversionResult::NK_SOURCE_ILLEGAL;
                }

                // Initialisation des compteurs de progression
                bytesRead = 0;
                charsWritten = 0;

                // Boucle de conversion tant qu'il reste des données et de l'espace
                while (bytesRead < srcLen && charsWritten < dstLen) {

                    // Décodage du codepoint source UTF-8
                    uint32 codepoint = 0;
                    usize bytes = NkDecodeChar(src + bytesRead, codepoint);

                    // Gestion des erreurs de décodage source
                    if (bytes == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }

                    // Encodage du codepoint en UTF-16 via le module dédié
                    usize units = utf16::NkEncodeChar(codepoint, dst + charsWritten);
                    if (units == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }

                    // Vérification de la capacité du buffer destination
                    if (charsWritten + units > dstLen) {
                        return NkConversionResult::NK_TARGET_EXHAUSTED;
                    }

                    // Mise à jour des compteurs de progression
                    bytesRead += bytes;
                    charsWritten += units;
                }

                // Détermination du statut final
                return (bytesRead < srcLen) ? 
                       NkConversionResult::NK_SOURCE_EXHAUSTED : 
                       NkConversionResult::NK_SUCCESS;
            }

            NkConversionResult NkToUTF32(
                const char* src, usize srcLen, 
                uint32* dst, usize dstLen,
                usize& bytesRead, usize& charsWritten) {

                // Validation des pointeurs d'entrée
                if (!src || !dst) {
                    return NkConversionResult::NK_SOURCE_ILLEGAL;
                }

                // Initialisation des compteurs de progression
                bytesRead = 0;
                charsWritten = 0;

                // Boucle de conversion
                while (bytesRead < srcLen && charsWritten < dstLen) {

                    // Décodage du codepoint source
                    uint32 codepoint = 0;
                    usize bytes = NkDecodeChar(src + bytesRead, codepoint);

                    // Gestion des erreurs
                    if (bytes == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }

                    // Écriture directe en UTF-32 (1 codepoint = 1 uint32)
                    dst[charsWritten++] = codepoint;
                    bytesRead += bytes;
                }

                // Statut final
                return (bytesRead < srcLen) ? 
                       NkConversionResult::NK_SOURCE_EXHAUSTED : 
                       NkConversionResult::NK_SUCCESS;
            }

            // =====================================================================
            // IMPLÉMENTATION : CONVERSIONS DEPUIS AUTRES ENCODAGES
            // =====================================================================

            NkConversionResult NkFromUTF16(
                const uint16* src, usize srcLen, 
                char* dst, usize dstLen,
                usize& charsRead, usize& bytesWritten) {

                // Validation des pointeurs
                if (!src || !dst) {
                    return NkConversionResult::NK_SOURCE_ILLEGAL;
                }

                // Initialisation des compteurs
                charsRead = 0;
                bytesWritten = 0;

                // Boucle de conversion
                while (charsRead < srcLen && bytesWritten < dstLen) {

                    // Décodage du codepoint depuis UTF-16
                    uint32 codepoint = 0;
                    usize units = utf16::NkDecodeChar(src + charsRead, codepoint);

                    // Gestion des erreurs de décodage
                    if (units == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }

                    // Encodage temporaire en buffer local UTF-8
                    char buffer[4];
                    usize bytes = NkEncodeChar(codepoint, buffer);
                    if (bytes == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }

                    // Vérification de l'espace destination
                    if (bytesWritten + bytes > dstLen) {
                        return NkConversionResult::NK_TARGET_EXHAUSTED;
                    }

                    // Copie des bytes encodés vers la destination
                    for (usize i = 0; i < bytes; ++i) {
                        dst[bytesWritten++] = buffer[i];
                    }

                    // Avancement dans la source
                    charsRead += units;
                }

                // Statut final
                return (charsRead < srcLen) ? 
                       NkConversionResult::NK_SOURCE_EXHAUSTED : 
                       NkConversionResult::NK_SUCCESS;
            }

            NkConversionResult NkFromUTF32(
                const uint32* src, usize srcLen, 
                char* dst, usize dstLen,
                usize& charsRead, usize& bytesWritten) {

                // Validation des pointeurs
                if (!src || !dst) {
                    return NkConversionResult::NK_SOURCE_ILLEGAL;
                }

                // Initialisation des compteurs
                charsRead = 0;
                bytesWritten = 0;

                // Boucle de conversion
                while (charsRead < srcLen && bytesWritten < dstLen) {

                    // Encodage direct du codepoint UTF-32 vers UTF-8
                    char buffer[4];
                    usize bytes = NkEncodeChar(src[charsRead], buffer);

                    // Gestion des erreurs
                    if (bytes == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }
                    if (bytesWritten + bytes > dstLen) {
                        return NkConversionResult::NK_TARGET_EXHAUSTED;
                    }

                    // Copie vers la destination
                    for (usize i = 0; i < bytes; ++i) {
                        dst[bytesWritten++] = buffer[i];
                    }

                    // Avancement
                    ++charsRead;
                }

                // Statut final
                return (charsRead < srcLen) ? 
                       NkConversionResult::NK_SOURCE_EXHAUSTED : 
                       NkConversionResult::NK_SUCCESS;
            }

        } // namespace utf8

    } // namespace encoding

} // namespace nkentseu

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Validation en streaming de données réseau UTF-8
    // =====================================================================
    {
        class UTF8StreamValidator {
        public:
            bool FeedAndValidate(const char* chunk, usize chunkSize) {
                // Accumuler les données
                buffer_.insert(buffer_.end(), chunk, chunk + chunkSize);
                
                // Tenter de valider le buffer complet
                if (nkentseu::encoding::utf8::NkIsValid(buffer_.data(), buffer_.size())) {
                    // Buffer valide : traiter et vider
                    ProcessCompleteMessage(buffer_.data(), buffer_.size());
                    buffer_.clear();
                    return true;
                }
                
                // Buffer incomplet ou invalide partiellement
                // Conserver pour le prochain chunk (cas incomplet)
                // Ou rejeter si erreur détectée (cas illégal)
                return false;
            }
            
        private:
            std::vector<char> buffer_;
        };
    }

    // =====================================================================
    // Exemple 2 : Troncation sûre d'une chaîne UTF-8 à N caractères
    // =====================================================================
    {
        usize TruncateUTF8ToChars(const char* str, usize byteLimit, usize maxChars, 
                                  usize& outBytesUsed) {
            if (!str || maxChars == 0) {
                outBytesUsed = 0;
                return 0;
            }
            
            usize charCount = 0;
            usize bytePos = 0;
            
            while (bytePos < byteLimit && charCount < maxChars) {
                uint32 cp = 0;
                usize charBytes = nkentseu::encoding::utf8::NkDecodeChar(str + bytePos, cp);
                
                if (charBytes == 0) {
                    break;  // Séquence invalide : arrêter
                }
                
                bytePos += charBytes;
                ++charCount;
            }
            
            outBytesUsed = bytePos;
            return charCount;
        }
        
        // Usage : Tronquer "Hello 🌍 World" à 8 caractères max
        // Résultat : "Hello 🌍 W" (11 bytes, pas 8 bytes !)
    }

    // =====================================================================
    // Exemple 3 : Détection et correction d'encodages mal formés
    // =====================================================================
    {
        usize SanitizeUTF8(const char* src, usize srcLen, char* dst, usize dstLen) {
            usize srcPos = 0;
            usize dstPos = 0;
            
            while (srcPos < srcLen && dstPos < dstLen) {
                uint32 cp = 0;
                usize bytes = nkentseu::encoding::utf8::NkDecodeChar(src + srcPos, cp);
                
                if (bytes == 0) {
                    // Caractère invalide : remplacer par U+FFFD ()
                    if (dstPos + 3 <= dstLen) {
                        dst[dstPos++] = static_cast<char>(0xEF);
                        dst[dstPos++] = static_cast<char>(0xBF);
                        dst[dstPos++] = static_cast<char>(0xBD);
                    }
                    ++srcPos;  // Avancer d'un byte pour tenter de resynchroniser
                } else {
                    // Caractère valide : copier tel quel
                    usize toCopy = (bytes <= dstLen - dstPos) ? bytes : (dstLen - dstPos);
                    memcpy(dst + dstPos, src + srcPos, toCopy);
                    srcPos += bytes;
                    dstPos += toCopy;
                }
            }
            
            return dstPos;  // Nombre de bytes écrits dans dst
        }
    }
*/