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

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos;
    vec4  camDir;
    vec2  viewport;
    float time;
    float deltaTime;
    float iblStrength;
    float _p0, _p1, _p2;
    mat4  mirrorViewProj;
    vec4  reflectionFlags;
} uCam;

layout(std140, set=1, binding=1) uniform ObjectUBO {
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
    // NkVSM v1 : doit matcher EXACTEMENT le ObjectUBO du FS sinon OpenGL
    // strict-link refuse "struct type mismatch between shaders".
    vec4  shadowOverrides;
    // 2026-05-24 Triplanar : .x = tileSize en metres reels (0 = disabled),
    // .y = metersPerUnit, .z = enable flag (0/1), .w = reserve.
    vec4  triplanarParams;
} uObj;

// Phase NkVSM : ShadowUBO supprime du VS. Shadow coord est calcule dans le
// FS via la shadowMatrix du slot correspondant (ShadowSlotsUBO).

layout(location=0) out vec3 vWorldPos;
layout(location=1) out vec3 vNormal;
layout(location=2) out vec3 vTangent;
layout(location=3) out vec3 vBitangent;
layout(location=4) out vec2 vUV;
layout(location=5) out vec2 vUV2;
layout(location=6) out vec4 vColor;

void main() {
    // worldPos_mirror : pos dans l'espace shader (model deja pre-multiplie par
    // mirrorMat si mirror pass). Sert au gl_Position (la rasterization se fait
    // a la position miroir). worldPos_real : pos un-mirror pour
    // l'illumination, l'ombre, et le sample IBL (Y-flip si mirror pass).
    vec4 worldPos_mirror = uObj.model * vec4(aPos, 1.0);
    vec4 worldPos_real   = worldPos_mirror;

    mat3 nm        = mat3(uObj.normalMatrix);
    vec3 N_mirror  = normalize(nm * aNormal);
    vec3 N_real    = N_mirror;

    vec3 T_mirror;
    if (dot(aTangent, aTangent) > 1e-6) {
        T_mirror = normalize(nm * aTangent);
        T_mirror = normalize(T_mirror - dot(T_mirror, N_mirror) * N_mirror);
    } else {
        vec3 fallback = abs(N_mirror.y) < 0.99 ? vec3(0,1,0) : vec3(1,0,0);
        T_mirror = normalize(cross(fallback, N_mirror));
    }
    vec3 T_real = T_mirror;

    // Phase Planar Reflection fix 2026-05-24 : en passe miroir, on un-mirror Y
    // sur worldPos/N/T avant de les passer au FS. Le FS travaille alors en
    // espace "real geometry" (= comme si la cam etait en mirror cam voyant la
    // vraie scene), ce qui rend lighting/shadow/IBL coherents avec ce qui doit
    // etre reflete (sphere bottom self-shadowed, IBL sample direction correcte).
    if (uCam.reflectionFlags.x > 0.5) {
        worldPos_real.y = -worldPos_real.y;
        N_real.y        = -N_real.y;
        T_real.y        = -T_real.y;
    }
    // B doit etre recalcule depuis N_real et T_real (pas un Y-flip independant
    // de B_mirror) car le mirror inverse la handedness du systeme TBN. Sans ca,
    // cross(N_mirror,T_mirror) donne un B_mirror avec un signe potentiellement
    // inverse par rapport a B_real attendu. Bug observe sur cube multi-face.
    vec3 B_real = cross(N_real, T_real);

    vWorldPos  = worldPos_real.xyz;
    vNormal    = N_real;
    vTangent   = T_real;
    vBitangent = B_real;

    vUV    = aUV;
    vUV2   = aUV2;
    vColor = aColor * uObj.tint;

    // gl_Position utilise la pos miroir pour rasterizer au bon endroit ecran.
    gl_Position = uCam.viewProj * worldPos_mirror;
}
