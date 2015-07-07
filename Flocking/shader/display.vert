#version 330 core

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 color;

out vec4 pixel;

uniform mat4 mvp;
uniform vec2 windowSize;

void main() {
	gl_Position = mvp * position;
	pixel = color;
}
