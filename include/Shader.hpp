#ifndef SHADER_HPP
#define SHADER_HPP

#include <glad.h>
#include <string>

std::string loadShaderSource(const char* filepath);
GLuint compileShader(GLenum type, const char* filepath);
GLuint createShaderProgram(const char* vertPath, const char* fragPath);

#endif // SHADER_HPP