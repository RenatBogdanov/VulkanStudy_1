#include "EditorUI.h"
#include "VulkanContext.h"
#include "Window.h"
#include "Swapchain.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <glm/gtc/type_ptr.hpp>

#include <vector>

void EditorUI::init(VulkanContext& context, Window& window, Swapchain& swapchain) {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 }
    };
    VkDescriptorPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;
    VK_CHECK(vkCreateDescriptorPool(context.getDevice(), &pool_info, nullptr, &m_imguiDescriptorPool));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForVulkan(window.getNativeWindow(), true);

    m_swapchainFormat = swapchain.getImageFormat();

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = context.getInstance();
    init_info.PhysicalDevice = context.getPhysicalDevice();
    init_info.Device = context.getDevice();
    init_info.QueueFamily = context.findQueueFamilies().graphicsFamily.value();
    init_info.Queue = context.getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_imguiDescriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = static_cast<uint32_t>(swapchain.getImages().size());
    init_info.UseDynamicRendering = true;

    VkPipelineRenderingCreateInfo pipelineRenderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &m_swapchainFormat;

    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

    ImGui_ImplVulkan_Init(&init_info);
}

void EditorUI::cleanup(VkDevice device) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (m_imguiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
}

void EditorUI::render(EngineSettings& settings,
    VulkanContext& context,
    Window& window,
    bool gamepadConnected,
    std::vector<SceneObject>& objects,
    EditorActions& actions) {
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("PS2 Atmosphere & Post-FX", &settings.showGui);
    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::Text("Gamepad: %s", gamepadConnected ? "Connected" : "Disconnected");
    ImGui::Separator();

    // --- Display & Resolution ---
    ImGui::Text("Display & PS2 Resolution");
    bool isFullscreen = window.isFullscreen();
    if (ImGui::Checkbox("Fullscreen", &isFullscreen)) {
        window.setFullscreen(isFullscreen);
        actions.requestRecreateSwapchain = true;
    }

    // --- GPU selection ---
    ImGui::Text("Active GPU: %s", context.getCurrentGpuName().c_str());

    const auto& gpus = context.getAvailableGpus();
    std::vector<const char*> gpuNames;
    gpuNames.reserve(gpus.size());
    for (const auto& gpu : gpus) gpuNames.push_back(gpu.name.c_str());

    if (!gpuNames.empty()) {
        if (ImGui::Combo("GPU", &settings.selectedGpuIndex, gpuNames.data(),
            static_cast<int>(gpuNames.size()))) {
            if (settings.selectedGpuIndex != static_cast<int>(context.getCurrentGpuIndex())) {
                context.saveGpuPreference(static_cast<uint32_t>(settings.selectedGpuIndex));
                settings.gpuChangeHint = true;
            }
            else {
                settings.gpuChangeHint = false;
            }
        }
    }
    if (settings.gpuChangeHint) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
            "Restart application to apply GPU change");
    }

    // --- Internal resolution ---
    int currentRes = 1;
    if (settings.renderWidth == 512)  currentRes = 0;
    else if (settings.renderWidth == 640)  currentRes = 1;
    else if (settings.renderWidth == 800)  currentRes = 2;
    else if (settings.renderWidth == 1280) currentRes = 3;

    const char* resolutions[] = {
        "512x448 (PS2 NTSC Low)",
        "640x448 (PS2 Standard)",
        "800x448 (PS2 Widescreen 16:9)",
        "1280x720 (HD Remaster)"
    };
    if (ImGui::Combo("Internal Resolution", &currentRes, resolutions, IM_ARRAYSIZE(resolutions))) {
        if (currentRes == 0) { settings.renderWidth = 512;  settings.renderHeight = 448; }
        else if (currentRes == 1) { settings.renderWidth = 640;  settings.renderHeight = 448; }
        else if (currentRes == 2) { settings.renderWidth = 800;  settings.renderHeight = 448; }
        else if (currentRes == 3) { settings.renderWidth = 1280; settings.renderHeight = 720; }
        actions.requestRecreateOffscreen = true;
    }
    ImGui::Separator();

    // --- Fog ---
    ImGui::Checkbox("Enable Fog", &settings.fogEnabled);
    ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, 0.1f);
    ImGui::ColorEdit3("Fog Color", glm::value_ptr(settings.fogColor));

    // --- Post-FX ---
    ImGui::Separator();
    ImGui::Text("Post-FX");
    ImGui::SliderFloat("Bloom Threshold", &settings.ppParams.bloomThreshold, 0.0f, 1.0f);
    ImGui::SliderFloat("Bloom Intensity", &settings.ppParams.bloomIntensity, 0.0f, 2.0f);
    ImGui::SliderFloat("Saturation", &settings.ppParams.saturation, 0.0f, 1.5f);
    ImGui::SliderFloat("Dither Strength", &settings.ppParams.ditherStrength, 0.0f, 2.0f);
    ImGui::Separator();

    // --- Fur / Grass ---
    ImGui::Text("Colossus Fur / Grass (Shells, Instanced)");
    ImGui::Checkbox("Enable Fur", &settings.furEnabled);
    ImGui::SliderInt("Shell Count", &settings.shellCount, 4, 32);
    ImGui::SliderFloat("Fur Length", &settings.furLength, 0.02f, 0.5f);
    ImGui::SliderFloat("Fur Density", &settings.furDensity, 20.0f, 400.0f);
    ImGui::SliderFloat("Slope Threshold", &settings.slopeThreshold, -1.0f, 0.95f);
    ImGui::SliderFloat("Fur Gravity", &settings.furGravity, 0.0f, 2.0f); // <-- Новый слайдер

    // --- Lighting ---
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Lighting (Gouraud + Dynamic)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Sun Azimuth", &settings.sunAzimuth, 0.0f, 6.2832f);
        ImGui::SliderFloat("Sun Elevation", &settings.sunElevation, 0.0f, 1.5708f);
        ImGui::SliderFloat("Sun Intensity", &settings.sunIntensity, 0.0f, 3.0f);
        ImGui::ColorEdit3("Sky Ambient", glm::value_ptr(settings.skyAmbient));
        ImGui::ColorEdit3("Ground Shadow", glm::value_ptr(settings.groundAmbient));
        ImGui::Separator();
        ImGui::Checkbox("Animate Point Light", &settings.animatePointLight);
        ImGui::SliderFloat("Point Light Intensity", &settings.pointLightIntensity, 0.0f, 3.0f);
        ImGui::ColorEdit3("Point Light Color", glm::value_ptr(settings.pointLightColorBase));
        if (!settings.animatePointLight) {
            ImGui::SliderFloat3("Point Light Pos", glm::value_ptr(settings.pointLightPos), -10.0f, 10.0f);
        }
    }

    // --- Selected Object Inspector ---
    if (settings.selectedObjectIndex >= 0 && settings.selectedObjectIndex < static_cast<int>(objects.size())) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Selected Object (Index %d)", settings.selectedObjectIndex);
        auto& obj = objects[settings.selectedObjectIndex];
        ImGui::DragFloat3("Position", glm::value_ptr(obj.position), 0.05f);
        ImGui::DragFloat3("Scale", glm::value_ptr(obj.scale), 0.05f, 0.1f, 20.0f);
        ImGui::Checkbox("Is Furry", &obj.isFurry);
        ImGui::Checkbox("Rotate", &obj.isRotating);

        if (ImGui::Button("Deselect")) {
            settings.selectedObjectIndex = -1;
        }
    }

    ImGui::End();
}