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

typedef enum {
	inActive,
	rotate,
	translate,
	zoom
} MouseStatus;

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

mat4 projection = perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
vec3 cameraPosition = vec3(10, 3, 3);
vec3 cameraFocus = vec3(0, 0, 0);
vec3 cameraHeadup = vec3(0, 1, 0);
mat4 view = lookAt(cameraPosition, cameraFocus, cameraHeadup);
mat4 model = mat4(1.0f);
mat4 mvp = projection * view * model;

GLuint frameBuffer, positionVertexBuffer, colorVertexBuffer;
GLuint computeProgram, displayProgram;
GLuint positionTex, velocityTex, colorTex;

MouseStatus mouseStatus;
vec2 mousePosition;
vec3 lastCameraPosition;

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

	// vertex used for draw
	GLuint vertexArray;
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);
	const static GLfloat vertexData[] = {
		-1.0f, -1.0f,
		-1.0f,  1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f
	};

	GLuint vertexBuffer;
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
	glEnableVertexAttribArray(3);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// variable used in draw loop
	double lastTime = glfwGetTime();
	double nowTime;
	char fpsTitle[32];
	GLenum drawBuffer[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };

	// draw loop
	do {
		// compute
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
		glViewport(0, 0, texSize, texSize);
		glDrawBuffers(3, drawBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(3);
		glUseProgram(computeProgram);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, sizeof(vertexData));
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// retrive
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, positionTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, velocityTex);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, colorTex);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, positionVertexBuffer);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glReadPixels(0, 0, texSize, texSize, GL_RGBA, GL_FLOAT, NULL);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, colorVertexBuffer);
		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glReadPixels(0, 0, texSize, texSize, GL_RGBA, GL_FLOAT, NULL);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		// display
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, windowWidth, windowHeight);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

		glDisable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glEnable(GL_BLEND);

		glBindBuffer(GL_ARRAY_BUFFER, positionVertexBuffer);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, colorVertexBuffer);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
		glEnableVertexAttribArray(2);

		glUseProgram(displayProgram);
		glDrawArrays(GL_POINTS, 0, numParticles);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);

		// other update stuff
		glfwSwapBuffers(window);
		glfwPollEvents();

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
	glGenTextures(1, &colorTex);
	glGenFramebuffers(1, &frameBuffer);
	glGenBuffers(1, &positionVertexBuffer);
	glGenBuffers(1, &colorVertexBuffer);
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

	// init color data
	for (int i = 0; i < numParticles; ++i) {
		texData[i * 4 + 0] = (float(rand()) / RAND_MAX) * 0.1;
		texData[i * 4 + 1] = (float(rand()) / RAND_MAX) * 0.1;
		texData[i * 4 + 2] = (float(rand()) / RAND_MAX) * 0.1 + 0.5;
		texData[i * 4 + 3] = 0.0;
	}
	glGenTextures(1, &colorTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
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
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// color vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, colorVertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, numParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_COPY);
	glEnableVertexAttribArray(2);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void initComputation() {
	const char* computeSrc[] = { readFileBytes("shader/compute.vert"), readFileBytes("shader/compute.frag") };
	GLenum computeType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	computeProgram = buildProgram(computeSrc, computeType, 2);
	
	glUseProgram(computeProgram);
	glUniform1f(glGetUniformLocation(computeProgram, "timeStep"), timeStep);
	glUniform1i(glGetUniformLocation(computeProgram, "texSixe"), texSize);
	glUniform1f(glGetUniformLocation(computeProgram, "cohesion"), cohesion);
	glUniform1f(glGetUniformLocation(computeProgram, "alignment"), alignment);
	glUniform1f(glGetUniformLocation(computeProgram, "neighborRadius"), neighborRadius);
	glUniform1f(glGetUniformLocation(computeProgram, "collisionRadius"), collisionRadius);
	glUseProgram(0);

	delete[] computeSrc[0];
	delete[] computeSrc[1];
}

void setupComputation() {
	glUseProgram(computeProgram);
	// fill in
	glUseProgram(0);
}

void initDisplay() {
	const char* displaySrc[] = { readFileBytes("shader/display.vert"), readFileBytes("shader/display.frag") };
	GLenum displayType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	displayProgram = buildProgram(displaySrc, displayType, 2);

	delete[] displaySrc[0];
	delete[] displaySrc[1];
}

void setupDisplay() {
	glUseProgram(displayProgram);
	view = lookAt(cameraPosition, cameraFocus, cameraHeadup);
	mvp = projection * view * model;
	glUniformMatrix4fv(glGetUniformLocation(displayProgram, "mvp"), 1, GL_FALSE, &mvp[0][0]);
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
			case GLFW_KEY_UP: {
				cameraPosition += vec3(0, 1, 0);
				cameraFocus += vec3(0, 1, 0);
				setupComputation();
				break;
			}
			case GLFW_KEY_DOWN: {
				cameraPosition += vec3(0, -1, 0);
				cameraFocus += vec3(0, -1, 0);
				setupComputation();
				break;
			}
			case GLFW_KEY_LEFT: {
				cameraPosition += vec3(-1, 0, 0);
				cameraFocus += vec3(-1, 0, 0);
				setupComputation();
				break;
			}
			case GLFW_KEY_RIGHT: {
				cameraPosition += vec3(1, 0, 0);
				cameraFocus += vec3(1, 0, 0);
				setupComputation();
				break;
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
		mousePosition = vec2(xpos, ypos);
		lastCameraPosition = cameraPosition;

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
			break;
		}
		case MouseStatus::translate: {
			break;
		}
		case MouseStatus::zoom:{
			cout << format("cameraosition: (%1%, %2%, %3%), lastCameraPosition: (%4%, %5%, %6%)") % cameraPosition.x % cameraPosition.y % cameraPosition.z % lastCameraPosition.x % lastCameraPosition.y % lastCameraPosition.z << endl;
			cameraPosition = lastCameraPosition + normalize(cameraFocus - cameraPosition) * length(vec2(xpos, ypos) - mousePosition);
			setupComputation();
			break;
		}
	}
}

void onScroll(GLFWwindow* window, double xoffset, double yoffset) {
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
