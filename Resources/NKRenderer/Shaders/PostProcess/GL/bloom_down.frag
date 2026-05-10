// bloom_downsample.frag — NKRenderer v4.0 — OpenGL GLSL 4.60
// Luminance threshold + downsample (13-tap Karis average)
#version 460 core
layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D tSrc;
layout(binding=0,std140) uniform BloomUBO {
    float threshold; float strength; vec2 texelSize;
};

// Karis average weight
float KarisAverage(vec3 c) {
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return 1.0 / (1.0 + lum);
}

void main() {
    vec2 ts = texelSize;
    // 13-tap downsample (COD 2014 method)
    vec3 a = texture(tSrc, vUV + ts * vec2(-2, 2)).rgb;
    vec3 b = texture(tSrc, vUV + ts * vec2( 0, 2)).rgb;
    vec3 c = texture(tSrc, vUV + ts * vec2( 2, 2)).rgb;
    vec3 d = texture(tSrc, vUV + ts * vec2(-2, 0)).rgb;
    vec3 e = texture(tSrc, vUV + ts * vec2( 0, 0)).rgb;
    vec3 f = texture(tSrc, vUV + ts * vec2( 2, 0)).rgb;
    vec3 g = texture(tSrc, vUV + ts * vec2(-2,-2)).rgb;
    vec3 h = texture(tSrc, vUV + ts * vec2( 0,-2)).rgb;
    vec3 k = texture(tSrc, vUV + ts * vec2( 2,-2)).rgb;
    vec3 l = texture(tSrc, vUV + ts * vec2(-1, 1)).rgb;
    vec3 m = texture(tSrc, vUV + ts * vec2( 1, 1)).rgb;
    vec3 n = texture(tSrc, vUV + ts * vec2(-1,-1)).rgb;
    vec3 o = texture(tSrc, vUV + ts * vec2( 1,-1)).rgb;

    // Threshold pour le premier downsample
    vec3 result = e * 0.125
                + (a+c+g+k) * 0.03125
                + (b+d+f+h) * 0.0625
                + (l+m+n+o) * 0.125;

    // Luminance threshold
    float lum = dot(result, vec3(0.2126,0.7152,0.0722));
    result *= smoothstep(threshold - 0.1, threshold + 0.1, lum);

    fragColor = vec4(result, 1.0);
}
