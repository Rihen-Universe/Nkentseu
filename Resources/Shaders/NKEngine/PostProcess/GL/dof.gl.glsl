// =============================================================================
// dof.gl.glsl — Depth of Field Bokeh circulaire OpenGL 4.6
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
layout(binding=0) uniform sampler2D uColor;
layout(binding=1) uniform sampler2D uDepth;
layout(std140,binding=0) uniform DOF_UBO {
    float uFocusDistance;
    float uFocusRange;
    float uBokehRadius;
    float uNear;
    float uFar;
    int   uSamples;
    float _p0,_p1;
};
out vec4 oColor;

float LinearDepth(float d){
    float z=d*2.0-1.0;
    return 2.0*uNear*uFar/(uFar+uNear-z*(uFar-uNear));
}
// CoC : Circle of Confusion
float CoC(float depth){
    return clamp((depth-uFocusDistance)/uFocusRange,-1.0,1.0)*uBokehRadius;
}

const float PI=3.14159265;

void main(){
    float depth=LinearDepth(texture(uDepth,vUV).r);
    float coc=abs(CoC(depth));
    vec2 ts=1.0/textureSize(uColor,0);

    vec4 sum=vec4(0);
    float weight=0;
    // Spiral sampling (Vogel disk)
    for(int i=0;i<uSamples;++i){
        float r=sqrt(float(i+0.5)/float(uSamples));
        float theta=float(i)*2.399963*2.0*PI;
        vec2 offset=vec2(cos(theta),sin(theta))*r*coc*ts;
        float sampleDepth=LinearDepth(texture(uDepth,vUV+offset).r);
        float sampleCoc=abs(CoC(sampleDepth));
        // Ne pas saigner les objets proches sur les lointains
        float w=sampleDepth>=depth-0.1?1.0:sampleCoc;
        sum+=texture(uColor,vUV+offset)*w;
        weight+=w;
    }
    oColor=sum/max(weight,0.0001);
}
