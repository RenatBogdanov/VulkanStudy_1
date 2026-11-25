#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

// ============================================================
//  PUSH CONSTANTS (Instanced fur)
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
//  SCENE UBO Ч std140, 272 bytes
// ============================================================
struct SceneUBO {
    glm::mat4 viewProj;              // offset   0
    glm::mat4 view;                  // offset  64
    glm::vec4 fogColorDensity;       // offset 128
    glm::vec4 furParams;             // offset 144
    glm::vec4 furGravity;            // offset 160
    glm::vec4 sunDirection;          // offset 176
    glm::vec4 sunColor;              // offset 192
    glm::vec4 skyAmbientColor;       // offset 208
    glm::vec4 groundAmbientColor;    // offset 224
    glm::vec4 pointLightPos;         // offset 240
    glm::vec4 pointLightColor;       // offset 256
};
static_assert(sizeof(SceneUBO) == 272, "SceneUBO must be 272 bytes for std140 layout");

// ============================================================
//  POST-PROCESS
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
    std::string name = "Object";
    glm::vec3   position{};
    glm::vec3   rotation{ 0.0f };      // Euler angles in degrees (XYZ order)
    glm::vec3   scale{ 1.0f };
    bool        isRotating{ false };
    bool        useIdolMesh{ false };
    bool        isFurry{ false };
    bool        isLightMarker{ false }; // animated point-light proxy, non-editable
    float       emissive{ 0.0f };

    // јвторитетна€ сборка матрицы модели (T * Rz * Ry * Rx * S Ч XYZ Euler).
    // »спользуетс€ и в OffscreenRenderer, и в ImGuizmo, чтобы гизмо точно совпадало с мешем.
    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, scale);
        return model;
    }
};

// ============================================================
//  ENGINE SETTINGS
// ============================================================
struct EngineSettings {
    // --- Editor UI ---
    bool showMainSettings = false;
    bool showStatsOverlay = true;
    bool showSceneWindow = false;

    // --- Camera ---
    float cameraFov = 45.0f;
    float cameraSpeed = 5.0f;

    // --- Selection / Gizmo ---
    int hoveredObjectIndex = -1;
    int selectedObjectIndex = -1;
    int gizmoOperation = 0;   // 0=Translate, 1=Rotate, 2=Scale

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
    float furGravity = 0.65f;

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