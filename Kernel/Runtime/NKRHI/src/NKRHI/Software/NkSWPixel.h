#pragma once
// =============================================================================
// NkSWPixel.h — Layout de pixel natif plateforme + helpers SIMD
//
// PROBLÈME RÉSOLU : La conversion RGBA→BGRA était faite en post-traitement
// sur tout le framebuffer (1280×720 × 4 bytes = 3.6 MB par frame).
// Solution : écrire directement dans l'ordre attendu par la plateforme
// au moment du SetPixel, donc zéro copie supplémentaire.
//
// Sur Windows (GDI DIBSection) :
//   mémoire → [B][G][R][A]   (BGRA little-endian)
//   NkVertex2D.r/g/b/a → on stocke r→[2], g→[1], b→[0], a→[3]
//
// Sur toutes les autres plateformes :
//   mémoire → [R][G][B][A]   (RGBA)
//   NkVertex2D.r/g/b/a → r→[0], g→[1], b→[2], a→[3]
//
// SIMD : SSE2 (disponible sur tout x86_64), NEON sur ARM/Android.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKPlatform/NkPlatformInline.h"
#include "NKPlatform/NkPlatformDetect.h"

// ── Détection SIMD ────────────────────────────────────────────────────────────
#if !defined(NK_SW_NO_SIMD)
#  if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || _M_IX86_FP >= 2))
#    include <emmintrin.h>   // SSE2
#    define NK_SW_SIMD_SSE2 1
#  endif
#  if defined(__SSSE3__)
#    include <tmmintrin.h>   // SSSE3 _mm_shuffle_epi8
#    define NK_SW_SIMD_SSSE3 1
#  endif
#  if defined(__ARM_NEON) || defined(__ARM_NEON__)
#    include <arm_neon.h>
#    define NK_SW_SIMD_NEON 1
#  endif
#endif

// ── Ordre de stockage natif ───────────────────────────────────────────────────
// Sur Windows GDI, le DIBSection top-down attend BGRA.
// On stocke BGRA dans le framebuffer pour éviter toute conversion à la présentation.
// Sur les autres plateformes on stocke RGBA.

#if defined(NKENTSEU_PLATFORM_WINDOWS)
#  define NK_SW_PIXEL_BGRA 1  // ordre mémoire: [B][G][R][A]
#else
#  define NK_SW_PIXEL_BGRA 0  // ordre mémoire: [R][G][B][A]
#endif

namespace nkentseu {
    namespace sw_detail {

        // ── Stockage d'un pixel (inline, zéro overhead) ───────────────────────────────
        // Convertit r,g,b,a → ordre natif plateforme et écrit dans p[0..3]
        NK_FORCE_INLINE void StorePixel(uint8* __restrict p, uint8 r, uint8 g, uint8 b, uint8 a = 255u) noexcept {
        #if NK_SW_PIXEL_BGRA
            p[0] = b;
            p[1] = g;
            p[2] = r;
            p[3] = a;
        #else
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = a;
        #endif
        }

        // Lire un pixel et retourner (r,g,b,a) quel que soit l'ordre
        NK_FORCE_INLINE void LoadPixel(const uint8* __restrict p, uint8& r, uint8& g, uint8& b, uint8& a) noexcept {
        #if NK_SW_PIXEL_BGRA
            b = p[0]; g = p[1]; r = p[2]; a = p[3];
        #else
            r = p[0]; g = p[1]; b = p[2]; a = p[3];
        #endif
        }

        // Stocker un uint32 packed RGBA dans l'ordre natif
        NK_FORCE_INLINE uint32 PackNative(uint8 r, uint8 g, uint8 b, uint8 a = 255u) noexcept {
        #if NK_SW_PIXEL_BGRA
            return (uint32)b | ((uint32)g << 8) | ((uint32)r << 16) | ((uint32)a << 24);
        #else
            return (uint32)r | ((uint32)g << 8) | ((uint32)b << 16) | ((uint32)a << 24);
        #endif
        }

        // Alpha-blend src sur dst dans l'ordre natif (résultat écrit dans p)
        NK_FORCE_INLINE void BlendPixel(uint8* __restrict p,
                                        uint8 r, uint8 g, uint8 b, uint8 a) noexcept {
            if (a == 0u) return;
            if (a == 255u) { StorePixel(p, r, g, b, 255u); return; }

            const uint32 sa  = a;
            const uint32 inv = 255u - sa;

        #if NK_SW_PIXEL_BGRA
            // p[0]=B, p[1]=G, p[2]=R, p[3]=A
            p[0] = (uint8)((b * sa + p[0] * inv + 127u) / 255u);
            p[1] = (uint8)((g * sa + p[1] * inv + 127u) / 255u);
            p[2] = (uint8)((r * sa + p[2] * inv + 127u) / 255u);
            p[3] = 255u;
        #else
            p[0] = (uint8)((r * sa + p[0] * inv + 127u) / 255u);
            p[1] = (uint8)((g * sa + p[1] * inv + 127u) / 255u);
            p[2] = (uint8)((b * sa + p[2] * inv + 127u) / 255u);
            p[3] = 255u;
        #endif
        }

        // =============================================================================
        // SIMD : remplir un span horizontal avec une couleur unie opaque
        // Gère l'alignement et les pixels résiduels
        // =============================================================================
        NK_FORCE_INLINE void FillSpanOpaque(uint8* __restrict row,
                                            int32 xStart, int32 xEnd,
                                            uint8 r, uint8 g, uint8 b) noexcept {
            if (xStart >= xEnd) return;
            const uint32 color32 = PackNative(r, g, b, 255u);
            uint32* __restrict p = reinterpret_cast<uint32*>(row) + xStart;
            int32 count = xEnd - xStart;

        #if NK_SW_SIMD_SSE2
            // Broadcast color32 dans un registre 128 bits (4 pixels × 32 bits)
            const __m128i vColor = _mm_set1_epi32(static_cast<int32>(color32));

            // Aligner sur 16 bytes (4 pixels uint32)
            while (count >= 4 && ((uintptr_t)p & 15u) != 0u) {
                *p++ = color32;
                --count;
            }
            // Boucle principale SIMD : 4 pixels par itération
            while (count >= 4) {
                _mm_storeu_si128(reinterpret_cast<__m128i*>(p), vColor);
                p += 4; count -= 4;
            }
            // Résidus scalaires
            while (count-- > 0) *p++ = color32;

        #elif NK_SW_SIMD_NEON
            const uint32x4_t vColor = vdupq_n_u32(color32);
            while (count >= 4) {
                vst1q_u32(p, vColor);
                p += 4; count -= 4;
            }
            while (count-- > 0) *p++ = color32;

        #else
            // Scalaire pure
            while (count-- > 0) *p++ = color32;
        #endif
        }

        // =============================================================================
        // SIMD : blend additif d'un span (Add blend)
        // dst = clamp(dst + src*alpha/255)
        // =============================================================================
        NK_FORCE_INLINE void BlendSpanAdd(uint8* __restrict row,
                                        int32 xStart, int32 xEnd,
                                        uint8 r, uint8 g, uint8 b, uint8 a) noexcept {
            if (xStart >= xEnd || a == 0u) return;
            uint8* p = row + xStart * 4;
            int32 count = xEnd - xStart;

            const uint32 sr = (uint32)r * a / 255u;
            const uint32 sg = (uint32)g * a / 255u;
            const uint32 sb = (uint32)b * a / 255u;

        #if NK_SW_SIMD_SSE2
            // Traitement scalaire pour le blend additif (rare path en 2D)
            // SSE2 ne facilite pas beaucoup ici sans SSSE3 pour le shuffle
            // → scalaire optimisé avec variables locales
            while (count-- > 0) {
        #if NK_SW_PIXEL_BGRA
                p[0] = (uint8)((uint32)p[0] + sb > 255u ? 255u : (uint32)p[0] + sb);
                p[1] = (uint8)((uint32)p[1] + sg > 255u ? 255u : (uint32)p[1] + sg);
                p[2] = (uint8)((uint32)p[2] + sr > 255u ? 255u : (uint32)p[2] + sr);
                p[3] = 255u;
        #else
                p[0] = (uint8)((uint32)p[0] + sr > 255u ? 255u : (uint32)p[0] + sr);
                p[1] = (uint8)((uint32)p[1] + sg > 255u ? 255u : (uint32)p[1] + sg);
                p[2] = (uint8)((uint32)p[2] + sb > 255u ? 255u : (uint32)p[2] + sb);
                p[3] = 255u;
        #endif
                p += 4;
            }
        #else
            while (count-- > 0) {
        #if NK_SW_PIXEL_BGRA
                p[0] = (uint8)((uint32)p[0] + sb > 255u ? 255u : (uint32)p[0] + sb);
                p[1] = (uint8)((uint32)p[1] + sg > 255u ? 255u : (uint32)p[1] + sg);
                p[2] = (uint8)((uint32)p[2] + sr > 255u ? 255u : (uint32)p[2] + sr);
        #else
                p[0] = (uint8)((uint32)p[0] + sr > 255u ? 255u : (uint32)p[0] + sr);
                p[1] = (uint8)((uint32)p[1] + sg > 255u ? 255u : (uint32)p[1] + sg);
                p[2] = (uint8)((uint32)p[2] + sb > 255u ? 255u : (uint32)p[2] + sb);
        #endif
                p[3] = 255u;
                p += 4;
            }
        #endif
        }

        // =============================================================================
        // SIMD : alpha blend standard d'un span
        // dst = src*a + dst*(255-a) / 255
        // =============================================================================
        NK_FORCE_INLINE void BlendSpanAlpha(uint8* __restrict row,
                                            int32 xStart, int32 xEnd,
                                            uint8 r, uint8 g, uint8 b, uint8 a) noexcept {
            if (xStart >= xEnd || a == 0u) return;
            if (a == 255u) { FillSpanOpaque(row, xStart, xEnd, r, g, b); return; }

            uint8* p = row + xStart * 4;
            int32 count = xEnd - xStart;
            const uint32 sa  = a;
            const uint32 inv = 255u - sa;

        #if NK_SW_SIMD_SSE2
            // SSE2 : traiter 2 pixels par itération (les calculs 16 bits permettent
            // de gérer sans débordement les multiplications uint8×uint8)
            const __m128i vAlpha  = _mm_set1_epi16(static_cast<int16_t>(sa));
            const __m128i vInvAlpha = _mm_set1_epi16(static_cast<int16_t>(inv));
            const __m128i vZero = _mm_setzero_si128();

            while (count >= 2) {
                // Charger 2 pixels dst (8 bytes)
                __m128i dst8 = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(p));
                // Expand uint8 → uint16
                __m128i dst16 = _mm_unpacklo_epi8(dst8, vZero);  // 8×uint16

                // Src color pour 2 pixels (répété)
        #if NK_SW_PIXEL_BGRA
                __m128i src16 = _mm_set_epi16(0, (int16_t)r, (int16_t)g, (int16_t)b,
                                            0, (int16_t)r, (int16_t)g, (int16_t)b);
        #else
                __m128i src16 = _mm_set_epi16(0, (int16_t)b, (int16_t)g, (int16_t)r,
                                            0, (int16_t)b, (int16_t)g, (int16_t)r);
        #endif
                // result = (src * alpha + dst * inv_alpha + 127) / 255
                // Approximation rapide : >> 8 au lieu de / 255 (erreur max = 1/256)
                __m128i blended = _mm_srli_epi16(
                    _mm_add_epi16(
                        _mm_add_epi16(_mm_mullo_epi16(src16, vAlpha),
                                    _mm_mullo_epi16(dst16, vInvAlpha)),
                        _mm_set1_epi16(127)),
                    8);

                // Forcer alpha=255 dans les canaux A (position 3 et 7)
                const __m128i vAlpha255 = _mm_set_epi16(255, 0, 0, 0, 255, 0, 0, 0);
                blended = _mm_or_si128(
                    _mm_andnot_si128(_mm_set_epi16(-1, 0, 0, 0, -1, 0, 0, 0), blended),
                    vAlpha255);

                // Pack uint16 → uint8 et stocker
                __m128i result8 = _mm_packus_epi16(blended, vZero);
                _mm_storel_epi64(reinterpret_cast<__m128i*>(p), result8);
                p += 8; count -= 2;
            }
        #endif

            // Résidus scalaires
            while (count-- > 0) {
        #if NK_SW_PIXEL_BGRA
                p[0] = (uint8)((b * sa + p[0] * inv + 127u) / 255u);
                p[1] = (uint8)((g * sa + p[1] * inv + 127u) / 255u);
                p[2] = (uint8)((r * sa + p[2] * inv + 127u) / 255u);
        #else
                p[0] = (uint8)((r * sa + p[0] * inv + 127u) / 255u);
                p[1] = (uint8)((g * sa + p[1] * inv + 127u) / 255u);
                p[2] = (uint8)((b * sa + p[2] * inv + 127u) / 255u);
        #endif
                p[3] = 255u;
                p += 4;
            }
        }

        // =============================================================================
        // SIMD : Clear d'une ligne (memset accéléré)
        // =============================================================================
        NK_FORCE_INLINE void ClearRow(uint8* __restrict row, uint32 width,
                                    uint8 r, uint8 g, uint8 b, uint8 a = 255u) noexcept {
            FillSpanOpaque(row, 0, (int32)width, r, g, b);
            // Alpha channel : géré séparément si ≠ 255
            if (a != 255u) {
                uint32* p = reinterpret_cast<uint32*>(row);
                uint32 color32 = PackNative(r, g, b, a);
                for (uint32 i = 0; i < width; ++i) p[i] = color32;
            }
        }

    } // namespace sw_detail
} // namespace nkentseu