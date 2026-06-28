/*
    * NkGuiRHIBackend.h
    *
    * Backend NKRHI reutilisable pour le rendu des draw lists NKGui.
    *
    * Ce fichier fait partie de Nkentseu.
    *
    * Porte de Integrations/NKUI/NkUIRHIBackend : meme pipeline, memes buffers
    * dynamiques (VBO/IBO/UBO), meme upload texture/atlas (RGBA8 + Gray8 -> RGBA8),
    * meme gestion scissor (top-left DX/VK/SW, bottom-left OpenGL), meme conversion
    * de vertices (couleur RGBA8 packee -> vec4). Difference : NKGui expose UNE
    * draw-list (NkGuiDrawList) avec un clipRect PAR commande (et non des couches
    * avec commande NK_CLIP_RECT comme NKUI) ; le scissor est applique par commande.
    *
    * Symbole : nkentseu::nkgui::NkGuiRHIBackend.
    *
    * Sert au rendu de l'UI des applications 2D/3D (app d'animation, moteur de jeu)
    * qui utilisent NKRenderer/NKRHI ; NKCanvas reste pour l'IDE.
*/

#pragma once

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKGui/Core/NkGuiDrawList.h"
#include "NKContainers/Associative/NkHashMap.h"
#include "NKContainers/Sequential/NkVector.h"

namespace nkentseu {
    namespace nkgui {

        // Backend NKGui -> NKRHI reutilisable.
        //
        //   NkGuiRHIBackend backend;
        //   backend.Init(device, renderPass, api);
        //   // chaque frame, dans une render pass active :
        //   backend.Submit(cmd, drawList, fbW, fbH);   // appele par draw-list
        //   backend.Destroy();
        //
        // texId == 0 est reserve a la texture blanche interne (geometrie unie).
        // Les atlas/police + images sont fournis via UploadTextureGray8/RGBA8, ou
        // une texture GPU externe (ex. viewport 3D offscreen) via RegisterTexture.
        class NkGuiRHIBackend {
            public:
                bool Init(NkIDevice* device, NkRenderPassHandle renderPass, NkGraphicsApi api);
                void Destroy();

                // A appeler dans une render pass active. fbW/fbH = framebuffer cible.
                void Submit(NkICommandBuffer* cmd, const NkGuiDrawList& dl, uint32 fbW, uint32 fbH);

                bool UploadTextureRGBA8(uint32 texId, const uint8* data, int32 width, int32 height);
                bool UploadTextureGray8(uint32 texId, const uint8* data, int32 width, int32 height);

                // Enregistre une texture GPU DEJA existante (possedee ailleurs, ex. la
                // cible offscreen du viewport 3D NKRenderer) sous texId, pour l'afficher
                // via NkGuiDrawList::AddImage. La texture n'est PAS detruite par le backend.
                bool RegisterTexture(uint32 texId, NkTextureHandle texture);

                bool HasTexture(uint32 texId) const noexcept;

            private:
                static constexpr uint64 kMinVBOCap = 1ull * 1024ull * 1024ull;
                static constexpr uint64 kMinIBOCap = 512ull * 1024ull;
                static constexpr uint32 kShrinkDelayFrames = 240u;
                static constexpr uint64 kRetireDelayFrames = 8ull;

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

                struct TextureEntry {
                    NkTextureHandle texture;
                    NkDescSetHandle descSet;
                    int32 width = 0;
                    int32 height = 0;
                    bool  owned = true;   // false = texture externe (RegisterTexture)
                };
                struct RetiredTextureEntry { TextureEntry entry; uint64 retireFrame = 0; };

                NkHashMap<uint32, TextureEntry> mTextures;
                NkVector<RetiredTextureEntry>   mRetiredTextures;
                uint32 mBoundTexId = 0xFFFFFFFFu;
                uint64 mFrameIndex = 0;

                bool CreatePipeline();
                bool CreateResources();
                bool UploadTextureInternal(uint32 texId, const uint8* data, int32 width, int32 height, bool rgba8);
                bool EnsureGeometryBuffers(uint64 requiredVtxBytes, uint64 requiredIdxBytes, bool allowShrink);
                bool CreateDescriptorSetForTexture(NkTextureHandle texture, NkDescSetHandle& outSet);
                void RetireTextureEntry(const TextureEntry& entry);
                void CollectRetiredTextures();
                void BindTexture(NkICommandBuffer* cmd, uint32 texId);
        };

    } // namespace nkgui
} // namespace nkentseu
