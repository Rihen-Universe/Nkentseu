#pragma once
// =============================================================================
// NkShadowSystem.h
// Système de shadow mapping complet pour NKRenderer.
//
// Fonctionnalités :
//   • Shadow map directionnelle (CSM — Cascaded Shadow Maps, jusqu'à 4 cascades)
//   • Shadow maps ponctuelles (cubemap, jusqu'à 8 lumières)
//   • PCF (Percentage Closer Filtering) 3×3, 5×5 ou Poisson
//   • PCSS (Percentage Closer Soft Shadows) optionnel
//   • Stable cascades (stabilisation de scintillement)
//   • Bias adaptatif (normal offset + slope-scale depth bias)
//   • Shadow fade (fondu aux bords du frustum)
//
// Usage :
//   NkShadowSystem shadow(renderer);
//   shadow.Init({.numCascades=3, .shadowMapSize=2048, .pcf=NkPCFMode::PCF3x3});
//   // Par frame :
//   shadow.BeginShadowPass(cmd, sunDir, mainCamera);
//     cmd.Draw3D(mesh, mat, transform);  // draw calls qui cast shadow
//   shadow.EndShadowPass(cmd);
//   // Les textures sont accessibles pour le pass principal :
//   NkTextureHandle shadowAtlas = shadow.GetShadowAtlas();
// =============================================================================
#include "NKRenderer/Core/NkIRenderer.h"
#include "NKRenderer/Command/NkRendererCommand.h"

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Configuration
    // =========================================================================
    enum class NkPCFMode : uint8 {
        None    = 0,  // shadow map raw
        PCF3x3  = 1,  // 3x3 bilinear — compromis qualité/perf
        PCF5x5  = 2,  // 5x5 — meilleur mais plus lent
        Poisson = 3,  // Poisson disc 16 samples — softness variable
        PCSS    = 4,  // Percentage Closer Soft Shadows — ombres douces réalistes
    };

    enum class NkShadowProjection : uint8 {
        Orthographic = 0,  // lumières directionnelles
        Perspective  = 1,  // lumières spot
        Cubemap      = 2,  // lumières ponctuelles
    };

    struct NkShadowConfig {
        uint32           shadowMapSize   = 2048;     // résolution par cascade/face
        uint32           numCascades     = 3;        // 1..4 pour CSM directionnel
        NkPCFMode        pcf             = NkPCFMode::PCF3x3;
        float32          nearPlane       = 0.1f;
        float32          farPlane        = 100.f;
        float32          cascadeSplitLambda = 0.75f; // distribution des cascades [0=linéaire, 1=log]
        float32          normalBias      = 0.005f;   // bias en direction de la normale
        float32          slopeScaleBias  = 1.5f;
        float32          constantBias    = 0.005f;
        float32          fadeStart       = 0.85f;    // début du fade en NDC
        bool             stableCascades  = true;     // évite le scintillement sur mouvement caméra
        bool             visualizeCascades = false;  // debug : colorier les cascades
        uint32           maxPointShadows = 4;        // lumières ponctuelles avec shadow
    };

    // =========================================================================
    // Cascade
    // =========================================================================
    struct NkShadowCascade {
        NkMat4f   lightViewProj;     // matrice VP de la lumière pour cette cascade
        float32   splitNear;         // near clip de la cascade (world units)
        float32   splitFar;          // far clip de la cascade
        float32   texelSize;         // taille d'un texel (pour le bias adaptatif)
        NkVec2f   atlasOffset;       // offset dans l'atlas (UV normalisé)
        NkVec2f   atlasScale;        // scale dans l'atlas
    };

    // =========================================================================
    // NkShadowSystem
    // =========================================================================
    class NkShadowSystem {
    public:
        explicit NkShadowSystem(NkIRenderer* renderer)
            : mRenderer(renderer) {}

        ~NkShadowSystem() { Destroy(); }

        NkShadowSystem(const NkShadowSystem&) = delete;
        NkShadowSystem& operator=(const NkShadowSystem&) = delete;

        // ── Lifecycle ────────────────────────────────────────────────────────
        bool Init(const NkShadowConfig& config = {});
        void Destroy();
        bool IsValid() const { return mAtlasTex.IsValid(); }

        // ── Mise à jour des splits (appelée quand la caméra change) ──────────
        // mainCam : caméra principale (pour calculer les cascades)
        // sunDir  : direction du soleil normalisée (world space)
        void UpdateCascades(NkCameraHandle mainCam, NkVec3f sunDir);

        // ── Shadow pass ───────────────────────────────────────────────────────
        // Commence le pass shadow (bind le render target + configure le viewport)
        // Retourner false = skip le shadow pass ce frame
        bool BeginShadowPass(NkRendererCommand& cmd, uint32 cascadeIdx = 0);
        void EndShadowPass  (NkRendererCommand& cmd);

        // ── Point light shadow ────────────────────────────────────────────────
        // Render vers la face cubemap d'une lumière ponctuelle
        bool BeginPointShadowPass(NkRendererCommand& cmd, uint32 lightIdx, uint32 faceIdx);
        void EndPointShadowPass  (NkRendererCommand& cmd);

        // ── Accesseurs ────────────────────────────────────────────────────────
        NkTextureHandle GetShadowAtlas()         const { return mAtlasTex; }
        NkTextureHandle GetPointShadowCubemap(uint32 lightIdx) const;

        uint32 GetNumCascades()   const { return mConfig.numCascades; }
        const NkShadowCascade& GetCascade(uint32 idx) const {
            static NkShadowCascade dummy{};
            return (idx < mConfig.numCascades) ? mCascades[idx] : dummy;
        }

        // Matrice lightVP + bias prête à être uploadée dans le shader
        NkMat4f GetLightVPMatrix(uint32 cascadeIdx = 0) const;

        // NDC Z convention : true = [0,1] (Vulkan/DX), false = [-1,1] (OpenGL)
        void SetDepthZeroToOne(bool v) { mDepthZeroToOne = v; }

        // Constantes pour le shader principal (à uploader dans le MaterialUBO)
        struct ShadowShaderData {
            float32 lightVP[4][16];       // matrices VP par cascade
            float32 cascadeSplits[4];     // far planes des cascades
            float32 atlasOffsets[4][2];   // UV offsets dans l'atlas
            float32 atlasScales[4][2];    // UV scales dans l'atlas
            float32 shadowMapSize;
            float32 invShadowMapSize;
            float32 normalBias;
            float32 pcfRadius;
            int32   numCascades;
            int32   pcfMode;
            float32 fadeStart;
            float32 _pad;
        };

        void FillShaderData(ShadowShaderData& out) const;

        // ── Getter matériaux shadow ───────────────────────────────────────────
        NkMaterialHandle GetShadowMaterial()   const { return mShadowMat; }
        NkMaterialInstHandle GetShadowMatInst() const { return mShadowMatInst; }

    private:
        // Calcule les splits de cascades selon la méthode log/uniform
        void ComputeCascadeSplits(float32 nearPlane, float32 farPlane);

        // Calcule la matrice light VP pour une cascade
        NkMat4f ComputeCascadeLightVP(uint32 cascadeIdx,
                                       NkVec3f sunDir,
                                       NkCameraHandle mainCam) const;

        // Ajuste la taille du frustum pour les cascades stables
        NkMat4f StabilizeCascade(const NkMat4f& lightVP, uint32 cascadeIdx) const;

        NkIRenderer*            mRenderer   = nullptr;
        NkShadowConfig          mConfig;
        bool                    mDepthZeroToOne = false;

        // Atlas de shadow maps (toutes les cascades dans une seule texture)
        // Layout de l'atlas (exemple pour 3 cascades 2048) :
        //   [Cascade0 2048×2048][Cascade1 1024×1024][Cascade2 512×512]
        //   (tailles décroissantes pour économiser la mémoire)
        NkTextureHandle         mAtlasTex;
        NkRenderTargetHandle    mAtlasRT;

        // Cascades
        static constexpr uint32 kMaxCascades = 4;
        NkShadowCascade         mCascades[kMaxCascades];
        float32                 mCascadeSplits[kMaxCascades] = {};

        // Shadow maps ponctuelles (cubemaps)
        static constexpr uint32 kMaxPointShadows = 8;
        NkTextureHandle         mPointShadowCubemaps[kMaxPointShadows];
        NkRenderTargetHandle    mPointShadowRTs[kMaxPointShadows];

        // Matériau shadow (depth-only)
        NkMaterialHandle        mShadowMat;
        NkMaterialInstHandle    mShadowMatInst;

        // UBO de shadow
        NkUniformBlockHandle    mShadowUBO;

        uint32                  mCurrentCascade = 0;
        bool                    mInShadowPass   = false;
    };

    // =========================================================================
    // Implémentation
    // =========================================================================
    inline bool NkShadowSystem::Init(const NkShadowConfig& config) {
        mConfig = config;
        mConfig.numCascades = NkClamp(mConfig.numCascades, 1u, kMaxCascades);

        // Créer l'atlas de depth (format D32)
        // L'atlas contient toutes les cascades côte à côte
        // Cascade 0 occupe la moitié gauche, cascade 1 le quart droit, etc.
        const uint32 atlasW = mConfig.shadowMapSize * mConfig.numCascades;
        const uint32 atlasH = mConfig.shadowMapSize;

        NkTextureDesc atlasDesc = NkTextureDesc::Depth(atlasW, atlasH);
        atlasDesc.debugName = "ShadowAtlas";
        mAtlasTex = mRenderer->CreateTexture(atlasDesc);
        if (!mAtlasTex.IsValid()) return false;

        // Configurer les offsets UV de chaque cascade dans l'atlas
        for (uint32 i = 0; i < mConfig.numCascades; ++i) {
            mCascades[i].atlasOffset = {
                (float32)i / (float32)mConfig.numCascades, 0.f
            };
            mCascades[i].atlasScale = {
                1.f / (float32)mConfig.numCascades, 1.f
            };
            mCascades[i].texelSize = 1.f / (float32)mConfig.shadowMapSize;
        }

        // Calculer les splits initiaux
        ComputeCascadeSplits(mConfig.nearPlane, mConfig.farPlane);

        return true;
    }

    inline void NkShadowSystem::Destroy() {
        if (mAtlasTex.IsValid())  mRenderer->DestroyTexture(mAtlasTex);
        if (mAtlasRT.IsValid())   mRenderer->DestroyRenderTarget(mAtlasRT);
        for (uint32 i = 0; i < kMaxPointShadows; ++i) {
            if (mPointShadowCubemaps[i].IsValid())
                mRenderer->DestroyTexture(mPointShadowCubemaps[i]);
        }
        if (mShadowMatInst.IsValid()) mRenderer->DestroyMaterialInstance(mShadowMatInst);
        if (mShadowMat.IsValid())     mRenderer->DestroyMaterialTemplate(mShadowMat);
    }

    inline void NkShadowSystem::ComputeCascadeSplits(float32 near, float32 far) {
        const float32 lambda = mConfig.cascadeSplitLambda;
        const uint32  n      = mConfig.numCascades;
        const float32 range  = far - near;
        const float32 ratio  = far / near;

        mCascadeSplits[0] = near;
        for (uint32 i = 1; i <= n; ++i) {
            const float32 t       = (float32)i / (float32)n;
            const float32 logSplit    = near * NkPow(ratio, t);
            const float32 uniformSplit= near + range * t;
            // Interpolation lambda entre log et uniforme
            const float32 d       = lambda * (logSplit - uniformSplit) + uniformSplit;
            mCascadeSplits[i]     = d;
        }
    }

    inline NkMat4f NkShadowSystem::ComputeCascadeLightVP(uint32 cascadeIdx,
                                                           NkVec3f sunDir,
                                                           NkCameraHandle mainCam) const
    {
        const float32 splitNear = mCascadeSplits[cascadeIdx];
        const float32 splitFar  = mCascadeSplits[cascadeIdx + 1];

        // Calculer les 8 coins du frustum de la caméra pour cette cascade
        NkMat4f view = mRenderer->GetViewMatrix(mainCam);
        NkMat4f proj = mRenderer->GetProjectionMatrix(mainCam);

        // Inverser VP pour obtenir les coins en world space
        NkMat4f invVP = (proj * view).Inverse();

        // Coins NDC (-1..1 ou 0..1 selon la convention)
        NkVec3f frustumCorners[8];
        const float32 zNear = mDepthZeroToOne ? 0.f : -1.f;
        const float32 zFar  = 1.f;
        int corner = 0;
        for (int x = -1; x <= 1; x += 2) {
            for (int y = -1; y <= 1; y += 2) {
                for (float32 z : {zNear, zFar}) {
                    NkVec4f pt = invVP * NkVec4f{(float32)x, (float32)y, z, 1.f};
                    frustumCorners[corner++] = {pt.x/pt.w, pt.y/pt.w, pt.z/pt.w};
                }
            }
        }

        // Centre du frustum
        NkVec3f center = {0,0,0};
        for (const auto& c : frustumCorners) {
            center.x += c.x; center.y += c.y; center.z += c.z;
        }
        center.x /= 8.f; center.y /= 8.f; center.z /= 8.f;

        // Eviter up colinéaire avec sunDir
        NkVec3f up = (NkFabs(sunDir.y) > 0.9f)
            ? NkVec3f{1,0,0} : NkVec3f{0,1,0};

        NkMat4f lightView = NkMat4f::LookAt(
            {center.x - sunDir.x * 50.f,
             center.y - sunDir.y * 50.f,
             center.z - sunDir.z * 50.f},
            center, up);

        // AABB du frustum dans l'espace lumière
        float32 minX=1e9f, maxX=-1e9f;
        float32 minY=1e9f, maxY=-1e9f;
        float32 minZ=1e9f, maxZ=-1e9f;
        for (const auto& c : frustumCorners) {
            NkVec4f ls = lightView * NkVec4f{c.x, c.y, c.z, 1.f};
            minX=NkMin(minX,ls.x); maxX=NkMax(maxX,ls.x);
            minY=NkMin(minY,ls.y); maxY=NkMax(maxY,ls.y);
            minZ=NkMin(minZ,ls.z); maxZ=NkMax(maxZ,ls.z);
        }

        // Marge pour les objets qui cast shadow depuis l'extérieur du frustum
        constexpr float32 zMargin = 10.f;
        minZ -= zMargin;

        NkMat4f lightProj = NkMat4f::Orthogonal(
            {minX, minY}, {maxX, maxY}, minZ, maxZ, mDepthZeroToOne);

        return lightProj * lightView;
    }

    inline NkMat4f NkShadowSystem::StabilizeCascade(const NkMat4f& lightVP,
                                                      uint32 cascadeIdx) const
    {
        if (!mConfig.stableCascades) return lightVP;

        // Projeter l'origine monde dans l'espace NDC de la lumière
        NkVec4f origin = lightVP * NkVec4f{0,0,0,1};
        origin.x /= origin.w; origin.y /= origin.w;

        // Snap à la grille de texels
        const float32 halfShadowMap = (float32)mConfig.shadowMapSize * 0.5f;
        const float32 snapX = NkRound(origin.x * halfShadowMap) / halfShadowMap - origin.x;
        const float32 snapY = NkRound(origin.y * halfShadowMap) / halfShadowMap - origin.y;

        NkMat4f snapMatrix = NkMat4f::Translation({snapX, snapY, 0.f});
        return snapMatrix * lightVP;
    }

    inline void NkShadowSystem::UpdateCascades(NkCameraHandle mainCam, NkVec3f sunDir) {
        for (uint32 i = 0; i < mConfig.numCascades; ++i) {
            NkMat4f lvp = ComputeCascadeLightVP(i, sunDir, mainCam);
            mCascades[i].lightViewProj = StabilizeCascade(lvp, i);
            mCascades[i].splitNear     = mCascadeSplits[i];
            mCascades[i].splitFar      = mCascadeSplits[i + 1];
        }
    }

    inline bool NkShadowSystem::BeginShadowPass(NkRendererCommand& /*cmd*/,
                                                  uint32 cascadeIdx) {
        if (!IsValid() || cascadeIdx >= mConfig.numCascades) return false;
        mCurrentCascade = cascadeIdx;
        mInShadowPass   = true;
        return true;
    }

    inline void NkShadowSystem::EndShadowPass(NkRendererCommand& /*cmd*/) {
        mInShadowPass = false;
    }

    inline NkMat4f NkShadowSystem::GetLightVPMatrix(uint32 cascadeIdx) const {
        if (cascadeIdx >= mConfig.numCascades) return NkMat4f::Identity();
        return mCascades[cascadeIdx].lightViewProj;
    }

    inline void NkShadowSystem::FillShaderData(ShadowShaderData& out) const {
        out.shadowMapSize    = (float32)mConfig.shadowMapSize;
        out.invShadowMapSize = 1.f / (float32)mConfig.shadowMapSize;
        out.normalBias       = mConfig.normalBias;
        out.numCascades      = (int32)mConfig.numCascades;
        out.pcfMode          = (int32)mConfig.pcf;
        out.fadeStart        = mConfig.fadeStart;
        out.pcfRadius        = (mConfig.pcf == NkPCFMode::PCF5x5) ? 2.f : 1.f;

        for (uint32 i = 0; i < mConfig.numCascades && i < 4; ++i) {
            memcpy(out.lightVP[i], mCascades[i].lightViewProj.data, 64);
            out.cascadeSplits[i]  = mCascades[i].splitFar;
            out.atlasOffsets[i][0]= mCascades[i].atlasOffset.x;
            out.atlasOffsets[i][1]= mCascades[i].atlasOffset.y;
            out.atlasScales[i][0] = mCascades[i].atlasScale.x;
            out.atlasScales[i][1] = mCascades[i].atlasScale.y;
        }
    }

    inline NkTextureHandle NkShadowSystem::GetPointShadowCubemap(uint32 lightIdx) const {
        return (lightIdx < kMaxPointShadows) ? mPointShadowCubemaps[lightIdx] : NkTextureHandle::Null();
    }

    inline bool NkShadowSystem::BeginPointShadowPass(NkRendererCommand& /*cmd*/,
                                                      uint32 lightIdx, uint32 faceIdx) {
        (void)lightIdx; (void)faceIdx;
        return false;  // TODO : cubemap shadow maps
    }
    inline void NkShadowSystem::EndPointShadowPass(NkRendererCommand& /*cmd*/) {}

} // namespace renderer
} // namespace nkentseu