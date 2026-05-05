// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkUTF16.cpp
// DESCRIPTION: Implémentation des fonctions d'encodage/décodage UTF-16
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Inclusions locales
// -------------------------------------------------------------------------
#include "NkUTF16.h"
#include "NkUTF8.h"
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
        // Sous-namespace utf16
        // -------------------------------------------------------------------------
        namespace utf16 {

            // =====================================================================
            // IMPLÉMENTATION : DÉCODAGE DE CODEPOINTS
            // =====================================================================

            usize NkDecodeChar(const uint16* str, uint32& outCodepoint) {

                // Validation du pointeur d'entrée
                if (!str) {
                    return 0;
                }

                // Lecture de la première unité UTF-16
                uint16 unit1 = str[0];

                // -----------------------------------------------------------------
                // Cas 1 : Plan Multilingue de Base (BMP) - 0x0000 à 0xFFFF
                // -----------------------------------------------------------------
                // Exclut la plage des surrogates qui a un traitement spécial
                if (!NkIsSurrogate(unit1)) {
                    outCodepoint = static_cast<uint32>(unit1);
                    return 1;
                }

                // -----------------------------------------------------------------
                // Cas 2 : Paire de surrogates pour les plans supplémentaires
                // -----------------------------------------------------------------
                // Un high surrogate doit être suivi d'un low surrogate valide
                if (NkIsHighSurrogate(unit1)) {

                    // Lecture de l'unité suivante attendue comme low surrogate
                    uint16 unit2 = str[1];

                    // Validation du low surrogate
                    if (!NkIsLowSurrogate(unit2)) {
                        return 0;  // High surrogate orphelin : séquence invalide
                    }

                    // Calcul du codepoint selon la formule UTF-16
                    // Codepoint = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00)
                    outCodepoint = 0x10000 + 
                                   ((static_cast<uint32>(unit1 & 0x3FF)) << 10) + 
                                   static_cast<uint32>(unit2 & 0x3FF);

                    // Une paire de surrogates consomme 2 unités uint16
                    return 2;
                }

                // -----------------------------------------------------------------
                // Cas 3 : Low surrogate isolé (erreur de séquence)
                // -----------------------------------------------------------------
                // Un low surrogate ne peut jamais apparaître seul
                return 0;
            }

            // =====================================================================
            // IMPLÉMENTATION : ENCODAGE DE CODEPOINTS
            // =====================================================================

            usize NkEncodeChar(uint32 codepoint, uint16* outBuffer) {

                // Validation du buffer de sortie
                if (!outBuffer) {
                    return 0;
                }

                // -----------------------------------------------------------------
                // Cas 1 : Codepoint dans le BMP (0x0000 - 0xFFFF)
                // -----------------------------------------------------------------
                if (codepoint < 0x10000) {

                    // Rejet explicite des valeurs dans la plage surrogate
                    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        return 0;
                    }

                    // Encodage direct : 1 codepoint = 1 unité UTF-16
                    outBuffer[0] = static_cast<uint16>(codepoint);
                    return 1;
                }

                // -----------------------------------------------------------------
                // Cas 2 : Codepoint dans les plans supplémentaires (0x10000 - 0x10FFFF)
                // -----------------------------------------------------------------
                // Rejet des codepoints au-delà de l'espace Unicode valide
                if (codepoint > 0x10FFFF) {
                    return 0;
                }

                // Calcul de la paire de surrogates selon l'algorithme UTF-16
                // Étape 1 : Soustraire l'offset de base 0x10000
                codepoint -= 0x10000;

                // Étape 2 : Extraire les 10 bits de poids fort pour le high surrogate
                outBuffer[0] = static_cast<uint16>(
                    0xD800 + ((codepoint >> 10) & 0x3FF));

                // Étape 3 : Extraire les 10 bits de poids faible pour le low surrogate
                outBuffer[1] = static_cast<uint16>(
                    0xDC00 + (codepoint & 0x3FF));

                // Une paire de surrogates produit 2 unités uint16
                return 2;
            }

            // =====================================================================
            // IMPLÉMENTATION : ANALYSE DE LONGUEUR
            // =====================================================================

            usize NkCharLength(uint16 firstUnit) {

                // Les unités non-surrogate représentent un caractère complet
                if (!NkIsSurrogate(firstUnit)) {
                    return 1;
                }

                // Les high surrogates introduisent une paire (2 unités)
                if (NkIsHighSurrogate(firstUnit)) {
                    return 2;
                }

                // Les low surrogates isolés sont invalides
                return 0;
            }

            usize NkCodepointLength(uint32 codepoint) {

                // Codepoints du BMP : encodage direct en 1 unité
                if (codepoint < 0x10000) {

                    // Exclusion de la plage surrogate qui n'est pas encodable directement
                    if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
                        return 0;
                    }

                    return 1;
                }

                // Plans supplémentaires : nécessitent une paire de surrogates
                if (codepoint <= 0x10FFFF) {
                    return 2;
                }

                // Hors de l'espace Unicode défini
                return 0;
            }

            // =====================================================================
            // IMPLÉMENTATION : PARCOURS ET COMPTAGE
            // =====================================================================

            usize NkCountChars(const uint16* str, usize unitLength) {

                // Gestion du cas de pointeur nul
                if (!str) {
                    return 0;
                }

                // Initialisation des compteurs
                usize count = 0;
                usize pos = 0;

                // Parcours séquentiel des unités UTF-16
                while (pos < unitLength) {

                    // Tentative de décodage à la position courante
                    uint32 codepoint = 0;
                    usize units = NkDecodeChar(str + pos, codepoint);

                    // Arrêt en cas de séquence invalide
                    if (units == 0) {
                        break;
                    }

                    // Avancement et comptage
                    pos += units;
                    ++count;
                }

                return count;
            }

            const uint16* NkNextChar(const uint16* str) {

                // Validation du pointeur
                if (!str) {
                    return str;
                }

                // Détermination de la longueur du caractère courant
                usize len = NkCharLength(*str);

                // Avancement sécurisé
                return (len > 0) ? (str + len) : (str + 1);
            }

            const uint16* NkPrevChar(const uint16* str, const uint16* start) {

                // Validation des bornes
                if (!str || str <= start) {
                    return str;
                }

                // Recul d'une unité pour analyse
                const uint16* ptr = str - 1;

                // Si on est sur un low surrogate, reculer vers son high surrogate
                if (ptr > start && 
                    NkIsLowSurrogate(*ptr) && 
                    NkIsHighSurrogate(*(ptr - 1))) {
                    --ptr;
                }

                return ptr;
            }

            // =====================================================================
            // IMPLÉMENTATION : VALIDATION
            // =====================================================================

            bool NkIsValid(const uint16* str, usize length) {

                // Gestion du pointeur nul
                if (!str) {
                    return false;
                }

                // Parcours complet de la séquence
                usize pos = 0;
                while (pos < length) {

                    // Tentative de décodage
                    uint32 codepoint = 0;
                    usize units = NkDecodeChar(str + pos, codepoint);

                    // Échec en cas de séquence mal formée
                    if (units == 0) {
                        return false;
                    }

                    // Avancement
                    pos += units;
                }

                // Toute la séquence est valide
                return true;
            }

            // =====================================================================
            // IMPLÉMENTATION : CONVERSIONS VERS AUTRES ENCODAGES
            // =====================================================================

            NkConversionResult NkToUTF8(
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
                    usize units = NkDecodeChar(src + charsRead, codepoint);

                    // Gestion des erreurs de décodage
                    if (units == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }

                    // Encodage temporaire en UTF-8 via le module dédié
                    char buffer[4];
                    usize bytes = utf8::NkEncodeChar(codepoint, buffer);
                    if (bytes == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }

                    // Vérification de l'espace destination
                    if (bytesWritten + bytes > dstLen) {
                        return NkConversionResult::NK_TARGET_EXHAUSTED;
                    }

                    // Copie des bytes encodés
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

            NkConversionResult NkToUTF32(
                const uint16* src, usize srcLen, 
                uint32* dst, usize dstLen,
                usize& charsRead, usize& unitsWritten) {

                // Validation des pointeurs
                if (!src || !dst) {
                    return NkConversionResult::NK_SOURCE_ILLEGAL;
                }

                // Initialisation des compteurs
                charsRead = 0;
                unitsWritten = 0;

                // Boucle de conversion
                while (charsRead < srcLen && unitsWritten < dstLen) {

                    // Décodage du codepoint
                    uint32 codepoint = 0;
                    usize units = NkDecodeChar(src + charsRead, codepoint);

                    // Gestion des erreurs
                    if (units == 0) {
                        return NkConversionResult::NK_SOURCE_ILLEGAL;
                    }

                    // Écriture directe en UTF-32
                    dst[unitsWritten++] = codepoint;
                    charsRead += units;
                }

                // Statut final
                return (charsRead < srcLen) ? 
                       NkConversionResult::NK_SOURCE_EXHAUSTED : 
                       NkConversionResult::NK_SUCCESS;
            }

        } // namespace utf16

    } // namespace encoding

} // namespace nkentseu

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Gestion robuste des surrogates dans un parser JSON
    // =====================================================================
    {
        bool ParseJSONString(const uint16_t* jsonStr, usize length, std::u32string& out) {
            usize pos = 0;
            
            while (pos < length) {
                uint32 cp = 0;
                usize units = nkentseu::encoding::utf16::NkDecodeChar(jsonStr + pos, cp);
                
                if (units == 0) {
                    // Paire de surrogate invalide dans le JSON
                    return false;
                }
                
                // Gestion des échappements JSON (\uXXXX)
                if (cp == '\\' && pos + 1 < length && jsonStr[pos + 1] == 'u') {
                    // Parser l'échappement Unicode...
                    // (logique spécifique au parser JSON)
                }
                
                out.push_back(cp);
                pos += units;
            }
            return true;
        }
    }

    // =====================================================================
    // Exemple 2 : Conversion sûre entre std::u16string et std::string
    // =====================================================================
    {
        class EncodingConverter {
        public:
            static std::string UTF16ToUTF8(const std::u16string& src) {
                if (src.empty()) {
                    return {};
                }
                
                // Pré-calcul de la taille UTF-8 nécessaire
                usize srcLen = src.length();
                usize charsRead = 0, bytesWritten = 0;
                
                auto result = nkentseu::encoding::utf16::NkToUTF8(
                    src.data(), srcLen, nullptr, 0, charsRead, bytesWritten);
                
                if (result != nkentseu::encoding::NkConversionResult::NK_TARGET_EXHAUSTED) {
                    throw std::runtime_error("Invalid UTF-16 source");
                }
                
                // Conversion réelle
                std::string dst(bytesWritten, '\0');
                result = nkentseu::encoding::utf16::NkToUTF8(
                    src.data(), srcLen, dst.data(), bytesWritten,
                    charsRead, bytesWritten);
                
                if (result != nkentseu::encoding::NkConversionResult::NK_SUCCESS) {
                    throw std::runtime_error("UTF-16 to UTF-8 conversion failed");
                }
                
                return dst;
            }
        };
    }

    // =====================================================================
    // Exemple 3 : Détection et correction de paires de surrogates inversées
    // =====================================================================
    {
        usize FixSwappedSurrogates(uint16_t* str, usize length) {
            usize fixed = 0;
            
            for (usize i = 0; i + 1 < length; ++i) {
                // Détecter un low surrogate suivi d'un high surrogate (ordre inversé)
                if (nkentseu::encoding::utf16::NkIsLowSurrogate(str[i]) &&
                    nkentseu::encoding::utf16::NkIsHighSurrogate(str[i + 1])) {
                    
                    // Échanger les deux unités pour restaurer l'ordre correct
                    std::swap(str[i], str[i + 1]);
                    ++fixed;
                    ++i;  // Sauter la paire corrigée
                }
            }
            
            return fixed;  // Nombre de paires corrigées
        }
    }
*/