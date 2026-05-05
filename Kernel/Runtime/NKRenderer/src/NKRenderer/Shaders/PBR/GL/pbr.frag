// pbr.frag — NKRenderer v4.0 — OpenGL GLSL 4.60 core
#version 460 core

layout(location=0) in vec3 vWorldPos;
layout(location=1) in vec3 vNormal;
layout(location=2) in vec3 vTangent;
layout(location=3) in vec3 vBitangent;
layout(location=4) in vec2 vUV;
layout(location=5) in vec2 vUV2;
layout(location=6) in vec4 vColor;

layout(location=0) out vec4 fragColor;

layout(binding=0,std140) uniform CameraUBO { mat4 view; mat4 proj; mat4 viewProj; mat4 invViewProj; vec4 camPos; vec2 viewportSize; float time; float deltaTime; };
layout(binding=1,std140) uniform ObjectUBO { mat4 model; mat4 normalMatrix; vec4 tint; float metallic; float roughness; float aoStrength; float emissiveStrength; };
layout(binding=2,std140) uniform LightsUBO { vec4 lightPositions[32]; vec4 lightColors[32]; vec4 lightDirections[32]; vec4 lightAngles[32]; int lightCount; int _pad[3]; };

layout(binding=3) uniform sampler2D  tAlbedo;
layout(binding=4) uniform sampler2D  tNormal;
layout(binding=5) uniform sampler2D  tORM;
layout(binding=6) uniform sampler2D  tEmissive;
layout(binding=7) uniform samplerCube tEnvIrradiance;
layout(binding=8) uniform samplerCube tEnvPrefilter;
layout(binding=9) uniform sampler2D  tBRDFLUT;
layout(binding=10)uniform sampler2D  tShadowAtlas;

const float PI = 3.14159265358979;

float DistributionGGX(vec3 N,vec3 H,float r){float a=r*r,a2=a*a,NdH=max(dot(N,H),0.0);float d=NdH*NdH*(a2-1.0)+1.0;return a2/(PI*d*d);}
float GeometrySchlick(float NdV,float k){return NdV/(NdV*(1.0-k)+k);}
float GeometrySmith(vec3 N,vec3 V,vec3 L,float r){float k=(r+1.0)*(r+1.0)/8.0;return GeometrySchlick(max(dot(N,V),0.0),k)*GeometrySchlick(max(dot(N,L),0.0),k);}
vec3  FresnelSchlick(float c,vec3 F0){return F0+(1.0-F0)*pow(max(1.0-c,0.0),5.0);}
vec3  FresnelRough(float c,vec3 F0,float r){return F0+(max(vec3(1.0-r),F0)-F0)*pow(max(1.0-c,0.0),5.0);}

void main() {
    vec4 alb4  = texture(tAlbedo,vUV)*vColor;
    vec3 albedo= pow(alb4.rgb,vec3(2.2));
    float alpha= alb4.a;
    if(alpha<0.01)discard;

    vec3 orm   = texture(tORM,vUV).rgb;
    float ao   = orm.r*aoStrength;
    float roug = orm.g*roughness;
    float metal= orm.b*metallic;

    vec3 nts   = texture(tNormal,vUV).xyz*2.0-1.0;
    mat3 TBN   = mat3(normalize(vTangent),normalize(vBitangent),normalize(vNormal));
    vec3 N     = normalize(TBN*nts);
    vec3 V     = normalize(camPos.xyz-vWorldPos);
    vec3 F0    = mix(vec3(0.04),albedo,metal);

    vec3 Lo=vec3(0.0);
    for(int i=0;i<lightCount;i++){
        int ltype=int(lightPositions[i].w);
        vec3 L; float att=1.0;
        if(ltype==0){
            L=normalize(-lightDirections[i].xyz);
        } else {
            vec3 d=lightPositions[i].xyz-vWorldPos;
            float dist=length(d);
            L=normalize(d);
            att=max(1.0-dist/lightDirections[i].w,0.0);att*=att;
            if(ltype==2){
                float th=dot(L,normalize(-lightDirections[i].xyz));
                float eps=lightAngles[i].x-lightAngles[i].y;
                att*=clamp((th-lightAngles[i].y)/eps,0.0,1.0);
            }
        }
        vec3 H=normalize(V+L);
        float NdL=max(dot(N,L),0.0);
        vec3 rad=lightColors[i].rgb*lightColors[i].w*att;
        float NDF=DistributionGGX(N,H,roug);
        float G=GeometrySmith(N,V,L,roug);
        vec3 F=FresnelSchlick(max(dot(H,V),0.0),F0);
        vec3 spec=NDF*G*F/(4.0*max(dot(N,V),0.0)*NdL+0.001);
        vec3 kD=(1.0-F)*(1.0-metal);
        Lo+=(kD*albedo/PI+spec)*rad*NdL;
    }

    // IBL
    vec3 Fib=FresnelRough(max(dot(N,V),0.0),F0,roug);
    vec3 kDi=(1.0-Fib)*(1.0-metal);
    vec3 irr=texture(tEnvIrradiance,N).rgb;
    vec3 R=reflect(-V,N);
    vec3 pref=textureLod(tEnvPrefilter,R,roug*4.0).rgb;
    vec2 brdf=texture(tBRDFLUT,vec2(max(dot(N,V),0.0),roug)).rg;
    vec3 amb=(kDi*irr*albedo+pref*(Fib*brdf.x+brdf.y))*ao;

    vec3 emis=texture(tEmissive,vUV).rgb*emissiveStrength;
    fragColor=vec4(amb+Lo+emis,alpha);
}
