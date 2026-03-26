#version 450

layout (location = 0) in VertexOutput
{
	vec2 texCoord;
	vec3 normal;
	vec3 fragPos;
} vertexOutput;

layout (location = 0) out vec4 FragColor;

struct Material{
	// [Ambient]
	vec3 ambient;
	// sampler2D ambientTexture;

	// [Diffuse]
	vec3 diffuse;
	int hasDifTex;
	sampler2D diffuseTexture;

	// [Specular]
	vec3 specular;
	int hasSpecTex;
	sampler2D specularTexture;

	// [Emissive]
	vec3 emissive;
	int hasEmitTex;
	sampler2D emissiveTexture;

    float shininess; // brillance
};

struct Light{
	int lightType;

	// position
	vec3 position;

	// directional light
	vec3 direction;

	// [Ambient]
	vec3 ambient;

	// [Diffuse]
	vec3 diffuse;

	// [Specular]
	vec3 specular;
};

uniform Material material;
uniform Light light;
uniform vec3 viewPos;

void main()
{
	vec3 norm = normalize(vertexOutput.normal);
	vec3 lightDir = vec3(0);

	if (light.lightType == 1){ // directional light
		lightDir = normalize(-light.direction);
	} else if (light.lightType == 2){ // ponctual
		lightDir = normalize(light.position - vertexOutput.fragPos);
	}

	float NdotL = dot(norm, lightDir);
    float diff = max(NdotL, 0.0);

	vec3 ambientFromTexture = material.ambient; // Valeur par défaut
	vec3 diffuseFromTexture = diff * material.diffuse; 
	vec3 specularFromTexture = vec3(0.0); 
	vec3 emissionFromTexture = material.emissive; 

	if (material.hasDifTex == 1) {
        // Utiliser la texture ambiante
        ambientFromTexture = vec3(texture(material.diffuseTexture, vertexOutput.texCoord));
		diffuseFromTexture = diff * ambientFromTexture;
    }
	if (NdotL > 0.0){
		vec3 viewDir = normalize(viewPos - vertexOutput.fragPos);
		vec3 reflectDir = reflect(-lightDir, norm);
		//vec3 reflectDir = reflect(lightDir, norm);
		float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

		specularFromTexture = material.specular * spec;
		if (material.hasSpecTex == 1) {
			// Utiliser la texture ambiante
			specularFromTexture = vec3(texture(material.specularTexture, vertexOutput.texCoord)) * spec;
		}
	}
	if (material.hasEmitTex == 1) {
        // Utiliser la texture ambiante
        emissionFromTexture = texture(material.emissiveTexture, vertexOutput.texCoord).rgb;
    }

	// [Calculate Ambient]
	vec3 ambient = light.ambient * ambientFromTexture;
	// [Calculate Diffuse]
    vec3 diffuse = light.diffuse * diffuseFromTexture;
	// [Calculate Specular]
	vec3 specular = light.specular * specularFromTexture;
	// [ Calculate Emission]
    vec3 emission = emissionFromTexture;

	vec3 result = ambient + diffuse + specular + emission;
    FragColor = vec4(result, 1.0);
} 
