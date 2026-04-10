// -----------------------------------------------------------------------------
// FICHIER: NKFont/Embedded/NkFontEmbedded.h
// DESCRIPTION: Polices intégrées dans le binaire (pas de fichier externe requis).
//
// Architecture :
//   - Les données TTF sont compressées avec deflate (zlib) et encodées en C array.
//   - Elles sont décompressées à la demande lors du premier accès.
//   - NkFontAtlas::AddFontEmbedded() décompresse et charge la fonte.
//
// Pour ajouter une nouvelle police :
//   1. Lancez le script Tools/EmbedFont.py sur votre TTF.
//   2. Copiez le fichier _data.h généré dans ce dossier.
//   3. Déclarez la fonte dans NkEmbeddedFontId.
//   4. Ajoutez l'entrée dans sEmbeddedFontRegistry[].
//
// Polices incluses :
//   - ProggyClean    : bitmap monospace 13px (MIT License)
//   - DroidSans      : vectorielle sans-serif (Apache 2)
//   - Cousine        : vectorielle monospace (Apache 2)
//   - Karla          : vectorielle sans-serif compact (OFL)
//
// Note : Pour inclure les vraies données, copiez vos TTF dans Tools/
//        et exécutez : python Tools/EmbedFont.py Resources/Fonts/MyFont.ttf
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKFONT_EMBEDDED_NKFONTEMBEDDED_H_INCLUDED
#define NK_NKFONT_EMBEDDED_NKFONTEMBEDDED_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NKFont/Core/NkFontTypes.h"
#include "NKFont/Core/NkFontDetect.h"
#include "NKFont/Core/NkFontParser.h"
#include "NKFont/NkFont.h"

namespace nkentseu {

    // ============================================================
    // NkEmbeddedFontId — identifiants des polices intégrées
    // ============================================================

    /**
     * @brief Identifiant d'une police embarquée dans le binaire.
     */
    enum class NkEmbeddedFontId : nkft_uint32 {
        // ── Polices bitmap ────────────────────────────────────────────────────
        ProggyClean    = 0,  ///< Bitmap monospace 13px — idéal pour debug/console.
        ProggyTiny     = 1,  ///< Bitmap monospace 10px — très petite.

        // ── Polices vectorielles sans-serif ───────────────────────────────────
        DroidSans      = 2,  ///< Sans-serif généraliste (Android UI).
        Karla          = 3,  ///< Sans-serif compact, excellente lisibilité.
        Roboto         = 4,  ///< Google Material Design (si intégrée).

        // ── Polices vectorielles monospace ────────────────────────────────────
        Cousine        = 5,  ///< Monospace vectorielle (Code/Console).
        SourceCodePro  = 6,  ///< Adobe Source Code Pro.

        // ── Polices vectorielles avec serif ───────────────────────────────────
        DroidSerif     = 7,  ///< Serif classique.

        Count          = 8,
        Default        = ProggyClean, ///< Police par défaut du système.
    };

    // ============================================================
    // NkEmbeddedFontData — données compressées d'une police
    // ============================================================

    /**
     * @brief Entrée du registre des polices intégrées.
     */
    struct NkEmbeddedFontData {
        const char*         name;            ///< Nom lisible (ex: "ProggyClean").
        const nkft_uint8*   compressedData;  ///< Données TTF compressées (deflate).
        nkft_uint32         compressedSize;  ///< Taille compressée en octets.
        nkft_uint32         originalSize;    ///< Taille originale TTF en octets.
        NkFontKind          kind;            ///< Bitmap ou vectoriel.
        nkft_float32        nativeSizePx;    ///< Taille native (0 si vectoriel).
        const char*         license;         ///< Résumé de la licence.
    };

    // ============================================================
    // NkFontEmbedded — gestionnaire des polices intégrées
    // ============================================================

    /**
     * @brief Gestionnaire centralisé des polices embarquées dans le binaire.
     *
     * @example Utilisation basique
     * @code
     * // Charge une police embarquée dans l'atlas
     * NkFont* font = NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::ProggyClean, 13.f);
     * atlas.Build();
     *
     * // Vérifie la disponibilité
     * if (NkFontEmbedded::IsAvailable(NkEmbeddedFontId::DroidSans)) {
     *     NkFont* sans = NkFontEmbedded::AddToAtlas(atlas, NkEmbeddedFontId::DroidSans, 18.f);
     * }
     * @endcode
     */
    class NkFontEmbedded {
        public:

            /**
             * @brief Vérifie si une police est disponible dans ce build.
             *
             * Certaines polices peuvent être absentes si leurs données n'ont pas
             * été intégrées (données TTF non incluses pour raisons de taille).
             */
            static bool IsAvailable(NkEmbeddedFontId id);

            /**
             * @brief Retourne les informations d'une police intégrée.
             * @return nullptr si la police n'est pas disponible.
             */
            static const NkEmbeddedFontData* GetData(NkEmbeddedFontId id);

            /**
             * @brief Décompresse et ajoute une police intégrée à l'atlas.
             *
             * @param atlas   Atlas cible.
             * @param id      Identifiant de la police.
             * @param sizePx  Taille de rendu (0 = taille native pour bitmap).
             * @param cfg     Config supplémentaire (oversampleH est géré automatiquement).
             * @return NkFont* créée, ou nullptr si la police n'est pas disponible.
             */
            static NkFont* AddToAtlas(NkFontAtlas& atlas,
                                    NkEmbeddedFontId id,
                                    nkft_float32 sizePx = 0.f,
                                    const NkFontConfig* cfg = nullptr);

            /**
             * @brief Ajoute la police par défaut (ProggyClean 13px) à l'atlas.
             * Équivalent de AddToAtlas(atlas, NkEmbeddedFontId::Default, 13.f).
             */
            static NkFont* AddDefaultFont(NkFontAtlas& atlas,
                                        const NkFontConfig* cfg = nullptr);

            /**
             * @brief Retourne le nom lisible d'une police.
             */
            static const char* GetName(NkEmbeddedFontId id);

            /**
             * @brief Retourne la liste de toutes les polices disponibles.
             * @param outCount  Nombre de polices disponibles.
             */
            static const NkEmbeddedFontData* GetAll(nkft_int32* outCount);

        private:
            static nkft_uint8* Decompress(const NkEmbeddedFontData& data);
            static void        DecompressFree(nkft_uint8* ptr);
    };

    // ============================================================
    // NkFontEmbedder — outil de création d'une police embarquée
    // (usage développeur uniquement, pas en production)
    // ============================================================

    /**
     * @brief Utilitaire pour générer le code C d'une police embarquée.
     *
     * Génère un fichier .h contenant les données compressées d'un TTF,
     * prêt à être inclus dans NkFontEmbedded.cpp.
     *
     * @example
     * @code
     * // Génère Roboto_data.h depuis le fichier TTF
     * NkFontEmbedder::GenerateHeader("Resources/Fonts/Roboto-Regular.ttf",
     *                                "Roboto",
     *                                "NKFont/Embedded/Roboto_data.h");
     * @endcode
     */
    class NkFontEmbedder {
        public:
            /**
             * @brief Génère un fichier .h avec les données compressées du TTF.
             *
             * Le fichier généré contient un tableau C de bytes compressés
             * utilisable directement avec NkFontEmbedded.
             *
             * @param inputPath   Chemin vers le fichier TTF source.
             * @param fontName    Nom symbolique (ex: "Roboto", "ProggyClean").
             * @param outputPath  Chemin du fichier .h généré.
             * @return true si la génération a réussi.
             */
            static bool GenerateHeader(const char* inputPath,
                                    const char* fontName,
                                    const char* outputPath);

            /**
             * @brief Calcule les données compressées d'un buffer TTF.
             * Les données retournées doivent être libérées avec free().
             */
            static nkft_uint8* Compress(const nkft_uint8* data,
                                        nkft_size size,
                                        nkft_uint32* outCompressedSize);
    };

} // namespace nkentseu

#endif // NK_NKFONT_EMBEDDED_NKFONTEMBEDDED_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================