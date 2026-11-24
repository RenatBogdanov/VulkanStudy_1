#include "Window.h"

#include <stdexcept>

void Window::init(uint32_t width, uint32_t height, const char* title) {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("failed to initialize GLFW!");
    }
    m_glfwInitialized = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // ВАЖНО: включаем ресайз — иначе F11/смена разрешения не сможет пересоздать swapchain корректно
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_width = width;
    m_height = height;
    m_windowedWidth = static_cast<int>(width);
    m_windowedHeight = static_cast<int>(height);

    m_window = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title, nullptr, nullptr);
    if (m_window == nullptr) {
        cleanup();
        throw std::runtime_error("failed to create GLFW window!");
    }
}

void Window::cleanup() {
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    if (m_glfwInitialized) {
        glfwTerminate();
        m_glfwInitialized = false;
    }
}

// --- Новая сигнатура (5.3) ---
VkSurfaceKHR Window::createSurface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, m_window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    return surface;
}

// --- Старая сигнатура (совместимость с VulkanContext.cpp) ---
void Window::createSurface(VkInstance instance, VkSurfaceKHR* surface) {
    if (glfwCreateWindowSurface(instance, m_window, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::getFramebufferSize(int* width, int* height) const {
    glfwGetFramebufferSize(m_window, width, height);
}

std::vector<const char*> Window::getRequiredInstanceExtensions() {
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(extensions, extensions + count);
}

void Window::setFullscreen(bool fullscreen) {
    if (m_isFullscreen == fullscreen || m_window == nullptr) return;
    m_isFullscreen = fullscreen;

    if (fullscreen) {
        glfwGetWindowPos(m_window, &m_windowedX, &m_windowedY);
        glfwGetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else {
        glfwSetWindowMonitor(m_window, nullptr, m_windowedX, m_windowedY, m_windowedWidth, m_windowedHeight, 0);
    }
}