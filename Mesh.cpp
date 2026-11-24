#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 26495 26498 6287)
#endif

#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "Mesh.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

#include <unordered_map>
#include <iostream>

void Mesh::loadFromObj(VulkanContext& context, VkCommandPool commandPool, const std::string& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        std::cerr << "Failed to load obj: " << filepath << ". Falling back to procedural cube." << std::endl;
        createProceduralCube(context, commandPool);
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    glm::vec3 defaultColor(0.78f, 0.75f, 0.68f);

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0) {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            else {
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            }

            if (index.texcoord_index >= 0) {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else {
                vertex.texCoord = glm::vec2(0.0f);
            }

            vertex.color = defaultColor;

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

    m_vertexBuffer.createDeviceLocal(context, commandPool, vertices.data(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_indexBuffer.createDeviceLocal(context, commandPool, indices.data(), indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    m_indexCount = static_cast<uint32_t>(indices.size());
}

void Mesh::createProceduralCube(VulkanContext& context, VkCommandPool commandPool) {
    std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}, { 0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, { 0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}, { 0.0f, -1.0f,  0.0f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}, { 1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}
    };

    std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0,
        4, 7, 6, 6, 5, 4,
        8, 11, 10, 10, 9, 8,
        12, 13, 14, 14, 15, 12,
        16, 19, 18, 18, 17, 16,
        20, 23, 22, 22, 21, 20
    };

    m_indexCount = static_cast<uint32_t>(indices.size());

    VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();

    m_vertexBuffer.createDeviceLocal(context, commandPool, vertices.data(), vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    m_indexBuffer.createDeviceLocal(context, commandPool, indices.data(), indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void Mesh::bind(VkCommandBuffer cmd) {
    VkBuffer vertexBuffers[] = { m_vertexBuffer.getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer.getBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::draw(VkCommandBuffer cmd) {
    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}

void Mesh::drawInstanced(VkCommandBuffer cmd, uint32_t instanceCount) {
    vkCmdDrawIndexed(cmd, m_indexCount, instanceCount, 0, 0, 0);
}

void Mesh::cleanup(VmaAllocator allocator) {
    m_vertexBuffer.cleanup(allocator);
    m_indexBuffer.cleanup(allocator);
}