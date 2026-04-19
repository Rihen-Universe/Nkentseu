// =============================================================================
// bloom.gl.glsl — Bloom (Dual Kawase / Downsample + Upsample) OpenGL 4.6
// Deux passes : THRESHOLD+DOWNSAMPLE, puis UPSAMPLE+COMPOSITE
// =============================================================================
// #vert — triangle fullscreen
#version 460 core
out vec2 vUV;
void main() {
    vec2 pos[3] = vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vUV = pos[gl_VertexID]*0.5+0.5;
    gl_Position = vec4(pos[gl_VertexID],0,1);
}

// #frag
#version 460 core
in vec2 vUV;

layout(binding=0) uniform sampler2D uHDR;
layout(binding=1) uniform sampler2D uBloom; // résultat passe précédente (upsample)

layout(std140, binding=0) uniform BloomUBO {
    float uThreshold;
    float uKnee;
    float uIntensity;
    float uFilterRadius;
    int   uPass;    // 0=threshold, 1=downsample, 2=upsample, 3=composite
};

out vec4 oColor;

// Luminance perceptuelle
float Luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

// Seuillage doux (quadratic threshold / knee)
vec3 QuadThreshold(vec3 c, float thr, float knee) {
    float l = Luma(c);
    float rq = clamp(l - thr + knee, 0.0, 2.0*knee);
    rq = (rq*rq) / (4.0*knee + 0.00001);
    return c * max(rq, l - thr) / max(l, 0.00001);
}

// Filtrage 13 taps Kawase (downsample de qualité)
vec3 Downsample13(sampler2D tex, vec2 uv, vec2 texelSize) {
    vec3 c = vec3(0);
    // Centre
    c += texture(tex, uv).rgb * 4.0;
    // 4 diagonales proches
    c += texture(tex, uv + texelSize*vec2(-1,-1)).rgb;
    c += texture(tex, uv + texelSize*vec2( 1,-1)).rgb;
    c += texture(tex, uv + texelSize*vec2(-1, 1)).rgb;
    c += texture(tex, uv + texelSize*vec2( 1, 1)).rgb;
    // 4 horizontaux/verticaux
    c += texture(tex, uv + texelSize*vec2(-2,-2)).rgb * 0.5;
    c += texture(tex, uv + texelSize*vec2( 0,-2)).rgb;
    c += texture(tex, uv + texelSize*vec2( 2,-2)).rgb * 0.5;
    c += texture(tex, uv + texelSize*vec2(-2, 0)).rgb;
    c += texture(tex, uv + texelSize*vec2( 2, 0)).rgb;
    c += texture(tex, uv + texelSize*vec2(-2, 2)).rgb * 0.5;
    c += texture(tex, uv + texelSize*vec2( 0, 2)).rgb;
    c += texture(tex, uv + texelSize*vec2( 2, 2)).rgb * 0.5;
    return c / 16.0;
}

// Upsample tent filter 3×3
vec3 Upsample9(sampler2D tex, vec2 uv, float r) {
    vec2 ts = r / textureSize(tex, 0);
    vec3 c = vec3(0);
    c += texture(tex, uv + ts*vec2(-1, 1)).rgb * 1.0;
    c += texture(tex, uv + ts*vec2( 0, 1)).rgb * 2.0;
    c += texture(tex, uv + ts*vec2( 1, 1)).rgb * 1.0;
    c += texture(tex, uv + ts*vec2(-1, 0)).rgb * 2.0;
    c += texture(tex, uv               ).rgb   * 4.0;
    c += texture(tex, uv + ts*vec2( 1, 0)).rgb * 2.0;
    c += texture(tex, uv + ts*vec2(-1,-1)).rgb * 1.0;
    c += texture(tex, uv + ts*vec2( 0,-1)).rgb * 2.0;
    c += texture(tex, uv + ts*vec2( 1,-1)).rgb * 1.0;
    return c / 16.0;
}

void main() {
    vec2 ts = 1.0 / textureSize(uHDR, 0);

    if (uPass == 0) {
        // Passe 0 : seuil (threshold)
        vec3 color = texture(uHDR, vUV).rgb;
        oColor = vec4(QuadThreshold(color, uThreshold, uKnee), 1.0);
    }
    else if (uPass == 1) {
        // Passe 1 : downsample
        oColor = vec4(Downsample13(uHDR, vUV, ts), 1.0);
    }
    else if (uPass == 2) {
        // Passe 2 : upsample
        oColor = vec4(Upsample9(uHDR, vUV, uFilterRadius), 1.0);
    }
    else {
        // Passe 3 : composite HDR + bloom
        vec3 hdr   = texture(uHDR,   vUV).rgb;
        vec3 bloom = texture(uBloom, vUV).rgb;
        oColor = vec4(hdr + bloom * uIntensity, 1.0);
    }
}
