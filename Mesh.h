#pragma once

#include "Buffer.h"
#include "Vertex.h"
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

class VulkanContext;

class Mesh {
public:
    Mesh() = default;

    // Загружает модель из .obj. Если файл не найден, создает процедурный куб.
    void loadFromObj(VulkanContext& context, VkCommandPool commandPool, const std::string& filepath);

    // Создает примитивный куб (используется как fallback и для колонн)
    void createProceduralCube(VulkanContext& context, VkCommandPool commandPool);

    void bind(VkCommandBuffer cmd);
    void draw(VkCommandBuffer cmd);
    void cleanup(VmaAllocator allocator);

private:
    Buffer m_vertexBuffer;
    Buffer m_indexBuffer;
    uint32_t m_indexCount = 0;
};