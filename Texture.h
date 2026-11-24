#pragma once

#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>
#include <string>

class VulkanContext;

class Texture {
public:
    Texture() = default;

    void init(VulkanContext& context, VkCommandPool commandPool, const std::string& filepath);
    void cleanup(VkDevice device, VmaAllocator allocator);

    VkImageView getImageView() const { return m_imageView; }
    VkSampler getSampler() const { return m_sampler; }

private:
    VkImage m_image = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
};