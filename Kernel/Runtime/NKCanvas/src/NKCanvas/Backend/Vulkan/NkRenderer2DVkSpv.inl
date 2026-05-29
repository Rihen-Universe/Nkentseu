#pragma once
// =============================================================================
// NkRenderer2DVkSpv.inl — SPIR-V shaders for NkVulkanRenderer2D
//
// ── OPTION A : Pré-compilé (recommandé en production) ───────────────────────
// Exécuter scripts/gen_spv.bat (Windows) ou scripts/gen_spv.sh (Linux/macOS)
// pour générer ce fichier avec les vrais tableaux SPIR-V.
//
// ── OPTION B : Compilation runtime (fallback si pas de SPIR-V pré-compilé) ──
// Définir NK_VK2D_RUNTIME_COMPILE avant d'inclure ce header.
// Nécessite shaderc (libshaderc) ou glslang dans le projet.
// =============================================================================

#include "NKCore/NkTypes.h"

namespace nkentseu {

    // =============================================================================
    // GLSL SOURCES (stockées inline pour compilation runtime)
    // =============================================================================

    static const char* kVk2DVertGLSL = R"GLSL(
    #version 450
    
    layout(push_constant) uniform PC { mat4 proj; } u_PC;

    layout(location = 0) in vec2  a_Pos;
    layout(location = 1) in vec2  a_UV;
    layout(location = 2) in vec4  a_Color;

    layout(location = 0) out vec2 v_UV;
    layout(location = 1) out vec4 v_Color;

    void main() {
        v_UV    = a_UV;
        v_Color = a_Color;
        gl_Position = u_PC.proj * vec4(a_Pos, 0.0, 1.0);
    }
    )GLSL";

    static const char* kVk2DFragGLSL = R"GLSL(
    #version 450

    layout(set = 0, binding = 0) uniform sampler2D u_Tex;

    layout(location = 0) in  vec2 v_UV;
    layout(location = 1) in  vec4 v_Color;

    layout(location = 0) out vec4 out_Color;

    void main() {
        out_Color = texture(u_Tex, v_UV) * v_Color;
    }
    )GLSL";

    // =============================================================================
    // NkRenderer2DVkSpv.inl — SPIR-V pré-assemblé pour NkVulkanRenderer2D
    // Généré par gen_spirv.py (assemblage manuel des opcodes SPIR-V)
    // =============================================================================
    #define NK_VK2D_PRECOMPILED_SPV 1

    static const uint32_t kVk2DVertSpv[] = {
        0x07230203u, 0x00010000u, 0x00080001u, 0x00000037u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0003000Eu,
        0x00000000u, 0x00000001u, 0x000B000Fu, 0x00000000u, 0x0000001Du, 0x6E69616Du, 0x00000000u, 0x0000000Fu,
        0x00000010u, 0x00000011u, 0x00000012u, 0x00000013u, 0x00000016u, 0x00020013u, 0x00000001u, 0x00030021u,
        0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u, 0x00000020u, 0x00040015u, 0x00000019u, 0x00000020u,
        0x00000001u, 0x00040017u, 0x00000004u, 0x00000003u, 0x00000002u, 0x00040017u, 0x00000005u, 0x00000003u,
        0x00000004u, 0x00040020u, 0x00000006u, 0x00000005u, 0x00000004u, 0x0003001Eu, 0x00000007u, 0x00000006u,
        0x0003001Eu, 0x00000014u, 0x00000005u, 0x00040020u, 0x00000008u, 0x00000009u, 0x00000007u, 0x00040020u,
        0x00000015u, 0x00000003u, 0x00000014u, 0x00040020u, 0x0000000Au, 0x00000001u, 0x00000004u, 0x00040020u,
        0x0000000Bu, 0x00000001u, 0x00000004u, 0x00040020u, 0x0000000Cu, 0x00000001u, 0x00000005u, 0x00040020u,
        0x0000000Du, 0x00000003u, 0x00000004u, 0x00040020u, 0x0000000Eu, 0x00000003u, 0x00000005u, 0x00040020u,
        0x0000001Fu, 0x00000003u, 0x00000005u, 0x00040020u, 0x00000017u, 0x00000009u, 0x00000006u, 0x0004002Bu,
        0x00000019u, 0x0000001Au, 0x00000000u, 0x0004002Bu, 0x00000003u, 0x0000001Bu, 0x00000000u, 0x0004002Bu,
        0x00000003u, 0x0000001Cu, 0x3F800000u, 0x0004003Bu, 0x00000008u, 0x00000009u, 0x00000009u, 0x0004003Bu,
        0x00000015u, 0x00000016u, 0x00000003u, 0x0004003Bu, 0x0000000Au, 0x0000000Fu, 0x00000001u, 0x0004003Bu,
        0x0000000Bu, 0x00000010u, 0x00000001u, 0x0004003Bu, 0x0000000Cu, 0x00000011u, 0x00000001u, 0x0004003Bu,
        0x0000000Du, 0x00000012u, 0x00000003u, 0x0004003Bu, 0x0000000Eu, 0x00000013u, 0x00000003u, 0x00030047u,
        0x00000007u, 0x00000002u, 0x00050048u, 0x00000007u, 0x00000000u, 0x00000023u, 0x00000000u, 0x00040048u,
        0x00000007u, 0x00000000u, 0x00000005u, 0x00050048u, 0x00000007u, 0x00000000u, 0x00000007u, 0x00000010u,
        0x00030047u, 0x00000014u, 0x00000002u, 0x00050048u, 0x00000014u, 0x00000000u, 0x0000000Bu, 0x00000000u,
        0x00040047u, 0x0000000Fu, 0x0000001Eu, 0x00000000u, 0x00040047u, 0x00000010u, 0x0000001Eu, 0x00000001u,
        0x00040047u, 0x00000011u, 0x0000001Eu, 0x00000002u, 0x00040047u, 0x00000012u, 0x0000001Eu, 0x00000000u,
        0x00040047u, 0x00000013u, 0x0000001Eu, 0x00000001u, 0x00050036u, 0x00000001u, 0x0000001Du, 0x00000000u,
        0x00000002u, 0x000200F8u, 0x0000001Eu, 0x0004003Du, 0x00000004u, 0x00000028u, 0x0000000Fu, 0x0004003Du,
        0x00000004u, 0x00000029u, 0x00000010u, 0x0004003Du, 0x00000005u, 0x0000002Au, 0x00000011u, 0x0003003Eu,
        0x00000012u, 0x00000029u, 0x0003003Eu, 0x00000013u, 0x0000002Au, 0x00050041u, 0x00000017u, 0x0000002Bu,
        0x00000009u, 0x0000001Au, 0x0004003Du, 0x00000006u, 0x0000002Cu, 0x0000002Bu, 0x00050051u, 0x00000003u,
        0x0000002Eu, 0x00000028u, 0x00000000u, 0x00050051u, 0x00000003u, 0x0000002Fu, 0x00000028u, 0x00000001u,
        0x00070050u, 0x00000005u, 0x0000002Du, 0x0000002Eu, 0x0000002Fu, 0x0000001Bu, 0x0000001Cu, 0x00050091u,
        0x00000005u, 0x00000030u, 0x0000002Cu, 0x0000002Du, 0x00050041u, 0x0000001Fu, 0x00000031u, 0x00000016u,
        0x0000001Au, 0x0003003Eu, 0x00000031u, 0x00000030u, 0x000100FDu, 0x00010038u,
    };
    static const uint32_t kVk2DVertSpvWordCount = 238u;

    static const uint32_t kVk2DFragSpv[] = {
        0x07230203u, 0x00010000u, 0x00080001u, 0x0000001Eu, 0x00000000u, 0x00020011u, 0x00000001u, 0x0003000Eu,
        0x00000000u, 0x00000001u, 0x0008000Fu, 0x00000004u, 0x0000000Fu, 0x6E69616Du, 0x00000000u, 0x0000000Cu,
        0x0000000Du, 0x0000000Eu, 0x00030010u, 0x0000000Fu, 0x00000007u, 0x00020013u, 0x00000001u, 0x00030021u,
        0x00000002u, 0x00000001u, 0x00030016u, 0x00000003u, 0x00000020u, 0x00040017u, 0x00000004u, 0x00000003u,
        0x00000002u, 0x00040017u, 0x00000005u, 0x00000003u, 0x00000004u, 0x00090019u, 0x00000006u, 0x00000003u,
        0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000001u, 0x00000000u, 0x0003001Bu, 0x00000007u,
        0x00000006u, 0x00040020u, 0x00000008u, 0x00000000u, 0x00000007u, 0x00040020u, 0x0000000Au, 0x00000001u,
        0x00000004u, 0x00040020u, 0x0000000Bu, 0x00000001u, 0x00000005u, 0x00040020u, 0x00000011u, 0x00000003u,
        0x00000005u, 0x0004003Bu, 0x00000008u, 0x00000009u, 0x00000000u, 0x0004003Bu, 0x0000000Au, 0x0000000Cu,
        0x00000001u, 0x0004003Bu, 0x0000000Bu, 0x0000000Du, 0x00000001u, 0x0004003Bu, 0x00000011u, 0x0000000Eu,
        0x00000003u, 0x00040047u, 0x00000009u, 0x00000022u, 0x00000000u, 0x00040047u, 0x00000009u, 0x00000021u,
        0x00000000u, 0x00040047u, 0x0000000Cu, 0x0000001Eu, 0x00000000u, 0x00040047u, 0x0000000Du, 0x0000001Eu,
        0x00000001u, 0x00040047u, 0x0000000Eu, 0x0000001Eu, 0x00000000u, 0x00050036u, 0x00000001u, 0x0000000Fu,
        0x00000000u, 0x00000002u, 0x000200F8u, 0x00000010u, 0x0004003Du, 0x00000007u, 0x00000012u, 0x00000009u,
        0x0004003Du, 0x00000004u, 0x00000013u, 0x0000000Cu, 0x0004003Du, 0x00000005u, 0x00000014u, 0x0000000Du,
        0x00050057u, 0x00000005u, 0x00000015u, 0x00000012u, 0x00000013u, 0x00050085u, 0x00000005u, 0x00000016u,
        0x00000015u, 0x00000014u, 0x0003003Eu, 0x0000000Eu, 0x00000016u, 0x000100FDu, 0x00010038u,
    };
    static const uint32_t kVk2DFragSpvWordCount = 135u;

}