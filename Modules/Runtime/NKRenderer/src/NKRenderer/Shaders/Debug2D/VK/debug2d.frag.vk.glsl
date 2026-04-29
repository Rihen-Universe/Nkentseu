// debug2d.frag.vk.glsl — 2D Batch Fragment (Vulkan)
// Supports: solid color, textured, SDF text
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2  vUV;
layout(location=1) in vec4  vColor;
layout(location=2) in flat uint vFlags;
layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tAtlas;
void main(){
    if((vFlags&2u)!=0u){ // textured
        vec4 t=texture(tAtlas,vUV);
        if((vFlags&1u)!=0u){ // SDF text
            float d=t.a; float aa=fwidth(d)*0.7;
            float a=smoothstep(0.5-aa,0.5+aa,d);
            if(a<0.01)discard;
            fragColor=vec4(vColor.rgb,vColor.a*a);
        } else {
            fragColor=t*vColor;
            if(fragColor.a<0.01)discard;
        }
    } else {
        fragColor=vColor;
        if(fragColor.a<0.01)discard;
    }
}
