#version 330 core

in vec3 frag_Normal;
in vec3 frag_Position;
in vec2 frag_TexCoord; // Ajout de l'entrée de texture

out vec4 out_Color;

uniform vec3 uColor; // Variable uniforme pour la couleur
uniform sampler2D uTexture; // Sampler pour la texture

void main() {
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 objectColor = uColor;

    vec3 lightPos = vec3(0.0, 0.0, 0.0);
    vec3 viewPos = vec3(0.0, 0.0, 5.0);

    vec3 normal = normalize(frag_Normal);
    vec3 lightDir = normalize(lightPos - frag_Position);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor * objectColor;

    // Échantillonner la texture à partir des coordonnées de texture
    vec4 textureColor = texture(uTexture, frag_TexCoord);

    // Combinez la couleur diffuse avec la couleur de la texture
    out_Color = vec4(diffuse * textureColor.rgb, 1.0);
}
