// Vertex shader
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

void main() {
    frag_Normal = mat3(uNormalMatrix) * in_Normal;
    frag_Position = vec3(uMVMatrix * vec4(in_Position, 1.0));
    frag_TexCoord = in_TexCoord;
    gl_Position = uMVPMatrix * vec4(in_Position, 1.0);
}
