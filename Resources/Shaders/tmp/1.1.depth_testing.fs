#version 450 core
out vec4 FragColor;

float near = 0.1;
float far = 100.0;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

in vec2 TexCoords;

uniform sampler2D texture1;

void main()
{    
    vec4 texColor = texture(texture1, TexCoords);
    if(texColor.a < 0.1)
        discard;
    FragColor = texColor;
    //FragColor = texture(texture1, TexCoords);
    // FragColor = vec4(vec3(gl_FragCoord.z), 1.0);
    //float depth = LinearizeDepth(gl_FragCoord.z) / far; // division par far pour la démonstration
    //FragColor = vec4(vec3(depth), 1.0);
}