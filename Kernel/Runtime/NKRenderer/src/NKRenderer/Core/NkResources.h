#pragma once
// =============================================================================
// NkResources.h  — NKRenderer v5.0  (Core/)
//
// Helpers RHI centralises pour eviter la duplication entre sous-systemes
// (Render2D, Render3D, Materials, Shadow, PostProcess...).
//
// Couvre :
//   1. Default textures (1x1 white/black/normal/magenta-fallback)
//      -> rappel : ces textures sont aussi exposees par NkTextureLibrary
//         (GetWhite1x1 etc.). NkResources stocke les handles RHI bruts ;
//         NkTextureLibrary les wrappe en NkTexHandle.
//
//   2. Default samplers (Linear/Nearest x Repeat/Clamp/Border, Anisotropic16,
//      Shadow comparaison-sampler)
//
//   3. Standard descriptor-set layouts par convention UE5/UPBGE :
//        - Frame (set 0) : camera + lights + IBL + shadow atlas
//        - Object (set 1) : modele + bones + per-instance
//        - Material (set 2) : PBR params + 5 textures (albedo, normal, ORM, emissive, AO)
//        - PostProcess (set 3) : sampler 2D unique + push constants
//
//   4. Buffer factories : CreateUBO / CreateSSBO / CreateVertexDynamic / etc.
//
// La ressource centrale est cree une seule fois au demarrage du renderer ;
// tous les sous-systemes la consomment en lecture (jamais en mutation).
// =============================================================================
#include "NkRendererTypes.h"
#include "NkRendererResult.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDescs.h"
#include "NKRHI/Core/NkTypes.h"

namespace nkentseu {
    namespace renderer {

        // =====================================================================
        // Convention sets (numerotation UE-style)
        // =====================================================================
        enum NkDescSetIndex : uint32 {
            NK_SET_FRAME       = 0,   // camera, lights, IBL, shadow
            NK_SET_OBJECT      = 1,   // model, bones, instance data
            NK_SET_MATERIAL    = 2,   // PBR params + textures
            NK_SET_POSTPROCESS = 3,   // input image + sampler
        };

        // Bindings dans le set Frame (set=0)
        enum NkFrameBinding : uint32 {
            NK_BIND_CAMERA_UBO    = 0,
            NK_BIND_LIGHTS_UBO    = 1,
            NK_BIND_LIGHTS_SSBO   = 2,   // forward+ light-list
            NK_BIND_CLUSTERS_SSBO = 3,
            NK_BIND_SHADOW_ATLAS  = 4,
            NK_BIND_IBL_IRRADIANCE= 5,
            NK_BIND_IBL_SPECULAR  = 6,
            NK_BIND_BRDF_LUT      = 7,
        };

        // Bindings dans le set Object (set=1)
        enum NkObjectBinding : uint32 {
            NK_BIND_OBJECT_UBO    = 0,
            NK_BIND_BONES_SSBO    = 1,
            NK_BIND_INSTANCE_SSBO = 2,
        };

        // Bindings dans le set Material (set=2)
        enum NkMaterialBinding : uint32 {
            NK_BIND_PBR_PARAMS    = 0,   // UBO
            NK_BIND_TEX_ALBEDO    = 1,
            NK_BIND_TEX_NORMAL    = 2,
            NK_BIND_TEX_ORM       = 3,   // O=AO, R=Roughness, M=Metallic
            NK_BIND_TEX_EMISSIVE  = 4,
            NK_BIND_TEX_AO        = 5,   // AO separe (override de l'ORM si fourni)
        };

        // =====================================================================
        // NkResources
        // =====================================================================
        class NkResources {
            public:
                NkResources() = default;
                ~NkResources();

                NkResources(const NkResources&)            = delete;
                NkResources& operator=(const NkResources&) = delete;

                NkRResult Init(NkIDevice* device);
                void      Shutdown();
                bool      IsReady() const noexcept { return mReady; }

                // ── Default textures (1x1) ────────────────────────────────────
                // Utiles comme fallback quand un slot de material est vide.
                NkTextureHandle GetWhiteTex()    const noexcept { return mTexWhite; }
                NkTextureHandle GetBlackTex()    const noexcept { return mTexBlack; }
                NkTextureHandle GetNormalTex()   const noexcept { return mTexNormal; }   // (0.5, 0.5, 1, 1)
                NkTextureHandle GetMagentaTex()  const noexcept { return mTexMagenta; }  // missing-tex marker
                NkTextureHandle GetGrayTex()     const noexcept { return mTexGray; }     // (0.5, 0.5, 0.5, 1)

                // ── Default samplers ──────────────────────────────────────────
                NkSamplerHandle GetSamplerLinearRepeat()    const noexcept { return mSamLinearRepeat; }
                NkSamplerHandle GetSamplerLinearClamp()     const noexcept { return mSamLinearClamp; }
                NkSamplerHandle GetSamplerLinearBorder()    const noexcept { return mSamLinearBorder; }
                NkSamplerHandle GetSamplerNearestRepeat()   const noexcept { return mSamNearestRepeat; }
                NkSamplerHandle GetSamplerNearestClamp()    const noexcept { return mSamNearestClamp; }
                NkSamplerHandle GetSamplerAnisotropic16()   const noexcept { return mSamAniso16; }
                NkSamplerHandle GetSamplerShadow()          const noexcept { return mSamShadow; }    // PCF compare-sampler
                NkSamplerHandle GetSamplerCubemap()         const noexcept { return mSamCubemap; }   // tri-linear, clamp

                // ── Standard descriptor set layouts ───────────────────────────
                NkDescSetHandle GetFrameLayout()       const noexcept { return mFrameLayout; }
                NkDescSetHandle GetObjectLayout()      const noexcept { return mObjectLayout; }
                NkDescSetHandle GetMaterialLayout()    const noexcept { return mMaterialLayout; }
                NkDescSetHandle GetPostProcessLayout() const noexcept { return mPostProcessLayout; }

                // ── Buffer factories ──────────────────────────────────────────
                // Tous renvoient NkBufferHandle::Null si echec.
                NkBufferHandle CreateUBO          (uint64 sizeBytes, const char* debugName = nullptr);
                NkBufferHandle CreateSSBO         (uint64 sizeBytes, const char* debugName = nullptr);
                NkBufferHandle CreateVertexDynamic(uint64 sizeBytes, const char* debugName = nullptr);
                NkBufferHandle CreateIndexBuffer  (const uint32* data, uint32 count, const char* debugName = nullptr);
                NkBufferHandle CreateStagingBuffer(uint64 sizeBytes, const char* debugName = nullptr);

            private:
                NkIDevice* mDevice = nullptr;
                bool       mReady  = false;

                NkTextureHandle mTexWhite, mTexBlack, mTexNormal, mTexMagenta, mTexGray;

                NkSamplerHandle mSamLinearRepeat, mSamLinearClamp, mSamLinearBorder;
                NkSamplerHandle mSamNearestRepeat, mSamNearestClamp;
                NkSamplerHandle mSamAniso16, mSamShadow, mSamCubemap;

                NkDescSetHandle mFrameLayout, mObjectLayout, mMaterialLayout, mPostProcessLayout;

                bool CreateDefaultTextures();
                bool CreateDefaultSamplers();
                bool CreateStandardLayouts();
        };

    } // namespace renderer
} // namespace nkentseu
