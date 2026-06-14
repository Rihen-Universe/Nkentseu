// skybox.frag.vk.glsl — Phase N v0.5 : Background HDR skybox fragment
//
// Approche view-space (plus leger + moins sensible aux conventions NDC) :
//   1. Reconstruit le ray en view space depuis NDC + FOV extrait de proj
//   2. Transforme en world space via la rotation inverse de view (= transpose
//      car view est une matrice rigide / rotation pure modulo translation)
//
// Cross-API : vNDC.y au top du screen vaut -1 en Vulkan (viewport Y flip
// natif) et +1 en OpenGL (pas de flip). On multiplie par uCam.yFlipNDC
// (+1 VK, -1 GL) pour rendre la formule symetrique entre les deux backends.
//
// @material("Skybox")
#version 460 core

layout(location=0) in vec2 vNDC;
layout(location=0) out vec4 fragColor;

layout(std140, set=0, binding=0) uniform CameraUBO {
    mat4  view, proj, viewProj, invViewProj;
    vec4  camPos, camDir;
    vec2  viewport;
    float time, deltaTime;
    float iblStrength;
    float yFlipNDC;          // +1 Vulkan (viewport Y flip), -1 OpenGL (no flip)
    float _p1, _p2;
    mat4  mirrorViewProj;    // offset 320
    vec4  reflectionFlags;   // offset 384, .x = isMirrorPass
} uCam;

// Phase N v1 : sample le cubemap HDR brut (binding=26) au lieu du prefilter
// (binding=9). Le prefilter a subi Reinhard tonemap (necessaire pour l'IBL
// specular sans clamper en blanc) ce qui detruit le dynamic range pour le
// background. tSkyEnvCube est RGBA32F brut, preserve les valeurs > 1.0.
layout(set=0, binding=26) uniform samplerCube tSkyEnvCube;

void main() {
    // proj[1][1] = 1/tan(fovY/2). Identique entre VK et GL dans ce projet
    // (le flip Y de Vulkan est fait au niveau du viewport, pas de la proj).
    float tanHalfY = 1.0 / uCam.proj[1][1];
    float tanHalfX = 1.0 / uCam.proj[0][0];
    // View space ray : on applique yFlipNDC pour absorber la difference de
    // convention NDC entre Vulkan (Y down) et OpenGL (Y up).
    vec3 viewRay = normalize(vec3(vNDC.x * tanHalfX,
                                    vNDC.y * tanHalfY * uCam.yFlipNDC,
                                    -1.0));
    // World space : rotation inverse de view = transpose(mat3(view))
    // (view est rigide, donc rotation pure modulo translation).
    mat3 worldFromView = transpose(mat3(uCam.view));
    vec3 dir = worldFromView * viewRay;

    // Phase Planar Reflection fix 2026-05-24 : en mirror pass, l'UBO Camera
    // contient encore view_real (le VS applique mirror via xform pour les
    // objets). Pour que le skybox sample la bonne face du cubemap dans
    // rtPos/rtNeg, on doit utiliser dir_mirror = mirror * dir_real qui est
    // un simple Y-flip pour un plan horizontal Y=0.
    if (uCam.reflectionFlags.x > 0.5) {
        dir.y = -dir.y;
    }

    // Sample mip 0 du cubemap dedie skybox (HDR brut sans Reinhard).
    vec3 hdr = textureLod(tSkyEnvCube, dir, 0.0).rgb * uCam.iblStrength;
    fragColor = vec4(hdr, 1.0);
}
