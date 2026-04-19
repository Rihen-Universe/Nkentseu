#pragma once
// =============================================================================
// Nkentseu/Design/Raster/NkRasterCanvas.h
// =============================================================================
// Canvas raster tile-based pour la peinture numérique.
//
// ARCHITECTURE TILE-BASED :
//   Le canvas est découpé en tiles de 64×64 pixels.
//   • Seuls les tiles modifiés sont renvoyés au GPU (FlushDirtyTiles)
//   • Permet des canvases très larges (16000×16000px) sans saturer la VRAM
//   • Chaque tile est une sous-texture dans un atlas GPU
//   • Zoom avec interpolation GPU (pas de re-tessellation CPU)
//
// PRÉCISION :
//   • Depth8  : RGBA 8-bit (sRGB, 0-255 par canal) — standard
//   • Depth16 : RGBA 16-bit (linéaire) — HDR painting, Krita-style
//   • Depth32 : RGBA float32 — VFX, retouche pro (très lourd)
//
// USAGE :
//   NkRasterCanvas canvas;
//   canvas.Create(4096, 4096, NkPixelDepth::Depth8);
//
//   // Via NkBrushEngine (peinture)
//   NkBrushEngine brush;
//   brush.SetCanvas(&canvas);
//   brush.SetPreset(NkBrushPreset::Pencil());
//   brush.SetColor({50, 30, 10, 255});
//   brush.PointerDown({512, 384}, 1.f);
//   brush.PointerMove({600, 400}, 0.8f);
//   brush.PointerUp();
//
//   // Upload GPU et rendu
//   canvas.FlushDirtyTiles(cmd);
//   canvas.DrawToRender2D(r2d, {0, 0, 800, 600});
// =============================================================================

#include "NKECS/NkECSDefines.h"
#include "NKMath/NkMath.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKRenderer/src/Tools/Render2D/NkRender2D.h"
#include "NKRHI/Commands/NkICommandBuffer.h"

namespace nkentseu {
    using namespace math;

    // =========================================================================
    // NkColorRGBA — couleur raster 8-bit par canal
    // =========================================================================
    struct NkColorRGBA {
        uint8 r = 0, g = 0, b = 0, a = 255;

        NkColorRGBA() noexcept = default;
        constexpr NkColorRGBA(uint8 r,uint8 g,uint8 b,uint8 a=255) noexcept
            : r(r),g(g),b(b),a(a) {}

        [[nodiscard]] static NkColorRGBA FromFloat(float32 r,float32 g,float32 b,float32 a=1.f) noexcept {
            return { uint8(r*255), uint8(g*255), uint8(b*255), uint8(a*255) };
        }
        [[nodiscard]] static NkColorRGBA Black()  noexcept { return {0,0,0,255}; }
        [[nodiscard]] static NkColorRGBA White()  noexcept { return {255,255,255,255}; }
        [[nodiscard]] static NkColorRGBA Transparent() noexcept { return {0,0,0,0}; }
        [[nodiscard]] NkVec4f ToFloat() const noexcept {
            return {r/255.f,g/255.f,b/255.f,a/255.f};
        }
    };

    // =========================================================================
    // NkPixelDepth — précision du canal
    // =========================================================================
    enum class NkPixelDepth : uint8 {
        Depth8  = 1,  ///< RGBA8  — 4 bytes/pixel (standard)
        Depth16 = 2,  ///< RGBA16 — 8 bytes/pixel (HDR peinture)
        Depth32 = 4,  ///< RGBA32F — 16 bytes/pixel (VFX, retouche pro)
    };

    // =========================================================================
    // NkCanvasTile — unité de stockage CPU du canvas
    // =========================================================================
    struct NkCanvasTile {
        static constexpr uint32 kSize = 64u;  ///< 64×64 pixels par tile

        uint8*    pixels   = nullptr;   ///< Buffer CPU (4 × kSize × kSize bytes)
        nk_uint64 gpuTex   = 0;         ///< Sous-texture dans atlas GPU
        bool      dirty    = false;     ///< Doit être re-uploadé au GPU
        bool      blank    = true;      ///< Tile vide (économise mémoire)

        NkCanvasTile()  noexcept = default;
        ~NkCanvasTile() noexcept { delete[] pixels; }
        NkCanvasTile(const NkCanvasTile&) = delete;
        NkCanvasTile& operator=(const NkCanvasTile&) = delete;

        void Allocate(NkPixelDepth depth) noexcept;
        void Clear(NkColorRGBA color = {0,0,0,0}) noexcept;
    };

    // =========================================================================
    // NkBrushDab — un tampon de pinceau (dab = stamp individuel)
    // =========================================================================
    struct NkBrushDab {
        float32 sizePx     = 20.f;
        float32 hardness   = 0.8f;   ///< [0=flou, 1=dur]
        float32 opacity    = 1.f;
        float32 flow       = 1.f;
        float32 angleDeg   = 0.f;
        float32 roundness  = 1.f;    ///< [0=plat, 1=rond]
        float32 scatter    = 0.f;
        NkColorRGBA color  = {0,0,0,255};
        nk_uint64 stampTex = 0;       ///< Texture de forme bitmap (0 = cercle)

        // Blend mode pour ce dab
        enum class BlendMode : uint8 {
            Normal, Multiply, Screen, Overlay, SoftLight,
            HardLight, Darken, Lighten, Difference, Hue,
            Saturation, Color, Luminosity, Erase
        };
        BlendMode blendMode = BlendMode::Normal;
    };

    // =========================================================================
    // NkRasterCanvas — canvas raster tile-based
    // =========================================================================
    class NkRasterCanvas {
        public:
            NkRasterCanvas()  noexcept = default;
            ~NkRasterCanvas() noexcept { Destroy(); }

            NkRasterCanvas(const NkRasterCanvas&) = delete;
            NkRasterCanvas& operator=(const NkRasterCanvas&) = delete;

            // ── Création / Destruction ────────────────────────────────────
            /**
             * @brief Crée le canvas avec les dimensions données.
             * @param width  Largeur en pixels.
             * @param height Hauteur en pixels.
             * @param depth  Précision par canal.
             * @return true si succès.
             */
            bool Create(uint32 width, uint32 height,
                        NkPixelDepth depth = NkPixelDepth::Depth8) noexcept;

            void Destroy() noexcept;

            [[nodiscard]] bool IsValid() const noexcept { return mValid; }

            // ── Peinture (CPU-side) ───────────────────────────────────────

            /**
             * @brief Applique un dab de pinceau sur le canvas.
             * @param dab    Paramètres du dab (taille, dureté, couleur, blend mode).
             * @param center Position centrale du dab en pixels.
             */
            void PaintDab(const NkBrushDab& dab, NkVec2f center) noexcept;

            /**
             * @brief Remplit une région avec une couleur unie.
             */
            void Fill(NkColorRGBA color,
                      const NkIRect& region = {}) noexcept;

            /**
             * @brief Efface une région (met à transparent).
             */
            void Erase(const NkIRect& region = {}) noexcept;

            /**
             * @brief Flood fill depuis un point (baguette magique).
             * @param x, y     Point de départ.
             * @param fillColor Couleur de remplissage.
             * @param tolerance Tolérance de couleur [0..255].
             */
            void FloodFill(uint32 x, uint32 y,
                           NkColorRGBA fillColor,
                           uint32 tolerance = 32) noexcept;

            /**
             * @brief Copie une région depuis un autre canvas.
             */
            void Blit(const NkRasterCanvas& src,
                      NkIRect srcRect, NkIVec2 dstPos,
                      float32 opacity = 1.f,
                      NkBrushDab::BlendMode mode = NkBrushDab::BlendMode::Normal) noexcept;

            // ── Transformations ───────────────────────────────────────────
            void FlipH()   noexcept;
            void FlipV()   noexcept;
            void Rotate90 (bool clockwise = true) noexcept;
            void Scale    (uint32 newWidth, uint32 newHeight) noexcept;
            void Crop     (const NkIRect& region) noexcept;

            // ── Accès aux pixels ──────────────────────────────────────────
            [[nodiscard]] NkColorRGBA GetPixel(uint32 x, uint32 y) const noexcept;
            void                      SetPixel(uint32 x, uint32 y, NkColorRGBA c) noexcept;

            /**
             * @brief Accès direct aux pixels d'un tile (pour peinture batch).
             */
            [[nodiscard]] NkSpan<uint8> GetTilePixels(uint32 tileX,
                                                        uint32 tileY) noexcept;

            /**
             * @brief Marque un tile comme dirty (pour flush GPU).
             */
            void MarkTileDirty(uint32 tileX, uint32 tileY) noexcept;

            void MarkAllDirty() noexcept;

            // ── GPU Sync ──────────────────────────────────────────────────
            /**
             * @brief Envoie les tiles modifiés vers le GPU.
             * À appeler après toute session de peinture, avant le rendu.
             */
            void FlushDirtyTiles(NkICommandBuffer* cmd) noexcept;

            // ── Rendu ─────────────────────────────────────────────────────
            /**
             * @brief Dessine le canvas dans un NkRender2D.
             * @param r2d  Le renderer 2D.
             * @param dst  Rectangle destination en pixels écran.
             * @param opacity Opacité globale du canvas [0..1].
             */
            void DrawToRender2D(renderer::NkRender2D& r2d,
                                 const NkRectF& dst,
                                 float32 opacity = 1.f) const noexcept;

            // ── Propriétés ────────────────────────────────────────────────
            [[nodiscard]] uint32       GetWidth()      const noexcept { return mWidth; }
            [[nodiscard]] uint32       GetHeight()     const noexcept { return mHeight; }
            [[nodiscard]] NkPixelDepth GetDepth()      const noexcept { return mDepth; }
            [[nodiscard]] uint32       BytesPerPixel() const noexcept {
                return static_cast<uint32>(mDepth) * 4u;
            }
            [[nodiscard]] uint32       TileCountX()    const noexcept { return mTilesX; }
            [[nodiscard]] uint32       TileCountY()    const noexcept { return mTilesY; }
            [[nodiscard]] uint32       DirtyTileCount()const noexcept { return mDirtyCount; }

            // ── Import/Export ─────────────────────────────────────────────
            bool SavePNG  (const char* path) const noexcept;
            bool SaveTIFF (const char* path) const noexcept;  ///< 16-bit si Depth16
            bool SaveEXR  (const char* path) const noexcept;  ///< Depth32 uniquement
            bool LoadPNG  (const char* path) noexcept;
            bool LoadTIFF (const char* path) noexcept;

        private:
            // ── Helpers internes ──────────────────────────────────────────
            [[nodiscard]] uint32 TileIndexOf(uint32 tileX, uint32 tileY) const noexcept {
                return tileY * mTilesX + tileX;
            }
            [[nodiscard]] NkCanvasTile& GetOrCreateTile(uint32 tx, uint32 ty) noexcept;

            void BlendPixel(uint8* dst, const uint8* src, float32 alpha,
                             NkBrushDab::BlendMode mode) const noexcept;

            // ── Dab painting par tile ─────────────────────────────────────
            void PaintDabOnTile(NkCanvasTile& tile,
                                 uint32 tileX, uint32 tileY,
                                 const NkBrushDab& dab,
                                 NkVec2f dabCenter) noexcept;

            // ── État ──────────────────────────────────────────────────────
            NkVector<NkCanvasTile> mTiles;
            uint32       mWidth     = 0;
            uint32       mHeight    = 0;
            uint32       mTilesX    = 0;
            uint32       mTilesY    = 0;
            NkPixelDepth mDepth     = NkPixelDepth::Depth8;
            uint32       mDirtyCount = 0;
            bool         mValid     = false;

            // Atlas GPU pour les tiles
            nk_uint64    mAtlasHandle = 0;
            uint32       mAtlasSize   = 4096;  ///< Taille de l'atlas (4096 = 64×64 tiles)
    };

    // =========================================================================
    // NkBrushEngine — moteur de pinceau complet
    // =========================================================================
    struct NkBrushPreset {
        // ── Forme ─────────────────────────────────────────────────────────
        float32  size          = 20.f;
        float32  hardness      = 0.8f;
        float32  angle         = 0.f;
        float32  roundness     = 1.f;
        float32  angleJitter   = 0.f;
        float32  sizeJitter    = 0.f;
        nk_uint64 stampTex     = 0;          ///< Texture bitmap de forme (0=cercle)

        // ── Espacement ────────────────────────────────────────────────────
        float32  spacing       = 0.1f;       ///< [fraction du diamètre]
        float32  scatter       = 0.f;        ///< Dispersion aléatoire

        // ── Opacité & Flow ────────────────────────────────────────────────
        float32  opacity       = 1.f;
        float32  flow          = 1.f;
        bool     pressureOpacity = true;
        bool     pressureSize    = false;
        bool     velocityOpacity = false;
        bool     tiltAngle       = false;    ///< Inclinaison stylet → angle dab

        // ── Grain (texture de support) ────────────────────────────────────
        nk_uint64 grainTex     = 0;
        float32  grainStrength = 0.f;
        float32  grainScale    = 1.f;

        // ── Blend mode ────────────────────────────────────────────────────
        NkBrushDab::BlendMode blendMode = NkBrushDab::BlendMode::Normal;

        // ── Presets prédéfinis ────────────────────────────────────────────
        static NkBrushPreset HardRound()  noexcept;
        static NkBrushPreset SoftRound()  noexcept;
        static NkBrushPreset Pencil()     noexcept;
        static NkBrushPreset Ink()        noexcept;
        static NkBrushPreset Watercolor() noexcept;
        static NkBrushPreset Airbrush()   noexcept;
        static NkBrushPreset Eraser()     noexcept;
    };

    class NkBrushEngine {
        public:
            NkBrushEngine()  noexcept = default;
            ~NkBrushEngine() noexcept = default;

            // ── Configuration ─────────────────────────────────────────────
            void SetPreset (const NkBrushPreset& p) noexcept { mPreset = p; }
            void SetColor  (NkColorRGBA c)          noexcept { mColor  = c; }
            void SetCanvas (NkRasterCanvas* canvas) noexcept { mCanvas = canvas; }

            // ── Traits ────────────────────────────────────────────────────
            /**
             * @brief Début d'un trait.
             * @param pos      Position en pixels canvas.
             * @param pressure Pression stylet [0..1].
             * @param tiltDeg  Inclinaison stylet [0..90°].
             * @param rotation Rotation stylet [0..360°].
             */
            void PointerDown(NkVec2f pos, float32 pressure = 1.f,
                              float32 tiltDeg = 0.f, float32 rotation = 0.f) noexcept;

            void PointerMove(NkVec2f pos, float32 pressure = 1.f,
                              float32 tiltDeg = 0.f, float32 rotation = 0.f) noexcept;

            void PointerUp() noexcept;

            // ── Stabilisation ─────────────────────────────────────────────
            float32 stabilizerStrength = 0.f;  ///< 0=off, 1=fort (Lazy Nezumi)
            uint32  stabilizerWindow   = 10;   ///< Nombre de points dans la fenêtre

            [[nodiscard]] bool IsDrawing() const noexcept { return mDrawing; }

        private:
            void PaintDabAt(NkVec2f center, float32 pressure, float32 tiltDeg) noexcept;
            void BuildDab  (NkBrushDab& out, float32 pressure, float32 tiltDeg) const noexcept;

            NkBrushPreset   mPreset;
            NkColorRGBA     mColor    = {0, 0, 0, 255};
            NkRasterCanvas* mCanvas   = nullptr;

            NkVec2f mLastPos          = {};
            float32 mDistAccum        = 0.f;
            float32 mLastPressure     = 1.f;
            bool    mDrawing          = false;

            // Stabiliseur (buffer de positions)
            NkVector<NkVec2f> mStabBuffer;
    };

} // namespace nkentseu
