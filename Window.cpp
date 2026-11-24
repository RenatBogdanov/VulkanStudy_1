#include "Window.h"

#include <stdexcept>

void Window::init(uint32_t width, uint32_t height, const char* title) {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("failed to initialize GLFW!");
    }
    m_glfwInitialized = true;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    m_width = width;
    m_height = height;
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

std::vector<const char*> Window::getRequiredInstanceExtensions() {
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(extensions, extensions + count);
}
