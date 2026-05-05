#pragma once
// =============================================================================
// Nkentseu/Design/Vector/NkVectorPath.h
// =============================================================================
// Chemin vectoriel complet pour les applications de design (Illustrator-like).
//
// RENDU :
//   NkVectorPath produit des polygones tessellés passés à NkRender2D::FillPolygon
//   et NkRender2D::DrawPolyline. Aucune modification de NKRenderer requise.
//
// MODÈLE DE DONNÉES (inspiré de SVG) :
//   Un chemin = séquence de sous-chemins (contours).
//   Chaque sous-chemin = séquence de commandes.
//   Les commandes sont stockées de façon compacte (variant-like).
//
// FILL RULES :
//   NonZero : standard Illustrator (rempli si le winding ≠ 0)
//   EvenOdd : alterné Photoshop/PDF (intérieur/extérieur alternent)
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKContainers/String/NkString.h"
#include "NKRenderer/Tools/Render2D/NkRender2D.h"

namespace nkentseu {
    using namespace math;

    // =========================================================================
    // Enums de style
    // =========================================================================
    enum class NkFillRule  : uint8 { NonZero, EvenOdd };
    enum class NkStrokeCap : uint8 { Butt, Round, Square };
    enum class NkStrokeJoin: uint8 { Miter, Round, Bevel };

    // =========================================================================
    // NkPaint — système de remplissage avancé
    // =========================================================================
    struct NkGradientStop {
        float32 offset;      ///< [0..1]
        NkVec4f color;       ///< RGBA [0..1]
    };

    struct NkPaint {
        enum class Type : uint8 {
            None = 0, Solid, LinearGradient, RadialGradient,
            ConicGradient, Pattern, ImageFill
        };

        Type    type         = Type::Solid;
        NkVec4f solidColor   = {0, 0, 0, 1};  ///< Pour Type::Solid

        // Gradient
        NkVec2f gradientStart = {0, 0};        ///< Espace local [0..1] du bounding box
        NkVec2f gradientEnd   = {1, 0};
        float32 gradientRadius = 0.5f;         ///< Pour radial

        static constexpr uint32 kMaxStops = 16u;
        NkGradientStop stops[kMaxStops] = {};
        uint32         stopCount        = 0;

        // Image fill
        nk_uint64 imageHandle = 0;
        float32   imageScaleX = 1.f;
        float32   imageScaleY = 1.f;

        // Factory helpers
        static NkPaint Solid(NkVec4f color) noexcept {
            NkPaint p; p.type = Type::Solid; p.solidColor = color; return p;
        }
        static NkPaint Black() noexcept { return Solid({0,0,0,1}); }
        static NkPaint White() noexcept { return Solid({1,1,1,1}); }
        static NkPaint Transparent() noexcept { return Solid({0,0,0,0}); }

        static NkPaint LinearGrad(NkVec2f from, NkVec2f to,
                                   NkVec4f c0, NkVec4f c1) noexcept {
            NkPaint p;
            p.type = Type::LinearGradient;
            p.gradientStart = from; p.gradientEnd = to;
            p.stops[0] = {0.f, c0}; p.stops[1] = {1.f, c1}; p.stopCount = 2;
            return p;
        }
    };

    // =========================================================================
    // NkStrokeStyle — style du contour
    // =========================================================================
    struct NkStrokeStyle {
        float32       width       = 1.f;
        NkStrokeCap   lineCap     = NkStrokeCap::Round;
        NkStrokeJoin  lineJoin    = NkStrokeJoin::Round;
        float32       miterLimit  = 4.f;

        // Tirets (dash pattern)
        static constexpr uint32 kMaxDash = 8u;
        float32 dash[kMaxDash] = {};
        uint32  dashCount      = 0;
        float32 dashOffset     = 0.f;

        bool    enabled        = true;
    };

    // =========================================================================
    // NkPathCmd — commande de chemin (variant compact)
    // =========================================================================
    enum class NkPathCmdType : uint8 {
        MoveTo = 0,    ///< M x y
        LineTo,        ///< L x y
        CubicTo,       ///< C cx1 cy1 cx2 cy2 x y
        QuadTo,        ///< Q cx cy x y
        ArcTo,         ///< A rx ry xRot largeArc sweep x y
        Close,         ///< Z
    };

    struct NkPathCmd {
        NkPathCmdType type = NkPathCmdType::MoveTo;
        float32 pts[6]     = {};  // Suffisant pour CubicTo (6 floats)
        bool    largeArc   = false;
        bool    sweepFlag  = false;

        static NkPathCmd Move (float32 x, float32 y) noexcept {
            NkPathCmd c; c.type=NkPathCmdType::MoveTo; c.pts[0]=x; c.pts[1]=y; return c;
        }
        static NkPathCmd Line (float32 x, float32 y) noexcept {
            NkPathCmd c; c.type=NkPathCmdType::LineTo; c.pts[0]=x; c.pts[1]=y; return c;
        }
        static NkPathCmd Cubic(float32 cx1, float32 cy1,
                                float32 cx2, float32 cy2,
                                float32 x,   float32 y) noexcept {
            NkPathCmd c; c.type=NkPathCmdType::CubicTo;
            c.pts[0]=cx1; c.pts[1]=cy1; c.pts[2]=cx2;
            c.pts[3]=cy2; c.pts[4]=x;   c.pts[5]=y; return c;
        }
        static NkPathCmd Quad (float32 cx, float32 cy, float32 x, float32 y) noexcept {
            NkPathCmd c; c.type=NkPathCmdType::QuadTo;
            c.pts[0]=cx; c.pts[1]=cy; c.pts[2]=x; c.pts[3]=y; return c;
        }
        static NkPathCmd CloseCmd() noexcept {
            NkPathCmd c; c.type=NkPathCmdType::Close; return c;
        }
    };

    // =========================================================================
    // NkVectorPath — chemin vectoriel complet
    // =========================================================================
    class NkVectorPath {
        public:
            NkVectorPath()  noexcept = default;
            ~NkVectorPath() noexcept = default;

            // ── Construction du chemin (API SVG-like) ─────────────────────
            NkVectorPath& MoveTo (float32 x, float32 y)  noexcept;
            NkVectorPath& LineTo (float32 x, float32 y)  noexcept;
            NkVectorPath& CubicTo(float32 cx1, float32 cy1,
                                   float32 cx2, float32 cy2,
                                   float32 x,   float32 y)  noexcept;
            NkVectorPath& QuadTo (float32 cx,  float32 cy,
                                   float32 x,   float32 y)  noexcept;
            NkVectorPath& ArcTo  (float32 rx, float32 ry,
                                   float32 xRotDeg,
                                   bool largeArc, bool sweep,
                                   float32 x, float32 y)    noexcept;
            NkVectorPath& Close  ()                          noexcept;

            // ── Formes de haut niveau ─────────────────────────────────────
            NkVectorPath& AddRect       (float32 x,float32 y,float32 w,float32 h,
                                          float32 radius=0.f) noexcept;
            NkVectorPath& AddCircle     (float32 cx,float32 cy,float32 r) noexcept;
            NkVectorPath& AddEllipse    (float32 cx,float32 cy,
                                          float32 rx,float32 ry) noexcept;
            NkVectorPath& AddStar       (float32 cx,float32 cy,
                                          float32 outerR,float32 innerR,
                                          uint32 points) noexcept;
            NkVectorPath& AddRoundedRect(float32 x,float32 y,float32 w,float32 h,
                                          float32 tl,float32 tr,
                                          float32 br,float32 bl) noexcept;

            // ── Transform ─────────────────────────────────────────────────
            NkVectorPath& Transform(const NkMat3f& matrix) noexcept;
            NkVectorPath& Translate(float32 tx, float32 ty) noexcept;
            NkVectorPath& Scale    (float32 sx, float32 sy) noexcept;
            NkVectorPath& Rotate   (float32 angleDeg, float32 cx=0, float32 cy=0) noexcept;

            // ── Opérations booléennes ─────────────────────────────────────
            [[nodiscard]] static NkVectorPath Union    (const NkVectorPath& a, const NkVectorPath& b) noexcept;
            [[nodiscard]] static NkVectorPath Subtract (const NkVectorPath& a, const NkVectorPath& b) noexcept;
            [[nodiscard]] static NkVectorPath Intersect(const NkVectorPath& a, const NkVectorPath& b) noexcept;
            [[nodiscard]] static NkVectorPath Exclude  (const NkVectorPath& a, const NkVectorPath& b) noexcept;

            // ── Métriques ─────────────────────────────────────────────────
            [[nodiscard]] NkAABB2f GetBoundingBox()    const noexcept;
            [[nodiscard]] float32  GetLength()         const noexcept;
            [[nodiscard]] NkVec2f  PointAtLength(float32 t) const noexcept;  ///< t∈[0..1]
            [[nodiscard]] NkVec2f  TangentAtLength(float32 t) const noexcept;
            [[nodiscard]] bool     Contains(float32 x, float32 y) const noexcept;

            // ── Tessellation → NkRender2D ─────────────────────────────────
            /**
             * @brief Tesselle le fill et le dessine via NkRender2D.
             */
            void DrawFill(renderer::NkRender2D& r,
                          const NkPaint& fill,
                          NkFillRule rule = NkFillRule::NonZero,
                          float32 tolerance = 0.5f) const noexcept;

            /**
             * @brief Tesselle le stroke et le dessine via NkRender2D.
             */
            void DrawStroke(renderer::NkRender2D& r,
                             const NkPaint& paint,
                             const NkStrokeStyle& stroke) const noexcept;

            /**
             * @brief Dessine fill + stroke en un appel.
             */
            void Draw(renderer::NkRender2D& r,
                       const NkPaint& fill,
                       const NkPaint& strokePaint,
                       const NkStrokeStyle& stroke,
                       NkFillRule rule = NkFillRule::NonZero) const noexcept;

            // ── Sérialisation SVG ─────────────────────────────────────────
            [[nodiscard]] NkString ToSVGPath() const noexcept;
            static NkVectorPath    FromSVGPath(const char* d) noexcept;

            // ── Accès ─────────────────────────────────────────────────────
            [[nodiscard]] bool  IsEmpty()  const noexcept { return mCmds.IsEmpty(); }
            [[nodiscard]] uint32 CmdCount()const noexcept { return static_cast<uint32>(mCmds.Size()); }
            void Clear() noexcept { mCmds.Clear(); mBoundsDirty = true; }

        private:
            NkVector<NkPathCmd> mCmds;
            mutable NkAABB2f    mBounds;
            mutable bool        mBoundsDirty = true;
            mutable float32     mLength      = 0.f;
            mutable bool        mLengthDirty = true;

            // ── Tessellation interne ──────────────────────────────────────
            void TesselateFill  (NkVector<NkVec2f>& verts,
                                  NkVector<uint32>& idx,
                                  NkFillRule rule,
                                  float32 tol) const noexcept;
            void TessellateStroke(NkVector<NkVec2f>& verts,
                                   NkVector<uint32>& idx,
                                   const NkStrokeStyle& s) const noexcept;
            void SubdivideBezier (float32 x0,float32 y0,
                                   float32 cx1,float32 cy1,
                                   float32 cx2,float32 cy2,
                                   float32 x1,float32 y1,
                                   NkVector<NkVec2f>& out,
                                   float32 tol) const noexcept;
    };

} // namespace nkentseu
