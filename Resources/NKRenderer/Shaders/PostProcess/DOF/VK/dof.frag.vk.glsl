// dof.frag.vk.glsl — NKRenderer v4.0 — Depth of Field Bokeh (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tHDR;
layout(set=1,binding=1) uniform sampler2D tDepth;
layout(set=1,binding=2) uniform sampler2D tCoCMap;  // Circle of Confusion
layout(set=0,binding=3,std140) uniform DOFUBO{float focusDistance;float aperture;float focalLength;float maxBlur;float near,far,_p[2];}uDOF;
float LinearDepth(float d){return (2.*uDOF.near*uDOF.far)/(uDOF.far+uDOF.near-d*(uDOF.far-uDOF.near));}
void main(){
    float depth=LinearDepth(texture(tDepth,vUV).r);
    // CoC: negative=behind focus, positive=in front
    float coc=clamp((depth-uDOF.focusDistance)*uDOF.aperture/max(depth,0.001),-uDOF.maxBlur,uDOF.maxBlur);
    float blurRadius=abs(coc);
    if(blurRadius<0.5){fragColor=vec4(texture(tHDR,vUV).rgb,1.);return;}
    // Bokeh disk sampling (16 samples in a Poisson disk)
    vec3 accum=vec3(0.); float weight=0.;
    vec2 ts=1./vec2(textureSize(tHDR,0));
    // Precomputed Poisson disk offsets
    vec2 disk[16];
    disk[0]=vec2(0.,0.5);disk[1]=vec2(0.707,0.707);disk[2]=vec2(0.5,0.);disk[3]=vec2(0.707,-0.707);
    disk[4]=vec2(0.,-0.5);disk[5]=vec2(-0.707,-0.707);disk[6]=vec2(-0.5,0.);disk[7]=vec2(-0.707,0.707);
    disk[8]=vec2(0.,1.);disk[9]=vec2(1.,0.);disk[10]=vec2(0.,-1.);disk[11]=vec2(-1.,0.);
    disk[12]=vec2(0.5,0.866);disk[13]=vec2(0.866,0.5);disk[14]=vec2(0.866,-0.5);disk[15]=vec2(0.5,-0.866);
    for(int i=0;i<16;i++){
        vec2 offset=disk[i]*blurRadius*ts;
        float sDepth=LinearDepth(texture(tDepth,vUV+offset).r);
        float sCoc=clamp((sDepth-uDOF.focusDistance)*uDOF.aperture/max(sDepth,0.001),-uDOF.maxBlur,uDOF.maxBlur);
        // Include sample if its CoC covers the current offset
        float w=(abs(sCoc)>=length(disk[i])*blurRadius)?1.:0.;
        accum+=texture(tHDR,vUV+offset).rgb*w; weight+=w;
    }
    fragColor=vec4(accum/max(weight,1.),1.);
}
