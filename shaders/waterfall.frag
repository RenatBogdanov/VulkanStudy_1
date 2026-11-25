#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in float fragFog;
layout(location = 3) in float fragAlpha;
layout(location = 4) in float fragHeight; // Y-координата

layout(set = 0, binding = 0) uniform sampler2D texSampler;

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

layout(location = 0) out vec4 outColor;

void main() {
    float time = ubo.furParams.x;
    vec2 uv = fragTexCoord;

    // 1. Две медленные волны (низкая частота — нет мерцания)
    float wave1 = sin(uv.y * 8.0 + time * pc.waterSpeed * 1.5 + uv.x * 3.0);
    float wave2 = sin(uv.y * 5.0 - time * pc.waterSpeed * 1.0 + uv.x * 5.0);
    float wave = (wave1 + wave2) * 0.5; // -1..1

    // 2. Текстура камня, растянутая вертикально (создаёт "нити" воды)
    float texLine = texture(texSampler, uv * vec2(1.0, 0.5) + vec2(0.0, time * pc.waterSpeed)).r;

    // 3. Комбинируем: волны задают форму, текстура — мелкую детализацию
    float pattern = wave * 0.7 + texLine * 0.3;
    // Сглаживаем, чтобы убрать резкие переходы
    pattern = smoothstep(-0.4, 0.8, pattern);

    // 4. Цветовой градиент по высоте: сверху светлее, снизу темнее
    // fragHeight: 0 (верх) .. -8 (низ) — нормализуем
    float heightFactor = clamp(fragHeight / -8.0, 0.0, 1.0); // 0 = верх, 1 = низ
    vec3 topColor    = vec3(0.4, 0.7, 0.9);  // Светло-голубая вода
    vec3 bottomColor = vec3(0.05, 0.15, 0.3); // Тёмно-синяя глубина
    vec3 waterColor  = mix(topColor, bottomColor, heightFactor);

    // 5. Пена — ярко-белая, появляется на гребнях волн (где pattern высокий)
    float foamAmount = smoothstep(0.6, 1.0, pattern * pc.waterFoam);
    vec3 foamColor = vec3(1.0, 1.0, 1.0);
    vec3 finalColor = mix(waterColor, foamColor, foamAmount);

    // 6. Освещение и туман
    finalColor *= fragColor;
    finalColor = mix(finalColor, ubo.fogColorDensity.rgb, fragFog);

    // 7. Альфа: края прозрачные, центр плотный, пена плотнее
    float edgeFade = sin(uv.x * 3.14159); // 0 по краям, 1 в центре
    float alpha = fragAlpha * edgeFade * (0.5 + 0.5 * foamAmount);

    outColor = vec4(finalColor, alpha);
}