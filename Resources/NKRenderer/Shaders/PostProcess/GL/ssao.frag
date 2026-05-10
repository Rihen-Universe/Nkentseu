// ssao.frag — NKRenderer v4.0 — OpenGL GLSL 4.60
#version 460 core
layout(location=0) in  vec2 vUV;
layout(location=0) out float fragAO;

layout(binding=0) uniform sampler2D   tDepth;
layout(binding=1) uniform sampler2D   tNoise;   // 4x4 random rotation
layout(binding=0,std140) uniform SSAOUBO {
    mat4  proj, invProj;
    vec4  samples[64];
    vec2  noiseScale;    // viewport / 4
    float radius;
    float bias;
    int   numSamples;
    int   _p[3];
};

// Reconstruct view-space position from depth
vec3 ReconstructPos(vec2 uv, float depth) {
    vec4 ndcPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewPos = invProj * ndcPos;
    return viewPos.xyz / viewPos.w;
}

void main() {
    float depth   = texture(tDepth, vUV).r;
    if (depth >= 0.9999) { fragAO = 1.0; return; } // skybox

    vec3  fragPos = ReconstructPos(vUV, depth);
    // Approximate normal from depth derivatives
    vec3  ddx  = ReconstructPos(vUV + vec2(1.0/1920.0, 0), depth) - fragPos;
    vec3  ddy  = ReconstructPos(vUV + vec2(0, 1.0/1080.0), depth) - fragPos;
    vec3  N    = normalize(cross(ddx, ddy));

    vec3  rvec = texture(tNoise, vUV * noiseScale).xyz * 2.0 - 1.0;
    vec3  tangent = normalize(rvec - N * dot(rvec, N));
    vec3  bitang  = cross(N, tangent);
    mat3  TBN     = mat3(tangent, bitang, N);

    float occlusion = 0.0;
    for (int i = 0; i < numSamples; i++) {
        vec3 samplePos = TBN * samples[i].xyz;
        samplePos = fragPos + samplePos * radius;

        vec4 offset = proj * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sampleDepth = texture(tDepth, offset.xy).r;
        vec3  sampleVP    = ReconstructPos(offset.xy, sampleDepth);

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleVP.z));
        occlusion += (sampleVP.z >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
    }

    fragAO = 1.0 - (occlusion / float(numSamples));
}
