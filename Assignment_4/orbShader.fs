#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

uniform vec3 emissionColor;
uniform float emissionStrength;

void main()
{
    // Ambient
    float ambientStrength = 0.25;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.4;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16);
    vec3 specular = specularStrength * spec * lightColor;

    // Emission
    vec3 emission = emissionColor * emissionStrength;

    float rim = 1.0 - max(dot(norm, viewDir), 0.0);
    rim = pow(rim, 1.0);     // almost linear very wide halo
    rim = clamp(rim, 0.0, 1.0);

    vec3 rimGlow = emissionColor * rim * (emissionStrength * 10.0);

    // Final output
    vec3 result = ambient * objectColor
                + diffuse * objectColor
                + specular
                + emission
                + rimGlow;   // outer halo

    FragColor = vec4(result, 1.0);
}
