// =============================================================================
// NkRender2D.cpp  — NKRenderer v5.0
// =============================================================================
#include "NkRender2D.h"
#include "NKRenderer/Shader/NkShaderLibrary.h"
#include "NKLogger/NkLog.h"
#include <cmath>
#ifndef NK_PI
#define NK_PI 3.14159265358979f
#endif

namespace nkentseu {
namespace renderer {

    // =========================================================================
    // Shaders 2D embarques en GLSL OpenGL-natif (pas de syntaxe Vulkan-GLSL).
    // Conventions :
    //  - Push constants : uniform vec4 _PushConstants[4] (matriceortho stockee
    //    comme 4 vec4) — c'est la convention que le backend GL reconnait
    //    (cf. NkOpenglCommandBuffer::PushConstants).
    //  - Texture sampler : layout(binding=0) — pas de "set" Vulkan.
    // =========================================================================
    static const char* kRender2D_VS = R"GLSL(
#version 460 core
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(location=3) in uint  aFlags;

layout(location=0) out vec2  vUV;
layout(location=1) out vec4  vColor;
layout(location=2) out flat uint vFlags;
layout(location=3) out vec2  vWorldPos;   // pour le lit fragment shader

uniform vec4 _PushConstants[4];   // mat4 stockee en 4 vec4 column-major

void main() {
    mat4 ortho = mat4(_PushConstants[0], _PushConstants[1],
                      _PushConstants[2], _PushConstants[3]);
    gl_Position = ortho * vec4(aPos, 0.0, 1.0);
    vUV       = aUV;
    vWorldPos = aPos;   // espace pre-ortho = espace 'world 2D' (pixels logiques)
    vColor = vec4(float((aColor      ) & 0xFFu) / 255.0,
                  float((aColor >>  8u) & 0xFFu) / 255.0,
                  float((aColor >> 16u) & 0xFFu) / 255.0,
                  float((aColor >> 24u) & 0xFFu) / 255.0);
    vFlags = aFlags;
}
)GLSL";

    static const char* kRender2D_FS = R"GLSL(
#version 460 core
layout(location=0) in vec2  vUV;
layout(location=1) in vec4  vColor;
layout(location=2) in flat uint vFlags;
layout(location=3) in vec2  vWorldPos;
layout(location=0) out vec4 fragColor;

layout(binding=0) uniform sampler2D tAtlas;

// Phase E : Lights 2D UBO. Layout std140.
//   Light : position xy + intensity z + radius w (vec4)
//           color rgb + cookieIdx w (vec4, -1 = pas de cookie)
//           angleRad x + coneInnerRad y + coneOuterRad z + _pad w (vec4)
struct Light2D {
    vec4 posIntRad;     // xy=position, z=intensity, w=radius
    vec4 colorCookie;   // xyz=color, w=cookieIdx (-1 = none)
    vec4 dirCone;       // x=angleRad, y=coneInnerRad, z=coneOuterRad, w=pad
};
layout(std140, binding=1) uniform Lights2DUBO {
    Light2D lights[16];
    vec4    ambient;       // xyz=ambient color, w=light count (cast a int)
} uL2D;

// E.5 : shadow casters (cercles). 32 max.
//   xy = position, z = radius, w = unused
// E.7d : isoMat = transform 2x2 applique aux positions avant raycast (default
//   identity = pas de transform). Permet d'avoir des ombres iso-correctes.
layout(std140, binding=10) uniform Shadows2DUBO {
    vec4 occluders[32];
    vec4 meta;            // x = count actif, yzw unused
    vec4 isoMat;          // x=m00, y=m01, z=m10, w=m11 (default 1,0,0,1)
} uSh2D;

// E.7a : AABB shadow casters (walls / plateformes). 32 max.
//   xy = min, zw = max
layout(std140, binding=11) uniform ShadowsAABB2DUBO {
    vec4 boxes[32];       // 32 AABBs
    vec4 metaAABB;        // x = count actif, yzw unused
} uShAABB2D;

// Test segment-vs-cercle : retourne true si le segment [a..b] intersecte
// le disque (c, r). Utilise pour le shadow raycaste de fragment a light.
// Ignore self-occlusion : on saute les cercles dont le centre est tres pres
// du fragment (caster qui se recouvre lui-meme).
bool SegmentIntersectsCircle(vec2 a, vec2 b, vec2 c, float r) {
    vec2 ab = b - a;
    vec2 ac = c - a;
    float lab2 = dot(ab, ab);
    if (lab2 < 1e-6) return false;
    // Projete c sur la droite ab, clamp [0,1] pour rester sur le segment
    float t = clamp(dot(ac, ab) / lab2, 0.0, 1.0);
    vec2 closest = a + ab * t;
    vec2 d = c - closest;
    return dot(d, d) <= r * r;
}

// Test segment-vs-AABB (slab method). Retourne true si le segment [a..b]
// traverse l'AABB (mn, mx). Standard ray-AABB adapte aux segments finis.
bool SegmentIntersectsAABB(vec2 a, vec2 b, vec2 mn, vec2 mx) {
    vec2 dir = b - a;
    float tmin = 0.0;
    float tmax = 1.0;
    for (int i = 0; i < 2; i++) {
        if (abs(dir[i]) < 1e-6) {
            if (a[i] < mn[i] || a[i] > mx[i]) return false;
        } else {
            float inv = 1.0 / dir[i];
            float t1 = (mn[i] - a[i]) * inv;
            float t2 = (mx[i] - a[i]) * inv;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            tmin = max(tmin, t1);
            tmax = min(tmax, t2);
            if (tmin > tmax) return false;
        }
    }
    return true;
}

float ShadowVisibility2D(vec2 fragPos, vec2 lightPos) {
    // E.7d : iso transform applique aux positions avant raycast. Default
    // identity -> aucun changement. Pour iso 2:1 ou sol incline : remappe
    // les coords screen vers un espace ou les directions sont "rectifiees".
    mat2 isoMat = mat2(uSh2D.isoMat.x, uSh2D.isoMat.y,
                       uSh2D.isoMat.z, uSh2D.isoMat.w);
    vec2 fragI  = isoMat * fragPos;
    vec2 lightI = isoMat * lightPos;

    // Reduit le segment de 1px cote light pour eviter qu'un caster bloque
    // sa propre lumiere quand light est posee dessus.
    vec2 dir   = lightI - fragI;
    float L    = length(dir);
    if (L < 1.0) return 1.0;
    vec2 ndir  = dir / L;
    vec2 b     = lightI - ndir * 1.0;

    // 1) Test cercles (occluder.xy transforme aussi par isoMat)
    int n = int(uSh2D.meta.x);
    for (int i = 0; i < n && i < 32; i++) {
        vec4 occ = uSh2D.occluders[i];
        vec2 occI = isoMat * occ.xy;
        // Skip caster qui contient le fragment (iso-corrige)
        vec2 oc = occI - fragI;
        if (dot(oc, oc) <= occ.z * occ.z) continue;
        if (SegmentIntersectsCircle(fragI, b, occI, occ.z))
            return 0.0;
    }

    // 2) Test AABBs (box.xy/zw transformes aussi)
    int m = int(uShAABB2D.metaAABB.x);
    for (int j = 0; j < m && j < 32; j++) {
        vec4 box = uShAABB2D.boxes[j];
        vec2 mn = isoMat * box.xy;
        vec2 mx = isoMat * box.zw;
        // Apres iso transform, mn peut etre > mx sur un axe : reorder.
        vec2 mnR = vec2(min(mn.x, mx.x), min(mn.y, mx.y));
        vec2 mxR = vec2(max(mn.x, mx.x), max(mn.y, mx.y));
        if (fragI.x >= mnR.x && fragI.x <= mxR.x &&
            fragI.y >= mnR.y && fragI.y <= mxR.y) continue;
        if (SegmentIntersectsAABB(fragI, b, mnR, mxR))
            return 0.0;
    }
    return 1.0;
}

// Cookies textures (slots 2..9 = jusqu'a 8 cookies). -1 = pas de cookie.
layout(binding=2) uniform sampler2D tCookie0;
layout(binding=3) uniform sampler2D tCookie1;
layout(binding=4) uniform sampler2D tCookie2;
layout(binding=5) uniform sampler2D tCookie3;
layout(binding=6) uniform sampler2D tCookie4;
layout(binding=7) uniform sampler2D tCookie5;
layout(binding=8) uniform sampler2D tCookie6;
layout(binding=9) uniform sampler2D tCookie7;

// E.7c : normal map atlas pour fake-3D shading sur sprites 2D. Le bit 8 du
// flags active le sampling. Format : RGB tangent space normal en [0,1] →
// remappe a [-1,1] dans le shader. Z > 0 (vers le viewer).
layout(binding=12) uniform sampler2D tNormal;

float SampleCookie(int idx, vec2 uv) {
    // uv attendu en [0,1] x [0,1] (centre = (0.5, 0.5)).
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    if (idx == 0) return texture(tCookie0, uv).r;
    if (idx == 1) return texture(tCookie1, uv).r;
    if (idx == 2) return texture(tCookie2, uv).r;
    if (idx == 3) return texture(tCookie3, uv).r;
    if (idx == 4) return texture(tCookie4, uv).r;
    if (idx == 5) return texture(tCookie5, uv).r;
    if (idx == 6) return texture(tCookie6, uv).r;
    if (idx == 7) return texture(tCookie7, uv).r;
    return 1.0;
}

// E.7c : profondeur "ressentie" des lights au-dessus du plan 2D. Plus eleve
// = lights paraissent plus eloignees = NdotL plus uniforme. 80 = compromis
// pour un platformer typique (sprites de hauteur ~64-128 px).
const float LIGHT_Z = 80.0;

vec3 ComputeLighting2D(vec2 worldPos, uint shapeLayerMask, vec3 normal) {
    int n = int(uL2D.ambient.w);
    vec3 lit = uL2D.ambient.xyz;
    for (int i = 0; i < n && i < 16; i++) {
        // E.7b : test layer mask. Light n'affecte la shape que si AND non-zero.
        uint lightLayer = uint(uL2D.lights[i].dirCone.w);
        if ((lightLayer & shapeLayerMask) == 0u) continue;
        vec2 lp         = uL2D.lights[i].posIntRad.xy;
        float intensity = uL2D.lights[i].posIntRad.z;
        float radius    = max(uL2D.lights[i].posIntRad.w, 1.0);
        vec3  lcol      = uL2D.lights[i].colorCookie.xyz;
        // colorCookie.w encode (cookieIdx) + (-1000 si castShadow=false).
        float ckEnc     = uL2D.lights[i].colorCookie.w;
        bool  castSh    = ckEnc > -500.0;
        int   cookieIdx = castSh ? int(ckEnc) : int(ckEnc + 1000.0);
        float angleRad  = uL2D.lights[i].dirCone.x;
        float innerRad  = uL2D.lights[i].dirCone.y;
        float outerRad  = uL2D.lights[i].dirCone.z;

        vec2 toFrag = worldPos - lp;
        float dist  = length(toFrag);

        // Falloff radial smooth (1 au centre, 0 a radius)
        float t = clamp(1.0 - dist / radius, 0.0, 1.0);
        float att = t * t;
        if (att < 1e-4) continue;

        // Cone : si outerRad < 2*PI, on cull les fragments hors du cone
        if (outerRad < 6.28) {
            float fragAngle = atan(toFrag.y, toFrag.x);
            float delta     = fragAngle - angleRad;
            // wrap [-PI, PI]
            delta = mod(delta + 3.14159, 6.28318) - 3.14159;
            float adelta = abs(delta);
            // smooth coupure inner -> outer
            float coneFalloff = clamp((outerRad * 0.5 - adelta) /
                                       max(outerRad * 0.5 - innerRad * 0.5, 1e-3),
                                       0.0, 1.0);
            att *= coneFalloff;
            if (att < 1e-4) continue;
        }

        // Cookie : echantillonne une texture dans le repere local de la light.
        // UV = (toFrag rotated by -angleRad) / (2*radius) + 0.5
        if (cookieIdx >= 0) {
            float ca = cos(-angleRad), sa = sin(-angleRad);
            vec2 local = vec2(ca * toFrag.x - sa * toFrag.y,
                              sa * toFrag.x + ca * toFrag.y);
            vec2 uv = local / (2.0 * radius) + 0.5;
            att *= SampleCookie(cookieIdx, uv);
        }

        // E.5 : shadow visibility — raycaste seulement si la light castShadow
        if (castSh) att *= ShadowVisibility2D(worldPos, lp);

        // E.7c : si une normal map est fournie (normal != 0), modulate par
        // N.L (Lambertian). Le vector light.dir 3D est (toFrag.xy, -LIGHT_Z)
        // qui pointe DEPUIS le plan VERS la light (par convention OpenGL +Z
        // vers le viewer pour une normal map en tangent-space classique).
        if (dot(normal, normal) > 0.0) {
            vec3 L3 = normalize(vec3(-toFrag, LIGHT_Z));
            float ndl = max(dot(normalize(normal), L3), 0.0);
            att *= ndl;
        }

        lit += lcol * intensity * att;
    }
    return lit;
}

void main() {
    vec4 baseColor;
    if ((vFlags & 2u) != 0u) {
        vec4 t = texture(tAtlas, vUV);
        if ((vFlags & 1u) != 0u) {
            // SDF text
            float d  = t.a;
            float aa = fwidth(d) * 0.7;
            float a  = smoothstep(0.5 - aa, 0.5 + aa, d);
            if (a < 0.01) discard;
            baseColor = vec4(vColor.rgb, vColor.a * a);
        } else {
            baseColor = t * vColor;
            if (baseColor.a < 0.01) discard;
        }
    } else {
        baseColor = vColor;
        if (baseColor.a < 0.01) discard;
    }

    // Phase E : LIT bit (4) -> applique le lighting 2D sur la couleur de base.
    // E.7b : layer mask de la shape extrait des bits 8..15 du flags packe.
    // E.7c : NORMAL bit (8) -> sample la normal map au binding 12.
    if ((vFlags & 4u) != 0u) {
        uint shapeLayer = (vFlags >> 8) & 0xFFu;
        vec3 normal = vec3(0.0);   // 0 = pas de N.L modulation
        if ((vFlags & 8u) != 0u) {
            vec3 nTs = texture(tNormal, vUV).xyz * 2.0 - 1.0;
            normal = nTs;
        }
        vec3 lit = ComputeLighting2D(vWorldPos, shapeLayer, normal);
        baseColor.rgb *= lit;
    }
    fragColor = baseColor;
}
)GLSL";

    NkRender2D::~NkRender2D() { Shutdown(); }

    bool NkRender2D::Init(NkIDevice* device, NkTextureLibrary* texLib,
                          NkShaderLibrary* shaderLib, uint32 maxVerts) {
        mDevice = device;
        mTexLib = texLib;
        mVerts.Reserve(maxVerts);

        // VBO dynamique
        NkBufferDesc vbd;
        vbd.sizeBytes = maxVerts * sizeof(NkVertex2D);
        vbd.usage     = NkResourceUsage::NK_UPLOAD;
        vbd.type      = NkBufferType::NK_VERTEX;
        mVBO = mDevice->CreateBuffer(vbd);
        if (!mVBO.IsValid()) return false;

        // IBO (quads → triangles)
        uint32 maxIdx = maxVerts / 4 * 6;
        NkVector<uint32> idata;
        idata.Reserve(maxIdx);
        for(uint32 v=0; v < maxVerts; v+=4){
            idata.PushBack(v);   idata.PushBack(v+1); idata.PushBack(v+2);
            idata.PushBack(v);   idata.PushBack(v+2); idata.PushBack(v+3);
        }
        {
            NkBufferDesc ibd;
            ibd.sizeBytes   = maxIdx * sizeof(uint32);
            ibd.type        = NkBufferType::NK_INDEX;
            ibd.usage       = NkResourceUsage::NK_IMMUTABLE;
            ibd.initialData = idata.Data();
            mIBO = mDevice->CreateBuffer(ibd);
        }

        // Descriptor set : binding 0 = atlas texture, binding 1 = Lights2D UBO,
        // bindings 2..9 = cookies textures (8 slots max). Tous au stage ALL.
        NkDescriptorSetLayoutDesc texLayout;
        texLayout.Add(0, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                      ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                 .Add(1, NkDescriptorType::NK_UNIFORM_BUFFER,
                      ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        for (uint32 i = 0; i < kMaxCookies2D; i++) {
            texLayout.Add(2 + i, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                           ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        }
        // E.5 : binding 10 = Shadows2D UBO (occluders cercles)
        // E.7a : binding 11 = ShadowsAABB2D UBO (occluders AABB)
        // E.7c : binding 12 = normal map (sampler2D)
        texLayout.Add(10, NkDescriptorType::NK_UNIFORM_BUFFER,
                       ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                 .Add(11, NkDescriptorType::NK_UNIFORM_BUFFER,
                       ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS)
                 .Add(12, NkDescriptorType::NK_COMBINED_IMAGE_SAMPLER,
                       ::nkentseu::NkShaderStage::NK_ALL_GRAPHICS);
        mTexLayout    = mDevice->CreateDescriptorSetLayout(texLayout);
        mTexSet       = mDevice->AllocateDescriptorSet(mTexLayout);
        mLinearSampler= mDevice->CreateSampler(NkSamplerDesc::Linear());

        // Pre-bind les 8 slots cookies sur la texture White1x1 (no-op = cookie
        // toujours 1.0, donc light non affectee). User remplace via SetLightCookie.
        if (mTexLib) {
            NkTexHandle white = mTexLib->GetWhite1x1();
            NkTextureHandle whiteRhi = mTexLib->GetRHIHandle(white);
            for (uint32 i = 0; i < kMaxCookies2D; i++) {
                mDevice->BindTextureSampler(mTexSet, 2 + i, whiteRhi, mLinearSampler);
            }
        }

        // Phase E : UBO Lights2D — std140 layout
        // 16 lights * 3 vec4 (48 bytes) + 1 vec4 ambient = 768 + 16 = 784 bytes
        struct Lights2DBlock {
            float lights[16 * 12];   // 16 lights * 12 floats (3 vec4 each)
            float ambient[4];
        };
        static_assert(sizeof(Lights2DBlock) == 784, "Lights2DBlock std140");
        {
            NkBufferDesc ubd;
            ubd.sizeBytes = sizeof(Lights2DBlock);
            ubd.usage     = NkResourceUsage::NK_UPLOAD;
            ubd.type      = NkBufferType::NK_UNIFORM;
            mUBOLights2D = mDevice->CreateBuffer(ubd);
            // Init avec ambient gris faible et 0 lights pour eviter du black
            // si le user n'appelle jamais SetLights2D().
            Lights2DBlock zero{};
            zero.ambient[0] = 0.2f; zero.ambient[1] = 0.2f; zero.ambient[2] = 0.2f;
            zero.ambient[3] = 0.f;   // count = 0
            mDevice->WriteBuffer(mUBOLights2D, &zero, sizeof(zero));
            mDevice->BindUniformBuffer(mTexSet, 1, mUBOLights2D);
        }

        // E.5 + E.7d : Shadows2D UBO std140 — 32 vec4 occluders + meta + isoMat
        // = 32*16 + 16 + 16 = 544 bytes.
        struct Shadows2DBlock {
            float occluders[32 * 4];
            float meta[4];
            float isoMat[4];   // m00, m01, m10, m11 (default identity)
        };
        static_assert(sizeof(Shadows2DBlock) == 544, "Shadows2DBlock std140");
        {
            NkBufferDesc ubd;
            ubd.sizeBytes = sizeof(Shadows2DBlock);
            ubd.usage     = NkResourceUsage::NK_UPLOAD;
            ubd.type      = NkBufferType::NK_UNIFORM;
            mUBOShadows2D = mDevice->CreateBuffer(ubd);
            // Init = 0 occluders + identity iso transform -> shader skip la
            // loop d'ombre et n'applique aucune deformation iso.
            Shadows2DBlock init{};
            init.isoMat[0] = 1.f; init.isoMat[1] = 0.f;
            init.isoMat[2] = 0.f; init.isoMat[3] = 1.f;
            mDevice->WriteBuffer(mUBOShadows2D, &init, sizeof(init));
            mDevice->BindUniformBuffer(mTexSet, 10, mUBOShadows2D);
        }

        // E.7a : ShadowsAABB2D UBO — std140 : 32 vec4 boxes + 1 vec4 meta = 528 bytes
        struct ShadowsAABB2DBlock {
            float boxes[32 * 4];
            float meta[4];
        };
        static_assert(sizeof(ShadowsAABB2DBlock) == 528, "ShadowsAABB2DBlock std140");
        {
            NkBufferDesc ubd;
            ubd.sizeBytes = sizeof(ShadowsAABB2DBlock);
            ubd.usage     = NkResourceUsage::NK_UPLOAD;
            ubd.type      = NkBufferType::NK_UNIFORM;
            mUBOShadowsAABB2D = mDevice->CreateBuffer(ubd);
            ShadowsAABB2DBlock zero{};
            mDevice->WriteBuffer(mUBOShadowsAABB2D, &zero, sizeof(zero));
            mDevice->BindUniformBuffer(mTexSet, 11, mUBOShadowsAABB2D);
        }

        // E.7c : pre-bind binding 12 (normal map) sur White1x1 = pas d'effet
        // (vec3(1,1,1)*2-1 = (1,1,1) → ndl ≈ z component, mais user normal=0
        // par defaut avec NORMAL_MAP bit off donc pas de souci).
        if (mTexLib) {
            NkTexHandle white = mTexLib->GetWhite1x1();
            mDevice->BindTextureSampler(mTexSet, 12,
                mTexLib->GetRHIHandle(white), mLinearSampler);
        }

        // Compile et lie le shader 2D si une ShaderLibrary est fournie.
        ::nkentseu::NkShaderHandle shader2D{};
        if (shaderLib) {
            auto progHandle = shaderLib->LoadOrCompileVF("Render2D", kRender2D_VS, kRender2D_FS);
            if (progHandle.IsValid()) {
                shader2D = shaderLib->GetRHIHandle(progHandle);
            }
        }

        // Pipelines 2D : meme shader, blend state different
        {
            NkGraphicsPipelineDesc pd;
            pd.shader       = shader2D;
            pd.depthStencil = NkDepthStencilDesc::NoDepth();
            // 2D : pas de cull (l'ortho inverse Y, donc le winding peut paraitre
            // back-face vs CCW; et de toute facon les sprites/formes 2D sont visibles
            // des deux cotes par convention).
            pd.rasterizer   = NkRasterizerDesc::NoCull();
            // Range push_constant ALL_GRAPHICS : matche le PushConstants(ALL_GRAPHICS)
            // appele plus loin dans le rendu 2D (sinon validation error VK).
            pd.AddPushConstant(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0, sizeof(NkMat4f));
            if (mTexLayout.IsValid()) pd.descriptorSetLayouts.PushBack(mTexLayout);

            // Vertex layout — NkVertex2D : pos(vec2), uv(vec2), color(uint32), texIdx(uint32)
            //   binding=0, stride=24
            //   loc 0 : aPos    @ 0  (RG32_FLOAT)
            //   loc 1 : aUV     @ 8  (RG32_FLOAT)
            //   loc 2 : aColor  @ 16 (R32_UINT)
            //   loc 3 : aFlags  @ 20 (R32_UINT)
            pd.vertexLayout
              .AddBinding(0, sizeof(Vert2D), false)
              .AddAttribute(0, 0, ::nkentseu::NkVertexFormat::NK_RG32_FLOAT, 0,  "POSITION", 0)
              .AddAttribute(1, 0, ::nkentseu::NkVertexFormat::NK_RG32_FLOAT, 8,  "TEXCOORD", 0)
              .AddAttribute(2, 0, ::nkentseu::NkVertexFormat::NK_R32_UINT,   16, "COLOR",    0)
              .AddAttribute(3, 0, ::nkentseu::NkVertexFormat::NK_R32_UINT,   20, "TEXCOORD", 1);

            pd.blend     = NkBlendDesc::Alpha();
            pd.debugName = "2D_Alpha";
            mPipeAlpha   = mDevice->CreateGraphicsPipeline(pd);

            pd.blend     = NkBlendDesc::Additive();
            pd.debugName = "2D_Add";
            mPipeAdd     = mDevice->CreateGraphicsPipeline(pd);

            pd.blend     = NkBlendDesc::Opaque();
            pd.debugName = "2D_Opaque";
            mPipeOpaque  = mDevice->CreateGraphicsPipeline(pd);
        }
        return mVBO.IsValid();
    }

    void NkRender2D::Shutdown() {
        if(mTexSet.IsValid())    { mDevice->FreeDescriptorSet(mTexSet); }
        if(mTexLayout.IsValid()) { mDevice->DestroyDescriptorSetLayout(mTexLayout); }
        if(mLinearSampler.IsValid()){ mDevice->DestroySampler(mLinearSampler); }
        if(mPipeAlpha.IsValid()) { mDevice->DestroyPipeline(mPipeAlpha); }
        if(mPipeAdd.IsValid())   { mDevice->DestroyPipeline(mPipeAdd); }
        if(mPipeOpaque.IsValid()){ mDevice->DestroyPipeline(mPipeOpaque); }
        if(mVBO.IsValid()){mDevice->DestroyBuffer(mVBO);mVBO={};}
        if(mIBO.IsValid()){mDevice->DestroyBuffer(mIBO);mIBO={};}
        if(mUBOLights2D.IsValid()){mDevice->DestroyBuffer(mUBOLights2D);mUBOLights2D={};}
        if(mUBOShadows2D.IsValid()){mDevice->DestroyBuffer(mUBOShadows2D);mUBOShadows2D={};}
        if(mUBOShadowsAABB2D.IsValid()){mDevice->DestroyBuffer(mUBOShadowsAABB2D);mUBOShadowsAABB2D={};}
    }

    // ── Phase E : Lights 2D ────────────────────────────────────────────────────
    void NkRender2D::SetLights2D(const NkLight2DDesc* lights, uint32 count, NkVec3f ambient) {
        if (!mUBOLights2D.IsValid() || !mDevice) return;
        struct Lights2DBlock {
            float lights[16 * 12];
            float ambient[4];
        } b{};
        const uint32 cap = count > kMaxLights2D ? kMaxLights2D : count;
        const float kDeg2Rad = 3.14159265f / 180.f;
        // Pack uniquement les lights enabled : si user en a 4 mais light[1] est
        // disabled, on en envoie 3 au shader (count effectif) et on conserve
        // l'ordre des autres. Plus efficace que sous-skipper dans le shader.
        uint32 N = 0;
        for (uint32 i = 0; i < cap; i++) {
            const auto& L = lights[i];
            if (!L.enabled) continue;
            uint32 base = N * 12;
            // vec4 0 : posIntRad
            b.lights[base + 0] = L.position.x;
            b.lights[base + 1] = L.position.y;
            b.lights[base + 2] = L.intensity;
            b.lights[base + 3] = L.radius;
            // vec4 1 : colorCookie (w=cookieIdx, ou -2 si castShadow=false)
            b.lights[base + 4] = L.color.x;
            b.lights[base + 5] = L.color.y;
            b.lights[base + 6] = L.color.z;
            // Encode cookieIdx normalement, mais si castShadow=false on ajoute
            // un decalage de -100 pour que le shader saute le raycaste.
            float ckEnc = (float)L.cookieIdx;
            if (!L.castShadow) ckEnc += -1000.f;   // sentinel "no shadow"
            b.lights[base + 7] = ckEnc;
            // vec4 2 : dirCone + layerMask en .w (8 layers max)
            b.lights[base + 8]  = L.angleDeg  * kDeg2Rad;
            b.lights[base + 9]  = L.coneInner * kDeg2Rad;
            b.lights[base + 10] = L.coneOuter * kDeg2Rad;
            b.lights[base + 11] = (float)((uint32)L.layerMask & 0xFFu);
            N++;
        }
        b.ambient[0] = ambient.x;
        b.ambient[1] = ambient.y;
        b.ambient[2] = ambient.z;
        b.ambient[3] = (float)N;
        mDevice->WriteBuffer(mUBOLights2D, &b, sizeof(b));
    }

    void NkRender2D::SetLightCookie(uint32 slot, NkTexHandle tex) {
        if (slot >= kMaxCookies2D || !mTexSet.IsValid() || !mTexLib) return;
        NkTextureHandle rhi = tex.IsValid() ? mTexLib->GetRHIHandle(tex)
                                            : mTexLib->GetRHIHandle(mTexLib->GetWhite1x1());
        mDevice->BindTextureSampler(mTexSet, 2 + slot, rhi, mLinearSampler);
    }

    void NkRender2D::SetNormalMap(NkTexHandle tex) {
        if (!mTexSet.IsValid() || !mTexLib) return;
        NkTextureHandle rhi = tex.IsValid() ? mTexLib->GetRHIHandle(tex)
                                            : mTexLib->GetRHIHandle(mTexLib->GetWhite1x1());
        mDevice->BindTextureSampler(mTexSet, 12, rhi, mLinearSampler);
    }

    // ── Phase E.7a : AABB Shadow Casters ──────────────────────────────────────
    void NkRender2D::SetShadowCastersAABB2D(const NkShadowCasterAABB2D* aabbs, uint32 count) {
        if (!mUBOShadowsAABB2D.IsValid() || !mDevice) return;
        struct ShadowsAABB2DBlock {
            float boxes[32 * 4];
            float meta[4];
        } b{};
        const uint32 N = count > kMaxAABBs2D ? kMaxAABBs2D : count;
        for (uint32 i = 0; i < N; i++) {
            const auto& a = aabbs[i];
            uint32 base = i * 4;
            b.boxes[base + 0] = a.min.x;
            b.boxes[base + 1] = a.min.y;
            b.boxes[base + 2] = a.max.x;
            b.boxes[base + 3] = a.max.y;
        }
        b.meta[0] = (float)N;
        mDevice->WriteBuffer(mUBOShadowsAABB2D, &b, sizeof(b));
    }

    // ── Phase E.5 : Shadow Casters 2D (raycast GPU) ────────────────────────────
    // Ecrit uniquement les occluders + meta (offsets 0..527), preserve isoMat
    // qui vit a l'offset 528.
    void NkRender2D::SetShadowCasters2D(const NkShadowCaster2D* casters, uint32 count) {
        if (!mUBOShadows2D.IsValid() || !mDevice) return;
        struct OccludersAndMeta {
            float occluders[32 * 4];
            float meta[4];
        } b{};
        const uint32 N = count > kMaxOccluders2D ? kMaxOccluders2D : count;
        for (uint32 i = 0; i < N; i++) {
            const auto& c = casters[i];
            uint32 base = i * 4;
            b.occluders[base + 0] = c.position.x;
            b.occluders[base + 1] = c.position.y;
            b.occluders[base + 2] = c.radius;
            b.occluders[base + 3] = 0.f;
        }
        b.meta[0] = (float)N;
        // Ecrit 528 bytes a partir de offset 0 -> ne touche pas l'isoMat (528..544).
        mDevice->WriteBuffer(mUBOShadows2D, &b, sizeof(b));
    }

    // ── Phase E.7d : Iso transform ─────────────────────────────────────────────
    // Ecrit uniquement les 16 bytes isoMat a l'offset 528 dans Shadows2DUBO,
    // sans toucher aux occluders ni au meta -> peut etre appele independamment
    // de SetShadowCasters2D, dans n'importe quel ordre.
    void NkRender2D::SetIsoTransform(float32 m00, float32 m01, float32 m10, float32 m11) {
        mIso[0] = m00; mIso[1] = m01; mIso[2] = m10; mIso[3] = m11;
        if (!mUBOShadows2D.IsValid() || !mDevice) return;
        const uint64 isoOffset = 32 * 16 + 16;   // apres occluders[32] + meta
        float iso[4] = {m00, m01, m10, m11};
        mDevice->WriteBuffer(mUBOShadows2D, iso, sizeof(iso), isoOffset);
    }

    // ── Frame ──────────────────────────────────────────────────────────────────
    void NkRender2D::Begin(NkICommandBuffer* cmd, uint32 w, uint32 h,
                             float32 cX, float32 cY, float32 zoom, float32 rotDeg) {
        mCmd=cmd; mW=w; mH=h; mInFrame=true;
        // NB : on NE clear PAS mVerts/mBatches ici. Plusieurs systemes (Render2D
        // depuis Demo, puis Overlay) appellent Begin successivement dans la meme
        // frame ; ils doivent accumuler leurs draws ensemble. Le clear est fait
        // par Flush() apres le dessin.
        // Ortho ecran : (cX,cY) = coin haut-gauche, (cX+w/zoom,cY+h/zoom) = bas-droite.
        // Avec defauts (cX=0, cY=0, zoom=1) → coords pixel directes (0..w, 0..h)
        // qui mappent vers clip space (-1..1, 1..-1) (Y inverse, top-left origin).
        float32 l=cX,                r=cX+(float32)w/zoom;
        float32 t=cY,                b=cY+(float32)h/zoom;
        mOrtho=NkMat4f::Zero();
        mOrtho[0][0]=2.f/(r-l); mOrtho[1][1]=2.f/(t-b);
        mOrtho[2][2]=-1.f;
        mOrtho[3][0]=-(r+l)/(r-l); mOrtho[3][1]=-(t+b)/(t-b); mOrtho[3][3]=1.f;
        (void)rotDeg; // TODO: rotation ortho
    }

    void NkRender2D::End() {
        // Ne flushe PAS ici : les draw calls doivent etre soumis APRES
        // BeginRenderPass (sinon glClear ecrase tout). Le RenderGraph
        // appelle FlushPending(cmd) dans la passe Overlay2D, au bon moment.
        mInFrame = false;
        mCmd     = nullptr;
    }

    void NkRender2D::FlushPending(NkICommandBuffer* cmd) {
        if (!mVerts.Empty()) { mCmd=cmd; Flush(); mCmd=nullptr; }
    }

    void NkRender2D::Flush() {
        if (mVerts.Empty() || !mCmd) return;

        mDevice->WriteBuffer(mVBO, mVerts.Data(),
                             (uint64)mVerts.Size() * sizeof(Vert2D));

        // NB sur OpenGL : le VAO est attache au pipeline. BindGraphicsPipeline
        // change le VAO actif. Donc BindVertexBuffer / BindIndexBuffer doivent
        // etre appeles APRES BindGraphicsPipeline (sinon les binds sont attaches
        // a un autre VAO et perdus).
        for (auto& b : mBatches) {
            NkPipelineHandle pipe = mPipeAlpha;
            if (b.blend==NkBlendMode::NK_ADDITIVE)    pipe=mPipeAdd;
            else if (b.blend==NkBlendMode::NK_OPAQUE) pipe=mPipeOpaque;
            if (pipe.IsValid()) mCmd->BindGraphicsPipeline(pipe);

            // Apres BindPipeline : bind buffers et push constants au VAO actif
            mCmd->BindVertexBuffer(0, mVBO, 0);
            mCmd->BindIndexBuffer(mIBO, NkIndexFormat::NK_UINT32, 0);
            mCmd->PushConstants(::nkentseu::NkShaderStage::NK_ALL_GRAPHICS, 0,
                                sizeof(NkMat4f), &mOrtho);

            NkTexHandle src = b.tex.IsValid() ? b.tex : mTexLib->GetWhite1x1();
            NkTextureHandle rhi  = mTexLib->GetRHIHandle(src);
            NkSamplerHandle samp = mTexLib->GetRHISampler(src);
            if (mTexSet.IsValid()) {
                mDevice->BindTextureSampler(mTexSet, 0, rhi, samp);
                // Le pipeline 2D n'a qu'un seul descriptor set layout (mTexLayout)
                // a l'index 0 (cf. CreateGraphicsPipeline ci-dessus). Le shader
                // render2d.frag.vk.glsl utilise set=0 pour tous ses bindings (atlas,
                // lights, shadows, cookies, normal). Bind donc en firstSet=0 — sinon
                // validation Vulkan : firstSet+count > setLayoutCount.
                mCmd->BindDescriptorSet(mTexSet, 0);
            }

            uint32 triIdx   = b.vStart / 4 * 6;
            uint32 triCount = b.vCount / 4 * 6;
            mCmd->DrawIndexed(triCount, 1, triIdx, 0, 0);
        }
        mBatchCount += (uint32)mBatches.Size();
        mVertCount  += (uint32)mVerts.Size();
        mVerts.Clear(); mBatches.Clear();
    }

    void NkRender2D::PushQuad(NkVec2f tl,NkVec2f tr,NkVec2f br,NkVec2f bl,
                                NkVec2f uvTL,NkVec2f uvTR,NkVec2f uvBR,NkVec2f uvBL,
                                NkVec4f color, NkTexHandle tex) {
        uint32 c = PackColor(color);
        uint32 vStart = (uint32)mVerts.Size();
        // aFlags : bit 1 (=2) → echantillonner la texture dans le fragment shader.
        // Pour les formes pleines on passe White1x1 (ou tex invalide) → flags=0
        // → le shader prend la branche "couleur unie".
        // bit 2 (=4) → mode LIT (Phase E) : applique le lighting 2D via UBO.
        // bits 8..15 = E.7b layer mask (8 layers max, default 0xFF).
        NkTexHandle white = mTexLib ? mTexLib->GetWhite1x1() : NkTexHandle{};
        uint32 flags = (tex.IsValid() && tex.id != white.id) ? 2u : 0u;
        if (mLitMode) flags |= 4u;
        if (mNormalMode) flags |= 8u;
        flags |= (mLayerMask & 0xFFu) << 8;
        mVerts.PushBack({tl,uvTL,c,flags}); mVerts.PushBack({tr,uvTR,c,flags});
        mVerts.PushBack({br,uvBR,c,flags}); mVerts.PushBack({bl,uvBL,c,flags});

        // Fusionner batch si même texture/blend/layer
        if (!mBatches.Empty()) {
            auto& last = mBatches[mBatches.Size()-1];
            if (last.tex==tex && last.blend==mBlend && last.layer==mLayer) {
                last.vCount+=4; return;
            }
        }
        Batch b; b.tex=tex; b.blend=mBlend; b.layer=mLayer;
        b.vStart=vStart; b.vCount=4;
        mBatches.PushBack(b);
    }

    // ── Sprites ────────────────────────────────────────────────────────────────
    void NkRender2D::DrawSprite(NkRectF dst, NkTexHandle tex, NkVec4f tint, NkRectF uv) {
        float32 x0=dst.x,y0=dst.y,x1=dst.x+dst.w,y1=dst.y+dst.h;
        float32 u0=uv.x,v0=uv.y,u1=uv.x+uv.w,v1=uv.y+uv.h;
        PushQuad({x0,y0},{x1,y0},{x1,y1},{x0,y1}, {u0,v0},{u1,v0},{u1,v1},{u0,v1}, tint, tex);
    }

    void NkRender2D::DrawSpriteRotated(NkRectF dst, NkTexHandle tex, float32 angleDeg, NkVec2f pivot, NkVec4f tint, NkRectF uv) {
        float32 cx=dst.x+dst.w*pivot.x, cy=dst.y+dst.h*pivot.y;
        float32 rad=angleDeg*NK_PI/180.f;
        float32 co=cosf(rad), si=sinf(rad);
        auto rot=[&](float32 x,float32 y)->NkVec2f{
            float32 dx=x-cx,dy=y-cy;
            return {cx+dx*co-dy*si, cy+dx*si+dy*co};
        };
        float32 x0=dst.x,y0=dst.y,x1=dst.x+dst.w,y1=dst.y+dst.h;
        float32 u0=uv.x,v0=uv.y,u1=uv.x+uv.w,v1=uv.y+uv.h;
        PushQuad(rot(x0,y0),rot(x1,y0),rot(x1,y1),rot(x0,y1), {u0,v0},{u1,v0},{u1,v1},{u0,v1}, tint, tex);
    }

    void NkRender2D::DrawNineSlice(NkRectF dst, NkTexHandle tex,
                                     float32 l, float32 t, float32 r, float32 b,
                                     NkVec4f tint) {
        // 9 quads : corners + edges + center
        float32 xs[4]={dst.x, dst.x+l, dst.x+dst.w-r, dst.x+dst.w};
        float32 ys[4]={dst.y, dst.y+t, dst.y+dst.h-b, dst.y+dst.h};
        float32 us[4]={0.f, l/dst.w, 1.f-r/dst.w, 1.f};
        float32 vs[4]={0.f, t/dst.h, 1.f-b/dst.h, 1.f};
        for(int i=0;i<3;i++) for(int j=0;j<3;j++){
            NkRectF d={xs[i],ys[j],xs[i+1]-xs[i],ys[j+1]-ys[j]};
            NkRectF u={us[i],vs[j],us[i+1]-us[i],vs[j+1]-vs[j]};
            DrawSprite(d,tex,tint,u);
        }
    }

    // ── Formes ────────────────────────────────────────────────────────────────
    void NkRender2D::DrawImage(NkTexHandle tex, NkRectF dst, NkVec4f tint) {
        DrawSprite(dst,tex,tint);
    }

    void NkRender2D::DrawImage(NkTexHandle tex, NkRectF dst, NkRectF src, NkVec4f tint) {
        DrawSprite(dst,tex,tint,src);
    }

    void NkRender2D::FillRect(NkRectF r, NkVec4f color) {
        DrawSprite(r, mTexLib->GetWhite1x1(), color);
    }

    void NkRender2D::FillRectGradH(NkRectF r, NkVec4f left, NkVec4f right) {
        float32 x0=r.x,y0=r.y,x1=r.x+r.w,y1=r.y+r.h;
        uint32 tl=PackColor(left), tr=PackColor(right);
        uint32 vStart=(uint32)mVerts.Size();
        mVerts.PushBack({{x0,y0},{0,0},tl,0}); mVerts.PushBack({{x1,y0},{1,0},tr,0});
        mVerts.PushBack({{x1,y1},{1,1},tr,0}); mVerts.PushBack({{x0,y1},{0,1},tl,0});
        Batch b;b.tex=mTexLib->GetWhite1x1();b.blend=mBlend;b.layer=mLayer;
        b.vStart=vStart;b.vCount=4; mBatches.PushBack(b);
    }

    void NkRender2D::FillRectGradV(NkRectF r, NkVec4f top, NkVec4f bot) {
        float32 x0=r.x,y0=r.y,x1=r.x+r.w,y1=r.y+r.h;
        uint32 tc=PackColor(top), bc=PackColor(bot);
        uint32 vStart=(uint32)mVerts.Size();
        mVerts.PushBack({{x0,y0},{0,0},tc,0}); mVerts.PushBack({{x1,y0},{1,0},tc,0});
        mVerts.PushBack({{x1,y1},{1,1},bc,0}); mVerts.PushBack({{x0,y1},{0,1},bc,0});
        Batch b;b.tex=mTexLib->GetWhite1x1();b.blend=mBlend;b.layer=mLayer;
        b.vStart=vStart;b.vCount=4; mBatches.PushBack(b);
    }

    void NkRender2D::FillCircle(NkVec2f c, float32 radius, NkVec4f color, uint32 segs) {
        for(uint32 i=0;i<segs;i++){
            float32 a0=2*NK_PI*i/segs, a1=2*NK_PI*(i+1)/segs;
            NkVec2f p0={c.x+cosf(a0)*radius,c.y+sinf(a0)*radius};
            NkVec2f p1={c.x+cosf(a1)*radius,c.y+sinf(a1)*radius};
            FillTriangle(c,p0,p1,color);
        }
    }

    void NkRender2D::FillTriangle(NkVec2f a, NkVec2f b, NkVec2f c, NkVec4f color) {
        uint32 col=PackColor(color);
        NkTexHandle white=mTexLib->GetWhite1x1();
        uint32 vStart=(uint32)mVerts.Size();
        // Use degenerate quad trick (repeat last vert)
        mVerts.PushBack({a,{0,0},col,0}); mVerts.PushBack({b,{1,0},col,0});
        mVerts.PushBack({c,{1,1},col,0}); mVerts.PushBack({c,{1,1},col,0});
        Batch bt;bt.tex=white;bt.blend=mBlend;bt.layer=mLayer;
        bt.vStart=vStart;bt.vCount=4; mBatches.PushBack(bt);
    }

    void NkRender2D::FillRoundRect(NkRectF r, NkVec4f color, float32 radius) {
        // Centre + 4 rectangles + 4 coins
        FillRect({r.x+radius,r.y,r.w-2*radius,r.h},color);
        FillRect({r.x,r.y+radius,radius,r.h-2*radius},color);
        FillRect({r.x+r.w-radius,r.y+radius,radius,r.h-2*radius},color);
        uint32 segs=8;
        NkVec2f corners[4]={
            {r.x+radius,r.y+radius},{r.x+r.w-radius,r.y+radius},
            {r.x+r.w-radius,r.y+r.h-radius},{r.x+radius,r.y+r.h-radius}
        };
        float32 startAngs[4]={NK_PI,1.5f*NK_PI,0,0.5f*NK_PI};
        for(int ci=0;ci<4;ci++){
            for(uint32 s=0;s<segs;s++){
                float32 a0=startAngs[ci]+0.5f*NK_PI*s/segs;
                float32 a1=startAngs[ci]+0.5f*NK_PI*(s+1)/segs;
                NkVec2f p0={corners[ci].x+cosf(a0)*radius,corners[ci].y+sinf(a0)*radius};
                NkVec2f p1={corners[ci].x+cosf(a1)*radius,corners[ci].y+sinf(a1)*radius};
                FillTriangle(corners[ci],p0,p1,color);
            }
        }
    }

    void NkRender2D::DrawLine(NkVec2f a, NkVec2f b, NkVec4f color, float32 thick) {
        float32 dx=b.x-a.x, dy=b.y-a.y;
        float32 len=sqrtf(dx*dx+dy*dy);
        if(len<1e-4f)return;
        float32 nx=-dy/len*thick*0.5f, ny=dx/len*thick*0.5f;
        PushQuad({a.x-nx,a.y-ny},{b.x-nx,b.y-ny},{b.x+nx,b.y+ny},{a.x+nx,a.y+ny},
                  {0,0},{1,0},{1,1},{0,1}, color, mTexLib->GetWhite1x1());
    }

    void NkRender2D::DrawRect(NkRectF r, NkVec4f color, float32 thick) {
        DrawLine({r.x,r.y},{r.x+r.w,r.y},color,thick);
        DrawLine({r.x+r.w,r.y},{r.x+r.w,r.y+r.h},color,thick);
        DrawLine({r.x+r.w,r.y+r.h},{r.x,r.y+r.h},color,thick);
        DrawLine({r.x,r.y+r.h},{r.x,r.y},color,thick);
    }

    void NkRender2D::DrawCircle(NkVec2f c, float32 radius, NkVec4f color,
                                  float32 thick, uint32 segs) {
        for(uint32 i=0;i<segs;i++){
            float32 a0=2*NK_PI*i/segs, a1=2*NK_PI*(i+1)/segs;
            NkVec2f p0={c.x+cosf(a0)*radius,c.y+sinf(a0)*radius};
            NkVec2f p1={c.x+cosf(a1)*radius,c.y+sinf(a1)*radius};
            DrawLine(p0,p1,color,thick);
        }
    }

    void NkRender2D::DrawArc(NkVec2f c, float32 r, float32 a0, float32 a1,
                               NkVec4f color, float32 thick) {
        uint32 segs=32;
        float32 step=(a1-a0)/segs;
        for(uint32 i=0;i<segs;i++){
            float32 ang0=a0+step*i, ang1=a0+step*(i+1);
            NkVec2f p0={c.x+cosf(ang0*NK_PI/180.f)*r,c.y+sinf(ang0*NK_PI/180.f)*r};
            NkVec2f p1={c.x+cosf(ang1*NK_PI/180.f)*r,c.y+sinf(ang1*NK_PI/180.f)*r};
            DrawLine(p0,p1,color,thick);
        }
    }

    void NkRender2D::DrawPolyline(const NkVec2f* pts, uint32 n, NkVec4f color,
                                    float32 thick, bool closed) {
        for(uint32 i=0;i<n-1;i++) DrawLine(pts[i],pts[i+1],color,thick);
        if(closed && n>1) DrawLine(pts[n-1],pts[0],color,thick);
    }

    void NkRender2D::DrawBezier(NkVec2f p0,NkVec2f p1,NkVec2f p2,NkVec2f p3,
                                  NkVec4f color, float32 thick, uint32 segs) {
        NkVec2f prev=p0;
        for(uint32 i=1;i<=segs;i++){
            float32 t=(float32)i/segs, t2=t*t, t3=t2*t;
            float32 mt=1-t,mt2=mt*mt,mt3=mt2*mt;
            NkVec2f cur={
                mt3*p0.x+3*mt2*t*p1.x+3*mt*t2*p2.x+t3*p3.x,
                mt3*p0.y+3*mt2*t*p1.y+3*mt*t2*p2.y+t3*p3.y
            };
            DrawLine(prev,cur,color,thick);
            prev=cur;
        }
    }

    void NkRender2D::DrawRoundRect(NkRectF r, NkVec4f color, float32 radius, float32 thick) {
        DrawRect({r.x+radius,r.y,r.w-2*radius,r.h},color,thick);
        DrawArc({r.x+radius,r.y+radius},radius,180,270,color,thick);
        DrawArc({r.x+r.w-radius,r.y+radius},radius,270,360,color,thick);
        DrawArc({r.x+r.w-radius,r.y+r.h-radius},radius,0,90,color,thick);
        DrawArc({r.x+radius,r.y+r.h-radius},radius,90,180,color,thick);
    }

    // ── Clip / Blend ──────────────────────────────────────────────────────────
    void NkRender2D::PushClip(NkRectF rect) {
        Flush();
        if(mCmd) mCmd->SetScissor(NkRect2D((int32)rect.x,(int32)rect.y,(int32)rect.w,(int32)rect.h));
        mClipStack.PushBack(rect);
    }

    void NkRender2D::PopClip() {
        Flush();
        if (!mClipStack.Empty()) mClipStack.PopBack();
        if(mCmd){
            if(!mClipStack.Empty()){
                auto& r=mClipStack[mClipStack.Size()-1];
                mCmd->SetScissor(NkRect2D((int32)r.x,(int32)r.y,(int32)r.w,(int32)r.h));
            } else {
                mCmd->SetScissor(NkRect2D(0,0,(int32)mW,(int32)mH));
            }
        }
    }

    void NkRender2D::SetBlendMode(NkBlendMode mode) { mBlend=mode; }
    void NkRender2D::SetLayer(uint8 layer) { mLayer=layer; }

} // namespace renderer
} // namespace nkentseu
