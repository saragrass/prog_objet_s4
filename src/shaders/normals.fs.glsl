// Fragment shader
#version 330 core

in vec3 frag_Normal;
in vec3 frag_Position;
in vec2 frag_TexCoord;

out vec4 out_Color;

uniform vec3 uColor; // Ajout de la variable uniforme pour la couleur

void main() {
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 objectColor = uColor; // Utilisation de la couleur définie par la variable uniforme

    vec3 lightPos = vec3(0.0, 0.0, 0.0); // Position de la lumière fixée pour l'exemple
    vec3 viewPos = vec3(0.0, 0.0, 5.0); // Position de la vue fixée pour l'exemple

    // Calcul de la lumière diffuse
    vec3 normal = normalize(frag_Normal);
    vec3 lightDir = normalize(lightPos - frag_Position);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * objectColor;

    out_Color = vec4(diffuse, 1.0);
}