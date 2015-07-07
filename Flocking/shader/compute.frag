#version 330 core

uniform sampler2D positionTex;
uniform sampler2D velocityTex;
uniform sampler2D colorTex;

uniform int texSixe;
uniform float timeStep;

uniform float cohesion;
uniform float alignment;
uniform float neighborRadius;
uniform float collisionRadius;

layout(location = 0) out vec3 positionOut;
layout(location = 1) out vec3 velocityOut;
layout(location = 2) out vec4 colorOut;

void main() {
	vec2 target = gl_FragCoord.xy / texSixe;
	vec3 position = texture(positionTex, target).xyz;
	vec3 velocity = texture(velocityTex, target).xyz;
	//vec4 color = texture(colorTex, target);

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
