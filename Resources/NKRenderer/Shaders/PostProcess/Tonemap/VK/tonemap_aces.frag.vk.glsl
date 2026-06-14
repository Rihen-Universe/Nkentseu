// tonemap_aces.frag.vk.glsl — ACES Filmic Tonemap + Bloom inline + Vignette
//
// API cote CPU (NkPostProcessStack::ExecuteRHI) :
//   - sampler2D tHDR au binding=0 (input HDR transient RGBA16F)
//   - push constant 32 bytes (FRAGMENT) :
//       PC[0] = (exposure, gamma, vignetteIntens, saturation)
//       PC[1] = (bloomStrength, bloomThreshold, invWidth, invHeight)
//
// Effets dans l'ordre :
//   1. Sample HDR + multiplie par exposure
//   2. Bloom inline (13-sample cross, 3 rayons) — extrait pixels > threshold,
//      les floute et les ajoute a hdr AVANT tonemap (compression naturelle)
//   3. ACES filmic (Krzysztof Narkowicz 2015)
//   4. Gamma correction (skip si gamma <= 1 = swap chain deja sRGB)
//   5. Saturation (color grading)
//   6. Vignette (assombrissement radial des bords)
//
// @material("Tonemap")
#version 460 core
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 fragColor;

layout(set=0, binding=0) uniform sampler2D tHDR;

// Push constant layout : 2 vec4 = 32 bytes. Doit matcher EXACTEMENT
// l'ordre des floats poussés par NkPostProcessStack::ExecuteRHI.
layout(push_constant) uniform PC {
    vec4 p0;   // (exposure, gamma, vignetteIntens, saturation)
    vec4 p1;   // (bloomStr, bloomThr, invW, invH)
} pc;

vec3 ACESFilm(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    float exposure       = pc.p0.x;
    float gamma          = pc.p0.y;
    float vignetteIntens = pc.p0.z;
    float saturation     = pc.p0.w;
    float bloomStr       = pc.p1.x;
    float bloomThr       = pc.p1.y;
    float invW           = pc.p1.z;
    float invH           = pc.p1.w;

    vec3 hdr = texture(tHDR, vUV).rgb * exposure;

    // ── Bloom inline ────────────────────────────────────────────────
    // 13-sample cross sur 3 rayons (2, 8, 20 pixels). Pour chaque sample,
    // on soustrait le seuil (highlights only) puis somme ponderee. Ajoute
    // AVANT tonemap pour que ACES compresse correctement la contribution.
    if (bloomStr > 0.001 && invW > 0.0 && invH > 0.0) {
        vec2 d = vec2(invW, invH);
        vec3 bloom = max(texture(tHDR, vUV).rgb * exposure - bloomThr, 0.0) * 4.0;
        const float radii[3]   = float[](2.0, 8.0, 20.0);
        const float weights[3] = float[](2.0, 1.0,  0.5);
        for (int r = 0; r < 3; r++) {
            vec2 off = radii[r] * d;
            bloom += max(texture(tHDR, vUV + vec2( off.x, 0.0)).rgb * exposure - bloomThr, 0.0) * weights[r];
            bloom += max(texture(tHDR, vUV + vec2(-off.x, 0.0)).rgb * exposure - bloomThr, 0.0) * weights[r];
            bloom += max(texture(tHDR, vUV + vec2( 0.0,  off.y)).rgb * exposure - bloomThr, 0.0) * weights[r];
            bloom += max(texture(tHDR, vUV + vec2( 0.0, -off.y)).rgb * exposure - bloomThr, 0.0) * weights[r];
        }
        bloom /= 18.0;  // total weight = 4 + 3*(4*weights[r]) = 4 + 8 + 4 + 2 = 18
        hdr += bloom * bloomStr;
    }

    // ── Tonemap ACES ────────────────────────────────────────────────
    vec3 mapped = ACESFilm(hdr);

    // Gamma : en Vulkan le swap chain est souvent en SRGB et fait le gamma
    // automatiquement -> pc.p0.y est pose a 1.0 dans ExecuteRHI cote VK.
    // Pour les autres backends (OpenGL) on applique manuellement.
    if (gamma > 1.01) mapped = pow(mapped, vec3(1.0 / gamma));

    // ── Saturation (color grading) ─────────────────────────────────
    if (abs(saturation - 1.0) > 0.01) {
        float lum = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
        mapped = clamp(mix(vec3(lum), mapped, saturation), 0.0, 1.0);
    }

    // ── Vignette ───────────────────────────────────────────────────
    if (vignetteIntens > 0.001) {
        vec2 uv = vUV * 2.0 - 1.0;
        mapped *= clamp(1.0 - dot(uv, uv) * vignetteIntens, 0.0, 1.0);
    }

    fragColor = vec4(mapped, 1.0);
}
