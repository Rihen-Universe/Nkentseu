#include "pch.h"
// =============================================================================
// NkVoxelStroke.cpp  — NKRenderer v5.0  (Tools/Voxel/)
// Logique CPU complete : interpolation des dabs le long du trace + dirty box
// (working set borne). Aucune dependance GPU ici.
// =============================================================================
#include "NKRenderer/Tools/Voxel/NkVoxelStroke.h"
#include <cmath>

namespace nkentseu {
    namespace renderer {

        // Borne dure de securite (le systeme applique aussi maxDabsPerFrame).
        static constexpr uint32 kMaxStrokeDabs = 256;

        void NkVoxelStroke::Begin(const NkVoxelBrush& brush) noexcept {
            mBrush   = brush;
            mActive  = true;
            mHasLast = false;
            mPending.Clear();
            mDirty = NkVoxelBox{};
        }

        void NkVoxelStroke::AddSample(const NkVec3f& centerVox, float32 pressure) noexcept {
            if (!mActive) return;
            pressure = (pressure < 0.f) ? 0.f : (pressure > 1.f ? 1.f : pressure);

            // Espacement des tampons : une fraction du rayon (>= 1 voxel) pour
            // eviter les trous a grande vitesse de deplacement.
            const float32 spacing = (mBrush.radiusVox * 0.35f > 1.f)
                                   ? mBrush.radiusVox * 0.35f : 1.f;

            // Depose un dab et etend la dirty box (capee par kMaxStrokeDabs).
            auto emit = [this](float32 x, float32 y, float32 z, float32 pr) {
                if (mPending.Size() >= kMaxStrokeDabs) return;
                NkVoxelDab d;
                d.centerVox = NkVec3f{ x, y, z };
                d.radiusVox = mBrush.radiusVox;
                d.pressure  = pr;
                mPending.PushBack(d);
                ExpandDirty(d);
            };

            if (!mHasLast) {
                emit(centerVox.x, centerVox.y, centerVox.z, pressure);
                mLastSample = centerVox;
                mHasLast    = true;
                return;
            }

            const float32 dx = centerVox.x - mLastSample.x;
            const float32 dy = centerVox.y - mLastSample.y;
            const float32 dz = centerVox.z - mLastSample.z;
            const float32 dist = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (dist < spacing) return; // trop proche : on attend le prochain echantillon

            const float32 inv = 1.f / dist;
            for (float32 t = spacing; t <= dist; t += spacing) {
                emit(mLastSample.x + dx * inv * t,
                     mLastSample.y + dy * inv * t,
                     mLastSample.z + dz * inv * t,
                     pressure);
            }
            mLastSample = centerVox;
        }

        void NkVoxelStroke::End() noexcept {
            mActive  = false;
            mHasLast = false;
        }

        void NkVoxelStroke::ClearPending() noexcept {
            mPending.Clear();
            mDirty = NkVoxelBox{};
        }

        void NkVoxelStroke::Reset() noexcept {
            mPending.Clear();
            mDirty   = NkVoxelBox{};
            mActive  = false;
            mHasLast = false;
        }

        void NkVoxelStroke::ExpandDirty(const NkVoxelDab& dab) noexcept {
            const int32 ts = (int32)kNkVoxelTileSize;

            int32 minx = (int32)std::floor(dab.centerVox.x - dab.radiusVox);
            int32 miny = (int32)std::floor(dab.centerVox.y - dab.radiusVox);
            int32 minz = (int32)std::floor(dab.centerVox.z - dab.radiusVox);
            int32 maxx = (int32)std::ceil (dab.centerVox.x + dab.radiusVox);
            int32 maxy = (int32)std::ceil (dab.centerVox.y + dab.radiusVox);
            int32 maxz = (int32)std::ceil (dab.centerVox.z + dab.radiusVox);

            // Aligner sur la grille de tuiles (gestion correcte des negatifs).
            auto floorTile = [ts](int32 v) {
                int32 q = v / ts; if (v < 0 && v % ts != 0) --q; return q * ts;
            };
            auto ceilTile = [ts](int32 v) {
                int32 q = v / ts; if (v > 0 && v % ts != 0) ++q;
                else if (v < 0 && v % ts != 0) { /* deja arrondi vers 0 */ }
                return q * ts;
            };
            minx = floorTile(minx); miny = floorTile(miny); minz = floorTile(minz);
            maxx = ceilTile(maxx);  maxy = ceilTile(maxy);  maxz = ceilTile(maxz);
            if (minx < 0) minx = 0; if (miny < 0) miny = 0; if (minz < 0) minz = 0;
            // (clamp superieur aux dimensions de la grille : fait par le systeme au dispatch)

            if (mDirty.IsEmpty()) {
                mDirty = NkVoxelBox{ minx, miny, minz, maxx, maxy, maxz };
                return;
            }
            if (minx < mDirty.minX) mDirty.minX = minx;
            if (miny < mDirty.minY) mDirty.minY = miny;
            if (minz < mDirty.minZ) mDirty.minZ = minz;
            if (maxx > mDirty.maxX) mDirty.maxX = maxx;
            if (maxy > mDirty.maxY) mDirty.maxY = maxy;
            if (maxz > mDirty.maxZ) mDirty.maxZ = maxz;
        }

    } // namespace renderer
} // namespace nkentseu
