#version 450 core
layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec2 iTexCoord;

uniform mat4 uTransform;

out vec3 sNormal;
out vec2 sTexCoord;

void main() {
	sNormal = iNormal;
	sTexCoord = iTexCoord;
	gl_Position = uTransform * vec4(iPosition, 1.0);
}
