#include "Mesh.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"

#pragma warning(push, 0)
#ifndef TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#endif
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#pragma warning(pop)

#include <stdexcept>
#include <unordered_map>
#include <cstring>
#include <cmath>
#include <algorithm>

struct VertexHash {
    size_t operator()(const Vertex& vertex) const {
        return ((std::hash<float>()(vertex.pos.x) ^
            (std::hash<float>()(vertex.pos.y) << 1)) >> 1) ^
            (std::hash<float>()(vertex.pos.z) << 1) ^
            (std::hash<float>()(vertex.normal.x) << 1) ^
            (std::hash<float>()(vertex.texCoord.x) << 1);
    }
};

struct VertexEqual {
    bool operator()(const Vertex& lhs, const Vertex& rhs) const {
        return lhs.pos == rhs.pos && lhs.normal == rhs.normal && lhs.texCoord == rhs.texCoord;
    }
};

void Mesh::loadFromObj(VulkanContext& context, VkCommandPool commandPool, const std::string& filename) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        throw std::runtime_error("Failed to load OBJ: " + warn + err);
    }

    std::vector<Vertex> tempVertices;
    std::vector<uint32_t> tempIndices;
    std::unordered_map<Vertex, uint32_t, VertexHash, VertexEqual> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};
            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };
            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };
            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };
            vertex.color = glm::vec3(1.0f);

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(tempVertices.size());
                tempVertices.push_back(vertex);
            }
            tempIndices.push_back(uniqueVertices[vertex]);
        }
    }

    createVertexBuffer(context, commandPool, tempVertices);
    createIndexBuffer(context, commandPool, tempIndices);
}

void Mesh::createProceduralCube(VulkanContext& context, VkCommandPool commandPool) {
    std::vector<Vertex> tempVertices;

    auto addVertex = [&](glm::vec3 p, glm::vec3 n, glm::vec2 uv) {
        Vertex v{};
        v.pos = p;
        v.normal = n;
        v.texCoord = uv;
        v.color = glm::vec3(1.0f);
        tempVertices.push_back(v);
        };

    // ��������
    addVertex({ -0.5f, -0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, { 0.0f, 0.0f });
    addVertex({ 0.5f, -0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, { 1.0f, 0.0f });
    addVertex({ 0.5f,  0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, { 1.0f, 1.0f });
    addVertex({ -0.5f,  0.5f,  0.5f }, { 0.0f,  0.0f,  1.0f }, { 0.0f, 1.0f });
    // ������
    addVertex({ 0.5f, -0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, { 0.0f, 0.0f });
    addVertex({ -0.5f, -0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, { 1.0f, 0.0f });
    addVertex({ -0.5f,  0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, { 1.0f, 1.0f });
    addVertex({ 0.5f,  0.5f, -0.5f }, { 0.0f,  0.0f, -1.0f }, { 0.0f, 1.0f });
    // �������
    addVertex({ -0.5f,  0.5f,  0.5f }, { 0.0f,  1.0f,  0.0f }, { 0.0f, 1.0f });
    addVertex({ 0.5f,  0.5f,  0.5f }, { 0.0f,  1.0f,  0.0f }, { 1.0f, 1.0f });
    addVertex({ 0.5f,  0.5f, -0.5f }, { 0.0f,  1.0f,  0.0f }, { 1.0f, 0.0f });
    addVertex({ -0.5f,  0.5f, -0.5f }, { 0.0f,  1.0f,  0.0f }, { 0.0f, 0.0f });
    // ������
    addVertex({ -0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f,  0.0f }, { 0.0f, 1.0f });
    addVertex({ 0.5f, -0.5f, -0.5f }, { 0.0f, -1.0f,  0.0f }, { 1.0f, 1.0f });
    addVertex({ 0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f,  0.0f }, { 1.0f, 0.0f });
    addVertex({ -0.5f, -0.5f,  0.5f }, { 0.0f, -1.0f,  0.0f }, { 0.0f, 0.0f });
    // ������
    addVertex({ 0.5f, -0.5f,  0.5f }, { 1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f });
    addVertex({ 0.5f, -0.5f, -0.5f }, { 1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f });
    addVertex({ 0.5f,  0.5f, -0.5f }, { 1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f });
    addVertex({ 0.5f,  0.5f,  0.5f }, { 1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f });
    // �����
    addVertex({ -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 0.0f });
    addVertex({ -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 0.0f });
    addVertex({ -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, { 1.0f, 1.0f });
    addVertex({ -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, { 0.0f, 1.0f });

    std::vector<uint32_t> tempIndices = {
        0,1,2, 2,3,0, 4,5,6, 6,7,4, 8,9,10, 10,11,8,
        12,13,14, 14,15,12, 16,17,18, 18,19,16, 20,21,22, 22,23,20
    };

    createVertexBuffer(context, commandPool, tempVertices);
    createIndexBuffer(context, commandPool, tempIndices);
}

void Mesh::createProceduralWaterfall(VulkanContext& context, VkCommandPool commandPool) {
    std::vector<Vertex> tempVertices;
    std::vector<uint32_t> tempIndices;

    int segmentsX = 12;
    int segmentsY = 24;
    float width = 5.0f;
    float height = 8.0f;
    float curveDepth = 0.8f;

    for (int y = 0; y <= segmentsY; ++y) {
        float ty = (float)y / segmentsY;
        float py = -ty * height;

        for (int x = 0; x <= segmentsX; ++x) {
            float tx = (float)x / segmentsX;

            float angle = (tx - 0.5f) * 1.2f;
            float px = sin(angle) * (width * 0.5f);
            float pz = cos(angle) * curveDepth;

            Vertex v{};
            v.pos = glm::vec3(px, py, pz);

            float alphaY = 1.0f;
            if (ty < 0.1f) alphaY = ty / 0.1f;
            if (ty > 0.85f) alphaY = (1.0f - ty) / 0.15f;
            float alphaX = sin(tx * 3.14159f);
            v.color = glm::vec3(alphaY * alphaX);

            v.texCoord = glm::vec2(tx, ty * 2.0f);
            v.normal = glm::vec3(sin(angle), 0.0f, cos(angle));

            tempVertices.push_back(v);
        }
    }

    for (int y = 0; y < segmentsY; ++y) {
        for (int x = 0; x < segmentsX; ++x) {
            uint32_t topLeft = y * (segmentsX + 1) + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (y + 1) * (segmentsX + 1) + x;
            uint32_t bottomRight = bottomLeft + 1;

            tempIndices.push_back(topLeft);
            tempIndices.push_back(bottomLeft);
            tempIndices.push_back(topRight);
            tempIndices.push_back(topRight);
            tempIndices.push_back(bottomLeft);
            tempIndices.push_back(bottomRight);
        }
    }

    createVertexBuffer(context, commandPool, tempVertices);
    createIndexBuffer(context, commandPool, tempIndices);
}

namespace {
    constexpr int   WORM_SEGMENTS = 12;       // колец вдоль тела (= RING_COUNT в worm.vert)
    constexpr int   WORM_SIDES = 8;           // граней в сечении
    constexpr float WORM_LENGTH = 1.0f;
    constexpr float WORM_RADIUS = 0.08f;      // максимальный радиус у головы
    constexpr float WORM_TAIL_TAPER = 0.65f;  // радиус хвоста относительно головы
    constexpr float WORM_CAP_POWER = 0.6f;    // <1 → скруглённые капсульные концы
    constexpr float WORM_PI = 3.14159265f;
    constexpr float WORM_PROFILE_EPS = 0.01f; // шаг для производной профиля

    // Радиус тела при t∈[0,1] (0 — хвост, 1 — голова): капсульные концы + сужение к хвосту.
    float wormRadius(float t) {
        float cap = std::pow(std::max(std::sin(t * WORM_PI), 0.0f), WORM_CAP_POWER);
        float taper = WORM_TAIL_TAPER + (1.0f - WORM_TAIL_TAPER) * t;
        return WORM_RADIUS * cap * taper;
    }
}

void Mesh::createProceduralWorm(VulkanContext& context, VkCommandPool commandPool) {
    std::vector<Vertex> tempVertices;
    std::vector<uint32_t> tempIndices;
    tempVertices.reserve((WORM_SEGMENTS + 1) * WORM_SIDES);
    tempIndices.reserve(WORM_SEGMENTS * WORM_SIDES * 6);

    // Тело вытянуто вдоль +Z (голова на +Z), центр в начале координат,
    // ось трубки поднята на радиус кольца — брюхо всегда лежит на y = 0.
    for (int i = 0; i <= WORM_SEGMENTS; ++i) {
        float t = (float)i / WORM_SEGMENTS;
        float z = (t - 0.5f) * WORM_LENGTH;
        float r = wormRadius(t);

        // dr/dz для наклона нормали (иначе сужение освещается как плоский цилиндр).
        float t0 = std::max(t - WORM_PROFILE_EPS, 0.0f);
        float t1 = std::min(t + WORM_PROFILE_EPS, 1.0f);
        float dr = (wormRadius(t1) - wormRadius(t0)) / ((t1 - t0) * WORM_LENGTH);

        for (int j = 0; j < WORM_SIDES; ++j) {
            float angle = (float)j / WORM_SIDES * 2.0f * WORM_PI;
            float c = std::cos(angle);
            float s = std::sin(angle);

            Vertex v{};
            v.pos = glm::vec3(c * r, r + s * r, z);
            v.normal = glm::normalize(glm::vec3(c, s, -dr * (1.0f + s)));
            v.texCoord = glm::vec2((float)j / WORM_SIDES, t);
            v.color = glm::vec3(1.0f);
            tempVertices.push_back(v);
        }
    }

    // Обход CCW снаружи (как у куба) — шейдер строит правосторонний базис.
    for (int i = 0; i < WORM_SEGMENTS; ++i) {
        for (int j = 0; j < WORM_SIDES; ++j) {
            uint32_t a = i * WORM_SIDES + j;
            uint32_t b = i * WORM_SIDES + (j + 1) % WORM_SIDES;
            uint32_t c = (i + 1) * WORM_SIDES + j;
            uint32_t d = (i + 1) * WORM_SIDES + (j + 1) % WORM_SIDES;
            tempIndices.push_back(a); tempIndices.push_back(b); tempIndices.push_back(c);
            tempIndices.push_back(b); tempIndices.push_back(d); tempIndices.push_back(c);
        }
    }

    createVertexBuffer(context, commandPool, tempVertices);
    createIndexBuffer(context, commandPool, tempIndices);
}

void Mesh::cleanup(VmaAllocator allocator) {
    if (m_vertexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, m_vertexBuffer, m_vertexBufferAllocation);
        m_vertexBuffer = VK_NULL_HANDLE;
    }
    if (m_indexBuffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(allocator, m_indexBuffer, m_indexBufferAllocation);
        m_indexBuffer = VK_NULL_HANDLE;
    }
}

void Mesh::bind(VkCommandBuffer commandBuffer) const {
    VkBuffer buffers[] = { m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Mesh::draw(VkCommandBuffer commandBuffer) const {
    vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
}

void Mesh::drawInstanced(VkCommandBuffer commandBuffer, uint32_t instanceCount) const {
    vkCmdDrawIndexed(commandBuffer, m_indexCount, instanceCount, 0, 0, 0);
}

void Mesh::createVertexBuffer(VulkanContext& context, VkCommandPool commandPool, const std::vector<Vertex>& vertices) {
    m_vertices = vertices;

    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    if (bufferSize == 0) return;

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VK_CHECK(vmaCreateBuffer(context.getAllocator(), &bufferInfo, &allocInfo, &stagingBuffer, &stagingBufferAllocation, nullptr));

    void* data;
    vmaMapMemory(context.getAllocator(), stagingBufferAllocation, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vmaUnmapMemory(context.getAllocator(), stagingBufferAllocation);

    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateBuffer(context.getAllocator(), &bufferInfo, &allocInfo, &m_vertexBuffer, &m_vertexBufferAllocation, nullptr));

    // ���������� ����� �� VulkanContext ������ ��������� �������
    context.copyBuffer(commandPool, stagingBuffer, m_vertexBuffer, bufferSize);

    vmaDestroyBuffer(context.getAllocator(), stagingBuffer, stagingBufferAllocation);
    m_vertexCount = static_cast<uint32_t>(vertices.size());
}

void Mesh::createIndexBuffer(VulkanContext& context, VkCommandPool commandPool, const std::vector<uint32_t>& indices) {
    m_indices = indices;

    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    if (bufferSize == 0) return;

    VkBuffer stagingBuffer;
    VmaAllocation stagingBufferAllocation;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    VK_CHECK(vmaCreateBuffer(context.getAllocator(), &bufferInfo, &allocInfo, &stagingBuffer, &stagingBufferAllocation, nullptr));

    void* data;
    vmaMapMemory(context.getAllocator(), stagingBufferAllocation, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vmaUnmapMemory(context.getAllocator(), stagingBufferAllocation);

    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateBuffer(context.getAllocator(), &bufferInfo, &allocInfo, &m_indexBuffer, &m_indexBufferAllocation, nullptr));

    // ���������� ����� �� VulkanContext
    context.copyBuffer(commandPool, stagingBuffer, m_indexBuffer, bufferSize);

    vmaDestroyBuffer(context.getAllocator(), stagingBuffer, stagingBufferAllocation);
    m_indexCount = static_cast<uint32_t>(indices.size());
}