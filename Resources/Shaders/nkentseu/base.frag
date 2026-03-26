#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;

// uniform sampler2D ourTexture;

void main() {
    //vec3 norm = normalize(normal);
    outColor = vec4(fragColor, 1.0);
    //if (ourTexture){
    //    outColor = texture(ourTexture, TexCoord);
    //}else{
    //    outColor = vec4(fragColor, 1.0);
    //}
}