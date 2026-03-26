#version 450

out gl_PerVertex { vec4 gl_Position; };

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;

uniform mat4 view;
uniform mat4 proj;
uniform mat4 model;
uniform mat4 modelNormal;

layout (location = 0) out VertexOutput
{
	vec2 texCoord;
	vec3 normal;
	vec3 fragPos;
	// vec3 lightDir; // Direction normalisée de la lumière vers le fragment
} vertexOutput;

void main()
{
	vertexOutput.texCoord = in_TexCoord;
	// effectuer le calcul mat3(transpose(inverse(model))) = modelNormal; depuis le programme principal
	//vertexOutput.normal =  mat3(transpose(inverse(model))) * in_Normal;
	vertexOutput.normal = in_Normal;
	vertexOutput.fragPos = vec3(model * vec4(in_Position, 1.0));

	// Calcule de la direction de la lumière vers le fragment
	// vertexOutput.lightDir = normalize(lightPosition - vertexOutput.fragPos);
	
	gl_Position = proj * view  * vec4(vertexOutput.fragPos, 1.0);;
}
//  0.539215 : 1.20711 : 0.701201 : 0.701201
