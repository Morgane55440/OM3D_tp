#version 450

#include "utils.glsl"

// fragment shader of the direct lighning and ssao pass

// #define DEBUG_NORMAL

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;

layout(binding = 0) uniform sampler2D in_color;
layout(binding = 1) uniform sampler2D in_normal;
layout(binding = 2) uniform sampler2D in_depth;

layout(binding = 3) uniform Data {
    FrameData frame;
};

layout(binding = 4) uniform WindowData {
    WindowSize window_size;
};

const vec3 ambient = vec3(0.0);

vec3 unproject(vec2 uv, float depth, mat4 inv_viewproj) {
    const vec3 ndc = vec3(uv * 2.0 - vec2(1.0), depth);
    const vec4 p = inv_viewproj * vec4(ndc, 1.0);
    return p.xyz / p.w;
}

float rand() {
    return fract(sin(gl_FragCoord.x * 12.9898 + gl_FragCoord.y * 78.233) * 43758.5453);
}

vec3 randomHemisphereDirection(vec3 normal, vec2 rand) {
    float phi = rand.x * 2.0 * 3.14159265;
    float cosTheta = sqrt(1.0 - rand.y);
    float sinTheta = sqrt(rand.y);

    vec3 tangent = normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    vec3 bitangent = cross(normal, tangent);
    return normalize(tangent * cos(phi) * sinTheta + bitangent * sin(phi) * sinTheta + normal * cosTheta);
}

// Monte Carlo AO
float calculateAO(vec3 fragPos, vec3 normal, ivec2 coord, mat4 invProj) {
    const int numSamples = 16;
    const float radius = 0.1;

    float occlusion = 0.0;
    for (int i = 0; i < numSamples; ++i) {
        // random direction sample around fragment pos
        vec2 randUV = vec2(float(i) / float(numSamples), rand());
        vec3 sampleDir = randomHemisphereDirection(normal, randUV);
        
        // pos of the neighbor pixel
        vec3 samplePos = fragPos + sampleDir * radius;

        // Screen space
        vec4 projPos = invProj * vec4(samplePos, 1.0);
        projPos.xyz /= projPos.w;
        vec2 sampleUV = projPos.xy * 0.5 + 0.5;  //  [-1;1] -> [0;1]

        // Get the depth of the sample pixel
        float sampleDepth = texture(in_depth, coord + sampleUV).x;
        vec3 samPos = unproject(sampleUV, sampleDepth, inverse(frame.camera.view_proj));


        // Check if there is an occlusion
        if (samPos.z < projPos.z - 0.01) {
            occlusion += 1.0;
        }
    }

    return 1.0 - (occlusion / float(numSamples));
}

void main() {
    const mat4 invProj = inverse(frame.camera.view_proj);
    const ivec2 coord = ivec2(gl_FragCoord.xy);
    const vec2 uv = vec2(gl_FragCoord.xy) / vec2(window_size.inner);
    const vec3 normal = texelFetch(in_normal, coord, 0).xyz * 2 - 1;
    const float pixel_depth = texelFetch(in_depth, coord, 0).x;
    const vec3 pos = unproject(uv, pixel_depth, invProj);

    vec3 direct_color = texelFetch(in_color, coord, 0).rgb;

    float ao = calculateAO(pos, normal, coord, invProj);

    out_color = vec4(direct_color * ao, 1.0);

    out_normal = vec4(normal, 1);
}