#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in float fragFog;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 viewProj;
    mat4 view;
    vec4 fogColorDensity;
} ubo;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = mix(fragColor, ubo.fogColorDensity.rgb, fragFog);
    outColor = vec4(color, 1.0);
}