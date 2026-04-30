// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkUTF32.h
// DESCRIPTION: Déclarations pour l'encodage/décodage UTF-32 (format à largeur fixe)
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF32_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF32_H_INCLUDED

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
            // Sous-namespace dédié aux opérations UTF-32
            // -------------------------------------------------------------------------
            namespace utf32 {

                // =================================================================
                // CONSTANTES - Limites de l'espace Unicode
                // =================================================================

                /// Valeur maximale valide pour un codepoint Unicode (U+10FFFF)
                constexpr uint32 NK_MAX_UNICODE = 0x10FFFF;

                // =================================================================
                // FONCTIONS INLINE - Validation et utilitaires
                // =================================================================

                /**
                 * @brief Vérifie si un codepoint est valide dans l'espace Unicode
                 * 
                 * @param codepoint Valeur uint32 à tester
                 * @return true si 0 <= codepoint <= 0x10FFFF et pas dans D800-DFFF
                 * 
                 * @note Exclut explicitement la plage des surrogates UTF-16 qui n'a
                 *       pas de sémantique en UTF-32
                 */
                NKENTSEU_FORCE_INLINE bool NkIsValidCodepoint(uint32 codepoint) {
                    return codepoint <= NK_MAX_UNICODE && 
                           !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
                }

                /**
                 * @brief Retourne la longueur fixe d'un caractère UTF-32
                 * 
                 * @param codepoint Codepoint (paramètre ignoré, présent pour uniformité API)
                 * @return Toujours 1 : UTF-32 utilise 1 uint32 par codepoint
                 * 
                 * @note Cette fonction existe pour compatibilité avec l'API générique
                 *       des autres modules d'encodage
                 */
                NKENTSEU_FORCE_INLINE usize NkCharLength(uint32) {
                    return 1;
                }

                /**
                 * @brief Compte les caractères dans une séquence UTF-32
                 * 
                 * @param str Pointeur vers les données (paramètre ignoré)
                 * @param length Nombre d'unités uint32 dans la séquence
                 * @return Retourne directement length : 1 unité = 1 codepoint
                 * 
                 * @note En UTF-32, le comptage est trivial car l'encodage est à largeur fixe
                 */
                NKENTSEU_FORCE_INLINE usize NkCountChars(const uint32*, usize length) {
                    return length;
                }

                /**
                 * @brief Avance au prochain caractère UTF-32
                 * 
                 * @param str Pointeur vers le caractère courant
                 * @return Pointeur vers le caractère suivant (str + 1)
                 */
                NKENTSEU_FORCE_INLINE const uint32* NkNextChar(const uint32* str) {
                    return str + 1;
                }

                /**
                 * @brief Recule au caractère UTF-32 précédent
                 * 
                 * @param str Pointeur vers une position dans la séquence
                 * @param start Paramètre ignoré (présent pour uniformité API)
                 * @return Pointeur vers le caractère précédent (str - 1)
                 */
                NKENTSEU_FORCE_INLINE const uint32* NkPrevChar(const uint32* str, const uint32*) {
                    return str - 1;
                }

                // =================================================================
                // FONCTIONS DE VALIDATION
                // =================================================================

                /**
                 * @brief Valide qu'une séquence UTF-32 ne contient que des codepoints valides
                 * 
                 * @param str Pointeur vers les données à valider
                 * @param length Nombre de codepoints (unités uint32) à examiner
                 * @return true si tous les codepoints sont dans la plage Unicode valide
                 * 
                 * @note Vérifie : valeur <= 0x10FFFF et exclusion de la plage surrogate
                 */
                NKENTSEU_CONTAINERS_API bool NkIsValid(const uint32* str, usize length);

                // =================================================================
                // FONCTIONS DE CONVERSION VERS AUTRES ENCODAGES
                // =================================================================

                /**
                 * @brief Convertit une séquence UTF-32 vers UTF-8
                 * 
                 * @param src Pointeur vers la source UTF-32
                 * @param srcLen Longueur source en unités uint32
                 * @param dst Pointeur vers le buffer destination UTF-8
                 * @param dstLen Capacité destination en bytes
                 * @param charsRead [out] Codepoints UTF-32 lus
                 * @param bytesWritten [out] Bytes UTF-8 écrits
                 * @return Code de résultat de la conversion
                 * 
                 * @note Chaque codepoint UTF-32 peut produire 1 à 4 bytes UTF-8
                 */
                NKENTSEU_CONTAINERS_API NkConversionResult NkToUTF8(
                    const uint32* src, usize srcLen,
                    char* dst, usize dstLen,
                    usize& charsRead, usize& bytesWritten);

                /**
                 * @brief Convertit une séquence UTF-32 vers UTF-16
                 * 
                 * @param src Pointeur vers la source UTF-32
                 * @param srcLen Longueur source en unités uint32
                 * @param dst Pointeur vers le buffer destination UTF-16
                 * @param dstLen Capacité destination en unités uint16
                 * @param charsRead [out] Codepoints UTF-32 lus
                 * @param unitsWritten [out] Unités UTF-16 écrites
                 * @return Code de résultat de la conversion
                 * 
                 * @note Les codepoints >= 0x10000 produisent 2 unités UTF-16 (surrogate pair)
                 */
                NKENTSEU_CONTAINERS_API NkConversionResult NkToUTF16(
                    const uint32* src, usize srcLen,
                    uint16* dst, usize dstLen,
                    usize& charsRead, usize& unitsWritten);

            } // namespace utf32

        } // namespace encoding

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKUTF32_H_INCLUDED

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Validation de codepoints avant traitement interne
    // =====================================================================
    {
        bool IsValidUnicodeString(const uint32_t* codepoints, usize count) {
            // UTF-32 permet une validation simple et directe
            for (usize i = 0; i < count; ++i) {
                if (!nkentseu::encoding::utf32::NkIsValidCodepoint(codepoints[i])) {
                    return false;  // Codepoint hors plage ou surrogate illégal
                }
            }
            return true;
        }
        
        // Alternative plus concise utilisant la fonction de validation de séquence
        bool IsValidUnicodeString_Alt(const uint32_t* cps, usize len) {
            return nkentseu::encoding::utf32::NkIsValid(cps, len);
        }
    }

    // =====================================================================
    // Exemple 2 : Conversion UTF-32 vers UTF-8 pour export de données
    // =====================================================================
    {
        std::string ExportAsUTF8(const std::vector<uint32_t>& codepoints) {
            if (codepoints.empty()) {
                return {};
            }
            
            // Estimation conservative : 4 bytes UTF-8 max par codepoint UTF-32
            usize estimatedSize = codepoints.size() * 4;
            std::string buffer(estimatedSize, '\0');
            
            usize charsRead = 0, bytesWritten = 0;
            auto result = nkentseu::encoding::utf32::NkToUTF8(
                codepoints.data(), codepoints.size(),
                buffer.data(), estimatedSize,
                charsRead, bytesWritten);
            
            if (result == nkentseu::encoding::NkConversionResult::NK_SUCCESS) {
                buffer.resize(bytesWritten);  // Ajuster à la taille réelle
                return buffer;
            }
            
            // Gestion d'erreur : buffer trop petit (théoriquement impossible avec l'estimation)
            throw std::runtime_error("UTF-32 to UTF-8 conversion failed");
        }
    }

    // =====================================================================
    // Exemple 3 : Normalisation de texte en utilisant UTF-32 comme format intermédiaire
    // =====================================================================
    {
        class TextNormalizer {
        public:
            std::vector<uint32_t> NormalizeToCodepoints(const std::string& utf8Input) {
                // Étape 1 : UTF-8 -> UTF-32 pour manipulation facile des codepoints
                usize srcLen = utf8Input.size();
                std::vector<uint32_t> codepoints(srcLen);  // Sur-allocation temporaire
                
                usize bytesRead = 0, charsWritten = 0;
                auto result = nkentseu::encoding::utf8::NkToUTF32(
                    utf8Input.data(), srcLen,
                    codepoints.data(), codepoints.size(),
                    bytesRead, charsWritten);
                
                if (result != nkentseu::encoding::NkConversionResult::NK_SUCCESS) {
                    throw std::runtime_error("Invalid UTF-8 input");
                }
                
                codepoints.resize(charsWritten);  // Ajuster la taille réelle
                
                // Étape 2 : Appliquer les transformations au niveau codepoint
                // (ex: décomposition NFC/NFD, filtrage, etc.)
                ApplyNormalizationRules(codepoints);
                
                return codepoints;
            }
            
        private:
            void ApplyNormalizationRules(std::vector<uint32_t>& cps) {
                // Exemple : supprimer les caractères de contrôle non-whitespace
                cps.erase(
                    std::remove_if(cps.begin(), cps.end(), [](uint32_t cp) {
                        return cp < 32 && cp != '\n' && cp != '\r' && cp != '\t';
                    }),
                    cps.end()
                );
            }
        };
    }

    // =====================================================================
    // Exemple 4 : Comparaison de chaînes Unicode en ignorant les variations de casse
    // =====================================================================
    {
        bool CaseInsensitiveEqualUTF32(
            const uint32_t* lhs, usize lhsLen,
            const uint32_t* rhs, usize rhsLen) {
            
            // Longueurs différentes -> immédiatement faux
            if (lhsLen != rhsLen) {
                return false;
            }
            
            // Comparaison codepoint par codepoint avec normalisation de casse
            for (usize i = 0; i < lhsLen; ++i) {
                uint32_t l = lhs[i];
                uint32_t r = rhs[i];
                
                // Conversion simple vers minuscules pour ASCII uniquement
                // Pour une vraie normalisation Unicode, utiliser ICU ou similaire
                if (l >= 'A' && l <= 'Z') l = l + ('a' - 'A');
                if (r >= 'A' && r <= 'Z') r = r + ('a' - 'A');
                
                if (l != r) {
                    return false;
                }
            }
            
            return true;
        }
    }
*/