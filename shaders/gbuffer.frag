#version 450

#include "utils.glsl"

// output
layout(location = 0) out vec4 out_color;
layout(location = 1) out vec3 out_normal;

// input
layout(location = 0) in vec3 in_normal;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_color;
layout(location = 3) in vec3 in_position;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;

layout(binding = 0) uniform sampler2D in_color_texture;
layout(binding = 1) uniform sampler2D in_normal_texture;

void main()
{
    #ifdef NORMAL_MAPPED
        const vec3 normal_map = unpack_normal_map(texture(in_normal_texture, in_uv).xy);
        out_normal = normal_map.x * in_tangent +
                            normal_map.y * in_bitangent +
                            normal_map.z * in_normal;
    #else
        out_normal = in_normal;
    #endif

    out_normal = out_normal * 0.5 + 0.5;

    #ifdef TEXTURED
        out_color = texture(in_color_texture, in_uv);
    #else
        out_color = vec4(1.0);
    #endif

}