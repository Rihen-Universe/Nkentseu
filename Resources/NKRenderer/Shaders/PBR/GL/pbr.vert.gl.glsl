// ============================================================
// pbr.vert.gl.glsl — NKRenderer v4.0 — PBR Vertex (OpenGL 4.6)
// Sync with embedded fallback in NkRender3D_PBRShaders.inl
// ============================================================
#version 460 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aTangent;
layout(location=3) in vec2 aUV;
layout(location=4) in vec2 aUV2;
layout(location=5) in vec4 aColor;

layout(std140, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos;
    vec4  camDir;
    vec2  viewport;
    float time;
    float deltaTime;
} uCam;

layout(std140, binding=1) uniform ObjectUBO {
    mat4  model;
    mat4  normalMatrix;
    vec4  tint;
    float metallic;
    float roughness;
    float aoStrength;
    float emissiveStrength;
    float normalStrength;
    float clearcoat;
    float clearcoatRoughness;
    float subsurface;
    vec4  subsurfaceColor;
} uObj;

layout(std140, binding=3) uniform ShadowUBO {
    mat4  cascadeMats[4];
    float cascadeSplits[4];
    vec4  cascadeTileBounds[4];
    int   cascadeCount;
    float shadowBias;
    float normalBias;
    int   softShadows;
    float softness;
    float _pad0, _pad1, _pad2;
} uShadow;

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;
layout(location=7) out vec4 vShadowCoord[4];

void main() {
    vec4 worldPos  = uObj.model * vec4(aPos, 1.0);
    vWorldPos      = worldPos.xyz;

    mat3 nm        = mat3(uObj.normalMatrix);
    vNormal        = normalize(nm * aNormal);

    // TBN robuste : si aTangent est degenere (length=0, cas typique des meshes
    // generes proceduralement comme icosphere), fallback sur un axe orthogonal
    // a vNormal pour eviter normalize((0,0,0)) -> NaN -> couleur noire.
    if (dot(aTangent, aTangent) > 1e-6) {
        vTangent = normalize(nm * aTangent);
        vTangent = normalize(vTangent - dot(vTangent, vNormal) * vNormal);
    } else {
        vec3 fallback = abs(vNormal.y) < 0.99 ? vec3(0,1,0) : vec3(1,0,0);
        vTangent = normalize(cross(fallback, vNormal));
    }
    vBitangent = cross(vNormal, vTangent);

    vUV    = aUV;
    vUV2   = aUV2;
    vColor = aColor * uObj.tint;

    for (int c = 0; c < 4; c++)
        vShadowCoord[c] = uShadow.cascadeMats[c] * worldPos;

    gl_Position = uCam.viewProj * worldPos;
}
