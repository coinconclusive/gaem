#version 450 core
out vec4 oColor;

in vec3 sNormal;
in vec3 sNormal;
in vec2 sTexCoord;

uniform vec3 uSunDir;
uniform vec4 uSunCol;
uniform sampler2D uTexture;

void main() {
	float shade = clamp(dot(uSunDir, sNormal), 0.1, 1.0);
	vec3 shadeColor = uSunCol.xyz * shade * uSunCol.w;
	vec3 color = texture(uTexture, sTexCoord).rgb * shadeColor;
	oColor = vec4(color, 1.0);
}
