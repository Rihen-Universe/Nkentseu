#pragma once
// =============================================================================
// Nkentseu/Design/Raster/NkSelectionSystem.h
// =============================================================================
// Système de sélection raster pour les applications de peinture numérique.
//
// OUTILS DE SÉLECTION :
//   Marquise rectangulaire   — sélection en rectangle
//   Marquise elliptique      — sélection en ellipse
//   Lasso libre              — tracé à main levée
//   Lasso polygonal          — clics pour créer un polygone
//   Baguette magique         — sélection par couleur similaire
//   Sélection par couleur    — comme baguette mais sur toute l'image
//
// OPÉRATIONS :
//   Add, Subtract, Intersect — booléennes entre sélections
//   Feather — flou du bord de sélection
//   Grow/Shrink — agrandir/réduire la sélection
//   Invert — inverser la sélection
//   Transform — déplacer, rotation, scale de la sélection
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NkRasterCanvas.h"

namespace nkentseu {
    using namespace math;

    // =========================================================================
    // NkSelectionMask — masque de sélection (alpha 8-bit par pixel)
    // =========================================================================
    /**
     * @class NkSelectionMask
     * @brief Masque de sélection à résolution de pixel.
     *
     * 0   = non sélectionné
     * 255 = entièrement sélectionné
     * [1..254] = sélection partielle (pour feather/anti-aliasing)
     *
     * Stocké de la même façon que NkRasterCanvas (tile-based).
     */
    class NkSelectionMask {
    public:
        NkSelectionMask()  noexcept = default;
        ~NkSelectionMask() noexcept = default;

        bool Create(uint32 width, uint32 height) noexcept;
        void Destroy() noexcept;

        // ── Opérations de sélection ───────────────────────────────────────
        void SelectAll()  noexcept;
        void SelectNone() noexcept;
        void Invert()     noexcept;

        void SelectRect    (const NkIRect& rect, float32 feather = 0.f) noexcept;
        void SelectEllipse (const NkIRect& bounds, float32 feather = 0.f) noexcept;
        void SelectPolygon (NkSpan<const NkVec2f> points, float32 feather = 0.f) noexcept;

        /**
         * @brief Flood-fill depuis un pixel jusqu'aux pixels de couleur similaire.
         * @param canvas     Canvas source pour comparer les couleurs.
         * @param x, y       Point de départ.
         * @param tolerance  Tolérance [0..255].
         * @param contiguous true = pixels connectés uniquement.
         */
        void SelectByColor(const NkRasterCanvas& canvas,
                            uint32 x, uint32 y,
                            uint32 tolerance = 32,
                            bool contiguous = true) noexcept;

        // ── Opérations booléennes ─────────────────────────────────────────
        enum class BoolOp : uint8 { Replace, Add, Subtract, Intersect };

        void Combine(const NkSelectionMask& other, BoolOp op) noexcept;
        void AddSelection     (const NkSelectionMask& other) noexcept;
        void SubtractSelection(const NkSelectionMask& other) noexcept;
        void IntersectSelection(const NkSelectionMask& other) noexcept;

        // ── Modifications ─────────────────────────────────────────────────
        void Feather(float32 radius) noexcept;       ///< Flou gaussien du bord
        void Grow   (uint32 pixels)  noexcept;       ///< Expand la sélection
        void Shrink (uint32 pixels)  noexcept;       ///< Contracte la sélection
        void Smooth (float32 radius = 2.f) noexcept; ///< Lisse les bords
        void Border (uint32 width)   noexcept;       ///< Garde uniquement la bordure

        // ── Accès ─────────────────────────────────────────────────────────
        [[nodiscard]] uint8  GetMask(uint32 x, uint32 y) const noexcept;
        void                 SetMask(uint32 x, uint32 y, uint8 value) noexcept;

        [[nodiscard]] bool   IsEmpty()  const noexcept { return mEmpty; }
        [[nodiscard]] bool   IsValid()  const noexcept { return mValid; }
        [[nodiscard]] uint32 GetWidth() const noexcept { return mWidth; }
        [[nodiscard]] uint32 GetHeight()const noexcept { return mHeight; }

        /**
         * @brief Bounding box de la sélection (pixels non-nuls).
         */
        [[nodiscard]] NkIRect GetBounds() const noexcept;

        /**
         * @brief Convertit en NkVectorPath (pour transformation vectorielle).
         */
        [[nodiscard]] NkVector<NkVec2f> MarchingSquares(float32 threshold = 128.f) const noexcept;

        // ── Rendu overlay ─────────────────────────────────────────────────
        /**
         * @brief Dessine le masque comme overlay "marching ants" sur le canvas.
         */
        void DrawOverlay(renderer::NkRender2D& r2d,
                          float32 animTime,
                          const NkRectF& dst) const noexcept;

    private:
        NkVector<uint8> mMask;
        uint32          mWidth  = 0;
        uint32          mHeight = 0;
        bool            mValid  = false;
        bool            mEmpty  = true;

        void FloodFillRecursive(uint32 x, uint32 y,
                                  const NkRasterCanvas& canvas,
                                  uint8 refR, uint8 refG, uint8 refB,
                                  uint32 tol,
                                  NkVector<bool>& visited) noexcept;
    };

    // =========================================================================
    // NkSelectionTool — outil de sélection actif
    // =========================================================================
    enum class NkSelectionToolType : uint8 {
        RectMarquee,
        EllipseMarquee,
        Lasso,          ///< Main levée
        PolyLasso,      ///< Polygonal
        MagicWand,
        ColorRange,
    };

    struct NkSelectionTool {
        NkSelectionToolType type       = NkSelectionToolType::RectMarquee;
        NkSelectionMask::BoolOp boolOp = NkSelectionMask::BoolOp::Replace;

        float32  feather    = 0.f;
        uint32   tolerance  = 32;
        bool     antiAlias  = true;
        bool     contiguous = true;   ///< MagicWand : pixels connectés uniquement
        bool     allLayers  = false;  ///< Sélection sur toutes les couches

        // ── État interne du tracé ─────────────────────────────────────────
        bool     isDrawing  = false;
        NkVec2f  startPos   = {};
        NkVector<NkVec2f> lassoPoints;

        void PointerDown(NkVec2f pos, NkSelectionMask& mask,
                          const NkRasterCanvas& canvas) noexcept;
        void PointerMove(NkVec2f pos, NkSelectionMask& mask) noexcept;
        void PointerUp  (NkVec2f pos, NkSelectionMask& mask,
                          const NkRasterCanvas& canvas) noexcept;
    };

    // =========================================================================
    // NkRasterFilter — filtres non-destructifs
    // =========================================================================
    enum class NkFilterType : uint8 {
        // Couleur
        BrightnessContrast,
        LevelsRGB,
        CurvesRGB,
        HueSaturation,
        ColorBalance,
        Exposure,
        Vibrance,
        Invert,
        Posterize,
        Threshold,
        Desaturate,
        // Flou
        GaussianBlur,
        BoxBlur,
        MotionBlur,
        RadialBlur,
        // Netteté
        Sharpen,
        UnsharpMask,
        HighPass,
        // Texture
        Noise,
        Emboss,
        EdgeDetect,
        // Déformation
        Distort,
        Warp,
        Liquify,
    };

    struct NkRasterFilter {
        NkFilterType type     = NkFilterType::GaussianBlur;
        bool         enabled  = true;
        float32      intensity = 1.f;   ///< Intensité [0..1]

        // Paramètres spécifiques
        union Params {
            struct { float32 brightness; float32 contrast; } bc;
            struct { float32 hue; float32 saturation; float32 lightness; } hsl;
            struct { float32 radius; } blur;
            struct { float32 amount; float32 radius; float32 threshold; } usm;
            struct { float32 amount; } noise;
            struct { float32 strength; float32 radius; } liquify;
        } params = {};

        // Courbe (pour CurvesRGB — 5 points de contrôle par canal)
        static constexpr uint32 kCurvePoints = 5u;
        NkVec2f curveR[kCurvePoints];
        NkVec2f curveG[kCurvePoints];
        NkVec2f curveB[kCurvePoints];

        /**
         * @brief Applique le filtre sur un canvas (modifie les pixels).
         * @param canvas     Canvas source et destination.
         * @param mask       Optionnel : applique uniquement dans la sélection.
         * @param region     Optionnel : zone à filtrer (tout si vide).
         */
        void Apply(NkRasterCanvas& canvas,
                    const NkSelectionMask* mask = nullptr,
                    const NkIRect& region = {}) const noexcept;

        /**
         * @brief Prévisualisation — applique sur une copie sans modifier l'original.
         */
        void Preview(const NkRasterCanvas& src,
                      NkRasterCanvas& dst,
                      const NkIRect& region = {}) const noexcept;
    };

    // =========================================================================
    // NkRasterIO — import/export raster
    // =========================================================================
    struct NkRasterImportOptions {
        bool    convertToLinear = true;   ///< Convertit sRGB → Linear
        bool    premultiplyAlpha = false;
        uint32  targetWidth    = 0;       ///< 0 = taille originale
        uint32  targetHeight   = 0;
        NkPixelDepth depth     = NkPixelDepth::Depth8;
    };

    struct NkRasterExportOptions {
        enum class Format : uint8 { PNG, JPEG, TIFF, EXR, WebP, BMP };
        Format  format      = Format::PNG;
        uint32  jpegQuality = 90;
        bool    exrHDR      = true;        ///< Float32 pour EXR
        bool    tiffFloat   = false;       ///< Float pour TIFF
        float32 scale       = 1.f;
        bool    flattenLayers = true;
        bool    exportAlpha   = true;
    };

    class NkRasterIO {
    public:
        /**
         * @brief Importe une image vers un NkRasterCanvas.
         * Supporte : PNG, JPEG, TIFF, EXR, BMP, WebP, PSD (simplifié).
         */
        [[nodiscard]] static bool Import(const char* path,
                                          NkRasterCanvas& out,
                                          const NkRasterImportOptions& opts = {}) noexcept;

        /**
         * @brief Exporte un NkRasterCanvas vers un fichier.
         */
        [[nodiscard]] static bool Export(const NkRasterCanvas& canvas,
                                          const char* path,
                                          const NkRasterExportOptions& opts = {}) noexcept;

        /**
         * @brief Charge depuis un buffer mémoire (sans passer par un fichier).
         */
        [[nodiscard]] static bool ImportFromMemory(const uint8* data,
                                                    nk_usize size,
                                                    NkRasterCanvas& out,
                                                    const NkRasterImportOptions& opts = {}) noexcept;

        /**
         * @brief Détecte le format depuis l'extension ou le magic number.
         */
        [[nodiscard]] static NkRasterExportOptions::Format DetectFormat(const char* path) noexcept;

        [[nodiscard]] static const NkString& GetLastError() noexcept;
    };

} // namespace nkentseu
