#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 VertexColor;

uniform float emissionStrength;
uniform vec3 viewPos;
uniform vec3 objectColor;


void main()
{
    // rim glow
    vec3 viewDir = normalize(viewPos - FragPos);
    float rim = 1.0 - max(dot(normalize(Normal), viewDir), 0.0);
    rim = pow(rim, 3.0);

    vec3 color = objectColor * emissionStrength + objectColor * rim * 0.5;
    FragColor = vec4(color, 1.0);
}
