#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>

class Window {
public:
    Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void init(uint32_t width, uint32_t height, const char* title);
    void cleanup();

    // --- Создание поверхности: две сигнатуры для совместимости ---
    VkSurfaceKHR createSurface(VkInstance instance);                         // новая (5.3)
    void         createSurface(VkInstance instance, VkSurfaceKHR* surface);  // старая (VulkanContext.cpp)

    bool shouldClose() const;
    void pollEvents() const;

    GLFWwindow* getNativeWindow() const { return m_window; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    // Размер фреймбуфера (учитывает HiDPI и состояние «свёрнуто»: 0x0).
    void getFramebufferSize(int* width, int* height) const;

    // static — можно вызывать и Window::getRequiredInstanceExtensions(), и m_window.getRequiredInstanceExtensions()
    static std::vector<const char*> getRequiredInstanceExtensions();

    // Полноэкранный режим (используется в main.cpp)
    void setFullscreen(bool fullscreen);
    bool isFullscreen() const { return m_isFullscreen; }

private:
    GLFWwindow* m_window = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_glfwInitialized = false;

    bool m_isFullscreen = false;
    int m_windowedX = 0;
    int m_windowedY = 0;
    int m_windowedWidth = 800;
    int m_windowedHeight = 600;
};