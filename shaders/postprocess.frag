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

    // 9-tap bloom: учитываем только «излишек» над порогом,
    // чтобы туман и общий фон не давали паразитную засветку.
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

    // Кинематографичная цветокоррекция
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, params.saturation);
    color = mix(color, vec3(1.0, 0.95, 0.85), luma * 0.2);
    color = mix(color, vec3(0.85, 0.90, 1.0), (1.0 - luma) * 0.2);

    // --- Контраст в духе Fumito Ueda: глубокие тени, чистые блики ---
    color = max(color, vec3(0.0));

    // Мягкий тонемаппинг: сжимаем только яркие участки,
    // оставляя тени и полутона без «размытия».
    vec3 mapped = color / (color + vec3(0.72));
    color = mix(color, mapped, 0.45);

    // S-образная кривая контраста (классический smoothstep-контраст).
    color = clamp(color, 0.0, 1.0);
    vec3 sCurve = color * color * (3.0 - 2.0 * color);
    color = mix(color, sCurve, 0.65);

    // Контрастная гамма: убирает «выстиранную» светло-серую пелену.
    color = pow(max(color, vec3(0.0)), vec3(1.18));

    // Дизеринг
    ivec2 pixelPos = ivec2(gl_FragCoord.xy) % 4;
    float dither = bayerMatrix[pixelPos.y * 4 + pixelPos.x] / 16.0 - 0.5;
    color += dither * (1.0 / 255.0) * params.ditherStrength;

    outColor = vec4(color, 1.0);
}