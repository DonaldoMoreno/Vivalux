#version 450 core

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv;

layout(location = 0) out vec2 frag_uv;

// Push constants for uniform data (smaller than UBO, preferred for per-draw-call data)
layout(push_constant) uniform PushConstants {
    vec2 corners[4];  // 0=TL, 1=TR, 2=BR, 3=BL
    vec2 screen_size;
} pc;

void main() {
    // Bilinear interpolation of quad corners
    // uv goes from [0,1] for a unit quad
    // We map it to the actual quad corner positions in screen space
    vec2 quad_corner = mix(mix(pc.corners[3], pc.corners[2], uv.x),
                           mix(pc.corners[0], pc.corners[1], uv.x), uv.y);
    
    // Convert from screen space to NDC (normalized device coordinates)
    vec2 ndc = (quad_corner / pc.screen_size) * 2.0 - 1.0;
    ndc.y = -ndc.y;  // Flip Y axis (screen Y goes down, NDC Y goes up)
    
    gl_Position = vec4(ndc, 0.0, 1.0);
    frag_uv = uv;
}
