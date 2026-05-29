#pragma once
// =============================================================================
// NkVoxelAOSystem.h — NKRenderer v5.0 (Tools/VoxelAO/)
//
// Phase H.6 : Voxel-based Ambient Occlusion (UE5 Lumen-light approximé).
//
// Principe :
//   1. L'application enregistre des occluders (AABB world-space) via
//      RegisterOccluder(). Typiquement : le sol, les gros meshes static.
//   2. Build() voxelize les occluders en CPU dans une texture 3D R8_UNORM
//      (resolution kRes³, bounds kBounds).
//   3. Le PBR shader sample uVoxelOpacity (binding=27) et fait du
//      cone-tracing dans l'hémisphère normale pour calculer l'AO long-range.
//   4. AO multiplie l'IBL irradiance/specular dans le PBR shader → les
//      zones occluses par d'autres geometry (ex: sphère sous le sol) sont
//      assombries même si l'IBL ne sait pas qu'elles sont cachées.
//
// Limitation v0 :
//   - Voxelization static seulement (au boot, pas update per-frame)
//   - Bounds + resolution hardcodés pour MVP (à exposer en config plus tard)
//   - AABB-based voxelize (pas mesh-precise) — suffisant pour primitives
// =============================================================================
#include "NKRenderer/Core/NkRendererTypes.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKContainers/Sequential/NkVector.h"
#include "NKMath/NkVec.h"

namespace nkentseu {
    namespace renderer {

        // AABB occluder en world space. Opacity 0..1 (1 = totalement opaque).
        struct NkVoxelOccluder {
            math::NkVec3f minWorld;
            math::NkVec3f maxWorld;
            float32       opacity;     // 1.0 = opaque, 0.5 = semi-transparent
        };

        struct NkVoxelAOConfig {
            // Bounds world-space couverts par le voxel grid.
            math::NkVec3f minBounds = {-10.f, -5.f, -10.f};
            math::NkVec3f maxBounds = { 10.f,  5.f,  10.f};
            // Résolution : 64×32×64 = 131072 voxels = 128 KB R8.
            uint32 resX = 64;
            uint32 resY = 32;
            uint32 resZ = 64;
        };

        class NkVoxelAOSystem {
            public:
                NkVoxelAOSystem() = default;
                ~NkVoxelAOSystem();

                bool Init(NkIDevice* device, const NkVoxelAOConfig& cfg = {});
                void Shutdown();
                bool IsValid() const { return mVoxelTex.IsValid(); }

                // L'app appelle ça pour chaque occluder static (sol, gros mur,
                // gros obstacle). Doit être suivi de Build() pour voxelize +
                // upload GPU.
                void RegisterOccluder(const NkVoxelOccluder& occ) {
                    mOccluders.PushBack(occ);
                    mDirty = true;
                }
                // Helper : enregistre depuis une AABB sans préciser opacity (=1).
                void RegisterAABB(const math::NkVec3f& mn, const math::NkVec3f& mx,
                                  float32 opacity = 1.f) {
                    RegisterOccluder({mn, mx, opacity});
                }
                void Clear() {
                    mOccluders.Clear();
                    mDirty = true;
                }

                // Voxelize les occluders enregistrés en CPU et upload dans la
                // texture 3D R8_UNORM. À appeler une fois après tous les
                // RegisterOccluder() (typiquement à la fin du setup app).
                // Retourne false si pas d'occluders ou texture invalide.
                bool Build();

                // Accesseurs GPU pour le pipeline PBR.
                NkTextureHandle GetVoxelTexture() const { return mVoxelTex; }
                NkSamplerHandle GetVoxelSampler() const { return mVoxelSampler; }
                const NkVoxelAOConfig& GetConfig() const { return mCfg; }

            private:
                NkIDevice*       mDevice    = nullptr;
                NkVoxelAOConfig  mCfg;

                NkVector<NkVoxelOccluder> mOccluders;
                bool                       mDirty = true;

                NkTextureHandle  mVoxelTex;        // R8_UNORM, kRes³
                NkSamplerHandle  mVoxelSampler;    // linear clamp pour cone trace

                // Helper : convertit une AABB world space en range de voxels
                // (indices entiers inclusive). Retourne false si hors bounds.
                bool WorldToVoxelRange(const math::NkVec3f& mn, const math::NkVec3f& mx,
                                        uint32& vMinX, uint32& vMinY, uint32& vMinZ,
                                        uint32& vMaxX, uint32& vMaxY, uint32& vMaxZ) const;
        };

    } // namespace renderer
} // namespace nkentseu
