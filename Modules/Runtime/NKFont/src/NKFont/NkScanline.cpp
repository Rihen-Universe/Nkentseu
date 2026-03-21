/**
 * @File    NkScanline.cpp
 * @Brief   Rasterizer scanline — AA 4×4 super-sampling complet.
 * @Author  TEUGUIA TADJUIDJE Rodolf Séderis
 * @License Apache-2.0
 *
 * @Correctness
 *  Super-sampling 4×4 : 16 sous-pixels par pixel final.
 *  Pour chaque pixel (px, py), on évalue 4 positions Y (py+1/8, py+3/8, py+5/8, py+7/8)
 *  et 4 positions X (px+1/8, px+3/8, px+5/8, px+7/8).
 *  La couverture = nombre de sous-pixels intérieurs / 16, mappée sur [0,255].
 *
 *  Anti-aliasing sub-pixel sur les bords gauche et droit de chaque span :
 *  les fractions F26Dot6 des arêtes donnent la couverture partielle exacte.
 *
 *  Winding rules NonZero et EvenOdd correctes.
 *  AEL tri insertion (O(n²) mais n ≤ 64 en pratique).
 */
#include "pch.h"
#include "NKFont/NkScanline.h"
#include <cstring>
namespace nkentseu {

// ─────────────────────────────────────────────────────────────────────────────
//  SortAEL
// ─────────────────────────────────────────────────────────────────────────────

void NkScanline::SortAEL(ActiveEdge* ael, int32 count) noexcept {
    for(int32 i=1;i<count;++i){
        ActiveEdge key=ael[i]; int32 j=i-1;
        while(j>=0&&ael[j].xCurrent.raw>key.xCurrent.raw){ael[j+1]=ael[j];--j;}
        ael[j+1]=key;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  UpdateAEL
// ─────────────────────────────────────────────────────────────────────────────

int32 NkScanline::UpdateAEL(ActiveEdge* ael, int32 count, int32 nextY) noexcept {
    int32 out=0;
    for(int32 i=0;i<count;++i)
        if(ael[i].yEnd>nextY) ael[out++]=ael[i];
    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AA 4×4 : calcule la couverture d'un pixel en sampant 16 points
//
//  subY ∈ {0,1,2,3} → Y du sous-pixel = py + (2*subY+1)/8
//  Logique :
//    Pour chaque sous-scanline (subY) on a l'AEL avancée à la bonne position.
//    On calcule les spans et on teste les 4 sous-pixels horizontaux.
//    coverBuffer[x] += nombre de sous-pixels horizontaux couverts à cette sous-Y.
//    Après les 4 sous-scanlines, coverBuffer[x] ∈ [0,16].
//    Valeur finale = coverBuffer[x] * 255 / 16.
// ─────────────────────────────────────────────────────────────────────────────

void NkScanline::RasterizeScanline(
    ActiveEdge*   ael,
    int32         aelCount,
    int32         /*y*/,
    int32         /*subY*/,
    int32*        coverBuffer,
    int32         bufWidth,
    int32         offsetX,
    NkWindingRule rule) noexcept
{
    SortAEL(ael, aelCount);

    int32 winding = 0;
    for(int32 i=0; i+1<aelCount; i+=2){
        const int32 w0 = ael[i].winding;
        winding += w0;

        bool filled = false;
        if     (rule==NkWindingRule::NonZero) filled=(winding!=0);
        else if(rule==NkWindingRule::EvenOdd) filled=(winding&1)!=0;

        if(!filled){ winding+=ael[i+1].winding; continue; }

        // Positions sub-pixel en F26Dot6 (déjà avancées à la bonne sous-Y)
        const F26Dot6 xLeft  = ael[i].xCurrent;
        const F26Dot6 xRight = ael[i+1].xCurrent;

        // Coordonnées entières dans l'espace bitmap
        const int32 xL = xLeft.Floor()  - offsetX;
        const int32 xR = xRight.Floor() - offsetX;

        // Sous-pixels horizontaux : positions 1/8, 3/8, 5/8, 7/8 du pixel
        // Exprimées en F26Dot6 : 0.125, 0.375, 0.625, 0.875 → ×64 = 8, 24, 40, 56
        static const int32 kSubX[4] = { 8, 24, 40, 56 }; // en unités F26Dot6

        // Plage de pixels touchés (pixel entier)
        const int32 pxStart = xL     < 0        ? 0        : xL;
        const int32 pxEnd   = xRight.Ceil()-offsetX > bufWidth ? bufWidth : xRight.Ceil()-offsetX;

        for(int32 px=pxStart; px<pxEnd && px<bufWidth; ++px){
            // Compte combien des 4 sous-pixels horizontaux sont à l'intérieur du span
            int32 hits = 0;
            for(int32 sx=0; sx<4; ++sx){
                // Position sub-pixel : px + subX/64 en unités F26Dot6
                const int32 subPxF26 = (px + offsetX) * F26Dot6::ONE + kSubX[sx];
                if(subPxF26 > xLeft.raw && subPxF26 < xRight.raw) ++hits;
            }
            if(hits > 0) coverBuffer[px] += hits;
        }

        winding += ael[i+1].winding;
    }

    // Avance les arêtes d'une sous-scanline (1/SUBPIXELS de pixel)
    for(int32 i=0; i<aelCount; ++i)
        ael[i].xCurrent += F26Dot6::FromRaw(ael[i].xStep.raw / SUBPIXELS);
}

// ─────────────────────────────────────────────────────────────────────────────
//  FlushCoverage — coverBuffer[x] ∈ [0,16] → Gray8 [0,255]
// ─────────────────────────────────────────────────────────────────────────────

void NkScanline::FlushCoverage(
    int32* coverBuffer, int32 bufWidth,
    uint8* rowPixels,   int32 rowWidth) noexcept
{
    const int32 w = bufWidth < rowWidth ? bufWidth : rowWidth;
    for(int32 x=0; x<w; ++x){
        int32 cov = (coverBuffer[x] * 255 + 8) / 16; // 16 = SUBPIXELS×4
        if(cov < 0)   cov = 0;
        if(cov > 255) cov = 255;
        // Accumulation additive (glyphes composites)
        const int32 combined = rowPixels[x] + cov;
        rowPixels[x] = static_cast<uint8>(combined > 255 ? 255 : combined);
        coverBuffer[x] = 0;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Rasterize
// ─────────────────────────────────────────────────────────────────────────────

void NkScanline::Rasterize(
    const NkEdgeList& edgeList,
    NkBitmap&         bitmap,
    NkMemArena&       arena,
    NkWindingRule     rule,
    int32             offsetX,
    int32             offsetY) noexcept
{
    if(!bitmap.IsValid()||edgeList.numEdges==0) return;

    NkScratchArena scratch(arena);

    ActiveEdge* ael        = arena.Alloc<ActiveEdge>(MAX_ACTIVE_EDGES);
    int32*      coverBuffer= arena.Alloc<int32>(bitmap.width + 4);
    if(!ael||!coverBuffer) return;

    ::memset(coverBuffer, 0, sizeof(int32)*(bitmap.width+4));

    int32  aelCount = 0;
    uint32 edgeIdx  = 0;

    const int32 yStart = edgeList.yMin;
    const int32 yEnd   = edgeList.yMax;

    for(int32 y=yStart; y<yEnd; ++y){
        const int32 bitmapY = y - offsetY;

        // Ajoute les nouvelles arêtes
        while(edgeIdx < edgeList.numEdges){
            const NkEdge& e = edgeList.edges[edgeIdx];
            if(e.y0 > y) break;
            if(aelCount < MAX_ACTIVE_EDGES){
                ActiveEdge& ae = ael[aelCount++];
                ae.xCurrent = e.xCurrent;
                ae.xStep    = e.xStep;
                ae.yEnd     = e.y1;
                ae.winding  = e.winding;
            }
            ++edgeIdx;
        }
        if(aelCount == 0) continue;

        // 4 sous-scanlines
        ::memset(coverBuffer, 0, sizeof(int32)*(bitmap.width+4));
        for(int32 sub=0; sub<SUBPIXELS; ++sub)
            RasterizeScanline(ael, aelCount, y, sub,
                              coverBuffer, bitmap.width+1, offsetX, rule);

        if(bitmapY >= 0 && bitmapY < bitmap.height)
            FlushCoverage(coverBuffer, bitmap.width+1,
                          bitmap.RowPtr(bitmapY), bitmap.width);
        else
            ::memset(coverBuffer, 0, sizeof(int32)*(bitmap.width+4));

        aelCount = UpdateAEL(ael, aelCount, y+1);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RasterizeOutline — pipeline complet
// ─────────────────────────────────────────────────────────────────────────────

bool NkScanline::RasterizeOutline(
    const NkOutline& outline,
    NkBitmap&        outBitmap,
    NkMemArena&      arena,
    NkWindingRule    rule,
    int32            tolerance) noexcept
{
    if(outline.numPoints==0) return true;
    NkScratchArena scratch(arena);
    NkEdgeList edges;
    if(!outline.Flatten(edges, arena, tolerance)) return false;
    if(edges.numEdges==0) return true;
    const int32 bmpW = outline.xMax.Ceil() - outline.xMin.Floor() + 1;
    const int32 bmpH = outline.yMax.Ceil() - outline.yMin.Floor() + 1;
    if(bmpW<=0||bmpH<=0) return true;
    outBitmap = NkBitmap::Alloc(arena, bmpW, bmpH);
    if(!outBitmap.IsValid()) return false;
    Rasterize(edges, outBitmap, arena, rule, outline.xMin.Floor(), outline.yMin.Floor());
    return true;
}

} // namespace nkentseu
