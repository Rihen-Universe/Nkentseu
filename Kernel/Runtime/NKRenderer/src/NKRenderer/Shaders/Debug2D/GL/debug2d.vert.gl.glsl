// debug2d.vert.gl.glsl — 2D Batch Vertex (OpenGL 4.6)
#version 460 core
layout(location=0) in vec2  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(location=3) in uint  aFlags;  // bit0=SDF, bit1=textured

layout(std140,binding=0) uniform OrthoUBO{mat4 ortho;float time;float _pad[3];};

layout(location=0) out vec2  vUV;
layout(location=1) out vec4  vColor;
layout(location=2) out flat uint vFlags;

void main(){
    gl_Position=ortho*vec4(aPos,0.,1.);
    vUV=aUV;
    vColor=vec4(float((aColor      )&0xFFu)/255.,
                float((aColor>> 8u)&0xFFu)/255.,
                float((aColor>>16u)&0xFFu)/255.,
                float((aColor>>24u)&0xFFu)/255.);
    vFlags=aFlags;
}
