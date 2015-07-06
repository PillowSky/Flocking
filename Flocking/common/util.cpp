#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <fstream>
#include <boost/format.hpp>
#include "util.hpp"

using namespace std;
using namespace boost;

const char* readFileBytes(const char* path) {
	ifstream sourceFile(path);
	string* sourceCode = new string(istreambuf_iterator<char>(sourceFile), (istreambuf_iterator<char>()));
	return sourceCode->c_str();
	/*FILE* fp = fopen(path, "r");
	if (!fp) {
	throw runtime_error((format("File not found: %1%") % path).str());
	}

	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	rewind(fp);

	char* buffer = new char[size + 1];
	fread(buffer, size, 1, fp);
	fclose(fp);
	buffer[size - 1] = '\0';
	buffer[size] = '\0';

	return buffer;*/
}

GLuint buildShader(const char* shaderCode, GLenum shaderType) {
	GLuint shader = glCreateShader(shaderType);
	glShaderSource(shader, 1, &shaderCode, NULL);
	glCompileShader(shader);

	GLint result;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
	if (result != GL_TRUE) {
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &result);
		char* info = new char[result];
		glGetShaderInfoLog(shader, result, NULL, info);
		throw runtime_error((format("Build shader failed: %1%") % info).str());
	}

	return shader;
}

GLuint buildProgram(const char** shaderCodeArray, GLenum* shaderTypeArray, GLuint length) {
	GLuint program = glCreateProgram();
	for (GLuint i = 0; i < length; i++) {
		glAttachShader(program, buildShader(shaderCodeArray[i], shaderTypeArray[i]));
	}
	glLinkProgram(program);

	GLint result;
	glGetProgramiv(program, GL_LINK_STATUS, &result);
	if (result != GL_TRUE) {
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &result);
		char* info = new char[result];
		glGetProgramInfoLog(program, result, NULL, info);
		throw runtime_error((format("Build program failed: %1%") % info).str());
	}

	return program;
}

void checkError(const char* desc) {
	GLenum error = glGetError();
	if (error != GL_NO_ERROR) {
		cerr << format("OpenGL error in %1%: %2% (%3%)") % desc % gluErrorString(error) % error << endl;
	}
}
