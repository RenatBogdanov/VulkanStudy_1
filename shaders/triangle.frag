#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragViewPos;
layout(location = 3) flat in float vFurLayer;
layout(location = 4) in float vSlopeMask;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

layout(binding = 1) uniform SceneUBO {
    mat4 viewProj;
    mat4 view;
    vec4 fogColorDensity;
    vec4 furParams;
    vec4 sunDirection;
    vec4 sunColor;
    vec4 skyAmbientColor;
    vec4 groundAmbientColor;
    vec4 pointLightPos;
    vec4 pointLightColor;
} scene;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    if (vFurLayer > 0.001) {
        if (vSlopeMask <= 0.01) {
            discard;
        }
        vec2 cellCoord = fragTexCoord * scene.furParams.z;
        vec2 cellID    = floor(cellCoord);
        vec2 cellUV    = fract(cellCoord) - 0.5;

        float nExist = hash(cellID);
        float existThreshold = vFurLayer * 0.92 / max(vSlopeMask, 0.01);
        if (nExist < existThreshold) {
            discard;
        }

        float r1 = hash(cellID + vec2(17.0,  0.0));
        float r2 = hash(cellID + vec2( 0.0, 31.0));
        vec2  jitter = (vec2(r1, r2) - 0.5) * 0.6;
        vec2  local  = cellUV - jitter;

        float bladeRadius = mix(0.45, 0.15, pow(vFurLayer, 1.5));
        float d = length(local);

        float edge = smoothstep(bladeRadius, bladeRadius * 0.6, d);
        if (hash(gl_FragCoord.xy) > edge) {
            discard;
        }
    }

    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 litColor = texColor.rgb * fragColor;

    if (vFurLayer > 0.001) {
        vec3 furColor = vec3(0.24, 0.30, 0.19);
        float shadow  = mix(0.35, 1.15, vFurLayer);
        litColor = furColor * shadow * fragColor;
    }

    float dist = length(fragViewPos);
    float fogFactor = clamp(exp(-dist * scene.fogColorDensity.a), 0.0, 1.0);
    vec3 finalColor = mix(scene.fogColorDensity.rgb, litColor, fogFactor);
    outColor = vec4(finalColor, 1.0);
}