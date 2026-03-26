#version 450 core
out vec4 FragColor;

in vec3 Normal;  
in vec3 FragPos;
in vec2 TexCoords;  

struct Material {
    sampler2D ambient;
    sampler2D diffuse;
    sampler2D specular;
    sampler2D emission;
    float shininess;
};

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    // intensiter
    // float shininess;
};

struct PointLight {
    vec3 position;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    // intensiter
    // float shininess;
};

#define NR_POINT_LIGHTS 4
uniform PointLight pointLights[NR_POINT_LIGHTS];
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
uniform DirLight dirLight;

struct Light {
    // for punctual light
    vec3 position;
    // for directional light like sun
    vec3 direction;

    // properties
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;

    // attenuation for light
    float constant;
    float linear;
    float quadratic;

    // cone for spot light
    float cutOff;
    float outerCutOff;

    // light type
    int type;
};

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main()
{
    // Propriétés
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // phase 1 : éclairage directionnel
    vec3 result = CalcDirLight(dirLight, norm, viewDir);

    // phase 2 : lumières ponctuelles
    for(int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(pointLights[i], norm, FragPos, viewDir);

    // phase 3 : spot
    //result += CalcSpotLight(spotLight, norm, FragPos, viewDir);
    FragColor = vec4(result, 1.0);

    /*vec3 lightDir = normalize(light.position - FragPos);

    if (light.type == 0){
        // for directional light
        lightDir = normalize(-light.direction);
    }

    // ambient
    vec3 ambient = light.ambient * texture(material.diffuse, TexCoords).rgb;
    
    // diffuse 
    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * texture(material.diffuse, TexCoords).rgb;  
    
    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * texture(material.specular, TexCoords).rgb; 
    
    // emission
    vec3 emission = texture(material.emission, TexCoords).rgb;

    if (light.type == 2){
        // spotlight (soft edges)
        float theta = dot(lightDir, normalize(-light.direction)); 
        float epsilon = (light.cutOff - light.outerCutOff);
        float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
        diffuse  *= intensity;
        specular *= intensity;
    }
    
    // attenuation
    float distance    = length(light.position - FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    

    ambient  *= attenuation; 
    diffuse  *= attenuation;
    specular *= attenuation;   
    emission *= attenuation;
        
    vec3 result = ambient + diffuse + specular + emission;
    FragColor = vec4(result, 1.0);*/
} 

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    // eclairage diffus
    float diff = max(dot(normal, lightDir), 0.0);

    // eclairage speculaire
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // assemblage du resultat
    vec3 ambient = light.ambient * vec3(texture(material.diffuse, TexCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse, TexCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));

    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);

    // eclairage diffus
    float diff = max(dot(normal, lightDir), 0.0);

    // eclairage speculaire
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);

    // atténuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance +
    light.quadratic * (distance * distance));

    // assemblage du résultat
    vec3 ambient = light.ambient * vec3(texture(material.diffuse, TexCoords));
    vec3 diffuse = light.diffuse * diff * vec3(texture(material.diffuse, TexCoords));
    vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;

    return (ambient + diffuse + specular);
}