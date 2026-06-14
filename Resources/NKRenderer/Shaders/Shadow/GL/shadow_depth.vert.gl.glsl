#version 460 core
// shadow_depth.vert.gl.glsl — Shadow Depth-Only Vertex (GL)
layout(location=0) in vec3 aPos;
layout(std140,binding=0,std140) uniform ShadowPassUBO{
    mat4 model; mat4 lightViewProj;
    float ndcZScale; float ndcZOffset; vec2 pad;
} uSP;
void main(){
    vec4 wp=uSP.model*vec4(aPos,1.0);
    gl_Position=uSP.lightViewProj*wp;
    gl_Position.z=gl_Position.z*uSP.ndcZScale+gl_Position.w*uSP.ndcZOffset;
}
