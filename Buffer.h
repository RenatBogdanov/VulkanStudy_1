#pragma once

#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>

class VulkanContext;

class Buffer {
public:
    Buffer() = default;
    ~Buffer() = default;

    // Создает пустой буфер (используется внутри класса для Staging)
    void init(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    // Уничтожает буфер
    void cleanup(VmaAllocator allocator);

    // Записывает данные в CPU-доступный буфер
    void uploadData(VmaAllocator allocator, const void* data, VkDeviceSize size);

    // Создает GPU-локальный буфер и копирует в него данные через временный Staging Buffer
    void createDeviceLocal(VulkanContext& context, VkCommandPool commandPool, const void* data, VkDeviceSize size, VkBufferUsageFlags usage);

    VkBuffer getBuffer() const { return m_buffer; }

private:
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
};