#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>

// ============================================================
//  PUSH CONSTANTS (Instanced fur)
//  sizeof = 72 байта (без GLM_FORCE_ALIGNED_GENTYPES).
//  GLSL-блок: mat4 model; uint furShellCount; float emissive;
// ============================================================
struct PushConstants {
    glm::mat4 model{};
    uint32_t  furShellCount = 1;
    float     emissive = 0.0f;
};

static_assert(sizeof(PushConstants) <= 128, "PushConstants must fit in maxPushConstantsSize (>=128)");
static_assert(offsetof(PushConstants, furShellCount) == 64, "furShellCount offset must match GLSL layout");
static_assert(offsetof(PushConstants, emissive) == 68, "emissive offset must match GLSL layout");

// ============================================================
//  SCENE UBO Ч std140, 256 байт
// ============================================================
struct SceneUBO {
    glm::mat4 viewProj;
    glm::mat4 view;
    glm::vec4 fogColorDensity;       // rgb = color, a = density
    glm::vec4 furParams;             // x=time, y=furLength, z=furDensity, w=slopeThreshold
    glm::vec4 sunDirection;          // xyz = normalized dir to sun
    glm::vec4 sunColor;              // rgb = sun color
    glm::vec4 skyAmbientColor;       // rgb = sky hemisphere ambient
    glm::vec4 groundAmbientColor;    // rgb = ground hemisphere ambient
    glm::vec4 pointLightPos;         // xyz = world pos, w = falloff radius
    glm::vec4 pointLightColor;       // rgb = tint * intensity
};
static_assert(sizeof(SceneUBO) == 256, "SceneUBO must be 256 bytes for std140 layout");

// ============================================================
//  POST-PROCESS (совпадает с postprocess.frag push constant)
// ============================================================
struct PostProcessParams {
    float bloomThreshold = 0.88f;
    float bloomIntensity = 0.45f;
    float saturation = 0.80f;
    float ditherStrength = 1.0f;
};

// ============================================================
//  SCENE OBJECT
// ============================================================
struct SceneObject {
    glm::vec3 position{};
    glm::vec3 scale{ 1.0f };
    bool      isRotating{ false };
    bool      useIdolMesh{ false };
    bool      isFurry{ false };
    float     emissive{ 0.0f };
};

// ============================================================
//  ENGINE SETTINGS Ч всЄ, что настраиваетс€ из ImGui
// ============================================================
struct EngineSettings {
    // --- UI ---
    bool showGui = false;

    // --- GPU selection ---
    int  selectedGpuIndex = 0;
    bool gpuChangeHint = false;

    // --- Fog ---
    bool      fogEnabled = true;
    float     fogDensity = 0.018f;
    glm::vec3 fogColor = glm::vec3(0.53f, 0.57f, 0.55f);

    // --- Post-FX ---
    PostProcessParams ppParams;

    // --- Fur / Grass ---
    bool  furEnabled = true;
    int   shellCount = 32;
    float furLength = 0.15f;
    float furDensity = 200.0f;
    float slopeThreshold = 0.45f;

    // --- Lighting ---
    float     sunAzimuth = 0.6435f;
    float     sunElevation = 0.55f;
    float     sunIntensity = 1.85f;
    glm::vec3 skyAmbient = glm::vec3(0.18f, 0.21f, 0.26f);
    glm::vec3 groundAmbient = glm::vec3(0.03f, 0.04f, 0.06f);

    // --- Point light ---
    bool      animatePointLight = true;
    float     pointLightIntensity = 1.0f;
    glm::vec3 pointLightColorBase = glm::vec3(1.4f, 0.65f, 0.2f);
    float     pointLightRadius = 8.0f;
    glm::vec3 pointLightPos = glm::vec3(3.5f, 0.5f, 0.0f);

    // --- Render size ---
    uint32_t renderWidth = 640;
    uint32_t renderHeight = 448;
};