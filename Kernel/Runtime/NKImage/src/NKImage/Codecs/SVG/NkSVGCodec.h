#pragma once
// =============================================================================
// NkSVGCodec.h
// -----------------------------------------------------------------------------
// Rasterisation logicielle de SVG vers NkImage RGBA32.
//
// API publique simple :
//   - NkSVGCodec::Decode(data, size, outW, outH)      : XML buffer -> NkImage
//   - NkSVGCodec::DecodeFromFile(path, outW, outH)    : .svg disque -> NkImage
//   - NkSVGCodec::Encode(img, out, outSize)           : NkImage -> SVG enrobe
//   - NkSVGCodec::EncodeToFile(img, path)             : NkImage -> .svg disque
//
// Implementation : single-file dans NkSVGCodec.cpp, inspiree de NanoSVG
// (Mikko Mononen), reecrite from scratch 2026-05-19 pour gerer correctement :
//   - Cascade <g> transform + style (push/pop sur fermeture </g>)
//   - Path data de taille arbitraire (>24 KB pour text-to-path)
//   - Rasterizer scanline AA (supersample 2x) avec fill-rule nonzero/evenodd
//   - Beziers cubiques/quadratiques avec subdivision adaptative
//   - Arc elliptique converti en Beziers
//
// Elements supportes : <svg> <g> <path> <rect> <circle> <ellipse> <line>
//                      <polyline> <polygon> <linearGradient> <radialGradient> <stop>
// Attributs styles  : fill, stroke, stroke-width, opacity, fill-opacity,
//                     stroke-opacity, fill-rule, transform,
//                     stroke-linecap (butt/round/square),
//                     stroke-linejoin (miter/round/bevel), stroke-miterlimit
// Gradients         : linear + radial, stops (offset/stop-color/stop-opacity),
//                     gradientUnits (objectBoundingBox + userSpaceOnUse),
//                     gradientTransform, spreadMethod (pad/reflect/repeat),
//                     fill/stroke="url(#id)", href (stops herites)
// Couleurs          : #RGB, #RRGGBB, #RRGGBBAA, rgb(), rgba(), nom CSS (148 noms)
//
// Pas supporte (Phase 3 +) : <text>, <use>, <defs><style> (classes CSS),
// patterns, masks, clipPath, filters.
//
// Auteur : Rihen (reecriture 2026-05-19), inspire de NanoSVG par Mikko Mononen.
// =============================================================================

#include "NKImage/Core/NkImage.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {

    // X11/Xlib definit la macro 'None' (=0L) qui peut fuiter (via un header GUI inclus
    // avant celui-ci) et casse NkSVGColor::None(). NKImage n'utilise jamais X11 -> on
    // neutralise la macro si presente. (Collision classique X11 <-> identifiants C++.)
    #ifdef None
    #  undef None
    #endif

    // ── Couleur RGBA pour parsing SVG ─────────────────────────────────────────
    struct NkSVGColor {
        uint8 r=0, g=0, b=0, a=255;
        bool  none=false;  ///< fill="none" ou stroke="none"

        static NkSVGColor Parse(const char* str) noexcept;
        static NkSVGColor Transparent() noexcept { NkSVGColor c; c.a=0; return c; }
        static NkSVGColor Black()       noexcept { return {0,0,0,255,false}; }
        static NkSVGColor White()       noexcept { return {255,255,255,255,false}; }
        static NkSVGColor None()        noexcept { NkSVGColor c; c.none=true; return c; }
    };

    // ── Terminaisons / jointures de trait (stroke-linecap / stroke-linejoin) ──
    enum class NkSVGLineCap  : uint8 { Butt = 0, Round, Square };
    enum class NkSVGLineJoin : uint8 { Miter = 0, Round, Bevel };

    // ── Style de dessin (cumulable via cascade <g>) ───────────────────────────
    struct NkSVGStyle {
        NkSVGColor    fill          = NkSVGColor::Black();
        NkSVGColor    stroke        = NkSVGColor::None();
        float32       strokeWidth   = 1.f;
        float32       opacity       = 1.f;
        float32       fillOpacity   = 1.f;
        float32       strokeOpacity = 1.f;
        bool          fillEvenOdd   = false;  ///< fill-rule="evenodd"
        bool          visible       = true;
        NkSVGLineCap  strokeLineCap  = NkSVGLineCap::Butt;   ///< stroke-linecap
        NkSVGLineJoin strokeLineJoin = NkSVGLineJoin::Miter; ///< stroke-linejoin
        float32       strokeMiterLimit = 4.f;                ///< stroke-miterlimit
    };

    // ── Matrice affine 2D (a,b,c,d,e,f) = [a c e; b d f; 0 0 1] ──────────────
    struct NkSVGTransform {
        float32 a=1, b=0, c=0, d=1, e=0, f=0;

        static NkSVGTransform Identity()                          noexcept { return {}; }
        static NkSVGTransform Translate(float32 tx, float32 ty)   noexcept { return {1,0,0,1,tx,ty}; }
        static NkSVGTransform Scale    (float32 sx, float32 sy)   noexcept { return {sx,0,0,sy,0,0}; }
        static NkSVGTransform Rotate   (float32 deg)              noexcept;
        static NkSVGTransform Parse    (const char* str)          noexcept;
        NkSVGTransform operator*(const NkSVGTransform& o) const   noexcept;
        void Apply(float32& x, float32& y) const noexcept
        {
            const float32 nx = a*x + c*y + e;
            const float32 ny = b*x + d*y + f;
            x = nx; y = ny;
        }
    };

    // =========================================================================
    // NkSVGShapeView — Vue read-only sur une shape vectorielle parsee
    // -------------------------------------------------------------------------
    // Permet d'acceder aux vertices/contours sans materialiser une copie. Les
    // pointeurs retournes restent valides tant que le NkSVGImage parent est
    // vivant. Coords en espace SVG (avant viewBox->output scaling), utile pour
    // generation de mesh 3D, polygons collisions, extrusion, etc.
    // =========================================================================
    class NKENTSEU_IMAGE_API NkSVGShapeView
    {
    public:
        NkSVGShapeView() noexcept = default;

        /// Nombre de contours dans cette shape (typiquement 1 pour un path
        /// simple, N pour fill-rule evenodd avec trous, etc.).
        int32 ContourCount() const noexcept;

        /// Nombre de vertices d'un contour donne.
        int32 ContourPointCount(int32 contourIdx) const noexcept;

        /// Pointeur vers les coordonnees X du contour (durabilite = vie du
        /// NkSVGImage parent). Coords en espace SVG.
        const float32* ContourXs(int32 contourIdx) const noexcept;
        const float32* ContourYs(int32 contourIdx) const noexcept;

        /// Style fill/stroke applique a cette shape (cascade + transform deja
        /// appliques aux vertices).
        NkSVGColor FillColor()    const noexcept;
        NkSVGColor StrokeColor()  const noexcept;
        float32    StrokeWidth()  const noexcept;
        float32    Opacity()      const noexcept;
        bool       FillEvenOdd()  const noexcept;

        /// Triangulation par ear-clipping (par contour separe, sans gestion
        /// de trous pour cette version). Produit des indices triangle-list
        /// pretes pour upload GPU. @p baseIndex permet de chainer plusieurs
        /// shapes dans le meme buffer (= taille de outXs avant l'appel).
        /// @return Nombre de triangles produits (== outIndices.Size()/3 delta).
        int32 Triangulate(NkVector<float32>& outXs,
                          NkVector<float32>& outYs,
                          NkVector<uint32>&  outIndices,
                          uint32 baseIndex = 0) const noexcept;

        /// True si la vue pointe sur une shape valide.
        bool IsValid() const noexcept { return mShapePtr != nullptr; }

    private:
        friend class NkSVGImage;
        const void* mShapePtr = nullptr;  // pointe sur Shape interne
    };

    // =========================================================================
    // NkSVGImage — Representation vectorielle d'un SVG, rasterisable a la demande
    // -------------------------------------------------------------------------
    // Apres parsing, le SVG est stocke comme une liste de shapes (paths +
    // styles + transforms) en memoire. On peut ensuite appeler Rasterize(W,H)
    // pour generer une NkImage RGBA a la resolution souhaitee SANS perte
    // (chaque appel re-rasterise depuis les shapes vectoriels).
    //
    // Usage typique :
    //   NkSVGImage* svg = NkSVGImage::LoadFromFile("logo.svg");
    //   if (svg) {
    //       NkImage* img100  = svg->Rasterize(100, 50);    // mini
    //       NkImage* img2000 = svg->Rasterize(2000, 1000); // hi-res
    //       // ... use img100 / img2000 ...
    //       img100->Free();  img2000->Free();
    //       svg->Free();
    //   }
    //
    // Stockage opaque : les internes (Shape) ne sont pas exposes -- on
    // expose uniquement la taille naturelle (Width/Height depuis viewBox) +
    // la rasterization.
    // =========================================================================
    class NKENTSEU_IMAGE_API NkSVGImage
    {
    public:
        /// Lit + parse un fichier .svg. Retourne nullptr si erreur.
        /// Caller doit appeler ->Free() pour liberer.
        static NkSVGImage* LoadFromFile(const char* path) noexcept;

        /// Parse depuis un buffer en memoire. Caller doit appeler ->Free().
        static NkSVGImage* LoadFromMemory(const uint8* data, usize size) noexcept;

        /// Rasterise les shapes a la resolution (outW, outH). Si outW=0 ou
        /// outH=0, calcule la taille manquante en preservant l'aspect ratio.
        /// Retourne un NkImage RGBA32 alloue (caller appelle ->Free()), nullptr si KO.
        NkImage* Rasterize(int32 outW, int32 outH) const noexcept;

        /// Taille naturelle (depuis viewBox ou width/height du <svg>).
        int32 NaturalWidth()  const noexcept;
        int32 NaturalHeight() const noexcept;

        /// Nombre de shapes parsees (paths/rect/circle/ellipse/line/polygon).
        int32 ShapeCount() const noexcept;

        /// Vue read-only sur la i-eme shape. Pour usage 3D / mesh / collision.
        NkSVGShapeView GetShape(int32 idx) const noexcept;

        /// Triangulation globale de toutes les shapes en un seul mesh.
        /// Produit des coordonnees vertices (xs/ys en espace SVG) et indices
        /// triangle-list pretes pour upload GPU. Couleurs par triangle dans
        /// outTriColors (RGBA8888 packed uint32).
        bool TriangulateAll(NkVector<float32>& outXs,
                            NkVector<float32>& outYs,
                            NkVector<uint32>&  outIndices,
                            NkVector<uint32>&  outTriColors) const noexcept;

        /// Libere les ressources (shapes + image vectorielle).
        void Free() noexcept;

    private:
        NkSVGImage()  noexcept = default;
        ~NkSVGImage() noexcept = default;
        NkSVGImage(const NkSVGImage&) = delete;
        NkSVGImage& operator=(const NkSVGImage&) = delete;
        // PIMPL : la structure interne (NkVector<Shape> + viewBox) est dans le
        // .cpp pour eviter d'exposer Shape dans l'API publique.
        void* mImpl = nullptr;
    };

    // =========================================================================
    // NkSVGCodec — API publique. L'implementation interne est totalement
    // privee au .cpp (pas de declarations dans le .h pour eviter le couplage).
    // =========================================================================
    class NKENTSEU_IMAGE_API NkSVGCodec
    {
    public:
        /// Decode un buffer SVG (XML UTF-8) en NkImage RGBA32 alloue sur le heap.
        /// @param data    Buffer XML UTF-8.
        /// @param size    Taille en octets.
        /// @param outW    Largeur de sortie en pixels (0 = taille SVG ou viewBox).
        /// @param outH    Hauteur de sortie (0 = idem largeur).
        /// @return        NkImage RGBA32 alloue (caller appelle ->Free()), ou nullptr.
        static NkImage* Decode(const uint8* data, usize size,
                               int32 outW = 0, int32 outH = 0) noexcept;

        /// Lit un fichier .svg disque et le rasterise.
        static NkImage* DecodeFromFile(const char* path,
                                       int32 outW = 0, int32 outH = 0) noexcept;

        /// Encode une NkImage en SVG : enrobe l'image comme <image href="data:png;base64,...">
        /// dans un <svg> de la meme taille. **Pas une vectorisation** -- conserve
        /// le pixel-perfect via PNG base64. Utile pour export/round-trip.
        static bool Encode(const NkImage& img, uint8*& out, usize& outSize) noexcept;
        static bool EncodeToFile(const NkImage& img, const char* path) noexcept;
    };

} // namespace nkentseu
