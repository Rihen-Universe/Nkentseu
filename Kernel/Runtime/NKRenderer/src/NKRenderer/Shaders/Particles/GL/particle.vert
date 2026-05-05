// particle.vert — NKRenderer v4.0 — OpenGL GLSL 4.60
#version 460 core
layout(location=0) in vec3  aPos;
layout(location=1) in vec2  aUV;
layout(location=2) in uint  aColor;
layout(location=3) in float aSize;
layout(location=4) in float aRotation;

layout(location=0) out vec2  vUV;
layout(location=1) out vec4  vColor;
layout(location=2) out float vRot;
layout(location=3) out float vSize;

layout(binding=0,std140) uniform CameraUBO {
    mat4 view, proj, viewProj, invViewProj;
    vec4 camPos; vec2 viewport; float time; float dt;
};

void main() {
    // Pass-through — geometry shader or sprite expansion in frag
    gl_Position = viewProj * vec4(aPos, 1.0);
    vUV         = aUV;
    vColor      = vec4(
        float((aColor      ) & 0xFF)/255.0,
        float((aColor >>  8) & 0xFF)/255.0,
        float((aColor >> 16) & 0xFF)/255.0,
        float((aColor >> 24) & 0xFF)/255.0);
    vRot  = aRotation;
    vSize = aSize;
}

// ─────────────────────────────────────────────────────────────────────────────
// particle.geom — geometry shader billboard expansion
// #version 460 core
// layout(points) in;
// layout(triangle_strip, max_vertices=4) out;
// ...expands each point to a camera-facing quad...
// (Contenu dans un fichier séparé particle.geom)
