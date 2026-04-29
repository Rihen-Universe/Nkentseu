// particles.geom.gl.glsl — Particle Billboard Geometry (OpenGL 4.6)
#version 460 core
layout(points) in;
layout(triangle_strip, max_vertices=4) out;

layout(location=0) in  vec2 vUV[];
layout(location=1) in  vec4 vColor[];

layout(location=0) out vec2 gUV;
layout(location=1) out vec4 gColor;

layout(std140,binding=0) uniform CameraUBO{mat4 view,proj,viewProj,invViewProj;vec4 camPos,camDir;vec2 viewport;float time,dt;}uCam;
layout(std140,binding=6) uniform ParticleUBO{float halfSize;float rotation;float _p[2];}uPart;

void main(){
    vec3 center=gl_in[0].gl_Position.xyz;
    float hs=uPart.halfSize, rot=uPart.rotation;
    float co=cos(rot), si=sin(rot);
    // Camera right/up from view matrix columns
    vec3 right=vec3(uCam.view[0][0],uCam.view[1][0],uCam.view[2][0]);
    vec3 up   =vec3(uCam.view[0][1],uCam.view[1][1],uCam.view[2][1]);
    vec3 rr=(right*co-up*si)*hs;
    vec3 ru=(right*si+up*co)*hs;
    vec2 uvs[4]=vec2[4](vec2(0.,1.),vec2(1.,1.),vec2(0.,0.),vec2(1.,0.));
    vec3 corners[4]=vec3[4](center-rr-ru,center+rr-ru,center-rr+ru,center+rr+ru);
    for(int i=0;i<4;i++){
        gl_Position=uCam.viewProj*vec4(corners[i],1.);
        gUV=uvs[i]; gColor=vColor[0]; EmitVertex();
    }
    EndPrimitive();
}
