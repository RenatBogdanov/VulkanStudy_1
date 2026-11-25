#include "Application.h"
#include "VulkanUtils.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>
#include <imguizmo/ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
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
    catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        cleanup();
        throw;
    }
    catch (...) {
        std::cerr << "FATAL UNKNOWN ERROR!" << std::endl;
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
    m_waterfallMesh.createProceduralWaterfall(m_context, m_commandPool);
    m_wormMesh.createProceduralWorm(m_context, m_commandPool); // НОВОЕ
    m_texture.init(m_context, m_commandPool, "textures/stone.png");

    // Передаем 8 аргументов, включая &m_wormMesh
    m_offscreenRenderer.init(m_context,
        m_swapchain.getImageFormat(),
        m_commandPool,
        &m_cubeMesh, &m_idolMesh, &m_waterfallMesh, &m_wormMesh, &m_texture,
        m_settings.renderWidth, m_settings.renderHeight);

    m_editorUI.init(m_context, m_window, m_swapchain);

    // --- Scene bootstrap ---
    std::ifstream probe("scene.txt");
    if (probe.good()) {
        probe.close();
        loadScene("scene.txt");
    }
    else {
        initDefaultScene();
        saveScene("scene.txt");
    }

    // Инициализация стаи червячков
    m_wormSwarm.init(m_settings.wormCount, m_sceneObjects, &m_cubeMesh, &m_idolMesh);

    createSyncObjects();
}

void Application::initDefaultScene() {
    // Исправлена ошибка C2679 (неверное количество аргументов для SceneObject)
    SceneObject idol;
    idol.name = "Idol";
    idol.position = glm::vec3(0.0f, -1.0f, 0.0f);
    idol.scale = glm::vec3(1.0f);
    idol.useIdolMesh = true;
    idol.isFurry = true;

    SceneObject pillar1;
    pillar1.name = "Pillar 1";
    pillar1.position = glm::vec3(-3.0f, -1.0f, -3.0f);
    pillar1.scale = glm::vec3(0.5f, 2.0f, 0.5f);
    pillar1.isFurry = true;

    SceneObject pillar2;
    pillar2.name = "Pillar 2";
    pillar2.position = glm::vec3(3.0f, -1.0f, -3.0f);
    pillar2.scale = glm::vec3(0.5f, 2.0f, 0.5f);
    pillar2.isFurry = true;

    SceneObject ground;
    ground.name = "Ground";
    ground.position = glm::vec3(0.0f, -1.05f, 0.0f);
    ground.scale = glm::vec3(20.0f, 0.05f, 20.0f);

    SceneObject light;
    light.name = "Point Light";
    light.position = glm::vec3(3.5f, 0.5f, 0.0f);
    light.scale = glm::vec3(0.2f);
    light.isLightMarker = true;

    m_sceneObjects = { idol, pillar1, pillar2, ground, light };
    m_settings.selectedObjectIndex = -1;
    m_settings.hoveredObjectIndex = -1;
}

void Application::loadScene(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) { initDefaultScene(); return; }

    std::vector<SceneObject> loaded;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string nameP, posP, rotP, sclP, flagsP, emissP, waterP;
        if (!std::getline(ss, nameP, '|')) continue;
        if (!std::getline(ss, posP, '|')) continue;
        if (!std::getline(ss, rotP, '|')) continue;
        if (!std::getline(ss, sclP, '|')) continue;
        if (!std::getline(ss, flagsP, '|')) continue;
        if (!std::getline(ss, emissP, '|')) continue;
        std::getline(ss, waterP, '|');

        SceneObject obj;
        obj.name = nameP;

        std::istringstream(posP) >> obj.position.x >> obj.position.y >> obj.position.z;
        std::istringstream(rotP) >> obj.rotation.x >> obj.rotation.y >> obj.rotation.z;
        std::istringstream(sclP) >> obj.scale.x >> obj.scale.y >> obj.scale.z;

        int uI = 0, iF = 0, iR = 0, iL = 0, iW = 0;
        std::istringstream(flagsP) >> uI >> iF >> iR >> iL >> iW;
        obj.useIdolMesh = (uI != 0);
        obj.isFurry = (iF != 0);
        obj.isRotating = (iR != 0);
        obj.isLightMarker = (iL != 0);
        obj.isWaterfall = (iW != 0);

        std::istringstream(emissP) >> obj.emissive;

        if (!waterP.empty()) {
            std::istringstream(waterP) >> obj.waterfallSpeed >> obj.waterfallFoam;
        }

        loaded.push_back(obj);
    }

    std::vector<SceneObject> final;
    for (auto& obj : loaded)
        if (!obj.isLightMarker) final.push_back(obj);

    SceneObject light;
    light.name = "Point Light";
    light.position = m_settings.pointLightPos;
    light.scale = glm::vec3(0.2f);
    light.isLightMarker = true;
    final.push_back(light);

    m_sceneObjects = std::move(final);
    m_settings.selectedObjectIndex = -1;
    m_settings.hoveredObjectIndex = -1;
}

void Application::saveScene(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return;

    file << "# Scene: name|px py pz|rx ry rz|sx sy sz|idol furry rotating light waterfall|emissive|waterspeed foam\n";
    for (const auto& obj : m_sceneObjects) {
        std::string n = obj.name;
        std::replace(n.begin(), n.end(), '|', '_');
        std::replace(n.begin(), n.end(), '\n', ' ');

        file << n << "|"
            << obj.position.x << " " << obj.position.y << " " << obj.position.z << "|"
            << obj.rotation.x << " " << obj.rotation.y << " " << obj.rotation.z << "|"
            << obj.scale.x << " " << obj.scale.y << " " << obj.scale.z << "|"
            << (obj.useIdolMesh ? 1 : 0) << " "
            << (obj.isFurry ? 1 : 0) << " "
            << (obj.isRotating ? 1 : 0) << " "
            << (obj.isLightMarker ? 1 : 0) << " "
            << (obj.isWaterfall ? 1 : 0) << "|"
            << obj.emissive << "|"
            << obj.waterfallSpeed << " " << obj.waterfallFoam << "\n";
    }
}

void Application::cleanup() {
    VkDevice device = m_context.getDevice();
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
        m_editorUI.cleanup(device);
        m_offscreenRenderer.cleanup(m_context);

        m_idolMesh.cleanup(m_context.getAllocator());
        m_cubeMesh.cleanup(m_context.getAllocator());
        m_waterfallMesh.cleanup(m_context.getAllocator());
        m_wormMesh.cleanup(m_context.getAllocator());
        m_texture.cleanup(device, m_context.getAllocator());

        for (VkSemaphore s : m_imageAvailableSemaphores) vkDestroySemaphore(device, s, nullptr);
        for (VkFence f : m_inFlightFences)               vkDestroyFence(device, f, nullptr);
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
            if (key == "gpu_index")        m_settings.selectedGpuIndex = std::stoi(val);
            else if (key == "render_width")     m_settings.renderWidth = std::stoul(val);
            else if (key == "render_height")    m_settings.renderHeight = std::stoul(val);
            else if (key == "camera_fov")       m_settings.cameraFov = std::stof(val);
            else if (key == "camera_speed")     m_settings.cameraSpeed = std::stof(val);
            else if (key == "fog_enabled")      m_settings.fogEnabled = std::stoi(val) != 0;
            else if (key == "fog_density")      m_settings.fogDensity = std::stof(val);
            else if (key == "bloom_threshold")  m_settings.ppParams.bloomThreshold = std::stof(val);
            else if (key == "bloom_intensity")  m_settings.ppParams.bloomIntensity = std::stof(val);
            else if (key == "saturation")       m_settings.ppParams.saturation = std::stof(val);
            else if (key == "dither_strength")  m_settings.ppParams.ditherStrength = std::stof(val);
            else if (key == "fur_enabled")      m_settings.furEnabled = std::stoi(val) != 0;
            else if (key == "fur_shells")       m_settings.shellCount = std::stoi(val);
            else if (key == "fur_length")       m_settings.furLength = std::stof(val);
            else if (key == "fur_density")      m_settings.furDensity = std::stof(val);
            else if (key == "fur_slope")        m_settings.slopeThreshold = std::stof(val);
            else if (key == "fur_gravity")      m_settings.furGravity = std::stof(val);
            else if (key == "sun_azimuth")      m_settings.sunAzimuth = std::stof(val);
            else if (key == "sun_elevation")    m_settings.sunElevation = std::stof(val);
            else if (key == "sun_intensity")    m_settings.sunIntensity = std::stof(val);
            else if (key == "animate_light")    m_settings.animatePointLight = std::stoi(val) != 0;
            else if (key == "light_intensity")  m_settings.pointLightIntensity = std::stof(val);
            else if (key == "light_radius")     m_settings.pointLightRadius = std::stof(val);
            // Worm Settings
            else if (key == "worms_enabled")    m_settings.wormsEnabled = std::stoi(val) != 0;
            else if (key == "worm_count")       m_settings.wormCount = std::stoi(val);
            else if (key == "worm_speed")       m_settings.wormSpeed = std::stof(val);
            else if (key == "worm_separation")  m_settings.wormSeparationRadius = std::stof(val);
            else if (key == "worm_wiggle")      m_settings.wormWiggleAmplitude = std::stof(val);
        }
        catch (...) { /* ignore */ }
    }
}

void Application::saveSettings() {
    std::map<std::string, std::string> data;
    std::ifstream fin("settings.ini");
    std::string line;
    while (std::getline(fin, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto pos = line.find('=');
        if (pos != std::string::npos)
            data[line.substr(0, pos)] = line.substr(pos + 1);
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
    data["worms_enabled"] = std::to_string(m_settings.wormsEnabled ? 1 : 0);
    data["worm_count"] = std::to_string(m_settings.wormCount);
    data["worm_speed"] = std::to_string(m_settings.wormSpeed);
    data["worm_separation"] = std::to_string(m_settings.wormSeparationRadius);
    data["worm_wiggle"] = std::to_string(m_settings.wormWiggleAmplitude);

    std::ofstream fout("settings.ini");
    for (auto& [k, v] : data) fout << k << "=" << v << "\n";
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
    if (!m_sceneObjects.empty() && m_sceneObjects.back().isLightMarker)
        m_sceneObjects.back().position = m_settings.pointLightPos;
}

// ============================================================
//  EDITOR ACTIONS
// ============================================================
static size_t findLightMarkerIndex(const std::vector<SceneObject>& v) {
    for (size_t i = 0; i < v.size(); ++i)
        if (v[i].isLightMarker) return i;
    return v.size();
}

void Application::handleEditorActions(const EditorActions& actions) {
    if (actions.duplicateObject) {
        int idx = m_settings.selectedObjectIndex;
        if (idx >= 0 && idx < (int)m_sceneObjects.size() && !m_sceneObjects[idx].isLightMarker) {
            SceneObject copy = m_sceneObjects[idx];
            copy.name += " Copy";
            copy.position.x += 1.0f;
            copy.isLightMarker = false;

            const size_t insertAt = findLightMarkerIndex(m_sceneObjects);
            m_sceneObjects.insert(m_sceneObjects.begin() + insertAt, copy);
            m_settings.selectedObjectIndex = (int)insertAt;
        }
    }

    if (actions.deleteObject) {
        int idx = m_settings.selectedObjectIndex;
        if (idx >= 0 && idx < (int)m_sceneObjects.size() && !m_sceneObjects[idx].isLightMarker) {
            m_sceneObjects.erase(m_sceneObjects.begin() + idx);
            m_settings.selectedObjectIndex = -1;
            m_settings.hoveredObjectIndex = -1;
        }
    }

    if (actions.addIdol) {
        SceneObject obj;
        obj.name = "Idol " + std::to_string(m_sceneObjects.size());
        obj.position = m_camera.getPosition() + m_camera.getFront() * 3.0f;
        obj.scale = glm::vec3(1.0f);
        obj.useIdolMesh = true;
        obj.isFurry = true;

        const size_t insertAt = findLightMarkerIndex(m_sceneObjects);
        m_sceneObjects.insert(m_sceneObjects.begin() + insertAt, obj);
        m_settings.selectedObjectIndex = (int)insertAt;
    }

    if (actions.addPillar) {
        SceneObject obj;
        obj.name = "Pillar " + std::to_string(m_sceneObjects.size());
        obj.position = m_camera.getPosition() + m_camera.getFront() * 3.0f;
        obj.scale = glm::vec3(0.5f, 2.0f, 0.5f);
        obj.useIdolMesh = false;

        const size_t insertAt = findLightMarkerIndex(m_sceneObjects);
        m_sceneObjects.insert(m_sceneObjects.begin() + insertAt, obj);
        m_settings.selectedObjectIndex = (int)insertAt;
    }

    if (actions.addWaterfall) {
        glm::vec3 forward = m_camera.getFront();
        forward.y = 0.0f;
        forward = glm::normalize(forward);
        glm::vec3 spawnPos = m_camera.getPosition() + forward * 10.0f;
        spawnPos.y = 0.0f;

        SceneObject rock;
        rock.name = "Cliff";
        rock.position = spawnPos - glm::vec3(0.0f, 5.0f, 0.0f);
        rock.scale = glm::vec3(6.0f, 10.0f, 4.0f);
        rock.useIdolMesh = false;

        const size_t insertAtRock = findLightMarkerIndex(m_sceneObjects);
        m_sceneObjects.insert(m_sceneObjects.begin() + insertAtRock, rock);

        SceneObject water;
        water.name = "Waterfall";
        water.position = spawnPos + glm::vec3(0.0f, 0.5f, 2.0f);
        water.scale = glm::vec3(1.0f);
        water.isWaterfall = true;
        water.waterfallSpeed = 1.5f;
        water.waterfallFoam = 1.5f;

        const size_t insertAtWater = findLightMarkerIndex(m_sceneObjects);
        m_sceneObjects.insert(m_sceneObjects.begin() + insertAtWater, water);
        m_settings.selectedObjectIndex = (int)insertAtWater;
    }

    if (actions.saveScene) saveScene("scene.txt");
    if (actions.loadScene) loadScene("scene.txt");
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
            if (!f4Pressed) {
                f4Pressed = true;
                m_settings.showMainSettings = false;
                m_settings.showSceneWindow = false;
            }
        }
        else f4Pressed = false;

        static bool f11Pressed = false;
        const bool f11Now = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
        if (f11Now && !f11Pressed && !ImGuizmo::IsUsing()) {
            f11Pressed = true;
            m_window.setFullscreen(!m_window.isFullscreen());
            m_firstMouse = true;
            recreateSwapchain();
        }
        else if (!f11Now) {
            f11Pressed = false;
        }

        const bool altPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
        static bool enterPressed = false;
        const bool altEnterNow = altPressed && glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
        if (altEnterNow && !enterPressed && !ImGuizmo::IsUsing()) {
            enterPressed = true;
            m_window.setFullscreen(!m_window.isFullscreen());
            m_firstMouse = true;
            recreateSwapchain();
        }
        else if (!altEnterNow) {
            enterPressed = false;
        }

        m_gamepad.update();

        const bool editorActive = m_settings.showMainSettings || m_settings.showSceneWindow;
        const bool rightMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        if (editorActive) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            m_firstMouse = true;
        }
        else {
            if (rightMouseDown) {
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
        if (!editorActive || rightMouseDown)
            m_camera.processKeyboard(window, deltaTime);

        m_camera.processGamepad(m_gamepad.getLeftStick(), m_gamepad.getRightStick(),
            m_gamepad.getLeftTrigger(), m_gamepad.getRightTrigger(), deltaTime);

        updatePointLightAnimation(m_lastFrameTime);

        // Обновление червячков
        if (m_settings.wormsEnabled) {
            if (m_wormSwarm.getCount() != static_cast<uint32_t>(m_settings.wormCount)) {
                m_wormSwarm.setCount(m_settings.wormCount);
            }
            m_wormSwarm.update(deltaTime, m_settings.pointLightPos, m_sceneObjects, m_settings);
            m_offscreenRenderer.updateWormInstances(m_context, m_wormSwarm.getInstances());
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        if (editorActive && !ImGui::GetIO().WantCaptureMouse
            && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
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
                    const auto& obj = m_sceneObjects[i];
                    if (obj.isLightMarker) continue;
                    if (obj.name == "Ground") continue;

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
                if (hoveredIdx != -1 && ImGui::IsMouseClicked(0))
                    m_settings.selectedObjectIndex = hoveredIdx;
            }
        }
        else {
            m_settings.hoveredObjectIndex = -1;
        }

        EditorActions actions;
        if (m_settings.showMainSettings || m_settings.showSceneWindow || m_settings.showStatsOverlay) {
            m_editorUI.render(m_settings, m_context, m_window, m_camera, m_gamepad.isConnected(), m_sceneObjects, m_wormSwarm, actions);
        }

        // Обработка кнопки пересборки сетки из UI
        if (actions.rebuildWormGrid) {
            m_wormSwarm.rebuildSurfaceGrid(m_sceneObjects, &m_cubeMesh, &m_idolMesh);
        }

        ImGui::Render();

        if (actions.requestRecreateSwapchain) recreateSwapchain();
        if (actions.requestRecreateOffscreen)
            m_offscreenRenderer.recreateOffscreen(m_context, m_settings.renderWidth, m_settings.renderHeight);
        if (actions.saveSettings)  saveSettings();
        if (actions.resetSettings) {
            m_settings = EngineSettings();
            m_settings.showMainSettings = true;
            actions.requestRecreateOffscreen = true;
        }
        handleEditorActions(actions);

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
        m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) VK_CHECK(result);
    const bool recreateAfterPresent = (result == VK_SUBOPTIMAL_KHR);

    VK_CHECK(vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]));

    glm::mat4 view = m_camera.getViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(m_settings.cameraFov),
        m_offscreenRenderer.getRenderWidth() / static_cast<float>(m_offscreenRenderer.getRenderHeight()),
        0.1f, 100.0f);
    proj[1][1] *= -1;

    std::vector<SceneObject> renderObjects = m_sceneObjects;
    for (size_t i = 0; i < renderObjects.size(); ++i) {
        if (static_cast<int>(i) == m_settings.hoveredObjectIndex)
            renderObjects[i].emissive += 0.25f + 0.1f * std::sin(m_lastFrameTime * 5.0f);
        if (static_cast<int>(i) == m_settings.selectedObjectIndex)
            renderObjects[i].emissive += 0.4f;
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
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || recreateAfterPresent)
        recreateSwapchain();
    else
        VK_CHECK(result);

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}