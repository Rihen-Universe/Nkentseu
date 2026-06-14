#include "pch.h"
// =============================================================================
// NkSculptStroke.cpp  — NKRenderer v5.0  (Tools/PixolSculpt/)
// Logique CPU complete : interpolation des dabs le long du trace + dirty rect
// (working set borne). Aucune dependance GPU ici.
// =============================================================================
#include "NKRenderer/Tools/PixolSculpt/NkSculptStroke.h"
#include <cmath>

namespace nkentseu {
    namespace renderer {

        static constexpr uint32 kMaxStrokeDabs = 256;

        void NkSculptStroke::Begin(const NkSculptBrush& brush) noexcept {
            mBrush   = brush;
            mActive  = true;
            mHasLast = false;
            mPending.Clear();
            mDirty = NkSculptRect{};
        }

        void NkSculptStroke::AddSample(const NkVec2f& screenPos, float32 pressure) noexcept {
            if (!mActive) return;
            pressure = (pressure < 0.f) ? 0.f : (pressure > 1.f ? 1.f : pressure);

            const float32 spacing = (mBrush.radiusPx * 0.25f > 1.f)
                                   ? mBrush.radiusPx * 0.25f : 1.f;

            auto emit = [this](float32 x, float32 y, float32 pr) {
                if (mPending.Size() >= kMaxStrokeDabs) return;
                NkSculptDab d;
                d.screenPos = NkVec2f{ x, y };
                d.radiusPx  = mBrush.radiusPx;
                d.pressure  = pr;
                mPending.PushBack(d);
                ExpandDirty(d);
            };

            if (!mHasLast) {
                emit(screenPos.x, screenPos.y, pressure);
                mLastSample = screenPos;
                mHasLast    = true;
                return;
            }

            const float32 dx = screenPos.x - mLastSample.x;
            const float32 dy = screenPos.y - mLastSample.y;
            const float32 dist = std::sqrt(dx*dx + dy*dy);
            if (dist < spacing) return;

            const float32 inv = 1.f / dist;
            for (float32 t = spacing; t <= dist; t += spacing)
                emit(mLastSample.x + dx * inv * t, mLastSample.y + dy * inv * t, pressure);
            mLastSample = screenPos;
        }

        void NkSculptStroke::End() noexcept {
            mActive  = false;
            mHasLast = false;
        }

        void NkSculptStroke::ClearPending() noexcept {
            mPending.Clear();
            mDirty = NkSculptRect{};
        }

        void NkSculptStroke::Reset() noexcept {
            mPending.Clear();
            mDirty   = NkSculptRect{};
            mActive  = false;
            mHasLast = false;
        }

        void NkSculptStroke::ExpandDirty(const NkSculptDab& dab) noexcept {
            const int32 ts = (int32)kNkSculptTileSize;

            int32 minx = (int32)std::floor(dab.screenPos.x - dab.radiusPx);
            int32 miny = (int32)std::floor(dab.screenPos.y - dab.radiusPx);
            int32 maxx = (int32)std::ceil (dab.screenPos.x + dab.radiusPx);
            int32 maxy = (int32)std::ceil (dab.screenPos.y + dab.radiusPx);

            auto floorTile = [ts](int32 v) { int32 q = v / ts; if (v < 0 && v % ts != 0) --q; return q * ts; };
            auto ceilTile  = [ts](int32 v) { int32 q = v / ts; if (v > 0 && v % ts != 0) ++q; return q * ts; };
            minx = floorTile(minx); miny = floorTile(miny);
            maxx = ceilTile(maxx);  maxy = ceilTile(maxy);
            if (minx < 0) minx = 0; if (miny < 0) miny = 0;

            if (mDirty.IsEmpty()) {
                mDirty.x = minx; mDirty.y = miny;
                mDirty.w = maxx - minx; mDirty.h = maxy - miny;
                return;
            }
            // Union (rect stocke en x/y/w/h -> on passe par min/max).
            int32 curMinX = mDirty.x, curMinY = mDirty.y;
            int32 curMaxX = mDirty.x + mDirty.w, curMaxY = mDirty.y + mDirty.h;
            if (minx < curMinX) curMinX = minx;
            if (miny < curMinY) curMinY = miny;
            if (maxx > curMaxX) curMaxX = maxx;
            if (maxy > curMaxY) curMaxY = maxy;
            mDirty.x = curMinX; mDirty.y = curMinY;
            mDirty.w = curMaxX - curMinX; mDirty.h = curMaxY - curMinY;
        }

    } // namespace renderer
} // namespace nkentseu
