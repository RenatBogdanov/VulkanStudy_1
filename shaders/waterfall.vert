#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor; // R-канал = альфа
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint furShellCount;
    float emissive;
    float waterSpeed;
    float waterFoam;
} pc;

layout(set = 0, binding = 1) uniform SceneUBO {
    mat4 viewProj;
    mat4 view;
    vec4 fogColorDensity;
    vec4 furParams;       // x = time
    vec4 furGravity;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 skyAmbientColor;
    vec4 groundAmbientColor;
    vec4 pointLightPos;
    vec4 pointLightColor;
} ubo;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out float fragFog;
layout(location = 3) out float fragAlpha;
layout(location = 4) out float fragHeight; // NEW: высота для градиента

void main() {
    vec3 pos = inPosition;
    // Лёгкое покачивание (можно убрать, если не нужно)
    pos.x += sin(pos.y * 1.5 + ubo.furParams.x * 2.0) * 0.03;

    vec4 worldPos = pc.model * vec4(pos, 1.0);
    gl_Position = ubo.viewProj * worldPos;

    // Освещение по Гуро
    vec3 normal = normalize(mat3(pc.model) * inNormal);
    float NdotL = max(dot(normal, ubo.sunDirection.xyz), 0.0);
    vec3 ambient = mix(ubo.groundAmbientColor.rgb, ubo.skyAmbientColor.rgb, normal.y * 0.5 + 0.5);
    vec3 diffuse = ubo.sunColor.rgb * NdotL;

    fragColor = (ambient + diffuse) * 0.5 + 0.5; // Вода всегда светлая
    fragAlpha = inColor.r;
    fragTexCoord = inTexCoord;
    fragHeight = pos.y; // Передаём локальную Y (0 — верх, -height — низ)

    // Туман
    float dist = length(ubo.view[3].xyz - worldPos.xyz);
    fragFog = clamp((dist - 5.0) * ubo.fogColorDensity.a, 0.0, 1.0);
}