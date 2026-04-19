// =============================================================================
// fxaa.gl.glsl — FXAA 3.11 OpenGL 4.6 (version simplifiée performante)
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
layout(std140,binding=0) uniform FXAA_UBO {
    vec2  uInvScreenSize;
    float uEdgeMin;         // 0.0833
    float uEdgeMax;         // 0.166
    float uEdgeSearch;      // 8.0
    float uEdgeGuess;       // 8.0
    float _p0,_p1;
};
out vec4 oColor;

float Luma(vec3 c) { return dot(c,vec3(0.299,0.587,0.114)); }

void main() {
    vec3 rgbNW = texture(uColor,vUV+vec2(-1,-1)*uInvScreenSize).rgb;
    vec3 rgbNE = texture(uColor,vUV+vec2( 1,-1)*uInvScreenSize).rgb;
    vec3 rgbSW = texture(uColor,vUV+vec2(-1, 1)*uInvScreenSize).rgb;
    vec3 rgbSE = texture(uColor,vUV+vec2( 1, 1)*uInvScreenSize).rgb;
    vec3 rgbM  = texture(uColor,vUV).rgb;

    float lumaNW=Luma(rgbNW),lumaNE=Luma(rgbNE),lumaSW=Luma(rgbSW),lumaSE=Luma(rgbSE),lumaM=Luma(rgbM);
    float lumaMin=min(lumaM,min(min(lumaNW,lumaNE),min(lumaSW,lumaSE)));
    float lumaMax=max(lumaM,max(max(lumaNW,lumaNE),max(lumaSW,lumaSE)));
    float lumaRange=lumaMax-lumaMin;

    if(lumaRange<max(uEdgeMin,lumaMax*uEdgeMax)) {
        oColor=vec4(rgbM,1.0); return;
    }

    vec2 dir;
    dir.x = -((lumaNW+lumaNE)-(lumaSW+lumaSE));
    dir.y =  ((lumaNW+lumaSW)-(lumaNE+lumaSE));
    float dirReduce=max((lumaNW+lumaNE+lumaSW+lumaSE)*0.25*uEdgeGuess,1.0/128.0);
    float rcpDirMin=1.0/(min(abs(dir.x),abs(dir.y))+dirReduce);
    dir=clamp(dir*rcpDirMin,vec2(-uEdgeSearch),vec2(uEdgeSearch))*uInvScreenSize;

    vec3 rgbA=0.5*(texture(uColor,vUV+dir*( 1.0/3.0-0.5)).rgb+texture(uColor,vUV+dir*(2.0/3.0-0.5)).rgb);
    vec3 rgbB=rgbA*0.5+0.25*(texture(uColor,vUV+dir*-0.5).rgb+texture(uColor,vUV+dir*0.5).rgb);

    float lumaB=Luma(rgbB);
    if(lumaB<lumaMin||lumaB>lumaMax) oColor=vec4(rgbA,1.0);
    else oColor=vec4(rgbB,1.0);
}
