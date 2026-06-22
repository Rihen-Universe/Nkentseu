// =============================================================================
// Games/Common/MouPlant.cpp
// =============================================================================
#include "Games/Common/MouPlant.h"
#include "Games/Common/MouFrame.h"

namespace mou {

    using namespace nkentseu;

    void MouPlant::Draw(const MouFrame& f, uint32 tex, float32 aspectWH,
                        float32 x, float32 y, float32 w, float32 h,
                        float32& outX, float32& outY, float32& outW, float32& outH) noexcept {
        float32 dw = w, dh = (aspectWH > 0.0001f) ? w / aspectWH : h;
        if (dh > h) { dh = h; dw = h * aspectWH; }
        const float32 dx = x + (w - dw) * 0.5f;
        const float32 dy = y + (h - dh);        // ancré EN BAS : le tronc touche le sol
        if (tex) f.Image(tex, dx, dy, dw, dh);
        outX = dx; outY = dy; outW = dw; outH = dh;
    }

    void MouPlant::Slots(float32 rx, float32 ry, float32 rw, float32 rh,
                         float32 canopyCYr, float32 canopyBWr, float32 canopyBHr,
                         int32 n, float32 fruitSz,
                         float32* outX, float32* outY) noexcept {
        if (n <= 0) return;
        const float32 ccx = rx + rw * 0.5f;
        const float32 ccy = ry + rh * canopyCYr;
        const float32 bw  = rw * canopyBWr;
        const float32 bh  = rh * canopyBHr;
        const int32 cols = (n <= 2) ? n : (n <= 6 ? 2 : 3);
        const int32 rows = (n + cols - 1) / cols;
        for (int32 i = 0; i < n; ++i) {
            const int32 r = i / cols, c = i % cols;
            const int32 inRow = (r == rows - 1 && (n % cols) != 0) ? (n % cols) : cols;
            const float32 cellW = bw / (float32)inRow;
            const float32 cellH = bh / (float32)rows;
            outX[i] = ccx - bw * 0.5f + (c + 0.5f) * cellW - fruitSz * 0.5f;
            outY[i] = ccy - bh * 0.5f + (r + 0.5f) * cellH - fruitSz * 0.5f;
        }
    }

}  // namespace mou
