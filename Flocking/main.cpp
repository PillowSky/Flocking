/*
* Flocking
*
* Flocking animation with avoidance(separation), cohesion, and alignment on a local scale.
* Completely implemented with glsl.
*
* 2015 Yuzo(Huang Yuzhong)
*
*/

#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <random>
#include <boost/format.hpp>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "common/camera.h"
#include "common/util.hpp"

using namespace std;
using namespace boost;
using namespace glm;

int WIDTH = 1024;
int HEIGHT = 768;
int ROOT_OF_NUM_PARTICLES = 64;

float PointSize = 3.0;
float Cohesion = 1000.0;
float Alignment = 80.0;
float NeighborRadius = 1.0;
float CollisionRadius = 0.1;

Camera* camera;
bool showGrid = false;

GLuint computeProgram; // programs
GLuint vbo, cbo;

GLuint arrayWidth, arrayHeight;
GLuint tex_velocity, tex_color, tex_position, fbo;
GLfloat time_step = 0.01;

void init();
void makeGrid();
void setupTextures();
void PerspDisplay();
void computationPass();
void mouseEventHandler(GLFWwindow* window, int button, int action, int mods);
void motionEventHandler(GLFWwindow* window, double xpos, double ypos);
void keyboardEventHandler(GLFWwindow* window, int key, int scancode, int action, int mods);
void onResize(GLFWwindow* window, int width, int height);

int main(int argc, char* argv[]) {
	if (!glfwInit()) {
		throw runtime_error("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Flocking", NULL, NULL);
	if (!window) {
		throw runtime_error("Failed to open GLFW window");
	}
	glfwMakeContextCurrent(window);
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GL_TRUE);
	glfwSetKeyCallback(window, keyboardEventHandler);
	glfwSetMouseButtonCallback(window, mouseEventHandler);
	glfwSetCursorPosCallback(window, motionEventHandler);
	glfwSetWindowSizeCallback(window, onResize);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		throw runtime_error("Failed to initialize GLEW");
	}

	// initialize the camera and such
	init();

	// Textures for lookup
	setupTextures();

	const char* shaderCode[] = { readFileBytes("shader/flocking.vert"), readFileBytes("shader/flocking.frag") };
	GLenum shaderType[] = { GL_VERTEX_SHADER, GL_FRAGMENT_SHADER };
	computeProgram = buildProgram(shaderCode, shaderType, 2);


	double lastTime = glfwGetTime();
	double nowTime;
	char fpsTitle[32];
	do {
		//render current frame
		PerspDisplay();
		glfwSwapBuffers(window);
		glfwPollEvents();

		// prepare for the next frame
		computationPass();
		nowTime = glfwGetTime();
		sprintf(fpsTitle, "Flocking - FPS: %.2f", 1 / (nowTime - lastTime));
		glfwSetWindowTitle(window, fpsTitle);
		lastTime = nowTime;
	} while (!glfwWindowShouldClose(window));

	return EXIT_SUCCESS;
}

void init() {
	// set up camera
	// parameters are eye point, aim point, up vector
	camera = new Camera(Vector3d(0, 12, 30), Vector3d(0, 12, 0), Vector3d(0, 1, 0));
	camera->SetCenterOfFocus(Vector3d(0, 12, 0));
	// grey background for window
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glShadeModel(GL_SMOOTH);
	glDepthRange(0.0, 1.0);
	glEnable(GL_NORMALIZE);
	glPointSize(PointSize);
	glEnable(GL_BLEND);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive
	arrayHeight = ROOT_OF_NUM_PARTICLES;
	arrayWidth = ROOT_OF_NUM_PARTICLES;
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
	glLoadIdentity();
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_position);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex_velocity);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, tex_color);


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
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

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
	glDisable(GL_BLEND);
}

void computationPass() {
	glLoadIdentity();

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
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
	// This is important, specify number of gl_FragData[N] outputs
	GLenum drawBufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	// N=3
	glDrawBuffers(3, drawBufs);

	// Use our program and set uniforms
	glUseProgram(computeProgram);
	glUniform1i(glGetUniformLocation(computeProgram, "positionTex"), 0);
	glUniform1i(glGetUniformLocation(computeProgram, "velocityTex"), 1);
	glUniform1i(glGetUniformLocation(computeProgram, "colorTex"), 2);
	glUniform1f(glGetUniformLocation(computeProgram, "time_step"), (GLfloat)time_step);
	glUniform1i(glGetUniformLocation(computeProgram, "texSixe"), ROOT_OF_NUM_PARTICLES);
	glUniform1f(glGetUniformLocation(computeProgram, "Cohesion"), (GLfloat)Cohesion);
	glUniform1f(glGetUniformLocation(computeProgram, "Alignment"), (GLfloat)Alignment);
	glUniform1f(glGetUniformLocation(computeProgram, "NeighborRadius"), (GLfloat)NeighborRadius);
	glUniform1f(glGetUniformLocation(computeProgram, "CollisionRadius"), (GLfloat)CollisionRadius);

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

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glDisable(GL_TEXTURE_2D);
}

void mouseEventHandler(GLFWwindow* window, int button, int action, int mods) {
	// let the camera handle some specific mouse events (similar to maya)
	camera->HandleMouseEvent(window, button, action, mods);
}

void motionEventHandler(GLFWwindow* window, double xpos, double ypos) {
	// let the camera handle some mouse motions if the camera is to be moved
	camera->HandleMouseMotion(window, xpos, ypos);
}

void setupTextures() {

	int numParticles = arrayWidth*arrayHeight;
	GLfloat *tex_data = new GLfloat[4 * numParticles];

	// Init all to fit in uniform cube
	for (int i = 0; i < numParticles; ++i) {
		tex_data[4 * i + 0] = ((float)rand() / RAND_MAX)*2.0 - 1.0;
		tex_data[4 * i + 1] = ((float)rand() / RAND_MAX)*2.0 - 1.0;
		tex_data[4 * i + 2] = ((float)rand() / RAND_MAX)*2.0 - 1.0;
		tex_data[4 * i + 3] = 1.0;
	}

	glGenTextures(1, &tex_position);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_position);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, arrayWidth, arrayHeight, 0, GL_RGBA, GL_FLOAT, &tex_data[0]);

	// Init all the velocities
	default_random_engine generator(rand());
	normal_distribution<double> distribution(0.0, 2.8);

	for (int i = 0; i < numParticles; ++i) {
		tex_data[i * 4 + 0] = distribution(generator);
		tex_data[i * 4 + 1] = distribution(generator);
		tex_data[i * 4 + 2] = distribution(generator);
		tex_data[i * 4 + 3] = 0.0;
	}

	glGenTextures(1, &tex_velocity);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex_velocity);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, arrayWidth, arrayHeight, 0, GL_RGBA, GL_FLOAT, &tex_data[0]);

	// Init all the colors and storage
	for (int i = 0; i < numParticles; ++i) {
		tex_data[i * 4 + 0] = ((float)rand() / RAND_MAX) * 0.1;
		tex_data[i * 4 + 1] = ((float)rand() / RAND_MAX) * 0.1;
		tex_data[i * 4 + 2] = ((float)rand() / RAND_MAX) * 0.1 + 0.5;
		tex_data[i * 4 + 3] = 0.0; // Initially invisible
	}
	glGenTextures(1, &tex_color);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, tex_color);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F_ARB, arrayWidth, arrayHeight, 0, GL_RGBA, GL_FLOAT, &tex_data[0]);

	delete[] tex_data;

	// Gen the framebuffer for rendering
	glGenFramebuffersEXT(1, &fbo);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

	// bind textures so we can use them laterz
	glBindTexture(GL_TEXTURE_2D, tex_position);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, tex_position, 0);

	glBindTexture(GL_TEXTURE_2D, tex_velocity);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, tex_velocity, 0);

	glBindTexture(GL_TEXTURE_2D, tex_color);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT2_EXT, GL_TEXTURE_2D, tex_color, 0);

	if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
		printf("ERROR - Incomplete FrameBuffer\n");

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	// Vertex buffer init, positions
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, arrayWidth*arrayHeight * 4 * sizeof(GLfloat), NULL, GL_STREAM_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Color buffer init, positions
	glGenBuffers(1, &cbo);
	glBindBuffer(GL_ARRAY_BUFFER, cbo);
	glBufferData(GL_ARRAY_BUFFER, arrayWidth*arrayHeight * 4 * sizeof(GLfloat), NULL, GL_STREAM_COPY);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


void keyboardEventHandler(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_PRESS) {
		switch (key) {
			case 'c': case 'C': {
				camera->Reset();
				break;
			}
			case 'r': case 'R': {
				arrayHeight = ROOT_OF_NUM_PARTICLES;
				arrayWidth = ROOT_OF_NUM_PARTICLES;
				glPointSize(PointSize);
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

void onResize(GLFWwindow* window, int width, int height) {
	WIDTH = width;
	HEIGHT = height;
}
