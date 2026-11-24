#include "OffscreenRenderer.h"
#include "Mesh.h"
#include "Texture.h"
#include "Swapchain.h"
#include "VulkanContext.h"
#include "VulkanUtils.h"
#include "Vertex.h"

#include <imgui/imgui.h>
#include <imgui/imgui_impl_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

void OffscreenRenderer::init(VulkanContext& context, VkFormat swapchainFormat,
    VkCommandPool commandPool,
    Mesh* cubeMesh, Mesh* idolMesh, Texture* texture,
    uint32_t initialWidth, uint32_t initialHeight) {
    (void)commandPool; // зарезервировано, меши создаютс€ снаружи

    m_cubeMesh = cubeMesh;
    m_idolMesh = idolMesh;
    m_texture = texture;
    m_renderWidth = initialWidth;
    m_renderHeight = initialHeight;

    // 1. —начала изображени€ и сэмплеры Ч они нужны дл€ дескрипторов пост-обработки
    createOffscreenResources(context);
    createDepthResources(context);

    // 2. UBO
    createUniformBuffer(context);

    // 3. ƒескрипторы сцены
    createSceneDescriptorSetLayout(context);
    createSceneDescriptorPool(context);
    createSceneDescriptorSet(context);

    // 4. ƒескрипторы пост-обработки (используют уже созданные m_offscreenImageView / m_offscreenSampler)
    createPostProcessDescriptorSetLayout(context);
    createPostProcessDescriptorPool(context);
    createPostProcessDescriptorSet(context);

    // 5. ѕайплайны
    createGraphicsPipeline(context);
    createPostProcessPipeline(context, swapchainFormat);
}

void OffscreenRenderer::cleanup(VulkanContext& context) {
    VkDevice device = context.getDevice();
    if (device == VK_NULL_HANDLE) return;

    if (m_graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
    if (m_postProcessPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, m_postProcessPipeline, nullptr);
    if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    if (m_postProcessPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_postProcessPipelineLayout, nullptr);

    if (m_descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    if (m_descriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
    if (m_postProcessDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, m_postProcessDescriptorPool, nullptr);
    if (m_postProcessDescriptorSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, m_postProcessDescriptorSetLayout, nullptr);

    if (m_offscreenSampler != VK_NULL_HANDLE) vkDestroySampler(device, m_offscreenSampler, nullptr);
    if (m_offscreenImageView != VK_NULL_HANDLE) vkDestroyImageView(device, m_offscreenImageView, nullptr);
    if (m_offscreenImage != VK_NULL_HANDLE) vmaDestroyImage(context.getAllocator(), m_offscreenImage, m_offscreenAllocation);

    if (m_depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, m_depthImageView, nullptr);
    if (m_depthImage != VK_NULL_HANDLE) vmaDestroyImage(context.getAllocator(), m_depthImage, m_depthAllocation);

    if (m_sceneUboMapped) vmaUnmapMemory(context.getAllocator(), m_sceneUboAllocation);
    if (m_sceneUboBuffer != VK_NULL_HANDLE) vmaDestroyBuffer(context.getAllocator(), m_sceneUboBuffer, m_sceneUboAllocation);

    m_graphicsPipeline = VK_NULL_HANDLE;
    m_postProcessPipeline = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_postProcessPipelineLayout = VK_NULL_HANDLE;
    m_descriptorPool = VK_NULL_HANDLE;
    m_descriptorSetLayout = VK_NULL_HANDLE;
    m_postProcessDescriptorPool = VK_NULL_HANDLE;
    m_postProcessDescriptorSetLayout = VK_NULL_HANDLE;
    m_offscreenSampler = VK_NULL_HANDLE;
    m_offscreenImageView = VK_NULL_HANDLE;
    m_offscreenImage = VK_NULL_HANDLE;
    m_offscreenAllocation = VK_NULL_HANDLE;
    m_depthImageView = VK_NULL_HANDLE;
    m_depthImage = VK_NULL_HANDLE;
    m_depthAllocation = VK_NULL_HANDLE;
    m_sceneUboBuffer = VK_NULL_HANDLE;
    m_sceneUboAllocation = VK_NULL_HANDLE;
    m_sceneUboMapped = nullptr;
}

void OffscreenRenderer::recreateOffscreen(VulkanContext& context, uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(context.getDevice());

    if (m_offscreenSampler != VK_NULL_HANDLE) vkDestroySampler(context.getDevice(), m_offscreenSampler, nullptr);
    if (m_offscreenImageView != VK_NULL_HANDLE) vkDestroyImageView(context.getDevice(), m_offscreenImageView, nullptr);
    if (m_offscreenImage != VK_NULL_HANDLE) vmaDestroyImage(context.getAllocator(), m_offscreenImage, m_offscreenAllocation);
    if (m_depthImageView != VK_NULL_HANDLE) vkDestroyImageView(context.getDevice(), m_depthImageView, nullptr);
    if (m_depthImage != VK_NULL_HANDLE) vmaDestroyImage(context.getAllocator(), m_depthImage, m_depthAllocation);

    m_offscreenImage = VK_NULL_HANDLE;
    m_offscreenAllocation = VK_NULL_HANDLE;
    m_offscreenImageView = VK_NULL_HANDLE;
    m_offscreenSampler = VK_NULL_HANDLE;
    m_depthImage = VK_NULL_HANDLE;
    m_depthAllocation = VK_NULL_HANDLE;
    m_depthImageView = VK_NULL_HANDLE;

    m_renderWidth = width;
    m_renderHeight = height;

    createOffscreenResources(context);
    createDepthResources(context);
    updatePostProcessDescriptorSet(context);
}

// ============================================================
//  BUFFERS
// ============================================================
void OffscreenRenderer::createUniformBuffer(VulkanContext& context) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(SceneUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(context.getAllocator(), &bufferInfo, &allocInfo,
        &m_sceneUboBuffer, &m_sceneUboAllocation, nullptr));
    VK_CHECK(vmaMapMemory(context.getAllocator(), m_sceneUboAllocation, &m_sceneUboMapped));
}

// ============================================================
//  OFFSCREEN IMAGE / DEPTH
// ============================================================
void OffscreenRenderer::createOffscreenResources(VulkanContext& context) {
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

    VK_CHECK(vmaCreateImage(context.getAllocator(), &imageInfo, &allocInfo,
        &m_offscreenImage, &m_offscreenAllocation, nullptr));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_offscreenImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(context.getDevice(), &viewInfo, nullptr, &m_offscreenImageView));

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    VK_CHECK(vkCreateSampler(context.getDevice(), &samplerInfo, nullptr, &m_offscreenSampler));
}

void OffscreenRenderer::createDepthResources(VulkanContext& context) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_renderWidth;
    imageInfo.extent.height = m_renderHeight;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(context.getAllocator(), &imageInfo, &allocInfo,
        &m_depthImage, &m_depthAllocation, nullptr));

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(context.getDevice(), &viewInfo, nullptr, &m_depthImageView));
}

// ============================================================
//  DESCRIPTORS Ч SCENE
// ============================================================
void OffscreenRenderer::createSceneDescriptorSetLayout(VulkanContext& context) {
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[1].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(context.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout));
}

void OffscreenRenderer::createSceneDescriptorPool(VulkanContext& context) {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;
    VK_CHECK(vkCreateDescriptorPool(context.getDevice(), &poolInfo, nullptr, &m_descriptorPool));
}

void OffscreenRenderer::createSceneDescriptorSet(VulkanContext& context) {
    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(context.getDevice(), &allocInfo, &m_descriptorSet));
    updateSceneDescriptorSet(context);
}

void OffscreenRenderer::updateSceneDescriptorSet(VulkanContext& context) {
    if (m_texture == nullptr) return;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_texture->getImageView();
    imageInfo.sampler = m_texture->getSampler();

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

    vkUpdateDescriptorSets(context.getDevice(),
        static_cast<uint32_t>(descriptorWrites.size()),
        descriptorWrites.data(), 0, nullptr);
}

// ============================================================
//  DESCRIPTORS Ч POST-PROCESS
// ============================================================
void OffscreenRenderer::createPostProcessDescriptorSetLayout(VulkanContext& context) {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(context.getDevice(), &layoutInfo, nullptr, &m_postProcessDescriptorSetLayout));
}

void OffscreenRenderer::createPostProcessDescriptorPool(VulkanContext& context) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    VK_CHECK(vkCreateDescriptorPool(context.getDevice(), &poolInfo, nullptr, &m_postProcessDescriptorPool));
}

void OffscreenRenderer::createPostProcessDescriptorSet(VulkanContext& context) {
    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = m_postProcessDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_postProcessDescriptorSetLayout;

    VK_CHECK(vkAllocateDescriptorSets(context.getDevice(), &allocInfo, &m_postProcessDescriptorSet));
    updatePostProcessDescriptorSet(context);
}

void OffscreenRenderer::updatePostProcessDescriptorSet(VulkanContext& context) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_offscreenImageView;
    imageInfo.sampler = m_offscreenSampler;

    VkWriteDescriptorSet descriptorWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    descriptorWrite.dstSet = m_postProcessDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(context.getDevice(), 1, &descriptorWrite, 0, nullptr);
}

// ============================================================
//  SHADERS
// ============================================================
std::vector<char> OffscreenRenderer::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open file: " + filename);
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}

VkShaderModule OffscreenRenderer::createShaderModule(VulkanContext& context, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(context.getDevice(), &createInfo, nullptr, &shaderModule));
    return shaderModule;
}

// ============================================================
//  GRAPHICS PIPELINE (main pass)
// ============================================================
void OffscreenRenderer::createGraphicsPipeline(VulkanContext& context) {
    VkDevice device = context.getDevice();
    auto vertShaderSPV = readFile("shaders/triangle.vert.spv");
    auto fragShaderSPV = readFile("shaders/triangle.frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(context, vertShaderSPV);
    VkShaderModule fragShaderModule = createShaderModule(context, fragShaderSPV);

    VkPipelineShaderStageCreateInfo vertStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

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
void OffscreenRenderer::createPostProcessPipeline(VulkanContext& context, VkFormat swapchainFormat) {
    VkDevice device = context.getDevice();
    auto vertShaderSPV = readFile("shaders/postprocess.vert.spv");
    auto fragShaderSPV = readFile("shaders/postprocess.frag.spv");
    VkShaderModule vertShaderModule = createShaderModule(context, vertShaderSPV);
    VkShaderModule fragShaderModule = createShaderModule(context, fragShaderSPV);

    VkPipelineShaderStageCreateInfo vertStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragStageInfo{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

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
//  LAYOUT TRANSITIONS
// ============================================================
void OffscreenRenderer::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
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

// ============================================================
//  UBO FILL
// ============================================================
void OffscreenRenderer::fillUbo(const EngineSettings& settings, float time,
    const glm::mat4& view, const glm::mat4& proj) {
    glm::vec3 sunDir(
        std::cos(settings.sunElevation) * std::cos(settings.sunAzimuth),
        std::sin(settings.sunElevation),
        std::cos(settings.sunElevation) * std::sin(settings.sunAzimuth)
    );

    SceneUBO ubo{};
    ubo.viewProj = proj * view;
    ubo.view = view;
    ubo.fogColorDensity = glm::vec4(settings.fogColor, settings.fogEnabled ? settings.fogDensity : 0.0f);
    ubo.furParams = glm::vec4(time, settings.furLength, settings.furDensity, settings.slopeThreshold);
    ubo.sunDirection = glm::vec4(glm::normalize(sunDir), 0.0f);
    ubo.sunColor = glm::vec4(glm::vec3(1.0f, 0.95f, 0.90f) * settings.sunIntensity, 1.0f);
    ubo.skyAmbientColor = glm::vec4(settings.skyAmbient, 1.0f);
    ubo.groundAmbientColor = glm::vec4(settings.groundAmbient, 1.0f);
    ubo.pointLightPos = glm::vec4(settings.pointLightPos, settings.pointLightRadius);
    ubo.pointLightColor = glm::vec4(settings.pointLightColorBase * settings.pointLightIntensity, 1.0f);

    memcpy(m_sceneUboMapped, &ubo, sizeof(ubo));
}

// ============================================================
//  RECORD COMMAND BUFFER
// ============================================================
void OffscreenRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer,
    Swapchain& swapchain,
    uint32_t imageIndex,
    const std::vector<SceneObject>& objects,
    const EngineSettings& settings,
    float time,
    const glm::mat4& view,
    const glm::mat4& proj) {
    fillUbo(settings, time, view, proj);

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
    colorAttachment.clearValue = { {{ settings.fogColor.r, settings.fogColor.g, settings.fogColor.b, 1.0f }} };

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

    // === Instanced fur: один vkCmdDrawIndexed на объект ===
    for (const auto& obj : objects) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), obj.position);
        model = glm::scale(model, obj.scale);
        if (obj.isRotating) {
            model = glm::rotate(model, time * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        uint32_t shells = (settings.furEnabled && obj.isFurry)
            ? static_cast<uint32_t>(settings.shellCount)
            : 1u;

        PushConstants pc{};
        pc.model = model;
        pc.furShellCount = shells;
        pc.emissive = obj.emissive;

        vkCmdPushConstants(commandBuffer, m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

        if (obj.useIdolMesh && m_idolMesh) {
            m_idolMesh->bind(commandBuffer);
            m_idolMesh->drawInstanced(commandBuffer, shells);
        }
        else if (m_cubeMesh) {
            m_cubeMesh->bind(commandBuffer);
            m_cubeMesh->drawInstanced(commandBuffer, shells);
        }
    }
    vkCmdEndRendering(commandBuffer);

    transitionImageLayout(commandBuffer, m_offscreenImage,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT);

    // --- PASS 2: post-process + ImGui -> swapchain ---
    VkImage swapchainImage = swapchain.getImages()[imageIndex];
    VkExtent2D swapExtent = swapchain.getExtent();

    transitionImageLayout(commandBuffer, swapchainImage,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_ASPECT_COLOR_BIT);

    VkRenderingAttachmentInfo postColorAttachment{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
    postColorAttachment.imageView = swapchain.getImageViews()[imageIndex];
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
        VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PostProcessParams), &settings.ppParams);

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