#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D screenTexture;

layout(push_constant) uniform PushConstants {
    float bloomThreshold;
    float bloomIntensity;
    float saturation;
    float ditherStrength;
} params;

const float bayerMatrix[16] = float[](
    0.0,  8.0,  2.0, 10.0,
   12.0,  4.0, 14.0,  6.0,
    3.0, 11.0,  1.0,  9.0,
   15.0,  7.0, 13.0,  5.0
);

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(screenTexture, 0));
    vec3 color = texture(screenTexture, fragTexCoord).rgb;

    // 9-tap bloom: учитываем только «излишек» над порогом.
    vec3 bloom = vec3(0.0);
    float weightSum = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize * 2.5;
            vec3 sampleColor = texture(screenTexture, fragTexCoord + offset).rgb;
            float luma = dot(sampleColor, vec3(0.299, 0.587, 0.114));
            if (luma > params.bloomThreshold) {
                bloom += max(sampleColor - vec3(params.bloomThreshold), vec3(0.0));
                weightSum += 1.0;
            }
        }
    }
    if (weightSum > 0.0) {
        bloom /= weightSum;
    }
    color += bloom * params.bloomIntensity;

    // Кинематографичная цветокоррекция (saturation + tint без высветления чёрного).
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, params.saturation);
    color = mix(color, color * vec3(1.05, 0.98, 0.88), luma * 0.25);
    color = mix(color, color * vec3(0.88, 0.93, 1.05), (1.0 - luma) * 0.25);

    // === Экспозиция + контраст: режем паразитную серую дымку в тенях ===
    color = clamp((color - 0.03) * 1.2, 0.0, 1.0);

    // === Мягкий Reinhard-тонемаппинг: чистые блики, глубокий чёрный ===
    color = color / (1.0 + color * 0.25);

    // Дизеринг (в 8-битном sRGB-пространстве до конвертации в linear)
    ivec2 pixelPos = ivec2(gl_FragCoord.xy) % 4;
    float dither = bayerMatrix[pixelPos.y * 4 + pixelPos.x] / 16.0 - 0.5;
    color = clamp(color + dither * (1.0 / 255.0) * params.ditherStrength, 0.0, 1.0);

    // === Фикс двойной гамма-коррекции (0.1.8) ===
    // Swapchain (VK_FORMAT_B8G8R8A8_SRGB) автоматически применяет гамму 1/2.2 при записи.
    // Переводим итоговый цвет в линейное пространство, чтобы избежать двойного осветления.
    color = pow(color, vec3(2.2));

    outColor = vec4(color, 1.0);
}
