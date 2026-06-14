// particle.geom — NKRenderer v4.0 — OpenGL GLSL 4.60
// Billboard expansion : transforme chaque point en quad face-caméra
#version 460 core
layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(location=0) in  vec2  vUV[];
layout(location=1) in  vec4  vColor[];
layout(location=2) in  float vRot[];
layout(location=3) in  float vSize[];

layout(location=0) out vec2 gUV;
layout(location=1) out vec4 gColor;

layout(binding=0,std140) uniform CameraUBO {
    mat4 view, proj, viewProj, invViewProj;
    vec4 camPos; vec2 viewport; float time; float dt;
};

void main() {
    vec3 center    = gl_in[0].gl_Position.xyz;
    float halfSize = vSize[0] * 0.5;
    float rot      = vRot[0];
    float co       = cos(rot), si = sin(rot);

    // Camera right and up (from view matrix)
    vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 up    = vec3(view[0][1], view[1][1], view[2][1]);

    // Rotated right/up
    vec3 rRight = (right * co - up * si) * halfSize;
    vec3 rUp    = (right * si + up * co) * halfSize;

    // 4 corners of the billboard quad
    vec4 verts[4];
    verts[0] = viewProj * vec4(center - rRight - rUp, 1.0);
    verts[1] = viewProj * vec4(center + rRight - rUp, 1.0);
    verts[2] = viewProj * vec4(center - rRight + rUp, 1.0);
    verts[3] = viewProj * vec4(center + rRight + rUp, 1.0);

    vec2 uvs[4] = vec2[4](vec2(0,1), vec2(1,1), vec2(0,0), vec2(1,0));

    for (int i = 0; i < 4; i++) {
        gl_Position = verts[i];
        gUV         = uvs[i];
        gColor      = vColor[0];
        EmitVertex();
    }
    EndPrimitive();
}
