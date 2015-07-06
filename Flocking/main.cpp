/*
* Flocking
*
* Flocking animation with avoidance(separation), cohesion, and alignment on a local scale.
* Completely implemented with glsl.
*
* 2015 Yuzo(Huang Yuzhong)
*
*/

#ifdef _WIN32
	#define snprintf sprintf_s
#endif

#include <iostream>
#include <random>
#include <boost/format.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "common/util.hpp"

using namespace std;
using namespace boost;
using namespace glm;

int windowWidth = 1024;
int windowHeight = 768;
int texSize = 64;
int numParticles = texSize * texSize;

bool showGrid = false;
float pointSize = 2.0;
float cohesion = 1000.0;
float alignment = 80.0;
float neighborRadius = 1.0;
float collisionRadius = 0.1;
float timeStep = 0.01;

mat4 model = mat4(0.1f);
mat4 view = lookAt(vec3(4, 3, 3), vec3(-8, -8, 0), vec3(-4, 8, 0));
mat4 projection = perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
mat4 mvp = projection * view * model;

GLuint computeProgram, displayProgram;
GLuint velocityTex, colorTex, positionTex, displayTex;

void initTexture();
void initComputation();
void initDisplay();
void setupTexture();
void setupComputation();
void setupDisplay();

void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
void onMouseButton(GLFWwindow* window, int button, int action, int mods);
void onCursorPos(GLFWwindow* window, double xpos, double ypos);
void onScroll(GLFWwindow* window, double xoffset, double yoffset);
void onResize(GLFWwindow* window, int width, int height);

int main(int argc, char* argv[]) {
	if (!glfwInit()) {
		throw runtime_error("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Flocking", NULL, NULL);
	if (!window) {
		throw runtime_error("Failed to open GLFW window");
	}

	glfwMakeContextCurrent(window);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GL_TRUE);
	glfwSetKeyCallback(window, onKey);
	//glfwSetMouseButtonCallback(window, onMouseButton);
	//glfwSetCursorPosCallback(window, onCursorPos);
	glfwSetScrollCallback(window, onScroll);
	glfwSetWindowSizeCallback(window, onResize);
	checkError("glfw init");

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		throw runtime_error("Failed to initialize GLEW");
	}
	checkError("glew init");

	// init texture, computation, display
	initTexture();
	checkError("initTexture");
	initComputation();
	checkError("initComputation");
	initDisplay();
	checkError("initDisplay");

	// setup texture, computation, display
	setupTexture();
	checkError("setupTexture");
	setupComputation();
	checkError("setupComputation");
	setupDisplay();
	checkError("setupDisplay");

	// draw loop
	double lastTime = glfwGetTime();
	double nowTime;
	char fpsTitle[32];
	do {
		glClearTexImage(displayTex, 0, GL_RGBA, GL_FLOAT, NULL);

		glUseProgram(computeProgram);
		glDispatchCompute(texSize / 16, texSize / 16, 1);

		glUseProgram(displayProgram);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, sizeof(GLfloat) * 8);

		glfwSwapBuffers(window);
		glfwPollEvents();

		// other stuff
		nowTime = glfwGetTime();
		snprintf(fpsTitle, 32, "Flocking - FPS: %.2f", 1 / (nowTime - lastTime));
		glfwSetWindowTitle(window, fpsTitle);
		lastTime = nowTime;
	} while (!glfwWindowShouldClose(window));

	return EXIT_SUCCESS;
}

void initTexture() {
	glGenTextures(1, &positionTex);
	glGenTextures(1, &velocityTex);
	glGenTextures(1, &displayTex);
}

void setupTexture() {
	GLfloat* texData = new GLfloat[4 * numParticles];

	// init position data
	for (int i = 0; i < numParticles; i++) {
		texData[4 * i + 0] = (float(rand()) / RAND_MAX) * 2.0 - 1.0;
		texData[4 * i + 1] = (float(rand()) / RAND_MAX) * 2.0 - 1.0;
		texData[4 * i + 2] = (float(rand()) / RAND_MAX) * 2.0 - 1.0;
		texData[4 * i + 3] = 1.0;
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, positionTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texSize, texSize, 0, GL_RGBA, GL_FLOAT, texData);
	glBindImageTexture(0, positionTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	// init velocity data
	default_random_engine generator(rand());
	normal_distribution<double> distribution(0.0, 2.8);
	for (int i = 0; i < numParticles; ++i) {
		texData[i * 4 + 0] = distribution(generator);
		texData[i * 4 + 1] = distribution(generator);
		texData[i * 4 + 2] = distribution(generator);
		texData[i * 4 + 3] = 0.0;
	}
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, velocityTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texSize, texSize, 0, GL_RGBA, GL_FLOAT, texData);
	glBindImageTexture(1, velocityTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	// init color data
	for (int i = 0; i < numParticles; ++i) {
		texData[i * 4 + 0] = (float(rand()) / RAND_MAX) * 0.1;
		texData[i * 4 + 1] = (float(rand()) / RAND_MAX) * 0.1;
		texData[i * 4 + 2] = (float(rand()) / RAND_MAX) * 0.1 + 0.5;
		texData[i * 4 + 3] = 0.0; // Initially invisible
	}
	glGenTextures(1, &colorTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texSize, texSize, 0, GL_RGBA, GL_FLOAT, texData);
	glBindImageTexture(2, colorTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	// init display data
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, displayTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, windowWidth, windowHeight, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindImageTexture(3, displayTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	delete[] texData;
}

void initComputation() {
	const char* computeSrc = readFileBytes("shader/flocking.comp");
	GLenum computeType[] = { GL_COMPUTE_SHADER };
	computeProgram = buildProgram(&computeSrc, computeType, 1);
	
	glUseProgram(computeProgram);
	glUniform1f(glGetUniformLocation(computeProgram, "timeStep"), timeStep);
	glUniform1i(glGetUniformLocation(computeProgram, "texSixe"), texSize);
	glUniform1f(glGetUniformLocation(computeProgram, "cohesion"), cohesion);
	glUniform1f(glGetUniformLocation(computeProgram, "alignment"), alignment);
	glUniform1f(glGetUniformLocation(computeProgram, "neighborRadius"), neighborRadius);
	glUniform1f(glGetUniformLocation(computeProgram, "collisionRadius"), collisionRadius);
	glUseProgram(0);

	delete[] computeSrc;
}

void setupComputation() {
	glUseProgram(computeProgram);
	mvp = projection * view * model;
	glUniformMatrix4fv(glGetUniformLocation(computeProgram, "mvp"), 1, GL_FALSE, &mvp[0][0]);
	glUseProgram(0);
}

void initDisplay() {
	const char* displaySrc[] = { readFileBytes("shader/display.vert"), readFileBytes("shader/display.frag") };
	GLenum displayType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	displayProgram = buildProgram(displaySrc, displayType, 2);

	GLuint vertexArray;
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);
	const static GLfloat vertexData[] = {
		-1.0f, -1.0f,
		-1.0f, 1.0f,
		1.0f, -1.0f,
		1.0f, 1.0f
	};

	GLuint vertexBuffer;
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);

	glUseProgram(0);

	delete[] displaySrc[0];
	delete[] displaySrc[1];
}

void setupDisplay() {
	glUseProgram(displayProgram);
	glUniform2i(glGetUniformLocation(displayProgram, "size"), windowWidth, windowHeight);
	glUseProgram(0);
}

void onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		switch (key) {
			case 'c': case 'C': {
				setupComputation();
				setupDisplay();
				break;
			}
			case 'r': case 'R': {
				setupTexture();
				break;
			}
			case 'f': case 'F': {
				//camera->SetCenterOfFocus(Vector3d(0, 10, 0));
				break;
			} case 'g': case 'G': {
				showGrid = !showGrid;
				break;
			}
			case 'q': case 'Q':	case GLFW_KEY_ESCAPE: {
				exit(EXIT_SUCCESS);
			}
		}
	}
}

void onMouseButton(GLFWwindow* window, int button, int action, int mods) {
	// let the camera handle some specific mouse events (similar to maya)
	//camera->HandleMouseEvent(window, button, action, mods);
}

void onCursorPos(GLFWwindow* window, double xpos, double ypos) {
	// let the camera handle some mouse motions if the camera is to be moved
	//camera->HandleMouseMotion(window, xpos, ypos);
}

void onScroll(GLFWwindow* window, double xoffset, double yoffset) {
	cout << format("X: %1%, Y: %2%") % xoffset % yoffset << endl;
	if (yoffset > 0) {
		model *= 1.5;
	} else {
		model /= 1.5;
	}
	setupComputation();
}

void onResize(GLFWwindow* window, int width, int height) {
	windowWidth = width;
	windowHeight = height;
	setupDisplay();
}
