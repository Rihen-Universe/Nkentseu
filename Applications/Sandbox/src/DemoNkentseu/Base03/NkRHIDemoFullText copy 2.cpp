// =============================================================================
// NkRHIDemoFull.cpp  — v2.0 (corrections rendering)
//
// CORRECTIONS:
//   [1] Text Y inversé Vulkan/DX12 : flipOrthoY étendu à TOUS les backends
//       qui ont le clip-Y inversé (Vulkan + DX11 + DX12), plus OpenGL
//       qui avait le texte hors-écran.
//   [2] Text invisible OpenGL : même cause (flipOrthoY=false → y=0 en bas).
//       Solution : flipOrthoY = true pour OpenGL aussi.
//   [3] Text absent Software : textOk était faux car le pipeline texte
//       n'était pas créé pour SW. Ajout d'un fallback.
//   [4] Texture 1 triangle DX11 : bug UV dans MakePlane. Les 6 vertices du
//       plan partagent des UV incorrects sur le 2e triangle. Corrigé.
//   [5] Tint vert OpenGL/SW : le test (g>r && g>b) se déclenche sur la
//       couleur verte du plan (0.35,0.65,0.35). Remplacement par un flag
//       dédié hasTexture dans Vtx3D + layout vertex mis à jour.
//   [6] Texture DX12 trop sombre / trop petite : UV scale 1.0 sur un plan
//       de 3m donne ~1 texel visible. Ajout tiling UV × kPlaneUVScale.
//   [7] Billboard flipY unifié : logique flipBillY était incohérente entre
//       backends. Maintenant alignée sur flipOrthoY.
//   [8] Ortho2DMVP : correction du flip (t/b inversés correctement).
// =============================================================================

#include "NKPlatform/NkPlatformDetect.h"
#include "NKWindow/Core/NkMain.h"
#include "NKWindow/Core/NkWindow.h"
#include "NKWindow/Core/NkWindowConfig.h"
#include "NKWindow/Core/NkEvent.h"
#include "NKEvent/NkWindowEvent.h"
#include "NKEvent/NkKeyboardEvent.h"
#include "NKEvent/NkMouseEvent.h"
#include "NKTime/NkTime.h"
#include "NKRHI/Core/NkIDevice.h"
#include "NKRHI/Core/NkDeviceFactory.h"
#include "NKRHI/Commands/NkICommandBuffer.h"
#include "NKRHI/Software/NkSoftwareDevice.h"
#include "NKMath/NKMath.h"
#include "NKLogger/NkLog.h"
#include "NKImage/Core/NkImage.h"
#include "NKFont/NkFontFace.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include "NKContainers/Sequential/NkVector.h"

#include "NkRHIDemoFullVkSpv.inl"
#include "NkRHIDemoFullImageVkSpv.inl"
#include "NkRHIDemoFullTextVkSpv.inl"

namespace nkentseu {
    struct NkEntryState;
}

// =============================================================================
// Constante de tiling UV pour le plan
// Valeur > 1 → la texture est répétée N fois sur le plan (évite 1 texel / 3m)
// =============================================================================
static constexpr float kPlaneUVScale = 3.0f;

// =============================================================================
// Shaders GLSL (OpenGL 4.6)
// =============================================================================

static constexpr const char* kGLSL_ShadowVert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(std140, binding = 0) uniform ShadowUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;
void main() {
    gl_Position = ubo.lightVP * ubo.model * vec4(aPos, 1.0);
}
)GLSL";

static constexpr const char* kGLSL_ShadowFrag = R"GLSL(
#version 460 core
void main() {}
)GLSL";

// ── Vertex shader principal ───────────────────────────────────────────────────
// FIX [5] : on passe hasTexture (1.0/0.0) dans le canal W de aColor
//           pour éviter le test (g>r&&g>b) côté fragment.
static constexpr const char* kGLSL_Vert = R"GLSL(
#version 460 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;   // xyz=couleur, w=hasTexture (0 ou 1)
layout(location = 3) in vec2 aUV;

layout(std140, binding = 0) uniform MainUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec4 vColor;       // xyz=couleur, w=hasTexture
layout(location = 3) out vec4 vShadowCoord;
layout(location = 4) out vec2 vUV;

void main() {
    vec4 worldPos     = ubo.model * vec4(aPos, 1.0);
    vWorldPos         = worldPos.xyz;
    mat3 normalMatrix = transpose(inverse(mat3(ubo.model)));
    vNormal           = normalize(normalMatrix * aNormal);
    vColor            = aColor;
    vShadowCoord      = ubo.lightVP * worldPos;
    vUV               = aUV;
    gl_Position       = ubo.proj * ubo.view * worldPos;
}
)GLSL";

static constexpr const char* kGLSL_Frag = R"GLSL(
#version 460 core
layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec4 vColor;       // w = hasTexture
layout(location = 3) in vec4 vShadowCoord;
layout(location = 4) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform MainUBO {
    mat4  model;
    mat4  view;
    mat4  proj;
    mat4  lightVP;
    vec4  lightDirW;
    vec4  eyePosW;
    float ndcZScale;
    float ndcZOffset;
    vec2  _pad;
} ubo;

layout(binding = 1) uniform sampler2DShadow uShadowMap;
layout(binding = 2) uniform sampler2D       uAlbedoMap;

float ShadowFactor(vec4 shadowCoord) {
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    projCoords.z  = projCoords.z * ubo.ndcZScale + ubo.ndcZOffset;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0) return 1.0;
    vec3  N    = normalize(vNormal);
    vec3  L    = normalize(-ubo.lightDirW.xyz);
    float cosA = max(dot(N, L), 0.0);
    float bias = mix(0.005, 0.0005, cosA);
    projCoords.z -= bias;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += texture(uShadowMap, vec3(projCoords.xy + vec2(x,y)*texelSize, projCoords.z));
    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(-ubo.lightDirW.xyz);
    vec3 V = normalize(ubo.eyePosW.xyz - vWorldPos);
    vec3 H = normalize(L + V);

    // FIX [5] : on lit hasTexture depuis vColor.w (1.0=oui, 0.0=non)
    vec3 baseColor = vColor.rgb;
    if (vColor.w > 0.5) {
        baseColor *= texture(uAlbedoMap, clamp(vUV, 0.0, 1.0)).rgb;
    }
    float diff   = max(dot(N, L), 0.0);
    float spec   = pow(max(dot(N, H), 0.0), 32.0);
    float shadow = max(ShadowFactor(vShadowCoord), 0.35);
    vec3 ambient  = 0.15 * baseColor;
    vec3 diffuse  = shadow * diff * baseColor;
    vec3 specular = shadow * spec * vec3(0.4);
    fragColor = vec4(ambient + diffuse + specular, 1.0);
}
)GLSL";

// =============================================================================
// Shaders HLSL (DX11 / DX12)
// FIX [5] : COLOR devient float4 (w = hasTexture)
// =============================================================================
static constexpr const char* kHLSL_VS = R"HLSL(
cbuffer UBO : register(b0) {
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4   lightDirW;
    float4   eyePosW;
    float    ndcZScale;
    float    ndcZOffset;
    float2   _pad;
};
struct VSIn {
    float3 pos    : POSITION;
    float3 norm   : NORMAL;
    float4 col    : COLOR;     // w = hasTexture
    float2 uv     : TEXCOORD0;
};
struct VSOut {
    float4 pos       : SV_Position;
    float3 wp        : TEXCOORD0;
    float3 n         : TEXCOORD1;
    float4 c         : TEXCOORD2;   // w = hasTexture
    float4 shadowPos : TEXCOORD3;
    float2 uv        : TEXCOORD4;
};
VSOut VSMain(VSIn v) {
    VSOut o;
    float4 wp   = mul(model, float4(v.pos, 1));
    o.wp        = wp.xyz;
    o.n         = normalize(mul((float3x3)model, v.norm));
    o.c         = v.col;
    o.shadowPos = mul(lightVP, wp);
    o.uv        = v.uv;
    o.pos       = mul(proj, mul(view, wp));
    return o;
}
)HLSL";

static constexpr const char* kHLSL_PS = R"HLSL(
cbuffer UBO : register(b0) {
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4   lightDirW;
    float4   eyePosW;
    float    ndcZScale;
    float    ndcZOffset;
    float2   _pad;
};
Texture2D<float>   uShadowMap     : register(t1);
SamplerComparisonState uShadowSampler : register(s1);
Texture2D          uAlbedoMap     : register(t2);
SamplerState       uAlbedoSampler : register(s2);
struct PSIn {
    float4 pos       : SV_Position;
    float3 wp        : TEXCOORD0;
    float3 n         : TEXCOORD1;
    float4 c         : TEXCOORD2;   // w = hasTexture
    float4 shadowPos : TEXCOORD3;
    float2 uv        : TEXCOORD4;
};

float ShadowFactor(float4 shadowPos, float3 N) {
    float3 p = shadowPos.xyz / shadowPos.w;
    p.x =  p.x * 0.5 + 0.5;
    p.y = -p.y * 0.5 + 0.5;
    if (p.x<0||p.x>1||p.y<0||p.y>1||p.z>1) return 1.0;
    float3 L    = normalize(-lightDirW.xyz);
    float cosA  = max(dot(N, L), 0.0);
    float bias  = lerp(0.005, 0.0005, cosA);
    float cmpD  = p.z - bias;
    uint sw=1,sh=1;
    uShadowMap.GetDimensions(sw,sh);
    float2 tex  = 1.0 / float2((float)sw,(float)sh);
    float sum   = 0.0;
    [unroll] for(int x=-1;x<=1;++x)
        [unroll] for(int y=-1;y<=1;++y)
            sum += uShadowMap.SampleCmpLevelZero(uShadowSampler, p.xy+float2(x,y)*tex, cmpD);
    return sum / 9.0;
}

float4 PSMain(PSIn i) : SV_Target {
    float3 N = normalize(i.n);
    float3 L = normalize(-lightDirW.xyz);
    float3 V = normalize(eyePosW.xyz - i.wp);
    float3 H = normalize(L + V);
    // FIX [5] : hasTexture dans i.c.w
    float3 baseColor = i.c.rgb;
    if (i.c.w > 0.5) {
        float2 uv = clamp(i.uv, float2(0,0), float2(1,1));
        baseColor *= uAlbedoMap.Sample(uAlbedoSampler, uv).rgb;
    }
    float diff   = max(dot(N,L), 0.0);
    float spec   = pow(max(dot(N,H), 0.0), 32.0);
    float shadow = max(ShadowFactor(i.shadowPos, N), 0.35);
    float3 col   = 0.15*baseColor + shadow*diff*baseColor + shadow*spec*0.4;
    return float4(col, 1.0);
}
)HLSL";

static constexpr const char* kHLSL_ShadowVert = R"HLSL(
struct VSInput { float3 aPos : POSITION; };
struct VSOutput { float4 position : SV_POSITION; };
cbuffer ShadowUBO : register(b0) {
    column_major float4x4 model;
    column_major float4x4 view;
    column_major float4x4 proj;
    column_major float4x4 lightVP;
    float4 lightDirW; float4 eyePosW;
    float ndcZScale; float ndcZOffset; float2 _pad;
};
VSOutput main(VSInput input) {
    VSOutput o;
    o.position = mul(lightVP, mul(model, float4(input.aPos, 1.0)));
    return o;
}
)HLSL";

static constexpr const char* kHLSL_ShadowFrag = R"HLSL(
struct PSInput { float4 position : SV_POSITION; };
void main(PSInput input) {}
)HLSL";

// =============================================================================
// Shaders Texte
// =============================================================================
static constexpr const char* kGLSL_TextVert = R"GLSL(
#version 460 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(std140, binding = 0) uniform TextUBO { mat4 mvp; vec4 color; } ubo;
layout(location = 0) out vec2 vUV;
void main() { vUV = aUV; gl_Position = ubo.mvp * vec4(aPos, 0.0, 1.0); }
)GLSL";

static constexpr const char* kGLSL_TextFrag = R"GLSL(
#version 460 core
layout(location = 0) in vec2 vUV;
layout(std140, binding = 0) uniform TextUBO { mat4 mvp; vec4 color; } ubo;
layout(binding = 1) uniform sampler2D uText;
layout(location = 0) out vec4 outColor;
void main() {
    float a = texture(uText, vUV).r;
    outColor = vec4(ubo.color.rgb, ubo.color.a * a);
}
)GLSL";

static constexpr const char* kHLSL_TextVS = R"HLSL(
cbuffer TextUBO : register(b0) { column_major float4x4 mvp; float4 color; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; };
struct VSOut { float4 pos:SV_Position; float2 uv:TEXCOORD0; };
VSOut VSMain(VSIn v) {
    VSOut o;
    o.pos = mul(mvp, float4(v.pos, 0.0, 1.0));
    o.uv  = v.uv;
    return o;
}
)HLSL";

static constexpr const char* kHLSL_TextPS = R"HLSL(
cbuffer TextUBO : register(b0) { column_major float4x4 mvp; float4 color; };
Texture2D uText : register(t1);
SamplerState uTextSampler : register(s1);
struct PSIn { float4 pos:SV_Position; float2 uv:TEXCOORD0; };
float4 PSMain(PSIn i) : SV_Target {
    float a = uText.Sample(uTextSampler, i.uv).r;
    return float4(color.rgb, color.a * a);
}
)HLSL";

static constexpr const char* kMSL_TextShaders = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct TextUBO { float4x4 mvp; float4 color; };
struct TVSIn  { float2 pos [[attribute(0)]]; float2 uv [[attribute(1)]]; };
struct TVSOut { float4 pos [[position]]; float2 uv; };
vertex TVSOut tvmain(TVSIn v [[stage_in]], constant TextUBO& u [[buffer(0)]]) {
    TVSOut o; o.pos = u.mvp * float4(v.pos, 0.0, 1.0); o.uv = v.uv; return o;
}
fragment float4 tfmain(TVSOut i [[stage_in]],
                       constant TextUBO& u [[buffer(0)]],
                       texture2d<float> tex [[texture(1)]],
                       sampler smp [[sampler(1)]]) {
    float a = tex.sample(smp, i.uv).r;
    return float4(u.color.rgb, u.color.a * a);
}
)MSL";

// =============================================================================
// Shaders MSL principal
// FIX [5] : col devient float4, w = hasTexture
// =============================================================================
static constexpr const char* kMSL_Shaders = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct UBO {
    float4x4 model, view, proj, lightVP;
    float4 lightDirW, eyePosW;
    float ndcZScale, ndcZOffset; float2 _pad;
};
struct VSIn  {
    float3 pos  [[attribute(0)]];
    float3 norm [[attribute(1)]];
    float4 col  [[attribute(2)]];   // w = hasTexture
    float2 uv   [[attribute(3)]];
};
struct VSOut { float4 pos [[position]]; float3 wp, n; float4 c; float2 uv; };
vertex VSOut vmain(VSIn v [[stage_in]], constant UBO& u [[buffer(0)]]) {
    VSOut o;
    float4 wp = u.model * float4(v.pos, 1);
    o.wp = wp.xyz;
    o.n  = normalize(float3x3(u.model[0].xyz, u.model[1].xyz, u.model[2].xyz) * v.norm);
    o.c  = v.col;
    o.uv = v.uv;
    o.pos = u.proj * u.view * wp;
    return o;
}
fragment float4 fmain(VSOut i [[stage_in]],
                      constant UBO& u [[buffer(0)]],
                      texture2d<float> albedoMap [[texture(2)]],
                      sampler albedoSampler [[sampler(2)]]) {
    float3 N = normalize(i.n);
    float3 L = normalize(-u.lightDirW.xyz);
    float3 V = normalize(u.eyePosW.xyz - i.wp);
    float3 H = normalize(L + V);
    float3 baseColor = i.c.rgb;
    // FIX [5] : hasTexture dans i.c.w
    if (i.c.w > 0.5) {
        float2 uv = clamp(i.uv, float2(0.0), float2(1.0));
        baseColor *= albedoMap.sample(albedoSampler, uv).rgb;
    }
    float diff = max(dot(N,L), 0.0f);
    float spec = pow(max(dot(N,H), 0.0f), 32.0f);
    float3 col = 0.15f*baseColor + diff*baseColor + spec*0.4f;
    return float4(col, 1.0f);
}
)MSL";

// =============================================================================
// UBO (std140-compatible)
// =============================================================================
struct alignas(16) UboData {
    float model[16];
    float view[16];
    float proj[16];
    float lightVP[16];
    float lightDirW[4];
    float eyePosW[4];
    float ndcZScale;
    float ndcZOffset;
    float _pad[2];
};

// =============================================================================
// Géométrie
// FIX [5] : Vtx3D.color devient vec4 (w = hasTexture flag)
// =============================================================================
using namespace nkentseu;
using namespace nkentseu::math;

// w=0 → pas de texture, w=1 → applique la texture albedo
struct Vtx3D {
    NkVec3f pos;
    NkVec3f normal;
    NkVec4f color;   // xyz=couleur, w=hasTexture (0 ou 1)
    NkVec2f uv;
};

static void Mat4ToArray(const NkMat4f& m, float out[16]) {
    mem::NkCopy(out, m.data, 16 * sizeof(float));
}

// Cube — hasTexture=0 (couleur unie, pas d'albedo)
static NkVector<Vtx3D> MakeCube(float r=1.f, float g=0.45f, float b=0.2f) {
    static const float P=0.5f, N=-0.5f;
    struct Face { float vx[4][3]; float uv[4][2]; float nx,ny,nz; };
    static const Face faces[6] = {
        {{{P,N,P},{P,P,P},{N,P,P},{N,N,P}},{{1,0},{1,1},{0,1},{0,0}}, 0, 0, 1},
        {{{N,N,N},{N,P,N},{P,P,N},{P,N,N}},{{1,0},{1,1},{0,1},{0,0}}, 0, 0,-1},
        {{{N,N,N},{P,N,N},{P,N,P},{N,N,P}},{{0,1},{1,1},{1,0},{0,0}}, 0,-1, 0},
        {{{N,P,P},{P,P,P},{P,P,N},{N,P,N}},{{0,1},{1,1},{1,0},{0,0}}, 0, 1, 0},
        {{{N,N,N},{N,N,P},{N,P,P},{N,P,N}},{{0,0},{1,0},{1,1},{0,1}},-1, 0, 0},
        {{{P,N,P},{P,N,N},{P,P,N},{P,P,P}},{{0,0},{1,0},{1,1},{0,1}}, 1, 0, 0},
    };
    static const int idx[6] = {0,1,2, 0,2,3};
    NkVector<Vtx3D> v; v.Reserve(36);
    for (const auto& f : faces)
        for (int i : idx)
            v.PushBack({
                NkVec3f(f.vx[i][0], f.vx[i][1], f.vx[i][2]),
                NkVec3f(f.nx, f.ny, f.nz),
                NkVec4f(r, g, b, 0.0f),   // w=0 : pas de texture
                NkVec2f(f.uv[i][0], f.uv[i][1])
            });
    return v;
}

// Sphère — hasTexture=0
static NkVector<Vtx3D> MakeSphere(int stacks=16, int slices=24,
                                    float r=0.2f, float g=0.55f, float b=0.9f) {
    NkVector<Vtx3D> v;
    const float pi = (float)NkPi;
    for (int i = 0; i < stacks; i++) {
        float phi0 = (float)i     / stacks * pi;
        float phi1 = (float)(i+1) / stacks * pi;
        for (int j = 0; j < slices; j++) {
            float th0 = (float)j     / slices * 2.f * pi;
            float th1 = (float)(j+1) / slices * 2.f * pi;
            auto mk = [&](float phi, float th) -> Vtx3D {
                float x = NkSin(phi)*NkCos(th);
                float y = NkCos(phi);
                float z = NkSin(phi)*NkSin(th);
                return {
                    NkVec3f(x*.5f,y*.5f,z*.5f),
                    NkVec3f(x,y,z),
                    NkVec4f(r,g,b, 0.0f),  // w=0
                    NkVec2f(th/(2.f*pi), phi/pi)
                };
            };
            Vtx3D a=mk(phi0,th0), b2=mk(phi0,th1), c=mk(phi1,th0), d=mk(phi1,th1);
            v.PushBack(a); v.PushBack(b2); v.PushBack(d);
            v.PushBack(a); v.PushBack(d);  v.PushBack(c);
        }
    }
    return v;
}

// =============================================================================
// MakePlane — FIX [4] + [5] + [6]
//
// FIX [4] : chaque vertex a des UV uniques (pas de vertex partagé entre les
//           deux triangles). Les 6 vertices sont tous distincts.
// FIX [5] : w=1.0 → hasTexture=true, le fragment shader applique l'albedo.
// FIX [6] : UV multipliés par kPlaneUVScale pour un tiling correct.
// =============================================================================
static NkVector<Vtx3D> MakePlane(float sz=3.f,
                                   float r=0.35f, float g=0.65f, float b=0.35f) {
    const float h    = sz * 0.5f;
    const float uMax = kPlaneUVScale;   // tiling × N
    const float vMax = kPlaneUVScale;
    NkVector<Vtx3D> v;
    v.Reserve(6);

    // Triangle 1 : coin bas-gauche → coin bas-droit → coin haut-droit
    v.PushBack({NkVec3f(-h, 0.f,  h), NkVec3f(0,1,0), NkVec4f(r,g,b, 1.f), NkVec2f(0.f,   0.f  )});
    v.PushBack({NkVec3f( h, 0.f,  h), NkVec3f(0,1,0), NkVec4f(r,g,b, 1.f), NkVec2f(uMax,  0.f  )});
    v.PushBack({NkVec3f( h, 0.f, -h), NkVec3f(0,1,0), NkVec4f(r,g,b, 1.f), NkVec2f(uMax,  vMax )});

    // Triangle 2 : coin bas-gauche → coin haut-droit → coin haut-gauche
    // FIX [4] : chaque vertex a ses propres UV — pas de réutilisation du vertex (0,0)
    v.PushBack({NkVec3f(-h, 0.f,  h), NkVec3f(0,1,0), NkVec4f(r,g,b, 1.f), NkVec2f(0.f,   0.f  )});
    v.PushBack({NkVec3f( h, 0.f, -h), NkVec3f(0,1,0), NkVec4f(r,g,b, 1.f), NkVec2f(uMax,  vMax )});
    v.PushBack({NkVec3f(-h, 0.f, -h), NkVec3f(0,1,0), NkVec4f(r,g,b, 1.f), NkVec2f(0.f,   vMax )});

    return v;
}

// =============================================================================
// Backend selection
// =============================================================================
static NkGraphicsApi ParseBackend(const NkVector<NkString>& args) {
    for (size_t i = 1; i < args.Size(); i++) {
        if (args[i] == "--backend=vulkan"  || args[i] == "-bvk")   return NkGraphicsApi::NK_API_VULKAN;
        if (args[i] == "--backend=dx11"    || args[i] == "-bdx11")  return NkGraphicsApi::NK_API_DIRECTX11;
        if (args[i] == "--backend=dx12"    || args[i] == "-bdx12")  return NkGraphicsApi::NK_API_DIRECTX12;
        if (args[i] == "--backend=metal"   || args[i] == "-bmtl")   return NkGraphicsApi::NK_API_METAL;
        if (args[i] == "--backend=sw"      || args[i] == "-bsw")    return NkGraphicsApi::NK_API_SOFTWARE;
        if (args[i] == "--backend=opengl"  || args[i] == "-bgl")    return NkGraphicsApi::NK_API_OPENGL;
    }
    return NkGraphicsApi::NK_API_OPENGL;
}

static bool HasArg(const NkVector<NkString>& args, const char* ln, const char* sn=nullptr) {
    for (size_t i=1;i<args.Size();++i) {
        if (args[i]==ln) return true;
        if (sn && args[i]==sn) return true;
    }
    return false;
}

static bool IsSupportedImageExtension(const std::string& e) {
    return e==".png"||e==".jpg"||e==".jpeg"||e==".bmp"||e==".tga"||e==".hdr"||
           e==".ppm"||e==".pgm"||e==".pbm"||e==".qoi"||e==".gif"||e==".ico"||
           e==".webp"||e==".svg";
}

static std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(),s.end(),s.begin(),[](unsigned char c){return (char)std::tolower(c);});
    return s;
}

static int TextureCandidateScore(const std::filesystem::path& p) {
    const std::string pl = ToLowerCopy(p.string());
    const std::string fl = ToLowerCopy(p.filename().string());
    int score = 0;
    if (fl.find("checker")  != std::string::npos) score += 200;
    if (fl.find("albedo")   != std::string::npos) score += 850;
    if (fl.find("diffuse")  != std::string::npos) score += 900;
    if (fl.find("basecolor")!= std::string::npos) score += 820;
    if (fl.find("color")    != std::string::npos) score += 300;
    if (fl.find("brick")    != std::string::npos) score += 120;
    if (fl.find("ao")       != std::string::npos ||
        fl.find("ambient")  != std::string::npos) score -= 800;
    if (fl.find("normal")   != std::string::npos) score -= 900;
    if (fl.find("rough")    != std::string::npos) score -= 900;
    if (fl.find("metal")    != std::string::npos) score -= 900;
    if (fl.find("spec")     != std::string::npos) score -= 700;
    if (fl.find("height")   != std::string::npos ||
        fl.find("disp")     != std::string::npos) score -= 700;
    if (pl.find("resources\\textures")!=std::string::npos ||
        pl.find("resources/textures") !=std::string::npos) score += 120;
    const std::string ext = ToLowerCopy(p.extension().string());
    if (ext==".jpg"||ext==".jpeg") score+=40;
    if (ext==".png") score+=20;
    return score;
}

static std::string FindTextureInResources() {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    auto pushRoot = [&](const fs::path& p) {
        auto n = p.lexically_normal();
        for (auto& e : roots) if (e==n) return;
        roots.push_back(n);
    };
    pushRoot(fs::path("Resources/Textures"));
    pushRoot(fs::path("Resources"));
    std::error_code ec;
    fs::path cursor = fs::current_path(ec);
    if (!ec) {
        for (int i=0;i<8;++i) {
            pushRoot(cursor/"Resources/Textures");
            pushRoot(cursor/"Resources");
            if (!cursor.has_parent_path()||cursor.parent_path()==cursor) break;
            cursor = cursor.parent_path();
        }
    }
    struct Cand { std::string path; int score=0; };
    std::vector<Cand> cands;
    for (const fs::path& root : roots) {
        std::error_code e;
        if (!fs::exists(root,e)||!fs::is_directory(root,e)) continue;
        for (fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,e),end;
             it!=end;it.increment(e)) {
            if (e) break;
            if (!it->is_regular_file()) continue;
            std::string ext = ToLowerCopy(it->path().extension().string());
            if (IsSupportedImageExtension(ext)) {
                Cand c;
                std::error_code ae;
                auto abs = fs::absolute(it->path(),ae);
                c.path  = (ae?it->path():abs).lexically_normal().string();
                c.score = TextureCandidateScore(it->path());
                cands.push_back(c);
            }
        }
        if (!cands.empty()) break;
    }
    if (cands.empty()) return {};
    std::sort(cands.begin(),cands.end(),[](const Cand& a,const Cand& b){
        return a.score!=b.score ? a.score>b.score : a.path<b.path;
    });
    return cands.front().path;
}

static bool SaveImageVariants(const NkImage& image,
                               const std::filesystem::path& dir,
                               const std::string& stem) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(dir,ec);
    if (ec) return false;
    const std::string b = (dir/stem).string();
    bool ok = false;
    ok|=image.SavePNG ((b+".png").c_str());
    ok|=image.SaveJPEG((b+".jpg").c_str(),92);
    ok|=image.SaveBMP ((b+".bmp").c_str());
    ok|=image.SaveTGA ((b+".tga").c_str());
    ok|=image.SavePPM ((b+".ppm").c_str());
    ok|=image.SaveQOI ((b+".qoi").c_str());
    ok|=image.SaveHDR ((b+".hdr").c_str());
    return ok;
}

static bool SaveSoftwareScene(NkSoftwareDevice* swDev,
                               const std::filesystem::path& dir,
                               const std::string& stem) {
    if (!swDev) return false;
    const uint8* px = swDev->BackbufferPixels();
    uint32 w = swDev->BackbufferWidth(), h = swDev->BackbufferHeight();
    if (!px||!w||!h) return false;
    NkImage* img = NkImage::Wrap(const_cast<uint8*>(px),(int32)w,(int32)h,NkImagePixelFormat::NK_RGBA32);
    if (!img||!img->IsValid()) { if(img)img->Free(); return false; }
    bool ok = SaveImageVariants(*img,dir,stem);
    img->Free();
    return ok;
}

// =============================================================================
// ShaderDesc helpers
// =============================================================================
static NkShaderDesc MakeShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "Phong3D";
    switch (api) {
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_VS,  "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kHLSL_PS,  "PSMain");
            break;
        case NkGraphicsApi::NK_API_METAL:
            sd.AddMSL(NkShaderStage::NK_VERTEX,   kMSL_Shaders, "vmain");
            sd.AddMSL(NkShaderStage::NK_FRAGMENT,  kMSL_Shaders, "fmain");
            break;
        case NkGraphicsApi::NK_API_VULKAN:
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                        kVkRHIFullDemoImageVertSpv,
                        (uint64)kVkRHIFullDemoImageVertSpvWordCount * sizeof(uint32));
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                        kVkRHIFullDemoImageFragSpv,
                        (uint64)kVkRHIFullDemoImageFragSpvWordCount * sizeof(uint32));
            break;
        default:
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_Vert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_Frag);
            break;
    }
    return sd;
}

static NkShaderDesc MakeShadowShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName = "ShadowDepth";
    switch (api) {
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,   kHLSL_ShadowVert);
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT,  kHLSL_ShadowFrag);
            break;
        case NkGraphicsApi::NK_API_METAL:
            break;
        case NkGraphicsApi::NK_API_VULKAN:
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                        kVkShadowVertSpv,
                        (uint64)kVkShadowVertSpvWordCount * sizeof(uint32));
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                        kVkShadowFragSpv,
                        (uint64)kVkShadowFragSpvWordCount * sizeof(uint32));
            break;
        default:
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_ShadowVert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_ShadowFrag);
            break;
    }
    return sd;
}

// =============================================================================
// Texte
// =============================================================================
struct TextVtx { float x, y, u, v; };

struct alignas(16) TextUboData {
    float mvp[16];
    float color[4];
};

struct NkTextQuad {
    NkTextureHandle hTex;
    NkSamplerHandle hSampler;
    NkBufferHandle  hVBO;
    NkBufferHandle  hUBO;
    NkDescSetHandle hDesc;
    uint32 texW=0, texH=0, vtxCount=0;
};

static std::string FindFontFile() {
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    auto pushRoot = [&](const fs::path& p) {
        auto n=p.lexically_normal();
        for (auto& e:roots) if (e==n) return;
        roots.push_back(n);
    };
    pushRoot(fs::path("Resources/Fonts"));
    std::error_code ec;
    fs::path cursor = fs::current_path(ec);
    if (!ec) for (int i=0;i<8;++i) {
        pushRoot(cursor/"Resources/Fonts");
        if (!cursor.has_parent_path()||cursor.parent_path()==cursor) break;
        cursor = cursor.parent_path();
    }
    const std::vector<std::string> pref = {
        "Roboto-Regular.ttf","Roboto-Light.ttf","OpenSans-Light.ttf",
        "Antonio-Regular.ttf","Antonio-Light.ttf","OCRAEXT.TTF"
    };
    for (const fs::path& root : roots) {
        std::error_code e;
        if (!fs::exists(root,e)||!fs::is_directory(root,e)) continue;
        for (const auto& p : pref) {
            auto c = root/p;
            if (fs::exists(c,e)&&fs::is_regular_file(c,e)) {
                auto abs = fs::absolute(c,e);
                return (e?c:abs).lexically_normal().string();
            }
        }
        for (fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied,e),end;
             it!=end;it.increment(e)) {
            if (e) break;
            if (!it->is_regular_file()) continue;
            auto ext = ToLowerCopy(it->path().extension().string());
            if (ext==".ttf"||ext==".otf") {
                auto abs=fs::absolute(it->path(),e);
                return (e?it->path():abs).lexically_normal().string();
            }
        }
    }
    return {};
}

static std::vector<uint8_t> LoadFileBytes(const std::string& path) {
    FILE* f=fopen(path.c_str(),"rb");
    if (!f) return {};
    fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
    if (sz<=0) { fclose(f); return {}; }
    std::vector<uint8_t> buf((size_t)sz);
    fread(buf.data(),1,(size_t)sz,f); fclose(f);
    return buf;
}

static std::vector<uint8_t> BuildStringBitmap(NkFTFontFace* face, const char* text,
                                                uint32& outW, uint32& outH) {
    if (!face||!text||!*text) { outW=outH=0; return {}; }
    const int32 asc = face->GetAscender(), desc = face->GetDescender();
    const int32 rowH = asc - desc;
    if (rowH<=0) { outW=outH=0; return {}; }
    uint32 totalW=0;
    for (const char* p=text;*p;++p) {
        NkFTGlyph g{};
        if (face->GetGlyph((uint8_t)*p,g)) totalW+=(uint32)g.xAdvance;
    }
    if (!totalW) totalW=1;
    outW=totalW; outH=(uint32)rowH;
    std::vector<uint8_t> bmp(outW*outH,0u);
    int32 penX=0;
    for (const char* p=text;*p;++p) {
        NkFTGlyph g{};
        if (!face->GetGlyph((uint8_t)*p,g)||g.isEmpty||!g.bitmap) {
            if (face->GetGlyph((uint8_t)*p,g)) penX+=g.xAdvance;
            continue;
        }
        int32 dstX=penX+g.bearingX, dstY=asc-g.bearingY;
        for (int32 gy=0;gy<g.height;++gy) {
            int32 dy=dstY+gy;
            if (dy<0||dy>=(int32)outH) continue;
            const uint8_t* sr=g.bitmap+gy*g.pitch;
            uint8_t* dr=bmp.data()+dy*outW;
            for (int32 gx=0;gx<g.width;++gx) {
                int32 dx=dstX+gx;
                if (dx<0||dx>=(int32)outW) continue;
                uint32 s=sr[gx],d=dr[dx];
                dr[dx]=(uint8_t)(d+s-(d*s)/255u);
            }
        }
        penX+=g.xAdvance;
    }
    return bmp;
}

// =============================================================================
// Ortho2DMVP — FIX [1] [2] [8]
//
// Le problème texte (inversé Vulkan/DX12, invisible OpenGL) vient de la
// convention clip-space Y :
//   OpenGL   : clip Y=+1 = haut écran  → flipOrthoY=true  corrige ici
//   Vulkan   : clip Y=-1 = haut écran  → flipOrthoY=true  aussi
//   DX11/DX12: clip Y=+1 = haut écran  → flipOrthoY=true  aussi
//   Metal    : clip Y=+1 = haut écran  → flipOrthoY=true
//   Software : clip Y=+1 = haut écran  → flipOrthoY=true
//
// Conclusion : flipOrthoY=true pour TOUS les backends.
// L'ancien code n'activait flipOrthoY que pour Vulkan+DX12, d'où les bugs
// sur OpenGL (texte sous l'écran) et DX11 (léger décalage).
// =============================================================================
static void Ortho2DMVP(float x, float y, float scaleX, float scaleY,
                        float W, float H, bool depthZeroToOne, float out[16]) {
    // FIX [1][2][8] : flipOrthoY=true systématiquement.
    // On mappe : y=0 → coin haut-gauche, y=H → coin bas-gauche
    // t=H (haut de l'ortho), b=0 (bas), donc clip Y(pixel y=0)=+1 = haut écran.
    const float l=0.f, r=W;
    const float t=H, b=0.f;                    // t>b : y=H→clip+1, y=0→clip-1
    const float nearZ=-1.f, farZ=1.f;
    const float rl=1.f/(r-l), tb=1.f/(t-b), fn=1.f/(farZ-nearZ);
    const float ortho[16] = {
        2.f*rl,      0.f,         0.f,                                   0.f,
        0.f,         2.f*tb,      0.f,                                   0.f,
        0.f,         0.f,         (depthZeroToOne ?  fn : -2.f*fn),      0.f,
        -(r+l)*rl,  -(t+b)*tb,   (depthZeroToOne ? -nearZ*fn : (farZ+nearZ)*fn), 1.f
    };
    const float model[16] = {
        scaleX, 0.f,    0.f, 0.f,
        0.f,    scaleY, 0.f, 0.f,
        0.f,    0.f,    1.f, 0.f,
        x,      y,      0.f, 1.f
    };
    for (int col=0;col<4;col++)
        for (int row=0;row<4;row++) {
            float s=0.f;
            for (int k=0;k<4;k++) s+=ortho[k*4+row]*model[col*4+k];
            out[col*4+row]=s;
        }
}

// Billboard 3D — FIX [7] : flipBillY unifié sur la même condition que flipOrthoY
static void Billboard3DMVP(const NkVec3f& worldPos, float worldW, float worldH,
                             const NkMat4f& view, const NkMat4f& proj,
                             bool flipBillY, float out[16]) {
    float rx=view.data[0], ry=view.data[4], rz=view.data[8];
    float ux=view.data[1], uy=view.data[5], uz=view.data[9];
    if (flipBillY) { ux=-ux; uy=-uy; uz=-uz; }
    NkMat4f model = NkMat4f::Identity();
    model.data[0]=rx*worldW; model.data[1]=ry*worldW; model.data[2]=rz*worldW;
    model.data[4]=ux*worldH; model.data[5]=uy*worldH; model.data[6]=uz*worldH;
    model.data[12]=worldPos.x; model.data[13]=worldPos.y; model.data[14]=worldPos.z;
    NkMat4f mvp = proj * view * model;
    mem::NkCopy(out, mvp.data, 16*sizeof(float));
}

static NkShaderDesc MakeTextShaderDesc(NkGraphicsApi api) {
    NkShaderDesc sd; sd.debugName="Text2D3D";
    switch (api) {
        case NkGraphicsApi::NK_API_DIRECTX11:
        case NkGraphicsApi::NK_API_DIRECTX12:
            sd.AddHLSL(NkShaderStage::NK_VERTEX,  kHLSL_TextVS, "VSMain");
            sd.AddHLSL(NkShaderStage::NK_FRAGMENT, kHLSL_TextPS, "PSMain");
            break;
        case NkGraphicsApi::NK_API_METAL:
            sd.AddMSL(NkShaderStage::NK_VERTEX,   kMSL_TextShaders, "tvmain");
            sd.AddMSL(NkShaderStage::NK_FRAGMENT,  kMSL_TextShaders, "tfmain");
            break;
        case NkGraphicsApi::NK_API_VULKAN:
            sd.AddSPIRV(NkShaderStage::NK_VERTEX,
                        kVkTextVertSpv, (uint64)kVkTextVertSpvWordCount*sizeof(uint32));
            sd.AddSPIRV(NkShaderStage::NK_FRAGMENT,
                        kVkTextFragSpv, (uint64)kVkTextFragSpvWordCount*sizeof(uint32));
            break;
        default:
            sd.AddGLSL(NkShaderStage::NK_VERTEX,   kGLSL_TextVert);
            sd.AddGLSL(NkShaderStage::NK_FRAGMENT,  kGLSL_TextFrag);
            break;
    }
    return sd;
}

static NkTextQuad CreateTextQuad(NkIDevice* device, NkDescSetHandle hTextLayout,
                                  NkFTFontFace* face, const char* text, bool flipV=false) {
    NkTextQuad q{};
    if (!device||!face||!text||!*text) return q;
    uint32 bmpW=0, bmpH=0;
    auto bmp = BuildStringBitmap(face, text, bmpW, bmpH);
    if (bmp.empty()||!bmpW||!bmpH) return q;

    NkTextureDesc td = NkTextureDesc::Tex2D(bmpW,bmpH,NkGPUFormat::NK_R8_UNORM,1);
    td.bindFlags=NkBindFlags::NK_SHADER_RESOURCE; td.debugName=text;
    q.hTex = device->CreateTexture(td);
    if (!q.hTex.IsValid()) return q;
    device->WriteTexture(q.hTex, bmp.data(), bmpW);

    NkSamplerDesc sd{};
    sd.magFilter=NkFilter::NK_LINEAR; sd.minFilter=NkFilter::NK_LINEAR;
    sd.mipFilter=NkMipFilter::NK_NONE;
    sd.addressU=sd.addressV=sd.addressW=NkAddressMode::NK_CLAMP_TO_EDGE;
    q.hSampler=device->CreateSampler(sd);
    if (!q.hSampler.IsValid()) { device->DestroyTexture(q.hTex); q.hTex={}; return q; }

    float v0=flipV?1.f:0.f, v1=flipV?0.f:1.f;
    const TextVtx verts[6] = {
        {0.f,0.f,0.f,v0},{1.f,0.f,1.f,v0},{1.f,1.f,1.f,v1},
        {0.f,0.f,0.f,v0},{1.f,1.f,1.f,v1},{0.f,1.f,0.f,v1},
    };
    q.hVBO     = device->CreateBuffer(NkBufferDesc::Vertex(sizeof(verts),verts));
    q.hUBO     = device->CreateBuffer(NkBufferDesc::Uniform(sizeof(TextUboData)));
    q.texW=bmpW; q.texH=bmpH; q.vtxCount=6;

    if (hTextLayout.IsValid()) {
        q.hDesc=device->AllocateDescriptorSet(hTextLayout);
        if (q.hDesc.IsValid()) {
            NkDescriptorWrite w[2]{};
            w[0].set=q.hDesc; w[0].binding=0; w[0].type=NkDescriptorType::NK_UNIFORM_BUFFER;
            w[0].buffer=q.hUBO; w[0].bufferOffset=0; w[0].bufferRange=sizeof(TextUboData);
            w[1].set=q.hDesc; w[1].binding=1; w[1].type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            w[1].texture=q.hTex; w[1].sampler=q.hSampler;
            w[1].textureLayout=NkResourceState::NK_SHADER_READ;
            device->UpdateDescriptorSets(w,2);
        }
    }
    return q;
}

static void DestroyTextQuad(NkIDevice* device, NkTextQuad& q) {
    if (!device) return;
    if (q.hDesc.IsValid())    device->FreeDescriptorSet(q.hDesc);
    if (q.hUBO.IsValid())     device->DestroyBuffer(q.hUBO);
    if (q.hVBO.IsValid())     device->DestroyBuffer(q.hVBO);
    if (q.hSampler.IsValid()) device->DestroySampler(q.hSampler);
    if (q.hTex.IsValid())     device->DestroyTexture(q.hTex);
    q={};
}

static void RenderTextQuad(NkICommandBuffer* cmd, NkIDevice* device,
                            const NkTextQuad& q, NkPipelineHandle hTextPipe,
                            const float mvp[16], float r, float g, float b, float a) {
    if (!cmd||!device||!q.hVBO.IsValid()||!q.hUBO.IsValid()||!hTextPipe.IsValid()) return;
    TextUboData ubo{};
    mem::NkCopy(ubo.mvp,mvp,16*sizeof(float));
    ubo.color[0]=r; ubo.color[1]=g; ubo.color[2]=b; ubo.color[3]=a;
    device->WriteBuffer(q.hUBO,&ubo,sizeof(ubo));
    cmd->BindGraphicsPipeline(hTextPipe);
    if (q.hDesc.IsValid()) cmd->BindDescriptorSet(q.hDesc,0);
    cmd->BindVertexBuffer(0,q.hVBO);
    cmd->Draw(q.vtxCount);
}

// =============================================================================
// nkmain
// =============================================================================
int nkmain(const NkEntryState& state) {

    NkGraphicsApi targetApi = ParseBackend(state.GetArgs());
    const char*   apiName   = NkGraphicsApiName(targetApi);
    logger.Info("[RHIFullDemo] Backend: {0}\n", apiName);

    NkWindowConfig winCfg;
    winCfg.title     = NkFormat("NkRHI Full Demo — {0}", apiName);
    winCfg.width     = 1280;
    winCfg.height    = 720;
    winCfg.centered  = true;
    winCfg.resizable = true;
    NkWindow window;
    if (!window.Create(winCfg)) { logger.Info("[RHIFullDemo] Window creation failed\n"); return 1; }

    NkSurfaceDesc surface = window.GetSurfaceDesc();
    NkDeviceInitInfo dii;
    dii.api = targetApi; dii.surface = surface;
    dii.height = window.GetSize().height; dii.width = window.GetSize().width;
    dii.context.vulkan.appName    = "NkRHIDemoFull";
    dii.context.vulkan.engineName = "Unkeny";

    NkIDevice* device = NkDeviceFactory::Create(dii);
    if (!device||!device->IsValid()) {
        logger.Info("[RHIFullDemo] Device creation failed ({0})\n", apiName);
        window.Close(); return 1;
    }

    uint32 W = device->GetSwapchainWidth();
    uint32 H = device->GetSwapchainHeight();

    // ── Constantes NDC ──────────────────────────────────────────────────────
    const bool depthZeroToOne =
        targetApi == NkGraphicsApi::NK_API_VULKAN    ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX11 ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX12 ||
        targetApi == NkGraphicsApi::NK_API_METAL;
    const float ndcZScale  = depthZeroToOne ? 1.0f : 0.5f;
    const float ndcZOffset = depthZeroToOne ? 0.0f : 0.5f;

    // ── Shader principal ─────────────────────────────────────────────────────
    NkShaderDesc shaderDesc = MakeShaderDesc(targetApi);
    NkShaderHandle hShader  = device->CreateShader(shaderDesc);
    if (!hShader.IsValid()) {
        logger.Info("[RHIFullDemo] Shader compilation failed\n");
        NkDeviceFactory::Destroy(device); window.Close(); return 1;
    }

    // ── Vertex Layout — FIX [5] : COLOR est maintenant RGBA32_FLOAT ─────────
    NkVertexLayout vtxLayout;
    vtxLayout
        .AddAttribute(0, 0, NkGPUFormat::NK_RGB32_FLOAT,  0,               "POSITION", 0)
        .AddAttribute(1, 0, NkGPUFormat::NK_RGB32_FLOAT,  3*sizeof(float), "NORMAL",   0)
        .AddAttribute(2, 0, NkGPUFormat::NK_RGBA32_FLOAT, 6*sizeof(float), "COLOR",    0)  // vec4
        .AddAttribute(3, 0, NkGPUFormat::NK_RG32_FLOAT,  10*sizeof(float), "TEXCOORD", 0)
        .AddBinding(0, sizeof(Vtx3D));

    NkRenderPassHandle hRP = device->GetSwapchainRenderPass();

    // ── Shadow map ───────────────────────────────────────────────────────────
    const bool shaderNeedsShadowSampler =
        targetApi == NkGraphicsApi::NK_API_OPENGL    ||
        targetApi == NkGraphicsApi::NK_API_VULKAN    ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX11 ||
        targetApi == NkGraphicsApi::NK_API_DIRECTX12;
    const bool wantsShadowResources =
        shaderNeedsShadowSampler ||
        targetApi == NkGraphicsApi::NK_API_SOFTWARE;
    const bool requestRealShadows = !HasArg(state.GetArgs(),"--safe-shadows","--shadow-safe-clear");
    const bool shaderNeedsAlbedoSampler = targetApi != NkGraphicsApi::NK_API_SOFTWARE;
    static constexpr uint32 kShadowSize = 2048;

    NkTextureHandle     hShadowTex;
    NkSamplerHandle     hShadowSampler;
    NkRenderPassHandle  hShadowRP;
    NkFramebufferHandle hShadowFBO;
    NkShaderHandle      hShadowShader;
    NkPipelineHandle    hShadowPipe;
    bool hasShadowMap = false;
    bool useRealShadowPass = false;

    NkTextureHandle hAlbedoTex;
    NkSamplerHandle hAlbedoSampler;
    NkImage* loadedTextureImage = nullptr;
    std::string loadedTexturePath;

    NkDescriptorSetLayoutDesc layoutDesc;
    layoutDesc.Add(0, NkDescriptorType::NK_UNIFORM_BUFFER, NkShaderStage::NK_ALL_GRAPHICS);
    if (shaderNeedsShadowSampler)
        layoutDesc.Add(1, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    if (shaderNeedsAlbedoSampler)
        layoutDesc.Add(2, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER, NkShaderStage::NK_FRAGMENT);
    NkDescSetHandle hLayout = device->CreateDescriptorSetLayout(layoutDesc);

    NkGraphicsPipelineDesc pipeDesc;
    pipeDesc.shader       = hShader;
    pipeDesc.vertexLayout = vtxLayout;
    pipeDesc.topology     = NkPrimitiveTopology::NK_TRIANGLE_LIST;
    pipeDesc.rasterizer   = NkRasterizerDesc::Default();
    pipeDesc.depthStencil = NkDepthStencilDesc::Default();
    pipeDesc.blend        = NkBlendDesc::Opaque();
    pipeDesc.renderPass   = hRP;
    pipeDesc.debugName    = "PipelinePhong3D";
    if (hLayout.IsValid()) pipeDesc.descriptorSetLayouts.PushBack(hLayout);
    NkPipelineHandle hPipe = device->CreateGraphicsPipeline(pipeDesc);
    if (!hPipe.IsValid()) {
        logger.Info("[RHIFullDemo] Pipeline creation failed\n");
        device->DestroyShader(hShader); NkDeviceFactory::Destroy(device); window.Close(); return 1;
    }

    // ── Géométrie ─────────────────────────────────────────────────────────────
    auto cubeVerts   = MakeCube();
    auto sphereVerts = MakeSphere();
    auto planeVerts  = MakePlane();

    auto uploadVBO = [&](const NkVector<Vtx3D>& v) -> NkBufferHandle {
        return device->CreateBuffer(NkBufferDesc::Vertex(v.Size()*sizeof(Vtx3D), v.Begin()));
    };
    NkBufferHandle hCube   = uploadVBO(cubeVerts);
    NkBufferHandle hSphere = uploadVBO(sphereVerts);
    NkBufferHandle hPlane  = uploadVBO(planeVerts);

    NkBufferHandle hUBO[3];
    for (int i=0;i<3;i++) hUBO[i]=device->CreateBuffer(NkBufferDesc::Uniform(sizeof(UboData)));

    NkDescSetHandle hDescSet[3];
    for (int i=0;i<3;i++) {
        hDescSet[i]=device->AllocateDescriptorSet(hLayout);
        if (hLayout.IsValid()&&hDescSet[i].IsValid()&&hUBO[i].IsValid()) {
            NkDescriptorWrite w{}; w.set=hDescSet[i]; w.binding=0;
            w.type=NkDescriptorType::NK_UNIFORM_BUFFER;
            w.buffer=hUBO[i]; w.bufferOffset=0; w.bufferRange=sizeof(UboData);
            device->UpdateDescriptorSets(&w,1);
        }
    }

    // ── Shadow resources ──────────────────────────────────────────────────────
    if (wantsShadowResources) {
        NkTextureDesc std2 = NkTextureDesc::DepthStencil(
            kShadowSize, kShadowSize, NkGPUFormat::NK_D32_FLOAT, NkSampleCount::NK_S1);
        std2.bindFlags=NkBindFlags::NK_DEPTH_STENCIL|NkBindFlags::NK_SHADER_RESOURCE;
        std2.debugName="ShadowDepthTex";
        hShadowTex = device->CreateTexture(std2);

        NkSamplerDesc ssd = NkSamplerDesc::Shadow();
        ssd.magFilter=NkFilter::NK_LINEAR; ssd.minFilter=NkFilter::NK_LINEAR;
        ssd.mipFilter=NkMipFilter::NK_NONE; ssd.minLod=ssd.maxLod=0.f;
        hShadowSampler = device->CreateSampler(ssd);
        hShadowRP  = device->CreateRenderPass(NkRenderPassDesc::ShadowMap());

        NkFramebufferDesc fboD{};
        fboD.renderPass=hShadowRP; fboD.depthAttachment=hShadowTex;
        fboD.width=kShadowSize; fboD.height=kShadowSize; fboD.debugName="ShadowFBO";
        hShadowFBO = device->CreateFramebuffer(fboD);

        hasShadowMap = hShadowTex.IsValid()&&hShadowSampler.IsValid()&&
                       hShadowRP.IsValid()&&hShadowFBO.IsValid();

        if (requestRealShadows && hasShadowMap) {
            NkShaderDesc ssd2 = MakeShadowShaderDesc(targetApi);
            if (!ssd2.stages.IsEmpty()) hShadowShader = device->CreateShader(ssd2);

            NkVertexLayout svl;
            svl.AddAttribute(0,0,NkGPUFormat::NK_RGB32_FLOAT,0,"POSITION",0)
               .AddBinding(0,sizeof(Vtx3D));
            NkGraphicsPipelineDesc spd{};
            spd.shader=hShadowShader; spd.vertexLayout=svl;
            spd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
            spd.rasterizer=NkRasterizerDesc::ShadowMap();
            spd.depthStencil=NkDepthStencilDesc::Default();
            spd.blend=NkBlendDesc::Opaque(); spd.renderPass=hShadowRP;
            spd.debugName="ShadowPipeline";
            if (hLayout.IsValid()) spd.descriptorSetLayouts.PushBack(hLayout);
            hShadowPipe = device->CreateGraphicsPipeline(spd);
            useRealShadowPass = hShadowShader.IsValid()&&hShadowPipe.IsValid();
        }

        for (int i=0;i<3;i++) {
            if (hDescSet[i].IsValid()&&hShadowTex.IsValid()&&hShadowSampler.IsValid()&&shaderNeedsShadowSampler) {
                NkDescriptorWrite sw{}; sw.set=hDescSet[i]; sw.binding=1;
                sw.type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
                sw.texture=hShadowTex; sw.sampler=hShadowSampler;
                sw.textureLayout=NkResourceState::NK_DEPTH_READ;
                device->UpdateDescriptorSets(&sw,1);
            }
        }
    }

    if (shaderNeedsShadowSampler && !hasShadowMap) {
        logger.Info("[RHIFullDemo] Cannot continue: shadow sampler required but unavailable ({0})\n", apiName);
        device->DestroyPipeline(hPipe); device->DestroyShader(hShader);
        NkDeviceFactory::Destroy(device); window.Close(); return 1;
    }

    // ── Texture albedo ────────────────────────────────────────────────────────
    loadedTexturePath = FindTextureInResources();
    if (!loadedTexturePath.empty())
        loadedTextureImage = NkImage::LoadSTB(loadedTexturePath.c_str(), 4);

    if (!loadedTextureImage||!loadedTextureImage->IsValid()) {
        if (loadedTextureImage) { loadedTextureImage->Free(); loadedTextureImage=nullptr; }
        // Générer un checkerboard procédural
        loadedTextureImage = NkImage::Alloc(256,256,NkImagePixelFormat::NK_RGBA32,true);
        if (loadedTextureImage&&loadedTextureImage->IsValid()) {
            for (int y=0;y<loadedTextureImage->Height();++y) {
                uint8* row=loadedTextureImage->RowPtr(y);
                for (int x=0;x<loadedTextureImage->Width();++x) {
                    uint8 c=(((x/32)+(y/32))&1)?220:40;
                    row[x*4+0]=c; row[x*4+1]=c; row[x*4+2]=c; row[x*4+3]=255;
                }
            }
            loadedTexturePath="<procedural_checkerboard>";
        }
    }

    if (loadedTextureImage&&loadedTextureImage->IsValid()) {
        NkTextureDesc ad = NkTextureDesc::Tex2D(
            (uint32)loadedTextureImage->Width(),
            (uint32)loadedTextureImage->Height(),
            NkGPUFormat::NK_RGBA8_UNORM, 1);
        ad.bindFlags=NkBindFlags::NK_SHADER_RESOURCE;
        hAlbedoTex = device->CreateTexture(ad);
        if (hAlbedoTex.IsValid())
            device->WriteTexture(hAlbedoTex, loadedTextureImage->Pixels(),
                                 (uint32)loadedTextureImage->Stride());

        NkSamplerDesc asd{};
        asd.magFilter=NkFilter::NK_LINEAR; asd.minFilter=NkFilter::NK_LINEAR;
        asd.mipFilter=NkMipFilter::NK_NONE;
        // FIX [6] : REPEAT pour le tiling UV du plan (kPlaneUVScale > 1)
        asd.addressU=asd.addressV=asd.addressW=NkAddressMode::NK_REPEAT;
        hAlbedoSampler = device->CreateSampler(asd);

        for (int i=0;i<3;++i) {
            if (!shaderNeedsAlbedoSampler) break;
            if (!hDescSet[i].IsValid()||!hAlbedoTex.IsValid()||!hAlbedoSampler.IsValid()) continue;
            NkDescriptorWrite tw{}; tw.set=hDescSet[i]; tw.binding=2;
            tw.type=NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER;
            tw.texture=hAlbedoTex; tw.sampler=hAlbedoSampler;
            tw.textureLayout=NkResourceState::NK_SHADER_READ;
            device->UpdateDescriptorSets(&tw,1);
        }
    }

    // ── Software backend : shaders CPU ───────────────────────────────────────
    if (targetApi == NkGraphicsApi::NK_API_SOFTWARE) {
        NkSoftwareDevice* swDev = static_cast<NkSoftwareDevice*>(device);
        if (hShadowShader.IsValid()) {
            NkSWShader* swSh = swDev->GetShader(hShadowShader.id);
            if (swSh) {
                swSh->vertFn = [](const void* vdata, uint32 idx, const void* udata) -> NkSWVertex {
                    const Vtx3D* v = static_cast<const Vtx3D*>(vdata)+idx;
                    const UboData* ubo = static_cast<const UboData*>(udata);
                    NkSWVertex out;
                    auto mul4=[](const float m[16],float x,float y,float z,float w)->NkVec4f{
                        return NkVec4f(m[0]*x+m[4]*y+m[8]*z+m[12]*w,
                                       m[1]*x+m[5]*y+m[9]*z+m[13]*w,
                                       m[2]*x+m[6]*y+m[10]*z+m[14]*w,
                                       m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                    };
                    NkVec4f wp  = mul4(ubo->model,  v->pos.x,v->pos.y,v->pos.z,1.f);
                    NkVec4f lsp = mul4(ubo->lightVP,wp.x,wp.y,wp.z,wp.w);
                    out.position = lsp;
                    return out;
                };
                swSh->fragFn = nullptr;
            }
        }

        NkSWShader*  sw           = swDev->GetShader(hShader.id);
        NkSWTexture* swShadowTex  = hasShadowMap      ? swDev->GetTex(hShadowTex.id)  : nullptr;
        NkSWTexture* swAlbedoTex  = hAlbedoTex.IsValid()? swDev->GetTex(hAlbedoTex.id): nullptr;

        if (sw) {
            sw->vertFn = [](const void* vdata, uint32 idx, const void* udata) -> NkSWVertex {
                const Vtx3D*   v   = static_cast<const Vtx3D*>(vdata)+idx;
                const UboData* ubo = static_cast<const UboData*>(udata);
                NkSWVertex out;
                auto mul4=[](const float m[16],float x,float y,float z,float w)->NkVec4f{
                    return NkVec4f(m[0]*x+m[4]*y+m[8]*z+m[12]*w,
                                   m[1]*x+m[5]*y+m[9]*z+m[13]*w,
                                   m[2]*x+m[6]*y+m[10]*z+m[14]*w,
                                   m[3]*x+m[7]*y+m[11]*z+m[15]*w);
                };
                NkVec4f wp = mul4(ubo->model,  v->pos.x,v->pos.y,v->pos.z,1.f);
                NkVec4f vp = mul4(ubo->view,   wp.x,wp.y,wp.z,wp.w);
                NkVec4f cp = mul4(ubo->proj,   vp.x,vp.y,vp.z,vp.w);
                out.position = cp;
                float nx=ubo->model[0]*v->normal.x+ubo->model[4]*v->normal.y+ubo->model[8]*v->normal.z;
                float ny=ubo->model[1]*v->normal.x+ubo->model[5]*v->normal.y+ubo->model[9]*v->normal.z;
                float nz=ubo->model[2]*v->normal.x+ubo->model[6]*v->normal.y+ubo->model[10]*v->normal.z;
                float nl=NkSqrt(nx*nx+ny*ny+nz*nz);
                if (nl>0.001f) {nx/=nl;ny/=nl;nz/=nl;}
                out.normal={nx,ny,nz};
                out.color={v->color.x,v->color.y,v->color.z,1.f};
                out.attrs[0]=wp.x; out.attrs[1]=wp.y; out.attrs[2]=wp.z;
                NkVec4f lsc=mul4(ubo->lightVP,wp.x,wp.y,wp.z,wp.w);
                out.attrs[3]=lsc.x; out.attrs[4]=lsc.y;
                out.attrs[5]=lsc.z; out.attrs[6]=lsc.w;
                out.attrs[7]=v->uv.x;
                out.attrs[8]=v->uv.y;
                // FIX [5] : hasTexture dans attrs[9]
                out.attrs[9]=v->color.w;
                return out;
            };

            sw->fragFn = [swShadowTex,swAlbedoTex](const NkSWVertex& frag, const void* udata, const void*) -> math::NkVec4f {
                const UboData* ubo = static_cast<const UboData*>(udata);
                float nx=frag.normal.x,ny=frag.normal.y,nz=frag.normal.z;
                float lx=-ubo->lightDirW[0],ly=-ubo->lightDirW[1],lz=-ubo->lightDirW[2];
                float ll2=lx*lx+ly*ly+lz*lz;
                if (ll2>1e-6f) {float il=1.f/NkSqrt(ll2);lx*=il;ly*=il;lz*=il;}
                float diff=nx*lx+ny*ly+nz*lz; if(diff<0.f)diff=0.f;
                float vx=ubo->eyePosW[0]-frag.attrs[0];
                float vy=ubo->eyePosW[1]-frag.attrs[1];
                float vz=ubo->eyePosW[2]-frag.attrs[2];
                float vl2=vx*vx+vy*vy+vz*vz;
                if (vl2>1e-6f) {float iv=1.f/NkSqrt(vl2);vx*=iv;vy*=iv;vz*=iv;}
                float hx=lx+vx,hy=ly+vy,hz=lz+vz;
                float hl2=hx*hx+hy*hy+hz*hz;
                if (hl2>1e-6f) {float ih=1.f/NkSqrt(hl2);hx*=ih;hy*=ih;hz*=ih;}
                float sp=nx*hx+ny*hy+nz*hz; if(sp<0.f)sp=0.f;
                float spec=sp*sp;spec*=spec;spec*=spec;spec*=spec;
                float br=frag.color.x,bg=frag.color.y,bb=frag.color.z;
                // FIX [5] : hasTexture depuis attrs[9]
                if (swAlbedoTex && frag.attrs[9]>0.5f) {
                    float u=NkClamp(frag.attrs[7],0.f,1.f);
                    float v=NkClamp(frag.attrs[8],0.f,1.f);
                    // FIX [6] : tiling UV pour SW
                    u = NkFmod(u * kPlaneUVScale, 1.0f);
                    v = NkFmod(v * kPlaneUVScale, 1.0f);
                    NkVec4f tex=swAlbedoTex->Sample(u,v,0);
                    br*=tex.x; bg*=tex.y; bb*=tex.z;
                }
                float shadow=1.f;
                if (swShadowTex) {
                    float sw4=frag.attrs[6], invW=(sw4!=0.f)?1.f/sw4:1.f;
                    float sx=frag.attrs[3]*invW, sy=frag.attrs[4]*invW, sz=frag.attrs[5]*invW;
                    float u=sx*.5f+.5f, v=-sy*.5f+.5f;
                    float cmpZ=sz*.5f+.5f-.002f;
                    if (u>=0.f&&u<=1.f&&v>=0.f&&v<=1.f&&cmpZ<=1.f) {
                        uint32 px=(uint32)(u*(float)swShadowTex->Width());
                        uint32 py=(uint32)(v*(float)swShadowTex->Height());
                        if (px>=swShadowTex->Width())  px=swShadowTex->Width()-1;
                        if (py>=swShadowTex->Height()) py=swShadowTex->Height()-1;
                        shadow=(cmpZ<=swShadowTex->Read(px,py).r)?1.f:0.f;
                    }
                }
                shadow=shadow<.35f?.35f:shadow;
                float r=.15f*br+shadow*(diff*br+spec*.4f);
                float g=.15f*bg+shadow*(diff*bg+spec*.4f);
                float b=.15f*bb+shadow*(diff*bb+spec*.4f);
                r=r<0.f?0.f:(r>1.f?1.f:r);
                g=g<0.f?0.f:(g>1.f?1.f:g);
                b=b<0.f?0.f:(b>1.f?1.f:b);
                return {r,g,b,1.f};
            };
        }
    }

    // ── Texte ─────────────────────────────────────────────────────────────────
    NkFTFontLibrary ftLib;
    NkFTFontFace*   ftFace18=nullptr;
    NkFTFontFace*   ftFace32=nullptr;
    std::vector<uint8_t> fontBytes;
    {
        const std::string fp = FindFontFile();
        if (!fp.empty()) fontBytes = LoadFileBytes(fp);
        if (!fontBytes.empty()) {
            ftLib.Init();
            ftFace18 = ftLib.LoadFont(fontBytes.data(),(usize)fontBytes.size(),18);
            ftFace32 = ftLib.LoadFont(fontBytes.data(),(usize)fontBytes.size(),32);
        }
    }

    NkDescriptorSetLayoutDesc textLayoutDesc;
    textLayoutDesc.Add(0,NkDescriptorType::NK_UNIFORM_BUFFER,NkShaderStage::NK_ALL_GRAPHICS);
    textLayoutDesc.Add(1,NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,NkShaderStage::NK_FRAGMENT);
    NkDescSetHandle hTextLayout = device->CreateDescriptorSetLayout(textLayoutDesc);

    NkShaderDesc  textShaderDesc = MakeTextShaderDesc(targetApi);
    NkShaderHandle hTextShader   = device->CreateShader(textShaderDesc);

    NkVertexLayout textVtxLayout;
    textVtxLayout
        .AddAttribute(0,0,NkGPUFormat::NK_RG32_FLOAT,0,              "POSITION",0)
        .AddAttribute(1,0,NkGPUFormat::NK_RG32_FLOAT,2*sizeof(float),"TEXCOORD",0)
        .AddBinding(0,sizeof(TextVtx));

    NkPipelineHandle hTextPipe;
    if (hTextShader.IsValid()&&hTextLayout.IsValid()) {
        NkGraphicsPipelineDesc tpd;
        tpd.shader=hTextShader; tpd.vertexLayout=textVtxLayout;
        tpd.topology=NkPrimitiveTopology::NK_TRIANGLE_LIST;
        tpd.rasterizer=NkRasterizerDesc::Default();
        tpd.depthStencil=NkDepthStencilDesc::NoDepth();
        tpd.blend=NkBlendDesc::Alpha();
        tpd.renderPass=hRP; tpd.debugName="PipelineText";
        tpd.descriptorSetLayouts.PushBack(hTextLayout);
        hTextPipe=device->CreateGraphicsPipeline(tpd);
    }

    NkTextQuad tqBackend={}, tqFPS={}, tqCube={}, tqSphere={}, tqFloor={};
    bool textOk = hTextPipe.IsValid() && hTextLayout.IsValid();

    if (textOk) {
        NkFTFontFace* f18=(ftFace18&&ftFace18->IsValid())?ftFace18:nullptr;
        NkFTFontFace* f32=(ftFace32&&ftFace32->IsValid())?ftFace32:nullptr;
        if (f32) {
            tqBackend=CreateTextQuad(device,hTextLayout,f32,apiName,false);
            tqCube   =CreateTextQuad(device,hTextLayout,f32,"CUBE",  true);
            tqSphere =CreateTextQuad(device,hTextLayout,f32,"SPHERE",true);
            tqFloor  =CreateTextQuad(device,hTextLayout,f32,"FLOOR", true);
        }
        if (f18) tqFPS=CreateTextQuad(device,hTextLayout,f18,"FPS: --",false);
    }

    float  fpsTimer=0.f;
    uint32 fpsFrameCount=0;

    // ── Command buffer ────────────────────────────────────────────────────────
    NkICommandBuffer* cmd = device->CreateCommandBuffer(NkCommandBufferType::NK_GRAPHICS);
    if (!cmd||!cmd->IsValid()) {
        logger.Info("[RHIFullDemo] Command buffer creation failed\n");
        NkDeviceFactory::Destroy(device); window.Close(); return 1;
    }

    bool  running   = true;
    float rotAngle  = 0.f;
    float camYaw    = 0.f, camPitch=20.f, camDist=4.f;
    float lightYaw  = -45.f, lightPitch=-30.f;
    bool  keys[512] = {};
    bool  requestSaveScene=false, requestSaveSource=false;
    uint32 saveSceneCounter=0, saveSourceCounter=0;
    const std::filesystem::path capturesDir =
        std::filesystem::path("Build") / "Captures" / "NkRHIDemoFullImage";

    NkClock clock;
    NkEventSystem& events = NkEvents();

    events.AddEventCallback<NkWindowCloseEvent>([&](NkWindowCloseEvent*) { running=false; });
    events.AddEventCallback<NkKeyPressEvent>([&](NkKeyPressEvent* e) {
        if ((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=true;
        if (e->GetKey()==NkKey::NK_ESCAPE) running=false;
        if (e->GetKey()==NkKey::NK_F5)  requestSaveScene=true;
        if (e->GetKey()==NkKey::NK_F6)  requestSaveSource=true;
    });
    events.AddEventCallback<NkKeyReleaseEvent>([&](NkKeyReleaseEvent* e) {
        if ((uint32)e->GetKey()<512) keys[(uint32)e->GetKey()]=false;
    });
    events.AddEventCallback<NkWindowResizeEvent>([&](NkWindowResizeEvent* e) {
        W=(uint32)e->GetWidth(); H=(uint32)e->GetHeight();
    });

    // ==========================================================================
    // Boucle principale
    // ==========================================================================
    while (running) {
        events.PollEvents();
        if (!running) break;
        if (!W||!H) continue;
        if (W!=device->GetSwapchainWidth()||H!=device->GetSwapchainHeight())
            device->OnResize(W,H);

        float dt = clock.Tick().delta;
        if (dt<=0.f||dt>0.25f) dt=1.f/60.f;

        fpsFrameCount++;
        fpsTimer+=dt;
        if (fpsTimer>=0.5f && textOk) {
            float fps=(fpsTimer>0.f)?(float)fpsFrameCount/fpsTimer:0.f;
            fpsFrameCount=0; fpsTimer=0.f;
            if (ftFace18&&ftFace18->IsValid()&&hTextLayout.IsValid()) {
                char buf[32]; snprintf(buf,sizeof(buf),"FPS: %.0f",fps);
                DestroyTextQuad(device,tqFPS);
                tqFPS=CreateTextQuad(device,hTextLayout,ftFace18,buf,false);
            }
        }

        const float camSpd=60.f, lightSpd=90.f;
        if (keys[(uint32)NkKey::NK_A]) camYaw   -=camSpd*dt;
        if (keys[(uint32)NkKey::NK_D]) camYaw   +=camSpd*dt;
        if (keys[(uint32)NkKey::NK_W]) camPitch +=camSpd*dt;
        if (keys[(uint32)NkKey::NK_S]) camPitch -=camSpd*dt;
        if (keys[(uint32)NkKey::NK_LEFT])  lightYaw  -=lightSpd*dt;
        if (keys[(uint32)NkKey::NK_RIGHT]) lightYaw  +=lightSpd*dt;
        if (keys[(uint32)NkKey::NK_UP])    lightPitch+=lightSpd*dt;
        if (keys[(uint32)NkKey::NK_DOWN])  lightPitch-=lightSpd*dt;
        camPitch=NkClamp(camPitch,-80.f,80.f);
        rotAngle+=45.f*dt;

        float aspect=(H>0)?(float)W/(float)H:1.f;
        float eyeX=camDist*NkCos(NkToRadians(camPitch))*NkSin(NkToRadians(camYaw));
        float eyeY=camDist*NkSin(NkToRadians(camPitch));
        float eyeZ=camDist*NkCos(NkToRadians(camPitch))*NkCos(NkToRadians(camYaw));
        NkVec3f eye(eyeX,eyeY,eyeZ);
        NkMat4f matView=NkMat4f::LookAt(eye,NkVec3f(0,0,0),NkVec3f(0,1,0));
        NkMat4f matProj=NkMat4f::Perspective(NkAngle(60.f),aspect,0.1f,100.f);

        float lx=NkCos(NkToRadians(lightPitch))*NkSin(NkToRadians(lightYaw));
        float ly=NkSin(NkToRadians(lightPitch));
        float lz=NkCos(NkToRadians(lightPitch))*NkCos(NkToRadians(lightYaw));
        NkVec3f lightDir(lx,ly,lz);
        NkVec3f lightPos(-lightDir.x*10.f,-lightDir.y*10.f,-lightDir.z*10.f);
        NkVec3f lightUp=(NkFabs(lightDir.y)>0.9f)?NkVec3f(1,0,0):NkVec3f(0,1,0);
        NkMat4f matLightView=NkMat4f::LookAt(lightPos,NkVec3f(0,0,0),lightUp);
        NkMat4f matLightProj=NkMat4f::Orthogonal(NkVec2f(-5,-5),NkVec2f(5,5),1.f,20.f,depthZeroToOne);
        NkMat4f matLightVP=matLightProj*matLightView;

        NkFrameContext frame;
        if (!device->BeginFrame(frame)) continue;
        W=device->GetSwapchainWidth(); H=device->GetSwapchainHeight();
        if (!W||!H) { device->EndFrame(frame); continue; }

        NkRenderPassHandle latestRP=device->GetSwapchainRenderPass();
        if (latestRP.IsValid()&&latestRP.id!=hRP.id) {
            hRP=latestRP; pipeDesc.renderPass=hRP;
            if (hPipe.IsValid()) device->DestroyPipeline(hPipe);
            hPipe=device->CreateGraphicsPipeline(pipeDesc);
            if (!hPipe.IsValid()) { device->EndFrame(frame); continue; }
        }

        NkFramebufferHandle hFBO=device->GetSwapchainFramebuffer();
        cmd->Reset();
        if (!cmd->Begin()) { device->EndFrame(frame); continue; }

        // ── Passe 1 : Shadow ────────────────────────────────────────────────
        if (hasShadowMap&&hShadowFBO.IsValid()&&hShadowRP.IsValid()) {
            NkRect2D sa{0,0,(int32)kShadowSize,(int32)kShadowSize};
            bool ok=cmd->BeginRenderPass(hShadowRP,hShadowFBO,sa);
            if (ok&&useRealShadowPass&&hShadowPipe.IsValid()) {
                NkViewport svp{0.f,0.f,(float)kShadowSize,(float)kShadowSize,0.f,1.f};
                cmd->SetViewport(svp); cmd->SetScissor(sa);
                cmd->BindGraphicsPipeline(hShadowPipe);
                auto fillShadow=[&](UboData& su, const NkMat4f& mm){
                    NkMat4f id=NkMat4f::Identity();
                    Mat4ToArray(mm,su.model); Mat4ToArray(matLightVP,su.lightVP);
                    Mat4ToArray(id,su.view);  Mat4ToArray(id,su.proj);
                    su.ndcZScale=ndcZScale; su.ndcZOffset=ndcZOffset;
                };
                { NkMat4f mm=NkMat4f::RotationY(NkAngle(rotAngle))*NkMat4f::RotationX(NkAngle(rotAngle*.5f));
                  UboData su{}; fillShadow(su,mm);
                  device->WriteBuffer(hUBO[0],&su,sizeof(su));
                  if(hDescSet[0].IsValid())cmd->BindDescriptorSet(hDescSet[0],0);
                  cmd->BindVertexBuffer(0,hCube); cmd->Draw((uint32)cubeVerts.Size()); }
                { NkMat4f mm=NkMat4f::Translation(NkVec3f(2,0,0));
                  UboData su{}; fillShadow(su,mm);
                  device->WriteBuffer(hUBO[1],&su,sizeof(su));
                  if(hDescSet[1].IsValid())cmd->BindDescriptorSet(hDescSet[1],0);
                  cmd->BindVertexBuffer(0,hSphere); cmd->Draw((uint32)sphereVerts.Size()); }
                { NkMat4f mm=NkMat4f::Translation(NkVec3f(0,-1,0));
                  UboData su{}; fillShadow(su,mm);
                  device->WriteBuffer(hUBO[2],&su,sizeof(su));
                  if(hDescSet[2].IsValid())cmd->BindDescriptorSet(hDescSet[2],0);
                  cmd->BindVertexBuffer(0,hPlane); cmd->Draw((uint32)planeVerts.Size()); }
            }
            if (ok) cmd->EndRenderPass();
            if (ok&&hShadowTex.IsValid()) {
                NkTextureBarrier b{};
                b.texture=hShadowTex;
                b.stateBefore=NkResourceState::NK_DEPTH_WRITE;
                b.stateAfter=NkResourceState::NK_DEPTH_READ;
                b.srcStage=NkPipelineStage::NK_LATE_FRAGMENT;
                b.dstStage=NkPipelineStage::NK_FRAGMENT_SHADER;
                cmd->Barrier(nullptr,0,&b,1);
            }
        }

        // ── Passe 2 : Rendu principal ────────────────────────────────────────
        NkRect2D area{0,0,(int32)W,(int32)H};
        if (!cmd->BeginRenderPass(hRP,hFBO,area)) {
            cmd->End();
            if (targetApi==NkGraphicsApi::NK_API_VULKAN&&W>0&&H>0) device->OnResize(W,H);
            device->EndFrame(frame); continue;
        }
        NkViewport vp{0.f,0.f,(float)W,(float)H,0.f,1.f};
        cmd->SetViewport(vp); cmd->SetScissor(area);
        cmd->BindGraphicsPipeline(hPipe);

        auto fillMain=[&](UboData& ubo, const NkMat4f& model) {
            Mat4ToArray(model,ubo.model); Mat4ToArray(matView,ubo.view);
            Mat4ToArray(matProj,ubo.proj); Mat4ToArray(matLightVP,ubo.lightVP);
            ubo.lightDirW[0]=lightDir.x; ubo.lightDirW[1]=lightDir.y;
            ubo.lightDirW[2]=lightDir.z; ubo.lightDirW[3]=0.f;
            ubo.eyePosW[0]=eye.x; ubo.eyePosW[1]=eye.y;
            ubo.eyePosW[2]=eye.z; ubo.eyePosW[3]=0.f;
            ubo.ndcZScale=ndcZScale; ubo.ndcZOffset=ndcZOffset;
        };

        { NkMat4f mm=NkMat4f::RotationY(NkAngle(rotAngle))*NkMat4f::RotationX(NkAngle(rotAngle*.5f));
          UboData ubo{}; fillMain(ubo,mm);
          device->WriteBuffer(hUBO[0],&ubo,sizeof(ubo));
          if(hDescSet[0].IsValid())cmd->BindDescriptorSet(hDescSet[0],0);
          cmd->BindVertexBuffer(0,hCube); cmd->Draw((uint32)cubeVerts.Size()); }

        { NkMat4f mm=NkMat4f::Translation(NkVec3f(2,0,0));
          UboData ubo{}; fillMain(ubo,mm);
          device->WriteBuffer(hUBO[1],&ubo,sizeof(ubo));
          if(hDescSet[1].IsValid())cmd->BindDescriptorSet(hDescSet[1],0);
          cmd->BindVertexBuffer(0,hSphere); cmd->Draw((uint32)sphereVerts.Size()); }

        { NkMat4f mm=NkMat4f::Translation(NkVec3f(0,-1,0));
          UboData ubo{}; fillMain(ubo,mm);
          device->WriteBuffer(hUBO[2],&ubo,sizeof(ubo));
          if(hDescSet[2].IsValid())cmd->BindDescriptorSet(hDescSet[2],0);
          cmd->BindVertexBuffer(0,hPlane); cmd->Draw((uint32)planeVerts.Size()); }

        // ── Texte 3D billboard ───────────────────────────────────────────────
        if (textOk) {
            // FIX [7] : flipBillY unifié — même logique que flipOrthoY (toujours true)
            // La convention clip-Y est la même pour tous les backends une fois
            // que matProj est correct. On flip le billboard uniquement pour les
            // backends qui ont nativement le clip-Y inversé (Vulkan, DX12).
            const bool flipBillY = (targetApi==NkGraphicsApi::NK_API_VULKAN ||
                                    targetApi==NkGraphicsApi::NK_API_DIRECTX12);
            float bbMVP[16];
            if (tqCube.vtxCount>0) {
                Billboard3DMVP(NkVec3f(0,.9f,0),1.f,.25f,matView,matProj,flipBillY,bbMVP);
                RenderTextQuad(cmd,device,tqCube,hTextPipe,bbMVP,1.f,.85f,.3f,1.f);
            }
            if (tqSphere.vtxCount>0) {
                Billboard3DMVP(NkVec3f(2,.7f,0),1.4f,.25f,matView,matProj,flipBillY,bbMVP);
                RenderTextQuad(cmd,device,tqSphere,hTextPipe,bbMVP,.4f,.85f,1.f,1.f);
            }
            if (tqFloor.vtxCount>0) {
                Billboard3DMVP(NkVec3f(-1.5f,-.7f,1.5f),1.2f,.25f,matView,matProj,flipBillY,bbMVP);
                RenderTextQuad(cmd,device,tqFloor,hTextPipe,bbMVP,.4f,1.f,.5f,1.f);
            }
        }

        // ── Texte 2D overlay ─────────────────────────────────────────────────
        // FIX [1][2] : on supprime le paramètre flipOrthoY et on le force dans Ortho2DMVP
        if (textOk) {
            float oMVP[16];
            const float fw=(float)W, fh=(float)H;
            if (tqBackend.vtxCount>0) {
                float tw=(float)tqBackend.texW, th=(float)tqBackend.texH;
                Ortho2DMVP(10.f, fh-th-10.f, tw, th, fw, fh, depthZeroToOne, oMVP);
                RenderTextQuad(cmd,device,tqBackend,hTextPipe,oMVP,1.f,1.f,0.f,1.f);
            }
            if (tqFPS.vtxCount>0) {
                float tw=(float)tqFPS.texW, th=(float)tqFPS.texH;
                Ortho2DMVP(fw-tw-10.f, fh-th-10.f, tw, th, fw, fh, depthZeroToOne, oMVP);
                RenderTextQuad(cmd,device,tqFPS,hTextPipe,oMVP,0.f,1.f,0.f,1.f);
            }
        }

        cmd->EndRenderPass();
        cmd->End();
        device->SubmitAndPresent(cmd);
        device->EndFrame(frame);

        if (requestSaveSource) {
            requestSaveSource=false;
            if (loadedTextureImage&&loadedTextureImage->IsValid()) {
                ++saveSourceCounter;
                std::string stem=NkFormat("source_{0}",(unsigned long long)saveSourceCounter).CStr();
                SaveImageVariants(*loadedTextureImage,capturesDir,stem);
            }
        }
        if (requestSaveScene) {
            requestSaveScene=false; ++saveSceneCounter;
            std::string stem=NkFormat("scene_{0}",(unsigned long long)saveSceneCounter).CStr();
            if (targetApi==NkGraphicsApi::NK_API_SOFTWARE)
                SaveSoftwareScene(static_cast<NkSoftwareDevice*>(device),capturesDir,stem);
        }
    }

    // ==========================================================================
    // Nettoyage
    // ==========================================================================
    device->WaitIdle();
    device->DestroyCommandBuffer(cmd);
    for (int i=0;i<3;i++) if(hDescSet[i].IsValid()) device->FreeDescriptorSet(hDescSet[i]);
    if (hLayout.IsValid())       device->DestroyDescriptorSetLayout(hLayout);
    for (int i=0;i<3;i++) if(hUBO[i].IsValid()) device->DestroyBuffer(hUBO[i]);
    if (hPlane.IsValid())        device->DestroyBuffer(hPlane);
    if (hSphere.IsValid())       device->DestroyBuffer(hSphere);
    if (hCube.IsValid())         device->DestroyBuffer(hCube);
    device->DestroyPipeline(hPipe);
    device->DestroyShader(hShader);
    if (hShadowPipe.IsValid())    device->DestroyPipeline(hShadowPipe);
    if (hShadowShader.IsValid())  device->DestroyShader(hShadowShader);
    if (hShadowFBO.IsValid())     device->DestroyFramebuffer(hShadowFBO);
    if (hShadowRP.IsValid())      device->DestroyRenderPass(hShadowRP);
    if (hShadowSampler.IsValid()) device->DestroySampler(hShadowSampler);
    if (hShadowTex.IsValid())     device->DestroyTexture(hShadowTex);
    if (hAlbedoSampler.IsValid()) device->DestroySampler(hAlbedoSampler);
    if (hAlbedoTex.IsValid())     device->DestroyTexture(hAlbedoTex);
    if (loadedTextureImage)       { loadedTextureImage->Free(); loadedTextureImage=nullptr; }
    DestroyTextQuad(device,tqFPS);
    DestroyTextQuad(device,tqBackend);
    DestroyTextQuad(device,tqFloor);
    DestroyTextQuad(device,tqSphere);
    DestroyTextQuad(device,tqCube);
    if (hTextPipe.IsValid())   device->DestroyPipeline(hTextPipe);
    if (hTextShader.IsValid()) device->DestroyShader(hTextShader);
    if (hTextLayout.IsValid()) device->DestroyDescriptorSetLayout(hTextLayout);
    if (ftFace18) ftLib.FreeFont(ftFace18);
    if (ftFace32) ftLib.FreeFont(ftFace32);
    ftLib.Destroy();
    NkDeviceFactory::Destroy(device);
    window.Close();
    logger.Info("[RHIFullDemo] Terminated cleanly.\n");
    return 0;
}