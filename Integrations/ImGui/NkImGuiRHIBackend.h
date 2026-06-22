#pragma once
// =============================================================================
// NkImGuiRHIBackend.h — Backend de rendu Dear ImGui -> NKRHI (bas niveau)
// -----------------------------------------------------------------------------
// Rend les ImDrawData* produits par Dear ImGui directement via NKRHI : cree son
// pipeline, ses shaders (GLSL/HLSL/SPIR-V selon l'API), ses vertex/index buffers
// dynamiques, la texture de l'atlas de police + son sampler, et emet le scissor
// par commande. C'est l'equivalent ImGui de NkUINKRHIBackend (Base04).
//
// DECOUPLAGE : ce header n'inclut PAS imgui.h (forward-declare ImDrawData +
// le typedef ImTextureID). ImGui n'est donc PAS lie dans le coeur de NKRHI :
// c'est l'application consommatrice qui compile cette integration.
//
// Usage (immediate-mode, par frame) :
//   NkImGuiRHIBackend imgui;
//   imgui.Init(device, renderPass, api);   // une fois
//   imgui.RebuildFontAtlas();              // upload io.Fonts -> texture, SetTexID
//   ...
//   ImGui::Render();
//   cmd->BeginRenderPass(...);
//   imgui.RenderDrawData(cmd, ImGui::GetDrawData(), fbW, fbH);
//   cmd->EndRenderPass();
// =============================================================================

#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKContainers/Sequential/NkVector.h"

// --- Forward declarations ImGui (pas d'include d'imgui.h dans le .h) ---------
struct ImDrawData;
#ifndef ImTextureID
typedef void* ImTextureID;   // doit correspondre au typedef d'imgui.h (void*)
#endif

namespace nkentseu {
    namespace imguiintegration {

        // Bridge immediate-mode Dear ImGui -> NKRHI. Possede pipeline/shader/
        // buffers/sampler + la texture de l'atlas de police et son descriptor set.
        class NkImGuiRHIBackend {
            public:
                NkImGuiRHIBackend() = default;
                ~NkImGuiRHIBackend() { Destroy(); }

                NkImGuiRHIBackend(const NkImGuiRHIBackend&)            = delete;
                NkImGuiRHIBackend& operator=(const NkImGuiRHIBackend&) = delete;

                // device      : peripherique NKRHI actif.
                // renderPass  : render pass cible (pour la creation du pipeline).
                // api         : backend graphique (selectionne le langage de shader).
                bool Init(NkIDevice* device, NkRenderPassHandle renderPass, NkGraphicsApi api);
                void Destroy();

                // (Re)construit la texture de l'atlas de police depuis le contexte
                // ImGui courant (io.Fonts->GetTexDataAsRGBA32 -> NkTextureHandle ->
                // io.Fonts->SetTexID). A appeler une fois apres Init().
                bool RebuildFontAtlas();

                // Enregistre les draw-calls d'ImGui dans cmd. A appeler dans une
                // render pass active. fbW/fbH = taille framebuffer en pixels.
                void RenderDrawData(NkICommandBuffer* cmd, const ImDrawData* drawData, uint32 fbW, uint32 fbH);

            private:
                static constexpr uint64 kMinVBOCap = 1ull * 1024ull * 1024ull;
                static constexpr uint64 kMinIBOCap = 512ull * 1024ull;

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

                NkSamplerHandle    mSampler;
                NkDescSetHandle    mLayout;

                // Atlas police : texture GPU + descriptor set (combined image/UBO).
                NkTextureHandle    mFontTex;
                NkDescSetHandle    mFontDescSet;
                int32              mFontW = 0;
                int32              mFontH = 0;

                // Scratch d'upload : geometrie concatenee de toutes les cmd-lists.
                NkVector<uint8>    mScratchVtx;   // copie brute des ImDrawVert (20 o)
                NkVector<uint32>   mScratchIdx;   // ImDrawIdx(u16) -> u32 + base vtx

                bool CreatePipeline();
                bool CreateResources();
                bool EnsureGeometryBuffers(uint64 requiredVtxBytes, uint64 requiredIdxBytes);
                bool CreateDescriptorSetForTexture(NkTextureHandle texture, NkDescSetHandle& outSet);
        };

    } // namespace imguiintegration
} // namespace nkentseu
