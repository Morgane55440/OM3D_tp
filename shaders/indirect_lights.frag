    #version 450

    #include "utils.glsl"

    layout(location = 0) out vec4 out_color;
    
    layout(location = 0) in vec2 in_uv;

    layout(binding = 0) uniform sampler2D in_hdr;
    layout(binding = 1) uniform sampler2D in_normal;
    layout(binding = 2) uniform sampler2D in_depth;
    layout(binding = 3) uniform Data {
        FrameData frame;
    };
    layout(binding = 5) uniform WindowData {
        WindowSize window_size;
    };

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

    vec3 rayMarchIndirect(vec3 fragPos, vec3 normal, sampler2D depthTexture, sampler2D hdrTexture) {
        const int numSamples = 32; 
        const int maxSteps = 100;
        const float stepSize = 0.02;
        const float maxDist = 20;
    
        vec3 indirectLights = vec3(0.0);
        
        for (int i = 0; i < numSamples; ++i) {
            vec2 randUV = vec2(float(i) / float(numSamples), rand());
            vec3 rayDir = randomHemisphereDirection(normal, randUV);
            rayDir = dot(rayDir, normal) > 0.0 ? rayDir : -rayDir;
            
            vec3 marchPos = fragPos;
            for (int j = 0; j < maxSteps; ++j) {
                marchPos += rayDir * stepSize;
    
                // Screen space
                vec4 screenPos = frame.camera.view_proj * vec4(marchPos, 1.0);
                screenPos.xyz /= screenPos.w;
                vec2 sampleUV = screenPos.xy * 0.5 + 0.5;
    
                // Get the depth
                float sceneDepth = texture(depthTexture, sampleUV).x;
                vec3 scenePos = unproject(sampleUV, sceneDepth, inverse(frame.camera.view_proj));
                
                // Check if there is an occlusion
                if (scenePos.z < marchPos.z - 0.01 * stepSize) {
                    // Get the direct lightColor at this pos
                    vec3 bouncedLight = texture(hdrTexture, sampleUV).rgb;
                    indirectLights += bouncedLight;
                    break;
                }

                if (length(marchPos - fragPos) > maxDist){     
                    break;
                }
            }
        }
        
        return indirectLights / float(numSamples);
    }

    void main() {
        const mat4 invProj = inverse(frame.camera.view_proj);
        const ivec2 coord = ivec2(gl_FragCoord.xy);
        const vec2 uv = vec2(gl_FragCoord.xy) / vec2(window_size.inner);
        const float pixel_depth = texelFetch(in_depth, coord, 0).x;
        const vec3 pos = unproject(uv, pixel_depth, invProj);
        const vec3 normal = texelFetch(in_normal, coord, 0).xyz * 2.0 - 1.0;

        const vec3 direct_light = texelFetch(in_hdr, coord, 0).rgb;

        vec3 indirect_lights = rayMarchIndirect(pos, normal, in_depth, in_hdr);
        vec3 final_color = indirect_lights; // directlight * ao
    
        out_color = vec4(final_color, 1.0);
    }
