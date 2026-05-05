// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkEncoding.cpp
// DESCRIPTION: Implémentation des fonctions de base pour la détection et validation d'encodage
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Inclusions locales
// -------------------------------------------------------------------------
#include "NkEncoding.h"
#include "NkASCII.h"
#include "NkUTF8.h"
#include "NkUTF16.h"
#include "NkUTF32.h"

// -------------------------------------------------------------------------
// Namespace principal
// -------------------------------------------------------------------------
namespace nkentseu {

    // -------------------------------------------------------------------------
    // Sous-namespace encoding
    // -------------------------------------------------------------------------
    namespace encoding {

        // =====================================================================
        // CONSTANTES INTERNES - Signatures BOM
        // =====================================================================

        /// Signature BOM pour UTF-8 (3 bytes : EF BB BF)
        static const uint8 NK_UTF8_BOM[] = {0xEF, 0xBB, 0xBF};

        /// Signature BOM pour UTF-16 Little Endian (2 bytes : FF FE)
        static const uint8 NK_UTF16LE_BOM[] = {0xFF, 0xFE};

        /// Signature BOM pour UTF-16 Big Endian (2 bytes : FE FF)
        static const uint8 NK_UTF16BE_BOM[] = {0xFE, 0xFF};

        /// Signature BOM pour UTF-32 Little Endian (4 bytes : FF FE 00 00)
        static const uint8 NK_UTF32LE_BOM[] = {0xFF, 0xFE, 0x00, 0x00};

        /// Signature BOM pour UTF-32 Big Endian (4 bytes : 00 00 FE FF)
        static const uint8 NK_UTF32BE_BOM[] = {0x00, 0x00, 0xFE, 0xFF};

        // =====================================================================
        // IMPLÉMENTATION DES FONCTIONS PUBLIQUES
        // =====================================================================

        NkEncodingType NkDetectBOM(const void* data, usize size) {

            // Vérification des paramètres d'entrée
            if (!data || size == 0) {
                return NkEncodingType::NK_UNKNOWN;
            }

            // Cast sécurisé vers tableau de bytes pour analyse
            const uint8* bytes = static_cast<const uint8*>(data);

            // -----------------------------------------------------------------
            // Détection UTF-32 (priorité : signature la plus longue - 4 bytes)
            // -----------------------------------------------------------------
            if (size >= 4) {

                // Vérification UTF-32LE : FF FE 00 00
                if (bytes[0] == 0xFF && 
                    bytes[1] == 0xFE && 
                    bytes[2] == 0x00 && 
                    bytes[3] == 0x00) {
                    return NkEncodingType::NK_UTF32LE;
                }

                // Vérification UTF-32BE : 00 00 FE FF
                if (bytes[0] == 0x00 && 
                    bytes[1] == 0x00 && 
                    bytes[2] == 0xFE && 
                    bytes[3] == 0xFF) {
                    return NkEncodingType::NK_UTF32BE;
                }
            }

            // -----------------------------------------------------------------
            // Détection UTF-8 (signature 3 bytes)
            // -----------------------------------------------------------------
            if (size >= 3) {

                // Vérification UTF-8 : EF BB BF
                if (bytes[0] == 0xEF && 
                    bytes[1] == 0xBB && 
                    bytes[2] == 0xBF) {
                    return NkEncodingType::NK_UTF8;
                }
            }

            // -----------------------------------------------------------------
            // Détection UTF-16 (signature 2 bytes)
            // -----------------------------------------------------------------
            if (size >= 2) {

                // Vérification UTF-16LE : FF FE
                if (bytes[0] == 0xFF && bytes[1] == 0xFE) {
                    return NkEncodingType::NK_UTF16LE;
                }

                // Vérification UTF-16BE : FE FF
                if (bytes[0] == 0xFE && bytes[1] == 0xFF) {
                    return NkEncodingType::NK_UTF16BE;
                }
            }

            // Aucun BOM reconnu : retour inconnu
            return NkEncodingType::NK_UNKNOWN;
        }

        bool NkIsValidASCII(const char* str, usize length) {

            // Délégation à l'implémentation spécialisée ASCII
            return ascii::NkIsValid(str, length);
        }

        bool NkIsValidUTF8(const char* str, usize length) {

            // Délégation à l'implémentation spécialisée UTF-8
            return utf8::NkIsValid(str, length);
        }

        bool NkIsValidUTF16(const uint16* str, usize length) {

            // Délégation à l'implémentation spécialisée UTF-16
            return utf16::NkIsValid(str, length);
        }

        bool NkIsValidUTF32(const uint32* str, usize length) {

            // Délégation à l'implémentation spécialisée UTF-32
            return utf32::NkIsValid(str, length);
        }

    } // namespace encoding

} // namespace nkentseu

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple : Chaîne de détection et validation
    // =====================================================================
    {
        // Données reçues d'une source externe (fichier, réseau, etc.)
        const void* rawData = GetExternalData();
        usize dataSize = GetExternalDataSize();

        // Étape 1 : Détecter l'encodage via BOM
        auto encoding = nkentseu::encoding::NkDetectBOM(rawData, dataSize);

        // Étape 2 : Adapter le traitement selon l'encodage détecté
        switch (encoding) {
            case nkentseu::encoding::NkEncodingType::NK_UTF8:
            case nkentseu::encoding::NkEncodingType::NK_ASCII: {
                const char* text = static_cast<const char*>(rawData) + 
                    (encoding == nkentseu::encoding::NkEncodingType::NK_UTF8 ? 3 : 0);
                usize len = dataSize - (encoding == nkentseu::encoding::NkEncodingType::NK_UTF8 ? 3 : 0);
                
                if (nkentseu::encoding::NkIsValidUTF8(text, len)) {
                    ProcessText(text, len);
                }
                break;
            }
            
            case nkentseu::encoding::NkEncodingType::NK_UTF16LE: {
                const uint16* text = static_cast<const uint16*>(
                    static_cast<const uint8*>(rawData) + 2);
                usize len = (dataSize - 2) / sizeof(uint16);
                
                if (nkentseu::encoding::NkIsValidUTF16(text, len)) {
                    ProcessUTF16Text(text, len);
                }
                break;
            }
            
            default:
                // Fallback : tenter d'interpréter comme UTF-8 sans BOM
                if (nkentseu::encoding::NkIsValidUTF8(
                        static_cast<const char*>(rawData), dataSize)) {
                    ProcessText(static_cast<const char*>(rawData), dataSize);
                } else {
                    LogWarning("Unknown or invalid encoding");
                }
                break;
        }
    }
*/