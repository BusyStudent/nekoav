#version 440

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D videoTex;

void main() {
    fragColor = texture(videoTex, v_texcoord);
}