#version 450

#include "utils.glsl"

layout(location = 0) out vec4 out_color;

layout(location = 0) in vec2 in_uv;

layout(binding = 0) uniform sampler2D in_color;
layout(binding = 1) uniform sampler2D in_normal;
layout(binding = 2) uniform sampler2D in_depth;

uniform uint outputtype = 0;


void main() {
    const ivec2 coord = ivec2(gl_FragCoord.xy);

    if (outputtype == 3) {
        out_color = texelFetch(in_color, coord, 0);
    }
    if (outputtype == 1) {
        out_color = texelFetch(in_normal, coord, 0);
    }
    if (outputtype == 2) {
        float d = pow(texelFetch(in_depth, coord, 0).x, 0.35);
        out_color = vec4(d, d, d , 1.0);
    }
}
