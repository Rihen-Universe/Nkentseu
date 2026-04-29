// ============================================================
// pbr.frag.gl.glsl — NKRenderer v4.0 — PBR Fragment (OpenGL 4.6)
// Full PBR Metallic-Roughness + IBL + CSM shadows + Clearcoat + SSS
// ============================================================
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;
layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV;
layout(location=5) in vec2 vUV2;
layout(location=6) in vec4 vColor;
layout(location=7) in vec4 vShadowCoord[4];

layout(location=0) out vec4 fragColor;

// ── UBOs ─────────────────────────────────────────────────────
layout(std140, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos; vec4 camDir; vec2 viewport; float time; float dt;
} uCam;

layout(std140, binding=1) uniform ObjectUBO {
    mat4  model, normalMatrix; vec4 tint;
    float metallic, roughness, aoStrength, emissiveStrength;
    float normalStrength, clearcoat, clearcoatRoughness, subsurface;
    vec4  subsurfaceColor;
} uObj;

layout(std140, binding=2) uniform LightsUBO {
    vec4 positions[32], colors[32], directions[32], angles[32];
    int count; int _pad[3];
} uLights;

layout(std140, binding=3) uniform ShadowUBO {
    mat4  cascadeMats[4]; float cascadeSplits[4];
    int cascadeCount; float shadowBias; float normalBias; int softShadows;
} uShadow;

// ── Textures ─────────────────────────────────────────────────
layout(binding=4)  uniform sampler2D   tAlbedo;
layout(binding=5)  uniform sampler2D   tNormal;
layout(binding=6)  uniform sampler2D   tORM;        // R=AO G=Roughness B=Metallic
layout(binding=7)  uniform sampler2D   tEmissive;
layout(binding=8)  uniform samplerCube tEnvIrradiance;
layout(binding=9)  uniform samplerCube tEnvPrefilter;
layout(binding=10) uniform sampler2D   tBRDFLUT;
layout(binding=11) uniform sampler2DShadow tShadowMap;

// ── PBR Functions ─────────────────────────────────────────────
const float PI = 3.14159265358979;

float D_GGX(vec3 N, vec3 H, float r) {
    float a=r*r, a2=a*a, NdH=max(dot(N,H),0.0);
    float d=NdH*NdH*(a2-1.0)+1.0;
    return a2/(PI*d*d+1e-4);
}
float G_Schlick(float x, float k) { return x/(x*(1.0-k)+k); }
float G_Smith(vec3 N, vec3 V, vec3 L, float r) {
    float k=(r+1.0)*(r+1.0)/8.0;
    return G_Schlick(max(dot(N,V),0.0),k)*G_Schlick(max(dot(N,L),0.0),k);
}
vec3 F_Schlick(float cosT, vec3 F0) {
    return F0+(1.0-F0)*pow(max(1.0-cosT,0.0),5.0);
}
vec3 F_SchlickR(float cosT, vec3 F0, float r) {
    return F0+(max(vec3(1.0-r),F0)-F0)*pow(max(1.0-cosT,0.0),5.0);
}

// ── Shadow PCF ────────────────────────────────────────────────
float ShadowPCF(int cascade, vec4 coord) {
    if (cascade >= uShadow.cascadeCount) return 1.0;
    vec3 p = coord.xyz / coord.w;
    p.xy   = p.xy * 0.5 + 0.5;
    p.z    = p.z  * 0.5 + 0.5 - uShadow.shadowBias;
    if (any(lessThan(p, vec3(0.0))) || any(greaterThan(p, vec3(1.0)))) return 1.0;
    vec2 ts = 1.0 / vec2(textureSize(tShadowMap, 0));
    float s = 0.0;
    if (uShadow.softShadows != 0) {
        for (int x=-2;x<=2;x++) for (int y=-2;y<=2;y++)
            s += texture(tShadowMap, vec3(p.xy+vec2(x,y)*ts, p.z));
        return s / 25.0;
    } else {
        for (int x=-1;x<=1;x++) for (int y=-1;y<=1;y++)
            s += texture(tShadowMap, vec3(p.xy+vec2(x,y)*ts, p.z));
        return s / 9.0;
    }
}

// ── Cascade selection ─────────────────────────────────────────
float GetShadow() {
    float depth = (uCam.view * vec4(vWorldPos,1.0)).z;
    for (int c = 0; c < uShadow.cascadeCount; c++) {
        if (depth > -uShadow.cascadeSplits[c])
            return ShadowPCF(c, vShadowCoord[c]);
    }
    return 1.0;
}

void main() {
    // ── Albedo + alpha ─────────────────────────────────────────
    vec4 albSample = texture(tAlbedo, vUV) * vColor;
    vec3 albedo    = pow(albSample.rgb, vec3(2.2));
    float alpha    = albSample.a;
    if (alpha < 0.01) discard;

    // ── ORM + overrides ───────────────────────────────────────
    vec3 orm  = texture(tORM, vUV).rgb;
    float ao  = orm.r * uObj.aoStrength;
    float rog = orm.g * uObj.roughness;
    float met = orm.b * uObj.metallic;

    // ── Normal mapping ────────────────────────────────────────
    vec3 nTs  = texture(tNormal, vUV).xyz * 2.0 - 1.0;
    nTs.xy   *= uObj.normalStrength;
    mat3 TBN  = mat3(normalize(vTangent), normalize(vBitangent), normalize(vNormal));
    vec3 N    = normalize(TBN * nTs);
    vec3 V    = normalize(uCam.camPos.xyz - vWorldPos);
    vec3 F0   = mix(vec3(0.04), albedo, met);

    // ── Direct lighting ───────────────────────────────────────
    vec3 Lo = vec3(0.0);
    float shadow = GetShadow();

    for (int i = 0; i < uLights.count && i < 32; i++) {
        int   lt  = int(uLights.positions[i].w);
        vec3  L;
        float att = 1.0;

        if (lt == 0) {  // Directional
            L   = normalize(-uLights.directions[i].xyz);
        } else {        // Point / Spot
            vec3 d = uLights.positions[i].xyz - vWorldPos;
            float dist = length(d);
            L   = normalize(d);
            att = max(1.0 - dist / max(uLights.directions[i].w, 0.001), 0.0);
            att = att * att;
            if (lt == 2) {  // Spot
                float th    = dot(L, normalize(-uLights.directions[i].xyz));
                float inner = uLights.angles[i].x;
                float outer = uLights.angles[i].y;
                att *= clamp((th - outer) / (inner - outer + 1e-4), 0.0, 1.0);
            }
        }

        float csf = (uLights.angles[i].z > 0.5) ? shadow : 1.0;
        vec3 H    = normalize(V + L);
        float NdL = max(dot(N, L), 0.0);
        vec3 rad  = uLights.colors[i].rgb * uLights.colors[i].w * att;

        float NDF = D_GGX(N, H, rog);
        float G   = G_Smith(N, V, L, rog);
        vec3  F   = F_Schlick(max(dot(H,V),0.0), F0);
        vec3 spec = NDF*G*F / (4.0*max(dot(N,V),0.0)*NdL+1e-4);
        vec3 kD   = (1.0-F)*(1.0-met);
        Lo += csf * (kD*albedo/PI + spec) * rad * NdL;
    }

    // ── IBL ───────────────────────────────────────────────────
    vec3 Fi   = F_SchlickR(max(dot(N,V),0.0), F0, rog);
    vec3 kDi  = (1.0-Fi)*(1.0-met);
    vec3 irr  = texture(tEnvIrradiance, N).rgb;
    vec3 R    = reflect(-V, N);
    vec3 pref = textureLod(tEnvPrefilter, R, rog*4.0).rgb;
    vec2 brdf = texture(tBRDFLUT, vec2(max(dot(N,V),0.0), rog)).rg;
    vec3 amb  = (kDi*irr*albedo + pref*(Fi*brdf.x+brdf.y)) * ao;

    // ── Clearcoat ─────────────────────────────────────────────
    vec3 ccContrib = vec3(0.0);
    if (uObj.clearcoat > 0.0) {
        float ccR = uObj.clearcoatRoughness;
        vec3  Fcc = F_Schlick(max(dot(N,V),0.0), vec3(0.04)) * uObj.clearcoat;
        vec3  prefCC = textureLod(tEnvPrefilter, R, ccR*4.0).rgb;
        vec2  brdfCC = texture(tBRDFLUT, vec2(max(dot(N,V),0.0), ccR)).rg;
        ccContrib = prefCC * (Fcc * brdfCC.x + brdfCC.y);
    }

    // ── Subsurface Scattering (simple wrap) ───────────────────
    vec3 sssContrib = vec3(0.0);
    if (uObj.subsurface > 0.0) {
        for (int i = 0; i < uLights.count && i < 32; i++) {
            vec3 L = (int(uLights.positions[i].w)==0) ?
                normalize(-uLights.directions[i].xyz) :
                normalize(uLights.positions[i].xyz - vWorldPos);
            float wrap  = max(dot(N,L)+uObj.subsurface,0.0)/(1.0+uObj.subsurface);
            vec3  trans = uObj.subsurfaceColor.rgb * uLights.colors[i].rgb *
                          uLights.colors[i].w * wrap * uObj.subsurface;
            sssContrib += trans;
        }
    }

    // ── Emissive ──────────────────────────────────────────────
    vec3 emissive = texture(tEmissive, vUV).rgb * uObj.emissiveStrength;

    // ── Final ─────────────────────────────────────────────────
    vec3 color = amb + Lo + ccContrib + sssContrib + emissive;
    fragColor  = vec4(color, alpha);
}
