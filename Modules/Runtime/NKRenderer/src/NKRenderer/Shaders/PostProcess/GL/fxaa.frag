// fxaa.frag — NKRenderer v4.0 — OpenGL GLSL 4.60
// FXAA 3.11 simplifié
#version 460 core
layout(location=0) in  vec2 vUV;
layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D tLDR;
layout(binding=0,std140) uniform FXAAUBO { vec2 texelSize; float _p[2]; };

float Luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec2 ts = texelSize;
    vec3 rgbM  = texture(tLDR, vUV).rgb;
    vec3 rgbNW = texture(tLDR, vUV + ts * vec2(-1, 1)).rgb;
    vec3 rgbNE = texture(tLDR, vUV + ts * vec2( 1, 1)).rgb;
    vec3 rgbSW = texture(tLDR, vUV + ts * vec2(-1,-1)).rgb;
    vec3 rgbSE = texture(tLDR, vUV + ts * vec2( 1,-1)).rgb;

    float lumM  = Luma(rgbM);
    float lumNW = Luma(rgbNW);
    float lumNE = Luma(rgbNE);
    float lumSW = Luma(rgbSW);
    float lumSE = Luma(rgbSE);

    float lumMin = min(lumM, min(min(lumNW,lumNE),min(lumSW,lumSE)));
    float lumMax = max(lumM, max(max(lumNW,lumNE),max(lumSW,lumSE)));
    float lumRange = lumMax - lumMin;

    // Skip non-edge pixels
    if (lumRange < max(0.0312, lumMax * 0.125)) {
        fragColor = vec4(rgbM, 1.0); return;
    }

    // Edge direction
    float lumNS = lumNW + lumNE - lumSW - lumSE;
    float lumEW = lumNW + lumSW - lumNE - lumSE;
    bool horzSpan = abs(lumNS) >= abs(lumEW);
    vec2 dir = horzSpan ? vec2(0, ts.y) : vec2(ts.x, 0);

    // Sample along edge
    vec3 rgb1 = texture(tLDR, vUV - dir).rgb;
    vec3 rgb2 = texture(tLDR, vUV + dir).rgb;
    vec3 blended = (rgbM + rgb1 + rgb2) / 3.0;

    // Blend based on contrast
    float blend = lumRange / lumMax;
    fragColor = vec4(mix(rgbM, blended, blend * 0.75), 1.0);
}
