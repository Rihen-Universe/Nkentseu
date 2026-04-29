#pragma once

#ifndef NK_NKFONT_UTILS_H
#define NK_NKFONT_UTILS_H

#include <vector>
#include <cmath>
#include <cstdint>

#include "NkFontTypes.h"
#include "NkFontParser.h"

namespace nkentseu {

    struct NkEdge {
        math::NkVec2f p0;
        math::NkVec2f p1;
    };

    struct NkShape {
        std::vector<NkEdge> edges;
    };

    // ============================================================
    // Rasterizer analytique (coverage)
    // ============================================================

    class NkRasterizer {
        public:
            // Rasterization coverage classique (AA)
            static void RasterizeCoverage(
                const NkShape& shape,
                uint8_t* out,
                int width,
                int height,
                int stride,
                int subSample = 4 // 4x4 par défaut
            );

            // Test point inside (non-zero winding)
            static bool PointInShape(const NkShape& shape, float x, float y);

        private:
            static float EdgeFunction(const math::NkVec2f& a, const math::NkVec2f& b, float x, float y);
    };

    struct NkMSDFPixel {
        float r, g, b;
    };

    class NkMSDFGenerator {
        public:
            static void GenerateMSDF(
                const NkShape& shape,
                NkMSDFPixel* out,
                int width,
                int height,
                float range
            );

        private:
            static float DistanceToEdge(const NkEdge& e, float x, float y);
            static float SignedDistance(const NkShape& shape, float x, float y);
    };

    class NkDilation {
        public:
            static void Apply(
                uint8_t* pixels,
                int width,
                int height,
                int stride,
                int iterations
            );
    };

    namespace nkfont {

        // ============================================================
        // BuildShapeFromGlyph
        // ============================================================

        /**
         * @brief Construit un NkShape à partir d'un glyphe (TTF/CFF)
         *
         * - Décode les vertices via GetGlyphShape
         * - Convertit les courbes en segments linéaires
         * - Produit une liste d'edges utilisable pour raster/SDF/MSDF
         */
        bool NkBuildShapeFromGlyph(
            const NkFontFaceInfo& face,
            NkGlyphId glyph,
            NkShape& outShape,
            float scale = 1.0f
        );

    } // namespace nkfont
} // namespace nkentseu

#endif