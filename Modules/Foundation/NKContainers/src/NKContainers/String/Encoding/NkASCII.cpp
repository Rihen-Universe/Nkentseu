// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkASCII.cpp
// DESCRIPTION: Implémentation des utilitaires de manipulation ASCII
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

// -------------------------------------------------------------------------
// Inclusion de l'en-tête correspondant
// -------------------------------------------------------------------------
#include "NkASCII.h"

// -------------------------------------------------------------------------
// Namespace principal
// -------------------------------------------------------------------------
namespace nkentseu {

    // -------------------------------------------------------------------------
    // Namespace encoding
    // -------------------------------------------------------------------------
    namespace encoding {

        // -------------------------------------------------------------------------
        // Sous-namespace ascii
        // -------------------------------------------------------------------------
        namespace ascii {

            // =====================================================================
            // IMPLÉMENTATION DES FONCTIONS PUBLIQUES
            // =====================================================================

            bool NkIsValid(const char* str, usize length) noexcept {

                // Vérification de la validité du pointeur d'entrée
                if (!str) {
                    return false;
                }

                // Parcours séquentiel de chaque caractère de la chaîne
                for (usize i = 0; i < length; ++i) {

                    // Test de conformité ASCII pour le caractère courant
                    if (!NkIsASCII(str[i])) {
                        return false;
                    }
                }

                // Tous les caractères sont valides : retour succès
                return true;
            }

            int32 NkCompareIgnoreCase(const char* lhs, const char* rhs, usize length) noexcept {

                // Gestion des cas de pointeurs nuls
                if (!lhs || !rhs) {
                    return (lhs == rhs) ? 0 : (lhs ? 1 : -1);
                }

                // Comparaison caractère par caractère en ignorant la casse
                for (usize i = 0; i < length; ++i) {

                    // Normalisation en minuscules pour comparaison équitable
                    char l = NkToLower(lhs[i]);
                    char r = NkToLower(rhs[i]);

                    // Détermination de l'ordre lexicographique
                    if (l < r) {
                        return -1;
                    }
                    if (l > r) {
                        return 1;
                    }
                }

                // Chaînes identiques sur la longueur spécifiée
                return 0;
            }

            bool NkEqualsIgnoreCase(const char* lhs, const char* rhs, usize length) noexcept {

                // Délégation à la fonction de comparaison pour réutilisation du code
                return NkCompareIgnoreCase(lhs, rhs, length) == 0;
            }

            void NkToUpperInPlace(char* str, usize length) noexcept {

                // Protection contre les pointeurs nuls
                if (!str) {
                    return;
                }

                // Conversion de chaque caractère en majuscule si applicable
                for (usize i = 0; i < length; ++i) {
                    str[i] = NkToUpper(str[i]);
                }
            }

            void NkToLowerInPlace(char* str, usize length) noexcept {

                // Protection contre les pointeurs nuls
                if (!str) {
                    return;
                }

                // Conversion de chaque caractère en minuscule si applicable
                for (usize i = 0; i < length; ++i) {
                    str[i] = NkToLower(str[i]);
                }
            }

        } // namespace ascii

    } // namespace encoding

} // namespace nkentseu

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Normalisation d'identifiants pour comparaison
    // =====================================================================
    {
        bool AreUsernamesEqual(const char* user1, const char* user2, usize len) {
            // Créer des copies temporaires pour la normalisation
            char temp1[256], temp2[256];
            
            strncpy(temp1, user1, len);
            strncpy(temp2, user2, len);
            
            // Normaliser en minuscules pour comparaison case-insensitive
            nkentseu::encoding::ascii::NkToLowerInPlace(temp1, len);
            nkentseu::encoding::ascii::NkToLowerInPlace(temp2, len);
            
            return memcmp(temp1, temp2, len) == 0;
        }
    }

    // =====================================================================
    // Exemple 2 : Validation stricte de tokens API
    // =====================================================================
    {
        bool IsValidApiToken(const char* token, usize length) {
            // Un token valide doit être : ASCII + imprimable + sans espaces
            if (!nkentseu::encoding::ascii::NkIsValid(token, length)) {
                return false;
            }
            
            for (usize i = 0; i < length; ++i) {
                char ch = token[i];
                
                // Rejeter les caractères de contrôle et les espaces
                if (nkentseu::encoding::ascii::NkIsControl(ch) || 
                    nkentseu::encoding::ascii::NkIsWhitespace(ch)) {
                    return false;
                }
                
                // Exiger des caractères imprimables
                if (!nkentseu::encoding::ascii::NkIsPrintable(ch)) {
                    return false;
                }
            }
            return true;
        }
    }

    // =====================================================================
    // Exemple 3 : Parsing de configuration case-insensitive
    // =====================================================================
    {
        bool ParseConfigKey(const char* key, usize length, ConfigOption& outOption) {
            // Comparaison avec les clés connues en ignorant la casse
            if (nkentseu::encoding::ascii::NkEqualsIgnoreCase(key, "debug", length)) {
                outOption = ConfigOption::DEBUG;
                return true;
            }
            if (nkentseu::encoding::ascii::NkEqualsIgnoreCase(key, "verbose", length)) {
                outOption = ConfigOption::VERBOSE;
                return true;
            }
            if (nkentseu::encoding::ascii::NkEqualsIgnoreCase(key, "silent", length)) {
                outOption = ConfigOption::SILENT;
                return true;
            }
            return false;  // Clé inconnue
        }
    }
*/