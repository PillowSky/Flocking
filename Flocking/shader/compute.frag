#version 440 core

layout(binding = 0) uniform sampler2D positionTex;
layout(binding = 1) uniform sampler2D velocityTex;
layout(binding = 2) uniform sampler2D colorTex;

uniform float timeStep;
uniform int texSixe;

uniform float cohesion;
uniform float alignment;
uniform float neighborRadius;
uniform float collisionRadius;

layout(location = 0) out vec3 positionOut;
layout(location = 1) out vec3 velocityOut;
layout(location = 2) out vec3 colorOut;

void main() {
	vec3 position = texture2D(positionTex, gl_FragCoord.xy).xyz;
	vec3 velocity = texture2D(velocityTex, gl_FragCoord.xy).xyz;
	vec3 color = texture2D(colorTex, gl_FragCoord.xy).xyz;

	// update particle if visible
	// Rule 1 : collision avoidance
	vec3 c1 = vec3(0.0, 0.0, 0.0);
	vec3 c2 = vec3(0.0, 0.0, 0.0);
	vec3 c3 = vec3(0.0, 0.0, 0.0);
	int N = 0;

	for (int i = 0; i < texSixe; i++) {
		for (int j = 0; j < texSixe; j++) {
			vec2 coord = vec2(i, j);

			if (coord != gl_FragCoord.xy) {
				vec3 position_n = texture2D(positionTex, coord).xyz;
				vec3 velocity_n = texture2D(velocityTex, coord).xyz;

				// Rule 1 : Collision avoidance
				if (length(position_n - position) < collisionRadius) { // Possible collision, avoid
					c1 -= position_n - position;
				}
				if (length(position_n - position) < neighborRadius) {
					// Rule 2 : cohesion, add up all position
					c2 += position_n;
					// Rule 3 : alignment, velocity
					c3 += velocity_n;
					N++;
				}
			}
		}
	}

	c2 /= (N - 1);
	c2 = (c2 - position) / cohesion;

	c3 /= (N - 1);
	c3 = (c3 - velocity) / alignment;

	// Constrain to sphere
	vec3 c4 = vec3(0.0, 0.0, 0.0);
	if (length(position) > 1.5) {
		c4 = -1 * position / 5.0;
	}

	velocity.xyz += c1 + c2 + c3 + c4;

	// Limit velocity
	if (length(velocity) > 3.0) {
		velocity = (velocity / length(velocity)) * 3.0;
	}

	position.xyz += velocity.xyz * timeStep;

	// update position
	positionOut = position;

	// update velocity and lifetime
	velocityOut = velocity;

	// update color
	//colorOut = color;

}
