#version 440 core

out vec4 color;

layout(binding = 3) uniform sampler2D displayTex;
uniform ivec2 size;

void main() {
	color = texture(displayTex, gl_FragCoord.xy / size);
}
