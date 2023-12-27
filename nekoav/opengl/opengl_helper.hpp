#pragma once

#include "../defs.hpp"
#include "opengl_func.hpp"

NEKO_NS_BEGIN

constexpr inline const char *vertexCommonShader = R"(
#version 330 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texcoord;

out vec2 texturePos;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    texturePos = texcoord;
}
)";

inline GLuint CompileGLProgram(const GLFunctions_2_0 *gl, const char *vertexShaderCode, const char *fragShaderCode) {
    GLuint fragShader = gl->glCreateShader(GL_FRAGMENT_SHADER);
    GLuint vertexShader = gl->glCreateShader(GL_VERTEX_SHADER);

    gl->glShaderSource(fragShader, 1, &fragShaderCode, nullptr);
    gl->glCompileShader(fragShader);

    gl->glShaderSource(vertexShader, 1, &vertexShaderCode, nullptr);
    gl->glCompileShader(vertexShader);


    // Make Program
    GLuint program = gl->glCreateProgram();
    gl->glAttachShader(program, vertexShader);
    gl->glAttachShader(program, fragShader);
    gl->glLinkProgram(program);

    // Release shader
    gl->glDeleteShader(fragShader);
    gl->glDeleteShader(vertexShader);

    GLint status = 0;
    gl->glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
#ifndef NDEBUG
        GLint logLength;
        gl->glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 0) {
            char *log = (char *)::malloc(logLength);
            gl->glGetProgramInfoLog(program, logLength, nullptr, log);
            fprintf(stderr, "GL: Create Porgram failed\n %s\n", log);
            ::free(log);
        }
#endif
        gl->glDeleteProgram(program);
        return 0;
    }
    return program;
}

/**
 * @brief A Utils for Create a common VAO and buffer than just copy texture to dest
 * 
 */
inline void CreateGLComputeEnv(const GLFunctions_3_0 *gl, GLuint *vertexArray, GLuint *buffer) {
    gl->glGenBuffers(1, buffer);
    gl->glGenVertexArrays(1, vertexArray);

    GLfloat vertex [] = {
        //< vertex       //< texture position
        -1.0f, 1.0f,       0.0f, 1.0f,
        1.0f,  1.0f,       1.0f, 1.0f,
        1.0f,  -1.0f,      1.0f, 0.0f,
        -1.0f, -1.0f,      0.0f, 0.0f, 
        -1.0f, 1.0f,       0.0f, 1.0f,
    };
    // Config Buffer
    gl->glBindBuffer(GL_ARRAY_BUFFER, *buffer);
    gl->glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), vertex, GL_STATIC_DRAW);

    // Config VertexArray
    gl->glBindVertexArray(*vertexArray);
    gl->glEnableVertexAttribArray(0);
    gl->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    gl->glEnableVertexAttribArray(1);
    gl->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)(2 * sizeof(GLfloat)));
    gl->glBindVertexArray(0);
    gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Config done
}
inline void DeleteGLComputeEnv(const GLFunctions_3_0 *gl, GLuint *vertexArray, GLuint *buffer) {
    if (*vertexArray != 0) {
        gl->glDeleteVertexArrays(1, vertexArray);
        *vertexArray = 0;
    }
    if (*buffer != 0) {
        gl->glDeleteBuffers(1, buffer);
        *buffer = 0;
    }
}
inline void DispatchGLCompute(const GLFunctions_2_0 *gl) {
    gl->glDrawArrays(GL_TRIANGLES, 0, 3);
    gl->glDrawArrays(GL_TRIANGLES, 2, 3);
}

NEKO_NS_END