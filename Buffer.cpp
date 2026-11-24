#include "Buffer.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

#include <cstring>
#include <stdexcept>

void Buffer::init(VmaAllocator allocator, VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_buffer, &m_allocation, nullptr));
}

void Buffer::cleanup(VmaAllocator allocator) {
    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

void Buffer::uploadData(VmaAllocator allocator, const void* data, VkDeviceSize size) {
    void* mappedData = nullptr;
    VK_CHECK(vmaMapMemory(allocator, m_allocation, &mappedData));
    memcpy(mappedData, data, size);
    vmaUnmapMemory(allocator, m_allocation);
}

void Buffer::createDeviceLocal(VulkanContext& context, VkCommandPool commandPool, const void* data, VkDeviceSize size, VkBufferUsageFlags usage) {
    // 1. Создаем Staging буфер (CPU_ONLY)
    Buffer stagingBuffer;
    stagingBuffer.init(context.getAllocator(), size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    stagingBuffer.uploadData(context.getAllocator(), data, size);

    // 2. Создаем GPU буфер (GPU_ONLY)
    init(context.getAllocator(), size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    // 3. Копируем данные
    context.copyBuffer(commandPool, stagingBuffer.getBuffer(), m_buffer, size);

    // 4. Уничтожаем Staging буфер
    stagingBuffer.cleanup(context.getAllocator());
}