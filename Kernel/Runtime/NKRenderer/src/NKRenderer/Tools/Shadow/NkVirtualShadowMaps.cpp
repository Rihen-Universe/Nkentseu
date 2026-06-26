// =============================================================================
// NkVirtualShadowMaps.cpp — implementation (Init + AllocSlots + Upload + Render)
// =============================================================================
#include "NKRenderer/Tools/Shadow/NkVirtualShadowMaps.h"
#include "NKRenderer/Tools/Render3D/NkRender3D.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#include <algorithm>

namespace nkentseu {
    namespace renderer {

        // ---------------------------------------------------------------------
        // Struct GPU std140 du UBO Shadows.
        // Total size = 256 slots * 112B + 32*4B*2 + 32B = 28928 bytes.
        // Sous la limite 64KB Vulkan/desktop GL UBO.
        // ---------------------------------------------------------------------
        struct ShadowSlotGPU {
            NkMat4f shadowMatrix;     // 64
            NkVec4f tileUV;           // 16 (.xy=minUV, .zw=maxUV)
            NkVec4f lightPosOrDir;    // 16 (.xyz pos/dir, .w range/splitFar)
            NkVec4f packedIds;        // 16 (.x=lightIdx, .y=slotType, .z=subIdx, .w=0)
        };
        static_assert(sizeof(ShadowSlotGPU) == 112, "ShadowSlotGPU size mismatch");

        struct ShadowSlotsUBOBlock {
            ShadowSlotGPU slots[kMaxShadowSlots];     // 256 * 112 = 28672
            // Packed integer arrays (4 per vec4 for std140). Index par light : on
            // lit slots[firstSlotPerLight[i]] et iterate slotCountPerLight[i].
            NkVec4f firstSlotPerLight[kMaxLightsShadow / 4];  // 8 * 16 = 128
            NkVec4f slotCountPerLight[kMaxLightsShadow / 4];  // 8 * 16 = 128
            NkVec4f globalCfg;         // .x=numSlots .y=softShadowMode .zw=0
            NkVec4f biasParams;        // .x=shadowBias .y=normalBias .z=softness .w=0
        };

        // ---------------------------------------------------------------------
        // Init : alloue atlas + FBO + UBO + sampler + state initial.
        // ---------------------------------------------------------------------
        bool NkVirtualShadowMaps::Init(NkIDevice* d, NkMeshSystem* m,
                                        NkMaterialSystem* mat,
                                        const NkVirtualShadowMapsConfig& cfg,
                                        uint32 framesInFlight) {
            mDevice = d; mMesh = m; mMat = mat; mCfg = cfg;
            mFramesInFlight = framesInFlight > 0 ? framesInFlight : 3;
            if (mFramesInFlight > 16) mFramesInFlight = 16;
            if (!mDevice) return false;

            uint32 atlasSz = mCfg.atlasSize > 0 ? mCfg.atlasSize : 4096;
            mCfg.atlasSize = atlasSz;

            // Atlas depth texture D32_FLOAT.
            auto td = NkTextureDesc::DepthStencil(atlasSz, atlasSz,
                                                   NkGPUFormat::NK_D32_FLOAT);
            td.debugName = "VSMShadowAtlas";
            mAtlasRhi = mDevice->CreateTexture(td);

            // FBO depth-only.
            NkFramebufferDesc fbd;
            fbd.width           = atlasSz;
            fbd.height          = atlasSz;
            fbd.depthAttachment = mAtlasRhi;
            fbd.debugName       = "VSMShadowFB";
            mShadowFB = mDevice->CreateFramebuffer(fbd);
            if (mShadowFB.IsValid()) {
                mShadowRP = mDevice->GetFramebufferRenderPass(mShadowFB);
            }

            // Samplers : compare-mode pour PCF, raw pour PCSS blocker search.
            mShadowSampler    = mDevice->CreateSampler(NkSamplerDesc::Shadow());
            mShadowRawSampler = mDevice->CreateSampler(NkSamplerDesc::Clamp());

            // UBO Shadows : ring de mFramesInFlight buffers pour eviter le data
            // hazard (CPU ecrit pendant que GPU lit la frame precedente).
            mUBOSlotsRing.Resize(mFramesInFlight);
            for (uint32 i = 0; i < mFramesInFlight; i++) {
                mUBOSlotsRing[i] = mDevice->CreateBuffer(
                    NkBufferDesc::Uniform(sizeof(ShadowSlotsUBOBlock)));
            }
            mCurFrameSlot = 0;

            // Init state : pas de slots actifs.
            mActiveSlotCount = 0;
            for (uint32 i = 0; i < kMaxLightsShadow; i++) {
                mFirstSlotPerLight[i] = -1;
                mSlotCountPerLight[i] = 0;
            }
            mPacker.Reset(atlasSz, atlasSz);
            UploadSlotsUBO();

            return mAtlasRhi.IsValid() && !mUBOSlotsRing.IsEmpty();
        }

        void NkVirtualShadowMaps::Shutdown() {
            if (mShadowFB.IsValid())          { mDevice->DestroyFramebuffer(mShadowFB); mShadowFB={}; }
            for (auto& ubo : mUBOSlotsRing) {
                if (ubo.IsValid()) mDevice->DestroyBuffer(ubo);
            }
            mUBOSlotsRing.Clear();
            if (mShadowSampler.IsValid())     { mDevice->DestroySampler(mShadowSampler); mShadowSampler={}; }
            if (mShadowRawSampler.IsValid())  { mDevice->DestroySampler(mShadowRawSampler); mShadowRawSampler={}; }
            if (mAtlasRhi.IsValid())          { mDevice->DestroyTexture(mAtlasRhi); mAtlasRhi={}; }
        }

        // ---------------------------------------------------------------------
        // Correction de profondeur NDC [-1,1] (GL) -> [0,1] (VK/DX) sur la matrice
        // d'ombre, identique a NkRender3D pour la camera principale. Sans elle,
        // l'atlas d'ombre se rend tronque sur DX12 -> scene entierement en ombre.
        // ---------------------------------------------------------------------
        bool NkVirtualShadowMaps::DepthIsZeroToOne() const {
            if (!mDevice) return false;
            auto api = mDevice->GetApi();
            return api == ::nkentseu::NkGraphicsApi::NK_GFX_API_VULKAN
                || api == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX11
                || api == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX12;
        }
        void NkVirtualShadowMaps::ApplyDepthClipCorrection(NkMat4f& m) const {
            if (!DepthIsZeroToOne()) return;
            // clipZ01 : z_new = 0.5*z + 0.5*w. NkMat4f column-major -> [2][2]=0.5, [3][2]=0.5.
            NkMat4f clipZ01 = NkMat4f::Identity();
            clipZ01[2][2] = 0.5f;
            clipZ01[3][2] = 0.5f;
            m = clipZ01 * m;
        }

        // ---------------------------------------------------------------------
        // ComputeCascadeSplits : Practical CSM (log+uniform blend).
        // ---------------------------------------------------------------------
        void NkVirtualShadowMaps::ComputeCascadeSplits(float32 nearP, float32 farP,
                                                        float32 lambda, uint32 N,
                                                        float32 out[kMaxCascades]) const {
            if (farP <= nearP) farP = nearP + 1.f;
            float32 ratio = farP / nearP;
            for (uint32 c = 0; c < N; c++) {
                float32 si  = float32(c + 1) / float32(N);
                float32 logS = nearP * std::pow(ratio, si);
                float32 uniS = nearP + (farP - nearP) * si;
                out[c] = lambda * logS + (1.f - lambda) * uniS;
            }
        }

        // ---------------------------------------------------------------------
        // Helper interne : compute frustum corners en world space (sub-range).
        // Copie de NkShadowSystem::ComputeSubFrustumCornersWS.
        // ---------------------------------------------------------------------
        static void ComputeSubFrustumCornersWS(const NkCamera3D& cam,
                                                float32 zNear, float32 zFar,
                                                NkVec3f outCorners[8]) {
            NkMat4f invVP = cam.GetViewProj().Inverse();
            const float32 kz0 = -1.f, kz1 = 1.f;
            NkVec3f rayNear[4], rayFar[4];
            for (int i = 0; i < 4; i++) {
                float32 nx = (i == 1 || i == 2) ? 1.f : -1.f;
                float32 ny = (i >= 2) ? 1.f : -1.f;
                NkVec4f cn = invVP * NkVec4f{nx, ny, kz0, 1.f};
                NkVec4f cf = invVP * NkVec4f{nx, ny, kz1, 1.f};
                rayNear[i] = NkVec3f{cn.x/cn.w, cn.y/cn.w, cn.z/cn.w};
                rayFar[i]  = NkVec3f{cf.x/cf.w, cf.y/cf.w, cf.z/cf.w};
            }
            float32 camNear = cam.GetNear(), camFar = cam.GetFar();
            float32 aNear = (zNear - camNear) / (camFar - camNear);
            float32 aFar  = (zFar  - camNear) / (camFar - camNear);
            for (int i = 0; i < 4; i++) {
                NkVec3f dir = rayFar[i] - rayNear[i];
                outCorners[i]     = rayNear[i] + dir * aNear;
                outCorners[i + 4] = rayNear[i] + dir * aFar;
            }
        }

        // ---------------------------------------------------------------------
        // ComputeDirectionalCascade : sphere-fit + texel-snap (stable shadows).
        // Reprend la logique de NkShadowSystem.cpp lignes 240-304.
        // ---------------------------------------------------------------------
        void NkVirtualShadowMaps::ComputeDirectionalCascade(
                const NkCamera3D& mainCam, const NkLightDesc& light,
                uint32 cascadeIdx, float32 splitNear, float32 splitFar,
                uint32 tilePx,
                NkMat4f& outView, NkMat4f& outProj) const {
            (void)splitNear; (void)splitFar;

            // V0 stable : mode FIXE radius par cascade. Le sphere-fit donne une
            // qualite adaptive mais introduit du shadow swimming car le centre
            // ET le radius varient d'un epsilon a chaque frame (cam mobile).
            // En mode FIXE, le radius est constant -> texelSize constant -> snap
            // 100% stable -> pas de clignotement.
            // Centre = position cam (suit la cam donc shadow couvre toujours
            // l'environnement immediat). XY snap stabilise le centre.
            NkVec3f center;
            float32 radius;
            if (mCfg.useFixedCascadeRadius && cascadeIdx < kMaxCascades) {
                // Center : soit ancre au monde (anti-swimming total pour une scene
                // close couverte par une seule grande cascade), soit suivant la
                // camera (mondes ouverts). Le snap-to-texel ci-dessous quantifie
                // ensuite le centre ; avec un center fixe il reste donc constant
                // -> l'ombre d'un caster fixe ne glisse plus.
                center = mCfg.useFixedCascadeCenter ? mCfg.cascadeWorldCenter
                                                    : mainCam.GetPosition();
                radius = mCfg.cascadeFixedRadius[cascadeIdx];
                if (radius < 1.f) radius = 1.f;
            } else {
                // Fallback sphere-fit du sub-frustum (V0 instable, V1 todo).
                NkVec3f corners[8];
                ComputeSubFrustumCornersWS(mainCam, splitNear, splitFar, corners);
                center = NkVec3f{0,0,0};
                for (int i = 0; i < 8; i++) center = center + corners[i];
                center = center * (1.f / 8.f);
                radius = 0.f;
                for (int i = 0; i < 8; i++) {
                    NkVec3f d = corners[i] - center;
                    float32 r = std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z);
                    if (r > radius) radius = r;
                }
                radius *= 1.2f;
                if (radius < 1.f) radius = 1.f;
                const float32 kQuantum = 0.5f;
                radius = std::ceil(radius / kQuantum) * kQuantum;
            }

            // Stable shadows : texel-snap du centre en light-space.
            NkVec3f lightDir = light.direction.Normalized();
            NkVec3f forward  = lightDir;
            NkVec3f right;
            NkVec3f upL;
            if (forward.y * forward.y > 0.9025f) {
                right = NkVec3f{1, 0, 0};
            } else {
                NkVec3f wUp = NkVec3f{0, 1, 0};
                right = wUp.Cross(forward).Normalized();
            }
            upL   = forward.Cross(right).Normalized();
            right = upL.Cross(forward).Normalized();

            if (mCfg.stable) {
                // Texel snap aggressif : snap center.xy en light-space au texel
                // grid. ET snap z aussi pour stabilite totale. Sans le snap z
                // le centre peut bouger le long de l'axe lumiere et changer la
                // proj depth d'un epsilon -> bias relatif change -> shadow acne
                // qui scintille.
                float32 texelSize = (radius * 2.f) / float32(tilePx);
                float32 cxLS = right.Dot(center);
                float32 cyLS = upL.Dot(center);
                float32 czLS = forward.Dot(center);
                cxLS = std::floor(cxLS / texelSize) * texelSize;
                cyLS = std::floor(cyLS / texelSize) * texelSize;
                czLS = std::floor(czLS / texelSize) * texelSize;
                center = right * cxLS + upL * cyLS + forward * czLS;
            }

            NkVec3f eye = center - lightDir * (radius * 4.f);
            outView = NkMat4f::LookAt(eye, center, upL);
            outProj = NkMat4f::Orthogonal(radius * 2.f, radius * 2.f,
                                           0.1f, radius * 8.f);
        }

        // ---------------------------------------------------------------------
        // AllocSlotsDirectional : 1 a kMaxCascades slots (cascades CSM).
        // Tile size decreasing par cascade : base, base/2, base/4, base/8.
        // ---------------------------------------------------------------------
        void NkVirtualShadowMaps::AllocSlotsDirectional(
                const NkCamera3D& mainCam, const NkLightDesc& light, uint32 lightIdx) {
            uint32 N = mCfg.numCascades;
            if (N < 1) N = 1; if (N > kMaxCascades) N = kMaxCascades;

            float32 zNear = (mCfg.cascadeNear > 0.f) ? mCfg.cascadeNear : mainCam.GetNear();
            float32 zFar  = (mCfg.cascadeFar  > 0.f) ? mCfg.cascadeFar  : mainCam.GetFar();
            float32 splits[kMaxCascades] = {0};
            ComputeCascadeSplits(zNear, zFar, mCfg.cascadeLambda, N, splits);

            uint32 baseTile = mCfg.cascadeBaseTile;
            uint32 firstSlot = mActiveSlotCount;
            uint32 successCount = 0;

            for (uint32 c = 0; c < N; c++) {
                if (mActiveSlotCount >= kMaxShadowSlots) break;
                uint32 tilePx = baseTile >> c;       // 1024, 512, 256, 128
                if (tilePx < 64) tilePx = 64;
                NkShadowTileRect rect;
                if (!mPacker.Allocate(tilePx, tilePx, rect)) {
                    logger.Warn("[NkVSM] Atlas plein, skip cascade {0} pour light {1}\n",
                                c, lightIdx);
                    break;
                }

                NkShadowSlot& s = mSlots[mActiveSlotCount];
                s.tileRect      = rect;
                s.tileUV        = mPacker.ToUV(rect);
                s.lightIdx      = lightIdx;
                s.slotType      = NkVSMSlotType::DIR_CASCADE;
                s.subIdx        = c;
                s.tileSize      = tilePx;

                float32 subNear = (c == 0) ? zNear : splits[c - 1];
                float32 subFar  = splits[c];
                NkMat4f lightView, lightProj;
                ComputeDirectionalCascade(mainCam, light, c, subNear, subFar, tilePx,
                                          lightView, lightProj);
                s.renderMatrix = lightProj * lightView;
                ApplyDepthClipCorrection(s.renderMatrix); // [-1,1]->[0,1] sur VK/DX

                // Bake la transform T pour mapper le NDC cascade -> uv du tile.
                // T = scale(uvSize) * translate(uvCenter) en post-mult.
                NkVec4f uv = s.tileUV;
                float32 sx = (uv.z - uv.x) * 0.5f;
                float32 sy = (uv.w - uv.y) * 0.5f;
                float32 tx = (uv.z + uv.x) * 0.5f;
                float32 ty = (uv.w + uv.y) * 0.5f;
                // T : transforme NDC [-1,1] vers UV [uv.x..uv.z] (en post-mult)
                // ATTENTION : le shader fait coord.xyz/coord.w puis *0.5+0.5
                // pour UV [0,1]. On veut UV [uv.x..uv.z]. Solution : ne pas
                // baker dans la matrice -> garder shadowMatrix = renderMatrix,
                // et faire le mapping (NDC -> tileUV) dans le shader via tileUV.
                // -> Plus simple et flexible.
                s.shadowMatrix  = s.renderMatrix;
                // Stocke splitFar pour cascade selection dans le shader.
                s.lightPosOrDir = NkVec4f{light.direction.x, light.direction.y,
                                           light.direction.z, subFar};

                // DIAG (gated NK_VSM_DIAG) : pour un worldPos de reference FIXE,
                // logge l'UV atlas que la shadowMatrix lui assigne. Si la camera
                // bouge mais que ce point monde fixe change d'UV -> swimming.
                static int vsmDiag = -1;
                if (vsmDiag == -1) {
                    const char* v = getenv("NK_VSM_DIAG");
                    vsmDiag = (v && v[0] && v[0] != '0') ? 1 : 0;
                }
                if (vsmDiag && c == 0) {
                    const NkVec3f wpRef{2.f, 0.5f, 2.f}; // sphere posee fixe
                    NkVec4f cc = s.shadowMatrix * NkVec4f{wpRef.x, wpRef.y, wpRef.z, 1.f};
                    if (cc.w != 0.f) {
                        float32 px = (cc.x / cc.w) * 0.5f + 0.5f;
                        float32 py = (cc.y / cc.w) * 0.5f + 0.5f;
                        float32 pz = (cc.z / cc.w);
                        logger.Info("[VSMDiag] camX={0} cas0 ref(2,0.5,2) ndcUV=({1},{2}) z={3}\n",
                                    mainCam.GetPosition().x, px, py, pz);
                    }
                }

                mActiveSlotCount++;
                successCount++;
            }

            if (successCount > 0) {
                mFirstSlotPerLight[lightIdx] = (int32)firstSlot;
                mSlotCountPerLight[lightIdx] = (int32)successCount;
            }
        }

        // ---------------------------------------------------------------------
        // AllocSlotsSpot : 1 slot par spot. Projection perspective avec
        // outerAngle * 2 comme fov, near=0.1, far=range.
        // ---------------------------------------------------------------------
        void NkVirtualShadowMaps::AllocSlotsSpot(const NkLightDesc& light, uint32 lightIdx) {
            if (mActiveSlotCount >= kMaxShadowSlots) return;

            uint32 tilePx = mCfg.spotTile > 0 ? mCfg.spotTile : 512;
            NkShadowTileRect rect;
            if (!mPacker.Allocate(tilePx, tilePx, rect)) {
                logger.Warn("[NkVSM] Atlas plein, skip spot light {0}\n", lightIdx);
                return;
            }

            NkShadowSlot& s = mSlots[mActiveSlotCount];
            s.tileRect      = rect;
            s.tileUV        = mPacker.ToUV(rect);
            s.lightIdx      = lightIdx;
            s.slotType      = NkVSMSlotType::SPOT;
            s.subIdx        = 0;
            s.tileSize      = tilePx;

            NkVec3f pos    = light.position;
            NkVec3f dir    = light.direction.Normalized();
            NkVec3f target = pos + dir;
            // Up axis : (0,1,0) sauf si dir presque vertical.
            NkVec3f up = (std::fabs(dir.y) > 0.95f) ? NkVec3f{0, 0, 1} : NkVec3f{0, 1, 0};
            NkMat4f lightView = NkMat4f::LookAt(pos, target, up);
            float32 fovDeg    = light.outerAngle * 2.f;
            if (fovDeg < 1.f) fovDeg = 30.f;
            if (fovDeg > 170.f) fovDeg = 170.f;
            float32 farP = light.range > 0.f ? light.range : 50.f;
            NkMat4f lightProj = NkMat4f::Perspective(NkAngle(fovDeg), 1.f, 0.1f, farP);

            s.renderMatrix  = lightProj * lightView;
            ApplyDepthClipCorrection(s.renderMatrix); // [-1,1]->[0,1] sur VK/DX
            s.shadowMatrix  = s.renderMatrix;
            s.lightPosOrDir = NkVec4f{pos.x, pos.y, pos.z, farP};

            mFirstSlotPerLight[lightIdx] = (int32)mActiveSlotCount;
            mSlotCountPerLight[lightIdx] = 1;
            mActiveSlotCount++;
        }

        // ---------------------------------------------------------------------
        // AllocSlotsPoint : 6 slots (cubemap virtuel unrolled dans l'atlas).
        // Ordre faces matche convention OpenGL/Vulkan cubemap :
        //   0:+X 1:-X 2:+Y 3:-Y 4:+Z 5:-Z
        // ---------------------------------------------------------------------
        void NkVirtualShadowMaps::AllocSlotsPoint(const NkLightDesc& light, uint32 lightIdx) {
            const NkVec3f faceDirs[6] = {
                { 1, 0, 0}, {-1, 0, 0},  // +X, -X
                { 0, 1, 0}, { 0,-1, 0},  // +Y, -Y
                { 0, 0, 1}, { 0, 0,-1}   // +Z, -Z
            };
            const NkVec3f faceUps[6] = {
                { 0,-1, 0}, { 0,-1, 0},  // +X, -X
                { 0, 0, 1}, { 0, 0,-1},  // +Y, -Y
                { 0,-1, 0}, { 0,-1, 0}   // +Z, -Z
            };

            uint32 firstSlot = mActiveSlotCount;
            uint32 successCount = 0;
            uint32 tilePx = mCfg.pointFaceTile > 0 ? mCfg.pointFaceTile : 256;
            NkVec3f pos    = light.position;
            float32 farP   = light.range > 0.f ? light.range : 20.f;
            NkMat4f lightProj = NkMat4f::Perspective(NkAngle(90.f), 1.f, 0.1f, farP);

            for (uint32 f = 0; f < 6; f++) {
                if (mActiveSlotCount >= kMaxShadowSlots) break;
                NkShadowTileRect rect;
                if (!mPacker.Allocate(tilePx, tilePx, rect)) {
                    logger.Warn("[NkVSM] Atlas plein, skip point face {0} light {1}\n",
                                f, lightIdx);
                    break;
                }

                NkShadowSlot& s = mSlots[mActiveSlotCount];
                s.tileRect = rect;
                s.tileUV   = mPacker.ToUV(rect);
                s.lightIdx = lightIdx;
                s.slotType = NkVSMSlotType::POINT_FACE;
                s.subIdx   = f;
                s.tileSize = tilePx;

                NkVec3f target = pos + faceDirs[f];
                NkMat4f lightView = NkMat4f::LookAt(pos, target, faceUps[f]);
                s.renderMatrix  = lightProj * lightView;
                ApplyDepthClipCorrection(s.renderMatrix); // [-1,1]->[0,1] sur VK/DX
                s.shadowMatrix  = s.renderMatrix;
                s.lightPosOrDir = NkVec4f{pos.x, pos.y, pos.z, farP};

                mActiveSlotCount++;
                successCount++;
            }

            if (successCount > 0) {
                mFirstSlotPerLight[lightIdx] = (int32)firstSlot;
                mSlotCountPerLight[lightIdx] = (int32)successCount;
            }
        }

        // ---------------------------------------------------------------------
        // AllocSlotsForLights : dispatcher par type. Itere lights et alloue
        // les tiles correspondants.
        // ---------------------------------------------------------------------
        void NkVirtualShadowMaps::AllocSlotsForLights(
                const NkCamera3D& mainCam, const NkVector<NkLightDesc>& lights) {
            uint32 numLights = lights.Size();
            if (numLights > kMaxLightsShadow) numLights = kMaxLightsShadow;
            for (uint32 i = 0; i < numLights; i++) {
                if (!lights[i].castShadow) continue;

                uint32 slotStart = mActiveSlotCount;

                switch (lights[i].type) {
                    case NkLightType::NK_DIRECTIONAL:
                        AllocSlotsDirectional(mainCam, lights[i], i);
                        break;
                    case NkLightType::NK_SPOT:
                        AllocSlotsSpot(lights[i], i);
                        break;
                    case NkLightType::NK_POINT:
                        AllocSlotsPoint(lights[i], i);
                        break;
                    case NkLightType::NK_AREA:
                    default:
                        // pas de shadow pour area light en V0.
                        break;
                }

                // NkVSM v1 caching : marque les slots de cette light comme cached
                // si shadowStatic=true ET la light n'a pas bouge depuis le render
                // precedent. Le packer alloue les tiles en ordre deterministe
                // donc les tileUV restent stables entre frames -> les anciennes
                // donnees depth dans l'atlas restent valides.
                NkLightShadowCache& cache = mLightCache[i];
                NkVec3f dp = cache.lastPosition  - lights[i].position;
                NkVec3f dd = cache.lastDirection - lights[i].direction;
                float32 dpSq = dp.x*dp.x + dp.y*dp.y + dp.z*dp.z;
                float32 ddSq = dd.x*dd.x + dd.y*dd.y + dd.z*dd.z;
                bool stateChanged =
                    dpSq > 1e-6f
                 || ddSq > 1e-6f
                 || std::fabs(cache.lastRange - lights[i].range) > 1e-4f
                 || (cache.wasStatic != lights[i].shadowStatic);

                bool canCache = lights[i].shadowStatic
                             && cache.renderedOnce
                             && !stateChanged;

                if (canCache) {
                    // Marque tous les slots de cette light comme cached -> skip
                    // au render. UBO toujours upload (slots restent reachable).
                    for (uint32 s = slotStart; s < mActiveSlotCount; s++) {
                        mSlots[s].cached = true;
                    }
                } else {
                    // Update cache state pour la prochaine frame.
                    cache.lastPosition  = lights[i].position;
                    cache.lastDirection = lights[i].direction;
                    cache.lastRange     = lights[i].range;
                    cache.wasStatic     = lights[i].shadowStatic;
                    cache.renderedOnce  = true;
                }
            }
        }


        // ---------------------------------------------------------------------
        // UploadSlotsUBO : pack data CPU -> ShadowSlotsUBOBlock -> WriteBuffer.
        // ---------------------------------------------------------------------
        void NkVirtualShadowMaps::UploadSlotsUBO() {
            ShadowSlotsUBOBlock b{};
            for (uint32 i = 0; i < mActiveSlotCount; i++) {
                const NkShadowSlot& s = mSlots[i];
                b.slots[i].shadowMatrix  = s.shadowMatrix;
                b.slots[i].tileUV        = s.tileUV;
                b.slots[i].lightPosOrDir = s.lightPosOrDir;
                b.slots[i].packedIds     = NkVec4f{
                    float32(s.lightIdx),
                    float32(int32(s.slotType)),
                    float32(s.subIdx),
                    0.f
                };
            }
            // Pack int32 arrays en vec4 (4 entiers par vec4 pour std140).
            for (uint32 i = 0; i < kMaxLightsShadow / 4; i++) {
                b.firstSlotPerLight[i] = NkVec4f{
                    float32(mFirstSlotPerLight[i*4+0]),
                    float32(mFirstSlotPerLight[i*4+1]),
                    float32(mFirstSlotPerLight[i*4+2]),
                    float32(mFirstSlotPerLight[i*4+3])
                };
                b.slotCountPerLight[i] = NkVec4f{
                    float32(mSlotCountPerLight[i*4+0]),
                    float32(mSlotCountPerLight[i*4+1]),
                    float32(mSlotCountPerLight[i*4+2]),
                    float32(mSlotCountPerLight[i*4+3])
                };
            }
            // softShadows : encode le mode PCF pour le shader.
            //   0 = NONE (PCF 3x3 hard)
            //   1 = PCF/Poisson (PCF3x3/PCF5x5/POISSON unified)
            //   2 = PCSS (contact-hardening)
            int32 softMode = 0;
            if (mCfg.quality == NkVSMShadowQuality::PCSS)        softMode = 2;
            else if (mCfg.quality != NkVSMShadowQuality::NONE)   softMode = 1;
            // globalCfg.z = depthRemap : 1.0 si la matrice d'ombre produit un Z en [-1,1]
            // (OpenGL, le shader doit faire p.z*0.5+0.5) ; 0.0 si deja en [0,1] (VK/DX, on a
            // baked clipZ01 dans renderMatrix -> le shader NE doit PAS refaire le remap).
            float32 depthRemap = DepthIsZeroToOne() ? 0.f : 1.f;
            // globalCfg.w = shadowYFlip : 1.0 sur DX (DX11/DX12), 0.0 sinon. L'atlas est
            // rendu SANS flip viewport sur tous les backends, mais la convention NDC.y→ligne
            // texture diffère : VK (NDC.y=-1=haut) et DX (NDC.y=+1=haut) avec origine texture
            // V=0 en haut. Le sample atlasUV.y = p.y*0.5+0.5 est correct en VK/GL mais
            // INVERSE en V sur DX → ombre décalée/mobile. Sur DX on échantillonne donc en
            // V = 1 - (p.y*0.5+0.5) = -p.y*0.5+0.5.
            const auto _api = mDevice ? mDevice->GetApi() : ::nkentseu::NkGraphicsApi::NK_GFX_API_OPENGL;
            float32 shadowYFlip = (_api == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX11 ||
                                   _api == ::nkentseu::NkGraphicsApi::NK_GFX_API_DX12) ? 1.f : 0.f;
            b.globalCfg  = NkVec4f{float32(mActiveSlotCount), float32(softMode), depthRemap, shadowYFlip};
            b.biasParams = NkVec4f{mCfg.shadowBias, mCfg.normalBias, mCfg.softness, 0.f};

            if (mCurFrameSlot < mUBOSlotsRing.Size()
                && mUBOSlotsRing[mCurFrameSlot].IsValid()) {
                mDevice->WriteBuffer(mUBOSlotsRing[mCurFrameSlot], &b, sizeof(b));
            }
        }

        // ---------------------------------------------------------------------
        // RenderAllShadows : entry-point unique. Lit le scene context via
        // mRender3D, alloue les slots, upload UBO, rend tous les tiles.
        // ---------------------------------------------------------------------
        void NkVirtualShadowMaps::RenderAllShadows(NkICommandBuffer* cmd) {
            if (!cmd || !mRender3D) return;

            // Sync ring frame slot avec NkRender3D pour eviter le data hazard
            // entre frames in flight (CPU vs GPU).
            mCurFrameSlot = mRender3D->GetFrameSlot();
            if (mCurFrameSlot >= mUBOSlotsRing.Size()) mCurFrameSlot = 0;

            // 1. Reset state + reallocate slots pour cette frame.
            const NkSceneContext& ctx = mRender3D->GetSceneContext();
            mActiveSlotCount = 0;
            for (uint32 i = 0; i < kMaxLightsShadow; i++) {
                mFirstSlotPerLight[i] = -1;
                mSlotCountPerLight[i] = 0;
            }
            mPacker.Reset(mCfg.atlasSize, mCfg.atlasSize);
            AllocSlotsForLights(ctx.camera, ctx.lights);
            UploadSlotsUBO();

            if (mActiveSlotCount == 0) return;

            // NkVSM v1 caching : compte combien de slots sont a re-rendre. Si
            // TOUS les slots sont cached (scene 100% statique), skip toute la
            // render pass (clear inclus -> preserve les donnees depth des
            // frames precedentes).
            //
            // Note V1 limitation : si AT LEAST 1 slot est non-cached, on clear
            // tout l'atlas et on re-rend les cached aussi (perte du benefice
            // caching pour cette frame). Fix V2 : ajouter ClearRect API au
            // RHI pour clear scissor-bound + rerender uniquement non-cached.
            mRenderedSlotsCount = 0;
            mCachedSlotsCount   = 0;
            for (uint32 i = 0; i < mActiveSlotCount; i++) {
                if (mSlots[i].cached) mCachedSlotsCount++;
                else                  mRenderedSlotsCount++;
            }
            if (mRenderedSlotsCount == 0) {
                // Toutes les lights sont en cache hit -> skip render pass.
                // L'atlas garde le contenu des frames precedentes (pas de clear).
                return;
            }

            uint32 atlasSz = mCfg.atlasSize;
            cmd->SetClearDepth(1.f, 0);
            NkRect2D area((int32)0, (int32)0, (int32)atlasSz, (int32)atlasSz);
            cmd->BeginRenderPass(NkRenderPassHandle{}, mShadowFB, area);

            for (uint32 i = 0; i < mActiveSlotCount; i++) {
                const NkShadowSlot& s = mSlots[i];
                // Note : on rend AUSSI les slots cached cette frame parce que
                // le clear global les efface. Le caching ne devient effectif
                // que si TOUS les slots sont cached (cf. early return ci-dessus).
                // V2 todo : clear scissor + skip cached rerender.
                NkViewport vp{
                    float32(s.tileRect.x), float32(s.tileRect.y),
                    float32(s.tileRect.w), float32(s.tileRect.h),
                    0.f, 1.f, /*flipY=*/false
                };
                NkRect2D scissor{
                    (int32)s.tileRect.x, (int32)s.tileRect.y,
                    (int32)s.tileRect.w, (int32)s.tileRect.h
                };
                cmd->SetViewport(vp);
                cmd->SetScissor(scissor);
                mRender3D->RenderShadowPass(cmd, s.renderMatrix);
            }

            cmd->EndRenderPass();

            // Transition atlas DEPTH_WRITE -> SHADER_READ pour les passes
            // Geometry qui suivent.
            if (mAtlasRhi.IsValid()) {
                cmd->TextureBarrier(mAtlasRhi,
                                    NkResourceState::NK_DEPTH_WRITE,
                                    NkResourceState::NK_SHADER_READ,
                                    NkPipelineStage::NK_LATE_FRAGMENT,
                                    NkPipelineStage::NK_FRAGMENT_SHADER);
            }
        }

    } // namespace renderer
} // namespace nkentseu
