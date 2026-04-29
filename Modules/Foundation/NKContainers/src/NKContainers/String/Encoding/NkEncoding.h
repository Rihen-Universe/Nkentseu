// -----------------------------------------------------------------------------
// FICHIER: Core\NKCore\src\NKCore\String\Encoding\NkEncoding.h
// DESCRIPTION: Définition des types de base pour l'encoding et détection d'encodage
// AUTEUR: Rihen
// DATE: 2026-02-07
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKENCODING_H_INCLUDED
#define NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKENCODING_H_INCLUDED

    // -------------------------------------------------------------------------
    // Inclusions des dépendances
    // -------------------------------------------------------------------------
    #include "NKCore/NkTypes.h"
    #include "NKContainers/NkContainersApi.h"
    #include "NKPlatform/NkPlatformInline.h"

    // -------------------------------------------------------------------------
    // Namespace principal du projet
    // -------------------------------------------------------------------------
    namespace nkentseu {

        // -------------------------------------------------------------------------
        // Sous-namespace dédié aux utilitaires d'encodage de chaînes
        // -------------------------------------------------------------------------
        namespace encoding {

            // =====================================================================
            // ÉNUMÉRATIONS
            // =====================================================================

            /**
             * @brief Énumération des types d'encodage supportés par la bibliothèque
             * 
             * Cette énumération permet d'identifier et de manipuler différents
             * formats d'encodage de caractères, des plus simples (ASCII) aux plus
             * complexes (UTF-32 avec gestion de l'endianness).
             */
            enum class NkEncodingType {

                /// Encodage inconnu ou non détecté
                NK_UNKNOWN,

                /// ASCII standard (7 bits, 0-127)
                NK_ASCII,

                /// UTF-8 : encodage variable (1-4 bytes), rétro-compatible ASCII
                NK_UTF8,

                /// UTF-16 Little Endian (2 ou 4 bytes par codepoint)
                NK_UTF16LE,

                /// UTF-16 Big Endian (2 ou 4 bytes par codepoint)
                NK_UTF16BE,

                /// UTF-32 Little Endian (4 bytes fixes par codepoint)
                NK_UTF32LE,

                /// UTF-32 Big Endian (4 bytes fixes par codepoint)
                NK_UTF32BE,

                /// Latin-1 / ISO-8859-1 (8 bits, 256 caractères)
                NK_LATIN1,
            };

            /**
             * @brief Codes de retour pour les opérations de conversion d'encodage
             * 
             * Ces valeurs permettent de gérer proprement les erreurs lors des
             * conversions entre différents formats d'encodage.
             */
            enum class NkConversionResult {

                /// Conversion réussie sans erreur
                NK_SUCCESS,

                /// Source incomplète : données tronquées en fin de buffer
                NK_SOURCE_EXHAUSTED,

                /// Buffer destination trop petit pour contenir le résultat
                NK_TARGET_EXHAUSTED,

                /// Séquence de bytes invalide dans l'encodage source
                NK_SOURCE_ILLEGAL,
            };

            // =====================================================================
            // FONCTIONS PUBLIQUES - API
            // =====================================================================

            /**
             * @brief Détecte le type d'encodage en analysant le BOM (Byte Order Mark)
             * 
             * @param data Pointeur vers les données brutes à analyser
             * @param size Taille des données en bytes
             * @return NkEncodingType identifié, ou NK_UNKNOWN si non détecté
             * 
             * @note Le BOM est une signature optionnelle en début de fichier/texte
             *       qui indique l'encodage utilisé. Cette fonction ne détecte
             *       que les encodages avec BOM explicite.
             */
            NKENTSEU_CONTAINERS_API NkEncodingType NkDetectBOM(const void* data, usize size);

            /**
             * @brief Vérifie si une chaîne contient uniquement des caractères ASCII valides
             * 
             * @param str Pointeur vers la chaîne de caractères à valider
             * @param length Nombre de caractères à vérifier
             * @return true si tous les caractères sont dans la plage 0-127, false sinon
             */
            NKENTSEU_CONTAINERS_API bool NkIsValidASCII(const char* str, usize length);

            /**
             * @brief Valide la conformité d'une séquence de bytes en UTF-8
             * 
             * @param str Pointeur vers les données UTF-8 à valider
             * @param length Nombre de bytes à vérifier
             * @return true si la séquence est un UTF-8 bien formé, false sinon
             * 
             * @note Vérifie : séquences bien formées, pas de sur-encodage, 
             *       pas de points de code réservés (surrogates)
             */
            NKENTSEU_CONTAINERS_API bool NkIsValidUTF8(const char* str, usize length);

            /**
             * @brief Valide la conformité d'une séquence UTF-16
             * 
             * @param str Pointeur vers les unités UTF-16 à valider
             * @param length Nombre d'unités (uint16) à vérifier
             * @return true si la séquence est valide (paires de surrogates correctes)
             */
            NKENTSEU_CONTAINERS_API bool NkIsValidUTF16(const uint16* str, usize length);

            /**
             * @brief Valide la conformité d'une séquence UTF-32
             * 
             * @param str Pointeur vers les codepoints UTF-32 à valider
             * @param length Nombre de codepoints (uint32) à vérifier
             * @return true si tous les codepoints sont dans la plage Unicode valide
             */
            NKENTSEU_CONTAINERS_API bool NkIsValidUTF32(const uint32* str, usize length);

        } // namespace encoding

    } // namespace nkentseu

#endif // NK_CORE_NKCORE_SRC_NKCORE_STRING_ENCODING_NKENCODING_H_INCLUDED

// -----------------------------------------------------------------------------
// EXEMPLES D'UTILISATION
// -----------------------------------------------------------------------------
/*
    // =====================================================================
    // Exemple 1 : Détection d'encodage via BOM
    // =====================================================================
    {
        const uint8_t dataWithBOM[] = {0xEF, 0xBB, 0xBF, 'H', 'e', 'l', 'l', 'o'};
        
        nkentseu::encoding::NkEncodingType detected = 
            nkentseu::encoding::NkDetectBOM(dataWithBOM, sizeof(dataWithBOM));
        
        if (detected == nkentseu::encoding::NkEncodingType::NK_UTF8) {
            // Traiter comme UTF-8...
        }
    }

    // =====================================================================
    // Exemple 2 : Validation UTF-8 avant traitement
    // =====================================================================
    {
        const char* userInput = "Données utilisateur...";
        usize inputLength = strlen(userInput);
        
        if (nkentseu::encoding::NkIsValidUTF8(userInput, inputLength)) {
            // Sécurisé pour traiter la chaîne UTF-8
            ProcessUTF8String(userInput, inputLength);
        } else {
            // Gérer l'erreur : données corrompues ou mauvais encodage
            LogError("Invalid UTF-8 sequence detected");
        }
    }

    // =====================================================================
    // Exemple 3 : Gestion des résultats de conversion
    // =====================================================================
    {
        const char* utf8Source = "Exemple de texte";
        usize srcLen = strlen(utf8Source);
        
        uint16_t utf16Buffer[256];
        usize dstLen = 256;
        usize bytesRead = 0;
        usize charsWritten = 0;
        
        nkentseu::encoding::NkConversionResult result = 
            nkentseu::encoding::utf8::NkToUTF16(
                utf8Source, srcLen,
                utf16Buffer, dstLen,
                bytesRead, charsWritten
            );
        
        switch (result) {
            case nkentseu::encoding::NkConversionResult::NK_SUCCESS:
                // Conversion complète réussie
                break;
                
            case nkentseu::encoding::NkConversionResult::NK_TARGET_EXHAUSTED:
                // Buffer destination trop petit : allouer plus et réessayer
                break;
                
            case nkentseu::encoding::NkConversionResult::NK_SOURCE_ILLEGAL:
                // Données source corrompues : rejeter ou nettoyer
                break;
                
            case nkentseu::encoding::NkConversionResult::NK_SOURCE_EXHAUSTED:
                // Données source tronquées : attendre plus de données
                break;
        }
    }

    // =====================================================================
    // Exemple 4 : Validation avant stockage sécurisé
    // =====================================================================
    {
        bool IsSafeToStore(const char* str, usize length) {
            // Vérifie que la chaîne est soit ASCII pur, soit UTF-8 valide
            return nkentseu::encoding::NkIsValidASCII(str, length) ||
                   nkentseu::encoding::NkIsValidUTF8(str, length);
        }
        
        if (IsSafeToStore(userData, dataLength)) {
            database.Save(userData, dataLength);
        }
    }
*/