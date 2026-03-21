#pragma once
/**
 * @File    NkSVGRenderer.h
 * @Brief   Rasteriseur SVG — parcourt NkSVGDOM et produit une NkImage RGBA32.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkSVGRenderer parcourt l'arbre NkSVGElement en profondeur (DFS) et
 *  rastérise chaque primitive dans une NkImage RGBA32.
 *
 *  Primitives rendues :
 *    rect (avec rx/ry), circle, ellipse, line, polyline, polygon, path
 *    Remplissage : scanline avec règle NonZero/EvenOdd
 *    Contour     : stroke avec linecap (butt/round/square)
 *                  et linejoin (miter/round/bevel)
 *    Dash        : stroke-dasharray + stroke-dashoffset
 *
 *  Couleurs :
 *    NkSVGPaint::Color direct
 *    NkSVGPaint::URL   → gradient linéaire ou radial (depuis NkSVGDefs)
 *    Opacité : fill-opacity, stroke-opacity, opacity (groupe)
 *
 *  Gradients :
 *    Linéaire : objectBoundingBox ou userSpaceOnUse
 *    Radial   : objectBoundingBox ou userSpaceOnUse, focal point
 *    GradientTransform appliqué
 *    Spread : pad, reflect, repeat
 *
 *  Transformations :
 *    CTM cumulé depuis la racine (déjà calculé dans NkSVGElement::ctm)
 *    Appliqué à chaque point avant le raster
 *
 *  Anti-aliasing :
 *    Super-sampling 4×4 (16 samples par pixel) pour fill et stroke
 *
 *  ClipPath :
 *    Masque alpha calculé depuis les enfants du clipPath,
 *    appliqué pixel par pixel lors du blit
 *
 *  Non supporté dans cette version :
 *    Filtres (feGaussianBlur, etc.) — phase suivante
 *    Texte — intégration NKFont — phase suivante
 *    <image> embarquée — phase suivante
 *    Marqueurs (marker-start/end/mid)
 */

#include "NkSVGDOM.h"
#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    // ─────────────────────────────────────────────────────────────────────────────
    //  Options de rendu
    // ─────────────────────────────────────────────────────────────────────────────

    struct NkSVGRenderOptions {
        int32   superSample = 2;   // 1=none, 2=2×2, 4=4×4
        uint8   bgR=255,bgG=255,bgB=255,bgA=255; // fond (blanc opaque par défaut)
        bool    transparentBg = false; // si true, fond transparent
        float   dpi = 96.f;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    //  NkSVGRenderer
    // ─────────────────────────────────────────────────────────────────────────────

    class NKENTSEU_IMAGE_API NkSVGRenderer {
        public:
            /**
             * @Brief Rastérise un NkSVGDOM en NkImage RGBA32.
             * @param dom   DOM SVG complet (construit par NkSVGDOMBuilder).
             * @param opts  Options de rendu.
             * @return NkImage RGBA32 allouée (à libérer avec Free()), nullptr si erreur.
             */
            static NkImage* Render(const NkSVGDOM& dom,
                                    const NkSVGRenderOptions& opts = {}) noexcept;

            /**
             * @Brief Raccourci : parse XML + build DOM + render en une étape.
             */
            static NkImage* RenderFromXML(const uint8* xml, usize size,
                                        int32 outW=0, int32 outH=0,
                                        const NkSVGRenderOptions& opts = {}) noexcept;
            static NkImage* RenderFromFile(const char* path,
                                            int32 outW=0, int32 outH=0,
                                            const NkSVGRenderOptions& opts = {}) noexcept;

        private:
            // ── Contexte de rendu ─────────────────────────────────────────────────────
            struct RCtx {
                NkImage*          img;
                const NkSVGDOM*   dom;
                NkSVGRenderOptions opts;
                int32             w, h;
            };

            // ── Rendu récursif ────────────────────────────────────────────────────────
            static void RenderElement(const NkSVGElement* elem, RCtx& ctx) noexcept;
            static void RenderRect    (const NkSVGElement* e, RCtx& ctx) noexcept;
            static void RenderCircle  (const NkSVGElement* e, RCtx& ctx) noexcept;
            static void RenderEllipse (const NkSVGElement* e, RCtx& ctx) noexcept;
            static void RenderLine    (const NkSVGElement* e, RCtx& ctx) noexcept;
            static void RenderPolyline(const NkSVGElement* e, RCtx& ctx, bool close) noexcept;
            static void RenderPath    (const NkSVGElement* e, RCtx& ctx) noexcept;

            // ── Pipeline de rendu d'une forme ─────────────────────────────────────────
            struct Shape {
                float*  xs;
                float*  ys;
                int32*  contourStart;   // indices de début de chaque contour
                int32*  contourLen;     // nombre de points par contour
                int32   numPoints;
                int32   numContours;
                // Bounding box calculée
                float   bboxX0,bboxY0,bboxX1,bboxY1;
            };

            static void ComputeBBox(Shape& s) noexcept;
            static void ApplyCTM(Shape& s, const NkSVGMatrix& ctm) noexcept;

            static void FillShape  (NkImage& img, const Shape& s,
                                    const NkSVGStyle& style,
                                    const NkSVGDefs& defs,
                                    int32 superSample) noexcept;
            static void StrokeShape(NkImage& img, const Shape& s,
                                    const NkSVGStyle& style,
                                    const NkSVGDefs& defs,
                                    int32 superSample) noexcept;

            // ── Résolution des couleurs ───────────────────────────────────────────────
            static void SamplePaint(const NkSVGPaint& paint,
                                    const NkSVGDefs& defs,
                                    float x, float y,
                                    float bboxX0,float bboxY0,float bboxX1,float bboxY1,
                                    float opacity,
                                    uint8& R, uint8& G, uint8& B, uint8& A) noexcept;

            // ── Scanline fill ─────────────────────────────────────────────────────────
            static void FillPolygon(NkImage& img,
                                    const float* xs, const float* ys, int32 n,
                                    const NkSVGPaint& paint, const NkSVGDefs& defs,
                                    float bboxX0,float bboxY0,float bboxX1,float bboxY1,
                                    float opacity, NkSVGFillRule rule,
                                    int32 superSample) noexcept;

            // ── Stroke ────────────────────────────────────────────────────────────────
            static void StrokePolyline(NkImage& img,
                                        const float* xs, const float* ys, int32 n,
                                        bool closed,
                                        const NkSVGPaint& paint, const NkSVGDefs& defs,
                                        float bboxX0,float bboxY0,float bboxX1,float bboxY1,
                                        float width, float opacity,
                                        NkSVGLineCap cap, NkSVGLineJoin join,
                                        float miterLimit,
                                        const float* dash, int32 dashCount, float dashOffset,
                                        int32 superSample) noexcept;

            // ── Primitive de pixel ────────────────────────────────────────────────────
            static void BlendPixel(NkImage& img, int32 x, int32 y,
                                    uint8 r, uint8 g, uint8 b, uint8 a) noexcept;
            static void DrawLineAA(NkImage& img,
                                    float x0, float y0, float x1, float y1,
                                    uint8 r, uint8 g, uint8 b, uint8 a,
                                    float width) noexcept;

            // ── Path parser ───────────────────────────────────────────────────────────
            static bool ParsePath(const char* d,
                                float* xs, float* ys,
                                int32* cStart, int32* cLen,
                                int32& numPts, int32& numContours,
                                int32 maxPts, int32 maxContours) noexcept;
            static void BezierFlatten(float x0,float y0,float x1,float y1,
                                    float x2,float y2,float x3,float y3,
                                    float* xs, float* ys, int32& n, int32 max,
                                    int32 depth=0) noexcept;
            static void ArcToLines   (float x1,float y1,float rx,float ry,
                                    float xAngle, bool la, bool sw,
                                    float x2, float y2,
                                    float* xs, float* ys, int32& n, int32 max) noexcept;
            static float ParseCoord   (const char*& p) noexcept;

            // ── Formes géométriques → polylignes ─────────────────────────────────────
            static void RectToPoints   (float x,float y,float w,float h,float rx,float ry,
                                        float* xs, float* ys, int32& n, int32 max) noexcept;
            static void CircleToPoints (float cx,float cy,float r,
                                        float* xs, float* ys, int32& n, int32 max) noexcept;
            static void EllipseToPoints(float cx,float cy,float rx,float ry,
                                        float* xs, float* ys, int32& n, int32 max) noexcept;
            static void PolygonFromAttr(const char* pts,
                                        float* xs, float* ys, int32& n, int32 max) noexcept;
    };

} // namespace nkentseu
