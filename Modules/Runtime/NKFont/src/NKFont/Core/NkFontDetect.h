// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontDetect.h
// DESCRIPTION: Détection automatique du type de fonte (bitmap vs vectorielle)
//              et configuration optimale automatique.
//
// Permet de déterminer sans intervention manuelle :
//   - Si une police est bitmap (ProggyClean, Terminus…) ou vectorielle (Roboto…)
//   - Le oversampleH optimal
//   - Le filtre de sampler GPU optimal (NEAREST vs LINEAR)
//   - La taille native recommandée
// -----------------------------------------------------------------------------

#pragma once

#ifndef NK_NKFONT_CORE_NKFONTDETECT_H_INCLUDED
#define NK_NKFONT_CORE_NKFONTDETECT_H_INCLUDED

// ============================================================
// INCLUDES
// ============================================================

#include "NkFontTypes.h"
#include "NkFontParser.h"
#include "NKFont/NkFont.h"

namespace nkentseu {

    // ============================================================
    // NkFontKind — Classification du type de fonte
    // ============================================================

    /**
     * @brief Classification du type de fonte détectée.
     */
    enum class NkFontKind : nkft_uint8 {
        Unknown   = 0, ///< Inconnu (non analysé).
        Bitmap    = 1, ///< Police bitmap (ex: ProggyClean, Terminus, Fixedsys).
        Vector    = 2, ///< Police vectorielle TrueType avec courbes (ex: Roboto).
        VectorCFF = 3, ///< Police vectorielle OTF/CFF (non supporté pour l'instant).
    };

    // ============================================================
    // NkFontProfile — Profil détecté d'une fonte
    // ============================================================

    /**
     * @brief Résultat de l'analyse automatique d'une fonte.
     *
     * Contient les réglages optimaux à utiliser dans NkFontConfig.
     */
    struct NkFontProfile {
        NkFontKind kind = NkFontKind::Unknown;

        // ── Réglages recommandés ──────────────────────────────────────────────
        nkft_int32   oversampleH      = 2;   ///< 1 pour bitmap, 2-3 pour vectoriel.
        nkft_int32   oversampleV      = 1;
        bool         pixelSnapH       = false; ///< true pour bitmap.
        bool         useNearestFilter = false; ///< true pour bitmap pur.

        // ── Taille native (surtout utile pour les bitmaps) ────────────────────
        nkft_float32 nativeSizePx         = 0.f;   ///< Taille native détectée (0 = non détectée).
        nkft_float32 recommendedMinSizePx = 8.f;
        nkft_float32 recommendedMaxSizePx = 72.f;

        // ── Statistiques utilisées pour la détection ──────────────────────────
        nkft_int32   sampleGlyphCount = 0;
        nkft_int32   curveGlyphCount  = 0;   ///< Glyphes avec au moins 1 courbe Bézier.
        nkft_float32 curveRatio       = 0.f; ///< curveGlyphCount / sampleGlyphCount.

        // ── Capacités ─────────────────────────────────────────────────────────
        bool isCFF            = false; ///< true si OTF/CFF (non supporté).
        bool hasBitmapStrikes = false; ///< true si table EBDT/CBDT présente.
    };

    // ============================================================
    // NkFontDetector — Analyse automatique d'une fonte
    // ============================================================

    /**
     * @brief Analyse une fonte pour déterminer son type et ses réglages optimaux.
     *
     * @example
     * @code
     * NkFontProfile profile = NkFontDetector::Analyze(&faceInfo);
     * NkFontConfig cfg = NkFontDetector::MakeOptimalConfig(profile, 18.f);
     * NkFont* font = atlas.AddFontFromFile("myfont.ttf", 18.f, &cfg);
     * @endcode
     */
    class NkFontDetector {
        public:

            /**
             * @brief Analyse une face de fonte et retourne son profil.
             *
             * Examine un échantillon de glyphes pour déterminer si la fonte
             * utilise des courbes Bézier (vectorielle) ou uniquement des lignes droites (bitmap).
             *
             * @param info    Face de fonte déjà initialisée via InitFontFace().
             * @param testCps Codepoints à tester (nullptr = jeu par défaut ASCII).
             * @param testCount Nombre de codepoints à tester.
             */
            static NkFontProfile Analyze(const nkfont::NkFontFaceInfo* info,
                                        const NkFontCodepoint* testCps = nullptr,
                                        nkft_int32 testCount = 0);

            /**
             * @brief Analyse directement depuis un buffer TTF (initialise en interne).
             */
            static NkFontProfile AnalyzeBuffer(const nkft_uint8* data,
                                            nkft_size size,
                                            nkft_int32 faceIndex = 0);

            /**
             * @brief Analyse directement depuis un fichier.
             */
            static NkFontProfile AnalyzeFile(const char* path,
                                            nkft_int32 faceIndex = 0);

            /**
             * @brief Crée un NkFontConfig optimal basé sur le profil détecté.
             *
             * @param profile  Profil retourné par Analyze().
             * @param sizePx   Taille de rendu souhaitée en pixels.
             */
            static void ApplyOptimalConfig(const NkFontProfile& profile,
                                        nkft_float32 sizePx,
                                        NkFontConfig* outConfig);

            /**
             * @brief Retourne le filtre GPU recommandé pour ce profil.
             * @return true = NK_NEAREST, false = NK_LINEAR.
             */
            static bool RecommendsNearestFilter(const NkFontProfile& profile);

            /**
             * @brief Retourne la taille d'affichage corrigée.
             *
             * Pour les polices bitmap, snap à la taille native la plus proche.
             * Pour les polices vectorielles, retourne sizePx inchangé.
             */
            static nkft_float32 SnapSize(const NkFontProfile& profile,
                                        nkft_float32 sizePx);

        private:
            // Codepoints ASCII représentatifs pour l'analyse
            static const NkFontCodepoint sDefaultTestCodepoints[];
            static const nkft_int32      sDefaultTestCount;

            static bool GlyphHasCurves(const nkfont::NkFontFaceInfo* info,
                                    NkGlyphId glyph);

            static nkft_float32 DetectNativeSize(const nkfont::NkFontFaceInfo* info);
    };

} // namespace nkentseu

#endif // NK_NKFONT_CORE_NKFONTDETECT_H_INCLUDED

// ============================================================
// Copyright © 2024-2026 Rihen. All rights reserved.
// ============================================================