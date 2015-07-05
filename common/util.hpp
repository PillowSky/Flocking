#ifndef UTIL_HPP
#define UTIL_HPP

#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/glx.h>
#include <GL/glext.h>

const char* readFileBytes(const char* path);
GLuint buildShader(const char* shaderCode, GLenum shaderType);
GLuint buildProgram(const char** shaderCodeArray, GLenum* shaderTypeArray, GLuint length);

void checkError(const char* desc);

#endif
