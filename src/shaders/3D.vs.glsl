#version 330 core

layout(location = 0) in vec3 in_Position;
layout(location = 1) in vec3 in_Normal;
layout(location = 2) in vec2 in_TexCoord;

out vec3 frag_Normal;
out vec3 frag_Position;
out vec2 frag_TexCoord;

uniform mat4 uMVPMatrix;
uniform mat4 uMVMatrix;
uniform mat4 uNormalMatrix;
uniform mat4 uModelMatrix; // New uniform for model matrix

void main() {
    // Compute transformed normal
    frag_Normal = mat3(uNormalMatrix) * in_Normal;

    // Compute transformed position
    frag_Position = vec3(uMVMatrix * vec4(in_Position, 1.0));

    // Pass texture coordinates to fragment shader
    frag_TexCoord = in_TexCoord;

    // Compute final vertex position in clip space
    gl_Position = uMVPMatrix * uModelMatrix * vec4(in_Position, 1.0);
}
