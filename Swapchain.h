#pragma once

#include <vulkan/vulkan.h>

#include <vector>

class VulkanContext;
class Window;

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

class Swapchain {
public:
    Swapchain() = default;
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Создаёт VkSwapchainKHR, VkImage, VkImageView и renderFinishedSemaphores.
    // Требует инициализированного VulkanContext (device) и Window (для extent).
    // При ошибке освобождает уже созданные объекты и пробрасывает исключение.
    void init(VulkanContext& context, Window& window);

    // Освобождает все ресурсы класса. Безопасно вызывать повторно.
    void cleanup(VkDevice device);

    VkSwapchainKHR getHandle() const { return m_swapChain; }
    VkFormat getImageFormat() const { return m_imageFormat; }
    VkExtent2D getExtent() const { return m_extent; }
    const std::vector<VkImage>& getImages() const { return m_images; }
    const std::vector<VkImageView>& getImageViews() const { return m_imageViews; }
    const std::vector<VkSemaphore>& getRenderFinishedSemaphores() const { return m_renderFinishedSemaphores; }

private:
    void createSwapChain(VulkanContext& context, Window& window);
    void createImageViews(VkDevice device);
    void createRenderFinishedSemaphores(VkDevice device);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window& window) const;

    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};
    std::vector<VkImageView> m_imageViews;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
};