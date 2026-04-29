// shadow_depth.vert — NKRenderer v4.0 — OpenGL GLSL 4.60
// Rendu depth-only pour shadow map CSM
#version 460 core
layout(location=0) in vec3 aPos;
layout(binding=0,std140) uniform ShadowUBO {
    mat4 lightViewProj[4];  // 4 cascades max
    int  cascadeIndex;
    int  _p[3];
};
layout(binding=1,std140) uniform ObjectUBO { mat4 model; mat4 _nm; vec4 _t; float _pp[8]; };

void main() {
    gl_Position = lightViewProj[cascadeIndex] * model * vec4(aPos, 1.0);
    // Slope-scale depth bias
    gl_Position.z += gl_Position.w * 0.005;
}
