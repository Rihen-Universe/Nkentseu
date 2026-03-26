#pragma once
/**
 * @File    NkSVGCodec.h
 * @Brief   Parser SVG → rasterisation logicielle.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  SVG est un format vectoriel XML. Ce module lit un sous-ensemble
 *  de SVG 1.1 et rastérise en NkImage (RGBA32).
 *
 *  Éléments supportés :
 *    <svg>     — viewBox, width, height
 *    <rect>    — x, y, width, height, rx, ry, fill, stroke, stroke-width, opacity
 *    <circle>  — cx, cy, r, fill, stroke, opacity
 *    <ellipse> — cx, cy, rx, ry, fill, stroke
 *    <line>    — x1, y1, x2, y2, stroke, stroke-width
 *    <polyline>— points, stroke, fill
 *    <polygon> — points, fill, stroke
 *    <path>    — d (M, L, H, V, C, S, Q, T, A, Z)
 *    <g>       — groupes, transformations (translate, scale, rotate, matrix)
 *    <text>    — non supporté (nécessite NKFont)
 *    <image>   — non supporté
 *    <use>     — non supporté
 *
 *  Propriétés CSS supportées :
 *    fill, stroke, stroke-width, opacity, fill-opacity, stroke-opacity
 *    fill-rule (nonzero, evenodd)
 *    stroke-linecap (butt, round, square)
 *    stroke-linejoin (miter, round, bevel)
 *    display, visibility
 *    Transformations : translate, scale, rotate, matrix
 *
 *  Couleurs supportées :
 *    #RRGGBB, #RGB, rgb(r,g,b), rgba(r,g,b,a)
 *    Noms CSS : red, green, blue, white, black, none, transparent + 140 noms courants
 *
 *  Rasterisation :
 *    Lignes et contours : algorithme de Bresenham avec antialiasing (Wu)
 *    Remplissage : scanline avec règle non-zero ou evenodd
 *    Courbes de Bézier : décomposition adaptative en segments
 *    Arcs elliptiques : conversion en Bézier
 *
 *  Sauvegarde : génère du SVG textuel depuis une NkImage (pas de vecteur → vecteur).
 *    La sauvegarde encode l'image comme <image> base64 PNG embarquée.
 *
 * @Limitations
 *  - Pas de clip-path, mask, filter, gradient, pattern
 *  - Pas de CSS externe ni <style> complexe
 *  - Pas de symboles/defs réutilisables
 *  - La précision des transformations est float32
 */

#include "NKImage/Core/NkImage.h"

namespace nkentseu {

    // ── Couleur RGBA ──────────────────────────────────────────────────────────
    struct NkSVGColor {
        uint8 r=0, g=0, b=0, a=255;
        bool  none=false; // fill="none" ou stroke="none"

        static NkSVGColor Parse(const char* str) noexcept;
        static NkSVGColor Transparent() noexcept { NkSVGColor c; c.a=0; return c; }
        static NkSVGColor Black()       noexcept { return {0,0,0,255,false}; }
        static NkSVGColor White()       noexcept { return {255,255,255,255,false}; }
        static NkSVGColor None()        noexcept { NkSVGColor c; c.none=true; return c; }
    };

    // ── Style de dessin ───────────────────────────────────────────────────────
    struct NkSVGStyle {
        NkSVGColor fill         = NkSVGColor::Black();
        NkSVGColor stroke       = NkSVGColor::None();
        float32    strokeWidth  = 1.f;
        float32    opacity      = 1.f;
        float32    fillOpacity  = 1.f;
        float32    strokeOpacity= 1.f;
        bool       fillEvenOdd  = false;  // fill-rule evenodd
        uint8      linecap      = 0;      // 0=butt, 1=round, 2=square
        uint8      linejoin     = 0;      // 0=miter, 1=round, 2=bevel
        float32    miterLimit   = 4.f;
        bool       visible      = true;
    };

    // ── Matrice de transformation 3×3 ────────────────────────────────────────
    struct NkSVGTransform {
        float32 a=1,b=0,c=0,d=1,e=0,f=0; // [a c e; b d f; 0 0 1]

        static NkSVGTransform Identity()  noexcept { return {}; }
        static NkSVGTransform Translate(float32 tx,float32 ty) noexcept { return {1,0,0,1,tx,ty}; }
        static NkSVGTransform Scale(float32 sx,float32 sy)     noexcept { return {sx,0,0,sy,0,0}; }
        static NkSVGTransform Rotate(float32 deg) noexcept;
        static NkSVGTransform Parse(const char* str) noexcept;
        NkSVGTransform operator*(const NkSVGTransform& o) const noexcept;
        void Apply(float32& x, float32& y) const noexcept {
            const float32 nx=a*x+c*y+e, ny=b*x+d*y+f; x=nx; y=ny;
        }
    };

    // =========================================================================
    //  NkSVGCodec
    // =========================================================================

    class NKENTSEU_IMAGE_API NkSVGCodec {
    public:
        /**
         * @Brief Rastérise un fichier SVG en NkImage RGBA32.
         * @param data       Données SVG (texte XML UTF-8).
         * @param size       Taille en octets.
         * @param outW       Largeur de sortie. 0 = utilise viewBox.
         * @param outH       Hauteur de sortie. 0 = utilise viewBox.
         * @return Image RGBA32 allouée, nullptr si erreur.
         */
        static NkImage* Decode(const uint8* data, usize size,
                                int32 outW = 0, int32 outH = 0) noexcept;

        static NkImage* DecodeFromFile(const char* path,
                                        int32 outW = 0, int32 outH = 0) noexcept;

        /**
         * @Brief Encode une NkImage comme SVG contenant une <image> PNG base64.
         *        Ce n'est pas une vectorisation — c'est une encapsulation SVG.
         */
        static bool Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;
        static bool EncodeToFile(const NkImage& img, const char* path) noexcept;

    private:
        // ── Parser XML minimal ────────────────────────────────────────────────
        struct Attr { const char* name; const char* value; };
        struct Element {
            const char* tag;
            Attr*       attrs;
            int32       numAttrs;
        };

        // ── Contexte de rendu ─────────────────────────────────────────────────
        struct RenderCtx {
            NkImage*       img;
            float32        scaleX, scaleY;
            float32        viewX, viewY, viewW, viewH;
            NkSVGTransform ctm; // current transform matrix
            NkSVGStyle     style;
        };

        // ── Chemin (path) ─────────────────────────────────────────────────────
        struct PathPoint { float32 x,y; bool onCurve; };
        struct Contour { PathPoint* pts; int32 n; };
        struct Path {
            Contour* contours; int32 numContours;
            PathPoint* allPts; int32 totalPts;
        };

        // ── Parser XML simplifié ──────────────────────────────────────────────
        static const char* SkipWS(const char* p) noexcept;
        static bool  MatchTag(const char* p, const char* tag, const char** end) noexcept;
        static const char* ParseAttrValue(const char* p, char* buf, int32 bufLen) noexcept;
        static float32 ParseFloat(const char* s, const char** end=nullptr) noexcept;
        static float32 GetAttr(const Attr* attrs, int32 n, const char* name,
                                float32 defVal=0.f) noexcept;
        static const char* GetAttrStr(const Attr* attrs, int32 n,
                                       const char* name) noexcept;

        // ── Style ──────────────────────────────────────────────────────────────
        static NkSVGStyle ParseStyle(const Attr* attrs, int32 n,
                                      const NkSVGStyle& parent) noexcept;
        static void ParseInlineStyle(const char* css, NkSVGStyle& out) noexcept;

        // ── Rendu des primitives ──────────────────────────────────────────────
        static void RenderRect    (RenderCtx& ctx, const Attr* a, int32 n) noexcept;
        static void RenderCircle  (RenderCtx& ctx, const Attr* a, int32 n) noexcept;
        static void RenderEllipse (RenderCtx& ctx, const Attr* a, int32 n) noexcept;
        static void RenderLine    (RenderCtx& ctx, const Attr* a, int32 n) noexcept;
        static void RenderPolyline(RenderCtx& ctx, const Attr* a, int32 n, bool close) noexcept;
        static void RenderPath    (RenderCtx& ctx, const Attr* a, int32 n) noexcept;

        // ── Primitives de dessin ──────────────────────────────────────────────
        static void DrawLine    (NkImage& img, float32 x0,float32 y0,
                                  float32 x1,float32 y1,
                                  NkSVGColor col, float32 width) noexcept;
        static void FillPolygon (NkImage& img, const float32* xs, const float32* ys,
                                  int32 n, NkSVGColor col, bool evenOdd) noexcept;
        static void StrokePolyline(NkImage& img, const float32* xs, const float32* ys,
                                    int32 n, NkSVGColor col, float32 width,
                                    bool closed, uint8 cap, uint8 join) noexcept;
        static void BlendPixel  (NkImage& img, int32 x, int32 y,
                                  NkSVGColor col, float32 alpha=1.f) noexcept;
        static void DrawAALine  (NkImage& img, float32 x0, float32 y0,
                                  float32 x1, float32 y1,
                                  NkSVGColor col, float32 width) noexcept;

        // ── Path parser ───────────────────────────────────────────────────────
        static const char* ParsePathData(const char* d,
                                          float32* xs, float32* ys,
                                          int32* contourStarts, int32* contourLens,
                                          int32& numPts, int32& numContours,
                                          int32 maxPts, int32 maxContours) noexcept;
        static void BezierFlatten(float32 x0,float32 y0,
                                   float32 x1,float32 y1,
                                   float32 x2,float32 y2,
                                   float32 x3,float32 y3,
                                   float32* xs,float32* ys,int32& n,int32 maxN,
                                   int32 depth=0) noexcept;
        static void ArcToLines(float32 x1,float32 y1,float32 rx,float32 ry,
                                float32 xAngle,bool largeArc,bool sweep,
                                float32 x2,float32 y2,
                                float32* xs,float32* ys,int32& n,int32 maxN) noexcept;

        // ── Main parser ───────────────────────────────────────────────────────
        static void ParseAndRender(const char* xml, usize xmlLen,
                                    RenderCtx& ctx) noexcept;
        static void RenderElement(const char* tag, const Attr* attrs, int32 numAttrs,
                                   RenderCtx& ctx) noexcept;

        // ── Encodage base64 pour SVG export ──────────────────────────────────
        static usize Base64Encode(const uint8* in, usize inLen,
                                   char* out, usize outLen) noexcept;
    };

} // namespace nkentseu
