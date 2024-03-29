#version 450

layout(location = 0) out vec3 frag_color;

layout(location = 0) in vec2 vert_pos;
layout(location = 1) in vec3 vert_color;

void main() {
    gl_Position = vec4(vert_pos, 0.0, 1.0);
    frag_color = vert_color;
}
