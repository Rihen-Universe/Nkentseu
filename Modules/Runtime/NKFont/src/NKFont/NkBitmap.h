#pragma once
/**
 * @File    NkBitmap.h
 * @Brief   Buffer de pixels grayscale 8 bits — sortie du rasterizer.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Design
 *  NkBitmap est le résultat final du rasterizer : un rectangle de pixels
 *  uint8 représentant la couverture (0 = transparent, 255 = plein).
 *
 *  Deux modes :
 *    - Owning  : alloué via NkMemArena (pas de free individuel).
 *    - View    : pointe sur un buffer externe (atlas, stack).
 *
 *  Stride : nombre d'octets par ligne. Peut être > width pour l'alignement
 *  ou pour les bitmaps sous-rectangles dans un atlas.
 *
 *  Origine : coin supérieur gauche, lignes vers le bas (convention raster).
 *  Le système de coordonnées TrueType (Y vers le haut) est converti lors
 *  du rasterizer — NkBitmap est toujours en coordonnées écran.
 */

#include "NKFont/NkFontExport.h"
#include "NKFont/NkMemArena.h"
#include "NKCore/NkTypes.h"
#include <cstring>  // memset, memcpy

namespace nkentseu {

    // =========================================================================
    //  NkBitmapFormat
    // =========================================================================

    enum class NkBitmapFormat : uint8 {
        Gray8   = 0,  ///< 1 octet par pixel, niveaux de gris
        Gray1   = 1,  ///< 1 bit par pixel (bitmap monochrome, BDF/PCF)
        RGBA32  = 2,  ///< 4 octets par pixel (LCD subpixel, future extension)
    };

    // =========================================================================
    //  NkBitmap
    // =========================================================================

    struct NKENTSEU_FONT_API NkBitmap {

        uint8*        pixels = nullptr;  ///< Buffer de pixels
        int32         width  = 0;        ///< Largeur en pixels
        int32         height = 0;        ///< Hauteur en pixels
        int32         stride = 0;        ///< Octets par ligne (>= width * bpp)
        NkBitmapFormat format = NkBitmapFormat::Gray8;

        // ── Fabriques ─────────────────────────────────────────────────────────

        /**
         * @Brief Alloue un bitmap grayscale dans l'arena fourni.
         * @param arena  Arena d'où provient la mémoire (pas de free individuel).
         * @param w, h   Dimensions en pixels.
         * @param align  Alignement du stride (défaut : 4 octets).
         * @return Bitmap valide, ou bitmap vide si l'arena est épuisé.
         */
        NKFONT_NODISCARD static NkBitmap Alloc(
            NkMemArena& arena,
            int32 w, int32 h,
            int32 strideAlign = 4
        ) noexcept {
            NkBitmap bm;
            bm.width  = w;
            bm.height = h;
            bm.format = NkBitmapFormat::Gray8;
            // Stride aligné sur strideAlign octets
            bm.stride = ((w + strideAlign - 1) / strideAlign) * strideAlign;
            const usize bytes = static_cast<usize>(bm.stride) * static_cast<usize>(h);
            bm.pixels = arena.Alloc<uint8>(bytes);
            // AllocRaw zero-initialise — pas de memset redondant
            return bm;
        }

        /**
         * @Brief Crée une vue sur un buffer externe (mode lecture/écriture, pas de ownership).
         */
        NKFONT_NODISCARD static NkBitmap View(
            uint8* buf, int32 w, int32 h, int32 stride,
            NkBitmapFormat fmt = NkBitmapFormat::Gray8
        ) noexcept {
            NkBitmap bm;
            bm.pixels = buf;
            bm.width  = w;
            bm.height = h;
            bm.stride = stride;
            bm.format = fmt;
            return bm;
        }

        // ── Accesseurs ────────────────────────────────────────────────────────

        NKFONT_NODISCARD bool  IsValid() const noexcept { return pixels && width > 0 && height > 0; }
        NKFONT_NODISCARD usize BytesPerPixel() const noexcept {
            return (format == NkBitmapFormat::RGBA32) ? 4u
                 : (format == NkBitmapFormat::Gray1)  ? 0u  // special : bits
                 : 1u;
        }
        NKFONT_NODISCARD usize TotalBytes() const noexcept {
            return static_cast<usize>(stride) * static_cast<usize>(height);
        }

        // ── Accès pixel ───────────────────────────────────────────────────────

        /// Lit un pixel grayscale en (x, y). Aucune vérification de bounds en release.
        NKFONT_NODISCARD NKFONT_INLINE uint8 GetPixel(int32 x, int32 y) const noexcept {
            return pixels[y * stride + x];
        }

        /// Écrit un pixel grayscale en (x, y).
        NKFONT_INLINE void SetPixel(int32 x, int32 y, uint8 v) noexcept {
            pixels[y * stride + x] = v;
        }

        /// Accumule (additionne saturée) un pixel — utilisé par le rasterizer AA.
        NKFONT_INLINE void AddPixel(int32 x, int32 y, uint8 v) noexcept {
            uint8& p = pixels[y * stride + x];
            const uint32 sum = static_cast<uint32>(p) + v;
            p = static_cast<uint8>(sum > 255u ? 255u : sum);
        }

        /// Pointeur sur le début d'une ligne.
        NKFONT_NODISCARD NKFONT_INLINE uint8* RowPtr(int32 y) noexcept {
            return pixels + y * stride;
        }
        NKFONT_NODISCARD NKFONT_INLINE const uint8* RowPtr(int32 y) const noexcept {
            return pixels + y * stride;
        }

        // ── Opérations ────────────────────────────────────────────────────────

        /// Met tous les pixels à zéro.
        void Clear() noexcept {
            if (pixels) ::memset(pixels, 0, TotalBytes());
        }

        /// Remplit toute la bitmap avec la valeur v.
        void Fill(uint8 v) noexcept {
            if (pixels) ::memset(pixels, v, TotalBytes());
        }

        /**
         * @Brief Copie src dans this à la position (dstX, dstY).
         *        Clippe automatiquement si src déborde de this.
         */
        void Blit(const NkBitmap& src, int32 dstX, int32 dstY) noexcept {
            if (!IsValid() || !src.IsValid()) return;

            int32 sx = 0, sy = 0;
            int32 dx = dstX, dy = dstY;
            int32 w  = src.width;
            int32 h  = src.height;

            // Clip left/top
            if (dx < 0) { sx -= dx; w += dx; dx = 0; }
            if (dy < 0) { sy -= dy; h += dy; dy = 0; }
            // Clip right/bottom
            if (dx + w > width)  w = width  - dx;
            if (dy + h > height) h = height - dy;
            if (w <= 0 || h <= 0) return;

            for (int32 row = 0; row < h; ++row)
                ::memcpy(RowPtr(dy + row) + dx,
                         src.RowPtr(sy + row) + sx,
                         static_cast<usize>(w));
        }

        /**
         * @Brief Extrait un sous-rectangle en tant que NkBitmap view (pas de copie).
         *        Utile pour accéder à une région d'un atlas.
         */
        NKFONT_NODISCARD NkBitmap SubView(int32 x, int32 y, int32 w, int32 h) const noexcept {
            NkBitmap sub;
            sub.pixels = pixels + y * stride + x;
            sub.width  = w;
            sub.height = h;
            sub.stride = stride;   // même stride que le parent
            sub.format = format;
            return sub;
        }

        // ── Métriques d'encre ─────────────────────────────────────────────────

        /**
         * @Brief Calcule la bounding box serrée des pixels non-nuls.
         *        Retourne false si le bitmap est entièrement vide.
         */
        bool InkBoundingBox(int32& outX, int32& outY,
                            int32& outW, int32& outH) const noexcept {
            int32 minX = width, minY = height, maxX = -1, maxY = -1;
            for (int32 y = 0; y < height; ++y) {
                const uint8* row = RowPtr(y);
                for (int32 x = 0; x < width; ++x) {
                    if (row[x]) {
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
            }
            if (maxX < 0) return false;
            outX = minX; outY = minY;
            outW = maxX - minX + 1;
            outH = maxY - minY + 1;
            return true;
        }
    };

} // namespace nkentseu
