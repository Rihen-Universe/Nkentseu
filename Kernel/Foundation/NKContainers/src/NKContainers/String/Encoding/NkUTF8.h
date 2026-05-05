// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkUTF8.h
// DESCRIPTION: Déclarations pour l'encodage/décodage UTF-8
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF8_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF8_H_INCLUDED

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
            // Sous-namespace dédié aux opérations UTF-8
            // -------------------------------------------------------------------------
            namespace utf8 {

                // =================================================================
                // FONCTIONS DE DÉCODAGE/ENCODAGE DE CODEPOINTS
                // =================================================================

                /**
                 * @brief Décode un codepoint Unicode à partir d'une séquence UTF-8
                 * 
                 * @param str Pointeur vers le début de la séquence UTF-8 à décoder
                 * @param outCodepoint Référence pour stocker le codepoint Unicode résultant
                 * @return Nombre de bytes consommés (0 si séquence invalide)
                 * 
                 * @note Gère les séquences de 1 à 4 bytes selon la spécification UTF-8
                 *       Rejette les sur-encodages et les points de code réservés
                 */
                NKENTSEU_CONTAINERS_API usize NkDecodeChar(const char* str, uint32& outCodepoint);

                /**
                 * @brief Encode un codepoint Unicode en séquence UTF-8
                 * 
                 * @param codepoint Codepoint Unicode à encoder (0x0000 - 0x10FFFF)
                 * @param outBuffer Pointeur vers le buffer de destination (min 4 bytes)
                 * @return Nombre de bytes écrits dans le buffer (0 si codepoint invalide)
                 * 
                 * @note Respect la norme UTF-8 : séquences minimales, pas de sur-encodage
                 */
                NKENTSEU_CONTAINERS_API usize NkEncodeChar(uint32 codepoint, char* outBuffer);

                // =================================================================
                // FONCTIONS D'ANALYSE DE LONGUEUR
                // =================================================================

                /**
                 * @brief Détermine la longueur attendue d'une séquence UTF-8 depuis son premier byte
                 * 
                 * @param firstByte Premier byte de la séquence à analyser
                 * @return Nombre de bytes attendus (1-4), ou 0 si byte de départ invalide
                 */
                NKENTSEU_CONTAINERS_API usize NkCharLength(char firstByte);

                /**
                 * @brief Calcule le nombre de bytes nécessaires pour encoder un codepoint en UTF-8
                 * 
                 * @param codepoint Codepoint Unicode à analyser
                 * @return Nombre de bytes requis (1-4), ou 0 si codepoint hors plage Unicode
                 */
                NKENTSEU_CONTAINERS_API usize NkCodepointLength(uint32 codepoint);

                // =================================================================
                // FONCTIONS DE PARCOURS ET COMPTAGE
                // =================================================================

                /**
                 * @brief Compte le nombre de caractères (codepoints) dans une séquence UTF-8
                 * 
                 * @param str Pointeur vers les données UTF-8
                 * @param byteLength Longueur totale en bytes de la séquence
                 * @return Nombre de codepoints valides rencontrés avant erreur ou fin
                 * 
                 * @note S'arrête au premier octet invalide sans lever d'exception
                 */
                NKENTSEU_CONTAINERS_API usize NkCountChars(const char* str, usize byteLength);

                /**
                 * @brief Avance le pointeur au début du prochain caractère UTF-8
                 * 
                 * @param str Pointeur vers le caractère courant
                 * @return Pointeur vers le caractère suivant, ou str si invalide
                 * 
                 * @note Ne dépasse pas les limites : vérifier la fin de chaîne en amont
                 */
                NKENTSEU_CONTAINERS_API const char* NkNextChar(const char* str);

                /**
                 * @brief Recule le pointeur au début du caractère UTF-8 précédent
                 * 
                 * @param str Pointeur vers une position dans la chaîne
                 * @param start Pointeur vers le début de la zone de texte (borne inférieure)
                 * @return Pointeur vers le début du caractère précédent
                 * 
                 * @note Parcourt les bytes de continuation (10xxxxxx) vers l'arrière
                 */
                NKENTSEU_CONTAINERS_API const char* NkPrevChar(const char* str, const char* start);

                // =================================================================
                // FONCTIONS INLINE - Prédicats utilitaires
                // =================================================================

                /**
                 * @brief Vérifie si un byte est un byte de continuation UTF-8 (10xxxxxx)
                 * 
                 * @param byte Byte à tester
                 * @return true si le byte correspond au motif de continuation
                 */
                NKENTSEU_FORCE_INLINE bool NkIsContinuationByte(char byte) {
                    return (static_cast<unsigned char>(byte) & 0xC0) == 0x80;
                }

                // =================================================================
                // FONCTIONS DE VALIDATION
                // =================================================================

                /**
                 * @brief Valide l'intégrité d'une séquence de bytes en UTF-8
                 * 
                 * @param str Pointeur vers les données à valider
                 * @param length Nombre de bytes à examiner
                 * @return true si toute la séquence est un UTF-8 bien formé
                 * 
                 * @note Vérifie : séquences complètes, pas de sur-encodage, 
                 *       pas de surrogates, codepoints dans la plage Unicode
                 */
                NKENTSEU_CONTAINERS_API bool NkIsValid(const char* str, usize length);

                /**
                 * @brief Vérifie si un codepoint est valide dans l'espace Unicode
                 * 
                 * @param codepoint Valeur à tester
                 * @return true si 0 <= codepoint <= 0x10FFFF et pas dans D800-DFFF
                 */
                NKENTSEU_CONTAINERS_API bool NkIsValidCodepoint(uint32 codepoint);

                // =================================================================
                // FONCTIONS DE CONVERSION VERS AUTRES ENCODAGES
                // =================================================================

                /**
                 * @brief Convertit une séquence UTF-8 vers UTF-16
                 * 
                 * @param src Pointeur vers la source UTF-8
                 * @param srcLen Longueur de la source en bytes
                 * @param dst Pointeur vers le buffer destination UTF-16
                 * @param dstLen Capacité du buffer destination en unités uint16
                 * @param bytesRead [out] Nombre de bytes lus depuis la source
                 * @param charsWritten [out] Nombre d'unités UTF-16 écrites
                 * @return Code de résultat indiquant le statut de la conversion
                 * 
                 * @note Gère les paires de surrogates pour les codepoints > 0xFFFF
                 */
                NKENTSEU_CONTAINERS_API NkConversionResult NkToUTF16(
                    const char* src, usize srcLen,
                    uint16* dst, usize dstLen,
                    usize& bytesRead, usize& charsWritten);

                /**
                 * @brief Convertit une séquence UTF-8 vers UTF-32
                 * 
                 * @param src Pointeur vers la source UTF-8
                 * @param srcLen Longueur de la source en bytes
                 * @param dst Pointeur vers le buffer destination UTF-32
                 * @param dstLen Capacité du buffer destination en unités uint32
                 * @param bytesRead [out] Nombre de bytes lus depuis la source
                 * @param charsWritten [out] Nombre de codepoints écrits
                 * @return Code de résultat indiquant le statut de la conversion
                 */
                NKENTSEU_CONTAINERS_API NkConversionResult NkToUTF32(
                    const char* src, usize srcLen,
                    uint32* dst, usize dstLen,
                    usize& bytesRead, usize& charsWritten);

                // =================================================================
                // FONCTIONS DE CONVERSION DEPUIS AUTRES ENCODAGES
                // =================================================================

                /**
                 * @brief Convertit une séquence UTF-16 vers UTF-8
                 * 
                 * @param src Pointeur vers la source UTF-16
                 * @param srcLen Longueur de la source en unités uint16
                 * @param dst Pointeur vers le buffer destination UTF-8
                 * @param dstLen Capacité du buffer destination en bytes
                 * @param charsRead [out] Nombre d'unités UTF-16 lues
                 * @param bytesWritten [out] Nombre de bytes UTF-8 écrits
                 * @return Code de résultat indiquant le statut de la conversion
                 */
                NKENTSEU_CONTAINERS_API NkConversionResult NkFromUTF16(
                    const uint16* src, usize srcLen,
                    char* dst, usize dstLen,
                    usize& charsRead, usize& bytesWritten);

                /**
                 * @brief Convertit une séquence UTF-32 vers UTF-8
                 * 
                 * @param src Pointeur vers la source UTF-32
                 * @param srcLen Longueur de la source en unités uint32
                 * @param dst Pointeur vers le buffer destination UTF-8
                 * @param dstLen Capacité du buffer destination en bytes
                 * @param charsRead [out] Nombre de codepoints lus
                 * @param bytesWritten [out] Nombre de bytes UTF-8 écrits
                 * @return Code de résultat indiquant le statut de la conversion
                 */
                NKENTSEU_CONTAINERS_API NkConversionResult NkFromUTF32(
                    const uint32* src, usize srcLen,
                    char* dst, usize dstLen,
                    usize& charsRead, usize& bytesWritten);

            } // namespace utf8

        } // namespace encoding

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF8_H_INCLUDED

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Décodage itératif d'une chaîne UTF-8
    // =====================================================================
    {
        void ProcessUTF8String(const char* utf8Data, usize byteLength) {
            usize pos = 0;
            
            while (pos < byteLength) {
                uint32 codepoint = 0;
                usize bytesConsumed = nkentseu::encoding::utf8::NkDecodeChar(
                    utf8Data + pos, codepoint);
                
                if (bytesConsumed == 0) {
                    // Séquence invalide : gérer l'erreur
                    LogError("Invalid UTF-8 sequence at position %zu", pos);
                    break;
                }
                
                // Traiter le codepoint décodé
                HandleUnicodeCodepoint(codepoint);
                
                pos += bytesConsumed;
            }
        }
    }

    // =====================================================================
    // Exemple 2 : Encodage d'un caractère Unicode spécial en UTF-8
    // =====================================================================
    {
        char buffer[5];  // 4 bytes max + null terminator pour sécurité
        uint32 emoji = 0x1F600;  // U+1F600 : 😀
        
        usize bytesWritten = nkentseu::encoding::utf8::NkEncodeChar(emoji, buffer);
        
        if (bytesWritten > 0) {
            buffer[bytesWritten] = '\0';  // Terminer la chaîne C
            printf("Encoded: %s\n", buffer);  // Affiche l'emoji
        }
    }

    // =====================================================================
    // Exemple 3 : Navigation bidirectionnelle dans un texte UTF-8
    // =====================================================================
    {
        const char* text = "Hello 🌍 World";
        const char* ptr = text + strlen(text);  // Pointeur à la fin
        
        // Reculer caractère par caractère (gère les emojis multi-bytes)
        while (ptr > text) {
            ptr = nkentseu::encoding::utf8::NkPrevChar(ptr, text);
            
            uint32 cp = 0;
            nkentseu::encoding::utf8::NkDecodeChar(ptr, cp);
            
            // Traiter le codepoint dans l'ordre inverse
            ProcessBackwards(cp);
        }
    }

    // =====================================================================
    // Exemple 4 : Conversion UTF-8 vers UTF-16 pour API Windows
    // =====================================================================
    {
        bool ConvertToWideString(const char* utf8Src, std::wstring& outWide) {
            usize srcLen = strlen(utf8Src);
            
            // Première passe : calculer la taille nécessaire
            usize bytesRead = 0, unitsWritten = 0;
            auto result = nkentseu::encoding::utf8::NkToUTF16(
                utf8Src, srcLen, nullptr, 0, bytesRead, unitsWritten);
            
            if (result != nkentseu::encoding::NkConversionResult::NK_TARGET_EXHAUSTED) {
                return false;  // Erreur de source
            }
            
            // Allouer le buffer exact et convertir
            std::vector<uint16_t> buffer(unitsWritten + 1);
            result = nkentseu::encoding::utf8::NkToUTF16(
                utf8Src, srcLen, buffer.data(), unitsWritten, 
                bytesRead, unitsWritten);
            
            if (result == nkentseu::encoding::NkConversionResult::NK_SUCCESS) {
                buffer[unitsWritten] = 0;  // Null-terminator
                outWide.assign(reinterpret_cast<wchar_t*>(buffer.data()), unitsWritten);
                return true;
            }
            return false;
        }
    }
*/