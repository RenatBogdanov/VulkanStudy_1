#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Camera.h"
#include "Gamepad.h"
#include "Mesh.h"
#include "Swapchain.h"
#include "Texture.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"
#include "Window.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

// ============================================================
//  PUSH CONSTANTS (80 байт с выравниванием mat4)
// ============================================================
struct PushConstants {
    glm::mat4 model;
    float     furLayer = 0.0f;
    float     emissive = 0.0f;
};

// ============================================================
//  SCENE UBO Ч std140, 256 байт
// ============================================================
struct SceneUBO {
    glm::mat4 viewProj;              // offset   0
    glm::mat4 view;                  // offset  64
    glm::vec4 fogColorDensity;       // offset 128  (rgb = color, a = density)
    glm::vec4 furParams;             // offset 144  (x=time, y=furLength, z=furDensity, w=slopeThreshold)
    glm::vec4 sunDirection;          // offset 160  (xyz = normalized dir to sun)
    glm::vec4 sunColor;              // offset 176  (rgb = sun color)
    glm::vec4 skyAmbientColor;       // offset 192  (rgb = sky hemisphere ambient)
    glm::vec4 groundAmbientColor;    // offset 208  (rgb = ground hemisphere ambient)
    glm::vec4 pointLightPos;         // offset 224  (xyz = world pos, w = falloff radius)
    glm::vec4 pointLightColor;       // offset 240  (rgb = tint * intensity)
};
static_assert(sizeof(SceneUBO) == 256, "SceneUBO must be 256 bytes for std140 layout");

// ============================================================
//  POST-PROCESS (совпадает с postprocess.frag push constant)
// ============================================================
struct PostProcessParams {
    float bloomThreshold = 0.88f;
    float bloomIntensity = 0.45f;
    float saturation = 0.80f;
    float ditherStrength = 1.0f;
};

struct SceneObject {
    glm::vec3 position;
    glm::vec3 scale;
    bool  isRotating;
    bool  useIdolMesh;
    bool  isFurry;
    float emissive = 0.0f;
};

// ============================================================
//  APPLICATION
// ============================================================
class HelloTriangleApplication {
public:
    void run() {
        m_window.init(WIDTH, HEIGHT, "Vulkan PS2 Scene");
        try {
            initVulkan();
            mainLoop();
        }
        catch (...) {
            cleanup();
            throw;
        }
        cleanup();
    }

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    Window          m_window;
    VulkanContext   m_context;
    Swapchain       m_swapchain;
    Camera          m_camera;
    Gamepad         m_gamepad;

    VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;

    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_postProcessPipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_postProcessPipeline = VK_NULL_HANDLE;

    Texture             m_texture;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool    m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet     m_descriptorSet = VK_NULL_HANDLE;

    VkBuffer       m_sceneUboBuffer = VK_NULL_HANDLE;
    VmaAllocation  m_sceneUboAllocation = VK_NULL_HANDLE;
    void* m_sceneUboMapped = nullptr;

    VkDescriptorSetLayout m_postProcessDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      m_postProcessDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet       m_postProcessDescriptorSet = VK_NULL_HANDLE;

    VkImage        m_offscreenImage = VK_NULL_HANDLE;
    VmaAllocation  m_offscreenAllocation = VK_NULL_HANDLE;
    VkImageView    m_offscreenImageView = VK_NULL_HANDLE;
    VkSampler      m_offscreenSampler = VK_NULL_HANDLE;

    VkImage        m_depthImage = VK_NULL_HANDLE;
    VmaAllocation  m_depthAllocation = VK_NULL_HANDLE;
    VkImageView    m_depthImageView = VK_NULL_HANDLE;

    Mesh m_idolMesh;
    Mesh m_cubeMesh;

    VkCommandPool                m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore>     m_imageAvailableSemaphores;
    std::vector<VkFence>         m_inFlightFences;

    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    bool             m_showGui = false;
    bool             m_f1Pressed = false;

    // --- Fog / Post-FX ---
    bool              m_fogEnabled = true;
    float             m_fogDensity = 0.018f;
    glm::vec3         m_fogColor = glm::vec3(0.53f, 0.57f, 0.55f);
    PostProcessParams m_ppParams;

    // --- Fur / Grass ---
    bool  m_furEnabled = true;
    int   m_shellCount = 32;
    float m_furLength = 0.15f;
    float m_furDensity = 200.0f;
    float m_slopeThreshold = 0.45f;

    // --- Lighting ---
    float     m_sunAzimuth = 0.6435f;   // atan2(0.3, 0.4)
    float     m_sunElevation = 0.55f;
    float     m_sunIntensity = 1.85f;
    glm::vec3 m_skyAmbient = glm::vec3(0.18f, 0.21f, 0.26f);
    glm::vec3 m_groundAmbient = glm::vec3(0.03f, 0.04f, 0.06f);

    bool      m_animatePointLight = true;
    float     m_pointLightIntensity = 1.0f;
    glm::vec3 m_pointLightColorBase = glm::vec3(1.4f, 0.65f, 0.2f);
    float     m_pointLightRadius = 8.0f;
    glm::vec3 m_pointLightPos = glm::vec3(3.5f, 0.5f, 0.0f);

    // --- Render size ---
    uint32_t m_renderWidth = 640;
    uint32_t m_renderHeight = 448;

    // --- Timing / input ---
    float  m_lastFrameTime = 0.0f;
    bool   m_firstMouse = true;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;

    uint32_t m_currentFrame = 0;

    // ============================================================
    //  INIT
    // ============================================================
    void initVulkan() {
        m_context.init(m_window);
        m_swapchain.init(m_context, m_window);
        m_swapchainImageFormat = m_swapchain.getImageFormat();

        createCommandPool();
        createOffscreenResources();
        createDepthResources();

        m_cubeMesh.createProceduralCube(m_context, m_commandPool);
        m_idolMesh.loadFromObj(m_context, m_commandPool, "models/idol.obj");

        createUniformBuffers();
        createDescriptorSetLayout();
        createTexture();
        createDescriptorPool();
        createDescriptorSet();

        createPostProcessDescriptorSetLayout();
        createPostProcessDescriptorPool();
        createPostProcessDescriptorSet();

        createGraphicsPipeline();
        createPostProcessPipeline();
        createCommandBuffers();
        createSyncObjects();
        initImGui();
    }

    // ============================================================
    //  OFFSCREEN / SWAPCHAIN RESIZE
    // ============================================================
    void recreateOffscreenResources(uint32_t width, uint32_t height) {
        vkDeviceWaitIdle(m_context.getDevice());

        if (m_offscreenSampler != VK_NULL_HANDLE) vkDestroySampler(m_context.getDevice(), m_offscreenSampler, nullptr);
        if (m_offscreenImageView != VK_NULL_HANDLE) vkDestroyImageView(m_context.getDevice(), m_offscreenImageView, nullptr);
        if (m_offscreenImage != VK_NULL_HANDLE) vmaDestroyImage(m_context.getAllocator(), m_offscreenImage, m_offscreenAllocation);
        if (m_depthImageView != VK_NULL_HANDLE) vkDestroyImageView(m_context.getDevice(), m_depthImageView, nullptr);
        if (m_depthImage != VK_NULL_HANDLE) vmaDestroyImage(m_context.getAllocator(), m_depthImage, m_depthAllocation);

        m_offscreenImage = VK_NULL_HANDLE;
        m_offscreenAllocation = VK_NULL_HANDLE;
        m_offscreenImageView = VK_NULL_HANDLE;
        m_offscreenSampler = VK_NULL_HANDLE;
        m_depthImage = VK_NULL_HANDLE;
        m_depthAllocation = VK_NULL_HANDLE;
        m_depthImageView = VK_NULL_HANDLE;

        m_renderWidth = width;
        m_renderHeight = height;

        createOffscreenResources();
        createDepthResources();
        updatePostProcessDescriptorSet();
    }

    bool framebufferIsZero() const {
        int width = 0;
        int height = 0;
        m_window.getFramebufferSize(&width, &height);
        return width == 0 || height == 0;
    }

    void waitForFramebuffer() {
        int width = 0;
        int height = 0;
        m_window.getFramebufferSize(&width, &height);

        while ((width == 0 || height == 0) && !m_window.shouldClose()) {
            glfwWaitEvents();
            m_window.getFramebufferSize(&width, &height);
        }
    }

    void recreateSwapchain() {
        // ќкно может быть свЄрнуто (Alt+Tab / minimize) Ч ждЄм восстановлени€ размера.
        waitForFramebuffer();
        if (m_window.shouldClose()) {
            return;
        }

        vkDeviceWaitIdle(m_context.getDevice());
        m_swapchain.cleanup(m_context.getDevice());
        m_swapchain.init(m_context, m_window);
        m_swapchainImageFormat = m_swapchain.getImageFormat();
    }

    // ============================================================
    //  BUFFERS
    // ============================================================
    void createUniformBuffers() {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(SceneUBO);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        VK_CHECK(vmaCreateBuffer(m_context.getAllocator(), &bufferInfo, &allocInfo,
            &m_sceneUboBuffer, &m_sceneUboAllocation, nullptr));
        VK_CHECK(vmaMapMemory(m_context.getAllocator(), m_sceneUboAllocation, &m_sceneUboMapped));
    }

    // ============================================================
    //  OFFSCREEN
    // ============================================================
    void createOffscreenResources() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_renderWidth;
        imageInfo.extent.height = m_renderHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo,
            &m_offscreenImage, &m_offscreenAllocation, nullptr));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_offscreenImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_offscreenImageView));

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        VK_CHECK(vkCreateSampler(m_context.getDevice(), &samplerInfo, nullptr, &m_offscreenSampler));
    }

    void createDepthResources() {
        VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_renderWidth;
        imageInfo.extent.height = m_renderHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        VK_CHECK(vmaCreateImage(m_context.getAllocator(), &imageInfo, &allocInfo,
            &m_depthImage, &m_depthAllocation, nullptr));

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_context.getDevice(), &viewInfo, nullptr, &m_depthImageView));
    }

    // ============================================================
    //  DESCRIPTORS Ч main pass
    // ============================================================
    void createDescriptorSetLayout() {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};

        bindings[0].binding = 0;
        bindings[0].descriptorCount = 1;
        bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[0].pImmutableSamplers = nullptr;
        bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        bindings[1].binding = 1;
        bindings[1].descriptorCount = 1;
        bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        bindings[1].pImmutableSamplers = nullptr;
        bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VK_CHECK(vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout));
    }

    void createTexture() {
        m_texture.init(m_context, m_commandPool, "textures/stone.png");
    }

    void createDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[0].descriptorCount = 1;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 1;

        VK_CHECK(vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_descriptorPool));
    }

    void createDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_descriptorSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, &m_descriptorSet));

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_texture.getImageView();
        imageInfo.sampler = m_texture.getSampler();

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_sceneUboBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SceneUBO);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_descriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_descriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(m_context.getDevice(),
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(), 0, nullptr);
    }

    // ============================================================
    //  DESCRIPTORS Ч post-process
    // ============================================================
    void createPostProcessDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        VK_CHECK(vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutInfo, nullptr, &m_postProcessDescriptorSetLayout));
    }

    void createPostProcessDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        VK_CHECK(vkCreateDescriptorPool(m_context.getDevice(), &poolInfo, nullptr, &m_postProcessDescriptorPool));
    }

    void createPostProcessDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_postProcessDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_postProcessDescriptorSetLayout;

        VK_CHECK(vkAllocateDescriptorSets(m_context.getDevice(), &allocInfo, &m_postProcessDescriptorSet));
        updatePostProcessDescriptorSet();
    }

    void updatePostProcessDescriptorSet() {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_offscreenImageView;
        imageInfo.sampler = m_offscreenSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_postProcessDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_context.getDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    // ============================================================
    //  SHADER HELPERS
    // ============================================================
    std::vector<char> readFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) throw std::runtime_error("failed to open file: " + filename);
        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        VK_CHECK(vkCreateShaderModule(m_context.getDevice(), &createInfo, nullptr, &shaderModule));
        return shaderModule;
    }

    // ============================================================
    //  GRAPHICS PIPELINE
    // ============================================================
    void createGraphicsPipeline() {
        VkDevice device = m_context.getDevice();
        auto vertShaderSPV = readFile("shaders/triangle.vert.spv");
        auto fragShaderSPV = readFile("shaders/triangle.frag.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderSPV);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderSPV);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstants);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));

        VkFormat offscreenFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkPipelineRenderingCreateInfo pipelineRenderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &offscreenFormat;
        pipelineRenderingInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.pNext = &pipelineRenderingInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline));

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    // ============================================================
    //  POST-PROCESS PIPELINE
    // ============================================================
    void createPostProcessPipeline() {
        VkDevice device = m_context.getDevice();
        auto vertShaderSPV = readFile("shaders/postprocess.vert.spv");
        auto fragShaderSPV = readFile("shaders/postprocess.frag.spv");
        VkShaderModule vertShaderModule = createShaderModule(vertShaderSPV);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderSPV);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        depthStencil.depthTestEnable = VK_FALSE;
        depthStencil.depthWriteEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PostProcessParams);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_postProcessDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_postProcessPipelineLayout));

        VkFormat swapchainFormat = m_swapchain.getImageFormat();
        VkPipelineRenderingCreateInfo pipelineRenderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &swapchainFormat;
        pipelineRenderingInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;

        VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        pipelineInfo.pNext = &pipelineRenderingInfo;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_postProcessPipelineLayout;

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_postProcessPipeline));

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    // ============================================================
    //  COMMAND POOL / BUFFERS / SYNC
    // ============================================================
    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = m_context.findQueueFamilies();
        VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
        VK_CHECK(vkCreateCommandPool(m_context.getDevice(), &poolInfo, nullptr, &m_commandPool));
    }

    void createCommandBuffers() {
        m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());
        VK_CHECK(vkAllocateCommandBuffers(m_context.getDevice(), &allocInfo, m_commandBuffers.data()));
    }

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
        VkImageLayout oldLayout, VkImageLayout newLayout,
        VkImageAspectFlags aspectMask) {
        VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspectMask;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags srcStage;
        VkPipelineStageFlags dstStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            barrier.dstAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
        else {
            throw std::runtime_error("unsupported image layout transition!");
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void createSyncObjects() {
        VkDevice device = m_context.getDevice();
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        VkFenceCreateInfo     fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]));
            VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]));
        }
    }

    // ============================================================
    //  IMGUI
    // ============================================================
    void initImGui() {
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER,                1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,       1000 }
        };
        VkDescriptorPoolCreateInfo pool_info{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = static_cast<uint32_t>(IM_ARRAYSIZE(pool_sizes));
        pool_info.pPoolSizes = pool_sizes;
        VK_CHECK(vkCreateDescriptorPool(m_context.getDevice(), &pool_info, nullptr, &m_imguiDescriptorPool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(m_window.getNativeWindow(), true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_context.getInstance();
        init_info.PhysicalDevice = m_context.getPhysicalDevice();
        init_info.Device = m_context.getDevice();
        init_info.QueueFamily = m_context.findQueueFamilies().graphicsFamily.value();
        init_info.Queue = m_context.getGraphicsQueue();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = m_imguiDescriptorPool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = static_cast<uint32_t>(m_swapchain.getImages().size());
        init_info.UseDynamicRendering = true;

        VkPipelineRenderingCreateInfo pipelineRenderingInfo{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        pipelineRenderingInfo.colorAttachmentCount = 1;
        pipelineRenderingInfo.pColorAttachmentFormats = &m_swapchainImageFormat;

        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.PipelineInfoMain.RenderPass = VK_NULL_HANDLE;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

        ImGui_ImplVulkan_Init(&init_info);
    }

    // ============================================================
    //  RECORD COMMAND BUFFER
    // ============================================================
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex,
        const std::vector<SceneObject>& objects, float time,
        const glm::mat4& view, const glm::mat4& proj) {
        (void)view; (void)proj; // уже в UBO

        VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        // --- PASS 1: scene -> offscreen ---
        transitionImageLayout(commandBuffer, m_offscreenImage,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT);
        transitionImageLayout(commandBuffer, m_depthImage,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_DEPTH_BIT);

        VkRenderingAttachmentInfo colorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        colorAttachment.imageView = m_offscreenImageView;
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue = { {{ m_fogColor.r, m_fogColor.g, m_fogColor.b, 1.0f }} };

        VkRenderingAttachmentInfo depthAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        depthAttachment.imageView = m_depthImageView;
        depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.clearValue.depthStencil = { 1.0f, 0 };

        VkRenderingInfo renderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        renderingInfo.renderArea = { {0, 0}, {m_renderWidth, m_renderHeight} };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;
        renderingInfo.pDepthAttachment = &depthAttachment;

        vkCmdBeginRendering(commandBuffer, &renderingInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);

        VkViewport viewport{};
        viewport.width = static_cast<float>(m_renderWidth);
        viewport.height = static_cast<float>(m_renderHeight);
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = { m_renderWidth, m_renderHeight };
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        for (const auto& obj : objects) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), obj.position);
            model = glm::scale(model, obj.scale);
            if (obj.isRotating) {
                model = glm::rotate(model, time * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            }

            uint32_t shells = (m_furEnabled && obj.isFurry) ? static_cast<uint32_t>(m_shellCount) : 1;

            for (uint32_t i = 0; i < shells; i++) {
                PushConstants pc{};
                pc.model = model;
                pc.furLayer = (shells > 1) ? (static_cast<float>(i) / static_cast<float>(shells - 1)) : 0.0f;
                pc.emissive = obj.emissive;

                vkCmdPushConstants(commandBuffer, m_pipelineLayout,
                    VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

                if (obj.useIdolMesh) {
                    m_idolMesh.bind(commandBuffer);
                    m_idolMesh.draw(commandBuffer);
                }
                else {
                    m_cubeMesh.bind(commandBuffer);
                    m_cubeMesh.draw(commandBuffer);
                }
            }
        }
        vkCmdEndRendering(commandBuffer);

        transitionImageLayout(commandBuffer, m_offscreenImage,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT);

        // --- PASS 2: post-process + ImGui -> swapchain ---
        VkImage  swapchainImage = m_swapchain.getImages()[imageIndex];
        VkExtent2D swapExtent = m_swapchain.getExtent();

        transitionImageLayout(commandBuffer, swapchainImage,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_ASPECT_COLOR_BIT);

        VkRenderingAttachmentInfo postColorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
        postColorAttachment.imageView = m_swapchain.getImageViews()[imageIndex];
        postColorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        postColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        postColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        postColorAttachment.clearValue = { {{0.0f, 0.0f, 0.0f, 1.0f}} };

        VkRenderingInfo postRenderingInfo{ VK_STRUCTURE_TYPE_RENDERING_INFO };
        postRenderingInfo.renderArea = { {0, 0}, swapExtent };
        postRenderingInfo.layerCount = 1;
        postRenderingInfo.colorAttachmentCount = 1;
        postRenderingInfo.pColorAttachments = &postColorAttachment;

        vkCmdBeginRendering(commandBuffer, &postRenderingInfo);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postProcessPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_postProcessPipelineLayout, 0, 1, &m_postProcessDescriptorSet, 0, nullptr);
        vkCmdPushConstants(commandBuffer, m_postProcessPipelineLayout,
            VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostProcessParams), &m_ppParams);

        VkViewport postViewport{};
        postViewport.width = static_cast<float>(swapExtent.width);
        postViewport.height = static_cast<float>(swapExtent.height);
        postViewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &postViewport);

        VkRect2D postScissor{};
        postScissor.extent = swapExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &postScissor);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
        vkCmdEndRendering(commandBuffer);

        transitionImageLayout(commandBuffer, swapchainImage,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            VK_IMAGE_ASPECT_COLOR_BIT);

        VK_CHECK(vkEndCommandBuffer(commandBuffer));
    }

    // ============================================================
    //  MAIN LOOP
    // ============================================================
    void mainLoop() {
        while (!m_window.shouldClose()) {
            m_window.pollEvents();

            // ќкно свЄрнуто (Alt+Tab / minimize): не рендерим и не пересоздаЄм swapchain.
            if (framebufferIsZero()) {
                glfwWaitEvents();
                m_lastFrameTime = static_cast<float>(glfwGetTime());
                continue;
            }

            float currentFrame = static_cast<float>(glfwGetTime());
            float deltaTime = currentFrame - m_lastFrameTime;
            m_lastFrameTime = currentFrame;

            GLFWwindow* window = m_window.getNativeWindow();

            if (glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS) {
                if (!m_f1Pressed) { m_f1Pressed = true; m_showGui = !m_showGui; }
            }
            else {
                m_f1Pressed = false;
            }

            static bool f11Pressed = false;
            if (glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS) {
                if (!f11Pressed) {
                    f11Pressed = true;
                    m_window.setFullscreen(!m_window.isFullscreen());
                    recreateSwapchain();
                }
            }
            else {
                f11Pressed = false;
            }

            m_gamepad.update();

            if (m_showGui) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            else {
                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    if (m_firstMouse) {
                        glfwGetCursorPos(window, &m_lastMouseX, &m_lastMouseY);
                        m_firstMouse = false;
                    }
                    double xpos, ypos;
                    glfwGetCursorPos(window, &xpos, &ypos);
                    m_camera.processMouseMovement(
                        static_cast<float>(xpos - m_lastMouseX),
                        static_cast<float>(m_lastMouseY - ypos));
                    m_lastMouseX = xpos;
                    m_lastMouseY = ypos;
                }
                else {
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    m_firstMouse = true;
                }
            }

            m_camera.processKeyboard(window, deltaTime);
            m_camera.processGamepad(m_gamepad.getLeftStick(), m_gamepad.getRightStick(),
                m_gamepad.getLeftTrigger(), m_gamepad.getRightTrigger(),
                deltaTime);

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            if (m_showGui) {
                ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
                ImGui::Begin("PS2 Atmosphere & Post-FX", &m_showGui);
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::Text("Gamepad: %s", m_gamepad.isConnected() ? "Connected" : "Disconnected");
                ImGui::Separator();

                // --- Display & Resolution ---
                ImGui::Text("Display & PS2 Resolution");
                bool isFullscreen = m_window.isFullscreen();
                if (ImGui::Checkbox("Fullscreen", &isFullscreen)) {
                    m_window.setFullscreen(isFullscreen);
                    recreateSwapchain();
                }

                int currentRes = 1;
                if (m_renderWidth == 512)  currentRes = 0;
                else if (m_renderWidth == 640)  currentRes = 1;
                else if (m_renderWidth == 800)  currentRes = 2;
                else if (m_renderWidth == 1280) currentRes = 3;

                const char* resolutions[] = {
                    "512x448 (PS2 NTSC Low)",
                    "640x448 (PS2 Standard)",
                    "800x448 (PS2 Widescreen 16:9)",
                    "1280x720 (HD Remaster)"
                };
                if (ImGui::Combo("Internal Resolution", &currentRes, resolutions, IM_ARRAYSIZE(resolutions))) {
                    if (currentRes == 0) recreateOffscreenResources(512, 448);
                    else if (currentRes == 1) recreateOffscreenResources(640, 448);
                    else if (currentRes == 2) recreateOffscreenResources(800, 448);
                    else if (currentRes == 3) recreateOffscreenResources(1280, 720);
                }
                ImGui::Separator();

                // --- Fog ---
                ImGui::Checkbox("Enable Fog", &m_fogEnabled);
                ImGui::SliderFloat("Fog Density", &m_fogDensity, 0.0f, 0.1f);
                ImGui::ColorEdit3("Fog Color", glm::value_ptr(m_fogColor));

                // --- Post-FX ---
                ImGui::Separator();
                ImGui::Text("Post-FX");
                ImGui::SliderFloat("Bloom Threshold", &m_ppParams.bloomThreshold, 0.0f, 1.0f);
                ImGui::SliderFloat("Bloom Intensity", &m_ppParams.bloomIntensity, 0.0f, 2.0f);
                ImGui::SliderFloat("Saturation", &m_ppParams.saturation, 0.0f, 1.5f);
                ImGui::SliderFloat("Dither Strength", &m_ppParams.ditherStrength, 0.0f, 2.0f);
                ImGui::Separator();

                // --- Fur / Grass ---
                ImGui::Text("Colossus Fur / Grass (Shells)");
                ImGui::Checkbox("Enable Fur", &m_furEnabled);
                ImGui::SliderInt("Shell Count", &m_shellCount, 4, 32);
                ImGui::SliderFloat("Fur Length", &m_furLength, 0.02f, 0.5f);
                ImGui::SliderFloat("Fur Density", &m_furDensity, 20.0f, 400.0f);
                ImGui::SliderFloat("Slope Threshold", &m_slopeThreshold, -1.0f, 0.95f);

                // --- Lighting ---
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Lighting (Gouraud + Dynamic)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    ImGui::SliderFloat("Sun Azimuth", &m_sunAzimuth, 0.0f, 6.2832f);
                    ImGui::SliderFloat("Sun Elevation", &m_sunElevation, 0.0f, 1.5708f);
                    ImGui::SliderFloat("Sun Intensity", &m_sunIntensity, 0.0f, 3.0f);
                    ImGui::ColorEdit3("Sky Ambient", glm::value_ptr(m_skyAmbient));
                    ImGui::ColorEdit3("Ground Shadow", glm::value_ptr(m_groundAmbient));
                    ImGui::Separator();
                    ImGui::Checkbox("Animate Point Light", &m_animatePointLight);
                    ImGui::SliderFloat("Point Light Intensity", &m_pointLightIntensity, 0.0f, 3.0f);
                    ImGui::ColorEdit3("Point Light Color", glm::value_ptr(m_pointLightColorBase));
                    if (!m_animatePointLight) {
                        ImGui::SliderFloat3("Point Light Pos", glm::value_ptr(m_pointLightPos), -10.0f, 10.0f);
                    }
                }

                ImGui::End();
            }

            ImGui::Render();
            drawFrame(deltaTime);
        }
        vkDeviceWaitIdle(m_context.getDevice());
    }

    // ============================================================
    //  DRAW FRAME
    // ============================================================
    void drawFrame(float deltaTime) {
        (void)deltaTime;

        // ќкно могло быть свЄрнуто между кадрами Ч не пытаемс€ рисовать.
        if (framebufferIsZero()) {
            return;
        }

        VkDevice device = m_context.getDevice();
        VK_CHECK(vkWaitForFences(device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX));

        uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(device, m_swapchain.getHandle(), UINT64_MAX,
            m_imageAvailableSemaphores[m_currentFrame],
            VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // ‘енс Ќ≈ сбрасываем Ч кадр не отправлен, синхронизаци€ не ломаетс€.
            recreateSwapchain();
            return;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            VK_CHECK(result);
        }

        const bool recreateAfterPresent = (result == VK_SUBOPTIMAL_KHR);

        VK_CHECK(vkResetFences(device, 1, &m_inFlightFences[m_currentFrame]));

        // --- Animate point light ---
        if (m_animatePointLight) {
            float t = m_lastFrameTime;
            float R = 3.5f;
            m_pointLightPos = glm::vec3(
                std::cos(t * 0.7f) * R,
                0.5f + std::sin(t * 1.3f) * 0.3f,
                std::sin(t * 0.7f) * R
            );
        }

        // --- Sun direction from azimuth / elevation ---
        glm::vec3 sunDir(
            std::cos(m_sunElevation) * std::cos(m_sunAzimuth),
            std::sin(m_sunElevation),
            std::cos(m_sunElevation) * std::sin(m_sunAzimuth)
        );

        // --- Scene objects ---
        std::vector<SceneObject> objects = {
            {{ 0.0f, -1.00f,  0.0f}, {1.0f, 1.0f, 1.0f},  false, true,  true  }, // Idol
            {{-3.0f, -1.00f, -3.0f}, {0.5f, 2.0f, 0.5f},  false, false, true  }, // Columns
            {{ 3.0f, -1.00f, -3.0f}, {0.5f, 2.0f, 0.5f},  false, false, true  },
            {{-3.0f, -1.00f,  3.0f}, {0.5f, 2.0f, 0.5f},  false, false, true  },
            {{ 3.0f, -1.00f,  3.0f}, {0.5f, 2.0f, 0.5f},  false, false, true  },
            {{ 0.0f, -1.05f,  0.0f}, {20.0f, 0.05f, 20.0f}, false, false, true } // Ground
        };

        // Visibly glowing fire cube at point-light position
        SceneObject fire;
        fire.position = m_pointLightPos;
        fire.scale = glm::vec3(0.2f);
        fire.isRotating = false;
        fire.useIdolMesh = false;
        fire.isFurry = false;
        fire.emissive = m_pointLightIntensity * 5.0f;
        objects.push_back(fire);

        // --- Camera matrices ---
        glm::mat4 view = m_camera.getViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(45.0f),
            m_renderWidth / static_cast<float>(m_renderHeight),
            0.1f, 100.0f);
        proj[1][1] *= -1; // Vulkan Y-down

        // --- Fill UBO ---
        SceneUBO ubo{};
        ubo.viewProj = proj * view;
        ubo.view = view;
        ubo.fogColorDensity = glm::vec4(m_fogColor, m_fogEnabled ? m_fogDensity : 0.0f);
        ubo.furParams = glm::vec4(m_lastFrameTime, m_furLength, m_furDensity, m_slopeThreshold);

        ubo.sunDirection = glm::vec4(glm::normalize(sunDir), 0.0f);
        ubo.sunColor = glm::vec4(glm::vec3(1.0f, 0.95f, 0.90f) * m_sunIntensity, 1.0f);

        ubo.skyAmbientColor = glm::vec4(m_skyAmbient, 1.0f);
        ubo.groundAmbientColor = glm::vec4(m_groundAmbient, 1.0f);

        ubo.pointLightPos = glm::vec4(m_pointLightPos, m_pointLightRadius);
        ubo.pointLightColor = glm::vec4(m_pointLightColorBase * m_pointLightIntensity, 1.0f);

        memcpy(m_sceneUboMapped, &ubo, sizeof(ubo));

        // --- Record & submit ---
        VK_CHECK(vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0));
        recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex,
            objects, m_lastFrameTime, view, proj);

        VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffers[m_currentFrame];

        VkSemaphore signalSemaphores[] = { m_swapchain.getRenderFinishedSemaphores()[imageIndex] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]));

        VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = { m_swapchain.getHandle() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_context.getPresentQueue(), &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || recreateAfterPresent) {
            recreateSwapchain();
        }
        else {
            VK_CHECK(result);
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ============================================================
    //  CLEANUP
    // ============================================================
    void cleanup() {
        VkDevice device = m_context.getDevice();
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);

            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            if (m_imguiDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, m_imguiDescriptorPool, nullptr);

            m_idolMesh.cleanup(m_context.getAllocator());
            m_cubeMesh.cleanup(m_context.getAllocator());
            m_texture.cleanup(device, m_context.getAllocator());

            if (m_postProcessDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, m_postProcessDescriptorPool, nullptr);
            if (m_postProcessDescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, m_postProcessDescriptorSetLayout, nullptr);

            if (m_descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
            if (m_descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);

            if (m_sceneUboMapped) vmaUnmapMemory(m_context.getAllocator(), m_sceneUboAllocation);
            if (m_sceneUboBuffer != VK_NULL_HANDLE) vmaDestroyBuffer(m_context.getAllocator(), m_sceneUboBuffer, m_sceneUboAllocation);

            if (m_offscreenSampler != VK_NULL_HANDLE) vkDestroySampler(device, m_offscreenSampler, nullptr);
            if (m_offscreenImageView != VK_NULL_HANDLE) vkDestroyImageView(device, m_offscreenImageView, nullptr);
            if (m_offscreenImage != VK_NULL_HANDLE) vmaDestroyImage(m_context.getAllocator(), m_offscreenImage, m_offscreenAllocation);

            if (m_depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, m_depthImageView, nullptr);
            if (m_depthImage != VK_NULL_HANDLE) vmaDestroyImage(m_context.getAllocator(), m_depthImage, m_depthAllocation);

            for (VkSemaphore semaphore : m_imageAvailableSemaphores) vkDestroySemaphore(device, semaphore, nullptr);
            for (VkFence fence : m_inFlightFences)          vkDestroyFence(device, fence, nullptr);

            vkDestroyCommandPool(device, m_commandPool, nullptr);

            if (m_postProcessPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, m_postProcessPipeline, nullptr);
            if (m_postProcessPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_postProcessPipelineLayout, nullptr);

            if (m_graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
            if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);

            m_swapchain.cleanup(device);
        }
        m_context.cleanup();
        m_window.cleanup();
    }
};

// ============================================================
//  ENTRY POINT
// ============================================================
int main() {
    HelloTriangleApplication app;
    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}