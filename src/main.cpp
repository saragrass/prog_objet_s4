#include <cstdlib> // pour std::rand() et std::srand() et autre
#include <ctime>   // pour std::time()
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "p6/p6.h"
#include <iostream>
#include <random>
#include <filesystem> // pour std::filesystem::path
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <map>
#include <cmath>
#include "imgui.h"
#include "sphere.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/random.hpp" // Pour glm::sphericalRand 
#include "glm/gtc/type_ptr.hpp"
#include <vector>

#define VERTEX_ATTR_POSITION 0
#define VERTEX_ATTR_NORMAL 1
#define VERTEX_ATTR_TEXCOORDS 2

float random(float min, float max) {
    return min + static_cast <float> (rand()) / (static_cast<float>(RAND_MAX + 1) / (max - min));
}

float randomNormal(float mean, float stddev) {
    const int numSamples = 12; // Nombre de valeurs aléatoires uniformes à additionner
    float sum = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        sum += random(0.0f, 1.0f); // Ajouter des valeurs aléatoires uniformément distribuées
    }
    return mean + stddev * (sum - numSamples / 2.0f) / (numSamples / 2.0f); // Normalisation
}

struct Boid {
    glm::vec3 position;
    glm::vec3 velocity;
    bool isFemale;
    float alignmentWeight;
    float cohesionWeight;
    float separationWeight;
    float interactionRadius;
    int markovState;
    glm::vec3 color;
};

struct Model {
    GLuint vao; // Vertex Array Object
    GLuint vbo; // Vertex Buffer Object
    GLuint vboNormals; // Vertex Buffer Object for normals
    GLuint vboTexCoords; // Vertex Buffer Object for texture coordinates
    int numVertices; // Number of vertices
    std::map<std::string, GLuint> materialTextureIDs; // Texture IDs per material
};

struct Surveyor {
    glm::vec3 position;
    float speed;
};

struct TextureInfo {
    std::string path;
    float scaleX;
    float scaleY;
    float scaleZ;
};

float separationDistance = 0.1f; // Distance minimale de séparation des boids
bool dayMode = true; // Mode jour ou nuit
float transition = 0.0f; // Valeur de transition pour le fondu

// Facteurs de pondération pour les règles de comportement des boids
float alignmentWeight = 0.1f;
float cohesionWeight = 0.1f;

// Facteurs pour la règle d'évitement de la caméra
float distanceMinToCamera = 0.2f;
float avoidanceWeight = 0.2f;

// Rayon du dôme
float domeRadius = 2.0f;

int countNeighbors(const Boid& boid, const std::vector<Boid>& boids, int numBoids) {
    int count = 0;
    for (int i = 0; i < numBoids; ++i) {
        if (&boid != &boids[i]) {
            float distance = glm::distance(boid.position, boids[i].position);
            if (distance < boid.interactionRadius) {
                ++count;
            }
        }
    }
    return count;
}

void updateMarkovState(Boid& boid, const std::vector<Boid>& boids, int numBoids) {
    int neighborCount = countNeighbors(boid, boids, numBoids);
    
    // Si le boid a plus de 5 voisins, changer son état de la chaîne de Markov
    if (neighborCount > 5) {
        boid.markovState = 1;
    } else {
        boid.markovState = 0;
    }
}

// Fonction pour obtenir la couleur en fonction du mode jour/nuit et du type de boid
glm::vec3 getBoidColor(bool markovState, bool isFemale) {
    if (markovState) {
        // Couleur des boids pendant le jour
        return isFemale ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(1.0f, 1.0f, 1.0f); // Vert pour les femelles, orange pour les autres
    } else {
        // Couleur des boids pendant la nuit
        return glm::vec3(1.0f, 1.0f, 1.0f);
    }
}

Model loadModel(const char* objPath, const char* mtlPath) {
    Model model;

    // Open the OBJ file
    std::ifstream file(objPath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << objPath << std::endl;
        return model;
    }

    // Open the MTL file
    std::ifstream mtlFile(mtlPath);
    if (!mtlFile.is_open()) {
        std::cerr << "Error: Could not open file " << mtlPath << std::endl;
        return model;
    }

    std::map<std::string, std::pair<std::string, glm::vec3>> materialTexturePaths;

    // Process the MTL file
    std::string lineTexture;
    std::string currentMaterial;
    while (std::getline(mtlFile, lineTexture)) {
        std::istringstream iss(lineTexture);
        std::string type;
        iss >> type;
        if (type == "newmtl") {
            iss >> currentMaterial;
        } else if (type == "map_Kd") {
            std::string texturePath;
            iss >> texturePath;
            // Check if there are scaling parameters
            glm::vec3 scaleParameters(1.0f);
            if (texturePath == "-s") {
                iss >> scaleParameters.x >> scaleParameters.y >> scaleParameters.z;
                // Read the actual texture path
                iss >> texturePath;
            }
            // Adjust texture path to match the directory structure
            texturePath = "img/" + texturePath;
            // Store the texture path along with its scaling parameters
            materialTexturePaths[currentMaterial] = std::make_pair(texturePath, scaleParameters);
        }
    }

    // Load textures and associate them with materials
    for (const auto& [material, textureInfo] : materialTexturePaths) {
        // Log the texture path before loading
        std::cout << "Loading texture for material: " << material << ", path: " << textureInfo.first << std::endl;

        // Load texture using p6::load_image or any other method you prefer
        // For example:
        p6::Image textureImage = p6::load_image(textureInfo.first.c_str(), true);

        // Concatenate scaling parameters if present
        std::string scaleParams = std::to_string(textureInfo.second.x) + " " + std::to_string(textureInfo.second.y) + " " + std::to_string(textureInfo.second.z);
        std::string texturePathWithScale = textureInfo.first + " -s " + scaleParams;

        // Generate OpenGL texture and bind it
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        // No need to upload image data explicitly, handled internally by p6::Image

        // Set texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Associate texture with material
        // You may need to modify your Model struct to store texture IDs per material
        model.materialTextureIDs[material] = textureID;
    }

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<unsigned int> indices;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        if (type == "v") {
            glm::vec3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            vertices.push_back(vertex);
        } else if (type == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        } else if (type == "vt") {
            glm::vec2 texCoord;
            iss >> texCoord.s >> texCoord.t;
            texCoords.push_back(texCoord);
        } else if (type == "f") {
            unsigned int vertexIndex1, vertexIndex2, vertexIndex3;
            unsigned int normalIndex1, normalIndex2, normalIndex3;
            unsigned int texCoordIndex1, texCoordIndex2, texCoordIndex3;
            char slash;
            iss >> vertexIndex1 >> slash >> normalIndex1 >> slash >> texCoordIndex1
                >> vertexIndex2 >> slash >> normalIndex2 >> slash >> texCoordIndex2
                >> vertexIndex3 >> slash >> normalIndex3 >> slash >> texCoordIndex3;

            // OBJ indices start from 1, so we need to decrement them to match C++ indexing
            indices.push_back(vertexIndex1 - 1);
            indices.push_back(vertexIndex2 - 1);
            indices.push_back(vertexIndex3 - 1);
        }
    }

    model.numVertices = indices.size();

    // Generate and bind VAO and VBO
    glGenVertexArrays(1, &model.vao);
    glGenBuffers(1, &model.vbo);

    glBindVertexArray(model.vao);
    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers for positions
    glVertexAttribPointer(VERTEX_ATTR_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); // Position attribute (aPos)
    glEnableVertexAttribArray(VERTEX_ATTR_POSITION);

    // Generate and bind VBO for normals
    GLuint vboNormals;
    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers for normals
    glVertexAttribPointer(VERTEX_ATTR_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); // Normal attribute (aNormal)
    glEnableVertexAttribArray(VERTEX_ATTR_NORMAL);

    // Generate and bind VBO for texture coordinates
    GLuint vboTexCoords;
    glGenBuffers(1, &vboTexCoords);
    glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords);
    glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(glm::vec2), texCoords.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers for texture coordinates
    glVertexAttribPointer(VERTEX_ATTR_TEXCOORDS, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0); // Texture coordinates attribute (aTexCoord)
    glEnableVertexAttribArray(VERTEX_ATTR_TEXCOORDS);

    // Generate and bind EBO (Element Buffer Object)
    GLuint ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Unbind VAO
    glBindVertexArray(0);

    return model;
}


int main() {
    auto ctx = p6::Context{{1280, 720, "fireflies around bignones"}};
    ctx.maximize_window();
    std::srand(std::time(nullptr));

    // Load shaders using Shader class
    p6::Shader shader = p6::load_shader("shaders/3D.vs.glsl", "shaders/normals.fs.glsl");

    // Enable depth test
    glEnable(GL_DEPTH_TEST);

    // Variables de la caméra
    glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 6.0f);
    float cameraSpeed = 1.5f;

    // Declare variables for ImGui sliders
    int numBoids = 25;
    float speedBoids = 2.5f;
    float boidSize = 0.05f;

    // Create boids
    std::vector<Boid> boids(numBoids);
    for (int i = 0; i < numBoids; ++i) {
        // Position aléatoire des boids dans la sphère
        float theta = random(0.0f, 2.0f * static_cast<float>(M_PI));
        float phi = random(0.0f, static_cast<float>(M_PI));
        float r = boidSize * cbrt(random(0.0f, 1.0f));
        float x = r * sin(phi) * cos(theta);
        float y = r * sin(phi) * sin(theta);
        float z = r * cos(phi);
        boids[i].position = glm::vec3(x, y, z);

        // Vitesse aléatoire des boids dans une certaine plage
        boids[i].velocity = glm::sphericalRand(speedBoids);
        
        // Définir aléatoirement si le boid est une femelle
        boids[i].isFemale = (rand() % 2 == 0); 

        // Poids des règles de comportement (distribution normale)
        boids[i].alignmentWeight = randomNormal(0.75f, 0.1f);
        boids[i].cohesionWeight = randomNormal(0.75f, 0.1f);
        boids[i].separationWeight = randomNormal(0.75f, 0.1f);

        // Rayon de la zone d'interaction (distribution uniforme)
        boids[i].interactionRadius = random(1.0f, 2.0f);

        // État initial de la chaîne de Markov
        boids[i].markovState = 0;
    }

    // Load the OBJ model
    Model ghostModel = loadModel("assets/models/projet-sara-creation-3d-only-ghost-3.obj","assets/models/projet-sara-creation-3d-only-ghost-3.mtl");
    if (ghostModel.numVertices == 0) {
        std::cerr << "Failed to load model" << std::endl;
        return -1;
    }

    // Load the OBJ model
    Model surveyorModel = loadModel("assets/models/projet-sara-creation-3d-only-box-1.obj","assets/models/projet-sara-creation-3d-only-box-1.mtl");
    if (surveyorModel.numVertices == 0) {
        std::cerr << "Failed to load model" << std::endl;
        return -1;
    }

    // Create surveyor
    Surveyor surveyor;
    surveyor.position = glm::vec3{1.0f, -3.0f, 0.0f};
    surveyor.speed = 0.5f;

    // Create dome
    Sphere dome(domeRadius, 32, 16);
    GLuint domeVBO, domeVAO;
    glGenBuffers(1, &domeVBO);
    glGenVertexArrays(1, &domeVAO);

    glBindVertexArray(domeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, domeVBO);
    glBufferData(GL_ARRAY_BUFFER, dome.getVertexCount() * sizeof(ShapeVertex),
                dome.getDataPointer(), GL_STATIC_DRAW);

    // Specify attribute pointers for dome
    glEnableVertexAttribArray(VERTEX_ATTR_POSITION);
    glVertexAttribPointer(VERTEX_ATTR_POSITION, 3, GL_FLOAT, GL_FALSE,
                        sizeof(ShapeVertex),
                        (const GLvoid *)offsetof(ShapeVertex, position));
    glEnableVertexAttribArray(VERTEX_ATTR_NORMAL);
    glVertexAttribPointer(VERTEX_ATTR_NORMAL, 3, GL_FLOAT, GL_FALSE,
                        sizeof(ShapeVertex),
                        (const GLvoid *)offsetof(ShapeVertex, normal));
    glEnableVertexAttribArray(VERTEX_ATTR_TEXCOORDS);
    glVertexAttribPointer(VERTEX_ATTR_TEXCOORDS, 2, GL_FLOAT, GL_FALSE,
                        sizeof(ShapeVertex),
                        (const GLvoid *)offsetof(ShapeVertex, texCoords));

    // Unbind VAO
    glBindVertexArray(0);

    // Boucle de mise à jour des boids
    ctx.update = [&]() {
    glm::vec3 backgroundColor = dayMode ? glm::vec3{0.06, 0.03, 0.5} : glm::vec3{0.0, 0.0, 0.1}; 
    backgroundColor = glm::mix(backgroundColor, glm::vec3{0.8, 0.9, 1.0}, transition); 
    ctx.background(p6::Color{backgroundColor.r, backgroundColor.g, backgroundColor.b}); 

    float currentTime = ctx.time();
    float deltaTime = ctx.delta_time();

    // Handle input for moving the surveyor
        if (ctx.key_is_pressed(GLFW_KEY_LEFT)) {
            surveyor.position.x -= surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_RIGHT)) {
            surveyor.position.x += surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_UP)) {
            surveyor.position.y += surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_DOWN)) {
            surveyor.position.y -= surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_A)) {
            surveyor.position.z -= surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_Z)) {
            surveyor.position.z += surveyor.speed * ctx.delta_time();
        }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 ProjMatrix = glm::perspective(glm::radians(70.f), 1280.f / 720.f, 0.1f, 100.f);
    glm::mat4 MVMatrix = glm::lookAt(cameraPosition, cameraPosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 NormalMatrix = glm::transpose(glm::inverse(MVMatrix));

    shader.use(); 
    shader.set("uMVPMatrix", ProjMatrix * MVMatrix);
    shader.set("uMVMatrix", MVMatrix);
    shader.set("uNormalMatrix", NormalMatrix);

    ImGui::Begin("Settings");
    ImGui::SliderInt("Number of Boids", &numBoids, 1, 100);
    ImGui::SliderFloat("Speed of Boids", &speedBoids, 1.0f, 10.0f);
    ImGui::SliderFloat("Boid Size", &boidSize, 0.01f, 0.1f);
    ImGui::Checkbox("Day/Night Mode", &dayMode);
    ImGui::SliderFloat("Alignment Weight", &alignmentWeight, 0.0f, 1.0f); 
    ImGui::SliderFloat("Cohesion Weight", &cohesionWeight, 0.0f, 1.0f); 
    ImGui::SliderFloat("Separation Distance", &separationDistance, 0.1f, 2.0f); 

    ImGui::End();

    // Bind dome VAO
    glBindVertexArray(domeVAO);

    // Render dome
    glm::mat4 domeModelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(domeRadius));
    shader.set("uModelMatrix", domeModelMatrix);

    // Set dome color
    glm::vec3 domeColor = glm::vec3(1.0f, 0.0f, 0.0f); // Gray color for dome
    shader.set("uColor", domeColor);

    // Draw dome
    glDrawArrays(GL_TRIANGLES, 0, dome.getVertexCount());

    // Unbind VAO
    glBindVertexArray(0);

    // Rotation angle in degrees
    float surveyorRotationAngleY = 100.0f;

    // Convert rotation angle to radians
    float surveyorRotationAngleYRadians = glm::radians(surveyorRotationAngleY);

    // Apply rotation around the Y axis to the model matrix
    glm::mat4 surveyorModelMatrix = glm::rotate(glm::translate(glm::mat4(1.0f), surveyor.position), surveyorRotationAngleYRadians, glm::vec3(0.0f, 1.0f, 0.0f));

    // Render surveyor
    //glm::mat4 surveyorModelMatrix = glm::translate(glm::mat4(1.0f), surveyor.position);
    shader.set("uModelMatrix", surveyorModelMatrix);
    glm::vec3 surveyorColor = glm::vec3(1.0f, 1.0f, 1.0f); // White color for surveyor
    shader.set("uColor", surveyorColor);
    glBindVertexArray(surveyorModel.vao);
    glDrawElements(GL_TRIANGLES, surveyorModel.numVertices, GL_UNSIGNED_INT, 0);

    for (int i = 0; i < numBoids; ++i) {
        // Vérifier si c'est la nuit pour dessiner les fantômes
        if (!dayMode) {
            // Rotation autour de l'axe x
            float angleDegreesX = -45.0f;
            float angleRadiansX = glm::radians(angleDegreesX);
            glm::quat rotationQuatX = glm::angleAxis(angleRadiansX, glm::vec3(1.0f, 0.0f, 0.0f));
            glm::mat4 rotationMatrixX = glm::mat4_cast(rotationQuatX);

            // Rotation autour de l'axe y
            float angleDegreesY = -75.0f;
            float angleRadiansY = glm::radians(angleDegreesY);
            glm::quat rotationQuatY = glm::angleAxis(angleRadiansY, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 rotationMatrixY = glm::mat4_cast(rotationQuatY);

            // Concaténation des deux rotations
            glm::mat4 totalRotationMatrix = rotationMatrixX * rotationMatrixY;

            // Appliquer la rotation à la matrice du modèle
            glm::mat4 boidModelMatrix = glm::translate(glm::mat4(1.0f), boids[i].position) * totalRotationMatrix * glm::scale(glm::mat4(1.0f), glm::vec3(boidSize));
            
            shader.set("uModelMatrix", glm::mat4(1.0f));
            glm::vec3 boidColor = getBoidColor(boids[i].markovState, boids[i].isFemale);
            shader.set("uColor", boidColor);
            shader.set("uMVPMatrix", ProjMatrix * MVMatrix * boidModelMatrix);
            shader.set("uMVMatrix", MVMatrix * boidModelMatrix);
            shader.set("uNormalMatrix", glm::transpose(glm::inverse(MVMatrix * boidModelMatrix)));

            glBindVertexArray(ghostModel.vao);
            glDrawElements(GL_TRIANGLES, ghostModel.numVertices, GL_UNSIGNED_INT, 0);
        }
        // Mise à jour de l'état de la chaîne de Markov en fonction du nombre de voisins
        updateMarkovState(boids[i], boids, numBoids);
    }
    // Update number of boids
        if (numBoids > boids.size()) {
            for (int i = 0; i < numBoids; ++i){
                Boid boid;
                // Position aléatoire des boids dans la sphère
                float theta = random(0.0f, 2.0f * static_cast<float>(M_PI));
                float phi = random(0.0f, static_cast<float>(M_PI));
                float r = boidSize * cbrt(random(0.0f, 1.0f));
                float x = r * sin(phi) * cos(theta);
                float y = r * sin(phi) * sin(theta);
                float z = r * cos(phi);
                boid.position = glm::vec3(x, y, z);

                // Vitesse aléatoire des boids dans une certaine plage
                boid.velocity = glm::sphericalRand(speedBoids);
                
                // Définir aléatoirement si le boid est une femelle
                boid.isFemale = (rand() % 2 == 0); 

                // Poids des règles de comportement (distribution normale)
                boid.alignmentWeight = randomNormal(0.75f, 0.1f);
                boid.cohesionWeight = randomNormal(0.75f, 0.1f);
                boid.separationWeight = randomNormal(0.75f, 0.1f);

                // Rayon de la zone d'interaction (distribution uniforme)
                boid.interactionRadius = random(0.1f, 0.5f);

                boids.push_back(boid);
            }
        } else if (numBoids < boids.size()) {
            boids.resize(numBoids);
        }

        // Variables pour stocker les vecteurs de séparation, alignement et cohésion pour chaque boid
        std::vector<glm::vec3> separation(numBoids, glm::vec3(0.0f));
        std::vector<glm::vec3> alignment(numBoids, glm::vec3(0.0f));
        std::vector<glm::vec3> cohesion(numBoids, glm::vec3(0.0f));

        // Calculer les vecteurs de séparation, alignement et cohésion
        for (int i = 0; i < numBoids; ++i) {
            for (int j = 0; j < numBoids; ++j) {
                if (i != j) {
                    // Règle de séparation
                    float distance = glm::length(boids[j].position - boids[i].position);
                    if (distance < separationDistance) {
                        separation[i] -= glm::normalize(boids[j].position - boids[i].position) / distance;
                    }

                    // Règle d'alignement
                    alignment[i] += boids[j].velocity;

                    // Règle de cohésion
                    cohesion[i] += boids[j].position;
                }
            }
        }

        // Appliquer les règles
        for (int i = 0; i < numBoids; ++i) {
            // Règle de séparation
            boids[i].velocity += separation[i];

            // Règle d'alignement
            alignment[i] /= numBoids - 1;
            boids[i].velocity += (alignment[i] - boids[i].velocity) * alignmentWeight;

            // Règle de cohésion
            cohesion[i] /= numBoids - 1;
            cohesion[i] = (cohesion[i] - boids[i].position) / glm::length(cohesion[i] - boids[i].position);
            boids[i].velocity += cohesion[i] * cohesionWeight;

            // Règle d'évitement de la caméra
            glm::vec3 directionToCamera = cameraPosition - boids[i].position;
            float distanceToCamera = glm::length(directionToCamera);
            if (distanceToCamera < distanceMinToCamera) {
                // Normaliser le vecteur de direction et ajouter à la vitesse
                glm::vec3 avoidance = glm::normalize(directionToCamera);
                boids[i].velocity += avoidance * avoidanceWeight;
            }

            // Normaliser la vitesse
            // Calculate direction to avoid the surveyor
            glm::vec3 directionFromSurveyor = glm::normalize(boids[i].position - surveyor.position);

            // Adjust boid velocity to move away from the surveyor
            boids[i].velocity += directionFromSurveyor * avoidanceWeight * ctx.delta_time();

            // Appliquer simple intégration d'Euler pour mettre à jour la position du boid
            boids[i].position += boids[i].velocity * deltaTime;

            // Keep boids within the dome bounds
            float distanceToCenter = glm::length(boids[i].position);
            if (distanceToCenter > domeRadius) {
                // Move the boid back inside the dome
                boids[i].position = glm::normalize(boids[i].position) * domeRadius;
            }   

            // Calculate boid's model matrix
            glm::mat4 boidModelMatrix = glm::translate(glm::mat4(1.0f), boids[i].position) * glm::scale(glm::mat4(1.0f), glm::vec3(boidSize));

            // Get the color of the current boid based on day/night mode and boid type
            glm::vec3 boidColor = getBoidColor(boids[i].markovState, boids[i].isFemale);

            // Send the color of the current boid to the shader
            shader.set("uColor", boidColor);

            // Send boid matrices to the GPU
            shader.set("uMVPMatrix", ProjMatrix * MVMatrix * boidModelMatrix);
            shader.set("uMVMatrix", MVMatrix * boidModelMatrix);
            shader.set("uNormalMatrix", glm::transpose(
                                glm::inverse(MVMatrix * boidModelMatrix)));

        }
        if (dayMode && transition < 1.0f) {
            transition += 0.01f;
        } else if (!dayMode && transition > 0.0f) {
            transition -= 0.01f;
        }
    };

    // Should be done last. It starts the infinite loop.
    ctx.start();

    // Clean up
    glDeleteBuffers(1, &domeVBO);
    glDeleteVertexArrays(1, &domeVAO);

    return EXIT_SUCCESS;
}
