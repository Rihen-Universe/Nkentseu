// =============================================================================
// ssao.gl.glsl — Screen Space Ambient Occlusion OpenGL 4.6
// Passe fullscreen : lit GBuffer depth+normal, génère texture occlusion.
// =============================================================================
// #vert — fullscreen triangle (pas de VBO nécessaire)
#version 460 core
out vec2 vUV;
void main() {
    // Triangle fullscreen : 3 sommets hardcodés
    vec2 pos[3] = vec2[](vec2(-1,-1),vec2(3,-1),vec2(-1,3));
    vec2 uvs[3] = vec2[](vec2(0,0),vec2(2,0),vec2(0,2));
    vUV = uvs[gl_VertexID];
    gl_Position = vec4(pos[gl_VertexID],0,1);
}

// #frag
#version 460 core
in vec2 vUV;

layout(binding=0) uniform sampler2D uGDepth;   // depth buffer
layout(binding=1) uniform sampler2D uGNormal;  // normal en view-space
layout(binding=2) uniform sampler2D uNoise;    // bruit 4×4

layout(std140, binding=0) uniform SSAO_UBO {
    mat4  uProj;
    mat4  uProjInv;
    vec4  uKernel[64];  // hemisphère de 64 samples
    vec2  uNoiseScale;  // (screenW/4, screenH/4)
    float uRadius;
    float uBias;
    float uIntensity;
    int   uKernelSize;
};

out float oOcclusion;

// Reconstitue position en view-space depuis depth + UV
vec3 ReconstructPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv*2.0-1.0, depth*2.0-1.0, 1.0);
    vec4 vp   = uProjInv * clip;
    return vp.xyz / vp.w;
}

void main() {
    float depth   = texture(uGDepth, vUV).r;
    if (depth >= 0.9999) { oOcclusion = 1.0; return; }

    vec3  fragPos = ReconstructPos(vUV, depth);
    vec3  normal  = normalize(texture(uGNormal, vUV).xyz * 2.0 - 1.0);
    vec3  randVec = normalize(texture(uNoise, vUV * uNoiseScale).xyz);

    // Construction TBN aléatoire (Gram–Schmidt)
    vec3 tangent   = normalize(randVec - normal * dot(randVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < uKernelSize; ++i) {
        vec3 samplePos = TBN * uKernel[i].xyz;
        samplePos = fragPos + samplePos * uRadius;

        // Projeter en clip-space
        vec4 offset = uProj * vec4(samplePos, 1.0);
        offset.xyz /= offset.w;
        vec2 sampleUV = offset.xy * 0.5 + 0.5;

        float sampleDepth  = texture(uGDepth, sampleUV).r;
        vec3  sampleFP     = ReconstructPos(sampleUV, sampleDepth);

        // Comparaison profondeur avec atténuation de distance
        float rangeCheck = smoothstep(0.0, 1.0, uRadius / abs(fragPos.z - sampleFP.z));
        occlusion += (sampleFP.z >= samplePos.z + uBias ? 1.0 : 0.0) * rangeCheck;
    }

    oOcclusion = 1.0 - (occlusion / float(uKernelSize)) * uIntensity;
}
