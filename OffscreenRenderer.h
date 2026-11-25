#pragma once

#include "RenderTypes.h"
#include "WormSwarmSystem.h"
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

class VulkanContext;
class Swapchain;
class Mesh;
class Texture;

class OffscreenRenderer {
public:
    OffscreenRenderer() = default;
    OffscreenRenderer(const OffscreenRenderer&) = delete;
    OffscreenRenderer& operator=(const OffscreenRenderer&) = delete;

    void init(VulkanContext& context, VkFormat swapchainFormat, VkCommandPool commandPool,
        Mesh* cubeMesh, Mesh* idolMesh, Mesh* waterfallMesh, Mesh* wormMesh, Texture* texture,
        uint32_t initialWidth, uint32_t initialHeight);
    void cleanup(VulkanContext& context);
    void recreateOffscreen(VulkanContext& context, uint32_t width, uint32_t height);

    void updateWormInstances(VulkanContext& context, const std::vector<WormInstanceData>& instances);

    void recordCommandBuffer(VkCommandBuffer commandBuffer,
        Swapchain& swapchain,
        uint32_t imageIndex,
        const std::vector<SceneObject>& objects,
        const EngineSettings& settings,
        float time,
        const glm::mat4& view,
        const glm::mat4& proj);

    uint32_t getRenderWidth() const { return m_renderWidth; }
    uint32_t getRenderHeight() const { return m_renderHeight; }

private:
    void createOffscreenResources(VulkanContext& context);
    void createDepthResources(VulkanContext& context);
    void createUniformBuffer(VulkanContext& context);
    void createWormBuffer(VulkanContext& context);
    void createSceneDescriptorSetLayout(VulkanContext& context);
    void createSceneDescriptorPool(VulkanContext& context);
    void createSceneDescriptorSet(VulkanContext& context);
    void updateSceneDescriptorSet(VulkanContext& context);

    void createWormDescriptorSetLayout(VulkanContext& context);
    void createWormDescriptorPool(VulkanContext& context);
    void createWormDescriptorSet(VulkanContext& context);
    void updateWormDescriptorSet(VulkanContext& context);

    void createPostProcessDescriptorSetLayout(VulkanContext& context);
    void createPostProcessDescriptorPool(VulkanContext& context);
    void createPostProcessDescriptorSet(VulkanContext& context);
    void updatePostProcessDescriptorSet(VulkanContext& context);

    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(VulkanContext& context, const std::vector<char>& code);

    void createGraphicsPipeline(VulkanContext& context);
    void createWaterfallPipeline(VulkanContext& context);
    void createWormPipeline(VulkanContext& context);
    void createPostProcessPipeline(VulkanContext& context, VkFormat swapchainFormat);

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
        VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask);

    void fillUbo(const EngineSettings& settings, float time, const glm::mat4& view, const glm::mat4& proj);

    Mesh* m_cubeMesh = nullptr;
    Mesh* m_idolMesh = nullptr;
    Mesh* m_waterfallMesh = nullptr;
    Mesh* m_wormMesh = nullptr;
    Texture* m_texture = nullptr;

    uint32_t m_renderWidth = 0;
    uint32_t m_renderHeight = 0;

    VkImage m_offscreenImage = VK_NULL_HANDLE;
    VmaAllocation m_offscreenAllocation = VK_NULL_HANDLE;
    VkImageView m_offscreenImageView = VK_NULL_HANDLE;
    VkSampler m_offscreenSampler = VK_NULL_HANDLE;

    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthAllocation = VK_NULL_HANDLE;
    VkImageView m_depthImageView = VK_NULL_HANDLE;

    VkBuffer m_sceneUboBuffer = VK_NULL_HANDLE;
    VmaAllocation m_sceneUboAllocation = VK_NULL_HANDLE;
    void* m_sceneUboMapped = nullptr;

    VkBuffer m_wormInstanceBuffer = VK_NULL_HANDLE;
    VmaAllocation m_wormInstanceAllocation = VK_NULL_HANDLE;
    void* m_wormInstanceMapped = nullptr;
    uint32_t m_maxWorms = 3000;

    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_wormDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_wormDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_wormDescriptorSet = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_postProcessDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_postProcessDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_postProcessDescriptorSet = VK_NULL_HANDLE;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_wormPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_wormPipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_waterfallPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_waterfallPipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_postProcessPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_postProcessPipeline = VK_NULL_HANDLE;
};