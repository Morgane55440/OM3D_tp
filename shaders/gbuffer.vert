#version 450

// input
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

// Uniformes
layout(binding = 0) uniform mat4 model_matrix;
layout(binding = 1) uniform mat4 view_proj_matrix;

out vec3 frag_pos;
out vec3 frag_normal;
out vec2 frag_uv;
out vec3 frag_color;

void main()
{
    frag_pos = vec3(model_matrix * vec4(in_position, 1.0));

    frag_normal = mat3(model_matrix) * in_normal;

    // Passer les coordonnées UV
    frag_uv = in_uv;

    frag_color = vec3(1.0, 0.5, 0.0);  

    gl_Position = view_proj_matrix * vec4(frag_pos, 1.0);
}