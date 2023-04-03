#version 410 core
out vec4 oColor;

in vec3 sNormal;

uniform vec3 uSunDir;
uniform vec4 uSunCol;

void main() {
	// float shade = clamp(dot(sNormal, uSunDir), 0.1, 1.0);
	// vec3 shadeColor = uSunCol.xyz * shade * uSunCol.w;
	// vec3 color = vec3(1.0, 1.0, 1.0) * shadeColor;
	oColor = vec4((sNormal + 1.0) / 2.0, 1.0);
}
