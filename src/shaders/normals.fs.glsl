#version 330 core

in vec3 frag_Normal;
in vec3 frag_Position;
in vec2 frag_TexCoord;

out vec4 out_Color;

uniform vec3 uColor; // Couleur de l'objet
uniform vec4 uDomeColor; // Couleur du dome avec alpha
uniform vec3 surveyorPosition; // Position de l'arpenteur
uniform vec3 boidPosition; // Position de l'arpenteur

void main() {
    vec3 lightColor1 = vec3(0.0, 0.0, 1.0); // Couleur de la première lumière
    vec3 lightColor2 = vec3(1.0, 1.0, 1.0); // Couleur de la deuxième lumière
    vec3 lightColorSurveyor = vec3(1.0, 1.0, 0.0); // Couleur de la lumière sur l'arpenteur
    vec3 lightColorBoid = vec3(0.0, 1.0, 0.0); // Couleur de la lumière sur les boids

    vec3 lightPos1 = vec3(5.0, 0.0, 0.0); // Position de la première lumière (fixe)
    vec3 lightPos2 = vec3(0.0, -5.0, 0.0); // Position de la deuxième lumière (fixe)
    vec3 lightPosSurveyor = surveyorPosition; // Position de la lumière sur l'arpenteur
    vec3 lightPosBoid = boidPosition; // Position de la lumière sur le boid

    vec3 objectColor = uColor; // Utilisation de la couleur définie par la variable uniforme

    // Calcul de la lumière diffuse pour la première lumière
    vec3 normal = normalize(frag_Normal);
    vec3 lightDir1 = normalize(lightPos1 - frag_Position);
    float diff1 = max(dot(normal, lightDir1), 0.0);
    vec3 diffuse1 = diff1 * lightColor1 * objectColor;
    
    // Calcul de la lumière diffuse pour la deuxième lumière
    vec3 lightDir2 = normalize(lightPos2 - frag_Position);
    float diff2 = max(dot(normal, lightDir2), 0.0);
    vec3 diffuse2 = diff2 * lightColor2 * objectColor;

    // Calcul de la lumière diffuse pour la lumière sur l'arpenteur
    vec3 lightDirSurveyor = normalize(lightPosSurveyor - frag_Position);
    float diffSurveyor = max(dot(normal, lightDirSurveyor), 0.0);
    vec3 diffuseSurveyor = diffSurveyor * lightColorSurveyor * objectColor;

    // Calcul de la lumière diffuse pour la lumière sur le boid
    vec3 lightDirBoid = normalize(lightPosBoid - frag_Position);
    float diffBoid = max(dot(normal, lightDirBoid), 0.0);
    vec3 diffuseBoid = diffBoid * lightColorBoid * objectColor;

    // Additionner les contributions de chaque lumière
    vec3 finalDiffuse = diffuse1 + diffuse2 + diffuseSurveyor + diffuseBoid;

    // Utiliser la composante alpha du dome pour la transparence
    out_Color = vec4(finalDiffuse, uDomeColor.a);
}
