#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include "glm/glm.hpp"

class Shader {
public:
    // Le programme d'identifiant
    unsigned int ID;

    // Constructeur
    Shader(const char* vertexPath, const char* fragmentPath) {
        // 1. Lire les shaders depuis les fichiers
        std::string vertexCode;
        std::string fragmentCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;

        // Assurez-vous que les objets ifstream peuvent lancer des exceptions
        vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            // Ouvrez les fichiers
            vShaderFile.open(vertexPath);
            fShaderFile.open(fragmentPath);
            std::stringstream vShaderStream, fShaderStream;

            // Lisez le contenu des fichiers dans les flux
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();

            // Fermez les fichiers
            vShaderFile.close();
            fShaderFile.close();

            // Convertissez les flux en chaînes
            vertexCode = vShaderStream.str();
            fragmentCode = fShaderStream.str();
        }
        catch (std::ifstream::failure e) {
            std::cout << "Erreur : Impossible de lire le shader" << std::endl;
        }

        const char* vShaderCode = vertexCode.c_str();
        const char* fShaderCode = fragmentCode.c_str();

        // 2. Compilation des shaders
        unsigned int vertex, fragment;
        int success;
        char infoLog[512];

        // Vertex Shader
        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        checkCompileErrors(vertex, "VERTEX");

        // Fragment Shader
        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        checkCompileErrors(fragment, "FRAGMENT");

        // Programme de shader
        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        checkCompileErrors(ID, "PROGRAM");

        // Supprimez les shaders car ils sont liés au programme de shader maintenant
        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    // Utiliser le shader
    void use() {
        glUseProgram(ID);
    }

    // Définir les uniformes du shader
    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
    }

private:
    // Vérifier les erreurs de compilation de shader / de programme
    void checkCompileErrors(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        if (type != "PROGRAM") {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "Erreur de compilation de shader de type : " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
        else {
            glGetProgramiv(shader, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(shader, 1024, NULL, infoLog);
                std::cout << "Erreur de liaison de programme de type : " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
            }
        }
    }
};
