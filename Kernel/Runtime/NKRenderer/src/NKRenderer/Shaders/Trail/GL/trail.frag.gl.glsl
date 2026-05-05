// trail.frag.gl.glsl — NKRenderer v4.0 — Trail Ribbon Fragment (OpenGL 4.6)
#version 460 core
layout(location=0) in vec2 vUV; layout(location=1) in vec4 vColor;
layout(location=0) out vec4 fragColor;
layout(binding=0) uniform sampler2D tTrail;
void main(){
    vec4 t=texture(tTrail,vUV)*vColor;
    // Soft edges along U (ribbon width)
    float edge=smoothstep(0.,0.15,vUV.x)*smoothstep(0.,0.15,1.-vUV.x);
    t.a*=edge;
    if(t.a<0.01)discard;
    fragColor=t;
}
