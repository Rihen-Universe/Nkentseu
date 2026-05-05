// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontRasterizer.cpp
// DESCRIPTION: Rastériseur scanline pour contours de glyphes.
//              Implémente un algorithme de remplissage par scanline avec deux
//              buffers flottants (contribution locale et déversement "fill").
//              Aucune dépendance externe — utilise les types nkft_.
//
// ─────────────────────────────────────────────────────────────────────────────
// ALGORITHME DE RASTÉRISATION PAR SCANLINE — VERSION À DEUX BUFFERS
// ─────────────────────────────────────────────────────────────────────────────
//
// Le rastériseur utilise DEUX buffers flottants par scanline :
//
//   scanline[W]   : contribution "locale" de l'arête dans chaque pixel
//                   (aire du trapèze dans ce pixel, signée)
//
//   scanline2[W+1]: contribution "fill" = déversement vers la droite
//                   (hauteur de l'arête au-dessus de ce pixel)
//
// Intégration finale :
//   float sum = 0;
//   for (i = 0; i < W; ++i) {
//       sum += scanline2[i];          // accumule le déversement "fill"
//       k = scanline[i] + sum;        // local + tout ce qui est à gauche
//       k = fabs(k) * 255 + 0.5f;
//       m = (int) k;
//       if (m > 255) m = 255;
//       result[i] = (unsigned char) m;
//   }
//
// L'arête active stocke :
//   fx  = position X au DÉBUT (top) de la scanline courante (à y+0.0)
//   fdx = delta X par scanline complète
//   fdy = 1/fdx si fdx != 0, sinon 0
//   sy  = y0 de l'arête (start)
//   ey  = y1 de l'arête (end)
//
// Avance en fin de scanline :
//   z->fx += z->fdx;   (fx passe de y → y+1, prêt pour la prochaine scanline)
//
// Contribution d'une arête à une scanline :
//   La fonction NkFillActiveEdgesNew() calcule exactement quels pixels
//   sont touchés par le trapèze formé par l'arête entre y_top et y_bottom.
//   Elle écrit dans scanline[x] la contribution "locale" et dans
//   scanline2[x+1] le déversement "fill" (tout ce qui est à droite).
//
// PROPRIÉTÉ CLÉ :
//   sum(scanline[0..W-1] + scanline_fill déversement) = 0 pour un contour fermé.
//   Cela garantit que l'accumulateur revient à 0 → aucun artefact dans les zones vides.
// -----------------------------------------------------------------------------

#include "NkFontParser.h"
#include <math.h>
#include <string.h>

namespace nkentseu
{
    namespace nkfont
    {

        // ============================================================
        // CONSTANTES
        // ============================================================

        static constexpr nkft_int32 NK_RE_MAX_EDGES  = 8192;
        static constexpr nkft_int32 NK_RE_MAX_ACTIVE = 1024;
        static constexpr nkft_int32 NK_RE_SCAN_W     = 4096; // largeur maximale de bitmap supportée
        // scanline2 a besoin de W+1 slots (déversement possible sur le slot W)
        static constexpr nkft_int32 NK_RE_SCAN2_W    = NK_RE_SCAN_W + 1;

        // ============================================================
        // STRUCTURES (représentation des arêtes)
        // ============================================================

        // Arête brute (avant activation)
        struct NkREdge
        {
            nkft_float32 x0, y0, x1, y1;
            nkft_int32   invert; // 1 si la direction est vers le haut dans l'espace original
        };

        // Arête active pendant le parcours des scanlines
        struct NkRActive
        {
            struct NkRActive* next;
            nkft_float32 fx;        // position X au TOP de la scanline courante (à y+0.0)
            nkft_float32 fdx;       // delta X par scanline entière
            nkft_float32 fdy;       // 1/fdx si fdx != 0, sinon 0
            nkft_float32 direction; // +1.0 ou -1.0 selon le sens de l'arête
            nkft_float32 sy;        // y0 de l'arête (start)
            nkft_float32 ey;        // y1 de l'arête (end)
        };

        // ============================================================
        // ÉTAT GLOBAL (buffers réutilisables)
        // ============================================================

        static NkREdge   sREdges[NK_RE_MAX_EDGES];
        static nkft_int32 sRNum = 0;

        // Pool d'arêtes actives (évite les allocations dynamiques)
        static NkRActive  sRPool[NK_RE_MAX_ACTIVE];
        static nkft_int32 sRPoolTop = 0;

        // Deux buffers de scanline
        static nkft_float32 sScanline [NK_RE_SCAN_W ];   // contribution locale
        static nkft_float32 sScanline2[NK_RE_SCAN2_W];   // déversement "fill"

        // ============================================================
        // GESTION DU POOL D'ARÊTES ACTIVES
        // ============================================================

        static NkRActive* NkPoolAlloc()
        {
            if (sRPoolTop >= NK_RE_MAX_ACTIVE)
            {
                return nullptr;
            }
            return &sRPool[sRPoolTop++];
        }

        static void NkPoolReset()
        {
            sRPoolTop = 0;
        }

        // ============================================================
        // AJOUT D'UNE ARÊTE (segment de ligne)
        // ============================================================

        static void NkPushEdge(nkft_float32 x0, nkft_float32 y0,
                             nkft_float32 x1, nkft_float32 y1,
                             nkft_int32 invert)
        {
            // Ignorer les arêtes de hauteur nulle
            if (y0 == y1)
            {
                return;
            }
            if (sRNum >= NK_RE_MAX_EDGES)
            {
                return;
            }
            NkREdge& e = sREdges[sRNum++];

            // Normaliser pour que y0 < y1 (top en premier)
            if (y0 < y1)
            {
                e.x0 = x0;
                e.y0 = y0;
                e.x1 = x1;
                e.y1 = y1;
                e.invert = invert;
            }
            else
            {
                e.x0 = x1;
                e.y0 = y1;
                e.x1 = x0;
                e.y1 = y0;
                e.invert = !invert;
            }
        }

        // ============================================================
        // APLATISSEMENT DES COURBES QUADRATIQUES (Bézier)
        // ============================================================

        static void NkFlattenQuad(nkft_float32 x0, nkft_float32 y0,
                                nkft_float32 x1, nkft_float32 y1,
                                nkft_float32 x2, nkft_float32 y2,
                                nkft_float32 objspace_flatness_sq,
                                nkft_int32 n)
        {
            // Calcul du milieu de la courbe vs la corde
            nkft_float32 mx = (x0 + 2 * x1 + x2) / 4;
            nkft_float32 my = (y0 + 2 * y1 + y2) / 4;
            nkft_float32 dx = (x0 + x2) / 2 - mx;
            nkft_float32 dy = (y0 + y2) / 2 - my;

            // Sécurité : ne pas subdiviser trop profondément
            if (n > 16)
            {
                NkPushEdge(x0, y0, x2, y2, 1);
                return;
            }

            // Si l'écart dépasse la tolérance, subdiviser
            if (dx * dx + dy * dy > objspace_flatness_sq)
            {
                nkft_float32 m01x = (x0 + x1) * 0.5f;
                nkft_float32 m01y = (y0 + y1) * 0.5f;
                nkft_float32 m12x = (x1 + x2) * 0.5f;
                nkft_float32 m12y = (y1 + y2) * 0.5f;
                nkft_float32 mmx  = (m01x + m12x) * 0.5f;
                nkft_float32 mmy  = (m01y + m12y) * 0.5f;

                NkFlattenQuad(x0, y0, m01x, m01y, mmx, mmy, objspace_flatness_sq, n + 1);
                NkFlattenQuad(mmx, mmy, m12x, m12y, x2, y2, objspace_flatness_sq, n + 1);
            }
            else
            {
                NkPushEdge(x0, y0, x2, y2, 1);
            }
        }

        // ============================================================
        // APLATISSEMENT DES COURBES CUBIQUES (Bézier)
        // ============================================================

        static void NkFlattenCubic(nkft_float32 x0, nkft_float32 y0,
                                 nkft_float32 x1, nkft_float32 y1,
                                 nkft_float32 x2, nkft_float32 y2,
                                 nkft_float32 x3, nkft_float32 y3,
                                 nkft_float32 objspace_flatness_sq,
                                 nkft_int32 n)
        {
            nkft_float32 dx0 = x1 - x0;
            nkft_float32 dy0 = y1 - y0;
            nkft_float32 dx1 = x2 - x1;
            nkft_float32 dy1 = y2 - y1;
            nkft_float32 dx2 = x3 - x2;
            nkft_float32 dy2 = y3 - y2;
            nkft_float32 dx  = x3 - x0;
            nkft_float32 dy  = y3 - y0;

            nkft_float32 ll = (nkft_float32)(
                sqrtf(dx0 * dx0 + dy0 * dy0) +
                sqrtf(dx1 * dx1 + dy1 * dy1) +
                sqrtf(dx2 * dx2 + dy2 * dy2));
            nkft_float32 sl = sqrtf(dx * dx + dy * dy);
            nkft_float32 flat_sq = ll * ll - sl * sl;

            if (n > 16)
            {
                NkPushEdge(x0, y0, x3, y3, 1);
                return;
            }

            if (flat_sq > objspace_flatness_sq)
            {
                nkft_float32 x01 = (x0 + x1) * 0.5f;
                nkft_float32 y01 = (y0 + y1) * 0.5f;
                nkft_float32 x12 = (x1 + x2) * 0.5f;
                nkft_float32 y12 = (y1 + y2) * 0.5f;
                nkft_float32 x23 = (x2 + x3) * 0.5f;
                nkft_float32 y23 = (y2 + y3) * 0.5f;
                nkft_float32 xa  = (x01 + x12) * 0.5f;
                nkft_float32 ya  = (y01 + y12) * 0.5f;
                nkft_float32 xb  = (x12 + x23) * 0.5f;
                nkft_float32 yb  = (y12 + y23) * 0.5f;
                nkft_float32 mx  = (xa + xb) * 0.5f;
                nkft_float32 my  = (ya + yb) * 0.5f;

                NkFlattenCubic(x0, y0, x01, y01, xa, ya, mx, my, objspace_flatness_sq, n + 1);
                NkFlattenCubic(mx, my, xb, yb, x23, y23, x3, y3, objspace_flatness_sq, n + 1);
            }
            else
            {
                NkPushEdge(x0, y0, x3, y3, 1);
            }
        }

        // ============================================================
        // CONSTRUCTION DES ARÊTES À PARTIR D'UN VERTEX BUFFER
        // ============================================================

        static void NkBuildEdges(const NkFontVertexBuffer* vb,
                               nkft_float32 sx, nkft_float32 sy,
                               nkft_float32 ox, nkft_float32 oy)
        {
            sRNum = 0;
            nkft_float32 px = 0.f;
            nkft_float32 py = 0.f;

            // Tolérance d'aplatissement en pixels (valeur empirique)
            static constexpr nkft_float32 flatness = 0.35f;
            const nkft_float32 flat_sq = flatness * flatness;

            for (nkft_uint32 i = 0; i < vb->count; ++i)
            {
                const NkFontVertex& v = vb->verts[i];

                // Application de l'échelle et du flip Y (TrueType Y-up → écran Y-down)
                nkft_float32 vx = v.x * sx + ox;
                nkft_float32 vy = -v.y * sy + oy;

                switch (v.type)
                {
                    case NK_FONT_VERTEX_MOVE:
                        px = vx;
                        py = vy;
                        break;

                    case NK_FONT_VERTEX_LINE:
                        if (py != vy)
                        {
                            NkPushEdge(px, py, vx, vy, 1);
                        }
                        px = vx;
                        py = vy;
                        break;

                    case NK_FONT_VERTEX_CURVE:
                    {
                        nkft_float32 cx = v.cx * sx + ox;
                        nkft_float32 cy = -v.cy * sy + oy;
                        NkFlattenQuad(px, py, cx, cy, vx, vy, flat_sq, 0);
                        px = vx;
                        py = vy;
                        break;
                    }

                    case NK_FONT_VERTEX_CUBIC:
                    {
                        nkft_float32 c0x = v.cx  * sx + ox;
                        nkft_float32 c0y = -v.cy  * sy + oy;
                        nkft_float32 c1x = v.cx1 * sx + ox;
                        nkft_float32 c1y = -v.cy1 * sy + oy;
                        NkFlattenCubic(px, py, c0x, c0y, c1x, c1y, vx, vy, flat_sq, 0);
                        px = vx;
                        py = vy;
                        break;
                    }
                }
            }
        }

        // ============================================================
        // TRI DES ARÊTES PAR y0 CROISSANT (INSERTION SORT)
        // ============================================================

        static void NkSortEdges()
        {
            for (nkft_int32 i = 1; i < sRNum; ++i)
            {
                NkREdge t = sREdges[i];
                nkft_int32 j = i - 1;
                while (j >= 0 && sREdges[j].y0 > t.y0)
                {
                    sREdges[j + 1] = sREdges[j];
                    --j;
                }
                sREdges[j + 1] = t;
            }
        }

        // ============================================================
        // TRAITEMENT D'UN SEGMENT D'ARÊTE CLIPPÉ À UN PIXEL
        // ============================================================
        // Calcule la contribution d'un segment d'arête qui reste dans le pixel [x, x+1].
        // Écrit dans scanline[x] (contribution locale).
        // ============================================================

        static void NkHandleClippedEdge(nkft_float32* scanline,
                                      nkft_int32 x,
                                      NkRActive* e,
                                      nkft_float32 x0,
                                      nkft_float32 y0,
                                      nkft_float32 x1,
                                      nkft_float32 y1)
        {
            if (y0 == y1)
            {
                return;
            }

            // Clip vertical aux bornes sy..ey de l'arête
            if (y0 > e->ey)
            {
                return;
            }
            if (y1 < e->sy)
            {
                return;
            }
            if (y0 < e->sy)
            {
                x0 += (x1 - x0) * (e->sy - y0) / (y1 - y0);
                y0 = e->sy;
            }
            if (y1 > e->ey)
            {
                x1 += (x1 - x0) * (e->ey - y1) / (y1 - y0);
                y1 = e->ey;
            }

            // Contribution au pixel x : coverage = direction * (y1 - y0) * (1 - avg_x_in_pixel)
            if (x0 <= x && x1 <= x)
            {
                scanline[x] += e->direction * (y1 - y0);
            }
            else if (x0 >= x + 1 && x1 >= x + 1)
            {
                // entièrement à droite du pixel → aucune contribution locale
            }
            else
            {
                // Partiellement dans le pixel
                scanline[x] += e->direction * (y1 - y0) * (1.f - ((x0 - x) + (x1 - x)) / 2.f);
            }
        }

        // ============================================================
        // FONCTIONS DE CALCUL D'AIRES (TRAPÈZES / TRIANGLES)
        // ============================================================

        static inline nkft_float32 NkSizedTrapArea(nkft_float32 height,
                                                  nkft_float32 top_w,
                                                  nkft_float32 bot_w)
        {
            return (top_w + bot_w) / 2.f * height;
        }

        static inline nkft_float32 NkPosTrapArea(nkft_float32 height,
                                               nkft_float32 tx0,
                                               nkft_float32 tx1,
                                               nkft_float32 bx0,
                                               nkft_float32 bx1)
        {
            return NkSizedTrapArea(height, tx1 - tx0, bx1 - bx0);
        }

        static inline nkft_float32 NkSizedTriArea(nkft_float32 height,
                                                nkft_float32 width)
        {
            return height * width / 2.f;
        }

        // ============================================================
        // REMPLISSAGE DES ARÊTES ACTIVES (VERSION À DEUX BUFFERS)
        // ============================================================
        // Pour chaque arête active, calcule sa contribution à la scanline Y.
        // Utilise DEUX buffers :
        //   scanline[x]       += contribution locale (aire dans le pixel x)
        //   scanline_fill[x]  += hauteur de l'arête au-dessus (déversement vers droite)
        // ============================================================

        static void NkFillActiveEdgesNew(nkft_float32* scanline,
                                       nkft_float32* scanline_fill,
                                       nkft_int32 len,
                                       NkRActive* e,
                                       nkft_float32 y_top)
        {
            nkft_float32 y_bottom = y_top + 1.f;

            while (e)
            {
                if (e->fdx == 0.f)
                {
                    // Arête verticale parfaite
                    nkft_float32 x0 = e->fx;
                    if (x0 < len)
                    {
                        if (x0 >= 0.f)
                        {
                            NkHandleClippedEdge(scanline, (nkft_int32)x0,    e, x0, y_top, x0, y_bottom);
                            NkHandleClippedEdge(scanline_fill - 1, (nkft_int32)x0 + 1, e, x0, y_top, x0, y_bottom);
                        }
                        else
                        {
                            NkHandleClippedEdge(scanline_fill - 1, 0, e, x0, y_top, x0, y_bottom);
                        }
                    }
                }
                else
                {
                    // Arête oblique
                    nkft_float32 x0 = e->fx;
                    nkft_float32 dx = e->fdx;
                    nkft_float32 xb = x0 + dx;
                    nkft_float32 x_top;
                    nkft_float32 x_bottom;
                    nkft_float32 sy0;
                    nkft_float32 sy1;
                    nkft_float32 dy = e->fdy;

                    if (e->sy > y_top)
                    {
                        x_top = x0 + dx * (e->sy - y_top);
                        sy0 = e->sy;
                    }
                    else
                    {
                        x_top = x0;
                        sy0 = y_top;
                    }
                    if (e->ey < y_bottom)
                    {
                        x_bottom = x0 + dx * (e->ey - y_top);
                        sy1 = e->ey;
                    }
                    else
                    {
                        x_bottom = xb;
                        sy1 = y_bottom;
                    }

                    if (x_top >= 0.f && x_bottom >= 0.f && x_top < len && x_bottom < len)
                    {
                        // Cas rapide : l'arête est entièrement dans le bitmap
                        if ((nkft_int32)x_top == (nkft_int32)x_bottom)
                        {
                            // Traverse un seul pixel en X
                            nkft_float32 height;
                            nkft_int32 x = (nkft_int32)x_top;
                            height = (sy1 - sy0) * e->direction;
                            scanline[x]      += NkPosTrapArea(height, x_top, x + 1.f, x_bottom, x + 1.f);
                            scanline_fill[x] += height;
                        }
                        else
                        {
                            // Traverse plusieurs pixels en X
                            nkft_int32 x;
                            nkft_int32 x1;
                            nkft_int32 x2;
                            nkft_float32 y_crossing;
                            nkft_float32 y_final;
                            nkft_float32 step;
                            nkft_float32 sign;
                            nkft_float32 area;

                            if (x_top > x_bottom)
                            {
                                // Inverser pour aller de gauche à droite
                                nkft_float32 t;
                                sy0 = y_bottom - (sy0 - y_top);
                                sy1 = y_bottom - (sy1 - y_top);
                                t = sy0;
                                sy0 = sy1;
                                sy1 = t;
                                t = x_bottom;
                                x_bottom = x_top;
                                x_top = t;
                                dx = -dx;
                                dy = -dy;
                                t = x0;
                                x0 = xb;
                                xb = t;
                            }

                            x1 = (nkft_int32)x_top;
                            x2 = (nkft_int32)x_bottom;

                            // Intersection avec la colonne x1+1
                            y_crossing = y_top + dy * (x1 + 1 - x0);
                            // Intersection avec la colonne x2
                            y_final    = y_top + dy * (x2 - x0);

                            if (y_crossing > y_bottom)
                            {
                                y_crossing = y_bottom;
                            }

                            sign = e->direction;
                            area = sign * (y_crossing - sy0);
                            scanline[x1] += NkSizedTriArea(area, x1 + 1 - x_top);

                            if (y_final > y_bottom)
                            {
                                y_final = y_bottom;
                                dy = (y_final - y_crossing) / (x2 - (x1 + 1));
                            }

                            step = sign * dy * 1.f;
                            for (x = x1 + 1; x < x2; ++x)
                            {
                                scanline[x] += area + step / 2.f;
                                area += step;
                            }

                            scanline[x2] += area + sign * NkPosTrapArea(
                                sy1 - y_final,
                                (nkft_float32)x2,
                                x2 + 1.f,
                                x_bottom,
                                x2 + 1.f
                            );
                            scanline_fill[x2] += sign * (sy1 - sy0);
                        }
                    }
                    else
                    {
                        // Chement lent : clipping par force brute (arête sort du bitmap)
                        for (nkft_int32 x = 0; x < len; ++x)
                        {
                            nkft_float32 y0_ = y_top;
                            nkft_float32 x1_ = (nkft_float32)x;
                            nkft_float32 x2_ = (nkft_float32)(x + 1);
                            nkft_float32 x3_ = xb;
                            nkft_float32 y3_ = y_bottom;
                            nkft_float32 y1_ = (x - x0) / dx + y_top;
                            nkft_float32 y2_ = (x + 1 - x0) / dx + y_top;

                            if (x0 < x1_ && x3_ > x2_)
                            {
                                NkHandleClippedEdge(scanline, x, e, x0, y0_, x1_, y1_);
                                NkHandleClippedEdge(scanline, x, e, x1_, y1_, x2_, y2_);
                                NkHandleClippedEdge(scanline, x, e, x2_, y2_, x3_, y3_);
                            }
                            else if (x3_ < x1_ && x0 > x2_)
                            {
                                NkHandleClippedEdge(scanline, x, e, x0, y0_, x2_, y2_);
                                NkHandleClippedEdge(scanline, x, e, x2_, y2_, x1_, y1_);
                                NkHandleClippedEdge(scanline, x, e, x1_, y1_, x3_, y3_);
                            }
                            else if (x0 < x1_ && x3_ > x1_)
                            {
                                NkHandleClippedEdge(scanline, x, e, x0, y0_, x1_, y1_);
                                NkHandleClippedEdge(scanline, x, e, x1_, y1_, x3_, y3_);
                            }
                            else if (x3_ < x1_ && x0 > x1_)
                            {
                                NkHandleClippedEdge(scanline, x, e, x0, y0_, x1_, y1_);
                                NkHandleClippedEdge(scanline, x, e, x1_, y1_, x3_, y3_);
                            }
                            else if (x0 < x2_ && x3_ > x2_)
                            {
                                NkHandleClippedEdge(scanline, x, e, x0, y0_, x2_, y2_);
                                NkHandleClippedEdge(scanline, x, e, x2_, y2_, x3_, y3_);
                            }
                            else if (x3_ < x2_ && x0 > x2_)
                            {
                                NkHandleClippedEdge(scanline, x, e, x0, y0_, x2_, y2_);
                                NkHandleClippedEdge(scanline, x, e, x2_, y2_, x3_, y3_);
                            }
                            else
                            {
                                NkHandleClippedEdge(scanline, x, e, x0, y0_, x3_, y3_);
                            }
                        }
                    }
                }
                e = e->next;
            }
        }

        // ============================================================
        // RASTÉRISATION PRINCIPALE (PARCOURS DES SCANLINES)
        // ============================================================

        static void NkDoRasterize(nkft_uint8* out,
                                nkft_int32 W,
                                nkft_int32 H,
                                nkft_int32 stride,
                                nkft_int32 off_x,
                                nkft_int32 off_y)
        {
            NkPoolReset();

            NkRActive* active = nullptr;
            nkft_int32 j = 0;   // ligne de sortie courante
            nkft_int32 ei = 0;  // index dans sREdges[]

            // Sentinelle : la dernière arête du tableau
            sREdges[sRNum].y0 = (nkft_float32)(off_y + H) + 1.f;

            while (j < H)
            {
                nkft_float32 scan_y_top    = (nkft_float32)(off_y + j) + 0.0f;
                nkft_float32 scan_y_bottom = (nkft_float32)(off_y + j) + 1.0f;

                // Nettoyage des deux buffers de scanline
                memset(sScanline,  0, (nkft_size)W        * sizeof(nkft_float32));
                memset(sScanline2, 0, (nkft_size)(W + 1)  * sizeof(nkft_float32));

                // Suppression des arêtes terminées avant le top de cette scanline
                {
                    NkRActive** step = &active;
                    while (*step)
                    {
                        NkRActive* z = *step;
                        if (z->ey <= scan_y_top)
                        {
                            *step = z->next;
                            z->direction = 0; // marque comme libre
                        }
                        else
                        {
                            step = &((*step)->next);
                        }
                    }
                }

                // Activation des arêtes qui commencent avant le bas de la scanline
                while (sREdges[ei].y0 <= scan_y_bottom)
                {
                    if (sREdges[ei].y0 != sREdges[ei].y1)
                    {
                        NkRActive* z = NkPoolAlloc();
                        if (z)
                        {
                            nkft_float32 dxdy = (sREdges[ei].x1 - sREdges[ei].x0) /
                                                (sREdges[ei].y1 - sREdges[ei].y0);
                            z->fdx = dxdy;
                            z->fdy = (dxdy != 0.f) ? (1.f / dxdy) : 0.f;
                            // fx = position X au TOP de la scanline (scan_y_top)
                            z->fx  = sREdges[ei].x0 + dxdy * (scan_y_top - sREdges[ei].y0);
                            z->fx -= (nkft_float32)off_x; // décalage bitmap
                            z->direction = sREdges[ei].invert ? 1.f : -1.f;
                            z->sy = sREdges[ei].y0;
                            z->ey = sREdges[ei].y1;

                            // Sécurité flottante : garantir ey >= scan_y_top
                            if (j == 0 && off_y != 0 && z->ey < scan_y_top)
                            {
                                z->ey = scan_y_top;
                            }
                            z->next = active;
                            active  = z;
                        }
                    }
                    ++ei;
                }

                // Calcul des contributions de toutes les arêtes actives
                if (active)
                {
                    NkFillActiveEdgesNew(sScanline, sScanline2 + 1, W, active, scan_y_top);
                }

                // Intégration finale pour produire les pixels de la scanline
                {
                    nkft_float32 sum = 0.f;
                    nkft_uint8* row = out + j * stride;
                    for (nkft_int32 i = 0; i < W; ++i)
                    {
                        nkft_float32 k;
                        nkft_int32 m;
                        sum += sScanline2[i];      // accumule le déversement "fill"
                        k = sScanline[i] + sum;    // contribution locale + tout ce qui est à gauche
                        k = (k < 0.f ? -k : k) * 255.f + 0.5f;
                        m = (nkft_int32)k;
                        if (m > 255)
                        {
                            m = 255;
                        }
                        row[i] = (nkft_uint8)m;
                    }
                }

                // Avance toutes les arêtes actives d'une scanline (fx passe de y → y+1)
                {
                    NkRActive** step = &active;
                    while (*step)
                    {
                        NkRActive* z = *step;
                        z->fx += z->fdx;
                        step = &((*step)->next);
                    }
                }

                ++j;
            }
        }

        // ============================================================
        // API PUBLIQUE : GÉNÉRATION D'UN BITMAP DE GLYPHE
        // ============================================================

        void NkMakeGlyphBitmapExact(const NkFontFaceInfo* info,
                                  nkft_uint8*  output,
                                  nkft_int32   outW,
                                  nkft_int32   outH,
                                  nkft_int32   outStride,
                                  nkft_float32 scaleX,
                                  nkft_float32 scaleY,
                                  nkft_float32 shiftX,
                                  nkft_float32 shiftY,
                                  NkGlyphId    glyph)
        {
            if (!info || !output || outW <= 0 || outH <= 0)
            {
                return;
            }

            // Effacement du bitmap de sortie
            for (nkft_int32 r = 0; r < outH; ++r)
            {
                memset(output + r * outStride, 0, (nkft_size)outW);
            }

            // Récupération des vertices du contour du glyphe
            NkFontVertexBuffer vb;
            if (!NkGetGlyphShape(info, glyph, &vb) || vb.count == 0)
            {
                return;
            }

            // Calcul de la bounding box en pixels (pour connaître l'offset)
            nkft_int32 bx0, by0, bx1, by1;
            NkGetGlyphBitmapBox(info, glyph, scaleX, scaleY, shiftX, shiftY,
                              &bx0, &by0, &bx1, &by1);

            // Construction des arêtes avec l'offset pour que le glyphe
            // commence à (0,0) dans le bitmap de sortie
            NkBuildEdges(&vb, scaleX, scaleY,
                       shiftX - (nkft_float32)bx0,
                       shiftY - (nkft_float32)by0);

            if (sRNum == 0)
            {
                return;
            }

            // Tri des arêtes par y0 croissant
            NkSortEdges();

            // Ajout d'une sentinelle à la fin (NkDoRasterize l'utilise)
            if (sRNum < NK_RE_MAX_EDGES)
            {
                sREdges[sRNum].y0 = 1e10f;
                sREdges[sRNum].y1 = 1e10f;
            }

            // Rastérisation finale
            NkDoRasterize(output, outW, outH, outStride, 0, 0);
        }

        // Alias de la fonction principale
        void NkMakeGlyphBitmap(const NkFontFaceInfo* info,
                             nkft_uint8*  output,
                             nkft_int32   outW,
                             nkft_int32   outH,
                             nkft_int32   outStride,
                             nkft_float32 scaleX,
                             nkft_float32 scaleY,
                             nkft_float32 shiftX,
                             nkft_float32 shiftY,
                             NkGlyphId    glyph)
        {
            NkMakeGlyphBitmapExact(info, output, outW, outH, outStride, scaleX, scaleY, shiftX, shiftY, glyph);
        }

    } // namespace nkfont
} // namespace nkentseu

/*
// -----------------------------------------------------------------------------
// FICHIER: NKFont/Core/NkFontRasterizer.cpp
// DESCRIPTION: Rasteriseur scanline pour les contours de glyphes TrueType/OpenType.
//              Implémente l'algorithme classique à deux buffers (local + fill).
// -----------------------------------------------------------------------------

#include "NkFontParser.h"
#include <math.h>
#include <string.h>

namespace nkentseu {
    namespace nkfont {

        // ============================================================
        // CONSTANTES
        // ============================================================

        static constexpr nkft_int32 NK_RE_MAX_EDGES  = 8192;  // Nombre maximal d'arêtes
        static constexpr nkft_int32 NK_RE_MAX_ACTIVE = 1024;  // Arêtes actives maximales
        static constexpr nkft_int32 NK_RE_SCAN_W     = 4096;  // Largeur max de bitmap
        static constexpr nkft_int32 NK_RE_SCAN2_W    = NK_RE_SCAN_W + 1; // scanline2 a besoin de +1

        // ============================================================
        // STRUCTURES (arêtes et arêtes actives)
        // ============================================================

        struct NkREdge {
            nkft_float32 x0, y0, x1, y1;
            nkft_int32   invert; // 1 = direction montante dans l'espace original
        };

        struct NkRActive {
            struct NkRActive* next;
            nkft_float32 fx;        // Position X au top de la scanline courante
            nkft_float32 fdx;       // Delta X par scanline entière
            nkft_float32 fdy;       // 1/fdx si fdx != 0, sinon 0
            nkft_float32 direction; // +1.0 ou -1.0
            nkft_float32 sy;        // Y de début de l'arête
            nkft_float32 ey;        // Y de fin de l'arête
        };

        // ============================================================
        // ÉTAT GLOBAL (pool d'arêtes et buffers de scanline)
        // ============================================================

        static NkREdge   sREdges[NK_RE_MAX_EDGES];
        static nkft_int32 sRNum = 0;

        static NkRActive  sRPool[NK_RE_MAX_ACTIVE];
        static nkft_int32 sRPoolTop = 0;

        static nkft_float32 sScanline [NK_RE_SCAN_W ];   // Contribution locale
        static nkft_float32 sScanline2[NK_RE_SCAN2_W];   // Déversement (fill)

        // ============================================================
        // Gestion du pool d'arêtes actives
        // ============================================================

        static NkRActive* pool_alloc() {
            if (sRPoolTop >= NK_RE_MAX_ACTIVE) return nullptr;
            return &sRPool[sRPoolTop++];
        }

        static void pool_reset() {
            sRPoolTop = 0;
        }

        // ============================================================
        // Ajout d'une arête (normalisée avec y0 < y1)
        // ============================================================

        static void PushEdge(nkft_float32 x0, nkft_float32 y0,
                            nkft_float32 x1, nkft_float32 y1,
                            nkft_int32 invert) {
            if (y0 == y1) return;
            if (sRNum >= NK_RE_MAX_EDGES) return;
            NkREdge& e = sREdges[sRNum++];
            if (y0 < y1) {
                e.x0 = x0; e.y0 = y0; e.x1 = x1; e.y1 = y1; e.invert = invert;
            } else {
                e.x0 = x1; e.y0 = y1; e.x1 = x0; e.y1 = y0; e.invert = !invert;
            }
        }

        // ============================================================
        // Aplatissement de courbe quadratique (Bézier)
        // ============================================================

        static void FlattenQuad(nkft_float32 x0, nkft_float32 y0,
                                nkft_float32 x1, nkft_float32 y1,
                                nkft_float32 x2, nkft_float32 y2,
                                nkft_float32 objspace_flatness_sq,
                                nkft_int32 n) {
            nkft_float32 mx = (x0 + 2*x1 + x2) / 4;
            nkft_float32 my = (y0 + 2*y1 + y2) / 4;
            nkft_float32 dx = (x0 + x2) / 2 - mx;
            nkft_float32 dy = (y0 + y2) / 2 - my;
            if (n > 16) {
                PushEdge(x0, y0, x2, y2, 1);
                return;
            }
            if (dx*dx + dy*dy > objspace_flatness_sq) {
                nkft_float32 m01x = (x0+x1)*0.5f, m01y = (y0+y1)*0.5f;
                nkft_float32 m12x = (x1+x2)*0.5f, m12y = (y1+y2)*0.5f;
                nkft_float32 mmx  = (m01x+m12x)*0.5f, mmy = (m01y+m12y)*0.5f;
                FlattenQuad(x0, y0, m01x, m01y, mmx, mmy, objspace_flatness_sq, n+1);
                FlattenQuad(mmx, mmy, m12x, m12y, x2, y2, objspace_flatness_sq, n+1);
            } else {
                PushEdge(x0, y0, x2, y2, 1);
            }
        }

        // ============================================================
        // Aplatissement de courbe cubique (Bézier)
        // ============================================================

        static void FlattenCubic(nkft_float32 x0, nkft_float32 y0,
                                nkft_float32 x1, nkft_float32 y1,
                                nkft_float32 x2, nkft_float32 y2,
                                nkft_float32 x3, nkft_float32 y3,
                                nkft_float32 objspace_flatness_sq,
                                nkft_int32 n) {
            nkft_float32 dx0 = x1-x0, dy0 = y1-y0;
            nkft_float32 dx1 = x2-x1, dy1 = y2-y1;
            nkft_float32 dx2 = x3-x2, dy2 = y3-y2;
            nkft_float32 dx  = x3-x0, dy  = y3-y0;
            nkft_float32 ll = (nkft_float32)(sqrtf(dx0*dx0+dy0*dy0) +
                                            sqrtf(dx1*dx1+dy1*dy1) +
                                            sqrtf(dx2*dx2+dy2*dy2));
            nkft_float32 sl = sqrtf(dx*dx+dy*dy);
            nkft_float32 flat_sq = ll*ll - sl*sl;
            if (n > 16) {
                PushEdge(x0, y0, x3, y3, 1);
                return;
            }
            if (flat_sq > objspace_flatness_sq) {
                nkft_float32 x01=(x0+x1)*0.5f, y01=(y0+y1)*0.5f;
                nkft_float32 x12=(x1+x2)*0.5f, y12=(y1+y2)*0.5f;
                nkft_float32 x23=(x2+x3)*0.5f, y23=(y2+y3)*0.5f;
                nkft_float32 xa=(x01+x12)*0.5f, ya=(y01+y12)*0.5f;
                nkft_float32 xb=(x12+x23)*0.5f, yb=(y12+y23)*0.5f;
                nkft_float32 mx=(xa+xb)*0.5f,   my=(ya+yb)*0.5f;
                FlattenCubic(x0,y0, x01,y01, xa,ya, mx,my, objspace_flatness_sq, n+1);
                FlattenCubic(mx,my, xb,yb, x23,y23, x3,y3, objspace_flatness_sq, n+1);
            } else {
                PushEdge(x0, y0, x3, y3, 1);
            }
        }

        // ============================================================
        // Construction des arêtes à partir des vertices du contour
        // ============================================================

        static void BuildEdges(const NkFontVertexBuffer* vb,
                            nkft_float32 sx, nkft_float32 sy,
                            nkft_float32 ox, nkft_float32 oy) {
            sRNum = 0;
            nkft_float32 px = 0.f, py = 0.f;
            const nkft_float32 flatness = 0.35f;
            const nkft_float32 flat_sq = flatness * flatness;

            for (nkft_uint32 i = 0; i < vb->count; ++i) {
                const NkFontVertex& v = vb->verts[i];
                nkft_float32 vx =  v.x * sx + ox;
                nkft_float32 vy = -v.y * sy + oy;   // Flip Y (TrueType → écran)

                switch (v.type) {
                case NK_FONT_VERTEX_MOVE:
                    px = vx; py = vy;
                    break;
                case NK_FONT_VERTEX_LINE:
                    if (py != vy) PushEdge(px, py, vx, vy, 1);
                    px = vx; py = vy;
                    break;
                case NK_FONT_VERTEX_CURVE: {
                    nkft_float32 cx =  v.cx * sx + ox;
                    nkft_float32 cy = -v.cy * sy + oy;
                    FlattenQuad(px, py, cx, cy, vx, vy, flat_sq, 0);
                    px = vx; py = vy;
                    break;
                }
                case NK_FONT_VERTEX_CUBIC: {
                    nkft_float32 c0x =  v.cx  * sx + ox, c0y = -v.cy  * sy + oy;
                    nkft_float32 c1x =  v.cx1 * sx + ox, c1y = -v.cy1 * sy + oy;
                    FlattenCubic(px, py, c0x, c0y, c1x, c1y, vx, vy, flat_sq, 0);
                    px = vx; py = vy;
                    break;
                }
                }
            }
        }

        // ============================================================
        // Comparateur pour tri des arêtes par y0
        // ============================================================

        static int CompareEdge(const void* a, const void* b) {
            const NkREdge* ea = (const NkREdge*)a;
            const NkREdge* eb = (const NkREdge*)b;
            if (ea->y0 < eb->y0) return -1;
            if (ea->y0 > eb->y0) return  1;
            return 0;
        }

        static void SortEdges() {
            for (nkft_int32 i = 1; i < sRNum; ++i) {
                NkREdge t = sREdges[i];
                nkft_int32 j = i - 1;
                while (j >= 0 && sREdges[j].y0 > t.y0) {
                    sREdges[j+1] = sREdges[j];
                    --j;
                }
                sREdges[j+1] = t;
            }
        }

        // ============================================================
        // Gestion d'un segment d'arête dans un pixel donné
        // ============================================================

        static void HandleClippedEdge(nkft_float32* scanline, nkft_int32 x,
                                    NkRActive* e,
                                    nkft_float32 x0, nkft_float32 y0,
                                    nkft_float32 x1, nkft_float32 y1) {
            if (y0 == y1) return;
            if (y0 > e->ey) return;
            if (y1 < e->sy) return;
            if (y0 < e->sy) {
                x0 += (x1-x0) * (e->sy - y0) / (y1-y0);
                y0 = e->sy;
            }
            if (y1 > e->ey) {
                x1 += (x1-x0) * (e->ey - y1) / (y1-y0);
                y1 = e->ey;
            }
            if (x0 <= x && x1 <= x)
                scanline[x] += e->direction * (y1-y0);
            else if (x0 >= x+1 && x1 >= x+1)
                ; // entièrement à droite du pixel
            else {
                // partiellement dans le pixel
                scanline[x] += e->direction * (y1-y0) * (1.f - ((x0-x)+(x1-x))/2.f);
            }
        }

        // ============================================================
        // Aires de trapèzes / triangles (helpers)
        // ============================================================

        static inline nkft_float32 SizedTrapArea(nkft_float32 height,
                                                nkft_float32 top_w,
                                                nkft_float32 bot_w) {
            return (top_w + bot_w) / 2.f * height;
        }

        static inline nkft_float32 PosTrapArea(nkft_float32 height,
                                                nkft_float32 tx0, nkft_float32 tx1,
                                                nkft_float32 bx0, nkft_float32 bx1) {
            return SizedTrapArea(height, tx1-tx0, bx1-bx0);
        }

        static inline nkft_float32 SizedTriArea(nkft_float32 height, nkft_float32 width) {
            return height * width / 2.f;
        }

        // ============================================================
        // Remplissage des arêtes actives pour une scanline (deux buffers)
        // ============================================================

        static void FillActiveEdgesNew(nkft_float32* scanline,
                                    nkft_float32* scanline_fill,
                                    nkft_int32 len,
                                    NkRActive* e,
                                    nkft_float32 y_top) {
            nkft_float32 y_bottom = y_top + 1.f;
            while (e) {
                if (e->fdx == 0.f) {
                    // Arête verticale
                    nkft_float32 x0 = e->fx;
                    if (x0 < len) {
                        if (x0 >= 0.f) {
                            HandleClippedEdge(scanline, (nkft_int32)x0,    e, x0, y_top, x0, y_bottom);
                            HandleClippedEdge(scanline_fill-1, (nkft_int32)x0+1, e, x0, y_top, x0, y_bottom);
                        } else {
                            HandleClippedEdge(scanline_fill-1, 0, e, x0, y_top, x0, y_bottom);
                        }
                    }
                } else {
                    // Arête oblique
                    nkft_float32 x0 = e->fx;
                    nkft_float32 dx = e->fdx;
                    nkft_float32 xb = x0 + dx;
                    nkft_float32 x_top, x_bottom, sy0, sy1;
                    nkft_float32 dy = e->fdy;

                    if (e->sy > y_top) {
                        x_top = x0 + dx * (e->sy - y_top);
                        sy0 = e->sy;
                    } else {
                        x_top = x0;
                        sy0 = y_top;
                    }
                    if (e->ey < y_bottom) {
                        x_bottom = x0 + dx * (e->ey - y_top);
                        sy1 = e->ey;
                    } else {
                        x_bottom = xb;
                        sy1 = y_bottom;
                    }

                    if (x_top >= 0.f && x_bottom >= 0.f && x_top < len && x_bottom < len) {
                        // Cas entièrement dans le bitmap
                        if ((nkft_int32)x_top == (nkft_int32)x_bottom) {
                            nkft_float32 height;
                            nkft_int32 x = (nkft_int32)x_top;
                            height = (sy1 - sy0) * e->direction;
                            scanline[x]      += PosTrapArea(height, x_top, x+1.f, x_bottom, x+1.f);
                            scanline_fill[x] += height;
                        } else {
                            nkft_int32 x, x1, x2;
                            nkft_float32 y_crossing, y_final, step, sign, area;
                            if (x_top > x_bottom) {
                                nkft_float32 t;
                                sy0 = y_bottom - (sy0 - y_top);
                                sy1 = y_bottom - (sy1 - y_top);
                                t = sy0; sy0 = sy1; sy1 = t;
                                t = x_bottom; x_bottom = x_top; x_top = t;
                                dx = -dx; dy = -dy;
                                t = x0; x0 = xb; xb = t;
                            }
                            x1 = (nkft_int32)x_top;
                            x2 = (nkft_int32)x_bottom;
                            y_crossing = y_top + dy * (x1+1 - x0);
                            y_final    = y_top + dy * (x2 - x0);
                            if (y_crossing > y_bottom) y_crossing = y_bottom;
                            sign = e->direction;
                            area = sign * (y_crossing - sy0);
                            scanline[x1] += SizedTriArea(area, x1+1 - x_top);
                            if (y_final > y_bottom) {
                                y_final = y_bottom;
                                dy = (y_final - y_crossing) / (x2 - (x1+1));
                            }
                            step = sign * dy * 1.f;
                            for (x = x1+1; x < x2; ++x) {
                                scanline[x] += area + step/2.f;
                                area += step;
                            }
                            scanline[x2] += area + sign * PosTrapArea(sy1 - y_final,
                                                    (nkft_float32)x2, x2+1.f,
                                                    x_bottom, x2+1.f);
                            scanline_fill[x2] += sign * (sy1 - sy0);
                        }
                    } else {
                        // Chemin lent : clipping par pixel (arête sort du bitmap)
                        for (nkft_int32 x = 0; x < len; ++x) {
                            nkft_float32 y0_ = y_top;
                            nkft_float32 x1_ = (nkft_float32)x;
                            nkft_float32 x2_ = (nkft_float32)(x+1);
                            nkft_float32 x3_ = xb;
                            nkft_float32 y3_ = y_bottom;
                            nkft_float32 y1_ = (x - x0) / dx + y_top;
                            nkft_float32 y2_ = (x+1 - x0) / dx + y_top;
                            if (x0 < x1_ && x3_ > x2_) {
                                HandleClippedEdge(scanline,x,e, x0,y0_, x1_,y1_);
                                HandleClippedEdge(scanline,x,e, x1_,y1_, x2_,y2_);
                                HandleClippedEdge(scanline,x,e, x2_,y2_, x3_,y3_);
                            } else if (x3_ < x1_ && x0 > x2_) {
                                HandleClippedEdge(scanline,x,e, x0,y0_, x2_,y2_);
                                HandleClippedEdge(scanline,x,e, x2_,y2_, x1_,y1_);
                                HandleClippedEdge(scanline,x,e, x1_,y1_, x3_,y3_);
                            } else if (x0 < x1_ && x3_ > x1_) {
                                HandleClippedEdge(scanline,x,e, x0,y0_, x1_,y1_);
                                HandleClippedEdge(scanline,x,e, x1_,y1_, x3_,y3_);
                            } else if (x3_ < x1_ && x0 > x1_) {
                                HandleClippedEdge(scanline,x,e, x0,y0_, x1_,y1_);
                                HandleClippedEdge(scanline,x,e, x1_,y1_, x3_,y3_);
                            } else if (x0 < x2_ && x3_ > x2_) {
                                HandleClippedEdge(scanline,x,e, x0,y0_, x2_,y2_);
                                HandleClippedEdge(scanline,x,e, x2_,y2_, x3_,y3_);
                            } else if (x3_ < x2_ && x0 > x2_) {
                                HandleClippedEdge(scanline,x,e, x0,y0_, x2_,y2_);
                                HandleClippedEdge(scanline,x,e, x2_,y2_, x3_,y3_);
                            } else {
                                HandleClippedEdge(scanline,x,e, x0,y0_, x3_,y3_);
                            }
                        }
                    }
                }
                e = e->next;
            }
        }

        // ============================================================
        // Rastérisation principale (deux buffers + intégration)
        // ============================================================

        static void DoRasterize(nkft_uint8* out, nkft_int32 W, nkft_int32 H,
                                nkft_int32 stride, nkft_int32 off_x, nkft_int32 off_y) {
            pool_reset();
            NkRActive* active = nullptr;
            nkft_int32 j = 0;
            nkft_int32 ei = 0;
            sREdges[sRNum].y0 = (nkft_float32)(off_y + H) + 1.f; // sentinelle

            while (j < H) {
                nkft_float32 scan_y_top    = (nkft_float32)(off_y + j) + 0.0f;
                nkft_float32 scan_y_bottom = (nkft_float32)(off_y + j) + 1.0f;

                // Nettoyage des buffers
                memset(sScanline,  0, (nkft_size)W        * sizeof(nkft_float32));
                memset(sScanline2, 0, (nkft_size)(W+1)    * sizeof(nkft_float32));

                // Suppression des arêtes terminées
                {
                    NkRActive** step = &active;
                    while (*step) {
                        NkRActive* z = *step;
                        if (z->ey <= scan_y_top) {
                            *step = z->next;
                            z->direction = 0;
                        } else {
                            step = &((*step)->next);
                        }
                    }
                }

                // Activation des nouvelles arêtes
                while (sREdges[ei].y0 <= scan_y_bottom) {
                    if (sREdges[ei].y0 != sREdges[ei].y1) {
                        NkRActive* z = pool_alloc();
                        if (z) {
                            nkft_float32 dxdy = (sREdges[ei].x1 - sREdges[ei].x0) /
                                                (sREdges[ei].y1 - sREdges[ei].y0);
                            z->fdx = dxdy;
                            z->fdy = (dxdy != 0.f) ? (1.f / dxdy) : 0.f;
                            z->fx  = sREdges[ei].x0 + dxdy * (scan_y_top - sREdges[ei].y0);
                            z->fx -= (nkft_float32)off_x;
                            z->direction = sREdges[ei].invert ? 1.f : -1.f;
                            z->sy = sREdges[ei].y0;
                            z->ey = sREdges[ei].y1;
                            if (j == 0 && off_y != 0 && z->ey < scan_y_top) z->ey = scan_y_top;
                            z->next = active;
                            active  = z;
                        }
                    }
                    ++ei;
                }

                // Calcul des contributions
                if (active)
                    FillActiveEdgesNew(sScanline, sScanline2 + 1, W, active, scan_y_top);

                // Intégration (local + fill) → pixels
                {
                    nkft_float32 sum = 0.f;
                    nkft_uint8* row = out + j * stride;
                    for (nkft_int32 i = 0; i < W; ++i) {
                        nkft_float32 k;
                        nkft_int32 m;
                        sum += sScanline2[i];
                        k = sScanline[i] + sum;
                        k = (k < 0.f ? -k : k) * 255.f + 0.5f;
                        m = (nkft_int32)k;
                        if (m > 255) m = 255;
                        row[i] = (nkft_uint8)m;
                    }
                }

                // Avance des arêtes actives (fx += fdx)
                {
                    NkRActive** step = &active;
                    while (*step) {
                        NkRActive* z = *step;
                        z->fx += z->fdx;
                        step = &((*step)->next);
                    }
                }

                ++j;
            }
        }

        // ============================================================
        // API PUBLIQUE
        // ============================================================

        void MakeGlyphBitmapExact(const NkFontFaceInfo* info,
                                nkft_uint8*  output,
                                nkft_int32   outW,    nkft_int32   outH,    nkft_int32   outStride,
                                nkft_float32 scaleX,  nkft_float32 scaleY,
                                nkft_float32 shiftX,  nkft_float32 shiftY,
                                NkGlyphId    glyph) {
            if (!info || !output || outW <= 0 || outH <= 0) return;

            for (nkft_int32 r = 0; r < outH; ++r)
                memset(output + r * outStride, 0, (nkft_size)outW);

            NkFontVertexBuffer vb;
            if (!GetGlyphShape(info, glyph, &vb) || vb.count == 0) return;

            nkft_int32 bx0, by0, bx1, by1;
            GetGlyphBitmapBox(info, glyph, scaleX, scaleY, shiftX, shiftY,
                            &bx0, &by0, &bx1, &by1);

            BuildEdges(&vb, scaleX, scaleY,
                    shiftX - (nkft_float32)bx0,
                    shiftY - (nkft_float32)by0);

            if (sRNum == 0) return;

            SortEdges();

            if (sRNum < NK_RE_MAX_EDGES) {
                sREdges[sRNum].y0 = 1e10f;
                sREdges[sRNum].y1 = 1e10f;
            }

            DoRasterize(output, outW, outH, outStride, 0, 0);
        }

        void MakeGlyphBitmap(const NkFontFaceInfo* info,
                            nkft_uint8*  output,
                            nkft_int32   outW,    nkft_int32   outH,    nkft_int32   outStride,
                            nkft_float32 scaleX,  nkft_float32 scaleY,
                            nkft_float32 shiftX,  nkft_float32 shiftY,
                            NkGlyphId    glyph) {
            MakeGlyphBitmapExact(info, output, outW, outH, outStride,
                                scaleX, scaleY, shiftX, shiftY, glyph);
        }

    } // namespace nkfont
} // namespace nkentseu
*/