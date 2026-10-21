#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

out vec4 vertColor;

uniform mat4 projection;
uniform vec2 offset;

void main() {
    vertColor = aColor;
    gl_Position = projection * vec4(aPos + offset, 0.0, 1.0);
}
