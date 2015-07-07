#version 440 core

layout(location = 3) in vec4 vertex;

void main() {
	gl_Position = vertex;
}
