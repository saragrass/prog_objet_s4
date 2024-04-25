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
#include "glm/gtc/type_ptr.hpp"
#include <vector>
#include <cmath>

#define VERTEX_ATTR_POSITION 0
#define VERTEX_ATTR_NORMAL 1
#define VERTEX_ATTR_TEXCOORDS 2

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
    float lifespan;
    float markovTime;
};

struct Model {
    GLuint vao; // Vertex Array Object
    GLuint vbo; // Vertex Buffer Object
    GLuint ebo; // Element Buffer Object
    GLuint vboNormals; // Vertex Buffer Object for normals
    GLuint vboTexCoords; // Vertex Buffer Object for texture coordinates
    int numVertices; // Number of vertices
    std::map<std::string, GLuint> materialTextureIDs; // Texture IDs per material
    std::vector<glm::vec3> vertices; // Added member for storing vertices
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<unsigned int> indices;
};

struct Surveyor {
    glm::vec3 position;
    float rotationAngle;
    float speed;
};

struct TextureInfo {
    std::string path;
    float scaleX;
    float scaleY;
    float scaleZ;
};

// Déclaration d'une structure pour stocker chaque frame de l'animation
struct AnimationFrame {
    Model model;
    // Autres données nécessaires pour la frame, comme la position, l'orientation, etc.
};

float separationDistance = 0.1f; // Distance minimale de séparation des boids
bool dayMode = true; // Mode jour ou nuit
bool autoMode = false; // Mode jour ou nuit
float transition = 0.0f; // Valeur de transition pour le fondu
float elapsedTime = 0.0f; // Déclaration d'une variable pour suivre le temps écoulé depuis le début de l'animation

// Facteurs de pondération pour les règles de comportement des boids
float alignmentWeight = 0.1f;
float cohesionWeight = 0.1f;

// Facteurs pour la règle d'évitement de la caméra
float distanceMinToCamera = 0.2f;
float avoidanceWeight = 0.2f;
float distanceToSurveyor = 2.0f;

// Rayon du dôme
float domeRadius = 2.0f;

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

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string type;
        iss >> type;
        if (type == "v") {
            glm::vec3 vertex;
            iss >> vertex.x >> vertex.y >> vertex.z;
            model.vertices.push_back(vertex);
        } else if (type == "vn") {
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            model.normals.push_back(normal);
        } else if (type == "vt") {
            glm::vec2 texCoord;
            iss >> texCoord.s >> texCoord.t;
            model.texCoords.push_back(texCoord);
        } else if (type == "f") {
            unsigned int vertexIndex1, vertexIndex2, vertexIndex3;
            unsigned int normalIndex1, normalIndex2, normalIndex3;
            unsigned int texCoordIndex1, texCoordIndex2, texCoordIndex3;
            char slash;
            iss >> vertexIndex1 >> slash >> normalIndex1 >> slash >> texCoordIndex1
                >> vertexIndex2 >> slash >> normalIndex2 >> slash >> texCoordIndex2
                >> vertexIndex3 >> slash >> normalIndex3 >> slash >> texCoordIndex3;

            // OBJ indices start from 1, so we need to decrement them to match C++ indexing
            model.indices.push_back(vertexIndex1 - 1);
            model.indices.push_back(vertexIndex2 - 1);
            model.indices.push_back(vertexIndex3 - 1);
        }
    }

    model.numVertices = model.indices.size();

    // Generate and bind VAO and VBO
    glGenVertexArrays(1, &model.vao);
    glGenBuffers(1, &model.vbo);

    glBindVertexArray(model.vao);
    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * sizeof(glm::vec3), model.vertices.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers for positions
    glVertexAttribPointer(VERTEX_ATTR_POSITION, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); // Position attribute (aPos)
    glEnableVertexAttribArray(VERTEX_ATTR_POSITION);

    // Generate and bind VBO for normals
    GLuint vboNormals;
    glGenBuffers(1, &vboNormals);
    glBindBuffer(GL_ARRAY_BUFFER, vboNormals);
    glBufferData(GL_ARRAY_BUFFER, model.normals.size() * sizeof(glm::vec3), model.normals.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers for normals
    glVertexAttribPointer(VERTEX_ATTR_NORMAL, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0); // Normal attribute (aNormal)
    glEnableVertexAttribArray(VERTEX_ATTR_NORMAL);

    // Generate and bind VBO for texture coordinates
    GLuint vboTexCoords;
    glGenBuffers(1, &vboTexCoords);
    glBindBuffer(GL_ARRAY_BUFFER, vboTexCoords);
    glBufferData(GL_ARRAY_BUFFER, model.texCoords.size() * sizeof(glm::vec2), model.texCoords.data(), GL_STATIC_DRAW);

    // Set vertex attribute pointers for texture coordinates
    glVertexAttribPointer(VERTEX_ATTR_TEXCOORDS, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0); // Texture coordinates attribute (aTexCoord)
    glEnableVertexAttribArray(VERTEX_ATTR_TEXCOORDS);

    // Generate and bind EBO (Element Buffer Object)
    glGenBuffers(1, &model.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indices.size() * sizeof(unsigned int), model.indices.data(), GL_STATIC_DRAW);

    // Unbind VAO
    glBindVertexArray(0);

    return model;
}

// Load the OBJ model
Model newModel(const std::string& objFilePath, const std::string& mtlFilePath) {
    Model model = loadModel(objFilePath.c_str(), mtlFilePath.c_str());
    if (model.numVertices == 0) {
        std::cerr << "Failed to load model" << std::endl;
        exit(EXIT_FAILURE);
    }
    return model;
}

float random(float min, float max) {
    return min + static_cast <float> (rand()) / (static_cast<float>(RAND_MAX + 1) / (max - min));
}

// Génère un nombre aléatoire dans l'intervalle [min, max]
float linearRand(float min, float max) {
    // Calcul de la plage de valeurs
    float range = max - min;

    // Génération d'un nombre aléatoire dans [0, RAND_MAX]
    float randomNum = static_cast<float>(std::rand()) / RAND_MAX;

    // Mise à l'échelle et translation du nombre pour qu'il soit dans [min, max]
    return min + randomNum * range;
}

glm::vec3 customSphericalRand(float radius) {
    float theta = random(0.0f, 2.0f * static_cast<float>(M_PI));
    float phi = random(0.0f, static_cast<float>(M_PI));

    float sinPhi = sin(phi);

    float x = radius * sinPhi * cos(theta);
    float y = radius * sinPhi * sin(theta);
    float z = radius * cos(phi);

    return glm::vec3(x, y, z);
}

// Définir une fonction pour générer aléatoirement l'état de l'interrupteur (jour ou nuit) avec une distribution de Poisson
bool generateSwitchState(float lambda) {
    // Générer un nombre aléatoire selon la distribution de Poisson
    int k = 0;
    float p = 1.0;
    float L = std::exp(-lambda);
    float randVal = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

    while (p > L) {
        k++;
        randVal *= static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        p *= randVal;
    }

    // Si le nombre de "cliques" est impair, c'est la nuit, sinon c'est le jour
    return (k % 2 == 0);
}

float generateStateChangeTime(float lambda) {
    return -log(1.0f - static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) / lambda;
}

float generateExp(float lambda) {
    return -log(1.0f - static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) / lambda;
}

int countNeighbors(const Boid& boid, const std::vector<Boid>& boids, int numBoids) {
    int count = 0;
    for (int i = 0; i < numBoids; ++i) {
        if (&boid != &boids[i]) {
            float distance = glm::distance(boid.position, boids[i].position);
            if (distance < 1.0f) {
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
    float transitionDuration = generateExp(5); // Générer la durée de transition avec une distribution exponentielle
    if (markovState) {
        // Couleur des boids pendant le jour
        if (isFemale && transitionDuration > 0.0f) {
            // Calculer le facteur de transition en fonction du temps écoulé
            float transitionFactor = 1.0f - glm::clamp(transitionDuration, 0.0f, 1.0f);
            // Appliquer la transition vers le rouge
            return glm::mix(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), transitionFactor);
        } else {
            // Retourner la couleur blanche pour les autres boids ou lorsque la transition est terminée
            return glm::vec3(1.0f, 1.0f, 1.0f);
        }
    } else {
        // Couleur des boids pendant la nuit
        return glm::vec3(1.0f, 1.0f, 1.0f);
    }
}


void changeModelDetail(Model& model, int targetNumVertices) {
    // Obtenez le nombre actuel de sommets du modèle
    int currentNumVertices = model.numVertices;

    // Si le nombre de sommets actuel est inférieur ou égal à la cible,
    // il n'est pas nécessaire de simplifier le maillage
    if (currentNumVertices <= targetNumVertices) {
        return;
    }

    // Calculez le ratio de simplification
    float simplificationRatio = static_cast<float>(targetNumVertices) / static_cast<float>(currentNumVertices);

    // Appliquez la simplification en réduisant le nombre de sommets
    int newNumVertices = static_cast<int>(currentNumVertices * simplificationRatio);

    // Réduisez le nombre de sommets en supprimant les derniers sommets excédentaires
    model.vertices.resize(newNumVertices);

    // Mettez à jour les données du VBO avec les nouveaux sommets
    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER, model.vertices.size() * sizeof(glm::vec3), model.vertices.data(), GL_STATIC_DRAW);

    // Mettez à jour le nombre de sommets du modèle
    model.numVertices = newNumVertices;
}

std::vector<AnimationFrame> animationFrames;
// Déclarer une variable pour suivre l'indice de la frame actuelle
int currentFrameIndex = 0;

// Définir une fonction pour animer les frames
void animateFrames(float deltaTime) {
    // Incrémenter l'indice de la frame en fonction du temps écoulé
    currentFrameIndex = (currentFrameIndex + 1) % animationFrames.size();

    // Ton code pour afficher la frame actuelle ici
    Model currentFrameModel = animationFrames[currentFrameIndex].model;
    // Afficher la frame dans la scène
    // Par exemple :
    glBindVertexArray(currentFrameModel.vao);
    glDrawElements(GL_TRIANGLES, currentFrameModel.numVertices, GL_UNSIGNED_INT, 0);
}

int main() {
    auto ctx = p6::Context{{1280, 720, "pacman revenge"}};
    ctx.maximize_window();
    std::srand(std::time(nullptr));

    // Load shaders using Shader class
    p6::Shader shader = p6::load_shader("shaders/3D.vs.glsl", "shaders/normals.fs.glsl");

    // Enable depth test
    glEnable(GL_DEPTH_TEST);

    // Variables de la caméra
    glm::vec3 cameraPosition = glm::vec3(0.0f, 0.0f, 6.0f);
    glm::vec3 cameraDirection = glm::vec3(0.0f, 0.0f, -1.0f); // Direction de la caméra

    // Declare variables for ImGui sliders
    int numBoids = 25;
    float speedBoids = 2.5f;
    float boidSize = 0.05f;

    // Create boids
    std::vector<Boid> boids(numBoids);
    for (auto& boid : boids) {
        boid.position = glm::vec3(linearRand(-domeRadius, domeRadius),
                                      linearRand(-domeRadius, domeRadius),
                                      linearRand(-domeRadius, domeRadius));

        boid.velocity = customSphericalRand(speedBoids);
        
        // Définir aléatoirement si le boid est une femelle
        boid.isFemale = (rand() % 2 == 0);

        // État initial de la chaîne de Markov
        boid.markovState = 0;
        
        // Initialiser markovTime avec une valeur aléatoire entre 0 et la première transition
        boid.markovTime = generateStateChangeTime(5);

        // Générer la durée de vie du boid
        boid.lifespan = generateExp(1);
    }

    // Load the OBJ model
    Model ghostModel = newModel("assets/models/pacman_ghost_cube_v4.obj","assets/models/pacman_ghost_cube_v4.mtl");

    // Load the OBJ model
    Model switchModel = newModel("assets/models/seance6_switch.obj","assets/models/seance6_switch.mtl");

    int numberOfSwitch = 10;
    std::vector<glm::vec3> switchPos(numberOfSwitch);
    for (int i = 0; i < numberOfSwitch; ++i) {
        // Déclarer une variable pour stocker le nombre aléatoire
        float randomNumberX = linearRand(-domeRadius, domeRadius);
        float randomNumberY = linearRand(-domeRadius, domeRadius);
        float randomNumberZ = linearRand(-domeRadius, domeRadius);
        switchPos[i] = glm::vec3(randomNumberX, randomNumberY, randomNumberZ);
    }

    // Create surveyor
    Surveyor surveyor;
    surveyor.position = glm::vec3{0.0f, -2.75f, 1.0f};
    surveyor.rotationAngle = 0.0f;
    surveyor.speed = 2.5f;
    int targetNumVertices = ghostModel.numVertices;

    // Load the OBJ model
    // Vecteur pour stocker toutes les frames de l'animation
    // Chargement de chaque frame de l'animation depuis les fichiers OBJ et MTL
    int numFrames = 20;
    for (int i = 0; i < numFrames; ++i) {
        std::string objFilePath = "assets/models/pacman_cube_v3/pacman_cube_v3" + std::to_string(i+1) + ".obj";
        std::string mtlFilePath = "assets/models/pacman_cube_v3/pacman_cube_v3" + std::to_string(i+1) + ".mtl";
        // Convertir les std::string en const char *
        const char *objPath = objFilePath.c_str();
        const char *mtlPath = mtlFilePath.c_str();
        Model model = loadModel(objPath, mtlPath);
        if (model.numVertices == 0) {
            std::cerr << "Failed to load model" << std::endl;
            return -1;
        }
        // Ajouter la frame chargée au vecteur
        animationFrames.push_back({model});
    }

    // Create dome
    Sphere dome(domeRadius, 32, 16);
    GLuint domeVBO, domeVAO;
    glGenBuffers(1, &domeVBO);
    glGenVertexArrays(1, &domeVAO);


    // Boucle de mise à jour des boids
    ctx.update = [&]() {
        glm::vec3 backgroundColor = dayMode ? glm::vec3{0.06, 0.03, 0.5} : glm::vec3{0.0, 0.0, 0.5}; 
        backgroundColor = glm::mix(backgroundColor, glm::vec3{0.8, 0.9, 1.0}, transition); 
        ctx.background(p6::Color{backgroundColor.r, backgroundColor.g, backgroundColor.b}); 

        float deltaTime = ctx.delta_time();
        
        // Appeler la fonction d'animation des frames
        animateFrames(deltaTime);

        // Handle input for moving the surveyor
        if (ctx.key_is_pressed(GLFW_KEY_LEFT) && (-domeRadius < surveyor.position.x)) {
            surveyor.position.x -= surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_RIGHT) && (surveyor.position.x < domeRadius)) {
            surveyor.position.x += surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_UP) && (surveyor.position.y < domeRadius)) {
            surveyor.position.y += surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_DOWN) && (-domeRadius < surveyor.position.y)) {
            surveyor.position.y -= surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_A) && (-domeRadius < surveyor.position.z)) {
            surveyor.position.z -= surveyor.speed * ctx.delta_time();
        }
        if (ctx.key_is_pressed(GLFW_KEY_Z) && (surveyor.position.z < domeRadius)) {
            surveyor.position.z += surveyor.speed * ctx.delta_time();
        }

        // Handle rotation of surveyor (and camera) only when space key and arrow keys are pressed simultaneously
        if (ctx.key_is_pressed(GLFW_KEY_SPACE) && (ctx.key_is_pressed(GLFW_KEY_LEFT) || ctx.key_is_pressed(GLFW_KEY_RIGHT))) {
            float rotationSpeed = 100.0f; // Adjust as needed
            if (ctx.key_is_pressed(GLFW_KEY_RIGHT)) {
                // Rotate left
                surveyor.rotationAngle -= rotationSpeed * ctx.delta_time();
            }
            if (ctx.key_is_pressed(GLFW_KEY_LEFT)) {
                // Rotate right
                surveyor.rotationAngle += rotationSpeed * ctx.delta_time();
            }
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Convert rotation angle to radians
        float surveyorRotationAngleYRadians = glm::radians(surveyor.rotationAngle);

        // Mettre à jour la position de la caméra pour qu'elle suive l'arpenteur
        cameraPosition = surveyor.position + glm::vec3(0.0f, 1.0f, distanceToSurveyor); // Décalage en Z

        glm::mat4 ProjMatrix = glm::perspective(glm::radians(70.f), 1280.f / 720.f, 0.1f, 100.f);
        // Recalculer la matrice de vue en fonction de la nouvelle position et de la direction de la caméra
        glm::mat4 MVMatrix = glm::lookAt(cameraPosition, cameraPosition + cameraDirection, glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 NormalMatrix = glm::transpose(glm::inverse(MVMatrix));

        shader.use();
        shader.set("uMVPMatrix", ProjMatrix * MVMatrix);
        shader.set("uMVMatrix", MVMatrix);
        shader.set("uNormalMatrix", NormalMatrix);

        ImGui::Begin("Settings");
        ImGui::SliderInt("Number of Boids", &numBoids, 1, 100);
        ImGui::SliderFloat("Boid Size", &boidSize, 0.01f, 1.0f);
        ImGui::Checkbox("Day/Night Mode", &dayMode);
        ImGui::Checkbox("Day/Night Auto Mode", &autoMode);
        ImGui::SliderFloat("Alignment Weight", &alignmentWeight, 0.0f, 1.0f); 
        ImGui::SliderFloat("Cohesion Weight", &cohesionWeight, 0.0f, 1.0f); 
        ImGui::SliderFloat("Separation Distance", &separationDistance, 0.5f, 4.0f);
        ImGui::SliderInt("Target Num Vertices", &targetNumVertices, 100, 207004);
        ImGui::End();

        // Change model detail based on target number of vertices
        changeModelDetail(ghostModel, targetNumVertices);

        // Render switch model
        for (int i = 0; i < numberOfSwitch; ++i) {
            glm::vec3 switchPosition = switchPos[i]; // Position de la switch
            shader.use();
            glm::mat4 switchModelMatrix = glm::translate(glm::mat4(1.0f), switchPosition) *
                                        glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
            shader.set("uModelMatrix", switchModelMatrix);
            glm::vec3 switchColor = glm::vec3(1.0f, 1.0f, 1.0f); // White color for switch
            shader.set("uColor", switchColor);
            glBindVertexArray(switchModel.vao);
            glDrawElements(GL_TRIANGLES, switchModel.numVertices, GL_UNSIGNED_INT, 0);
        }

        // Bind dome VAO
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

        // Render dome
        glm::mat4 domeModelMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(domeRadius));
        shader.set("uModelMatrix", domeModelMatrix);

        // Activer le mélange pour permettre la transparence
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // Définir la couleur du dome avec une composante alpha
        glm::vec4 domeColorWithAlpha = glm::vec4(0.0f, 0.0f, 1.0f, 0.5f); // Rouge avec une transparence de 0.5
        // Passer la couleur du dome avec alpha au shader
        shader.set("uDomeColor", domeColorWithAlpha);
        // Draw dome
        glDrawArrays(GL_TRIANGLES, 0, dome.getVertexCount());

        // Désactiver le mélange une fois que vous avez fini de rendre des objets transparents
        glDisable(GL_BLEND);

        // Unbind VAO
        glBindVertexArray(0);

        // Utilise l'indice de frame courant pour sélectionner la frame appropriée à afficher
        // Incrémente le temps écoulé à chaque itération de la boucle
        elapsedTime += deltaTime;

        // Durée totale de l'animation (en secondes)
        float animationDuration = 1.0f; // Par exemple, 2 secondes

        // Calcule l'indice de frame en utilisant le temps écoulé
        int currentFrameIndex = static_cast<int>(std::fmod(elapsedTime / animationDuration, numFrames));

        Model currentFrameModel = animationFrames[currentFrameIndex].model;
        
        // Envoie les matrices au shader
        // Render surveyor
        shader.use();
        shader.set("surveyorPosition", surveyor.position);
        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), surveyor.position) *
                            glm::rotate(glm::mat4(1.0f), glm::radians(100.0f), glm::vec3(0.0f, 1.0f, 0.0f)) *
                            glm::rotate(glm::mat4(1.0f), surveyorRotationAngleYRadians, glm::vec3(0.0f, 1.0f, 0.0f)) *
                            glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        shader.set("uModelMatrix", modelMatrix);
        glm::vec3 surveyorColor = glm::vec3(1.0f, 1.0f, 0.0f); // Yellow color for surveyor
        shader.set("uColor", surveyorColor);
        glBindVertexArray(currentFrameModel.vao);
        glDrawElements(GL_TRIANGLES, currentFrameModel.numVertices, GL_UNSIGNED_INT, 0);
        
        // Change model detail based on target number of vertices
        changeModelDetail(currentFrameModel, targetNumVertices); // Change detail level of surveyor model
        
        for (auto& boid : boids) {
            // Vérifier si c'est la nuit pour dessiner les fantômes
            if (!dayMode) {

                // Appliquer la rotation à la matrice du modèle
                glm::mat4 boidModelMatrix = glm::translate(glm::mat4(1.0f), boid.position) * glm::scale(glm::mat4(1.0f), glm::vec3(boidSize));
                
                shader.set("uModelMatrix", glm::mat4(1.0f));
                glm::vec3 boidColor = getBoidColor(boid.markovState, boid.isFemale);
                shader.set("uColor", boidColor);
                shader.set("boidPosition", boid.position);
                shader.set("uMVPMatrix", ProjMatrix * MVMatrix * boidModelMatrix);
                shader.set("uMVMatrix", MVMatrix * boidModelMatrix);
                shader.set("uNormalMatrix", glm::transpose(glm::inverse(MVMatrix * boidModelMatrix)));

                glBindVertexArray(ghostModel.vao);
                glDrawElements(GL_TRIANGLES, ghostModel.numVertices, GL_UNSIGNED_INT, 0);
            }
            // Mise à jour de l'état de la chaîne de Markov en fonction du nombre de voisins
            updateMarkovState(boid, boids, numBoids);         
            // Mise à jour de l'état de la chaîne de Markov
            if (elapsedTime > boid.markovTime) {
                // Générer un nouveau temps entre les changements d'état
                boid.markovTime += generateStateChangeTime(1);

                if (autoMode){
                    // Générer aléatoirement le nouvel état de l'interrupteur
                    bool newSwitchState = generateSwitchState(1);

                    // Mettre à jour l'état de l'interrupteur
                    dayMode = newSwitchState;
                }
            }
        }

        // Update number of boids
        if (numBoids > boids.size()) {
            for (int i = 0; i < numBoids; ++i){
                Boid boid;
                boid.position = glm::vec3(linearRand(-domeRadius, domeRadius),
                                      linearRand(-domeRadius, domeRadius),
                                      linearRand(-domeRadius, domeRadius));

                // Vitesse aléatoire des boids dans une certaine plage
                boid.velocity = customSphericalRand(speedBoids);
                
                // Définir aléatoirement si le boid est une femelle
                boid.isFemale = (rand() % 2 == 0);

                // État initial de la chaîne de Markov
                boid.markovState = 0;
                
                // Générer la durée de vie du boid
                boid.lifespan = generateExp(5);

                // Initialiser markovTime avec une valeur aléatoire entre 0 et la première transition
                boid.markovTime = generateStateChangeTime(1);

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

    // Libération des VAO et VBO après utilisation
    glDeleteVertexArrays(1, &ghostModel.vao);
    glDeleteBuffers(1, &ghostModel.vbo);
    glDeleteBuffers(1, &ghostModel.ebo);
    
    // Libération des VAO et VBO après utilisation
    glDeleteVertexArrays(1, &switchModel.vao);
    glDeleteBuffers(1, &switchModel.vbo);
    glDeleteBuffers(1, &switchModel.ebo);

    return EXIT_SUCCESS;
}
