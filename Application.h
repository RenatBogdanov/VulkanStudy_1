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

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

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

    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    void recreateSwapchain();
    bool framebufferIsZero() const;
    void waitForFramebuffer();

    void updatePointLightAnimation(float time);
    std::vector<SceneObject> buildSceneObjects() const;

    Window           m_window;
    VulkanContext    m_context;
    Swapchain        m_swapchain;
    Camera           m_camera;
    Gamepad          m_gamepad;
    Mesh             m_idolMesh;
    Mesh             m_cubeMesh;
    Texture          m_texture;
    OffscreenRenderer m_offscreenRenderer;
    EditorUI         m_editorUI;

    EngineSettings   m_settings;

    VkCommandPool                m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore>     m_imageAvailableSemaphores;
    std::vector<VkFence>         m_inFlightFences;

    float    m_lastFrameTime = 0.0f;
    bool     m_firstMouse = true;
    double   m_lastMouseX = 0.0;
    double   m_lastMouseY = 0.0;
    uint32_t m_currentFrame = 0;
    bool     m_f1Pressed = false;
};