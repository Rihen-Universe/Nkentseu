/*
    * NkUIRHIBackend.h
    *
    * Backend NKRHI reutilisable pour le rendu des draw lists NKUI.
    *
    * Ce fichier fait partie de Nkentseu.
    *
    * Version formalisee (reutilisable) du backend qui existait sous forme de demo
    * dans Applications/Sandbox/src/DemoNkentseu/Base04/NkUINKRHIBackend.{h,cpp}.
    * Le comportement est fonctionnellement identique : meme pipeline, memes
    * buffers (VBO/IBO/UBO geometriques dynamiques avec croissance/retraction),
    * meme upload de texture/atlas (RGBA8 + Gray8 -> RGBA8), meme gestion du
    * scissor/clip (top-left pour DX/VK/SW, bottom-left pour OpenGL) et meme
    * conversion de vertices (couleur packee RGBA8 -> vec4).
*/

#pragma once

/*
 * NKUI_MAINTENANCE_GUIDE
 * Responsibility: Reusable NKRHI adapter interface for NKUI draw lists.
 * Main data: Pipeline/resources/descriptors/texture upload handles.
 * Change this file when: The backend behavior or integration contract changes.
 */

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKUI/NKUI.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace nkui {

        /**
         * @brief Backend de rendu NKUI -> NKRHI reutilisable.
         *
         * Permet a n'importe quelle application de rendre une UI NKUI via NKRHI
         * sans dupliquer le code de la demo Base04.
         *
         * Cycle de vie typique :
         *   NkUIRHIBackend backend;
         *   backend.Init(device, renderPass, api);
         *   // chaque frame, dans une render pass active :
         *   backend.Submit(cmd, uiContext, framebufferWidth, framebufferHeight);
         *   // a l'arret :
         *   backend.Destroy();
         *
         * Les textures (ex. atlas de police) sont fournies par l'appelant via
         * UploadTextureRGBA8 / UploadTextureGray8, identifiees par un texId != 0
         * (texId == 0 est reserve a la texture blanche interne, geometrie unie).
         */
        class NkUIRHIBackend {
            public:
                // Cree le pipeline et les ressources GPU. Retourne false en cas d'echec
                // (et libere ce qui a deja ete cree). renderPass doit etre compatible
                // avec la passe dans laquelle Submit sera appele.
                bool Init(NkIDevice* device, NkRenderPassHandle renderPass, NkGraphicsApi api);

                // Libere toutes les ressources GPU. Sur sans Init prealable.
                void Destroy();

                // Enregistre les draw lists du contexte NKUI dans le command buffer.
                // A appeler dans une render pass active. fbW/fbH = taille du framebuffer
                // cible en pixels (sert au passage NDC et au scissor).
                void Submit(NkICommandBuffer* cmd, const NkUIContext& ctx, uint32 fbW, uint32 fbH);

                // Upload (ou met a jour) une texture RGBA8 identifiee par texId (!= 0).
                bool UploadTextureRGBA8(uint32 texId, const uint8* data, int32 width, int32 height);

                // Upload (ou met a jour) une texture niveaux-de-gris 8 bits (ex. atlas
                // de police) identifiee par texId (!= 0). Convertie en RGBA8 (blanc + alpha).
                bool UploadTextureGray8(uint32 texId, const uint8* data, int32 width, int32 height);

                // True si texId est connu et pret (ou si texId == 0, la texture blanche).
                bool HasTexture(uint32 texId) const noexcept;

            private:
                static constexpr uint64 kMinVBOCap = 1ull * 1024ull * 1024ull;
                static constexpr uint64 kMinIBOCap = 512ull * 1024ull;
                static constexpr uint32 kShrinkDelayFrames = 240u;

                NkIDevice*         mDevice = nullptr;
                NkGraphicsApi      mApi = NkGraphicsApi::NK_GFX_API_OPENGL;
                NkRenderPassHandle mRenderPass;
                NkShaderHandle     mShader;
                NkPipelineHandle   mPipeline;
                NkBufferHandle     mVBO;
                NkBufferHandle     mIBO;
                NkBufferHandle     mUBO;
                uint64             mVBOCap = 0;
                uint64             mIBOCap = 0;
                uint32             mLowUsageFrames = 0;
                NkTextureHandle    mWhiteTex;
                NkSamplerHandle    mSampler;
                NkDescSetHandle    mLayout;
                NkDescSetHandle    mWhiteDescSet;
                NkVector<NkUIVertex> mScratchVtx;
                NkVector<uint32>     mScratchIdx;

                struct TextureEntry {
                    NkTextureHandle texture;
                    NkDescSetHandle descSet;
                    int32 width = 0;
                    int32 height = 0;
                };

                struct RetiredTextureEntry {
                    TextureEntry entry;
                    uint64 retireFrame = 0;
                };

                NkHashMap<uint32, TextureEntry> mTextures;
                NkVector<RetiredTextureEntry> mRetiredTextures;
                uint32 mBoundTexId = 0xFFFFFFFFu;
                uint64 mFrameIndex = 0;
                static constexpr uint64 kRetireDelayFrames = 8ull;

                bool CreatePipeline();
                bool CreateResources();
                bool UploadTextureInternal(uint32 texId, const uint8* data, int32 width, int32 height, bool rgba8);
                bool EnsureGeometryBuffers(uint64 requiredVtxBytes, uint64 requiredIdxBytes, bool allowShrink);
                bool CreateDescriptorSetForTexture(NkTextureHandle texture, NkDescSetHandle& outSet);
                void RetireTextureEntry(const TextureEntry& entry);
                void CollectRetiredTextures();

                void BindTexture(NkICommandBuffer* cmd, uint32 texId);
        };

    }
}
