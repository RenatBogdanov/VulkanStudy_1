#include "Application.h"
#include "VulkanUtils.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

static const uint32_t WIDTH = 800;
static const uint32_t HEIGHT = 600;

// ============================================================
//  RUN / INIT / CLEANUP
// ============================================================
void Application::run() {
    m_window.init(WIDTH, HEIGHT, "Vulkan PS2 Scene");
    try {
        initVulkan();
        mainLoop();
    }
    catch (...) {
        cleanup();
        throw;
    }
    cleanup();
}

void Application::initVulkan() {
    loadSettings();
    m_context.init(m_window);
    m_settings.selectedGpuIndex = static_cast<int>(m_context.getCurrentGpuIndex());

    m_swapchain.init(m_context, m_window);

    createCommandPool();
    createCommandBuffers();

    m_cubeMesh.createProceduralCube(m_context, m_commandPool);
    m_idolMesh.loadFromObj(m_context, m_commandPool, "models/idol.obj");
    m_texture.init(m_context, m_commandPool, "textures/stone.png");

    m_offscreenRenderer.init(m_context,
        m_swapchain.getImageFormat(),
        m_commandPool,
        &m_cubeMesh, &m_idolMesh, &m_texture,
        m_settings.renderWidth, m_settings.renderHeight);

    m_editorUI.init(m_context, m_window, m_swapchain);
    initScene();

    createSyncObjects();
}

void Application::initScene() {
    m_sceneObjects = {
        {{ 0.0f, -1.00f,  0.0f}, {1.0f, 1.0f, 1.0f},   false, true,  true  },
        {{-3.0f, -1.00f, -3.0f}, {0.5f, 2.0f, 0.5f},   false, false, true  },
        {{ 3.0f, -1.00f, -3.0f}, {0.5f, 2.0f, 0.5f},   false, false, true  },
        {{-3.0f, -1.00f,  3.0f}, {0.5f, 2.0f, 0.5f},   false, false, true  },
        {{ 3.0f, -1.00f,  3.0f}, {0.5f, 2.0f, 0.5f},   false, false, true  },
        {{ 0.0f, -1.05f,  0.0f}, {20.0f, 0.05f, 20.0f}, false, false, false },
        {{ 3.5f, 0.5f, 0.0f},    {0.2f, 0.2f, 0.2f},   false, false, false }
    };
}

void Application::cleanup() {
    VkDevice device = m_context.getDevice();
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        m_editorUI.cleanup(device);

        m_offscreenRenderer.cleanup(m_context);

        m_idolMesh.cleanup(m_context.getAllocator());
        m_cubeMesh.cleanup(m_context.getAllocator());
        m_texture.cleanup(device, m_context.getAllocator());

        for (VkSemaphore semaphore : m_imageAvailableSemaphores) vkDestroySemaphore(device, semaphore, nullptr);
        for (VkFence fence : m_inFlightFences)                    vkDestroyFence(device, fence, nullptr);
        m_imageAvailableSemaphores.clear();
        m_inFlightFences.clear();

        if (m_commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, m_commandPool, nullptr);
            m_commandPool = VK_NULL_HANDLE;
        }

        m_swapchain.cleanup(device);
    }
    m_context.cleanup();
    m_window.cleanup();
}

// ============================================================
//  SETTINGS LOAD / SAVE
// ============================================================
void Application::loadSettings() {
    std::ifstream file("settings.ini");
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string val = line.substr(pos + 1);
        try {
            if (key == "gpu_index") m_settings.selectedGpuIndex = std::stoi(val);
            else if (key == "render_width") m_settings.renderWidth = std::stoul(val);
            else if (key == "render_height") m_settings.renderHeight = std::stoul(val);
            else if (key == "camera_fov") m_settings.cameraFov = std::stof(val);
            else if (key == "camera_speed") m_settings.cameraSpeed = std::stof(val);
            else if (key == "fog_enabled") m_settings.fogEnabled = std::stoi(val) != 0;
            else if (key == "fog_density") m_settings.fogDensity = std::stof(val);
            else if (key == "bloom_threshold") m_settings.ppParams.bloomThreshold = std::stof(val);
            else if (key == "bloom_intensity") m_settings.ppParams.bloomIntensity = std::stof(val);
            else if (key == "saturation") m_settings.ppParams.saturation = std::stof(val);
            else if (key == "dither_strength") m_settings.ppParams.ditherStrength = std::stof(val);
            else if (key == "fur_enabled") m_settings.furEnabled = std::stoi(val) != 0;
            else if (key == "fur_shells") m_settings.shellCount = std::stoi(val);
            else if (key == "fur_length") m_settings.furLength = std::stof(val);
            else if (key == "fur_density") m_settings.furDensity = std::stof(val);
            else if (key == "fur_slope") m_settings.slopeThreshold = std::stof(val);
            else if (key == "fur_gravity") m_settings.furGravity = std::stof(val);
            else if (key == "sun_azimuth") m_settings.sunAzimuth = std::stof(val);
            else if (key == "sun_elevation") m_settings.sunElevation = std::stof(val);
            else if (key == "sun_intensity") m_settings.sunIntensity = std::stof(val);
            else if (key == "animate_light") m_settings.animatePointLight = std::stoi(val) != 0;
            else if (key == "light_intensity") m_settings.pointLightIntensity = std::stof(val);
            else if (key == "light_radius") m_settings.pointLightRadius = std::stof(val);
        }
        catch (...) { /* ignore bad values */ }
    }
}

void Application::saveSettings() {
    std::map<std::string, std::string> data;
    std::ifstream fin("settings.ini");
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto pos = line.find('=');
        if (pos != std::string::npos) {
            data[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }
    fin.close();

    data["gpu_index"] = std::to_string(m_settings.selectedGpuIndex);
    data["render_width"] = std::to_string(m_settings.renderWidth);
    data["render_height"] = std::to_string(m_settings.renderHeight);
    data["camera_fov"] = std::to_string(m_settings.cameraFov);
    data["camera_speed"] = std::to_string(m_settings.cameraSpeed);
    data["fog_enabled"] = std::to_string(m_settings.fogEnabled ? 1 : 0);
    data["fog_density"] = std::to_string(m_settings.fogDensity);
    data["bloom_threshold"] = std::to_string(m_settings.ppParams.bloomThreshold);
    data["bloom_intensity"] = std::to_string(m_settings.ppParams.bloomIntensity);
    data["saturation"] = std::to_string(m_settings.ppParams.saturation);
    data["dither_strength"] = std::to_string(m_settings.ppParams.ditherStrength);
    data["fur_enabled"] = std::to_string(m_settings.furEnabled ? 1 : 0);
    data["fur_shells"] = std::to_string(m_settings.shellCount);
    data["fur_length"] = std::to_string(m_settings.furLength);
    data["fur_density"] = std::to_string(m_settings.furDensity);
    data["fur_slope"] = std::to_string(m_settings.slopeThreshold);
    data["fur_gravity"] = std::to_string(m_settings.furGravity);
    data["sun_azimuth"] = std::to_string(m_settings.sunAzimuth);
    data["sun_elevation"] = std::to_string(m_settings.sunElevation);
    data["sun_intensity"] = std::to_string(m_settings.sunIntensity);
    data["animate_light"] = std::to_string(m_settings.animatePointLight ? 1 : 0);
    data["light_intensity"] = std::to_string(m_settings.pointLightIntensity);
    data["light_radius"] = std::to_string(m_settings.pointLightRadius);

    std::ofstream fout("settings.ini");
    for (auto& [k, v] : data) {
        fout << k << "=" << v << "\n";
    }
}

// ============================================================
//  COMMAND POOL / BUFFERS / SYNC
// ============================================================
void Application::createCommandPool() {
    QueueFamilyIndices indices = m_context.findQueueFamilies();
    VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.graphicsFamily.value();
    VK_CHECK(vkCreateCommandPool(m_context.getDevice(), &poolInfo, nullptr, &m_commandPool));
}

void Application::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
    VK_CHECK(vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, m_commandBuffers.data()));
}

void Application::createSyncObjects() {
    VkDevice device = m_context.getDevice();
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]));
    }
}

// ============================================================
//  SWAPCHAIN / MINIMIZE
// ============================================================
bool Application::framebufferIsZero() const {
    int width = 0, height = 0;
    m_window.getFramebufferSize(&width, &height);
    return width == 0 || height == 0;
}

void Application::waitForFramebuffer() {
    int width = 0, height = 0;
    m_window.getFramebufferSize(&width, &height);
    while ((width == 0 || height == 0) && !m_window.shouldClose()) {
        glfwWaitEvents();
        m_window.getFramebufferSize(&width, &height);
    }
}

void Application::recreateSwapchain() {
    waitForFramebuffer();
    if (m_window.shouldClose()) return;

    vkDeviceWaitIdle(m_context.getDevice());
    m_swapchain.cleanup(m_context.getDevice());
    m_swapchain.init(m_context, m_window);
}

// ============================================================
//  SCENE UPDATE
// ============================================================
void Application::updatePointLightAnimation(float time) {
    if (m_settings.animatePointLight) {
        const float R = 3.5f;
        m_settings.pointLightPos = glm::vec3(
            std::cos(time * 0.7f) * R,
            0.5f + std::sin(time * 1.3f) * 0.3f,
            std::sin(time * 0.7f) * R
        );
    }

    if (!m_sceneObjects.empty()) {
        m_sceneObjects.back().position = m_settings.pointLightPos;
    }
}

// ============================================================
//  MAIN LOOP
// ============================================================
void Application::mainLoop() {
    while (!m_window.shouldClose()) {
        m_window.pollEvents();

        if (framebufferIsZero()) {
            glfwWaitEvents();
            m_lastFrameTime = static_cast<float>(glfwGetTime());
            continue;
        }

        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - m_lastFrameTime;
        m_lastFrameTime = currentFrame;

        GLFWwindow* window = m_window.getNativeWindow();

        // --- Hotkeys ---
        static bool f1Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
            if (!f1Pressed) { f1Pressed = true; m_settings.showMainSettings = !m_settings.showMainSettings; }
        }
        else f1Pressed = false;

        static bool f2Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
            if (!f2Pressed) { f2Pressed = true; m_settings.showStatsOverlay = !m_settings.showStatsOverlay; }
        }
        else f2Pressed = false;

        static bool f3Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS) {
            if (!f3Pressed) { f3Pressed = true; m_settings.showSceneWindow = !m_settings.showSceneWindow; }
        }
        else f3Pressed = false;

        static bool f4Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS) {
            if (!f4Pressed) { f4Pressed = true; m_settings.showMainSettings = false; m_settings.showSceneWindow = false; }
        }
        else f4Pressed = false;

        static bool f11Pressed = false;
        if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS) {
            if (!f11Pressed) {
                f11Pressed = true;
                m_window.setFullscreen(!m_window.isFullscreen());
                recreateSwapchain();
            }
        }
        else f11Pressed = false;

        bool altPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
        static bool enterPressed = false;
        if (altPressed && glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            if (!enterPressed) {
                enterPressed = true;
                m_window.setFullscreen(!m_window.isFullscreen());
                recreateSwapchain();
            }
        }
        else enterPressed = false;

        m_gamepad.update();

        // --- Cursor logic ---
        bool editorActive = m_settings.showMainSettings || m_settings.showSceneWindow;
        if (editorActive) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_firstMouse = true;
        }
        else {
            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                if (m_firstMouse) {
                    glfwGetCursorPos(window, &m_lastMouseX, &m_lastMouseY);
                    m_firstMouse = false;
                }
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                m_camera.processMouseMovement(
                    static_cast<float>(xpos - m_lastMouseX),
                    static_cast<float>(m_lastMouseY - ypos));
                m_lastMouseX = xpos;
                m_lastMouseY = ypos;
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                m_firstMouse = true;
            }
        }

        m_camera.setSpeed(m_settings.cameraSpeed);
        m_camera.processKeyboard(window, deltaTime);
        m_camera.processGamepad(m_gamepad.getLeftStick(), m_gamepad.getRightStick(),
            m_gamepad.getLeftTrigger(), m_gamepad.getRightTrigger(),
            deltaTime);

        updatePointLightAnimation(m_lastFrameTime);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // --- Raycasting / Selection ---
        if (editorActive && !ImGui::GetIO().WantCaptureMouse) {
            double xpos, ypos;
            glfwGetCursorPos(window, &xpos, &ypos);
            int w, h;
            m_window.getFramebufferSize(&w, &h);

            if (w > 0 && h > 0) {
                float xndc = (2.0f * static_cast<float>(xpos)) / w - 1.0f;
                float yndc = (2.0f * static_cast<float>(ypos)) / h - 1.0f;

                glm::mat4 view = m_camera.getViewMatrix();
                glm::mat4 proj = glm::perspective(glm::radians(m_settings.cameraFov),
                    m_offscreenRenderer.getRenderWidth() / static_cast<float>(m_offscreenRenderer.getRenderHeight()),
                    0.1f, 100.0f);
                proj[1][1] *= -1;

                glm::vec4 rayClip(xndc, yndc, -1.0f, 1.0f);
                glm::vec4 rayEye = glm::inverse(proj) * rayClip;
                rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

                glm::vec3 rayDir = glm::normalize(glm::vec3(glm::inverse(view) * rayEye));
                glm::vec3 rayOrigin = glm::vec3(glm::inverse(view)[3]);

                float closestT = 99999.0f;
                int hoveredIdx = -1;

                for (size_t i = 0; i < m_sceneObjects.size(); ++i) {
                    if (i == 5) continue; // Пропускаем землю

                    const auto& obj = m_sceneObjects[i];
                    glm::vec3 center = obj.position;
                    float radius = glm::length(obj.scale) * 0.5f;

                    glm::vec3 oc = rayOrigin - center;
                    float b = glm::dot(oc, rayDir);
                    float c = glm::dot(oc, oc) - radius * radius;
                    float disc = b * b - c;

                    if (disc > 0.0f) {
                        float t = -b - std::sqrt(disc);
                        if (t > 0.0f && t < closestT) {
                            closestT = t;
                            hoveredIdx = static_cast<int>(i);
                        }
                    }
                }

                m_settings.hoveredObjectIndex = hoveredIdx;

                if (hoveredIdx != -1 && ImGui::IsMouseClicked(0)) {
                    m_settings.selectedObjectIndex = hoveredIdx;
                }
            }
        }
        else {
            m_settings.hoveredObjectIndex = -1;
        }

        EditorActions actions;
        if (m_settings.showMainSettings || m_settings.showSceneWindow || m_settings.showStatsOverlay) {
            m_editorUI.render(m_settings, m_context, m_window, m_camera,
                m_gamepad.isConnected(), m_sceneObjects, actions);
        }

        ImGui::Render();

        if (actions.requestRecreateSwapchain) {
            recreateSwapchain();
        }
        if (actions.requestRecreateOffscreen) {
            m_offscreenRenderer.recreateOffscreen(m_context,
                m_settings.renderWidth, m_settings.renderHeight);
        }
        if (actions.saveSettings) {
            saveSettings();
        }
        if (actions.resetSettings) {
            m_settings = EngineSettings();
            m_settings.showMainSettings = true; // keep window open
            actions.requestRecreateOffscreen = true;
        }

        drawFrame(deltaTime);
    }
    vkDeviceWaitIdle(m_context.getDevice());
}

// ============================================================
//  DRAW FRAME
// ============================================================
void Application::drawFrame(float deltaTime) {
    (void)deltaTime;

    if (framebufferIsZero()) return;

    VkDevice device = m_context.getDevice();
    VK_CHECK(vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX));

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(device, m_swapchain.getHandle(), UINT64_MAX,
        m_imageAvailableSemaphores[m_currentFrame],
        VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        VK_CHECK(result);
    }

    const bool recreateAfterPresent = (result == VK_SUBOPTIMAL_KHR);

    VK_CHECK(vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]));

    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(m_settings.cameraFov),
        m_offscreenRenderer.getRenderWidth() / static_cast<float>(m_offscreenRenderer.getRenderHeight()),
        0.1f, 100.0f);
    proj[1][1] *= -1;

    std::vector<SceneObject> renderObjects = m_sceneObjects;
    for (size_t i = 0; i < renderObjects.size(); ++i) {
        if (static_cast<int>(i) == m_settings.hoveredObjectIndex) {
            renderObjects[i].emissive += 0.25f + 0.1f * std::sin(m_lastFrameTime * 5.0f);
        }
        if (static_cast<int>(i) == m_settings.selectedObjectIndex) {
            renderObjects[i].emissive += 0.4f;
        }
    }

    VK_CHECK(vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0));
    m_offscreenRenderer.recordCommandBuffer(
        m_commandBuffers[m_currentFrame],
        m_swapchain, imageIndex,
        renderObjects, m_settings, m_lastFrameTime,
        view, proj);

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

    VkSemaphore signalSemaphores[] = { m_swapchain.getRenderFinishedSemaphores()[imageIndex] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VK_CHECK(vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]));

    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    VkSwapchainKHR swapChains[] = { m_swapchain.getHandle() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(m_context.getPresentQueue(), &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || recreateAfterPresent) {
        recreateSwapchain();
    }
    else {
        VK_CHECK(result);
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}