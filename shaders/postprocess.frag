#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D screenTexture;

// Матрица Байера 4x4 для упорядоченного дизеринга (PS2 стиль)
const float bayerMatrix[16] = float[](
    0.0,  8.0,  2.0, 10.0,
   12.0,  4.0, 14.0,  6.0,
    3.0, 11.0,  1.0,  9.0,
   15.0,  7.0, 13.0,  5.0
);

void main() {
    vec3 color = texture(screenTexture, fragTexCoord).rgb;

    // Кинематографичная цветокоррекция (Shadow of the Colossus)
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, 0.8); // Снижение насыщенности на 20%
    color = mix(color, vec3(1.0, 0.95, 0.85), luma * 0.2); // Тёплые света
    color = mix(color, vec3(0.85, 0.90, 1.0), (1.0 - luma) * 0.2); // Холодные тени

    // 16-битный упорядоченный дизеринг
    ivec2 pixelPos = ivec2(gl_FragCoord.xy) % 4;
    float dither = bayerMatrix[pixelPos.y * 4 + pixelPos.x] / 16.0 - 0.5;
    color += dither * (1.0 / 255.0);

    outColor = vec4(color, 1.0);
}