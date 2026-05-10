// colorgrade.frag.vk.glsl — NKRenderer v4.0 — Color Grading (Vulkan)
#version 460 core
#extension GL_ARB_separate_shader_objects : enable
layout(location=0) in vec2 vUV; layout(location=0) out vec4 fragColor;
layout(set=1,binding=0) uniform sampler2D tLDR;
layout(set=1,binding=1) uniform sampler3D tLUT;    // 3D LUT 32x32x32
layout(set=0,binding=2,std140) uniform ColorGradeUBO{
    float contrast,saturation,brightness,hueShift;
    float shadows,midtones,highlights,lutStrength;
    vec4 colorFilter; vec4 splitShadow; vec4 splitHighlight;
}uCG;
vec3 ApplyContrast(vec3 c,float cont){return mix(vec3(0.5),c,cont);}
vec3 ApplySaturation(vec3 c,float sat){float lum=dot(c,vec3(0.2126,0.7152,0.0722));return mix(vec3(lum),c,sat);}
vec3 ApplyColorFilter(vec3 c,vec4 f){return c*f.rgb;}
void main(){
    vec3 c=texture(tLDR,vUV).rgb;
    c=ApplyColorFilter(c,uCG.colorFilter);
    c=ApplyContrast(c,uCG.contrast);
    c=c*uCG.brightness;
    c=ApplySaturation(c,uCG.saturation);
    // Split toning
    float lum=dot(c,vec3(0.2126,0.7152,0.0722));
    vec3 shadowTone=mix(c,uCG.splitShadow.rgb*uCG.splitShadow.a,1.-lum);
    vec3 hlTone=mix(c,uCG.splitHighlight.rgb*uCG.splitHighlight.a,lum);
    c=mix(c,shadowTone+hlTone-c,0.5);
    // LUT lookup
    if(uCG.lutStrength>0.){
        vec3 lutUV=clamp(c,0.,1.)*(31./32.)+0.5/32.;
        vec3 lutColor=texture(tLUT,lutUV).rgb;
        c=mix(c,lutColor,uCG.lutStrength);
    }
    fragColor=vec4(clamp(c,0.,1.),1.);
}
