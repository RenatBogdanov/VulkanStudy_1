#pragma once

#include "RenderTypes.h"

#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;
class Window;
class Swapchain;

struct EditorActions {
    bool requestRecreateSwapchain = false;
    bool requestRecreateOffscreen = false;
};

class EditorUI {
public:
    EditorUI() = default;
    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;

    void init(VulkanContext& context, Window& window, Swapchain& swapchain);
    void cleanup(VkDevice device);

    void render(EngineSettings& settings,
        VulkanContext& context,
        Window& window,
        bool gamepadConnected,
        std::vector<SceneObject>& objects,
        EditorActions& actions);

private:
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    VkFormat         m_swapchainFormat = VK_FORMAT_UNDEFINED;
};