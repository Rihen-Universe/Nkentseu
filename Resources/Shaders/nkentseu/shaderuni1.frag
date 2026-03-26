#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;

layout (binding = 0) uniform sampler2D texture_sampler;

void main() {
    vec4 color = vec4(1.0, 1.0, 1.0, 1.0);
    if (fragColor.a != 0.0){
        color = vec4(fragColor.rgb, 1.0);
    }
    outColor = texture(texture_sampler, texCoord) * color;
}