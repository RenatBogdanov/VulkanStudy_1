#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

#include "Vertex.h"

class VulkanContext;

class Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;

    // Запрет копирования
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void loadFromObj(VulkanContext& context, VkCommandPool commandPool, const std::string& filename);
    void createProceduralCube(VulkanContext& context, VkCommandPool commandPool);
    void createProceduralWaterfall(VulkanContext& context, VkCommandPool commandPool);
    void createProceduralWorm(VulkanContext& context, VkCommandPool commandPool);

    void cleanup(VmaAllocator allocator);

    void bind(VkCommandBuffer commandBuffer) const;
    void draw(VkCommandBuffer commandBuffer) const;
    void drawInstanced(VkCommandBuffer commandBuffer, uint32_t instanceCount) const;

    // Геттеры для WormSwarmSystem
    const std::vector<Vertex>& getVertices() const { return m_vertices; }
    const std::vector<uint32_t>& getIndices() const { return m_indices; }

private:
    void createVertexBuffer(VulkanContext& context, VkCommandPool commandPool, const std::vector<Vertex>& vertices);
    void createIndexBuffer(VulkanContext& context, VkCommandPool commandPool, const std::vector<uint32_t>& indices);

    VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_vertexBufferAllocation = VK_NULL_HANDLE;

    VkBuffer m_indexBuffer = VK_NULL_HANDLE;
    VmaAllocation m_indexBufferAllocation = VK_NULL_HANDLE;

    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;

    // Копии данных в RAM для симуляции (WormSwarmSystem)
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
};