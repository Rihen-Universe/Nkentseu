// -----------------------------------------------------------------------------
// FICHIER: NKContainers/String/NkBasicString.cpp
// DESCRIPTION: Implémentation des conversions d'encodage pour NkBasicString
//              Supporte UTF-8 ↔ UTF-16 ↔ UTF-32 ↔ wide (plateforme-dépendant)
// AUTEUR: Rihen
// DATE: 2026-02-08
// VERSION: 1.0.0
// -----------------------------------------------------------------------------

#include "NkBasicString.h"
#include "Encoding/NkUTF8.h"
#include "Encoding/NkUTF16.h"
#include "Encoding/NkUTF32.h"

namespace nkentseu
{
    // =====================================================================
    // SECTION 1 : CONVERSION UTF-8 → UTF-16
    // =====================================================================

    // ---------------------------------------------------------------------
    // Conversion d'une chaîne UTF-8 vers UTF-16
    // ---------------------------------------------------------------------
    /**
     * @brief Convertit une chaîne encodée en UTF-8 vers UTF-16
     * @param utf8Str Chaîne source en encodage UTF-8 (NkString8)
     * @return Nouvelle chaîne encodée en UTF-16 (NkString16)
     * 
     * @note Algorithme :
     *       1. Estimation de la taille maximale nécessaire (cas pire : 1:1)
     *       2. Allocation d'un buffer temporaire pour la conversion
     *       3. Appel à encoding::utf8::NkToUTF16 pour la conversion réelle
     *       4. Copie du résultat dans un NkString16 avec terminaison nulle
     *       5. Libération du buffer temporaire
     * 
     * @warning Utilise new[]/delete[] pour le buffer temporaire.
     *          Envisager un allocateur personnalisé pour les environnements contraints.
     * 
     * @par Gestion des erreurs :
     *        Les codes NK_SUCCESS et NK_SOURCE_EXHAUSTED sont considérés comme valides.
     *        NK_SOURCE_EXHAUSTED indique que toute la source a été consommée,
     *        même si le buffer de destination n'était pas entièrement rempli.
     */
    NkString16 NkToUTF16(const NkString8& utf8Str)
    {
        // -----------------------------------------------------------------
        // Cas trivial : chaîne vide → retour immédiat
        // -----------------------------------------------------------------
        if (utf8Str.Empty())
        {
            return NkString16();
        }

        // -----------------------------------------------------------------
        // Estimation de la taille maximale du résultat UTF-16
        // -----------------------------------------------------------------
        // Cas pire : tous les codepoints sont dans le BMP (Basic Multilingual Plane)
        // → 1 code unit UTF-16 par byte UTF-8 (surestimation conservative)
        usize maxUnits = utf8Str.Length();

        NkString16 result;
        result.Reserve(maxUnits);

        // -----------------------------------------------------------------
        // Allocation du buffer temporaire pour la conversion
        // -----------------------------------------------------------------
        // +1 pour le caractère nul terminal
        uint16* buffer = new uint16[maxUnits + 1];

        // -----------------------------------------------------------------
        // Variables de sortie pour la conversion
        // -----------------------------------------------------------------
        usize bytesRead = 0;
        usize unitsWritten = 0;

        // -----------------------------------------------------------------
        // Appel à la fonction de conversion UTF-8 → UTF-16
        // -----------------------------------------------------------------
        encoding::NkConversionResult convResult = encoding::utf8::NkToUTF16(
            utf8Str.Data(),
            utf8Str.Length(),
            buffer,
            maxUnits,
            bytesRead,
            unitsWritten
        );

        // -----------------------------------------------------------------
        // Traitement du résultat de conversion
        // -----------------------------------------------------------------
        if (convResult == encoding::NkConversionResult::NK_SUCCESS ||
            convResult == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED)
        {
            // Ajout du caractère nul terminal
            buffer[unitsWritten] = 0;

            // Copie des données converties dans le résultat
            result.Append(buffer, unitsWritten);
        }

        // -----------------------------------------------------------------
        // Libération du buffer temporaire
        // -----------------------------------------------------------------
        delete[] buffer;

        return result;
    }

    // =====================================================================
    // SECTION 2 : CONVERSION UTF-8 → UTF-32
    // =====================================================================

    // ---------------------------------------------------------------------
    // Conversion d'une chaîne UTF-8 vers UTF-32
    // ---------------------------------------------------------------------
    /**
     * @brief Convertit une chaîne encodée en UTF-8 vers UTF-32
     * @param utf8Str Chaîne source en encodage UTF-8 (NkString8)
     * @return Nouvelle chaîne encodée en UTF-32 (NkString32)
     * 
     * @note Algorithme similaire à NkToUTF16 :
     *       - Estimation conservative (1:1 pour ASCII)
     *       - Buffer temporaire alloué sur le heap
     *       - Conversion via encoding::utf8::NkToUTF32
     *       - Copie et terminaison nulle
     * 
     * @note UTF-32 garantit 1 code unit par codepoint Unicode,
     *       facilitant l'itération sur les caractères graphiques.
     * 
     * @warning Mémoire : UTF-32 utilise 4 octets par codepoint,
     *          soit 4x plus que UTF-8 pour du texte ASCII.
     */
    NkString32 NkToUTF32(const NkString8& utf8Str)
    {
        // -----------------------------------------------------------------
        // Cas trivial : chaîne vide
        // -----------------------------------------------------------------
        if (utf8Str.Empty())
        {
            return NkString32();
        }

        // -----------------------------------------------------------------
        // Estimation de la taille maximale (cas pire : ASCII = 1:1)
        // -----------------------------------------------------------------
        usize maxChars = utf8Str.Length();

        NkString32 result;
        result.Reserve(maxChars);

        // -----------------------------------------------------------------
        // Allocation du buffer temporaire
        // -----------------------------------------------------------------
        uint32* buffer = new uint32[maxChars + 1];

        // -----------------------------------------------------------------
        // Variables de suivi de la conversion
        // -----------------------------------------------------------------
        usize bytesRead = 0;
        usize charsWritten = 0;

        // -----------------------------------------------------------------
        // Conversion UTF-8 → UTF-32
        // -----------------------------------------------------------------
        encoding::NkConversionResult convResult = encoding::utf8::NkToUTF32(
            utf8Str.Data(),
            utf8Str.Length(),
            buffer,
            maxChars,
            bytesRead,
            charsWritten
        );

        // -----------------------------------------------------------------
        // Validation et copie du résultat
        // -----------------------------------------------------------------
        if (convResult == encoding::NkConversionResult::NK_SUCCESS ||
            convResult == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED)
        {
            buffer[charsWritten] = 0;
            result.Append(buffer, charsWritten);
        }

        // -----------------------------------------------------------------
        // Nettoyage mémoire
        // -----------------------------------------------------------------
        delete[] buffer;

        return result;
    }

    // =====================================================================
    // SECTION 3 : CONVERSION UTF-16 → UTF-8
    // =====================================================================

    // ---------------------------------------------------------------------
    // Conversion d'une chaîne UTF-16 vers UTF-8
    // ---------------------------------------------------------------------
    /**
     * @brief Convertit une chaîne encodée en UTF-16 vers UTF-8
     * @param utf16Str Chaîne source en encodage UTF-16 (NkString16)
     * @return Nouvelle chaîne encodée en UTF-8 (NkString8)
     * 
     * @note Estimation de taille :
     *       Cas pire : chaque codepoint UTF-16 nécessite 4 bytes UTF-8
     *       (codepoints hors BMP encodés en surrogate pairs → 4 bytes chacun)
     * 
     * @note La conversion gère correctement les surrogate pairs UTF-16
     *       pour produire des séquences UTF-8 valides.
     * 
     * @warning Le cast reinterpret_cast<const uint16*> est sécurisé car
     *          char16_t et uint16_t ont la même représentation mémoire.
     */
    NkString8 NkToUTF8(const NkString16& utf16Str)
    {
        // -----------------------------------------------------------------
        // Cas trivial : chaîne vide
        // -----------------------------------------------------------------
        if (utf16Str.Empty())
        {
            return NkString8();
        }

        // -----------------------------------------------------------------
        // Estimation conservative : 4 bytes UTF-8 max par code unit UTF-16
        // -----------------------------------------------------------------
        usize maxBytes = utf16Str.Length() * 4;

        NkString8 result;
        result.Reserve(maxBytes);

        // -----------------------------------------------------------------
        // Allocation du buffer temporaire (type Char = char pour UTF-8)
        // -----------------------------------------------------------------
        Char* buffer = new Char[maxBytes + 1];

        // -----------------------------------------------------------------
        // Variables de suivi
        // -----------------------------------------------------------------
        usize unitsRead = 0;
        usize bytesWritten = 0;

        // -----------------------------------------------------------------
        // Conversion UTF-16 → UTF-8
        // -----------------------------------------------------------------
        // Cast sécurisé : char16_t et uint16_t sont layout-compatible
        encoding::NkConversionResult convResult = encoding::utf16::NkToUTF8(
            reinterpret_cast<const uint16*>(utf16Str.Data()),
            utf16Str.Length(),
            buffer,
            maxBytes,
            unitsRead,
            bytesWritten
        );

        // -----------------------------------------------------------------
        // Traitement du résultat
        // -----------------------------------------------------------------
        if (convResult == encoding::NkConversionResult::NK_SUCCESS ||
            convResult == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED)
        {
            buffer[bytesWritten] = '\0';
            result.Append(buffer, bytesWritten);
        }

        // -----------------------------------------------------------------
        // Libération mémoire
        // -----------------------------------------------------------------
        delete[] buffer;

        return result;
    }

    // =====================================================================
    // SECTION 4 : CONVERSION UTF-32 → UTF-8
    // =====================================================================

    // ---------------------------------------------------------------------
    // Conversion d'une chaîne UTF-32 vers UTF-8
    // ---------------------------------------------------------------------
    /**
     * @brief Convertit une chaîne encodée en UTF-32 vers UTF-8
     * @param utf32Str Chaîne source en encodage UTF-32 (NkString32)
     * @return Nouvelle chaîne encodée en UTF-8 (NkString8)
     * 
     * @note Estimation : 4 bytes UTF-8 maximum par codepoint UTF-32
     *       (cas des codepoints U+10000 et au-delà)
     * 
     * @note UTF-32 stocke chaque codepoint Unicode sur exactement 4 bytes,
     *       ce qui simplifie l'itération mais augmente l'empreinte mémoire.
     * 
     * @warning Le cast reinterpret_cast<const uint32*> est valide car
     *          char32_t et uint32_t ont la même taille et alignement.
     */
    NkString8 NkToUTF8(const NkString32& utf32Str)
    {
        // -----------------------------------------------------------------
        // Cas trivial : chaîne vide
        // -----------------------------------------------------------------
        if (utf32Str.Empty())
        {
            return NkString8();
        }

        // -----------------------------------------------------------------
        // Estimation : 4 bytes UTF-8 par codepoint UTF-32 (cas pire)
        // -----------------------------------------------------------------
        usize maxBytes = utf32Str.Length() * 4;

        NkString8 result;
        result.Reserve(maxBytes);

        // -----------------------------------------------------------------
        // Allocation du buffer temporaire
        // -----------------------------------------------------------------
        Char* buffer = new Char[maxBytes + 1];

        // -----------------------------------------------------------------
        // Variables de suivi de la conversion
        // -----------------------------------------------------------------
        usize charsRead = 0;
        usize bytesWritten = 0;

        // -----------------------------------------------------------------
        // Conversion UTF-32 → UTF-8
        // -----------------------------------------------------------------
        encoding::NkConversionResult convResult = encoding::utf32::NkToUTF8(
            reinterpret_cast<const uint32*>(utf32Str.Data()),
            utf32Str.Length(),
            buffer,
            maxBytes,
            charsRead,
            bytesWritten
        );

        // -----------------------------------------------------------------
        // Validation et copie du résultat converti
        // -----------------------------------------------------------------
        if (convResult == encoding::NkConversionResult::NK_SUCCESS ||
            convResult == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED)
        {
            buffer[bytesWritten] = '\0';
            result.Append(buffer, bytesWritten);
        }

        // -----------------------------------------------------------------
        // Nettoyage du buffer temporaire
        // -----------------------------------------------------------------
        delete[] buffer;

        return result;
    }

    // =====================================================================
    // SECTION 5 : CONVERSIONS WIDE STRING (PLATEFORME-DÉPENDANT)
    // =====================================================================

    // ---------------------------------------------------------------------
    // Implémentation Windows : wchar_t = 2 bytes (UTF-16)
    // ---------------------------------------------------------------------
    #if defined(NK_PLATFORM_WINDOWS)

        // -----------------------------------------------------------------
        // Conversion UTF-8 → wide string (Windows : UTF-16)
        // -----------------------------------------------------------------
        /**
         * @brief Convertit une chaîne UTF-8 vers wide string natif Windows
         * @param str Chaîne source en UTF-8 (NkString8)
         * @return Wide string encodé en UTF-16 (WString = NkBasicString<wchar_t>)
         * 
         * @note Sur Windows, wchar_t est défini sur 2 bytes et utilise UTF-16.
         *       La conversion passe donc par NkToUTF16 puis un reinterpret_cast.
         * 
         * @warning Le reinterpret_cast est sécurisé car wchar_t et char16_t
         *          ont la même taille (2 bytes) et représentation sur Windows.
         *          Ce code n'est compilé que si NK_PLATFORM_WINDOWS est défini.
         */
        WString NkToWide(const String8& str)
        {
            // -----------------------------------------------------------------
            // Conversion intermédiaire UTF-8 → UTF-16
            // -----------------------------------------------------------------
            NkString16 utf16Intermediate = NkToUTF16(str);

            // -----------------------------------------------------------------
            // Cast vers wchar_t (valide sur Windows : wchar_t == char16_t)
            // -----------------------------------------------------------------
            return WString(
                reinterpret_cast<const wchar*>(utf16Intermediate.Data()),
                utf16Intermediate.Length()
            );
        }

        // -----------------------------------------------------------------
        // Conversion wide string → UTF-8 (Windows : UTF-16 → UTF-8)
        // -----------------------------------------------------------------
        /**
         * @brief Convertit un wide string Windows vers UTF-8
         * @param wstr Wide string source en UTF-16 (WString)
         * @return Chaîne encodée en UTF-8 (String8)
         * 
         * @note Algorithme en deux étapes :
         *       1. Cast de wchar_t* vers char16_t* (sécurisé sur Windows)
         *       2. Conversion UTF-16 → UTF-8 via NkToUTF8
         * 
         * @warning Ce code est conditionnel à NK_PLATFORM_WINDOWS.
         *          Sur d'autres plateformes, voir l'implémentation #else.
         */
        String8 NkFromWide(const WString& wstr)
        {
            // -----------------------------------------------------------------
            // Construction d'un NkString16 depuis le wide string
            // -----------------------------------------------------------------
            // Cast sécurisé : wchar_t et char16_t sont layout-compatible sur Windows
            String16 utf16(
                reinterpret_cast<const char16*>(wstr.Data()),
                wstr.Length()
            );

            // -----------------------------------------------------------------
            // Conversion finale UTF-16 → UTF-8
            // -----------------------------------------------------------------
            return NkToUTF8(utf16);
        }

    // ---------------------------------------------------------------------
    // Implémentation Unix/Linux : wchar_t = 4 bytes (UTF-32)
    // ---------------------------------------------------------------------
    #else

        // -----------------------------------------------------------------
        // Conversion UTF-8 → wide string (Unix : UTF-32)
        // -----------------------------------------------------------------
        /**
         * @brief Convertit une chaîne UTF-8 vers wide string natif Unix
         * @param str Chaîne source en UTF-8 (NkString8)
         * @return Wide string encodé en UTF-32 (NkWString = NkBasicString<wchar_t>)
         * 
         * @note Sur les plateformes Unix/Linux, wchar_t est défini sur 4 bytes
         *       et utilise typiquement UTF-32 (chaque codepoint = 1 wchar_t).
         * 
         * @note La conversion passe donc par NkToUTF32 puis un reinterpret_cast.
         * 
         * @warning Le reinterpret_cast est sécurisé car wchar_t et char32_t
         *          ont la même taille (4 bytes) sur les plateformes POSIX.
         */
        NkWString NkToWide(const NkString8& str)
        {
            // -----------------------------------------------------------------
            // Conversion intermédiaire UTF-8 → UTF-32
            // -----------------------------------------------------------------
            NkString32 utf32Intermediate = NkToUTF32(str);

            // -----------------------------------------------------------------
            // Cast vers wchar_t (valide sur Unix : wchar_t == char32_t)
            // -----------------------------------------------------------------
            return NkWString(
                reinterpret_cast<const wchar*>(utf32Intermediate.Data()),
                utf32Intermediate.Length()
            );
        }

        // -----------------------------------------------------------------
        // Conversion wide string → UTF-8 (Unix : UTF-32 → UTF-8)
        // -----------------------------------------------------------------
        /**
         * @brief Convertit un wide string Unix vers UTF-8
         * @param wstr Wide string source en UTF-32 (NkWString)
         * @return Chaîne encodée en UTF-8 (NkString8)
         * 
         * @note Algorithme en deux étapes :
         *       1. Cast de wchar_t* vers char32_t* (sécurisé sur Unix)
         *       2. Conversion UTF-32 → UTF-8 via NkToUTF8
         * 
         * @warning Ce code est compilé uniquement si NK_PLATFORM_WINDOWS
         *          n'est PAS défini. Assurez-vous que votre système de build
         *          définit correctement ce macro selon la plateforme cible.
         */
        NkString8 NkFromWide(const NkWString& wstr)
        {
            // -----------------------------------------------------------------
            // Construction d'un NkString32 depuis le wide string
            // -----------------------------------------------------------------
            // Cast sécurisé : wchar_t et char32_t sont layout-compatible sur Unix
            NkString32 utf32(
                reinterpret_cast<const char32*>(wstr.Data()),
                wstr.Length()
            );

            // -----------------------------------------------------------------
            // Conversion finale UTF-32 → UTF-8
            // -----------------------------------------------------------------
            return NkToUTF8(utf32);
        }

    #endif // NK_PLATFORM_WINDOWS

} // namespace nkentseu

// =============================================================================
// EXEMPLES D'UTILISATION DES CONVERSIONS D'ENCODAGE
// =============================================================================
/*
    // -------------------------------------------------------------------------
    // Exemple 1 : Conversion basique UTF-8 ↔ UTF-16
    // -------------------------------------------------------------------------
    #include "NKContainers/String/NkBasicString.h"
    using namespace nkentseu;

    // Chaîne UTF-8 source
    NkString8 utf8Text = "Bonjour 世界 🌍";

    // Conversion vers UTF-16 (pour interop Windows/API Java)
    NkString16 utf16Text = NkToUTF16(utf8Text);

    // Conversion retour vers UTF-8
    NkString8 roundtrip = NkToUTF8(utf16Text);

    // Vérification d'intégrité
    if (utf8Text == roundtrip)
    {
        // Conversion réversible réussie
    }

    // -------------------------------------------------------------------------
    // Exemple 2 : Interopérabilité avec les API Windows (UTF-16)
    // -------------------------------------------------------------------------
    #if defined(NK_PLATFORM_WINDOWS)

    void DisplayWindowsMessage(const char* utf8Message)
    {
        // Conversion UTF-8 → wide string pour MessageBoxW
        WString wideMsg = NkToWide(utf8Message);

        // Appel à l'API Windows (nécessite LPCWSTR = const wchar_t*)
        MessageBoxW(nullptr, wideMsg.CStr(), L"Titre", MB_OK);
    }

    // Récupération de texte depuis une API Windows
    void ReadWindowText(HWND hwnd)
    {
        // Buffer pour recevoir le texte wide
        wchar_t buffer[1024];
        GetWindowTextW(hwnd, buffer, 1024);

        // Conversion vers UTF-8 pour traitement interne
        WString wideText(buffer);
        NkString8 utf8Text = NkFromWide(wideText);

        // Traitement en UTF-8...
        ProcessText(utf8Text);
    }

    #endif // NK_PLATFORM_WINDOWS

    // -------------------------------------------------------------------------
    // Exemple 3 : Gestion des emojis et codepoints Unicode étendus
    // -------------------------------------------------------------------------
    {
        // Chaîne contenant des emojis (codepoints hors BMP)
        NkString8 emojis = "Flags: 🇫🇷 🇯🇵 🇺🇸";

        // Conversion vers UTF-32 pour itération facile sur les codepoints
        NkString32 utf32Emojis = NkToUTF32(emojis);

        // Chaque emoji flag = 2 codepoints (regional indicator symbols)
        // utf32Emojis.Length() donne le nombre exact de codepoints
        for (usize i = 0; i < utf32Emojis.Length(); ++i)
        {
            char32_t codepoint = utf32Emojis[i];
            // Traitement par codepoint individuel...
        }

        // Retour vers UTF-8 pour affichage ou stockage
        NkString8 result = NkToUTF8(utf32Emojis);
    }

    // -------------------------------------------------------------------------
    // Exemple 4 : Conversion sécurisée avec validation d'entrée
    // -------------------------------------------------------------------------
    {
        bool SafeConvertUTF8ToUTF16(
            const NkString8& input,
            NkString16& output,
            usize maxOutputLength = 0)
        {
            // Validation : rejeter les entrées excessivement longues
            if (input.Length() > 1024 * 1024)  // 1 MB max
            {
                return false;
            }

            // Conversion avec estimation de taille
            output = NkToUTF16(input);

            // Validation post-conversion
            if (output.Empty() && !input.Empty())
            {
                // Échec silencieux de conversion → signaler l'erreur
                return false;
            }

            // Optionnel : vérifier la longueur maximale
            if (maxOutputLength > 0 && output.Length() > maxOutputLength)
            {
                output.Resize(maxOutputLength);  // Tronquer si nécessaire
            }

            return true;
        }

        // Usage
        NkString8 userInput = GetUserInput();  // Source non fiable
        NkString16 converted;

        if (SafeConvertUTF8ToUTF16(userInput, converted, 2048))
        {
            UseConvertedText(converted);
        }
        else
        {
            LogError("Conversion d'encodage échouée");
        }
    }

    // -------------------------------------------------------------------------
    // Exemple 5 : Performance — éviter les conversions redondantes
    // -------------------------------------------------------------------------
    {
        // ❌ MAUVAIS : Chaîne de conversions inutiles
        void ProcessText_Bad(const NkString8& text)
        {
            // Conversion vers UTF-16 pour une opération...
            NkString16 temp16 = NkToUTF16(text);
            DoSomethingUTF16(temp16);

            // ...puis reconversion vers UTF-8 sans nécessité
            NkString8 temp8 = NkToUTF8(temp16);
            DoSomethingUTF8(temp8);  // On aurait pu utiliser 'text' directement !
        }

        // ✅ BON : Conserver l'encodage natif autant que possible
        void ProcessText_Good(const NkString8& text)
        {
            // Travailler en UTF-8 tant que possible
            DoSomethingUTF8(text);

            // Convertir uniquement quand l'API externe l'exige
            if (NeedsUTF16API())
            {
                NkString16 utf16 = NkToUTF16(text);
                CallUTF16API(utf16.CStr());
            }
        }
    }

    // -------------------------------------------------------------------------
    // Exemple 6 : Gestion des erreurs de conversion (caractères invalides)
    // -------------------------------------------------------------------------
    {
        // Note : Les fonctions NkToUTF* retournent un résultat même en cas
        // d'erreur partielle (NK_SOURCE_EXHAUSTED). Pour un contrôle fin :

        NkString16 SafeToUTF16(const NkString8& utf8Str)
        {
            if (utf8Str.Empty())
            {
                return NkString16();
            }

            usize maxUnits = utf8Str.Length();
            uint16* buffer = new uint16[maxUnits + 1];

            usize bytesRead = 0;
            usize unitsWritten = 0;

            encoding::NkConversionResult result = encoding::utf8::NkToUTF16(
                utf8Str.Data(),
                utf8Str.Length(),
                buffer,
                maxUnits,
                bytesRead,
                unitsWritten
            );

            NkString16 output;

            if (result == encoding::NkConversionResult::NK_SUCCESS)
            {
                // Conversion complète réussie
                buffer[unitsWritten] = 0;
                output.Append(buffer, unitsWritten);
            }
            else if (result == encoding::NkConversionResult::NK_SOURCE_EXHAUSTED)
            {
                // Source entièrement consommée, mais destination peut être partielle
                // Décision : accepter ou rejeter selon la politique d'application
                buffer[unitsWritten] = 0;
                output.Append(buffer, unitsWritten);

                // Optionnel : logger un avertissement si bytesRead < utf8Str.Length()
                if (bytesRead < utf8Str.Length())
                {
                    LogWarning("Conversion tronquée : données UTF-8 invalides détectées");
                }
            }
            else
            {
                // Erreur de conversion : séquence UTF-8 malformée
                LogError("Échec de conversion UTF-8 → UTF-16");
                // Stratégie de fallback : retourner chaîne vide ou remplacer les erreurs
                output = NkString16("[CONVERSION_ERROR]");
            }

            delete[] buffer;
            return output;
        }
    }

    // -------------------------------------------------------------------------
    // Exemple 7 : Conversion batch pour traitement de fichiers
    // -------------------------------------------------------------------------
    {
        void ConvertFileEncoding(
            const char* inputPath,    // Fichier source en UTF-8
            const char* outputPath,   // Fichier destination en UTF-16
            memory::NkIAllocator& tempAlloc)  // Allocateur pour buffers temporaires
        {
            // Lecture du fichier source en UTF-8
            NkString8 content = ReadFileAsUTF8(inputPath);
            if (content.Empty())
            {
                return;  // Gérer l'erreur selon la politique de l'application
            }

            // Conversion avec allocateur temporaire pour éviter les allocs système
            NkString16 converted;
            {
                // Scope pour libérer les temporaires après conversion
                converted = NkToUTF16(content);
            }

            // Écriture du résultat en UTF-16 avec BOM (Byte Order Mark)
            WriteFileWithBOM(outputPath, converted.Data(), converted.Length());
        }

        // Usage : conversion de fichier de configuration
        ConvertFileEncoding(
            "config/settings.json",      // UTF-8 source
            "config/settings_win.json",  // UTF-16 destination pour Windows
            GetFrameAllocator()          // Pool allocator pour performance
        );
    }
*/

// ============================================================
// Copyright © 2024-2026 Rihen. Tous droits réservés.
// Licence Propriétaire - Utilisation et modification autorisées
//
// Généré par Rihen le 2026-02-08
// Date de création : 2026-02-08
// Dernière modification : 2026-04-26
// ============================================================