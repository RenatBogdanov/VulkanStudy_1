#version 450

// Порядок как в Vertex::getAttributeDescriptions(): 0 pos, 1 color, 2 normal, 3 texCoord.
layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

// Должно совпадать с WormInstanceData (WormSwarmSystem.h), 64 байта.
struct WormData {
    vec3 position;
    float wiggle;          // амплитуда волны [0..1], считается на CPU по скорости
    vec3 velocity;
    float padding2;
    vec3 surfaceNormal;    // единичная
    float phase;           // фаза бегущей волны, растёт со скоростью
    float scale;
    float padding3[3];
};

layout(std430, binding = 1) readonly buffer WormBuffer {
    WormData worms[];
} wormBuffer;

layout(set = 0, binding = 0) uniform SceneUBO {
    mat4 viewProj;
    mat4 view;
    vec4 fogColorDensity;
    vec4 furParams;
    vec4 furGravity;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 skyAmbientColor;
    vec4 groundAmbientColor;
    vec4 pointLightPos;
    vec4 pointLightColor;
} ubo;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out float fragFog;

const float MIN_LENGTH_SQ = 1e-8;
const float WAVE_FREQ = 9.42478;       // 1.5 волны на длину тела (2π * 1.5)
const float WAVE_MAX_OFFSET = 0.2;     // боковое смещение при wiggle = 1 (в длинах тела)
const float WAVE_TAIL_GAIN = 0.35;     // у головы амплитуда меньше, чем у хвоста
const float RING_COUNT = 12.0;         // = WORM_SEGMENTS в Mesh.cpp
const float RING_DARKEN = 0.18;        // затемнение чётных колец (эффект сегментов)
const vec3  COLOR_THIN = vec3(0.75, 0.18, 0.14);
const vec3  COLOR_THICK = vec3(0.95, 0.40, 0.28);

void main() {
    WormData w = wormBuffer.worms[gl_InstanceIndex];

    // Ортонормальный правосторонний базис: up — нормаль поверхности,
    // forward — скорость, спроецированная на касательную плоскость.
    vec3 up = w.surfaceNormal;
    vec3 fwd = w.velocity - up * dot(w.velocity, up);
    float fwdLenSq = dot(fwd, fwd);
    if (fwdLenSq > MIN_LENGTH_SQ) {
        fwd *= inversesqrt(fwdLenSq);
    } else {
        vec3 ref = abs(up.x) < 0.9 ? vec3(1.0, 0.0, 0.0) : vec3(0.0, 0.0, 1.0);
        fwd = normalize(cross(up, ref));
    }
    vec3 right = cross(up, fwd);

    // Бегущая волна изгиба от головы к хвосту (t: 0 — хвост, 1 — голова).
    float t = inTexCoord.y;
    float envelope = mix(1.0, WAVE_TAIL_GAIN, t);
    float lateral = sin(inPosition.z * WAVE_FREQ + w.phase) * w.wiggle * WAVE_MAX_OFFSET * envelope;

    vec3 localPos = inPosition;
    localPos.x += lateral;
    localPos *= w.scale;

    vec3 worldPos = w.position + right * localPos.x + up * localPos.y + fwd * localPos.z;
    // Базис ортонормален → нормаль остаётся единичной без normalize.
    vec3 worldNormal = right * inNormal.x + up * inNormal.y + fwd * inNormal.z;

    gl_Position = ubo.viewProj * vec4(worldPos, 1.0);

    // Гуро: освещение по вершинам + чередующиеся кольца для «сегментов».
    float ndl = max(dot(worldNormal, ubo.sunDirection.xyz), 0.0);
    float ring = floor(t * RING_COUNT + 0.5);
    float band = 1.0 - RING_DARKEN * mod(ring, 2.0);
    vec3 baseColor = mix(COLOR_THIN, COLOR_THICK, clamp((w.scale - 0.5) * 2.0, 0.0, 1.0));
    fragColor = baseColor * band * (0.35 + 0.65 * ndl);

    float viewDist = length((ubo.view * vec4(worldPos, 1.0)).xyz);
    fragFog = clamp(1.0 - exp(-viewDist * ubo.fogColorDensity.w), 0.0, 1.0);
}