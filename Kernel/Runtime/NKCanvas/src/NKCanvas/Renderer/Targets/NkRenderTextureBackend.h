#pragma once
// =============================================================================
// NkRenderTextureBackend.h — Dispatch GPU pour NkRenderTexture (cross-API)
//
// Meme pattern que NkTextureBackend/NkShaderBackend. Chaque renderer enregistre
// les 4 callbacks a son Initialize() ; NkRenderTexture les utilise pour
// creer/binder/unbinder/detruire son framebuffer offscreen.
//
// CONVENTION
//   - Create(w, h) renvoie un handle uint32 > 0 si OK, 0 sinon.
//     Le handle est interne au backend (FBO OpenGL, VkFramebuffer-index,
//     ID3D11RenderTargetView slot, etc.).
//   - GetColorTextureGPUId(handle) renvoie l'identifiant utilisable par
//     NkTexture (GL texture name, slot SRV, …) pour sampler le resultat.
//     Permet l'usage `target.GetTexture()` en lecture sans re-upload.
//   - Bind(handle) : redirige les draws suivants vers le framebuffer offscreen.
//     L'ancien framebuffer doit etre sauvegarde pour pouvoir restorer.
//   - Unbind() : restaure le framebuffer principal (back buffer).
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace renderer {

        struct NkRenderTextureBackend {
            uint32 (*Create)(uint32 width, uint32 height)                  = nullptr;
            void   (*Destroy)(uint32 handle)                               = nullptr;
            void   (*Bind)(uint32 handle)                                  = nullptr;
            void   (*Unbind)()                                             = nullptr;
            uint32 (*GetColorTextureGPUId)(uint32 handle)                  = nullptr;
        };

        /// Enregistre la table de dispatch active pour les render textures.
        /// Appele par chaque backend renderer en fin d'Initialize.
        void NkRenderTextureSetBackend(const NkRenderTextureBackend& backend);

        /// Helper stub pour les renderers sans support FBO encore implemente
        /// (Software/Vulkan/DX11/DX12 au 2026-05-30). Create retourne 0 ;
        /// NkRenderTexture::Create renverra false.
        void NkRenderTextureInstallUnsupportedBackend(const char* backendName);

    } // namespace renderer
} // namespace nkentseu
