#version 440 core

layout(location = 0) in vec4 position;
layout(location = 2) in vec4 color;

out vec4 pixel;

uniform mat4 mvp;
uniform vec2 size;

void main() {
	gl_Position = mvp * vec4(position.x / 2, position.y / 2, position.y / 2, 1);
	pixel = color;
}
