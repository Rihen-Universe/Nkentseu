// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkUTF16.h
// DESCRIPTION: Déclarations pour l'encodage/décodage UTF-16 avec gestion des surrogates
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF16_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF16_H_INCLUDED

    // -------------------------------------------------------------------------
    // Inclusion de l'en-tête de base pour les types d'encoding
    // -------------------------------------------------------------------------
    #include "NkEncoding.h"

    // -------------------------------------------------------------------------
    // Namespace principal
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // -------------------------------------------------------------------------
        // Namespace encoding
        // -------------------------------------------------------------------------
        namespace encoding {

            // -------------------------------------------------------------------------
            // Sous-namespace dédié aux opérations UTF-16
            // -------------------------------------------------------------------------
            namespace utf16 {

                // =================================================================
                // CONSTANTES - Plages de surrogates UTF-16
                // =================================================================

                /// Début de la plage des high surrogates (U+D800)
                constexpr uint16 NK_HIGH_SURROGATE_START = 0xD800;

                /// Fin de la plage des high surrogates (U+DBFF)
                constexpr uint16 NK_HIGH_SURROGATE_END = 0xDBFF;

                /// Début de la plage des low surrogates (U+DC00)
                constexpr uint16 NK_LOW_SURROGATE_START = 0xDC00;

                /// Fin de la plage des low surrogates (U+DFFF)
                constexpr uint16 NK_LOW_SURROGATE_END = 0xDFFF;

                // =================================================================
                // FONCTIONS INLINE - Prédicats de surrogates
                // =================================================================

                /**
                 * @brief Vérifie si une unité UTF-16 est un high surrogate
                 * 
                 * @param ch Valeur uint16 à tester
                 * @return true si ch est dans la plage 0xD800-0xDBFF
                 * 
                 * @note Les high surrogates doivent toujours être suivis d'un low surrogate
                 */
                NKENTSEU_FORCE_INLINE bool NkIsHighSurrogate(uint16 ch) {
                    return ch >= NK_HIGH_SURROGATE_START && ch <= NK_HIGH_SURROGATE_END;
                }

                /**
                 * @brief Vérifie si une unité UTF-16 est un low surrogate
                 * 
                 * @param ch Valeur uint16 à tester
                 * @return true si ch est dans la plage 0xDC00-0xDFFF
                 * 
                 * @note Les low surrogates ne sont valides qu'après un high surrogate
                 */
                NKENTSEU_FORCE_INLINE bool NkIsLowSurrogate(uint16 ch) {
                    return ch >= NK_LOW_SURROGATE_START && ch <= NK_LOW_SURROGATE_END;
                }

                /**
                 * @brief Vérifie si une unité UTF-16 est un surrogate (high ou low)
                 * 
                 * @param ch Valeur uint16 à tester
                 * @return true si ch est dans la plage réservée 0xD800-0xDFFF
                 */
                NKENTSEU_FORCE_INLINE bool NkIsSurrogate(uint16 ch) {
                    return ch >= NK_HIGH_SURROGATE_START && ch <= NK_LOW_SURROGATE_END;
                }

                // =================================================================
                // FONCTIONS DE DÉCODAGE/ENCODAGE DE CODEPOINTS
                // =================================================================

                /**
                 * @brief Décode un codepoint Unicode depuis une ou deux unités UTF-16
                 * 
                 * @param str Pointeur vers les unités UTF-16 à décoder
                 * @param outCodepoint Référence pour le codepoint Unicode résultant
                 * @return Nombre d'unités consommées (1 pour BMP, 2 pour surrogate pair, 0 si erreur)
                 * 
                 * @note Gère automatiquement les paires de surrogates pour U+10000 à U+10FFFF
                 */
                NKENTSEU_CORE_API usize NkDecodeChar(const uint16* str, uint32& outCodepoint);

                /**
                 * @brief Encode un codepoint Unicode en une ou deux unités UTF-16
                 * 
                 * @param codepoint Codepoint Unicode à encoder (0x0000 - 0x10FFFF)
                 * @param outBuffer Pointeur vers le buffer destination (min 2 unités)
                 * @return Nombre d'unités écrites (1 pour BMP, 2 pour supplementary, 0 si invalide)
                 * 
                 * @note Les codepoints dans la plage surrogate (D800-DFFF) sont rejetés
                 */
                NKENTSEU_CORE_API usize NkEncodeChar(uint32 codepoint, uint16* outBuffer);

                // =================================================================
                // FONCTIONS D'ANALYSE DE LONGUEUR
                // =================================================================

                /**
                 * @brief Détermine la longueur d'un caractère UTF-16 depuis sa première unité
                 * 
                 * @param firstUnit Première unité uint16 à analyser
                 * @return 1 pour BMP, 2 pour surrogate pair, 0 si unité invalide isolée
                 */
                NKENTSEU_CORE_API usize NkCharLength(uint16 firstUnit);

                /**
                 * @brief Calcule le nombre d'unités UTF-16 nécessaires pour un codepoint
                 * 
                 * @param codepoint Codepoint Unicode à analyser
                 * @return 1 si codepoint < 0x10000, 2 si >= 0x10000, 0 si hors plage valide
                 */
                NKENTSEU_CORE_API usize NkCodepointLength(uint32 codepoint);

                // =================================================================
                // FONCTIONS DE PARCOURS ET COMPTAGE
                // =================================================================

                /**
                 * @brief Compte le nombre de caractères (codepoints) dans une séquence UTF-16
                 * 
                 * @param str Pointeur vers les données UTF-16
                 * @param unitLength Nombre total d'unités uint16 dans la séquence
                 * @return Nombre de codepoints valides avant erreur ou fin de séquence
                 */
                NKENTSEU_CORE_API usize NkCountChars(const uint16* str, usize unitLength);

                /**
                 * @brief Avance au début du prochain caractère UTF-16
                 * 
                 * @param str Pointeur vers le caractère courant
                 * @return Pointeur vers le caractère suivant (str+1 ou str+2)
                 */
                NKENTSEU_CORE_API const uint16* NkNextChar(const uint16* str);

                /**
                 * @brief Recule au début du caractère UTF-16 précédent
                 * 
                 * @param str Pointeur vers une position dans la séquence
                 * @param start Pointeur vers le début de la zone (borne inférieure)
                 * @return Pointeur vers le début du caractère précédent
                 * 
                 * @note Gère le recul depuis un low surrogate vers son high surrogate associé
                 */
                NKENTSEU_CORE_API const uint16* NkPrevChar(const uint16* str, const uint16* start);

                // =================================================================
                // FONCTIONS DE VALIDATION
                // =================================================================

                /**
                 * @brief Valide l'intégrité d'une séquence d'unités UTF-16
                 * 
                 * @param str Pointeur vers les données à valider
                 * @param length Nombre d'unités uint16 à examiner
                 * @return true si toutes les paires de surrogates sont bien formées
                 * 
                 * @note Rejette : high surrogate isolé, low surrogate sans high précédent,
                 *       codepoints hors plage Unicode après décodage de paire
                 */
                NKENTSEU_CORE_API bool NkIsValid(const uint16* str, usize length);

                // =================================================================
                // FONCTIONS DE CONVERSION VERS AUTRES ENCODAGES
                // =================================================================

                /**
                 * @brief Convertit une séquence UTF-16 vers UTF-8
                 * 
                 * @param src Pointeur vers la source UTF-16
                 * @param srcLen Longueur source en unités uint16
                 * @param dst Pointeur vers le buffer destination UTF-8
                 * @param dstLen Capacité destination en bytes
                 * @param charsRead [out] Unités UTF-16 effectivement lues
                 * @param bytesWritten [out] Bytes UTF-8 effectivement écrits
                 * @return Code de résultat de la conversion
                 */
                NKENTSEU_CORE_API NkConversionResult NkToUTF8(
                    const uint16* src, usize srcLen,
                    char* dst, usize dstLen,
                    usize& charsRead, usize& bytesWritten);

                /**
                 * @brief Convertit une séquence UTF-16 vers UTF-32
                 * 
                 * @param src Pointeur vers la source UTF-16
                 * @param srcLen Longueur source en unités uint16
                 * @param dst Pointeur vers le buffer destination UTF-32
                 * @param dstLen Capacité destination en unités uint32
                 * @param charsRead [out] Unités UTF-16 lues
                 * @param unitsWritten [out] Codepoints UTF-32 écrits
                 * @return Code de résultat de la conversion
                 */
                NKENTSEU_CORE_API NkConversionResult NkToUTF32(
                    const uint16* src, usize srcLen,
                    uint32* dst, usize dstLen,
                    usize& charsRead, usize& unitsWritten);

            } // namespace utf16

        } // namespace encoding

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF16_H_INCLUDED

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Décodage d'une chaîne UTF-16 avec emojis
    // =====================================================================
    {
        void ProcessUTF16String(const uint16_t* utf16Data, usize unitCount) {
            usize pos = 0;
            
            while (pos < unitCount) {
                uint32 codepoint = 0;
                usize unitsConsumed = nkentseu::encoding::utf16::NkDecodeChar(
                    utf16Data + pos, codepoint);
                
                if (unitsConsumed == 0) {
                    // Paire de surrogate mal formée
                    LogError("Invalid UTF-16 sequence at position %zu", pos);
                    break;
                }
                
                // Traiter le codepoint (peut être > 0xFFFF pour les emojis)
                HandleUnicodeCodepoint(codepoint);
                
                pos += unitsConsumed;
            }
        }
    }

    // =====================================================================
    // Exemple 2 : Encodage d'un codepoint supplementary plane en UTF-16
    // =====================================================================
    {
        uint16_t buffer[3];  // 2 unités max + sentinel pour sécurité
        uint32_t musicalSymbol = 0x1D11E;  // U+1D11E : 𝄞 (clef de sol)
        
        usize unitsWritten = nkentseu::encoding::utf16::NkEncodeChar(
            musicalSymbol, buffer);
        
        if (unitsWritten == 2) {
            // Vérifier la paire de surrogates
            assert(nkentseu::encoding::utf16::NkIsHighSurrogate(buffer[0]));
            assert(nkentseu::encoding::utf16::NkIsLowSurrogate(buffer[1]));
            
            // buffer contient maintenant la représentation UTF-16 valide
        }
    }

    // =====================================================================
    // Exemple 3 : Validation avant passage à une API Windows (wchar_t*)
    // =====================================================================
    {
        bool IsSafeWideString(const wchar_t* wideStr, usize length) {
            // Sur Windows, wchar_t est généralement UTF-16 (2 bytes)
            #if defined(_WIN32) || defined(_WIN64)
                return nkentseu::encoding::utf16::NkIsValid(
                    reinterpret_cast<const uint16_t*>(wideStr), length);
            #else
                // Sur Unix, wchar_t est souvent UTF-32 : utiliser NkIsValidUTF32
                return nkentseu::encoding::NkIsValidUTF32(
                    reinterpret_cast<const uint32_t*>(wideStr), length);
            #endif
        }
    }

    // =====================================================================
    // Exemple 4 : Conversion UTF-16 vers UTF-8 pour stockage ou réseau
    // =====================================================================
    {
        std::string UTF16ToUTF8(const uint16_t* src, usize srcUnits) {
            // Première passe : calculer la taille UTF-8 nécessaire
            usize charsRead = 0, bytesWritten = 0;
            auto result = nkentseu::encoding::utf16::NkToUTF8(
                src, srcUnits, nullptr, 0, charsRead, bytesWritten);
            
            if (result != nkentseu::encoding::NkConversionResult::NK_TARGET_EXHAUSTED) {
                return {};  // Erreur de source
            }
            
            // Allocation exacte et conversion réelle
            std::string utf8Result(bytesWritten, '\0');
            result = nkentseu::encoding::utf16::NkToUTF8(
                src, srcUnits, utf8Result.data(), bytesWritten,
                charsRead, bytesWritten);
            
            return (result == nkentseu::encoding::NkConversionResult::NK_SUCCESS) 
                   ? utf8Result 
                   : std::string{};
        }
    }
*/