#pragma once

#include <vulkan/vulkan.h>

#include <optional>
#include <stdexcept>
#include <string>

// Проверка VkResult. Бросает std::runtime_error с именем вызова и кодом ошибки.
// НЕ применять к vkAcquireNextImageKHR / vkQueuePresentKHR: там штатно
// возвращаются VK_SUBOPTIMAL_KHR / VK_TIMEOUT.
#define VK_CHECK(call) do { \
    VkResult result_ = (call); \
    if (result_ != VK_SUCCESS) { \
        throw std::runtime_error(std::string("Vulkan error: ") + #call + " (code " + \
            std::to_string(static_cast<int>(result_)) + ")"); \
    } \
} while (0)

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};