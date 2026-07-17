/*
    * NkGuiRHIBackend.cpp
    *
    * Implementation du backend NKRHI reutilisable pour les draw lists NKGui.
    * Porte de Integrations/NKUI/NkUIRHIBackend.cpp (meme pipeline/buffers/upload/
    * scissor/conversion). Adapte a NkGuiDrawList : UNE liste, clipRect par commande.
    * Allocations CPU temporaires via NKMemory (regle dure : zero new/delete brut).
*/

#include "NKGui/NkGuiRHIBackend.h"

#include "NKSL/NKSL.h"
#include "NKLogger/NkLog.h"
#include "NKMemory/NkAllocator.h"
#include <cstring>
#include <cmath>

namespace nkentseu {
    namespace nkgui {

        static constexpr const char* kVertGLSL = R"GLSL(
#version 460 core
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(std140, binding=1) uniform ViewportBlock {
    vec2 uViewport;
    vec2 _pad;
};
out vec2  vUV;
out vec4  vColor;
void main() {
    vColor = vec4(
        float((aColor >>  0u) & 0xFFu) / 255.0,
        float((aColor >>  8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );
    vUV = aUV;
    vec2 ndc = (aPos / uViewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)GLSL";

        static constexpr const char* kFragGLSL = R"GLSL(
#version 460 core
in  vec2 vUV;
in  vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D uTex;
void main() {
    vec4 tc = vec4(1.0);
    if (vUV.x >= 0.0 && vUV.y >= 0.0) {
        tc = texture(uTex, vUV);
    }
    fragColor = vColor * tc;
}
)GLSL";

        static constexpr const char* kVertVk = R"GLSL(
#version 460
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(set=0, binding=1, std140) uniform ViewportBlock {
    vec2 uViewport;
    vec2 _pad;
} vpb;
layout(location=0) out vec2  vUV;
layout(location=1) out vec4  vColor;
void main() {
    vColor = vec4(
        float((aColor >>  0u) & 0xFFu) / 255.0,
        float((aColor >>  8u) & 0xFFu) / 255.0,
        float((aColor >> 16u) & 0xFFu) / 255.0,
        float((aColor >> 24u) & 0xFFu) / 255.0
    );
    vUV = aUV;
    vec2 ndc = (aPos / vpb.uViewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);
}
)GLSL";

        static constexpr const char* kFragVk = R"GLSL(
#version 460
layout(location=0) in  vec2 vUV;
layout(location=1) in  vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(set=0, binding=0) uniform sampler2D uTex;
void main() {
    vec4 tc = vec4(1.0);
    if (vUV.x >= 0.0 && vUV.y >= 0.0) {
        tc = texture(uTex, vUV);
    }
    vec4 color = vColor * tc;
    color.rgb = pow(color.rgb, vec3(2.2));
    fragColor = color;
}
)GLSL";

        static constexpr const char* kVertHlslDx11 = R"HLSL(
cbuffer Constants : register(b1) { float2 uViewport; float2 _pad; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; uint col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 c;
    c.r = float((v.col >>  0u) & 0xFFu) / 255.0;
    c.g = float((v.col >>  8u) & 0xFFu) / 255.0;
    c.b = float((v.col >> 16u) & 0xFFu) / 255.0;
    c.a = float((v.col >> 24u) & 0xFFu) / 255.0;
    o.col = c;
    float2 ndc = (v.pos / uViewport) * 2.0 - 1.0;
    o.pos = float4(ndc.x, -ndc.y, 0.0, 1.0);
    o.uv  = v.uv;
    return o;
}
)HLSL";

        static constexpr const char* kFragHlslDx11 = R"HLSL(
Texture2D    uTex     : register(t0);
SamplerState uSampler : register(s0);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tc = float4(1.0, 1.0, 1.0, 1.0);
    if (i.uv.x >= 0.0 && i.uv.y >= 0.0) {
        tc = uTex.Sample(uSampler, i.uv);
    }
    return i.col * tc;
}
)HLSL";

        static constexpr const char* kVertHlslDx12 = R"HLSL(
cbuffer Constants : register(b0) { float2 uViewport; float2 _pad; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; uint col:COLOR; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 c;
    c.r = float((v.col >>  0u) & 0xFFu) / 255.0;
    c.g = float((v.col >>  8u) & 0xFFu) / 255.0;
    c.b = float((v.col >> 16u) & 0xFFu) / 255.0;
    c.a = float((v.col >> 24u) & 0xFFu) / 255.0;
    o.col = c;
    float2 ndc = (v.pos / uViewport) * 2.0 - 1.0;
    o.pos = float4(ndc.x, -ndc.y, 0.0, 1.0);
    o.uv  = v.uv;
    return o;
}
)HLSL";

        static constexpr const char* kFragHlslDx12 = R"HLSL(
Texture2D    uTex     : register(t1);
SamplerState uSampler : register(s1);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 PSMain(PSIn i) : SV_Target {
    float4 tc = float4(1.0, 1.0, 1.0, 1.0);
    if (i.uv.x >= 0.0 && i.uv.y >= 0.0) {
        tc = uTex.Sample(uSampler, i.uv);
    }
    return i.col * tc;
}
)HLSL";

        // SPIR-V PRÉ-COMPILÉ de kVertVk/kFragVk (glslangValidator). Utilisé au lieu de
        // CompileVkSpirv : la compilation glslang au RUNTIME corrompt le heap sous
        // clang-mingw (crash Vulkan à l'init de ce backend — reproduit par la démo
        // Model). Régénérer si kVertVk/kFragVk changent.
        static const uint32 kNkGuiVkVertSpv[516] = {
            0x07230203, 0x00010000, 0x0008000b, 0x0000004e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
            0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
            0x000b000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000c, 0x0000002a,
            0x0000002c, 0x00000030, 0x00000044, 0x00030003, 0x00000002, 0x000001cc, 0x00040005, 0x00000004,
            0x6e69616d, 0x00000000, 0x00040005, 0x00000009, 0x6c6f4376, 0x0000726f, 0x00040005, 0x0000000c,
            0x6c6f4361, 0x0000726f, 0x00030005, 0x0000002a, 0x00565576, 0x00030005, 0x0000002c, 0x00565561,
            0x00030005, 0x0000002f, 0x0063646e, 0x00040005, 0x00000030, 0x736f5061, 0x00000000, 0x00060005,
            0x00000032, 0x77656956, 0x74726f70, 0x636f6c42, 0x0000006b, 0x00060006, 0x00000032, 0x00000000,
            0x65695675, 0x726f7077, 0x00000074, 0x00050006, 0x00000032, 0x00000001, 0x6461705f, 0x00000000,
            0x00030005, 0x00000034, 0x00627076, 0x00060005, 0x00000042, 0x505f6c67, 0x65567265, 0x78657472,
            0x00000000, 0x00060006, 0x00000042, 0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00070006,
            0x00000042, 0x00000001, 0x505f6c67, 0x746e696f, 0x657a6953, 0x00000000, 0x00070006, 0x00000042,
            0x00000002, 0x435f6c67, 0x4470696c, 0x61747369, 0x0065636e, 0x00070006, 0x00000042, 0x00000003,
            0x435f6c67, 0x446c6c75, 0x61747369, 0x0065636e, 0x00030005, 0x00000044, 0x00000000, 0x00040047,
            0x00000009, 0x0000001e, 0x00000001, 0x00040047, 0x0000000c, 0x0000001e, 0x00000002, 0x00040047,
            0x0000002a, 0x0000001e, 0x00000000, 0x00040047, 0x0000002c, 0x0000001e, 0x00000001, 0x00040047,
            0x00000030, 0x0000001e, 0x00000000, 0x00030047, 0x00000032, 0x00000002, 0x00050048, 0x00000032,
            0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x00000032, 0x00000001, 0x00000023, 0x00000008,
            0x00040047, 0x00000034, 0x00000021, 0x00000001, 0x00040047, 0x00000034, 0x00000022, 0x00000000,
            0x00030047, 0x00000042, 0x00000002, 0x00050048, 0x00000042, 0x00000000, 0x0000000b, 0x00000000,
            0x00050048, 0x00000042, 0x00000001, 0x0000000b, 0x00000001, 0x00050048, 0x00000042, 0x00000002,
            0x0000000b, 0x00000003, 0x00050048, 0x00000042, 0x00000003, 0x0000000b, 0x00000004, 0x00020013,
            0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017,
            0x00000007, 0x00000006, 0x00000004, 0x00040020, 0x00000008, 0x00000003, 0x00000007, 0x0004003b,
            0x00000008, 0x00000009, 0x00000003, 0x00040015, 0x0000000a, 0x00000020, 0x00000000, 0x00040020,
            0x0000000b, 0x00000001, 0x0000000a, 0x0004003b, 0x0000000b, 0x0000000c, 0x00000001, 0x0004002b,
            0x0000000a, 0x0000000e, 0x00000000, 0x0004002b, 0x0000000a, 0x00000010, 0x000000ff, 0x0004002b,
            0x00000006, 0x00000013, 0x437f0000, 0x0004002b, 0x0000000a, 0x00000016, 0x00000008, 0x0004002b,
            0x0000000a, 0x0000001c, 0x00000010, 0x0004002b, 0x0000000a, 0x00000022, 0x00000018, 0x00040017,
            0x00000028, 0x00000006, 0x00000002, 0x00040020, 0x00000029, 0x00000003, 0x00000028, 0x0004003b,
            0x00000029, 0x0000002a, 0x00000003, 0x00040020, 0x0000002b, 0x00000001, 0x00000028, 0x0004003b,
            0x0000002b, 0x0000002c, 0x00000001, 0x00040020, 0x0000002e, 0x00000007, 0x00000028, 0x0004003b,
            0x0000002b, 0x00000030, 0x00000001, 0x0004001e, 0x00000032, 0x00000028, 0x00000028, 0x00040020,
            0x00000033, 0x00000002, 0x00000032, 0x0004003b, 0x00000033, 0x00000034, 0x00000002, 0x00040015,
            0x00000035, 0x00000020, 0x00000001, 0x0004002b, 0x00000035, 0x00000036, 0x00000000, 0x00040020,
            0x00000037, 0x00000002, 0x00000028, 0x0004002b, 0x00000006, 0x0000003b, 0x40000000, 0x0004002b,
            0x00000006, 0x0000003d, 0x3f800000, 0x0004002b, 0x0000000a, 0x00000040, 0x00000001, 0x0004001c,
            0x00000041, 0x00000006, 0x00000040, 0x0006001e, 0x00000042, 0x00000007, 0x00000006, 0x00000041,
            0x00000041, 0x00040020, 0x00000043, 0x00000003, 0x00000042, 0x0004003b, 0x00000043, 0x00000044,
            0x00000003, 0x00040020, 0x00000045, 0x00000007, 0x00000006, 0x0004002b, 0x00000006, 0x0000004b,
            0x00000000, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005,
            0x0004003b, 0x0000002e, 0x0000002f, 0x00000007, 0x0004003d, 0x0000000a, 0x0000000d, 0x0000000c,
            0x000500c2, 0x0000000a, 0x0000000f, 0x0000000d, 0x0000000e, 0x000500c7, 0x0000000a, 0x00000011,
            0x0000000f, 0x00000010, 0x00040070, 0x00000006, 0x00000012, 0x00000011, 0x00050088, 0x00000006,
            0x00000014, 0x00000012, 0x00000013, 0x0004003d, 0x0000000a, 0x00000015, 0x0000000c, 0x000500c2,
            0x0000000a, 0x00000017, 0x00000015, 0x00000016, 0x000500c7, 0x0000000a, 0x00000018, 0x00000017,
            0x00000010, 0x00040070, 0x00000006, 0x00000019, 0x00000018, 0x00050088, 0x00000006, 0x0000001a,
            0x00000019, 0x00000013, 0x0004003d, 0x0000000a, 0x0000001b, 0x0000000c, 0x000500c2, 0x0000000a,
            0x0000001d, 0x0000001b, 0x0000001c, 0x000500c7, 0x0000000a, 0x0000001e, 0x0000001d, 0x00000010,
            0x00040070, 0x00000006, 0x0000001f, 0x0000001e, 0x00050088, 0x00000006, 0x00000020, 0x0000001f,
            0x00000013, 0x0004003d, 0x0000000a, 0x00000021, 0x0000000c, 0x000500c2, 0x0000000a, 0x00000023,
            0x00000021, 0x00000022, 0x000500c7, 0x0000000a, 0x00000024, 0x00000023, 0x00000010, 0x00040070,
            0x00000006, 0x00000025, 0x00000024, 0x00050088, 0x00000006, 0x00000026, 0x00000025, 0x00000013,
            0x00070050, 0x00000007, 0x00000027, 0x00000014, 0x0000001a, 0x00000020, 0x00000026, 0x0003003e,
            0x00000009, 0x00000027, 0x0004003d, 0x00000028, 0x0000002d, 0x0000002c, 0x0003003e, 0x0000002a,
            0x0000002d, 0x0004003d, 0x00000028, 0x00000031, 0x00000030, 0x00050041, 0x00000037, 0x00000038,
            0x00000034, 0x00000036, 0x0004003d, 0x00000028, 0x00000039, 0x00000038, 0x00050088, 0x00000028,
            0x0000003a, 0x00000031, 0x00000039, 0x0005008e, 0x00000028, 0x0000003c, 0x0000003a, 0x0000003b,
            0x00050050, 0x00000028, 0x0000003e, 0x0000003d, 0x0000003d, 0x00050083, 0x00000028, 0x0000003f,
            0x0000003c, 0x0000003e, 0x0003003e, 0x0000002f, 0x0000003f, 0x00050041, 0x00000045, 0x00000046,
            0x0000002f, 0x0000000e, 0x0004003d, 0x00000006, 0x00000047, 0x00000046, 0x00050041, 0x00000045,
            0x00000048, 0x0000002f, 0x00000040, 0x0004003d, 0x00000006, 0x00000049, 0x00000048, 0x0004007f,
            0x00000006, 0x0000004a, 0x00000049, 0x00070050, 0x00000007, 0x0000004c, 0x00000047, 0x0000004a,
            0x0000004b, 0x0000003d, 0x00050041, 0x00000008, 0x0000004d, 0x00000044, 0x00000036, 0x0003003e,
            0x0000004d, 0x0000004c, 0x000100fd, 0x00010038,
        };
        static const uint32 kNkGuiVkFragSpv[374] = {
            0x07230203, 0x00010000, 0x0008000b, 0x0000003e, 0x00000000, 0x00020011, 0x00000001, 0x0006000b,
            0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e, 0x00000000, 0x00000001,
            0x0008000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000f, 0x00000029, 0x0000003c,
            0x00030010, 0x00000004, 0x00000007, 0x00030003, 0x00000002, 0x000001cc, 0x00040005, 0x00000004,
            0x6e69616d, 0x00000000, 0x00030005, 0x00000009, 0x00006374, 0x00030005, 0x0000000f, 0x00565576,
            0x00040005, 0x00000023, 0x78655475, 0x00000000, 0x00040005, 0x00000027, 0x6f6c6f63, 0x00000072,
            0x00040005, 0x00000029, 0x6c6f4376, 0x0000726f, 0x00050005, 0x0000003c, 0x67617266, 0x6f6c6f43,
            0x00000072, 0x00040047, 0x0000000f, 0x0000001e, 0x00000000, 0x00040047, 0x00000023, 0x00000021,
            0x00000000, 0x00040047, 0x00000023, 0x00000022, 0x00000000, 0x00040047, 0x00000029, 0x0000001e,
            0x00000001, 0x00040047, 0x0000003c, 0x0000001e, 0x00000000, 0x00020013, 0x00000002, 0x00030021,
            0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006,
            0x00000004, 0x00040020, 0x00000008, 0x00000007, 0x00000007, 0x0004002b, 0x00000006, 0x0000000a,
            0x3f800000, 0x0007002c, 0x00000007, 0x0000000b, 0x0000000a, 0x0000000a, 0x0000000a, 0x0000000a,
            0x00020014, 0x0000000c, 0x00040017, 0x0000000d, 0x00000006, 0x00000002, 0x00040020, 0x0000000e,
            0x00000001, 0x0000000d, 0x0004003b, 0x0000000e, 0x0000000f, 0x00000001, 0x00040015, 0x00000010,
            0x00000020, 0x00000000, 0x0004002b, 0x00000010, 0x00000011, 0x00000000, 0x00040020, 0x00000012,
            0x00000001, 0x00000006, 0x0004002b, 0x00000006, 0x00000015, 0x00000000, 0x0004002b, 0x00000010,
            0x00000019, 0x00000001, 0x00090019, 0x00000020, 0x00000006, 0x00000001, 0x00000000, 0x00000000,
            0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x00000021, 0x00000020, 0x00040020, 0x00000022,
            0x00000000, 0x00000021, 0x0004003b, 0x00000022, 0x00000023, 0x00000000, 0x00040020, 0x00000028,
            0x00000001, 0x00000007, 0x0004003b, 0x00000028, 0x00000029, 0x00000001, 0x00040017, 0x0000002d,
            0x00000006, 0x00000003, 0x0004002b, 0x00000006, 0x00000030, 0x400ccccd, 0x0006002c, 0x0000002d,
            0x00000031, 0x00000030, 0x00000030, 0x00000030, 0x00040020, 0x00000033, 0x00000007, 0x00000006,
            0x0004002b, 0x00000010, 0x00000038, 0x00000002, 0x00040020, 0x0000003b, 0x00000003, 0x00000007,
            0x0004003b, 0x0000003b, 0x0000003c, 0x00000003, 0x00050036, 0x00000002, 0x00000004, 0x00000000,
            0x00000003, 0x000200f8, 0x00000005, 0x0004003b, 0x00000008, 0x00000009, 0x00000007, 0x0004003b,
            0x00000008, 0x00000027, 0x00000007, 0x0003003e, 0x00000009, 0x0000000b, 0x00050041, 0x00000012,
            0x00000013, 0x0000000f, 0x00000011, 0x0004003d, 0x00000006, 0x00000014, 0x00000013, 0x000500be,
            0x0000000c, 0x00000016, 0x00000014, 0x00000015, 0x000300f7, 0x00000018, 0x00000000, 0x000400fa,
            0x00000016, 0x00000017, 0x00000018, 0x000200f8, 0x00000017, 0x00050041, 0x00000012, 0x0000001a,
            0x0000000f, 0x00000019, 0x0004003d, 0x00000006, 0x0000001b, 0x0000001a, 0x000500be, 0x0000000c,
            0x0000001c, 0x0000001b, 0x00000015, 0x000200f9, 0x00000018, 0x000200f8, 0x00000018, 0x000700f5,
            0x0000000c, 0x0000001d, 0x00000016, 0x00000005, 0x0000001c, 0x00000017, 0x000300f7, 0x0000001f,
            0x00000000, 0x000400fa, 0x0000001d, 0x0000001e, 0x0000001f, 0x000200f8, 0x0000001e, 0x0004003d,
            0x00000021, 0x00000024, 0x00000023, 0x0004003d, 0x0000000d, 0x00000025, 0x0000000f, 0x00050057,
            0x00000007, 0x00000026, 0x00000024, 0x00000025, 0x0003003e, 0x00000009, 0x00000026, 0x000200f9,
            0x0000001f, 0x000200f8, 0x0000001f, 0x0004003d, 0x00000007, 0x0000002a, 0x00000029, 0x0004003d,
            0x00000007, 0x0000002b, 0x00000009, 0x00050085, 0x00000007, 0x0000002c, 0x0000002a, 0x0000002b,
            0x0003003e, 0x00000027, 0x0000002c, 0x0004003d, 0x00000007, 0x0000002e, 0x00000027, 0x0008004f,
            0x0000002d, 0x0000002f, 0x0000002e, 0x0000002e, 0x00000000, 0x00000001, 0x00000002, 0x0007000c,
            0x0000002d, 0x00000032, 0x00000001, 0x0000001a, 0x0000002f, 0x00000031, 0x00050041, 0x00000033,
            0x00000034, 0x00000027, 0x00000011, 0x00050051, 0x00000006, 0x00000035, 0x00000032, 0x00000000,
            0x0003003e, 0x00000034, 0x00000035, 0x00050041, 0x00000033, 0x00000036, 0x00000027, 0x00000019,
            0x00050051, 0x00000006, 0x00000037, 0x00000032, 0x00000001, 0x0003003e, 0x00000036, 0x00000037,
            0x00050041, 0x00000033, 0x00000039, 0x00000027, 0x00000038, 0x00050051, 0x00000006, 0x0000003a,
            0x00000032, 0x00000002, 0x0003003e, 0x00000039, 0x0000003a, 0x0004003d, 0x00000007, 0x0000003d,
            0x00000027, 0x0003003e, 0x0000003c, 0x0000003d, 0x000100fd, 0x00010038,
        };

        static NkShaderConvertResult CompileVkSpirv(const char* source, NkSLStage stage, const char* dbgName) {
            NkShaderCache& cache = NkShaderCache::Global();
            const uint64 key = NkShaderCache::ComputeKey(NkString(source), stage, "spirv");
            NkShaderConvertResult cached = cache.Load(key);
            if (cached.success && cached.SpirvWordCount() > 0 && cached.SpirvWords()[0] == 0x07230203) {
                return cached;
            }
            NkShaderConvertResult result = NkShaderConverter::GlslToSpirv(NkString(source), stage, dbgName);
            if (result.success) cache.Save(key, result);
            return result;
        }

        static uint64 NextPow2U64(uint64 v) {
            if (v <= 1ull) return 1ull;
            --v; v|=v>>1; v|=v>>2; v|=v>>4; v|=v>>8; v|=v>>16; v|=v>>32; return v + 1;
        }
        static uint64 GrowCapacityU64(uint64 current, uint64 required, uint64 minimum) {
            uint64 cap = current > minimum ? current : minimum;
            while (cap < required) { cap = cap + cap / 2; if (cap < minimum) cap = minimum; }
            return cap;
        }

        bool NkGuiRHIBackend::Init(NkIDevice* device, NkRenderPassHandle renderPass, NkGraphicsApi api) {
            if (!device) return false;
            mDevice = device; mApi = api; mRenderPass = renderPass; mBoundTexId = 0xFFFFFFFFu;
            if (!CreatePipeline())  { logger.Errorf("[NkGuiRHIBackend] pipeline failed\n");  Destroy(); return false; }
            if (!CreateResources()) { logger.Errorf("[NkGuiRHIBackend] resources failed\n"); Destroy(); return false; }
            return true;
        }

        bool NkGuiRHIBackend::CreatePipeline() {
            NkShaderDesc shaderDesc;
            shaderDesc.debugName = "NkGui_Backend";
            if (mApi == NkGraphicsApi::NK_GFX_API_OPENGL || mApi == NkGraphicsApi::NK_GFX_API_SOFTWARE) {
                shaderDesc.AddGLSL(NkShaderStage::NK_VERTEX,   kVertGLSL);
                shaderDesc.AddGLSL(NkShaderStage::NK_FRAGMENT, kFragGLSL);
            } else if (mApi == NkGraphicsApi::NK_GFX_API_DX11) {
                shaderDesc.AddHLSL(NkShaderStage::NK_VERTEX,   kVertHlslDx11, "VSMain");
                shaderDesc.AddHLSL(NkShaderStage::NK_FRAGMENT, kFragHlslDx11, "PSMain");
            } else if (mApi == NkGraphicsApi::NK_GFX_API_DX12) {
                shaderDesc.AddHLSL(NkShaderStage::NK_VERTEX,   kVertHlslDx12, "VSMain");
                shaderDesc.AddHLSL(NkShaderStage::NK_FRAGMENT, kFragHlslDx12, "PSMain");
            } else if (mApi == NkGraphicsApi::NK_GFX_API_VULKAN) {
                // SPIR-V PRÉ-COMPILÉ (kVertVk/kFragVk) : la compilation glslang au
                // runtime corrompt le heap sous clang-mingw (crash Vulkan). (void) sur
                // CompileVkSpirv pour garder la fonction référencée.
                (void)&CompileVkSpirv;
                shaderDesc.AddSPIRV(NkShaderStage::NK_VERTEX,   kNkGuiVkVertSpv, sizeof(kNkGuiVkVertSpv));
                shaderDesc.AddSPIRV(NkShaderStage::NK_FRAGMENT, kNkGuiVkFragSpv, sizeof(kNkGuiVkFragSpv));
            }
            mShader = mDevice->CreateShader(shaderDesc);
            if (!mShader.IsValid()) return false;

            NkVertexLayout layout;
            layout
                .AddAttribute(0, 0, NkGPUFormat::NK_RG32_FLOAT, 0,  "POSITION", 0)
                .AddAttribute(1, 0, NkGPUFormat::NK_RG32_FLOAT, 8,  "TEXCOORD", 0)
                .AddAttribute(2, 0, NkGPUFormat::NK_R32_UINT,   16, "COLOR",    0)
                .AddBinding(0, sizeof(NkGuiVertex));

            NkDescriptorSetLayoutDesc descLayout;
            descLayout.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
            descLayout.Add(1, NkDescriptorType::NK_UNIFORM_BUFFER,         NkShaderStage::NK_VERTEX);
            mLayout = mDevice->CreateDescriptorSetLayout(descLayout);
            if (!mLayout.IsValid()) return false;

            NkGraphicsPipelineDesc pd;
            pd.shader = mShader;
            pd.vertexLayout = layout;
            pd.topology = NkPrimitiveTopology::NK_TRIANGLE_LIST;
            pd.rasterizer = NkRasterizerDesc::NoCull();
            pd.rasterizer.scissorTest = true;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            pd.blend = NkBlendDesc::Alpha();
            pd.renderPass = mRenderPass;
            pd.debugName = "NkGui_Backend_Pipeline";
            pd.descriptorSetLayouts.PushBack(mLayout);
            mPipeline = mDevice->CreateGraphicsPipeline(pd);
            return mPipeline.IsValid();
        }

        bool NkGuiRHIBackend::CreateResources() {
            mUBO = mDevice->CreateBuffer(NkBufferDesc::Uniform(16));
            if (!mUBO.IsValid()) return false;
            if (!EnsureGeometryBuffers(kMinVBOCap, kMinIBOCap, false)) return false;

            NkTextureDesc whiteDesc = NkTextureDesc::Tex2D(1, 1, NkGPUFormat::NK_RGBA8_UNORM, 1);
            whiteDesc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
            whiteDesc.debugName = "NkGui_Backend_White";
            mWhiteTex = mDevice->CreateTexture(whiteDesc);
            if (!mWhiteTex.IsValid()) return false;
            static const uint8 kWhite[4] = {255, 255, 255, 255};
            if (!mDevice->WriteTexture(mWhiteTex, kWhite, 4)) return false;

            NkSamplerDesc samplerDesc{};
            samplerDesc.magFilter = NkFilter::NK_LINEAR;
            samplerDesc.minFilter = NkFilter::NK_LINEAR;
            samplerDesc.mipFilter = NkMipFilter::NK_NONE;
            samplerDesc.addressU = NkAddressMode::NK_CLAMP_TO_EDGE;
            samplerDesc.addressV = NkAddressMode::NK_CLAMP_TO_EDGE;
            samplerDesc.addressW = NkAddressMode::NK_CLAMP_TO_EDGE;
            mSampler = mDevice->CreateSampler(samplerDesc);
            if (!mSampler.IsValid()) return false;

            return CreateDescriptorSetForTexture(mWhiteTex, mWhiteDescSet);
        }

        bool NkGuiRHIBackend::CreateDescriptorSetForTexture(NkTextureHandle texture, NkDescSetHandle& outSet) {
            if (!mDevice || !mLayout.IsValid() || !mSampler.IsValid() || !mUBO.IsValid() || !texture.IsValid()) return false;
            NkDescSetHandle set = mDevice->AllocateDescriptorSet(mLayout);
            if (!set.IsValid()) return false;
            NkDescriptorWrite writes[2] = {};
            writes[0].set = set; writes[0].binding = 0;
            writes[0].type = NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            writes[0].texture = texture; writes[0].sampler = mSampler;
            writes[0].textureLayout = NkResourceState::NK_SHADER_READ;
            writes[1].set = set; writes[1].binding = 1;
            writes[1].type = NkDescriptorType::NK_UNIFORM_BUFFER;
            writes[1].buffer = mUBO; writes[1].bufferOffset = 0; writes[1].bufferRange = 16;
            mDevice->UpdateDescriptorSets(writes, 2);
            outSet = set;
            return true;
        }

        bool NkGuiRHIBackend::EnsureGeometryBuffers(uint64 requiredVtxBytes, uint64 requiredIdxBytes, bool allowShrink) {
            if (!mDevice) return false;
            if (requiredVtxBytes < kMinVBOCap) requiredVtxBytes = kMinVBOCap;
            if (requiredIdxBytes < kMinIBOCap) requiredIdxBytes = kMinIBOCap;
            uint64 wantedVBO = mVBOCap, wantedIBO = mIBOCap;
            if (!mVBO.IsValid() || requiredVtxBytes > mVBOCap) {
                wantedVBO = GrowCapacityU64(mVBOCap, requiredVtxBytes, kMinVBOCap);
            } else if (allowShrink && requiredVtxBytes <= (mVBOCap / 4ull)) {
                const uint64 t = NextPow2U64(requiredVtxBytes * 2ull); wantedVBO = t > kMinVBOCap ? t : kMinVBOCap;
            }
            if (!mIBO.IsValid() || requiredIdxBytes > mIBOCap) {
                wantedIBO = GrowCapacityU64(mIBOCap, requiredIdxBytes, kMinIBOCap);
            } else if (allowShrink && requiredIdxBytes <= (mIBOCap / 4ull)) {
                const uint64 t = NextPow2U64(requiredIdxBytes * 2ull); wantedIBO = t > kMinIBOCap ? t : kMinIBOCap;
            }
            if (wantedVBO != mVBOCap || !mVBO.IsValid()) {
                NkBufferHandle nv = mDevice->CreateBuffer(NkBufferDesc::VertexDynamic(wantedVBO));
                if (!nv.IsValid()) return false;
                if (mVBO.IsValid()) mDevice->DestroyBuffer(mVBO);
                mVBO = nv; mVBOCap = wantedVBO;
            }
            if (wantedIBO != mIBOCap || !mIBO.IsValid()) {
                NkBufferHandle ni = mDevice->CreateBuffer(NkBufferDesc::IndexDynamic(wantedIBO));
                if (!ni.IsValid()) return false;
                if (mIBO.IsValid()) mDevice->DestroyBuffer(mIBO);
                mIBO = ni; mIBOCap = wantedIBO;
            }
            return mVBO.IsValid() && mIBO.IsValid();
        }

        void NkGuiRHIBackend::BindTexture(NkICommandBuffer* cmd, uint32 texId) {
            if (!cmd || !mWhiteDescSet.IsValid()) return;
            const uint32 resolvedId = HasTexture(texId) ? texId : 0u;
            if (mBoundTexId == resolvedId) return;
            NkDescSetHandle set = mWhiteDescSet;
            if (resolvedId != 0) {
                const TextureEntry* entry = mTextures.Find(resolvedId);
                if (entry && entry->descSet.IsValid()) set = entry->descSet;
            }
            cmd->BindDescriptorSet(set, 0);
            mBoundTexId = resolvedId;
        }

        void NkGuiRHIBackend::RetireTextureEntry(const TextureEntry& entry) {
            if (!entry.texture.IsValid() && !entry.descSet.IsValid()) return;
            RetiredTextureEntry retired{};
            retired.entry = entry; retired.retireFrame = mFrameIndex;
            mRetiredTextures.PushBack(retired);
        }

        void NkGuiRHIBackend::CollectRetiredTextures() {
            if (!mDevice || mRetiredTextures.Empty()) return;
            usize writeIndex = 0;
            for (usize i = 0; i < mRetiredTextures.Size(); ++i) {
                RetiredTextureEntry& retired = mRetiredTextures[i];
                const uint64 age = mFrameIndex - retired.retireFrame;
                if (age >= kRetireDelayFrames) {
                    if (retired.entry.descSet.IsValid()) mDevice->FreeDescriptorSet(retired.entry.descSet);
                    if (retired.entry.owned && retired.entry.texture.IsValid()) mDevice->DestroyTexture(retired.entry.texture);
                    continue;
                }
                if (writeIndex != i) mRetiredTextures[writeIndex] = retired;
                ++writeIndex;
            }
            mRetiredTextures.Resize(writeIndex);
        }

        void NkGuiRHIBackend::Submit(NkICommandBuffer* cmd, const NkGuiDrawList& dl, uint32 fbW, uint32 fbH) {
            if (!cmd || !mPipeline.IsValid() || !mVBO.IsValid() || !mIBO.IsValid() || !mUBO.IsValid() || fbW == 0 || fbH == 0) return;
            ++mFrameIndex;
            CollectRetiredTextures();

            const uint64 vtxCount = (uint64)dl.vtx.Size();
            const uint64 idxCount = (uint64)dl.idx.Size();
            const uint64 cmdCount = (uint64)dl.cmds.Size();
            if (vtxCount == 0 || idxCount == 0 || cmdCount == 0) return;
            if (vtxCount > 0xFFFFFFFFull || idxCount > 0xFFFFFFFFull) {
                logger.Warn("[NkGuiRHIBackend] geometry overflow\n"); return;
            }

            const uint64 requiredVtxBytes = vtxCount * sizeof(NkGuiVertex);
            const uint64 requiredIdxBytes = idxCount * sizeof(uint32);
            if (requiredVtxBytes <= (mVBOCap / 4ull) && requiredIdxBytes <= (mIBOCap / 4ull)) ++mLowUsageFrames;
            else mLowUsageFrames = 0;
            const bool allowShrink = (mLowUsageFrames >= kShrinkDelayFrames);
            if (!EnsureGeometryBuffers(requiredVtxBytes, requiredIdxBytes, allowShrink)) return;
            if (allowShrink) mLowUsageFrames = 0;

            mDevice->WriteBuffer(mVBO, dl.vtx.Data(), requiredVtxBytes);
            mDevice->WriteBuffer(mIBO, dl.idx.Data(), requiredIdxBytes);
            const float32 vp[4] = {(float32)fbW, (float32)fbH, 0.f, 0.f};
            mDevice->WriteBuffer(mUBO, vp, sizeof(vp));

            cmd->BindGraphicsPipeline(mPipeline);
            cmd->BindVertexBuffer(0, mVBO);
            cmd->BindIndexBuffer(mIBO, NkIndexFormat::NK_UINT32);
            NkViewport viewport{0.f, 0.f, (float32)fbW, (float32)fbH, 0.f, 1.f};
            cmd->SetViewport(viewport);
            mBoundTexId = 0xFFFFFFFFu;

            for (uint64 ci = 0; ci < cmdCount; ++ci) {
                const NkGuiDrawCmd& dc = dl.cmds[(usize)ci];
                if (dc.idxCount == 0) continue;

                // Scissor depuis le clipRect de la commande (NKGui = top-left).
                const float32 x0 = ::fmaxf(dc.clipRect.x, 0.f);
                const float32 y0 = ::fmaxf(dc.clipRect.y, 0.f);
                const float32 x1 = ::fminf(dc.clipRect.x + dc.clipRect.w, (float32)fbW);
                const float32 y1 = ::fminf(dc.clipRect.y + dc.clipRect.h, (float32)fbH);
                const int32 clipW = (int32)::fmaxf(0.f, x1 - x0);
                const int32 clipH = (int32)::fmaxf(0.f, y1 - y0);
                if (clipW <= 0 || clipH <= 0) continue;
                NkRect2D scissor{};
                scissor.x = (int32)x0; scissor.w = clipW; scissor.h = clipH;
                if (mApi == NkGraphicsApi::NK_GFX_API_OPENGL) {
                    scissor.y = (int32)fbH - (int32)y1; if (scissor.y < 0) scissor.y = 0;
                } else {
                    scissor.y = (int32)y0;
                }
                cmd->SetScissor(scissor);

                const uint32 texId = (dc.type == NkGuiDrawCmdType::TexturedTriangles) ? dc.texId : 0u;
                BindTexture(cmd, texId);
                cmd->DrawIndexed(dc.idxCount, 1, dc.idxOffset, 0, 0);
            }
        }

        bool NkGuiRHIBackend::UploadTextureInternal(uint32 texId, const uint8* data, int32 width, int32 height, bool rgba8) {
            if (!mDevice || !data || width <= 0 || height <= 0 || texId == 0) return false;

            const uint8* uploadData = data;
            uint8* converted = nullptr;
            if (!rgba8) {
                const int32 count = width * height;
                converted = (uint8*)nkentseu::memory::NkAlloc((usize)count * 4u);
                if (!converted) return false;
                for (int32 i = 0; i < count; ++i) {
                    const uint8 a = data[i];
                    converted[i*4+0] = 255; converted[i*4+1] = 255; converted[i*4+2] = 255; converted[i*4+3] = a;
                }
                uploadData = converted;
            }

            TextureEntry* existing = mTextures.Find(texId);
            const bool canReuse = existing && existing->owned && existing->texture.IsValid()
                                && existing->descSet.IsValid() && existing->width == width && existing->height == height;
            if (canReuse) {
                const bool ok = mDevice->WriteTexture(existing->texture, uploadData, (uint32)(width * 4));
                if (converted) nkentseu::memory::NkFree(converted);
                return ok;
            }

            NkTextureDesc desc = NkTextureDesc::Tex2D(width, height, NkGPUFormat::NK_RGBA8_UNORM, 1);
            desc.bindFlags = NkBindFlags::NK_SHADER_RESOURCE;
            desc.debugName = "NkGui_Backend_Texture";
            NkTextureHandle texture = mDevice->CreateTexture(desc);
            if (!texture.IsValid()) { if (converted) nkentseu::memory::NkFree(converted); return false; }
            const bool ok = mDevice->WriteTexture(texture, uploadData, (uint32)(width * 4));
            if (converted) nkentseu::memory::NkFree(converted);
            if (!ok) { mDevice->DestroyTexture(texture); return false; }

            NkDescSetHandle descSet;
            if (!CreateDescriptorSetForTexture(texture, descSet)) { mDevice->DestroyTexture(texture); return false; }

            if (existing) {
                RetireTextureEntry(*existing);
                existing->texture = texture; existing->descSet = descSet;
                existing->width = width; existing->height = height; existing->owned = true;
            } else {
                TextureEntry entry; entry.texture = texture; entry.descSet = descSet;
                entry.width = width; entry.height = height; entry.owned = true;
                mTextures[texId] = entry;
            }
            return true;
        }

        bool NkGuiRHIBackend::UploadTextureRGBA8(uint32 texId, const uint8* data, int32 width, int32 height) {
            return UploadTextureInternal(texId, data, width, height, true);
        }
        bool NkGuiRHIBackend::UploadTextureGray8(uint32 texId, const uint8* data, int32 width, int32 height) {
            return UploadTextureInternal(texId, data, width, height, false);
        }

        bool NkGuiRHIBackend::RegisterTexture(uint32 texId, NkTextureHandle texture) {
            if (!mDevice || texId == 0 || !texture.IsValid()) return false;
            TextureEntry* existing = mTextures.Find(texId);
            if (existing && existing->texture == texture && existing->descSet.IsValid()) return true; // déjà à jour
            NkDescSetHandle descSet;
            if (!CreateDescriptorSetForTexture(texture, descSet)) return false;
            if (existing) {
                RetireTextureEntry(*existing);            // ancienne (texture détruite seulement si owned)
                existing->texture = texture; existing->descSet = descSet;
                existing->width = 0; existing->height = 0; existing->owned = false;
            } else {
                TextureEntry entry; entry.texture = texture; entry.descSet = descSet;
                entry.width = 0; entry.height = 0; entry.owned = false;
                mTextures[texId] = entry;
            }
            return true;
        }

        bool NkGuiRHIBackend::HasTexture(uint32 texId) const noexcept {
            if (texId == 0) return true;
            const TextureEntry* entry = mTextures.Find(texId);
            return entry && entry->texture.IsValid() && entry->descSet.IsValid();
        }

        void NkGuiRHIBackend::Destroy() {
            if (!mDevice) return;
            for (auto& it : mTextures) {
                if (it.Second.descSet.IsValid()) mDevice->FreeDescriptorSet(it.Second.descSet);
                if (it.Second.owned && it.Second.texture.IsValid()) mDevice->DestroyTexture(it.Second.texture);
            }
            mTextures.Clear();
            for (auto& retired : mRetiredTextures) {
                if (retired.entry.descSet.IsValid()) mDevice->FreeDescriptorSet(retired.entry.descSet);
                if (retired.entry.owned && retired.entry.texture.IsValid()) mDevice->DestroyTexture(retired.entry.texture);
            }
            mRetiredTextures.Clear();
            if (mWhiteDescSet.IsValid()) mDevice->FreeDescriptorSet(mWhiteDescSet);
            if (mLayout.IsValid())   mDevice->DestroyDescriptorSetLayout(mLayout);
            if (mPipeline.IsValid()) mDevice->DestroyPipeline(mPipeline);
            if (mShader.IsValid())   mDevice->DestroyShader(mShader);
            if (mVBO.IsValid())      mDevice->DestroyBuffer(mVBO);
            if (mIBO.IsValid())      mDevice->DestroyBuffer(mIBO);
            if (mUBO.IsValid())      mDevice->DestroyBuffer(mUBO);
            if (mSampler.IsValid())  mDevice->DestroySampler(mSampler);
            if (mWhiteTex.IsValid()) mDevice->DestroyTexture(mWhiteTex);
            mWhiteDescSet = {}; mLayout = {}; mPipeline = {}; mShader = {};
            mVBO = {}; mIBO = {}; mUBO = {}; mSampler = {}; mWhiteTex = {};
            mVBOCap = 0; mIBOCap = 0; mLowUsageFrames = 0; mFrameIndex = 0;
            mDevice = nullptr; mBoundTexId = 0xFFFFFFFFu;
        }

    } // namespace nkgui
} // namespace nkentseu
