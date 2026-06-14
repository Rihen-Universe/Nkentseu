// ============================================================
// pp_tonemap.frag.vk.glsl -- NKRenderer v5.0 -- PostProcess Tonemap FS (Vulkan)
//
// Pipeline = HDR + Bloom (lit le mip 0 du Dual-Kawase upsample chain) + ACES
//          filmic + Gamma + Saturation + Vignette.
//
// Reference Dual-Kawase Bloom : Jorge Jimenez 2014 — "Next Generation Post
// Processing in Call of Duty: Advanced Warfare". State-of-the-art moderne :
// pyramide downsample/upsample multi-mip, halo radial vraiment doux, pas
// d'artefacts de pattern (vs naive single-pass gaussian).
//
// Le bloom est calcule en amont par NkRendererImpl::BuildDefaultRenderGraph
// qui ajoute 5 passes downsample + 5 passes upsample au RenderGraph, avec
// transients RGBA16F. La mip 0 finale (apres tous les upsamples additifs)
// est passee ici via uBloom (binding=1).
//
// API cote CPU (NkPostProcessStack::ExecuteRHI) :
//   - sampler2D uHDR   au set=0, binding=0 : HDR transient (sortie Geometry)
//   - sampler2D uBloom au set=0, binding=1 : mip 0 bloom (downsample+upsample)
//   - push constant 32 bytes (FRAGMENT) :
//       PC[0] = (exposure, gamma, vignetteIntens, saturation)
//       PC[1] = (bloomStrength, bloomThreshold, invWidth, invHeight)
//       Note : bloomThreshold ici est inutilise (deja applique dans le 1er
//              downsample). On garde le slot pour compat ExecuteRHI.
//
// @material("PP_Tonemap")
// ============================================================
#version 460 core

layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uHDR;
layout(set=0, binding=1) uniform sampler2D uBloom;
layout(set=0, binding=2) uniform sampler2D uSSAO;
// Phase L 2026-05-23 : Color grading LUT 3D. Default = identity (linear color
// passthrough). User upload une LUT cinema 16/32/64 cube via NkRenderer API.
layout(set=0, binding=3) uniform sampler3D uColorLUT;

layout(push_constant) uniform PC {
    vec4 p0;   // (exposure, gamma, vignetteIntens, saturation)
    vec4 p1;   // (bloomStrength, lutStrength, yFlipUV, lutSize)
               // yFlipUV : +1 Vulkan (no UV flip), -1 OpenGL (flip Y to
               // compenser convention NDC inverse). Pose par ExecuteRHI.
               // lutStrength : 0=no grading, 1=full LUT applied.
               // lutSize : taille du LUT (16, 32, 64) pour le bias texel correct.
    vec4 p2;   // Phase L Auto-exposure : (autoExpStrength, autoExpKey, _, _)
               // autoExpStrength : 0=manuel, 1=full auto-exp
               // autoExpKey : 0.18 = mid-gray Reinhard standard
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
    float autoExpStr     = pc.p2.x;
    float autoExpKey     = pc.p2.y;

    // ── Phase L Auto-exposure V0 ────────────────────────────────────
    // Sample le centre du bloom RT (assume Dual-Kawase upsample = bonne
    // approximation de la moyenne luma scene). Tous les fragments lisent
    // le meme texel -> uniform exposure -> flicker-free.
    // V1 future : compute reduction proper + eye adaptation temporelle SSBO.
    if (autoExpStr > 0.001) {
        vec3  centerBloom = texture(uBloom, vec2(0.5)).rgb;
        float centerLuma  = dot(centerBloom, vec3(0.2126, 0.7152, 0.0722));
        float autoExp     = autoExpKey / max(centerLuma, 0.001);
        // Clamp pour eviter explosions sur scenes tres sombres / tres claires.
        autoExp = clamp(autoExp, 0.1, 10.0);
        exposure = mix(exposure, autoExp, clamp(autoExpStr, 0.0, 1.0));
    }

    // ── HDR + SSAO + Bloom ──────────────────────────────────────────
    // 1. Sample HDR transient (sortie geometry pass)
    // 2. Sample SSAO (R8_UNORM, 1.0 = pas occlus, 0.0 = totalement occlus) et
    //    multiplie le HDR pour assombrir les zones occluses (objets sous le
    //    sol, coins, contacts, etc.).
    // 3. Sample bloom multi-pass et l'additionne.
    vec3  hdr   = texture(uHDR, vUV).rgb * exposure;
    float ao    = texture(uSSAO, vUV).r;
    hdr *= ao;
    vec3  bloom = texture(uBloom, vUV).rgb;
    hdr += bloom * bloomStr;

    // ── Tonemap ACES ────────────────────────────────────────────────
    vec3 mapped = ACESFilm(hdr);

    // Gamma : en Vulkan le swap chain est souvent en SRGB et fait le gamma
    // automatiquement -> pc.gamma est pose a 1.0 dans ExecuteRHI cote VK.
    // Pour les autres backends (OpenGL) on applique manuellement.
    if (gamma > 1.01) mapped = pow(mapped, vec3(1.0 / gamma));

    // ── Saturation (color grading) ─────────────────────────────────
    if (abs(saturation - 1.0) > 0.01) {
        float lum = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
        mapped = clamp(mix(vec3(lum), mapped, saturation), 0.0, 1.0);
    }

    // ── Phase L : Color Grading LUT 3D ─────────────────────────────
    // Sample le LUT cinema (sampler3D) en utilisant le mapped color comme
    // coord 3D. Bias texel correct : on remap [0,1] vers [(0.5/N)..(N-0.5)/N]
    // pour eviter le filtering aux bords (sinon le boundary du LUT bleed sur
    // l'image). Blend avec strength pour permettre des grading subtle.
    float lutStrength = pc.p1.y;
    float lutSize     = pc.p1.w;
    if (lutStrength > 0.001 && lutSize > 0.5) {
        vec3 lutCoord = clamp(mapped, 0.0, 1.0)
                      * ((lutSize - 1.0) / lutSize)
                      + (0.5 / lutSize);
        vec3 graded   = texture(uColorLUT, lutCoord).rgb;
        mapped = mix(mapped, graded, clamp(lutStrength, 0.0, 1.0));
    }

    // ── Vignette ───────────────────────────────────────────────────
    if (vignetteIntens > 0.001) {
        vec2 uv = vUV * 2.0 - 1.0;
        mapped *= clamp(1.0 - dot(uv, uv) * vignetteIntens, 0.0, 1.0);
    }

    oColor = vec4(mapped, 1.0);
}
