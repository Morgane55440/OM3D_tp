#version 450

// output
layout(location = 0) out vec4 out_color;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out float out_depth;

// input
layout(location = 0) in vec3 frag_pos;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_uv;
layout(location = 3) in vec3 frag_color;

// Uniformes
layout(binding = 0) uniform sampler2D sunlight_texture;
layout(binding = 1) uniform Data {
    FrameData frame;
};

void main()
{
    out_depth = frag_pos.z;

    out_normal = normalize(frag_normal);

    out_color = vec4(frag_color, 1.0);
}