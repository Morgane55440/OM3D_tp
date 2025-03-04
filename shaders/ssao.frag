#version 450

#include "utils.glsl"

// fragment shader of the ssao pass

// #define DEBUG_NORMAL

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;



layout(binding = 0) uniform sampler2D in_color;
layout(binding = 1) uniform sampler2D in_normal;
layout(binding = 2) uniform sampler2D in_depth;
layout(binding = 3) uniform sampler2D in_position;

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

bool rayMarchBounce(vec3 startPos, vec3 dir, float maxDist, int steps, mat4 invProj, out vec3 hitPos) {
    float stepSize = maxDist / float(steps);
    vec3 pos = startPos;

    for (int i = 0; i < steps; i++) {
        pos += dir * stepSize;
        vec4 clipSpace = invProj * vec4(pos, 1.0);
        vec2 uv = clipSpace.xy / clipSpace.w * 0.5 + 0.5;
        float sceneDepth = texture(in_depth, uv).r * 2.0 - 1.0;

        if (sceneDepth < clipSpace.z / clipSpace.w) {
            hitPos = pos;
            return true;
        }
    }

    hitPos = vec3(0.0);
    return false;
}

vec3 ComputeIndirectColor(vec3 pos, vec3 normal, vec3 lightColor, vec3 light_vec, mat4 invProj) {
    vec3 reflectedRay = reflect(-light_vec, normal);

    vec3 hitPos;
    bool bounceHit = rayMarchBounce(pos, reflectedRay, 5.0, 50, invProj, hitPos);

    if (bounceHit) {
        vec4 clipSpace = invProj * vec4(hitPos, 1.0);
        vec2 hitUV = clipSpace.xy / clipSpace.w * 0.5 + 0.5;
        ivec2 hitCoord = ivec2(hitUV * window_size.inner);
        vec3 hitColor = texelFetch(in_position, ivec2(hitCoord.xy), 0).xyz;
        vec3 hitNormal = texelFetch(in_normal, ivec2(hitCoord.xy), 0).xyz * 2.0 - 1.0;

        float diffuseFactor = max(dot(hitNormal, light_vec), 0.0);
        return hitColor * lightColor * diffuseFactor;
    }

    return vec3(0.0);
}

void main() {
    const mat4 invProj = inverse(frame.camera.view_proj);
    const ivec2 coord = ivec2(gl_FragCoord.xy);
    const vec2 uv = vec2(gl_FragCoord.xy) / vec2(window_size.inner);
    const vec3 normal = texelFetch(in_normal, coord, 0).xyz * 2 - 1;
    const float pixel_depth = texelFetch(in_depth, coord, 0).x;
    const vec3 pos = unproject(uv, pixel_depth, invProj);

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

    vec3 direct_color = vec3(0.0);
    direct_color = (acc *  texelFetch(in_color, coord, 0).xyz);

    vec3 indirect_color = ComputeIndirectColor(pos, normal, light.color, light_vec, invProj);

    out_color = vec4(direct_color + indirect_color, 1.0);

    out_normal = vec4(normal, 1);

}