#include "NkUtils.h"
#include <algorithm>

namespace nkentseu {

    float NkRasterizer::EdgeFunction(const math::NkVec2f& a, const math::NkVec2f& b, float x, float y) {
        return (x - a.x) * (b.y - a.y) - (y - a.y) * (b.x - a.x);
    }

    bool NkRasterizer::PointInShape(const NkShape& shape, float x, float y) {
        int winding = 0;

        for (const auto& e : shape.edges) {
            if (e.p0.y <= y) {
                if (e.p1.y > y && EdgeFunction(e.p0, e.p1, x, y) > 0)
                    winding++;
            } else {
                if (e.p1.y <= y && EdgeFunction(e.p0, e.p1, x, y) < 0)
                    winding--;
            }
        }

        return winding != 0;
    }

    void NkRasterizer::RasterizeCoverage(
        const NkShape& shape,
        uint8_t* out,
        int width,
        int height,
        int stride,
        int subSample)
    {
        const float inv = 1.0f / (subSample * subSample);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {

                float coverage = 0.0f;

                for (int sy = 0; sy < subSample; sy++) {
                    for (int sx = 0; sx < subSample; sx++) {

                        float fx = x + (sx + 0.5f) / subSample;
                        float fy = y + (sy + 0.5f) / subSample;

                        if (PointInShape(shape, fx, fy))
                            coverage += 1.0f;
                    }
                }

                coverage *= inv;
                out[y * stride + x] = (uint8_t)(coverage * 255.0f);
            }
        }
    }

    float NkMSDFGenerator::DistanceToEdge(const NkEdge& e, float x, float y) {
        float vx = e.p1.x - e.p0.x;
        float vy = e.p1.y - e.p0.y;

        float wx = x - e.p0.x;
        float wy = y - e.p0.y;

        float t = (vx * wx + vy * wy) / (vx * vx + vy * vy);
        t = std::max(0.0f, std::min(1.0f, t));

        float px = e.p0.x + t * vx;
        float py = e.p0.y + t * vy;

        float dx = x - px;
        float dy = y - py;

        return sqrtf(dx * dx + dy * dy);
    }

    float NkMSDFGenerator::SignedDistance(const NkShape& shape, float x, float y) {
        float minDist = 1e9f;

        for (const auto& e : shape.edges) {
            float d = DistanceToEdge(e, x, y);
            if (d < minDist) minDist = d;
        }

        bool inside = NkRasterizer::PointInShape(shape, x, y);
        return inside ? minDist : -minDist;
    }

    void NkMSDFGenerator::GenerateMSDF(
        const NkShape& shape,
        NkMSDFPixel* out,
        int width,
        int height,
        float range)
    {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {

                float fx = x + 0.5f;
                float fy = y + 0.5f;

                float sd = SignedDistance(shape, fx, fy);

                float v = 0.5f + sd / range;
                v = std::max(0.0f, std::min(1.0f, v));

                out[y * width + x] = { v, v, v }; // version simple (à améliorer avec edge coloring)
            }
        }
    }

    void NkDilation::Apply(
        uint8_t* pixels,
        int width,
        int height,
        int stride,
        int iterations)
    {
        uint8_t* temp = new uint8_t[width * height];

        for (int it = 0; it < iterations; it++) {

            memcpy(temp, pixels, width * height);

            for (int y = 1; y < height - 1; y++) {
                for (int x = 1; x < width - 1; x++) {

                    uint8_t maxVal = 0;

                    for (int j = -1; j <= 1; j++) {
                        for (int i = -1; i <= 1; i++) {
                            uint8_t v = temp[(y + j) * stride + (x + i)];
                            maxVal = std::max(maxVal, v);
                        }
                    }

                    pixels[y * stride + x] = maxVal;
                }
            }
        }

        delete[] temp;
    }

    namespace nkfont {

        // ============================================================
        // Utils : subdivision courbes
        // ============================================================

        static void NkAddLine(NkShape& shape, const math::NkVec2f& a, const math::NkVec2f& b) {
            NkEdge e;
            e.p0 = a;
            e.p1 = b;
            shape.edges.push_back(e);
        }

        // Subdivision quadratique (Bezier)
        static void NkFlattenQuadratic(
            const math::NkVec2f& p0,
            const math::NkVec2f& p1,
            const math::NkVec2f& p2,
            float tolerance,
            NkShape& shape)
        {
            float dx = p2.x - p0.x;
            float dy = p2.y - p0.y;

            float d = fabsf((p1.x - p2.x) * dy - (p1.y - p2.y) * dx);

            if (d * d <= tolerance * (dx * dx + dy * dy)) {
                NkAddLine(shape, p0, p2);
                return;
            }

            // subdivision
            math::NkVec2f p01 = { (p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f };
            math::NkVec2f p12 = { (p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f };
            math::NkVec2f p012 = { (p01.x + p12.x) * 0.5f, (p01.y + p12.y) * 0.5f };

            NkFlattenQuadratic(p0, p01, p012, tolerance, shape);
            NkFlattenQuadratic(p012, p12, p2, tolerance, shape);
        }

        // Subdivision cubique (CFF)
        static void NkFlattenCubic(
            const math::NkVec2f& p0,
            const math::NkVec2f& p1,
            const math::NkVec2f& p2,
            const math::NkVec2f& p3,
            float tolerance,
            NkShape& shape)
        {
            float dx = p3.x - p0.x;
            float dy = p3.y - p0.y;

            float d1 = fabsf((p1.x - p3.x) * dy - (p1.y - p3.y) * dx);
            float d2 = fabsf((p2.x - p3.x) * dy - (p2.y - p3.y) * dx);

            if ((d1 + d2) * (d1 + d2) <= tolerance * (dx * dx + dy * dy)) {
                NkAddLine(shape, p0, p3);
                return;
            }

            // subdivision De Casteljau
            math::NkVec2f p01 = { (p0.x + p1.x)*0.5f, (p0.y + p1.y)*0.5f };
            math::NkVec2f p12 = { (p1.x + p2.x)*0.5f, (p1.y + p2.y)*0.5f };
            math::NkVec2f p23 = { (p2.x + p3.x)*0.5f, (p2.y + p3.y)*0.5f };

            math::NkVec2f p012 = { (p01.x + p12.x)*0.5f, (p01.y + p12.y)*0.5f };
            math::NkVec2f p123 = { (p12.x + p23.x)*0.5f, (p12.y + p23.y)*0.5f };

            math::NkVec2f p0123 = { (p012.x + p123.x)*0.5f, (p012.y + p123.y)*0.5f };

            NkFlattenCubic(p0, p01, p012, p0123, tolerance, shape);
            NkFlattenCubic(p0123, p123, p23, p3, tolerance, shape);
        }

        // ============================================================
        // BuildShapeFromGlyph
        // ============================================================

        bool NkBuildShapeFromGlyph(
            const NkFontFaceInfo& face,
            NkGlyphId glyph,
            NkShape& outShape,
            float scale)
        {
            NkFontVertexBuffer buf;
            buf.Clear();

            if (!NkGetGlyphShape(&face, glyph, &buf))
                return false;

            if (buf.count == 0)
                return false;

            outShape.edges.clear();

            math::NkVec2f pen = {0, 0};
            math::NkVec2f start = {0, 0};

            const float tolerance = 0.25f;

            for (nkft_uint32 i = 0; i < buf.count; ++i) {

                const NkFontVertex& v = buf.verts[i];

                math::NkVec2f p = {
                    v.x * scale,
                    v.y * scale
                };

                switch (v.type) {

                    case NK_FONT_VERTEX_MOVE:
                    {
                        pen = p;
                        start = p;
                    } break;

                    case NK_FONT_VERTEX_LINE:
                    {
                        NkAddLine(outShape, pen, p);
                        pen = p;
                    } break;

                    case NK_FONT_VERTEX_CURVE:
                    {
                        math::NkVec2f c = {
                            v.cx * scale,
                            v.cy * scale
                        };

                        NkFlattenQuadratic(pen, c, p, tolerance, outShape);
                        pen = p;
                    } break;

                    case NK_FONT_VERTEX_CUBIC:
                    {
                        math::NkVec2f c0 = {
                            v.cx * scale,
                            v.cy * scale
                        };

                        math::NkVec2f c1 = {
                            v.cx1 * scale,
                            v.cy1 * scale
                        };

                        NkFlattenCubic(pen, c0, c1, p, tolerance, outShape);
                        pen = p;
                    } break;
                }

                // fermeture de contour
                if (i + 1 == buf.count || buf.verts[i + 1].type == NK_FONT_VERTEX_MOVE) {
                    NkAddLine(outShape, pen, start);
                }
            }

            return true;
        }

    } // namespace nkfont
}