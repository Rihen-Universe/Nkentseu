#pragma once
// ============================================================================
// NkGizmoShaders.h
// Shaders pour le rendu GPU des gizmos 3D (lignes épaisses, cercles, etc.)
// Compatible OpenGL, Vulkan, DirectX 11, DirectX 12
// ============================================================================

namespace nkentseu {
    namespace gizmoshaders {

        // ----------------------------------------------------------------------------
        // 1. OpenGL (GLSL 4.30)
        // ----------------------------------------------------------------------------
        static const char* kVertexGL_GLSL = R"(
        #version 430 core
        layout(location = 0) in vec3 a_pos;
        layout(location = 1) in vec3 a_color;
        uniform mat4 u_mvp;
        out vec3 v_color;
        void main() {
            v_color = a_color;
            gl_Position = u_mvp * vec4(a_pos, 1.0);
        }
        )";

        static const char* kFragmentGL_GLSL = R"(
        #version 430 core
        in vec3 v_color;
        out vec4 fragColor;
        void main() {
            fragColor = vec4(v_color, 1.0);
        }
        )";

        // ----------------------------------------------------------------------------
        // 2. Vulkan (GLSL 4.50 avec push constants)
        // ----------------------------------------------------------------------------
        static const char* kVertexVK_GLSL = R"(
        #version 450 core
        layout(location = 0) in vec3 a_pos;
        layout(location = 1) in vec3 a_color;
        layout(push_constant) uniform PushConsts {
            mat4 mvp;
        } pc;
        layout(location = 0) out vec3 v_color;
        void main() {
            v_color = a_color;
            gl_Position = pc.mvp * vec4(a_pos, 1.0);
        }
        )";

        static const char* kFragmentVK_GLSL = R"(
        #version 450 core
        layout(location = 0) in vec3 v_color;
        layout(location = 0) out vec4 fragColor;
        void main() {
            fragColor = vec4(v_color, 1.0);
        }
        )";

        // ----------------------------------------------------------------------------
        // 3. DirectX 11 (HLSL)
        // ----------------------------------------------------------------------------
        static const char* kVertexDX11_HLSL = R"(
        cbuffer MVP : register(b0) {
            float4x4 u_mvp;
        };
        struct VSInput {
            float3 pos : POSITION;
            float3 col : COLOR;
        };
        struct VSOutput {
            float4 pos : SV_POSITION;
            float3 col : COLOR;
        };
        VSOutput main(VSInput input) {
            VSOutput output;
            output.pos = mul(u_mvp, float4(input.pos, 1.0));
            output.col = input.col;
            return output;
        }
        )";

        static const char* kFragmentDX11_HLSL = R"(
        struct PSInput {
            float4 pos : SV_POSITION;
            float3 col : COLOR;
        };
        float4 main(PSInput input) : SV_TARGET {
            return float4(input.col, 1.0);
        }
        )";

        // ----------------------------------------------------------------------------
        // 4. DirectX 12 (HLSL avec root signature implicite)
        // ----------------------------------------------------------------------------
        static const char* kVertexDX12_HLSL = R"(
        struct VSInput {
            float3 pos : POSITION;
            float3 col : COLOR;
        };
        struct VSOutput {
            float4 pos : SV_POSITION;
            float3 col : COLOR;
        };
        ConstantBuffer<matrix> u_mvp : register(b0, space0);
        VSOutput main(VSInput input) {
            VSOutput output;
            output.pos = mul(u_mvp, float4(input.pos, 1.0));
            output.col = input.col;
            return output;
        }
        )";

        static const char* kFragmentDX12_HLSL = R"(
        struct PSInput {
            float4 pos : SV_POSITION;
            float3 col : COLOR;
        };
        float4 main(PSInput input) : SV_TARGET {
            return float4(input.col, 1.0);
        }
        )";

    } // namespace gizmo_shaders
} // namespace nkentseu