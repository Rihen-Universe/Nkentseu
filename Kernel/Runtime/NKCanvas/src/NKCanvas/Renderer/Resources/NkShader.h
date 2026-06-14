#pragma once
// =============================================================================
// NkShader.h — Shader programmable user-ecrit (style SFML / Godot)
//
// Permet a l'utilisateur d'attacher un shader custom a un draw via
// NkRenderStates::shader, pour faire scroll UV, distortion, color grading,
// effets pixel-art, etc. Architecture parallele a NkTexture :
//   - Cette classe NkShader est l'objet user-facing (handle CPU + uniforms).
//   - L'implementation reelle GPU est dans une **dispatch table backend**
//     (NkShaderBackend.h) que chaque NkIRenderer2D peuple a Initialize().
//   - PAS DE DEPENDANCE A NKRHI : NKCanvas reste autonome. Pas de
//     cross-compile NkSL/SPIR-V/glslang ici (ce serait coupler).
//
// MODE DE FONCTIONNEMENT — SOURCES MULTI-LANGAGE
//   L'utilisateur fournit le source dans le langage du backend cible :
//   GLSL pour OpenGL/GLES/Vulkan-via-glslangValidator, HLSL pour DX11/DX12,
//   MSL pour Metal. Le backend selectionne la source qui lui correspond et
//   ignore les autres. C'est compatible avec un workflow ou l'app fournit
//   plusieurs sources (un par API) OU une seule source dans le langage du
//   backend qu'elle sait dejà utiliser.
//
//   Exemple :
//     const char* glsl_vert = R"(
//       #version 330 core
//       layout(location=0) in vec2 aPos;
//       layout(location=1) in vec2 aUV;
//       layout(location=2) in vec4 aCol;
//       uniform mat4 uProjection;
//       out vec2 vUV;
//       out vec4 vCol;
//       void main() {
//         gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
//         vUV = aUV; vCol = aCol;
//       }
//     )";
//     const char* glsl_frag = R"(
//       #version 330 core
//       in  vec2 vUV;
//       in  vec4 vCol;
//       uniform sampler2D uTex;
//       uniform float uTime;
//       out vec4 oColor;
//       void main() {
//         vec2 modUV = vUV + vec2(uTime * 0.5, 0.0);  // <-- scroll UV
//         oColor = texture(uTex, fract(modUV)) * vCol;
//       }
//     )";
//
//     NkShader shader;
//     shader.SetSourceGLSL(glsl_vert, glsl_frag);
//     shader.Compile(renderer);
//
//     // Frame loop
//     shader.SetFloat("uTime", t);
//     NkRenderStates st = NkRenderStates::Default();
//     st.shader  = &shader;
//     st.texture = &myTex;
//     target.Draw(sprite, st);
//
// SOURCES PAR BACKEND
//   - GLSL  : OpenGL desktop 3.3+, OpenGL ES 3.0+ (Android/Harmony/Web).
//   - HLSL  : DX11 (sm5), DX12 (sm5_1).
//   - MSL   : Metal 2.0+ (macOS/iOS) — non livre encore (NkMetalRenderer2D
//             absent a 2026-05-30).
//   - SPIRV : binaire pre-compile pour Vulkan (uint32_t* + size). L'utilisateur
//             peut le generer hors-engine (glslangValidator/shaderc).
//
//   Si la source pour le backend actif n'est pas fournie, Compile() retourne
//   false et IsValid() reste a false. Pas de fallback (l'app sait quels
//   backends elle vise).
//
// UNIFORMS
//   Set apres Compile(). L'identification est par nom string (resolu par le
//   backend a la premiere ecriture, cache ulterieurement). Types courants :
//   float, vec2/3/4, mat4, sampler2D (NkTexture).
//
//   Limitation : pas d'introspection (pas de GetUniformList) pour rester
//   minimaliste. L'utilisateur connait son shader.
//
// LIFECYCLE
//   - Construction : etat invalide, pas de GPU id.
//   - SetSource* : stocke le source ASCII.
//   - Compile(renderer) : appelle le dispatch backend Create avec les
//     sources stockees ; alloue le GPU id (uint32). Renvoie false sur
//     echec de compilation/link (les erreurs sont loggees par le backend).
//   - Destroy / dtor : libere via dispatch backend Destroy.
//   - Non-copiable (handle GPU). Movable.
// =============================================================================

#include "NKCore/NkTypes.h"
#include "NKCanvas/Renderer/Core/NkRenderer2DTypes.h"

namespace nkentseu {
    namespace renderer {

        class NkIRenderer2D;
        class NkTexture;
        class NkTransform;

        /// Etage de shader (vertex / fragment). Pas de geometry/compute pour
        /// l'instant (NKCanvas vise le 2D, NKRHI fournit le pipeline complet).
        enum class NkShaderStage : uint8 {
            NK_VERTEX   = 0,
            NK_FRAGMENT = 1,
        };

        class NkShader {
            public:
                NkShader() noexcept = default;
                ~NkShader() { Destroy(); }

                // Non-copyable (GPU resource).
                NkShader(const NkShader&)            = delete;
                NkShader& operator=(const NkShader&) = delete;

                // Movable.
                NkShader(NkShader&& o) noexcept;
                NkShader& operator=(NkShader&& o) noexcept;

                // ── Sources par langage (set au moins une avant Compile) ────────
                /// GLSL pour OpenGL/GLES. Donner les deux sources (vert + frag).
                void SetSourceGLSL(const char* vertexSrc, const char* fragmentSrc);
                /// HLSL pour DX11/DX12 (sm5 / sm5_1).
                void SetSourceHLSL(const char* vertexSrc, const char* fragmentSrc);
                /// MSL pour Metal (macOS/iOS).
                void SetSourceMSL(const char* vertexSrc, const char* fragmentSrc);
                /// SPIR-V binaire pre-compile pour Vulkan (uint32_t words).
                void SetSourceSPIRV(const uint32* vertexCode, usize vertexWords,
                                    const uint32* fragmentCode, usize fragmentWords);

                /// Compile + link via le dispatch backend du renderer fourni.
                /// Le renderer doit avoir appele NkShaderSetBackend() a son Init.
                bool Compile(NkIRenderer2D& renderer);

                /// Libere le programme GPU (idempotent).
                void Destroy();

                // ── Uniformes (set apres Compile, avant Draw) ──────────────────
                void SetFloat(const char* name, float32 v);
                void SetVec2 (const char* name, NkVec2f v);
                void SetVec3 (const char* name, float32 x, float32 y, float32 z);
                void SetVec4 (const char* name, float32 x, float32 y, float32 z, float32 w);
                void SetColor(const char* name, const NkColor2D& c);     // RGBA normalise [0..1]
                void SetMat4 (const char* name, const NkTransform& t);
                void SetMat4 (const char* name, const float32* mat16);   // raw 16 floats column-major
                void SetTexture(const char* name, const NkTexture* tex, uint32 slot = 0);

                // ── Etat ────────────────────────────────────────────────────────
                bool   IsValid() const noexcept { return mGPUId != 0; }
                uint32 GetGPUId() const noexcept { return mGPUId; }

            private:
                // Sources stockees ASCII (selectionnees par le backend a Compile).
                // Pas de NkString pour eviter une dep sur NKContainers ici (header).
                const char* mGLSLVert{nullptr};
                const char* mGLSLFrag{nullptr};
                const char* mHLSLVert{nullptr};
                const char* mHLSLFrag{nullptr};
                const char* mMSLVert {nullptr};
                const char* mMSLFrag {nullptr};
                const uint32* mSPIRVVert{nullptr};
                usize         mSPIRVVertWords{0};
                const uint32* mSPIRVFrag{nullptr};
                usize         mSPIRVFragWords{0};

                uint32 mGPUId{0};  ///< Handle backend (programme GL, ID3D11VertexShader+PS, etc.)
        };

    } // namespace renderer
} // namespace nkentseu
