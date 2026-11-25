#include "EditorUI.h"
#include "VulkanContext.h"
#include "Window.h"
#include "Swapchain.h"
#include "Camera.h"

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
    Camera& camera,
    bool gamepadConnected,
    std::vector<SceneObject>& objects,
    EditorActions& actions) {

    // ============================================================
    //  F2: Stats Overlay
    // ============================================================
    if (settings.showStatsOverlay) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10, 10), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::Begin("Stats", &settings.showStatsOverlay,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
        ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Active GPU: %s", context.getCurrentGpuName().c_str());
        ImGui::Text("Render Res: %dx%d", settings.renderWidth, settings.renderHeight);
        ImGui::End();
    }

    // ============================================================
    //  F1: Main Settings Window
    // ============================================================
    if (settings.showMainSettings) {
        ImGui::SetNextWindowSize(ImVec2(450, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin("Settings", &settings.showMainSettings);

        if (ImGui::BeginTabBar("SettingsTabs")) {
            // --- Display Tab ---
            if (ImGui::BeginTabItem("Display")) {
                ImGui::Text("Active GPU: %s", context.getCurrentGpuName().c_str());
                const auto& gpus = context.getAvailableGpus();
                std::vector<const char*> gpuNames;
                for (const auto& gpu : gpus) gpuNames.push_back(gpu.name.c_str());
                if (!gpuNames.empty()) {
                    if (ImGui::Combo("GPU", &settings.selectedGpuIndex, gpuNames.data(), (int)gpuNames.size())) {
                        if (settings.selectedGpuIndex != (int)context.getCurrentGpuIndex()) {
                            context.saveGpuPreference(settings.selectedGpuIndex);
                            settings.gpuChangeHint = true;
                        }
                        else {
                            settings.gpuChangeHint = false;
                        }
                    }
                }
                if (settings.gpuChangeHint) {
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Restart application to apply GPU change");
                }

                int currentRes = 1;
                if (settings.renderWidth == 512)  currentRes = 0;
                else if (settings.renderWidth == 640)  currentRes = 1;
                else if (settings.renderWidth == 800)  currentRes = 2;
                else if (settings.renderWidth == 1280) currentRes = 3;
                else if (settings.renderWidth == 1920) currentRes = 4;

                const char* resolutions[] = {
                    "512x448 (PS2 NTSC Low)",
                    "640x448 (PS2 Standard)",
                    "800x448 (PS2 Widescreen)",
                    "1280x720 (HD Remaster)",
                    "1920x1080 (Full HD)"
                };
                if (ImGui::Combo("Internal Resolution", &currentRes, resolutions, IM_ARRAYSIZE(resolutions))) {
                    if (currentRes == 0) { settings.renderWidth = 512;  settings.renderHeight = 448; }
                    else if (currentRes == 1) { settings.renderWidth = 640;  settings.renderHeight = 448; }
                    else if (currentRes == 2) { settings.renderWidth = 800;  settings.renderHeight = 448; }
                    else if (currentRes == 3) { settings.renderWidth = 1280; settings.renderHeight = 720; }
                    else if (currentRes == 4) { settings.renderWidth = 1920; settings.renderHeight = 1080; }
                    actions.requestRecreateOffscreen = true;
                }

                bool isFullscreen = window.isFullscreen();
                if (ImGui::Checkbox("Fullscreen", &isFullscreen)) {
                    window.setFullscreen(isFullscreen);
                    actions.requestRecreateSwapchain = true;
                }

                ImGui::SliderFloat("Camera FOV", &settings.cameraFov, 45.0f, 95.0f);
                ImGui::EndTabItem();
            }

            // --- Atmosphere & FX Tab ---
            if (ImGui::BeginTabItem("Atmosphere & FX")) {
                ImGui::Text("Fog");
                ImGui::Checkbox("Enable Fog", &settings.fogEnabled);
                ImGui::SliderFloat("Fog Density", &settings.fogDensity, 0.0f, 0.1f);
                ImGui::ColorEdit3("Fog Color", glm::value_ptr(settings.fogColor));
                ImGui::Separator();
                ImGui::Text("Bloom & Color");
                ImGui::SliderFloat("Bloom Threshold", &settings.ppParams.bloomThreshold, 0.0f, 1.0f);
                ImGui::SliderFloat("Bloom Intensity", &settings.ppParams.bloomIntensity, 0.0f, 2.0f);
                ImGui::SliderFloat("Saturation", &settings.ppParams.saturation, 0.0f, 1.5f);
                ImGui::SliderFloat("Dither Strength", &settings.ppParams.ditherStrength, 0.0f, 2.0f);
                ImGui::EndTabItem();
            }

            // --- Lighting Tab ---
            if (ImGui::BeginTabItem("Lighting")) {
                ImGui::Text("Sun");
                ImGui::SliderFloat("Sun Azimuth", &settings.sunAzimuth, 0.0f, 6.2832f);
                ImGui::SliderFloat("Sun Elevation", &settings.sunElevation, 0.0f, 1.5708f);
                ImGui::SliderFloat("Sun Intensity", &settings.sunIntensity, 0.0f, 3.0f);
                ImGui::Separator();
                ImGui::Text("Ambient");
                ImGui::ColorEdit3("Sky Ambient", glm::value_ptr(settings.skyAmbient));
                ImGui::ColorEdit3("Ground Ambient", glm::value_ptr(settings.groundAmbient));
                ImGui::Separator();
                ImGui::Text("Dynamic Point Light");
                ImGui::Checkbox("Animate", &settings.animatePointLight);
                ImGui::SliderFloat("Intensity", &settings.pointLightIntensity, 0.0f, 3.0f);
                ImGui::SliderFloat("Radius", &settings.pointLightRadius, 1.0f, 20.0f);
                ImGui::ColorEdit3("Color", glm::value_ptr(settings.pointLightColorBase));
                ImGui::EndTabItem();
            }

            // --- Fur & Grass Tab ---
            if (ImGui::BeginTabItem("Fur & Grass")) {
                ImGui::Checkbox("Enable Fur", &settings.furEnabled);
                ImGui::SliderInt("Shell Count", &settings.shellCount, 4, 32);
                ImGui::SliderFloat("Fur Length", &settings.furLength, 0.02f, 0.5f);
                ImGui::SliderFloat("Fur Density", &settings.furDensity, 20.0f, 400.0f);
                ImGui::SliderFloat("Slope Threshold", &settings.slopeThreshold, -1.0f, 0.95f);
                ImGui::SliderFloat("Fur Gravity", &settings.furGravity, 0.0f, 2.0f);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::Separator();
        if (ImGui::Button("Save Settings")) actions.saveSettings = true;
        ImGui::SameLine();
        if (ImGui::Button("Reset Defaults")) actions.resetSettings = true;

        ImGui::End();
    }

    // ============================================================
    //  F3: Scene & Camera Window
    // ============================================================
    if (settings.showSceneWindow) {
        ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Scene & Camera", &settings.showSceneWindow);

        ImGui::Text("Camera");
        ImGui::SliderFloat("Move Speed", &settings.cameraSpeed, 1.0f, 20.0f);
        if (ImGui::Button("Reset Camera")) {
            camera.reset();
        }
        ImGui::Separator();

        ImGui::Text("Scene Objects");
        for (size_t i = 0; i < objects.size(); ++i) {
            auto& obj = objects[i];
            ImGui::PushID((int)i);
            if (ImGui::TreeNode("Object", "Object %d", (int)i)) {
                if (ImGui::Button("Select")) settings.selectedObjectIndex = (int)i;
                ImGui::DragFloat3("Position", glm::value_ptr(obj.position), 0.05f);
                ImGui::DragFloat3("Scale", glm::value_ptr(obj.scale), 0.05f, 0.1f, 20.0f);
                ImGui::Checkbox("Is Furry", &obj.isFurry);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::End();
    }
}