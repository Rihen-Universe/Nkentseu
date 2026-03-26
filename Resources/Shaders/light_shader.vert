#version 450 core

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 proj;

void main()
{
	gl_Position = proj * view * model * vec4(in_Position, 1.0);
}
