#version 450

// CAS-like sharpening fragment shader
// 单通道全屏三角形，输入 3D 渲染目标，输出锐化后的图像
// Push constants: strength, radius, threshold

layout(set = 0, binding = 0) uniform sampler2D u_3d_image;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SharpenParams {
    float strength;     // 0.0-1.0, 0 = off
    int   radius;       // 1-3, sample radius
    float threshold;    // 0.0-0.1, edge threshold to avoid ringing
    float _pad;         // alignment
    vec4  source_rect;  // x, y, width, height in source pixels
    vec4  destination_rect; // x, y, width, height in destination pixels
} params;

// Fullscreen triangle vertex positions (handled by vertex shader)
// We use a simple vertex shader that outputs a fullscreen triangle
// The vertex shader is minimal and we just need the fragment shader here

// CAS-style edge detection weight
float cas_weight(float a, float b, float threshold) {
    float diff = abs(a - b);
    if (diff < threshold) return 0.0;
    return diff;
}

float luminance(vec3 c);

vec3 cas_sample(vec2 uv, vec2 dir, float threshold) {
    vec3 c = texture(u_3d_image, uv).rgb;
    vec3 s0 = texture(u_3d_image, uv + dir).rgb;
    vec3 s1 = texture(u_3d_image, uv - dir).rgb;
    float w0 = cas_weight(luminance(c), luminance(s0), threshold);
    float w1 = cas_weight(luminance(c), luminance(s1), threshold);
    return clamp(c + (s0 - c) * w0 + (s1 - c) * w1, vec3(0.0), vec3(1.0));
}

float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    // Input UV from vertex shader (fullscreen triangle)
    vec2 output_uv = (gl_FragCoord.xy - params.destination_rect.xy) / params.destination_rect.zw;
    vec2 uv = (params.source_rect.xy + output_uv * params.source_rect.zw) / vec2(textureSize(u_3d_image, 0));
    
    if (params.strength <= 0.0) {
        out_color = texture(u_3d_image, uv);
        return;
    }
    
    
    // Each later cas_sample overwrote the previous result. Keep the last
    // effective direction for each radius and avoid the discarded samples.
    int r = clamp(params.radius, 1, 3);
    float thresh = clamp(params.threshold, 0.0, 0.1);
    vec2 texel = 1.0 / vec2(textureSize(u_3d_image, 0));
    vec2 dir = r == 1 ? vec2(0.0, 1.0) :
               r == 2 ? vec2(-1.0, 1.0) :
                        vec2(0.0, 2.0);
    vec3 sharp = cas_sample(uv, dir * texel, thresh);
    
    vec3 original = texture(u_3d_image, uv).rgb;
    vec3 result = clamp(mix(original, sharp, params.strength), vec3(0.0), vec3(1.0));
    
    out_color = vec4(result, 1.0);
}