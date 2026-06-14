// =============================================================================
// NkVoxelAOSystem.cpp — NKRenderer v5.0
// =============================================================================
#include "NkVoxelAOSystem.h"
#include "NKLogger/NkLog.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace renderer {

        NkVoxelAOSystem::~NkVoxelAOSystem() { Shutdown(); }

        bool NkVoxelAOSystem::Init(NkIDevice* device, const NkVoxelAOConfig& cfg) {
            mDevice = device;
            mCfg    = cfg;
            if (!mDevice) return false;

            // Crée la texture 3D RGBA8_UNORM (4 bytes / voxel). R = opacity,
            // G/B/A inutilisés. RGBA8 est universellement supporté en sample 3D
            // sur tous les GPU desktop/mobile (vs R8_UNORM 3D qui requiert
            // VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT specifique). Coût mémoire :
            // 64x32x64*4 = 512KB (acceptable).
            NkTextureDesc td = NkTextureDesc::Tex3D(mCfg.resX, mCfg.resY, mCfg.resZ,
                                                     NkGPUFormat::NK_RGBA8_UNORM);
            td.debugName = "VoxelAO";
            mVoxelTex = mDevice->CreateTexture(td);
            if (!mVoxelTex.IsValid()) {
                logger.Errorf("[NkVoxelAOSystem] CreateTexture 3D R8 echec (%ux%ux%u)\n",
                              mCfg.resX, mCfg.resY, mCfg.resZ);
                return false;
            }

            // Sampler linear clamp pour cone tracing (interpolation entre voxels).
            mVoxelSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());

            // Init clear : remplit le grid avec des zeros (pas d'AO par defaut)
            // afin que le sample retourne 0 et le PBR shader ne soit pas perturbe
            // tant que l'app n'a pas appele Build() avec ses occluders.
            // RGBA8 -> 4 bytes par voxel.
            const uint32 totalVoxels = mCfg.resX * mCfg.resY * mCfg.resZ;
            NkVector<uint8> zeros;
            zeros.Resize(totalVoxels * 4);
            memset(zeros.Data(), 0, totalVoxels * 4);
            mDevice->WriteTexture(mVoxelTex, zeros.Data());

            logger.Info("[NkVoxelAOSystem] Init OK : {0}x{1}x{2} R8 ({3} KB)\n",
                        mCfg.resX, mCfg.resY, mCfg.resZ,
                        (mCfg.resX * mCfg.resY * mCfg.resZ) / 1024);
            return true;
        }

        void NkVoxelAOSystem::Shutdown() {
            if (mVoxelSampler.IsValid()) {
                mDevice->DestroySampler(mVoxelSampler);
                mVoxelSampler = {};
            }
            if (mVoxelTex.IsValid()) {
                mDevice->DestroyTexture(mVoxelTex);
                mVoxelTex = {};
            }
            mOccluders.Clear();
            mDirty = true;
        }

        bool NkVoxelAOSystem::WorldToVoxelRange(const math::NkVec3f& mn, const math::NkVec3f& mx,
                                                  uint32& vMinX, uint32& vMinY, uint32& vMinZ,
                                                  uint32& vMaxX, uint32& vMaxY, uint32& vMaxZ) const {
            // Map world AABB -> voxel indices [0, res-1].
            math::NkVec3f size = mCfg.maxBounds - mCfg.minBounds;
            if (size.x <= 1e-5f || size.y <= 1e-5f || size.z <= 1e-5f) return false;

            auto toVoxel = [&](float32 worldComp, float32 minB, float32 sizeB, uint32 res) -> int32 {
                float32 t = (worldComp - minB) / sizeB;
                return (int32)std::floor(t * (float32)res);
            };

            int32 ix0 = toVoxel(mn.x, mCfg.minBounds.x, size.x, mCfg.resX);
            int32 iy0 = toVoxel(mn.y, mCfg.minBounds.y, size.y, mCfg.resY);
            int32 iz0 = toVoxel(mn.z, mCfg.minBounds.z, size.z, mCfg.resZ);
            int32 ix1 = toVoxel(mx.x, mCfg.minBounds.x, size.x, mCfg.resX);
            int32 iy1 = toVoxel(mx.y, mCfg.minBounds.y, size.y, mCfg.resY);
            int32 iz1 = toVoxel(mx.z, mCfg.minBounds.z, size.z, mCfg.resZ);

            // Clamp aux bornes du voxel grid + reject si totalement hors grid.
            if (ix1 < 0 || iy1 < 0 || iz1 < 0) return false;
            if (ix0 >= (int32)mCfg.resX || iy0 >= (int32)mCfg.resY || iz0 >= (int32)mCfg.resZ) return false;

            vMinX = (uint32)NkMax(0, ix0);
            vMinY = (uint32)NkMax(0, iy0);
            vMinZ = (uint32)NkMax(0, iz0);
            vMaxX = (uint32)NkMin((int32)mCfg.resX - 1, ix1);
            vMaxY = (uint32)NkMin((int32)mCfg.resY - 1, iy1);
            vMaxZ = (uint32)NkMin((int32)mCfg.resZ - 1, iz1);
            return true;
        }

        bool NkVoxelAOSystem::Build() {
            if (!mDevice || !mVoxelTex.IsValid()) return false;
            // RGBA8 -> 4 bytes par voxel. On ecrit l'opacity dans R (offset 0),
            // les autres canaux restent a 0.
            const uint32 totalVoxels = mCfg.resX * mCfg.resY * mCfg.resZ;
            NkVector<uint8> grid;
            grid.Resize(totalVoxels * 4);
            memset(grid.Data(), 0, totalVoxels * 4);

            if (mOccluders.Empty()) {
                logger.Warnf("[NkVoxelAOSystem] Build : aucun occluder enregistre\n");
                mDevice->WriteTexture(mVoxelTex, grid.Data());
                mDirty = false;
                return true;
            }

            // CPU bake : pour chaque occluder, marque les voxels intersected
            // avec opacity max (un voxel touché par 2 occluders = max des deux).
            uint32 voxelsMarked = 0;
            for (uint32 oi = 0; oi < mOccluders.Size(); oi++) {
                const auto& occ = mOccluders[oi];
                uint32 vx0, vy0, vz0, vx1, vy1, vz1;
                if (!WorldToVoxelRange(occ.minWorld, occ.maxWorld,
                                       vx0, vy0, vz0, vx1, vy1, vz1)) continue;

                uint8 op = (uint8)NkClamp(occ.opacity * 255.f, 0.f, 255.f);
                for (uint32 z = vz0; z <= vz1; z++) {
                    for (uint32 y = vy0; y <= vy1; y++) {
                        uint32 rowStart = (z * mCfg.resY + y) * mCfg.resX;
                        for (uint32 x = vx0; x <= vx1; x++) {
                            uint32 idx = (rowStart + x) * 4;   // RGBA = 4 bytes
                            if (op > grid[idx]) {
                                if (grid[idx] == 0) voxelsMarked++;
                                grid[idx] = op;
                            }
                        }
                    }
                }
            }

            mDevice->WriteTexture(mVoxelTex, grid.Data());
            mDirty = false;

            logger.Info("[NkVoxelAOSystem] Build OK : {0} occluders -> {1} voxels marques (sur {2})\n",
                        mOccluders.Size(), voxelsMarked, totalVoxels);
            return true;
        }

    } // namespace renderer
} // namespace nkentseu
