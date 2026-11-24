#pragma once

#include "VulkanUtils.h"

#include <vulkan/vulkan.h>

class Window;

class VulkanContext {
public:
    VulkanContext() = default;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Порядок: instance -> surface -> physical device -> logical device.
    // При ошибке на любом шаге уже созданные объекты уничтожаются, исключение пробрасывается дальше.
    void init(Window& window);

    // Порядок уничтожения: device -> surface -> instance. Безопасно вызывать повторно.
    void cleanup();

    VkInstance getInstance() const { return m_instance; }
    VkSurfaceKHR getSurface() const { return m_surface; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice getDevice() const { return m_device; }
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue getPresentQueue() const { return m_presentQueue; }

    QueueFamilyIndices findQueueFamilies() const { return findQueueFamilies(m_physicalDevice); }
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

private:
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    bool isDeviceSuitable(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
};