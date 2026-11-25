#pragma once

#include "RenderTypes.h"
#include <vulkan/vulkan.h>
#include <vector>

class VulkanContext;
class Window;
class Swapchain;
class Camera;
class WormSwarmSystem;

struct EditorActions {
    bool requestRecreateOffscreen = false;
    bool requestRecreateSwapchain = false;
    bool saveSettings = false;
    bool resetSettings = false;

    bool addIdol = false;
    bool addPillar = false;
    bool addWaterfall = false;
    bool saveScene = false;
    bool loadScene = false;
    bool duplicateObject = false;
    bool deleteObject = false;

    bool rebuildWormGrid = false;
};

class EditorUI {
public:
    void init(VulkanContext& context, Window& window, Swapchain& swapchain);
    void cleanup(VkDevice device);

    void render(EngineSettings& settings,
        VulkanContext& context,
        Window& window,
        Camera& camera,
        bool gamepadConnected,
        std::vector<SceneObject>& objects,
        WormSwarmSystem& wormSwarm, // edited via WormTuning in Worms tab
        EditorActions& actions);

private:
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    bool m_isInitialized = false;
};