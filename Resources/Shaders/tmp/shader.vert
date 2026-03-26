#version 450

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    const int isVulkan = 1;
#else
    const int isVulkan = 0;
#endif

out gl_PerVertex {
    vec4 gl_Position;
};

layout(location = 0) out vec3 fragColor;

vec2 positions[3] = vec2[](
    vec2(0.1, mix(0.5, -0.5, isVulkan)),
    vec2(0.5, mix(-0.5, 0.5, isVulkan)),
    vec2(-0.5, mix(-0.5, 0.5, isVulkan))
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(1.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main()
{
    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    fragColor = colors[gl_VertexID];
}