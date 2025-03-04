#version 450

#include "utils.glsl"

// fragment shader of the main lighting pass

// #define DEBUG_NORMAL

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_sun;

layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec3 in_color;
layout(location = 4) in vec3 in_tangent;
layout(location = 5) in vec3 in_bitangent;

layout(binding = 0) uniform sampler2D in_texture;
layout(binding = 1) uniform sampler2D in_normal_texture;
layout(binding = 2) uniform sampler2D in_depth;
layout(binding = 3) uniform sampler2D in_position;
layout(binding = 4) uniform sampler2D in_normal;

layout(binding = 3) uniform Data {
    FrameData frame;
};

layout(binding = 6) uniform WindowData {
    WindowSize window_size;
};



const vec3 ambient = vec3(0.0);
const int NUM_SAMPLES = 16;  // Number of AO samples
const float AO_RADIUS = 1.0; // Maximum radius for occlusion testing

vec3 unproject(vec2 uv, float depth, mat4 inv_viewproj) {
    const vec3 ndc = vec3(uv * 2.0 - vec2(1.0), depth);
    const vec4 p = inv_viewproj * vec4(ndc, 1.0);
    return p.xyz / p.w;
}

vec2 randomUnitDisk(vec2 rand) {
    float theta = 2.0 * 3.14159265 * rand.x;
    float r = sqrt(rand.y);
    return vec2(r * cos(theta), r * sin(theta));
}

vec3 randomHemisphereDirection(vec3 normal, vec2 rand) {
    float phi = rand.x * 2.0 * 3.14159265;
    float cosTheta = sqrt(1.0 - rand.y);
    float sinTheta = sqrt(rand.y);

    vec3 tangent = normalize(cross(normal, vec3(0.0, 1.0, 0.0)));
    vec3 bitangent = cross(normal, tangent);
    return normalize(tangent * cos(phi) * sinTheta + bitangent * sin(phi) * sinTheta + normal * cosTheta);
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

float horizonAngleAO(vec3 pos, vec3 normal, mat4 invProj) {
    float occlusion = 0.0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        vec2 randUV = vec2(float(i) / float(NUM_SAMPLES), fract(sin(float(i) * 12.9898) * 43758.5453));
        vec3 sampleDir = randomHemisphereDirection(normal, randUV);

        vec3 hitPos;
        bool hit = rayMarchBounce(pos, sampleDir, AO_RADIUS, 10, invProj, hitPos);

        if (hit) {
            float angle = dot(normal, normalize(hitPos - pos));
            occlusion += clamp(1.0 - angle, 0.0, 1.0);
        }
    }
    return 1.0 - (occlusion / float(NUM_SAMPLES));
}

float MonteCarloAO(vec3 pos, vec3 normal, mat4 invProj, out vec3 bentNormal, out float bentConeAngle) {
    float occlusion = 0.0;
    bentNormal = vec3(0.0);
    float validSamples = 0.0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        vec2 randUV = vec2(float(i) / float(NUM_SAMPLES), fract(sin(float(i) * 12.9898) * 43758.5453));

        // Use Crytek2D for screen-space sample generation
        vec2 offset = randomUnitDisk(randUV) * AO_RADIUS;

        // Use Crytek3D for hemisphere sample distribution
        vec3 sampleDir = randomHemisphereDirection(normal, randUV);

        vec3 hitPos;
        bool hit = rayMarchBounce(pos, sampleDir, AO_RADIUS, 10, invProj, hitPos);

        if (hit) {
            occlusion += 1.0;
        } else {
            bentNormal += sampleDir;
            validSamples += 1.0;
        }
    }

    // Normalize bent normal
    if (validSamples > 0.0) {
        bentNormal = normalize(bentNormal / validSamples);
    } else {
        bentNormal = normal;
    }

    float ao = 1.0 - (occlusion / float(NUM_SAMPLES));

    // Apply AO correction using horizon-based approach (HBAO)
    float horizonAO = horizonAngleAO(pos, normal, invProj);

    float bentNormalLength = length(bentNormal);
    bentConeAngle = (1.0 - max(0.0, 2.0 * bentNormalLength - 1.0)) * (3.14159265 / 2.0);

    // Combine both effects
    return ao * horizonAO * max(dot(normal, bentNormal), 0.5);
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
    const vec2 uv = vec2(gl_FragCoord.xy) / vec2(window_size.inner);
    const ivec2 coord = ivec2(gl_FragCoord.xy);
    const float pixel_depth = texelFetch(in_depth, coord, 0).x;
    const vec3 pos = unproject(uv, pixel_depth, invProj);

#ifdef NORMAL_MAPPED
    const vec3 normal_map = unpack_normal_map(texture(in_normal_texture, in_uv).xy);
    const vec3 normal = normal_map.x * in_tangent +
                        normal_map.y * in_bitangent +
                        normal_map.z * texelFetch(in_normal, coord, 0).xyz * 2 - 1;
#else
    const vec3 normal = texelFetch(in_normal, coord, 0).xyz * 2 - 1;
#endif

    vec3 acc = frame.sun_color * max(0.0, dot(frame.sun_dir, normal)) + ambient;

    vec3 bentNormal;
    float bentConeAngle;
    float bentAO = MonteCarloAO(pos, normal, invProj, bentNormal, bentConeAngle);
    vec3 indirect_color = ComputeIndirectColor(pos, bentNormal, frame.sun_color, frame.sun_dir, invProj);

    out_sun = vec4(in_color * acc * bentAO + indirect_color, 1.0);

#ifdef TEXTURED
    out_sun *=  texture(in_texture, in_uv);
    out_color = texture(in_texture, in_uv);
#endif

    out_normal = vec4(normal * 0.5 + 0.5, 1);
}

