// debug3d.frag.vk.glsl — Debug 3D Fragment (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec3 vNormal; layout(location=1) in vec2 vUV;
layout(location=2) in vec4 vColor;  layout(location=3) in float vDepth;
layout(location=0) out vec4 fragColor;
layout(set=0,binding=4,std140) uniform DebugUBO{int mode;float nearPlane,farPlane,_pad;}uDbg;
void main(){
    if(uDbg.mode==0){ // Normals
        fragColor=vec4(vNormal*0.5+0.5,1.);
    } else if(uDbg.mode==1){ // UV checker
        vec2 c=floor(vUV*8.); float ch=mod(c.x+c.y,2.);
        fragColor=vec4(mix(vec3(0.2),vec3(0.8),ch),1.);
    } else if(uDbg.mode==2){ // Depth
        float nd=(-vDepth-uDbg.nearPlane)/(uDbg.farPlane-uDbg.nearPlane);
        fragColor=vec4(vec3(nd),1.);
    } else if(uDbg.mode==4){ // Wireframe overlay (grey)
        fragColor=vec4(0.8,0.8,0.8,0.5);
    } else {
        fragColor=vColor;
    }
}
