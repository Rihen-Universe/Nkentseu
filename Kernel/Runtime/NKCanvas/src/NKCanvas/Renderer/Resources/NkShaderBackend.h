#pragma once
// =============================================================================
// NkShaderBackend.h — Dispatch table GPU pour NkShader (cross-API)
//
// Pattern identique a NkTextureBackend : chaque backend renderer (Software,
// OpenGL, Vulkan, DX11, DX12, Metal) implemente les callbacks et appelle
// NkShaderSetBackend() en fin d'Initialize(). NkShader.cpp utilise la table
// active sans dependre du backend.
//
// SOURCES PASSEES A `Create`
//   On passe **toutes les variantes** au backend ; le backend choisit celle
//   qui correspond a son API et ignore les autres. Si la variante requise
//   n'est pas fournie, le backend retourne 0.
//
// CALLBACKS
//   - Create : compile + linke. Retourne un identifiant > 0 si OK, 0 sinon.
//   - Destroy : libere les ressources GPU.
//   - Use : active le shader pour les draws suivants. NkRenderTarget l'appelle
//     juste avant un submit batch quand states.shader != nullptr.
//   - SetUniform* : modifie un uniforme nomme du shader actif. Le backend
//     resout le nom en location/binding et cache.
//
// NO-COUPLAGE NKRHI
//   Aucun include NKRHI ici. Les backends NKCanvas peuvent appeler shaderc/
//   spirv-cross internement si necessaire, mais ce n'est pas exige par cette
//   interface. OpenGL/DX/Metal peuvent compiler le source directement via
//   leur API (glCompileShader, D3DCompile, MTLLibrary newLibraryWithSource).
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {
    namespace renderer {

        /// Bundle de sources passees a Create. Chaque pointeur peut etre null
        /// si la source pour ce langage n'est pas fournie.
        struct NkShaderSources {
            const char* glslVert     = nullptr;
            const char* glslFrag     = nullptr;
            const char* hlslVert     = nullptr;
            const char* hlslFrag     = nullptr;
            const char* mslVert      = nullptr;
            const char* mslFrag      = nullptr;
            const uint32* spirvVert  = nullptr;
            usize         spirvVertWords = 0;
            const uint32* spirvFrag  = nullptr;
            usize         spirvFragWords = 0;
        };

        struct NkShaderBackend {
            uint32 (*Create)(const NkShaderSources& sources)               = nullptr;
            void   (*Destroy)(uint32 id)                                   = nullptr;
            void   (*Use)(uint32 id)                                       = nullptr;

            void   (*SetFloat)(uint32 id, const char* name, float v)      = nullptr;
            void   (*SetVec2) (uint32 id, const char* name, float x, float y) = nullptr;
            void   (*SetVec3) (uint32 id, const char* name, float x, float y, float z) = nullptr;
            void   (*SetVec4) (uint32 id, const char* name, float x, float y, float z, float w) = nullptr;
            void   (*SetMat4) (uint32 id, const char* name, const float* mat16) = nullptr;
            /// `texGPUId` est l'identifiant retourne par NkTextureBackend::Create.
            void   (*SetTexture)(uint32 id, const char* name, uint32 texGPUId, uint32 slot) = nullptr;
        };

        /// Enregistre la table de dispatch active (appele par chaque backend
        /// renderer en fin d'Initialize). Remplace les callbacks precedents.
        void NkShaderSetBackend(const NkShaderBackend& backend);

        /// Helper d'installation d'un backend stub pour les renderers qui n'ont
        /// pas encore d'implementation shader user-custom (Software/Vulkan/
        /// DX11/DX12/Metal au 2026-05-30). Tous les callbacks sont no-op (Create
        /// retourne 0 → NkShader::Compile retourne false → l'app sait via
        /// IsValid()). Le `backendName` est utilise pour un log unique.
        void NkShaderInstallUnsupportedBackend(const char* backendName);

    } // namespace renderer
} // namespace nkentseu
