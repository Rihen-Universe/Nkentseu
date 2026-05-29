// ============================================================
// pp_bloomdown.frag.vk.glsl — NKRenderer v5.0 — Bloom Downsample FS (Vulkan)
//
// 13-tap downsample filter (Call of Duty: Advanced Warfare bloom).
// Reference : Jorge Jimenez 2014 — "Next Generation Post Processing in COD:AW".
//
// Pattern : 4 inner samples (0.5 px) + 9 outer samples (1.0 px) avec poids
// 0.5 (centre) / 0.125 (outer edges et coins) — pour un total normalise
// energetiquement vers la sortie a 0.5 size.
//
// Le premier downsample (mip 0 → mip 1) applique aussi un soft threshold
// pour extraire les pixels brillants (bloomThr). Les downsamples
// successifs (mip i → mip i+1, i>0) passent threshold=0 = pas de filtre.
//
// API :
//   - sampler2D uSrc au set=0,binding=0 (texture mip source)
//   - push constant 16 bytes :
//       (srcInvW, srcInvH, threshold, _pad)
//
// @material("PP_BloomDown")
// ============================================================
#version 460 core

layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 oColor;

layout(set=0, binding=0) uniform sampler2D uSrc;

layout(push_constant) uniform PC {
    vec2  srcInvSize;   // 1.0 / (srcWidth, srcHeight)
    float threshold;    // soft threshold (>0 = bright pass, 0 = passthrough)
    float yFlipUV;      // partage avec VS, ignore ici
} pc;

// Soft knee threshold (Jimenez 2014). Smooth fade-in pour eviter le hard
// cutoff qui produit du aliasing temporel sur les sources mobiles.
vec3 SoftThreshold(vec3 c, float thr) {
    if (thr <= 0.001) return c;
    float br   = max(c.r, max(c.g, c.b));
    float soft = max(br - thr, 0.0);
    soft       = soft * soft / (4.0 * thr + 1e-4);
    float contrib = max(soft, br - thr) / max(br, 1e-4);
    return c * contrib;
}

void main() {
    vec2 t = pc.srcInvSize;

    // 13-tap pattern :
    //   Outer 3x3 (a,b,c / f,g,h / k,l,m) : echantillons 1 px
    //   Inner 2x2 (d,e / i,j)              : echantillons 0.5 px
    vec3 a = texture(uSrc, vUV + t * vec2(-1.0,  1.0)).rgb;
    vec3 b = texture(uSrc, vUV + t * vec2( 0.0,  1.0)).rgb;
    vec3 c = texture(uSrc, vUV + t * vec2( 1.0,  1.0)).rgb;
    vec3 d = texture(uSrc, vUV + t * vec2(-0.5,  0.5)).rgb;
    vec3 e = texture(uSrc, vUV + t * vec2( 0.5,  0.5)).rgb;
    vec3 f = texture(uSrc, vUV + t * vec2(-1.0,  0.0)).rgb;
    vec3 g = texture(uSrc, vUV).rgb;
    vec3 h = texture(uSrc, vUV + t * vec2( 1.0,  0.0)).rgb;
    vec3 i = texture(uSrc, vUV + t * vec2(-0.5, -0.5)).rgb;
    vec3 j = texture(uSrc, vUV + t * vec2( 0.5, -0.5)).rgb;
    vec3 k = texture(uSrc, vUV + t * vec2(-1.0, -1.0)).rgb;
    vec3 l = texture(uSrc, vUV + t * vec2( 0.0, -1.0)).rgb;
    vec3 m = texture(uSrc, vUV + t * vec2( 1.0, -1.0)).rgb;

    // Reconstruction filtree (poids du Jimenez 2014 : centre 0.5, coins 0.125)
    vec3 col = (d + e + i + j) * 0.5            // inner box  (poids 0.5)
             + (a + b + g + f) * 0.125          // top-left   (poids 0.125)
             + (b + c + h + g) * 0.125          // top-right
             + (f + g + l + k) * 0.125          // bottom-left
             + (g + h + m + l) * 0.125;         // bottom-right
    col *= 1.0 / 4.0;                            // normalisation des 4 sub-blocks

    // Premier downsample : soft threshold pour extraire les highlights
    col = SoftThreshold(col, pc.threshold);

    oColor = vec4(col, 1.0);
}
