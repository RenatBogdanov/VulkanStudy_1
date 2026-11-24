#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragViewPos;
layout(location = 3) flat out vec3 vFogColor;
layout(location = 4) flat out float vFogDensity;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 modelView;
    vec3 fogColor;
    float fogDensity;
} push;

void main() {
    gl_Position = push.mvp * vec4(inPosition, 1.0);
    fragViewPos = (push.modelView * vec4(inPosition, 1.0)).xyz;
    fragTexCoord = inTexCoord;
    vFogColor = push.fogColor;
    vFogDensity = push.fogDensity;
    
    // Тёплый свет сверху, холодный снизу (стиль Fumito Ueda)
    float diffTop = max(dot(normalize(inNormal), normalize(vec3(0.5, 1.0, 0.3))), 0.0);
    float diffBottom = max(dot(normalize(inNormal), normalize(vec3(-0.5, -0.5, 0.0))), 0.0);
    vec3 warmLight = vec3(1.0, 0.95, 0.85);
    vec3 coolLight = vec3(0.6, 0.7, 0.9);
    
    vec3 finalColor = inColor * (warmLight * diffTop + coolLight * diffBottom * 0.4 + 0.2);
    fragColor = finalColor;
}