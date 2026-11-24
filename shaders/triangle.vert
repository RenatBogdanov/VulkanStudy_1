#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out vec3 fragViewPos;
layout(location = 3) flat out float vFurLayer;
layout(location = 4) out float vSlopeMask;

layout(binding = 1) uniform SceneUBO {
    mat4 viewProj;
    mat4 view;
    vec4 fogColorDensity;      // rgb = fog color, a = density
    vec4 furParams;            // x = time, y = furLength, z = furDensity, w = slopeThreshold
    vec4 sunDirection;         // xyz = normalized direction TO sun, w unused
    vec4 sunColor;             // rgb = sun color, a unused
    vec4 skyAmbientColor;      // rgb = sky (top) hemisphere ambient
    vec4 groundAmbientColor;   // rgb = ground (bottom) hemisphere ambient
    vec4 pointLightPos;        // xyz = world position, w = falloff radius
    vec4 pointLightColor;      // rgb = tint * intensity
} scene;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint furShellCount;        // 1 = обычный меш, >1 = Instanced fur
    float emissive;            // 0 = normal lighting, > 0 = force color = pointLightColor * emissive
} push;

void main() {
    mat3 normalMatrix = mat3(transpose(inverse(push.model)));
    vec3 worldNormal  = normalize(normalMatrix * inNormal);
    float upFactor    = dot(worldNormal, vec3(0.0, 1.0, 0.0));

    float slopeMask = smoothstep(scene.furParams.w, scene.furParams.w + 0.25, upFactor);

    // === Instanced fur ===
    // Instance 0 -> furLayer = 0 (базовый непрозрачный меш, БЕЗ смещения и БЕЗ discard).
    // Instance N-1 -> furLayer = 1 (верхний слой меха).
    // При furShellCount == 1 весь объект — базовый меш.
    float furLayer = 0.0;
    if (push.furShellCount > 1u) {
        furLayer = float(gl_InstanceIndex) / (float(push.furShellCount) - 1.0);
    }

    vec3 displacedPos = inPosition;
    if (furLayer > 0.001) {
        displacedPos += normalize(inNormal) * (furLayer * scene.furParams.y * slopeMask);
        float wind = sin(scene.furParams.x * 2.5 + inPosition.x * 4.0 + inPosition.z * 3.0)
                   * 0.02 * furLayer * furLayer * slopeMask;
        displacedPos.x += wind;
        displacedPos.z += wind * 0.5;
    }

    vec4 worldPos = push.model * vec4(displacedPos, 1.0);
    gl_Position   = scene.viewProj * worldPos;
    fragViewPos   = (scene.view * worldPos).xyz;
    fragTexCoord  = inTexCoord;
    vFurLayer     = furLayer;
    vSlopeMask    = slopeMask;

    // ===================== GOURAUD LIGHTING =====================
    float hemi = worldNormal.y * 0.5 + 0.5;
    vec3 ambient = mix(scene.groundAmbientColor.rgb, scene.skyAmbientColor.rgb, hemi);

    vec3 sunDir = normalize(scene.sunDirection.xyz);
    float sunDiff = max(dot(worldNormal, sunDir), 0.0);
    vec3 sunLight = scene.sunColor.rgb * sunDiff;

    vec3 toLight = scene.pointLightPos.xyz - worldPos.xyz;
    float d      = length(toLight);
    vec3 L       = toLight / max(d, 0.001);
    float pDiff  = max(dot(worldNormal, L), 0.0);
    float r      = scene.pointLightPos.w;
    float atten  = 1.0 / (1.0 + (d * d) / (r * r));
    vec3 pointLight = scene.pointLightColor.rgb * pDiff * atten;

    vec3 litColor = inColor * (ambient + sunLight + pointLight);

    if (push.emissive > 0.001) {
        litColor = scene.pointLightColor.rgb * push.emissive;
    }

    fragColor = litColor;
}