    #version 450

    #include "utils.glsl"

    layout(location = 0) out vec4 out_color;
    
    layout(location = 0) in vec2 in_uv;

    layout(binding = 0) uniform sampler2D in_indirect_light_texture;
    layout(binding = 0) uniform sampler2D in_direct_light_texture;
    layout(binding = 2) uniform sampler2D in_depth;
    layout(binding = 3) uniform Data {
        FrameData frame;
    };

    vec3 bilateralBlur(vec2 uv, vec2 texelSize) {
        vec3 result = vec3(0.0);
        float weightSum = 0.0;
        
        for (int i = -2; i <= 2; i++) {
            for (int j = -2; j <= 2; j++) {
                vec2 offset = vec2(i, j) * texelSize;
                float sampleDepth = texture(in_depth, uv + offset).x;
                float weight = exp(-length(offset) * 10.0);
                
                result += texture(in_indirect_light_texture, uv + offset).rgb * weight;
                weightSum += weight;
            }
        }
        return result / max(weightSum, 0.0001);
    }

    void main() {
        const ivec2 coord = ivec2(gl_FragCoord.xy);

        const vec3 direct_light = texelFetch(in_direct_light_texture, coord, 0).rgb;

        vec2 texelSize = 1.0 / vec2(textureSize(in_indirect_light_texture, 0));
        const vec3 indirect_light = texelFetch(in_direct_light_texture, coord, 0).rgb;
    
        out_color = vec4(direct_light + indirect_light * 2.0, 1.0);
    }
