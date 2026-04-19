// =============================================================================
// particles.gl.glsl — Shader Particules billboard OpenGL 4.6
// Rendu billboards orientés caméra, atlas texture animable.
// =============================================================================
// #vert
#version 460 core
layout(location=0) in vec3  aPos;       // Centre de la particule (world)
layout(location=1) in vec2  aSize;      // Taille (w, h)
layout(location=2) in vec4  aColor;     // Couleur RGBA
layout(location=3) in float aRotation;  // Rotation en radians
layout(location=4) in vec4  aUVRect;    // uv.x, uv.y, uv.w, uv.h (atlas)

layout(std140, binding=0) uniform SceneUBO {
    mat4 uModel,uView,uProj,uLightVP;
    vec4 uLightDir,uEyePos; float uTime,uDt,uNdcZS,uNdcZO;
};

// Coin du billboard dans l'espace caméra
const vec2 kCorners[4] = vec2[](vec2(-0.5,-0.5),vec2(0.5,-0.5),vec2(0.5,0.5),vec2(-0.5,0.5));
// Ordre : deux triangles (0,1,2 / 0,2,3)
const int kIdx[6] = int[](0,1,2,0,2,3);

out vec2 vUV;
out vec4 vColor;

void main() {
    int vid    = kIdx[gl_VertexID % 6];
    vec2 corner= kCorners[vid];

    // Rotation dans le plan billboard
    float co = cos(aRotation), si = sin(aRotation);
    vec2 rotated = vec2(corner.x*co - corner.y*si, corner.x*si + corner.y*co);

    // Vue en view-space (billboard axe caméra)
    vec3 viewPos = (uView * vec4(aPos,1.0)).xyz;
    viewPos.x += rotated.x * aSize.x;
    viewPos.y += rotated.y * aSize.y;

    // UV dans l'atlas
    vUV = aUVRect.xy + (kCorners[vid]+0.5) * aUVRect.zw;
    vColor = aColor;

    gl_Position = uProj * vec4(viewPos, 1.0);
}

// #frag
#version 460 core
in vec2 vUV;
in vec4 vColor;
layout(binding=0) uniform sampler2D uAtlas;
layout(std140,binding=2) uniform ParticleMat {
    float uAlphaClip;
    float uSoftParticle;
    float _p0,_p1;
};
layout(binding=1) uniform sampler2D uDepth; // Pour soft particles
out vec4 oColor;
void main() {
    vec4 c = texture(uAtlas, vUV) * vColor;
    if (c.a < uAlphaClip) discard;
    oColor = c;
}
