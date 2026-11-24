#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragViewPos;
layout(location = 3) flat in vec3 vFogColor;
layout(location = 4) flat in float vFogDensity;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 litColor = texColor.rgb * fragColor;
    
    float dist = length(fragViewPos);
    float fogFactor = clamp(exp(-dist * vFogDensity), 0.0, 1.0);
    vec3 finalColor = mix(vFogColor, litColor, fogFactor);
    outColor = vec4(finalColor, 1.0);
}