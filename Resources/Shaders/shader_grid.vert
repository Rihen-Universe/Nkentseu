#version 450 core

const vec3 position[4] = vec3[4](
	vec3(-1.0, 0.0, -1.0),
	vec3( 1.0, 0.0, -1.0),
	vec3( 1.0, 0.0,  1.0),
	vec3(-1.0, 0.0,  1.0)
);

const int indices[6] = int[6](
	0, 1, 2, 2, 3, 0
);

uniform mat4 view;
uniform mat4 proj;
uniform vec3 color;

out vec4 worldPosition;
out vec3 gridColor;

void main() {
	int idx = indices[gl_VertexID];
    worldPosition = view * vec4(position[idx], 1.0);
    gl_Position = proj * worldPosition;
	gridColor = color;
}
