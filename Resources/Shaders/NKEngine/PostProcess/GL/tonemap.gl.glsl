// =============================================================================
// tonemap.gl.glsl — Tonemap ACES + Gamma + Film Grain + Vignette OpenGL 4.6
// =============================================================================
// #vert
#version 460 core
out vec2 vUV;
void main() {
    vec2 pos[3]=vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vUV=pos[gl_VertexID]*0.5+0.5;
    gl_Position=vec4(pos[gl_VertexID],0,1);
}
// #frag
#version 460 core
in vec2 vUV;
layout(binding=0) uniform sampler2D uHDR;
layout(binding=1) uniform sampler2D uLUT;   // 3D LUT (flattened 256×16)
layout(std140,binding=0) uniform TonemapUBO {
    float uExposure;
    float uGamma;
    int   uOperator;     // 0=ACES, 1=Reinhard, 2=Lottes, 3=Linear, 4=LUT
    float uContrast;
    float uSaturation;
    float uFilmGrain;
    float uVignette;
    float uVignetteSmooth;
    float uChromaticAberr;
    float uWhiteBalance;
    float uTime;
    float _p;
};
out vec4 oColor;

// ACES filmic tone mapping
vec3 ACES(vec3 x) {
    const float a=2.51, b=0.03, c=2.43, d=0.59, e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}
// Reinhard
vec3 Reinhard(vec3 x) { return x/(1.0+x); }
// Lottes
vec3 Lottes(vec3 x) {
    const vec3 a=vec3(1.6),d=vec3(0.977),hdrMax=vec3(8.0),midIn=vec3(0.18),midOut=vec3(0.267);
    vec3 b=((-pow(midIn,a)+pow(hdrMax,a)*midOut)/(pow(pow(hdrMax,a),d)*midOut));
    vec3 c=(pow(pow(hdrMax,a),d)*midOut-pow(midIn,a)*d*midOut)/(d*midOut*pow(midIn,a));
    return clamp(pow(x,a)/(pow(pow(x,a),d)*b+c),0.0,1.0);
}
// Hash pour le film grain
float Hash(vec2 p) {
    p=fract(p*vec2(443.897,441.423));
    p+=dot(p,p.yx+19.19);
    return fract((p.x+p.y)*p.x);
}

void main() {
    // Aberration chromatique
    vec2 caOff = (vUV - 0.5) * uChromaticAberr * 0.01;
    vec3 hdr;
    hdr.r = texture(uHDR, vUV + caOff).r;
    hdr.g = texture(uHDR, vUV).g;
    hdr.b = texture(uHDR, vUV - caOff).b;

    // Exposition
    hdr *= uExposure;

    // Tonemap
    vec3 color;
    if      (uOperator==0) color = ACES(hdr);
    else if (uOperator==1) color = Reinhard(hdr);
    else if (uOperator==2) color = Lottes(hdr);
    else                   color = clamp(hdr,0,1); // linear

    // Contraste
    color = pow(color, vec3(uContrast));

    // Saturation
    float luma = dot(color, vec3(0.2126,0.7152,0.0722));
    color = mix(vec3(luma), color, uSaturation);

    // Balance des blancs simple (temperature)
    color.r *= 1.0 + uWhiteBalance * 0.1;
    color.b *= 1.0 - uWhiteBalance * 0.1;

    // Gamma
    color = pow(max(color,0.0), vec3(1.0/uGamma));

    // Vignette
    vec2 vc = vUV - 0.5;
    float vign = 1.0 - smoothstep(uVignette, uVignette+uVignetteSmooth, length(vc)*1.6);
    color *= vign;

    // Film grain
    float grain = Hash(vUV + fract(uTime)) * 2.0 - 1.0;
    color += grain * uFilmGrain * 0.02;

    oColor = vec4(color, 1.0);
}
