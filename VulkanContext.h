#pragma once

#include "VulkanUtils.h"

#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>

#include <cstdint>
#include <string>
#include <vector>

class Window;

struct GpuInfo {
    VkPhysicalDevice     device = VK_NULL_HANDLE;
    std::string          name;
    VkPhysicalDeviceType type = VK_PHYSICAL_DEVICE_TYPE_OTHER;
};

class VulkanContext {
public:
    VulkanContext() = default;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Порядок: instance -> surface -> physical device -> logical device -> allocator
    void init(Window& window);

    // Порядок уничтожения: allocator -> device -> surface -> instance
    void cleanup();

    VkInstance       getInstance() const { return m_instance; }
    VkSurfaceKHR     getSurface() const { return m_surface; }
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
    VkDevice         getDevice() const { return m_device; }
    VkQueue          getGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue          getPresentQueue() const { return m_presentQueue; }
    VmaAllocator     getAllocator() const { return m_allocator; }

    QueueFamilyIndices findQueueFamilies() const { return findQueueFamilies(m_physicalDevice); }
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

    // --- GPU selection ---
    const std::string& getCurrentGpuName() const { return m_gpuList[m_currentGpuIndex].name; }
    const std::vector<GpuInfo>& getAvailableGpus()  const { return m_gpuList; }
    uint32_t                     getCurrentGpuIndex() const { return m_currentGpuIndex; }

    // Сохраняет выбор в settings.ini. Применяется только после перезапуска.
    void saveGpuPreference(uint32_t index);

    // Одноразовые команды для работы с ресурсами
    void copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    // Хелперы для работы с изображениями (текстурами)
    void transitionImageLayout(VkCommandPool commandPool, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkCommandPool commandPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

private:
    void createInstance();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createAllocator();

    bool isDeviceSuitable(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;

    int  readGpuPreferenceFromFile() const;    // -1 если не найдено / ошибка
    void writeGpuPreferenceToFile(uint32_t index) const;

    VkInstance       m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR     m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice         m_device = VK_NULL_HANDLE;
    VkQueue          m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue          m_presentQueue = VK_NULL_HANDLE;
    VmaAllocator     m_allocator = VK_NULL_HANDLE;

    std::vector<GpuInfo> m_gpuList;
    uint32_t             m_currentGpuIndex = 0;
};