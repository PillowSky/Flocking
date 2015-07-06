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
#include "common/camera.h"
#include "common/util.hpp"

using namespace std;
using namespace boost;
using namespace glm;

int WIDTH = 1024;
int HEIGHT = 768;
int texSize = 64;

float pointSize = 2.0;
float cohesion = 1000.0;
float alignment = 80.0;
float neighborRadius = 1.0;
float collisionRadius = 0.1;
float timeStep = 0.01;
bool showGrid = false;

Camera* camera;
GLuint computeProgram, displayProgram;
GLuint vbo, cbo, fbo;
GLuint arrayWidth, arrayHeight;
GLuint velocityTex, colorTex, positionTex, displayTex;


void makeGrid();
void setupTextures();
void PerspDisplay();
void computationPass();
void onKey(GLFWwindow* window, int key, int scancode, int action, int mods);
void onMouseButton(GLFWwindow* window, int button, int action, int mods);
void onCursorPos(GLFWwindow* window, double xpos, double ypos);
void onResize(GLFWwindow* window, int width, int height);

int main(int argc, char* argv[]) {
	if (!glfwInit()) {
		throw runtime_error("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Flocking", NULL, NULL);
	if (!window) {
		throw runtime_error("Failed to open GLFW window");
	}

	glfwMakeContextCurrent(window);
	/*glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GL_TRUE);
	glfwSetKeyCallback(window, onKey);
	glfwSetMouseButtonCallback(window, onMouseButton);
	glfwSetCursorPosCallback(window, onCursorPos);
	glfwSetWindowSizeCallback(window, onResize);*/
	checkError("glfw init");

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		throw runtime_error("Failed to initialize GLEW");
	}
	checkError("glew init");
	// set up camera
	// parameters are eye point, aim point, up vector
	/*camera = new Camera(Vector3d(0, 12, 30), Vector3d(0, 12, 0), Vector3d(0, 1, 0));
	camera->SetCenterOfFocus(Vector3d(0, 12, 0));
	// grey background for window
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glShadeModel(GL_SMOOTH);
	glDepthRange(0.0, 1.0);
	glEnable(GL_NORMALIZE);
	glPointSize(pointSize);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);*/
	arrayHeight = texSize;
	arrayWidth = texSize;

	// Textures for lookup
	setupTextures();
	checkError("texture done");
	const char* computeSrc = readFileBytes("shader/flocking.comp");
	GLenum computeType[] = { GL_COMPUTE_SHADER };
	computeProgram = buildProgram(&computeSrc, computeType, 1);
	glUseProgram(computeProgram);
	mat4 projection = perspective(45.0f, 4.0f / 3.0f, 0.1f, 100.0f);
	mat4 view = lookAt(vec3(4, 3, 3), vec3(-4, -6, 0), vec3(0, 1, 0));
	mat4 model = mat4(0.1f);
	mat4 mvp = projection * view * model;
	glUniformMatrix4fv(glGetUniformLocation(computeProgram, "mvp"), 1, GL_FALSE, &mvp[0][0]);
	checkError("computeProgram");

	const char* displaySrc[] = { readFileBytes("shader/display.vert"), readFileBytes("shader/display.frag") };
	GLenum displayType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	displayProgram = buildProgram(displaySrc, displayType, 2);
	glUseProgram(displayProgram);
	glUniform2i(glGetUniformLocation(displayProgram, "size"), WIDTH, HEIGHT);
	checkError("displayProgram");

	// draw data
	glUseProgram(displayProgram);
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
	checkError("vertex");

	double lastTime = glfwGetTime();
	double nowTime;
	char fpsTitle[32];
	do {
		//render current frame
		//PerspDisplay();
		computationPass();

		glUseProgram(displayProgram);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, sizeof(vertexData));
		glfwSwapBuffers(window);
		glfwPollEvents();

		// prepare for the next frame

		nowTime = glfwGetTime();
		snprintf(fpsTitle, 32, "Flocking - FPS: %.2f", 1 / (nowTime - lastTime));
		glfwSetWindowTitle(window, fpsTitle);
		lastTime = nowTime;
		glClearTexImage(displayTex, 0, GL_RGBA, GL_FLOAT, NULL);
	} while (!glfwWindowShouldClose(window));

	return EXIT_SUCCESS;
}

void makeGrid() {
	glColor3f(0.63, 0.63, 0.63);
	for (float i = -12; i < 12; i++) {
		for (float j = -12; j < 12; j++) {
			glBegin(GL_LINES);
			glVertex3f(i, 0, j);
			glVertex3f(i, 0, j + 1);
			glEnd();
			glBegin(GL_LINES);
			glVertex3f(i, 0, j);
			glVertex3f(i + 1, 0, j);
			glEnd();

			if (j == 11) {
				glBegin(GL_LINES);
				glVertex3f(i, 0, j + 1);
				glVertex3f(i + 1, 0, j + 1);
				glEnd();
			}
			if (i == 11) {
				glBegin(GL_LINES);
				glVertex3f(i + 1, 0, j);
				glVertex3f(i + 1, 0, j + 1);
				glEnd();
			}
		}
	}

	glLineWidth(2.0);
	glBegin(GL_LINES);
	glVertex3f(-12, 0, 0);
	glVertex3f(12, 0, 0);
	glEnd();
	glBegin(GL_LINES);
	glVertex3f(0, 0, -12);
	glVertex3f(0, 0, 12);
	glEnd();
	glLineWidth(1.0);
}

void PerspDisplay() {
	glDrawBuffer(GL_BACK);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	// draw the camera created in perspective
	camera->PerspectiveDisplay(WIDTH, HEIGHT);
	glMatrixMode(GL_MODELVIEW);

	// Show the grid
	glLoadIdentity();
	glScalef(2.0, 0.0, 2.0);
	if (showGrid) {
		makeGrid();
	}
	// Now get the framebuffer we rendered textures to
	// bind the textures and enable them as well
	//glLoadIdentity();
	/*glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, positionTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, velocityTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, colorTex);

	// This is the magic speed component.
	// Here we get super fast access to the pixels on the GPU
	// and load them into a Vertex Buffer Object for reading.
	// We do this as opposed to loading into a heap allocatd array
	// Which saves a HUGE amount of process time, since everything virtually stays on
	// the GPU, no extra BUS accesses
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, vbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels(0, 0, arrayWidth, arrayHeight, GL_RGBA, GL_FLOAT, NULL); // MAGIC!!!
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, cbo);
	glReadBuffer(GL_COLOR_ATTACHMENT2);
	glReadPixels(0, 0, arrayWidth, arrayHeight, GL_RGBA, GL_FLOAT, NULL); // MAGIC!!!
	glBindBuffer(GL_PIXEL_PACK_BUFFER_ARB, 0);

	// Go back to the main framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Setup blending and make the particles look nice
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);

	// We have uniform locations to make the computation simpler
	// So scale and translate into position
	glScalef(5.0, 5.0, 5.0);
	glTranslatef(0.0, 2.0, 0.0);

	// Enable vertex arrays, bind our buffer with the texture PBO converted to VBO
	// Then draw the verts.
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexPointer(4, GL_FLOAT, 0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, cbo);
	glColorPointer(4, GL_FLOAT, 0, 0);

	glDrawArrays(GL_POINTS, 0, arrayWidth * arrayHeight);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);*/
}

void computationPass() {
	// Use our program and set uniforms
	glUseProgram(computeProgram);
	//glUniform1i(glGetUniformLocation(computeProgram, "positionTex"), 0);
	//glUniform1i(glGetUniformLocation(computeProgram, "velocityTex"), 1);
	//glUniform1i(glGetUniformLocation(computeProgram, "colorTex"), 2);
	glUniform1f(glGetUniformLocation(computeProgram, "timeStep"), timeStep);
	glUniform1i(glGetUniformLocation(computeProgram, "texSixe"), texSize);
	glUniform1f(glGetUniformLocation(computeProgram, "cohesion"), cohesion);
	glUniform1f(glGetUniformLocation(computeProgram, "alignment"), alignment);
	glUniform1f(glGetUniformLocation(computeProgram, "neighborRadius"), neighborRadius);
	glUniform1f(glGetUniformLocation(computeProgram, "collisionRadius"), collisionRadius);

	glDispatchCompute(texSize / 16, texSize / 16, 1);

	glUseProgram(0);

	/*glLoadIdentity();

	// Render to texture setup,
	// Esentially making a full screen quad so each texel turns into a fragment which can be
	// replaced using the frag shader. Hack magic
	glViewport(0, 0, arrayWidth, arrayHeight);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	gluOrtho2D(0.0, arrayWidth, 0.0, arrayHeight);

	glEnable(GL_TEXTURE_2D);

	// Bind to our new framebuffer that will not be seen
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	// This is important, specify number of gl_FragData[N] outputs
	GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	// N=3
	glDrawBuffers(3, drawBufs);

	// uniform values

	// Draw the quad ie create a texture(s) that hold our position, velocity, and color
	glShadeModel(GL_FLAT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0, 0.0);
	glVertex2f(0.0, 0.0);
	glTexCoord2f(1.0, 0.0);
	glVertex2f(arrayWidth, 0.0);
	glTexCoord2f(1.0, 1.0);
	glVertex2f(arrayWidth, arrayHeight);
	glTexCoord2f(0.0, 1.0);
	glVertex2f(0.0, arrayHeight);
	glEnd();

	glUseProgram(0);

	// Return it back to normal
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glViewport(0, 0, WIDTH, HEIGHT);
	glFlush();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDisable(GL_TEXTURE_2D);*/
}

void setupTextures() {

	int numParticles = arrayWidth*arrayHeight;
	GLfloat *texData = new GLfloat[4 * numParticles];

	// Init all to fit in uniform cube
	for (int i = 0; i < numParticles; ++i) {
		texData[4 * i + 0] = ((float)rand() / RAND_MAX)*2.0 - 1.0;
		texData[4 * i + 1] = ((float)rand() / RAND_MAX)*2.0 - 1.0;
		texData[4 * i + 2] = ((float)rand() / RAND_MAX)*2.0 - 1.0;
		texData[4 * i + 3] = 1.0;
	}

	glGenTextures(1, &positionTex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, positionTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, arrayWidth, arrayHeight, 0, GL_RGBA, GL_FLOAT, texData);
	glBindImageTexture(0, positionTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	// Init all the velocities
	default_random_engine generator(rand());
	normal_distribution<double> distribution(0.0, 2.8);

	for (int i = 0; i < numParticles; ++i) {
		texData[i * 4 + 0] = distribution(generator);
		texData[i * 4 + 1] = distribution(generator);
		texData[i * 4 + 2] = distribution(generator);
		texData[i * 4 + 3] = 0.0;
	}

	glGenTextures(1, &velocityTex);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, velocityTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, arrayWidth, arrayHeight, 0, GL_RGBA, GL_FLOAT, texData);
	glBindImageTexture(1, velocityTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	// Init all the colors and storage
	for (int i = 0; i < numParticles; ++i) {
		texData[i * 4 + 0] = ((float)rand() / RAND_MAX) * 0.1;
		texData[i * 4 + 1] = ((float)rand() / RAND_MAX) * 0.1;
		texData[i * 4 + 2] = ((float)rand() / RAND_MAX) * 0.1 + 0.5;
		texData[i * 4 + 3] = 0.0; // Initially invisible
	}
	glGenTextures(1, &colorTex);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, colorTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, arrayWidth, arrayHeight, 0, GL_RGBA, GL_FLOAT, texData);
	glBindImageTexture(2, colorTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	glGenTextures(1, &displayTex);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, displayTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WIDTH, HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
	glBindImageTexture(3, displayTex, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F);

	//delete[] texData;

	// Gen the framebuffer for rendering
	/*glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);

	// bind textures so we can use them later
	glBindTexture(GL_TEXTURE_2D, positionTex);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, positionTex, 0);

	glBindTexture(GL_TEXTURE_2D, velocityTex);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, velocityTex, 0);

	glBindTexture(GL_TEXTURE_2D, colorTex);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, colorTex, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		printf("ERROR - Incomplete FrameBuffer\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Vertex buffer init, positions
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, arrayWidth*arrayHeight * 4 * sizeof(GLfloat), NULL, GL_STREAM_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Color buffer init, positions
	glGenBuffers(1, &cbo);
	glBindBuffer(GL_ARRAY_BUFFER, cbo);
	glBufferData(GL_ARRAY_BUFFER, arrayWidth*arrayHeight * 4 * sizeof(GLfloat), NULL, GL_STREAM_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);*/
}


void onKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		switch (key) {
			case 'c': case 'C': {
				camera->Reset();
				break;
			}
			case 'r': case 'R': {
				arrayHeight = texSize;
				arrayWidth = texSize;
				glPointSize(pointSize);
				setupTextures();
				break;
			} case 'f': case 'F': {
				camera->SetCenterOfFocus(Vector3d(0, 10, 0));
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
	camera->HandleMouseEvent(window, button, action, mods);
}

void onCursorPos(GLFWwindow* window, double xpos, double ypos) {
	// let the camera handle some mouse motions if the camera is to be moved
	camera->HandleMouseMotion(window, xpos, ypos);
}

void onResize(GLFWwindow* window, int width, int height) {
	WIDTH = width;
	HEIGHT = height;
}
