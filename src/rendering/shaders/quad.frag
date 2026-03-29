#version 450 core

layout(location = 0) in vec2 frag_uv;

layout(location = 0) out vec4 out_color;

// Descriptor set 0: Sampled image and sampler
layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

// Push constants for per-draw material properties
layout(push_constant) uniform PushConstants {
    layout(offset = 32) float opacity;      // 32 = 4 corners * 2 floats (8 bytes) + screen_size (8 bytes) = 16 bytes origin
    int blend_mode;      // 0=Alpha, 1=Add, 2=Multiply, 3=Screen
    float brightness;
    int _pad;
} pc;

void main() {
    // Sample texture
    vec4 tex_color = texture(tex_sampler, frag_uv);
    
    // Apply brightness
    tex_color.rgb *= pc.brightness;
    
    // Apply opacity
    tex_color.a *= pc.opacity;
    
    // Apply blend mode
    // Note: blend operations are better done in fixed-function blend state,
    // but we can do some basic modes here if needed
    // For now, just apply opacity/brightness
    
    out_color = tex_color;
}
