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

    // Инициализирует GLFW и создаёт окно без графического API (Vulkan).
    void init(uint32_t width, uint32_t height, const char* title);
    void cleanup();

    // Создаёт VkSurfaceKHR для этого окна. Вызывать после создания VkInstance.
    void createSurface(VkInstance instance, VkSurfaceKHR* surface);

    bool shouldClose() const;
    void pollEvents() const;

    GLFWwindow* getNativeWindow() const { return m_window; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    // Расширения инстанса, необходимые GLFW. Вызывать только после Window::init().
    static std::vector<const char*> getRequiredInstanceExtensions();

private:
    GLFWwindow* m_window = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_glfwInitialized = false;
};
