// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkASCII.h
// DESCRIPTION: Utilitaires de manipulation et validation de l'encodage ASCII
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKASCII_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKASCII_H_INCLUDED

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
            // Sous-namespace dédié aux opérations ASCII
            // -------------------------------------------------------------------------
            namespace ascii {

                // =================================================================
                // FONCTIONS INLINE - Prédicats de caractères
                // =================================================================

                /**
                 * @brief Vérifie si un caractère appartient à l'ensemble ASCII (0-127)
                 * 
                 * @param ch Caractère à tester
                 * @return true si ch < 128, false sinon
                 * 
                 * @note Utilise un cast unsigned pour éviter les problèmes de signe
                 */
                NKENTSEU_FORCE_INLINE bool NkIsASCII(char ch) noexcept {
                    return static_cast<unsigned char>(ch) < 128;
                }

                /**
                 * @brief Valide qu'une chaîne entière ne contient que des caractères ASCII
                 * 
                 * @param str Pointeur vers la chaîne à valider
                 * @param length Nombre de caractères à examiner
                 * @return true si tous les caractères sont ASCII, false sinon
                 */
                NKENTSEU_CORE_API bool NkIsValid(const char* str, usize length) noexcept;

                /**
                 * @brief Détermine si un caractère est un caractère de contrôle ASCII
                 * 
                 * @param ch Caractère à tester
                 * @return true pour les valeurs 0-31 et 127 (DEL)
                 */
                NKENTSEU_FORCE_INLINE bool NkIsControl(char ch) noexcept {
                    return (static_cast<unsigned char>(ch) <= 31) || 
                           (static_cast<unsigned char>(ch) == 127);
                }

                /**
                 * @brief Vérifie si un caractère est imprimable en ASCII
                 * 
                 * @param ch Caractère à tester
                 * @return true pour la plage 32-126 (espace à tilde)
                 */
                NKENTSEU_FORCE_INLINE bool NkIsPrintable(char ch) noexcept {
                    return static_cast<unsigned char>(ch) >= 32 && 
                           static_cast<unsigned char>(ch) <= 126;
                }

                /**
                 * @brief Identifie les caractères d'espacement ASCII
                 * 
                 * @param ch Caractère à tester
                 * @return true pour espace, tab, newline, carriage return, vertical tab, form feed
                 */
                NKENTSEU_FORCE_INLINE bool NkIsWhitespace(char ch) noexcept {
                    return ch == ' '  || 
                           ch == '\t' || 
                           ch == '\n' || 
                           ch == '\r' || 
                           ch == '\v' || 
                           ch == '\f';
                }

                /**
                 * @brief Vérifie si un caractère est un chiffre décimal (0-9)
                 * 
                 * @param ch Caractère à tester
                 * @return true si ch est entre '0' et '9' inclus
                 */
                NKENTSEU_FORCE_INLINE bool NkIsDigit(char ch) noexcept {
                    return ch >= '0' && ch <= '9';
                }

                /**
                 * @brief Vérifie si un caractère est une lettre alphabétique (A-Z, a-z)
                 * 
                 * @param ch Caractère à tester
                 * @return true pour les lettres majuscules ou minuscules ASCII
                 */
                NKENTSEU_FORCE_INLINE bool NkIsAlpha(char ch) noexcept {
                    return (ch >= 'A' && ch <= 'Z') || 
                           (ch >= 'a' && ch <= 'z');
                }

                /**
                 * @brief Vérifie si un caractère est alphanumérique (lettre ou chiffre)
                 * 
                 * @param ch Caractère à tester
                 * @return true si NkIsAlpha(ch) ou NkIsDigit(ch)
                 */
                NKENTSEU_FORCE_INLINE bool NkIsAlphaNumeric(char ch) noexcept {
                    return NkIsAlpha(ch) || NkIsDigit(ch);
                }

                /**
                 * @brief Vérifie si un caractère est une majuscule (A-Z)
                 * 
                 * @param ch Caractère à tester
                 * @return true pour la plage 'A' à 'Z'
                 */
                NKENTSEU_FORCE_INLINE bool NkIsUpper(char ch) noexcept {
                    return ch >= 'A' && ch <= 'Z';
                }

                /**
                 * @brief Vérifie si un caractère est une minuscule (a-z)
                 * 
                 * @param ch Caractère à tester
                 * @return true pour la plage 'a' à 'z'
                 */
                NKENTSEU_FORCE_INLINE bool NkIsLower(char ch) noexcept {
                    return ch >= 'a' && ch <= 'z';
                }

                /**
                 * @brief Vérifie si un caractère est un chiffre hexadécimal (0-9, A-F, a-f)
                 * 
                 * @param ch Caractère à tester
                 * @return true pour les caractères valides en notation hexadécimale
                 */
                NKENTSEU_FORCE_INLINE bool NkIsHexDigit(char ch) noexcept {
                    return NkIsDigit(ch) || 
                           (ch >= 'A' && ch <= 'F') || 
                           (ch >= 'a' && ch <= 'f');
                }

                /**
                 * @brief Vérifie si un caractère est un signe de ponctuation ASCII
                 * 
                 * @param ch Caractère à tester
                 * @return true pour les caractères de ponctuation dans les plages ASCII
                 */
                NKENTSEU_FORCE_INLINE bool NkIsPunctuation(char ch) noexcept {
                    return (static_cast<unsigned char>(ch) >= 33  && static_cast<unsigned char>(ch) <= 47)  ||
                           (static_cast<unsigned char>(ch) >= 58  && static_cast<unsigned char>(ch) <= 64)  ||
                           (static_cast<unsigned char>(ch) >= 91  && static_cast<unsigned char>(ch) <= 96)  ||
                           (static_cast<unsigned char>(ch) >= 123 && static_cast<unsigned char>(ch) <= 126);
                }

                // =================================================================
                // FONCTIONS INLINE - Conversions de casse
                // =================================================================

                /**
                 * @brief Convertit un caractère en majuscule si possible
                 * 
                 * @param ch Caractère à convertir
                 * @return Version majuscule si ch est minuscule, sinon ch inchangé
                 * 
                 * @note Opération bit-wise : soustraction de 32 (0x20) pour ASCII
                 */
                NKENTSEU_FORCE_INLINE char NkToUpper(char ch) noexcept {
                    return NkIsLower(ch) ? static_cast<char>(ch - 32) : ch;
                }

                /**
                 * @brief Convertit un caractère en minuscule si possible
                 * 
                 * @param ch Caractère à convertir
                 * @return Version minuscule si ch est majuscule, sinon ch inchangé
                 */
                NKENTSEU_FORCE_INLINE char NkToLower(char ch) noexcept {
                    return NkIsUpper(ch) ? static_cast<char>(ch + 32) : ch;
                }

                // =================================================================
                // FONCTIONS INLINE - Conversions numériques
                // =================================================================

                /**
                 * @brief Convertit un caractère chiffre en sa valeur numérique
                 * 
                 * @param ch Caractère chiffre ('0'-'9')
                 * @return Valeur entière 0-9, ou -1 si ch n'est pas un chiffre
                 */
                NKENTSEU_FORCE_INLINE int32 NkToDigit(char ch) noexcept {
                    return NkIsDigit(ch) ? static_cast<int32>(ch - '0') : -1;
                }

                /**
                 * @brief Convertit un caractère hexadécimal en sa valeur numérique
                 * 
                 * @param ch Caractère hex ('0'-'9', 'A'-'F', 'a'-'f')
                 * @return Valeur entière 0-15, ou -1 si invalide
                 */
                NKENTSEU_FORCE_INLINE int32 NkToHexDigit(char ch) noexcept {
                    if (ch >= '0' && ch <= '9') {
                        return static_cast<int32>(ch - '0');
                    }
                    if (ch >= 'A' && ch <= 'F') {
                        return static_cast<int32>(ch - 'A' + 10);
                    }
                    if (ch >= 'a' && ch <= 'f') {
                        return static_cast<int32>(ch - 'a' + 10);
                    }
                    return -1;
                }

                /**
                 * @brief Convertit une valeur numérique en caractère chiffre
                 * 
                 * @param digit Valeur entière à convertir (0-9)
                 * @return Caractère '0'-'9', ou '\\0' si digit hors plage
                 */
                NKENTSEU_FORCE_INLINE char NkFromDigit(int32 digit) noexcept {
                    return (digit >= 0 && digit <= 9) ? static_cast<char>('0' + digit) : '\0';
                }

                /**
                 * @brief Convertit une valeur hexadécimale en caractère correspondant
                 * 
                 * @param digit Valeur 0-15 à convertir
                 * @param uppercase true pour A-F, false pour a-f
                 * @return Caractère hexadécimal, ou '\\0' si digit hors plage
                 */
                NKENTSEU_FORCE_INLINE char NkFromHexDigit(int32 digit, bool uppercase = true) noexcept {
                    if (digit >= 0 && digit <= 9) {
                        return static_cast<char>('0' + digit);
                    }
                    if (digit >= 10 && digit <= 15) {
                        return uppercase ? 
                               static_cast<char>('A' + digit - 10) : 
                               static_cast<char>('a' + digit - 10);
                    }
                    return '\0';
                }

                // =================================================================
                // FONCTIONS PUBLIQUES - Comparaisons et transformations
                // =================================================================

                /**
                 * @brief Compare deux chaînes ASCII sans tenir compte de la casse
                 * 
                 * @param lhs Première chaîne à comparer
                 * @param rhs Deuxième chaîne à comparer
                 * @param length Nombre de caractères à comparer
                 * @return <0 si lhs<rhs, 0 si égal, >0 si lhs>rhs (ordre lexicographique)
                 */
                NKENTSEU_CORE_API int32 NkCompareIgnoreCase(const char* lhs, const char* rhs, usize length) noexcept;

                /**
                 * @brief Teste l'égalité de deux chaînes ASCII en ignorant la casse
                 * 
                 * @param lhs Première chaîne
                 * @param rhs Deuxième chaîne
                 * @param length Nombre de caractères à comparer
                 * @return true si les chaînes sont égales (case-insensitive)
                 */
                NKENTSEU_CORE_API bool NkEqualsIgnoreCase(const char* lhs, const char* rhs, usize length) noexcept;

                /**
                 * @brief Convertit une chaîne en majuscules in-place
                 * 
                 * @param str Chaîne à modifier (doit être mutable)
                 * @param length Nombre de caractères à convertir
                 * 
                 * @note Modifie directement le buffer passé en paramètre
                 */
                NKENTSEU_CORE_API void NkToUpperInPlace(char* str, usize length) noexcept;

                /**
                 * @brief Convertit une chaîne en minuscules in-place
                 * 
                 * @param str Chaîne à modifier (doit être mutable)
                 * @param length Nombre de caractères à convertir
                 */
                NKENTSEU_CORE_API void NkToLowerInPlace(char* str, usize length) noexcept;

            } // namespace ascii

        } // namespace encoding

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKASCII_H_INCLUDED

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Validation et nettoyage d'entrée utilisateur
    // =====================================================================
    {
        bool IsValidUsername(const char* username, usize length) {
            // Un nom d'utilisateur valide : ASCII, alphanumérique ou underscore
            for (usize i = 0; i < length; ++i) {
                char ch = username[i];
                if (!nkentseu::encoding::ascii::NkIsAlphaNumeric(ch) && ch != '_') {
                    return false;
                }
            }
            return nkentseu::encoding::ascii::NkIsValid(username, length);
        }
    }

    // =====================================================================
    // Exemple 2 : Comparaison de commandes case-insensitive
    // =====================================================================
    {
        bool IsCommand(const char* input, const char* expected) {
            usize inputLen = strlen(input);
            usize expectedLen = strlen(expected);
            
            if (inputLen != expectedLen) {
                return false;
            }
            
            return nkentseu::encoding::ascii::NkEqualsIgnoreCase(
                input, expected, inputLen);
        }
        
        // Usage : IsCommand("START", "start") retourne true
    }

    // =====================================================================
    // Exemple 3 : Formatage de données pour affichage
    // =====================================================================
    {
        void FormatDisplayCode(char* code, usize length) {
            // Normaliser : majuscules pour les codes alphanumériques
            nkentseu::encoding::ascii::NkToUpperInPlace(code, length);
            
            // Vérifier que le résultat reste imprimable
            for (usize i = 0; i < length; ++i) {
                if (!nkentseu::encoding::ascii::NkIsPrintable(code[i])) {
                    code[i] = '?';  // Remplacer les caractères problématiques
                }
            }
        }
    }

    // =====================================================================
    // Exemple 4 : Parsing de valeurs hexadécimales
    // =====================================================================
    {
        bool ParseHexValue(const char* hexStr, usize length, uint32& outValue) {
            outValue = 0;
            
            for (usize i = 0; i < length; ++i) {
                int32 digit = nkentseu::encoding::ascii::NkToHexDigit(hexStr[i]);
                if (digit < 0) {
                    return false;  // Caractère hex invalide
                }
                outValue = (outValue << 4) | static_cast<uint32>(digit);
            }
            return true;
        }
        
        // Usage : ParseHexValue("1A3F", 4, value) -> value = 0x1A3F
    }
*/