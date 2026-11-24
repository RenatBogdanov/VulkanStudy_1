#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord; // <-- Добавлено

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} push;

void main() {
    gl_Position = push.mvp * vec4(inPosition, 1.0);
    fragTexCoord = inTexCoord; // <-- Передаем UV во фрагментный шейдер
    
    float diff = max(dot(normalize(inNormal), normalize(vec3(0.5, 1.0, 0.3))), 0.25);
    fragColor = inColor * diff;
}