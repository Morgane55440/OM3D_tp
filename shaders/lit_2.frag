#version 450

#include "utils.glsl"

// fragment shader of the main lighting pass

// #define DEBUG_NORMAL

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;



layout(binding = 0) uniform sampler2D in_color;
layout(binding = 1) uniform sampler2D in_normal;
layout(binding = 2) uniform sampler2D in_depth;

layout(binding = 3) uniform Data {
    FrameData frame;
};

layout(binding = 4) buffer PointLights {
        PointLight point_lights[];
};
layout(binding = 5) uniform WindowData {
    WindowSize window_size;
};


const vec3 ambient = vec3(0.0);

vec3 unproject(vec2 uv, float depth, mat4 inv_viewproj) {
    const vec3 ndc = vec3(uv * 2.0 - vec2(1.0), depth);
    const vec4 p = inv_viewproj * vec4(ndc, 1.0);
    return p.xyz / p.w;
}


void main() {
    const mat4 inv = inverse(frame.camera.view_proj);
    const ivec2 coord = ivec2(gl_FragCoord.xy);
    const vec2 uv = vec2(gl_FragCoord.xy) / vec2(window_size.inner);
    const vec3 normal = texelFetch(in_normal, coord, 0).xyz * 2 - 1;
    const float pixel_depth = texelFetch(in_depth, coord, 0).x;
    const vec3 pos = unproject(uv, pixel_depth, inv);

    PointLight light = point_lights[0];
   const vec3 to_light = (light.position - pos);
   const float dist = length(to_light);
   const vec3 light_vec = to_light / dist;

   const float NoL = dot(light_vec, normal);
   const float att = attenuation(dist, light.radius * 100);
   vec3 acc = vec3(0.0, 0.0, 0.0);
   if (NoL > 0.0f && att > 0.0f) {
        acc = light.color * (NoL * att);
   }
   

    out_color = vec4(0.0,0.0, 0.0, 1.0);
    out_color.xyz = acc *  texelFetch(in_color, coord, 0).xyz ;


    out_normal = vec4(normal, 1);

}

