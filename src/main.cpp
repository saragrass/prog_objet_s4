#include <cstdlib>
#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"
#include "p6/p6.h"
#include <iostream>
#include <random>
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

struct Boid {
    glm::vec3 position;
    glm::vec3 velocity;
    bool isFemale;
};

struct Model {
    GLuint vao; // Vertex Array Object
    GLuint vbo; // Vertex Buffer Object
    int numVertices; // Number of vertices
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
float domeRadius = 1.5f;

// Fonction pour obtenir la couleur en fonction du mode jour/nuit et du type de boid
glm::vec3 getBoidColor(bool dayMode, bool isFemale) {
    if (dayMode) {
        // Couleur des boids pendant le jour
        return isFemale ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.5f, 0.2f); // Vert pour les femelles, orange pour les autres
    } else {
        // Couleur des boids pendant la nuit
        return isFemale ? glm::vec3(0.0f, 0.5f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.1f); // Vert foncé pour les femelles, bleu foncé pour les autres
    }
}

Model loadModel(const char* objPath) {
    Model model;

    // Open the OBJ file
    std::ifstream file(objPath);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << objPath << std::endl;
        return model;
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
            iss >> vertexIndex1 >> slash >> texCoordIndex1 >> slash >> normalIndex1
                >> vertexIndex2 >> slash >> texCoordIndex2 >> slash >> normalIndex2
                >> vertexIndex3 >> slash >> texCoordIndex3 >> slash >> normalIndex3;
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

    // Set vertex attribute pointers
    glVertexAttribPointer(VERTEX_ATTR_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); // Position attribute (aPos)
    glEnableVertexAttribArray(VERTEX_ATTR_POSITION);

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

    // Load shaders using Shader class
    p6::Shader shader = p6::load_shader("shaders/3D.vs.glsl", "shaders/normals.fs.glsl");

    // Enable depth test
    glEnable(GL_DEPTH_TEST);

    // Variables de la caméra
    glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 3.0f);
    float cameraSpeed = 1.5f;

    // Declare variables for ImGui sliders
    int numBoids = 100;
    float speedBoids = 2.5f;
    float boidSize = 0.1f;

    // Create boids
    std::vector<Boid> boids(numBoids);
    int numFemales = numBoids * 0.2; // 20% des boids seront des femelles
    for (int i = 0; i < numBoids; ++i) {
        boids[i].position = glm::vec3(glm::linearRand(-1.5f, 1.5f),
                                      glm::linearRand(-1.5f, 1.5f),
                                      glm::linearRand(-1.5f, 1.5f));
        boids[i].velocity = glm::sphericalRand(speedBoids);
        // Définir aléatoirement si le boid est une femelle
        if (i < numFemales) {
            boids[i].isFemale = true;
        } else {
            boids[i].isFemale = false;
        }
    }

    // Load the OBJ model
    Model model = loadModel("assets/models/projet-sara-creation-3d-only-ghost-3.obj");
    if (model.numVertices == 0) {
        std::cerr << "Failed to load model" << std::endl;
        return -1;
    }
    // Create VAO and VBO for boid sphere
    Sphere boidSphere(boidSize, 16, 8);
    GLuint boidVBO, boidVAO;
    glGenBuffers(1, &boidVBO);
    glGenVertexArrays(1, &boidVAO);

    glBindVertexArray(boidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, boidVBO);
    glBufferData(GL_ARRAY_BUFFER, boidSphere.getVertexCount() * sizeof(ShapeVertex),
                boidSphere.getDataPointer(), GL_STATIC_DRAW);

    // Specify attribute pointers for boid sphere
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
        glm::vec3 backgroundColor = dayMode ? glm::vec3{0.06, 0.03, 0.5} : glm::vec3{0.0, 0.0, 0.1}; // Calculer la couleur du fond en fonction du mode jour ou nuit
        backgroundColor = glm::mix(backgroundColor, glm::vec3{0.8, 0.9, 1.0}, transition); // Appliquer la transition si nécessaire
        ctx.background(p6::Color{backgroundColor.r, backgroundColor.g, backgroundColor.b}); // Convertir la couleur en p6::Color

        // Get the current time
        float currentTime = ctx.time();

        // Compute time difference
        float deltaTime = ctx.delta_time();

        // Handle camera movement
        if (ctx.key_is_pressed(GLFW_KEY_UP)) {
            cameraPosition -= cameraSpeed * glm::vec3(0.0f, 0.0f, 1.0f) * deltaTime;
        }
        if (ctx.key_is_pressed(GLFW_KEY_DOWN)) {
            cameraPosition += cameraSpeed * glm::vec3(0.0f, 0.0f, 1.0f) * deltaTime;
        }
        if (ctx.key_is_pressed(GLFW_KEY_LEFT)) {
            cameraPosition -= cameraSpeed * glm::vec3(1.0f, 0.0f, 0.0f) * deltaTime;
        }
        if (ctx.key_is_pressed(GLFW_KEY_RIGHT)) {
            cameraPosition += cameraSpeed * glm::vec3(1.0f, 0.0f, 0.0f) * deltaTime;
        }
        if (ctx.key_is_pressed(GLFW_KEY_W)) {
            cameraPosition += cameraSpeed * glm::vec3(0.0f, 1.0f, 0.0f) * deltaTime;
        }
        if (ctx.key_is_pressed(GLFW_KEY_S)) {
            cameraPosition -= cameraSpeed * glm::vec3(0.0f, 1.0f, 0.0f) * deltaTime;
        }

        // Clear buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Compute matrices
        glm::mat4 ProjMatrix = glm::perspective(glm::radians(70.f), 1280.f / 720.f, 0.1f, 100.f);
        glm::mat4 MVMatrix = glm::lookAt(cameraPosition, cameraPosition + glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 NormalMatrix = glm::transpose(glm::inverse(MVMatrix));

        // Send matrices to the GPU
        shader.use(); // Use the shader
        shader.set("uMVPMatrix", ProjMatrix * MVMatrix);
        shader.set("uMVMatrix", MVMatrix);
        shader.set("uNormalMatrix", NormalMatrix);

        // Draw the model
        glBindVertexArray(model.vao);
        glDrawArrays(GL_TRIANGLES, 0, model.numVertices);
        glBindVertexArray(0);

        // Handle ImGui
        ImGui::Begin("Settings");
        ImGui::SliderInt("Number of Boids", &numBoids, 0, 500);
        ImGui::SliderFloat("Speed of Boids", &speedBoids, 1.0f, 10.0f);
        ImGui::SliderFloat("Boid Size", &boidSize, 0.1f, 1.0f);
        ImGui::Checkbox("Day/Night Mode", &dayMode);
        ImGui::SliderFloat("Alignment Weight", &alignmentWeight, 0.0f, 1.0f); // Ajouter un slider pour le poids d'alignement
        ImGui::SliderFloat("Cohesion Weight", &cohesionWeight, 0.0f, 1.0f); // Ajouter un slider pour le poids de cohésion
        ImGui::SliderFloat("Separation Distance", &separationDistance, 0.1f, 2.0f); // Ajouter un slider pour la distance de séparation

        ImGui::End();

        // Update number of boids
        if (numBoids > boids.size()) {
            int numFemalesToAdd = numBoids * 0.2 - (boids.size() - numFemales);
            for (int i = 0; i < numFemalesToAdd; ++i) {
                Boid boid;
                boid.position = glm::vec3(glm::linearRand(-1.5f, 1.5f),
                                            glm::linearRand(-1.5f, 1.5f),
                                            glm::linearRand(-1.5f, 1.5f));
                boid.velocity = glm::sphericalRand(speedBoids);
                boid.isFemale = true;
                boids.push_back(boid);
            }
            int numMalesToAdd = numBoids - numFemales - numFemalesToAdd;
            for (int i = 0; i < numMalesToAdd; ++i) {
                Boid boid;
                boid.position = glm::vec3(glm::linearRand(-1.5f, 1.5f),
                                            glm::linearRand(-1.5f, 1.5f),
                                            glm::linearRand(-1.5f, 1.5f));
                boid.velocity = glm::sphericalRand(speedBoids);
                boid.isFemale = false;
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
            boids[i].velocity = glm::normalize(boids[i].velocity) * speedBoids;

            // Appliquer simple intégration d'Euler pour mettre à jour la position du boid
            boids[i].position += boids[i].velocity * deltaTime;

            // Keep boids within the dome bounds
            float distanceToCenter = glm::length(boids[i].position);
            if (distanceToCenter > domeRadius) {
                boids[i].position = glm::normalize(boids[i].position);
                boids[i].velocity = glm::reflect(boids[i].velocity, glm::normalize(boids[i].position));
            }

            // Calculate boid's model matrix
            glm::mat4 boidModelMatrix = glm::translate(glm::mat4(1.0f), boids[i].position) * glm::scale(glm::mat4(1.0f), glm::vec3(boidSize));

            // Get the color of the current boid based on day/night mode and boid type
            glm::vec3 boidColor = getBoidColor(dayMode, boids[i].isFemale);

            // Send the color of the current boid to the shader
            shader.set("uColor", boidColor);

            // Send boid matrices to the GPU
            shader.set("uMVPMatrix", ProjMatrix * MVMatrix * boidModelMatrix);
            shader.set("uMVMatrix", MVMatrix * boidModelMatrix);
            shader.set("uNormalMatrix", glm::transpose(
                                glm::inverse(MVMatrix * boidModelMatrix)));

            // Bind VAO and draw boid sphere
            glBindVertexArray(boidVAO);
            glDrawArrays(GL_TRIANGLES, 0, boidSphere.getVertexCount());
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
    glDeleteBuffers(1, &boidVBO);
    glDeleteVertexArrays(1, &boidVAO);

    return EXIT_SUCCESS;
}
