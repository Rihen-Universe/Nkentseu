// ssao.frag.gl.glsl — NKRenderer v4.0 — SSAO (OpenGL 4.6)
#version 460 core
layout(location=0) in vec2 vUV; layout(location=0) out float fragAO;
layout(binding=0) uniform sampler2D tDepth;
layout(binding=1) uniform sampler2D tNoise;    // 4x4 random rotation
layout(std140,binding=2) uniform SSAOUBO{
    mat4 proj,invProj;
    vec4 samples[64];
    vec2 noiseScale;
    float radius,bias;
    int numSamples;
    int _pad[3];
}uSSAO;
vec3 ReconstructPos(vec2 uv,float depth){
    vec4 ndc=vec4(uv*2.-1.,depth*2.-1.,1.);
    vec4 vp=uSSAO.invProj*ndc;
    return vp.xyz/vp.w;
}
void main(){
    float depth=texture(tDepth,vUV).r;
    if(depth>=0.9999){fragAO=1.;return;}
    vec3 fragPos=ReconstructPos(vUV,depth);
    vec3 ddx=ReconstructPos(vUV+vec2(1./1920.,0.),depth)-fragPos;
    vec3 ddy=ReconstructPos(vUV+vec2(0.,1./1080.),depth)-fragPos;
    vec3 N=normalize(cross(ddx,ddy));
    vec3 rvec=texture(tNoise,vUV*uSSAO.noiseScale).xyz*2.-1.;
    vec3 tang=normalize(rvec-N*dot(rvec,N));
    mat3 TBN=mat3(tang,cross(N,tang),N);
    float occlusion=0.;
    for(int i=0;i<uSSAO.numSamples;i++){
        vec3 sp=TBN*uSSAO.samples[i].xyz;
        sp=fragPos+sp*uSSAO.radius;
        vec4 offset=uSSAO.proj*vec4(sp,1.);
        offset.xyz/=offset.w;
        offset.xyz=offset.xyz*.5+.5;
        float sampleDepth=texture(tDepth,offset.xy).r;
        vec3 sampleVP=ReconstructPos(offset.xy,sampleDepth);
        float rangeCheck=smoothstep(0.,1.,uSSAO.radius/abs(fragPos.z-sampleVP.z));
        occlusion+=(sampleVP.z>=sp.z+uSSAO.bias?1.:0.)*rangeCheck;
    }
    fragAO=1.-(occlusion/float(uSSAO.numSamples));
}
