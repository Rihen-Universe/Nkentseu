#pragma once
// =============================================================================
// NkTextureBackend.h — Dispatch table GPU pour NkTexture
//
// Chaque backend (Software, OpenGL, Vulkan, DX11, DX12) implemente les 5
// callbacks de cette table et appelle `NkTextureSetBackend(table)` a la fin
// de son `Initialize()`. NkTexture.cpp utilise ensuite la table active pour
// Create/Update/Destroy/SetFilter/SetWrap sans dependre du backend.
//
// CONVENTION
//   - Le `uint32` retourne par `Create` est un identifiant interne au backend
//     (texture name OpenGL, index dans une table Software, slot SRV DX12, …).
//     NkTexture le stocke dans `mGPUId`. Une valeur 0 = invalide.
//   - L'input pixel est RGBA8 (4 octets / pixel, packed, non premultiplie).
//   - Pas de mipmaps automatiques ; NkTexture::GenerateMipmap est un no-op
//     pour l'instant (chaque backend pourra l'exposer dans son dispatch v2).
//
// THREAD SAFETY
//   Les callbacks sont appeles depuis le thread qui possede le contexte
//   graphique (typiquement le main thread). Pas de garantie cross-thread.
//
// EXEMPLE D'ENREGISTREMENT (OpenGL, en fin de NkOpenGLRenderer2D::Initialize)
//   NkTextureBackend backend{
//       &NkOpenGLRenderer2D::CreateGLTexture,
//       &NkOpenGLRenderer2D::UpdateGLTexture,
//       &NkOpenGLRenderer2D::DeleteGLTexture,
//       &NkOpenGLRenderer2D::SetGLTextureFilter,
//       &NkOpenGLRenderer2D::SetGLTextureWrap,
//   };
//   NkTextureSetBackend(backend);
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace renderer {

        // Forward decl des enums (definis dans NkTexture.h).
        enum class NkTextureFilter : uint8;
        enum class NkTextureWrap   : uint8;

        struct NkTextureBackend {
            uint32 (*Create)(uint32 w, uint32 h, const uint8* rgba)            = nullptr;
            void   (*Update)(uint32 id, uint32 x, uint32 y, uint32 w, uint32 h,
                             const uint8* rgba)                                = nullptr;
            void   (*Destroy)(uint32 id)                                       = nullptr;
            void   (*SetFilter)(uint32 id, NkTextureFilter f)                  = nullptr;
            void   (*SetWrap)  (uint32 id, NkTextureWrap   w)                  = nullptr;
        };

        /// Enregistre la table de dispatch active. Appele par chaque backend
        /// a la fin de son Initialize() ; remplace les callbacks precedents.
        /// Un Initialize successif ecrase. Apres Shutdown, idealement on
        /// re-appelle avec une table vide (callbacks null) ; les Destroy
        /// ulterieurs sont alors no-op (protection dans NkTexture::Destroy).
        void NkTextureSetBackend(const NkTextureBackend& backend);

    } // namespace renderer
} // namespace nkentseu
