#pragma once

#include "Camera.h"
#include "EditorUI.h"
#include "Gamepad.h"
#include "Mesh.h"
#include "OffscreenRenderer.h"
#include "RenderTypes.h"
#include "Swapchain.h"
#include "Texture.h"
#include "VulkanContext.h"
#include "Window.h"
#include "WormSwarmSystem.h"  // <-- днаюбкемн

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

#include <string>
#include <vector>

class Application {
public:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void run();

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    void initVulkan();
    void mainLoop();
    void drawFrame(float deltaTime);
    void cleanup();

    void initDefaultScene();
    void loadScene(const std::string& path);
    void saveScene(const std::string& path) const;

    void loadSettings();
    void saveSettings();

    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    void recreateSwapchain();
    bool framebufferIsZero() const;
    void waitForFramebuffer();

    void updatePointLightAnimation(float time);
    void handleEditorActions(const EditorActions& actions);

    Window            m_window;
    VulkanContext     m_context;
    Swapchain         m_swapchain;
    Camera            m_camera;
    Gamepad           m_gamepad;

    Mesh              m_idolMesh;
    Mesh              m_cubeMesh;
    Mesh              m_waterfallMesh;  // <-- днаюбкемн
    Mesh              m_wormMesh;       // <-- днаюбкемн
    Texture           m_texture;

    OffscreenRenderer m_offscreenRenderer;
    EditorUI          m_editorUI;
    WormSwarmSystem   m_wormSwarm;      // <-- днаюбкемн

    EngineSettings    m_settings;
    std::vector<SceneObject> m_sceneObjects;

    VkCommandPool                m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore>     m_imageAvailableSemaphores;
    std::vector<VkFence>         m_inFlightFences;

    float    m_lastFrameTime = 0.0f;
    bool     m_firstMouse = true;
    double   m_lastMouseX = 0.0;
    double   m_lastMouseY = 0.0;
    uint32_t m_currentFrame = 0;
};