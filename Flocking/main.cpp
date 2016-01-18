/*
* Flocking
*
* Flocking animation with avoidance(separation), cohesion, and alignment on a local scale.
* Completely implemented with glsl.
*
* 2015 Yuzo(Huang Yuzhong)
*
*/

#pragma comment(lib, "Opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "glfw3.lib")
#pragma comment(lib, "glew32.lib")
#pragma comment(lib, "SOIL.lib")

#include <iostream>
#include <random>
#include <boost/format.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <SOIL.h>
#include "common/util.hpp"

using namespace std;
using namespace boost;
using namespace glm;

typedef enum {
	inActive,
	rotate,
	translate,
	zoom
} MouseStatus;

int windowWidth = 800;
int windowHeight = 600;
int texSize = 80;
int numParticles = texSize * texSize;

float pointSize = 2.0f;
float cohesion = 1000.0f;
float alignment = 80.0f;
float neighborRadius = 1.0f;
float collisionRadius = 0.1f;
float timeStep = 0.01f;

GLuint computeProgram, displayProgram, skyboxProgram;
GLuint positionTex, velocityTex, colorTex, skyboxTex;
GLuint frameBuffer, positionVertexBuffer, colorVertexBuffer;

mat4 projection = perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
mat4 view = lookAt(vec3(0, 0, 5), vec3(0.0, 0.0, 0.0), vec3(0, 1, 0));
mat4 model = mat4(1.0f);
mat4 mvp = projection * view * model;
mat4 mvpsky = projection * view * model;

float rotateSpeed = 0.01f;
float translateSpeed = 0.01f;
float zoomSpeed = 0.01f;

MouseStatus mouseStatus;
vec2 lastMousePosition;
mat4 lastMVP;
mat4 lastMVPSky;

void initTexture();
void initComputation();
void initDisplay();
void initSkybox();
void setupTexture();
void setupSkybox();
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
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Flocking", NULL, NULL);
	if (!window) {
		throw runtime_error("Failed to open GLFW window");
	}

	glfwMakeContextCurrent(window);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GL_TRUE);
	glfwSetKeyCallback(window, onKey);
	glfwSetMouseButtonCallback(window, onMouseButton);
	glfwSetCursorPosCallback(window, onCursorPos);
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
	initSkybox();
	initComputation();
	initDisplay();

	// setup texture, computation, display
	setupTexture();
	setupSkybox();
	setupComputation();
	setupDisplay();

	// vertex used for draw
	const static GLfloat vertexData[] = {
		-1.0f, -1.0f,
		-1.0f,  1.0f,
		1.0f, -1.0f,
		1.0f,  1.0f
	};

	GLuint vertexArray, vertexBuffer;
	glGenVertexArrays(1, &vertexArray);
	glGenBuffers(1, &vertexBuffer);
	glBindVertexArray(vertexArray);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// vertex used for skybox
	GLfloat skyboxVertices[] = {
		-5.0f,  5.0f, -5.0f,
		-5.0f, -5.0f, -5.0f,
		5.0f, -5.0f, -5.0f,
		5.0f, -5.0f, -5.0f,
		5.0f,  5.0f, -5.0f,
		-5.0f,  5.0f, -5.0f,

		-5.0f, -5.0f,  5.0f,
		-5.0f, -5.0f, -5.0f,
		-5.0f,  5.0f, -5.0f,
		-5.0f,  5.0f, -5.0f,
		-5.0f,  5.0f,  5.0f,
		-5.0f, -5.0f,  5.0f,

		5.0f, -5.0f, -5.0f,
		5.0f, -5.0f,  5.0f,
		5.0f,  5.0f,  5.0f,
		5.0f,  5.0f,  5.0f,
		5.0f,  5.0f, -5.0f,
		5.0f, -5.0f, -5.0f,

		-5.0f, -5.0f,  5.0f,
		-5.0f,  5.0f,  5.0f,
		5.0f,  5.0f,  5.0f,
		5.0f,  5.0f,  5.0f,
		5.0f, -5.0f,  5.0f,
		-5.0f, -5.0f,  5.0f,

		-5.0f,  5.0f, -5.0f,
		5.0f,  5.0f, -5.0f,
		5.0f,  5.0f,  5.0f,
		5.0f,  5.0f,  5.0f,
		-5.0f,  5.0f,  5.0f,
		-5.0f,  5.0f, -5.0f,

		-5.0f, -5.0f, -5.0f,
		-5.0f, -5.0f,  5.0f,
		5.0f, -5.0f, -5.0f,
		5.0f, -5.0f, -5.0f,
		-5.0f, -5.0f,  5.0f,
		5.0f, -5.0f,  5.0f
	};

	GLuint skyboxVAO, skyboxVBO;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// variable used in draw loop
	int count = 0;
	double lastTime = glfwGetTime();
	double nowTime;
	char fpsTitle[32];
	GLenum drawBuffer[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };

	// status used in loop
	glPointSize(pointSize);
	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_COLOR, GL_DST_COLOR);
	glDepthFunc(GL_LESS);

	// draw loop
	do {
		// compute
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
		glViewport(0, 0, texSize, texSize);
		glUseProgram(computeProgram);
		glBindVertexArray(vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glDrawBuffers(3, drawBuffer);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, sizeof(vertexData));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// retrive
		glBindBuffer(GL_PIXEL_PACK_BUFFER, positionVertexBuffer);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, texSize, texSize, GL_RGBA, GL_FLOAT, NULL);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, colorVertexBuffer);
		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glReadPixels(0, 0, texSize, texSize, GL_RGBA, GL_FLOAT, NULL);

		// draw to screen
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDrawBuffer(GL_BACK);
		glViewport(0, 0, windowWidth, windowHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// skybox
		glDepthMask(GL_FALSE);
		glUseProgram(skyboxProgram);
		glBindVertexArray(skyboxVAO);
		glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
		glDepthMask(GL_TRUE);

		// display
		glUseProgram(displayProgram);
		glBindVertexArray(vertexArray);
		glBindBuffer(GL_ARRAY_BUFFER, positionVertexBuffer);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(0);

		glBindBuffer(GL_ARRAY_BUFFER, colorVertexBuffer);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(2);

		glEnable(GL_BLEND);
		glDrawArrays(GL_POINTS, 0, numParticles);
		glDisable(GL_BLEND);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		// window event
		glfwSwapBuffers(window);
		glfwPollEvents();

		// update fps
		if (++count >= 60) {
			nowTime = glfwGetTime();
			snprintf(fpsTitle, 32, "Flocking - FPS: %.2f", count / (nowTime - lastTime));
			glfwSetWindowTitle(window, fpsTitle);
			lastTime = nowTime;
			count = 0;
		}
	} while (!glfwWindowShouldClose(window));

	return EXIT_SUCCESS;
}

void initTexture() {
	glGenTextures(1, &positionTex);
	glGenTextures(1, &velocityTex);
	glGenTextures(1, &colorTex);
	glGenFramebuffers(1, &frameBuffer);
	glGenBuffers(1, &positionVertexBuffer);
	glGenBuffers(1, &colorVertexBuffer);

	checkError("initTexture");
}
void initSkybox() {
	const char* skyboxSrc[] = { readFileBytes("shader/skybox.vert"), readFileBytes("shader/skybox.frag") };
	GLenum skyboxType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	skyboxProgram = buildProgram(skyboxSrc, skyboxType, 2);
	delete[] skyboxSrc[0];
	delete[] skyboxSrc[1];
	checkError("initSkybox");
}

void initComputation() {
	const char* computeSrc[] = { readFileBytes("shader/compute.vert"), readFileBytes("shader/compute.frag") };
	GLenum computeType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	computeProgram = buildProgram(computeSrc, computeType, 2);

	delete[] computeSrc[0];
	delete[] computeSrc[1];
	checkError("initComputation");
}

void initDisplay() {
	const char* displaySrc[] = { readFileBytes("shader/display.vert"), readFileBytes("shader/display.frag") };
	GLenum displayType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	displayProgram = buildProgram(displaySrc, displayType, 2);

	delete[] displaySrc[0];
	delete[] displaySrc[1];
	checkError("initDisplay");
}

void setupTexture() {
	GLfloat* texData = new GLfloat[4 * numParticles];

	// init position data
	for (int i = 0; i < numParticles; i++) {
		texData[4 * i + 0] = float(rand()) / RAND_MAX - 0.5f;
		texData[4 * i + 1] = float(rand()) / RAND_MAX - 0.5f;
		texData[4 * i + 2] = float(rand()) / RAND_MAX - 0.5f;
		texData[4 * i + 3] = 1.0f;
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, positionTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texSize, texSize, 0, GL_RGBA, GL_FLOAT, texData);

	// init velocity data
	default_random_engine generator(rand());
	normal_distribution<float> distribution(-2.0f, 2.0f);
	for (int i = 0; i < numParticles; ++i) {
		texData[i * 4 + 0] = distribution(generator);
		texData[i * 4 + 1] = distribution(generator);
		texData[i * 4 + 2] = distribution(generator);
		texData[i * 4 + 3] = 0.0f;
	}
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, velocityTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texSize, texSize, 0, GL_RGBA, GL_FLOAT, texData);

	// init color data
	for (int i = 0; i < numParticles; ++i) {
		texData[i * 4 + 0] = (float(rand()) / RAND_MAX) * 0.25f + 0.75f;
		texData[i * 4 + 1] = (float(rand()) / RAND_MAX) * 0.5f + 0.25f;
		texData[i * 4 + 2] = (float(rand()) / RAND_MAX) * 0.25f;
		texData[i * 4 + 3] = 1.0f;
	}
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, texSize, texSize, 0, GL_RGBA, GL_FLOAT, texData);

	delete[] texData;

	// init frame buffer
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
	glBindTexture(GL_TEXTURE_2D, positionTex);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, positionTex, 0);

	glBindTexture(GL_TEXTURE_2D, velocityTex);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, velocityTex, 0);

	glBindTexture(GL_TEXTURE_2D, colorTex);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, colorTex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		cerr << "ERROR - Incomplete FrameBuffer" << endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// position vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, positionVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, numParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// color vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, colorVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, numParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	checkError("setupTexture");
}

void setupSkybox() {
	char* faces[] = { "cubemap/right.jpg" , "cubemap/left.jpg" , "cubemap/top.jpg", "cubemap/bottom.jpg", "cubemap/back.jpg", "cubemap/front.jpg" };

	glGenTextures(1, &skyboxTex);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTex);

	int width, height;
	unsigned char* image = NULL;

	for (int i = 0; i < 6; i++) {
		image = SOIL_load_image(faces[i], &width, &height, 0, SOIL_LOAD_RGB);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		SOIL_free_image_data(image);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

	glUseProgram(skyboxProgram);
	glUniform1i(glGetUniformLocation(skyboxProgram, "skybox"), 3);
	glUseProgram(0);

	checkError("setupSkybox");
}

void setupComputation() {
	glUseProgram(computeProgram);
	glUniform1i(glGetUniformLocation(computeProgram, "positionTex"), 0);
	glUniform1i(glGetUniformLocation(computeProgram, "velocityTex"), 1);
	glUniform1i(glGetUniformLocation(computeProgram, "colorTex"), 2);
	glUniform1f(glGetUniformLocation(computeProgram, "timeStep"), timeStep);
	glUniform1i(glGetUniformLocation(computeProgram, "texSixe"), texSize);
	glUniform1f(glGetUniformLocation(computeProgram, "cohesion"), cohesion);
	glUniform1f(glGetUniformLocation(computeProgram, "alignment"), alignment);
	glUniform1f(glGetUniformLocation(computeProgram, "neighborRadius"), neighborRadius);
	glUniform1f(glGetUniformLocation(computeProgram, "collisionRadius"), collisionRadius);
	glUseProgram(0);

	checkError("setupComputation");
}

void setupDisplay() {
	glUseProgram(displayProgram);
	glUniformMatrix4fv(glGetUniformLocation(displayProgram, "mvp"), 1, GL_FALSE, value_ptr(glm::translate(mvp, vec3(0, 1.5f, 0))));
	glUseProgram(0);

	glUseProgram(skyboxProgram);
	glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "mvp"), 1, GL_FALSE, value_ptr(mvpsky));
	glUseProgram(0);

	checkError("setupDisplay");
}

void onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		switch (key) {
			case 'r': case 'R': {
				setupTexture();
				break;
			}
			case 'f': case 'F': {
				mvp = projection * view * model;
				mvpsky = projection * view * model;
				setupDisplay();
				break;
			}
			case 'q': case 'Q':	case GLFW_KEY_ESCAPE: {
				exit(EXIT_SUCCESS);
			}
		}
	}
}

void onMouseButton(GLFWwindow* window, int button, int action, int mods) {
	if (action == GLFW_RELEASE) {
		mouseStatus = MouseStatus::inActive;
	} else {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		lastMousePosition = vec2(xpos, ypos);
		lastMVP = mvp;
		lastMVPSky = mvpsky;

		switch (button) {
			case GLFW_MOUSE_BUTTON_LEFT: {
				mouseStatus = MouseStatus::rotate;
				break;
			}
			case GLFW_MOUSE_BUTTON_MIDDLE: {
				mouseStatus = MouseStatus::translate;
				break;
			}
			case GLFW_MOUSE_BUTTON_RIGHT: {
				mouseStatus = MouseStatus::zoom;
				break;
			}
		}
	}
}

void onCursorPos(GLFWwindow* window, double xpos, double ypos) {
	switch (mouseStatus) {
		case MouseStatus::rotate: {
			vec2 move = (vec2(xpos, ypos) - lastMousePosition) * rotateSpeed;
			mvp = glm::rotate(lastMVP, length(move), vec3(move.y, move.x, 0));
			mvpsky = glm::rotate(lastMVPSky, length(move), vec3(move.y, move.x, 0));
			break;
		}
		case MouseStatus::translate: {
			vec2 move = vec2(xpos - lastMousePosition.x, lastMousePosition.y - ypos) * translateSpeed;
			mvp = glm::translate(lastMVP, vec3(move, 0));
			break;
		}
		case MouseStatus::zoom: {
			vec2 move = (vec2(xpos, ypos) - lastMousePosition) * zoomSpeed;
			float len = length(move) + 1.0f;
			if ((move.x + move.y) < 0) {
				len = 1.0f / len;
			}
			mvp = glm::scale(lastMVP, vec3(len));
			break;
		}
	}
	setupDisplay();
}

void onScroll(GLFWwindow* window, double xoffset, double yoffset) {
	if (yoffset > 0) {
		mvp = glm::scale(mvp, vec3(1.1f));
	} else {
		mvp = glm::scale(mvp, vec3(0.9f));
	}
	setupDisplay();
}

void onResize(GLFWwindow* window, int width, int height) {
	windowWidth = width;
	windowHeight = height;
	setupDisplay();
}
