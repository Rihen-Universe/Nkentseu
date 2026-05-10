// terrain.vert.gl.glsl — NKRenderer v4.0 — Terrain Vertex (OpenGL 4.6)
// Height displacement from heightmap + normal from NxN sampler
#version 460 core
layout(location=0) in vec3 aPos; layout(location=1) in vec3 aNormal; layout(location=3) in vec2 aUV;
layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=1) uniform ObjectUBO{mat4 model,normalMatrix;vec4 tint;float metallic,roughness,aoStr,emissStr,normStr,clearcoat,ccRough,sss;vec4 sssColor;}uObj;
layout(std140,binding=11) uniform TerrainUBO{float heightScale;float uvScale;float _p[2];}uTerrain;
layout(binding=4) uniform sampler2D tHeightmap;
layout(location=0) out vec3 vWorldPos; layout(location=1) out vec3 vNormal;
layout(location=2) out vec2 vUV; layout(location=3) out vec2 vWorldXZ;
void main(){
    vec4 wp=uObj.model*vec4(aPos,1.);
    // Displace Y from heightmap
    float h=texture(tHeightmap,aUV).r*uTerrain.heightScale;
    wp.y+=h;
    vWorldPos=wp.xyz; vWorldXZ=aUV;
    // Compute normal from heightmap gradient
    float step=1./vec2(textureSize(tHeightmap,0)).x;
    float hR=texture(tHeightmap,aUV+vec2(step,0)).r*uTerrain.heightScale;
    float hU=texture(tHeightmap,aUV+vec2(0,step)).r*uTerrain.heightScale;
    vNormal=normalize(vec3(h-hR,1.,h-hU));
    vUV=aUV*uTerrain.uvScale;
    gl_Position=uCam.viewProj*wp;
}
