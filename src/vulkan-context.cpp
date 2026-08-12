#include "vulkan-context.hpp"

#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace VulkanOrbitViewer {
const char *kValidationLayerName = "VK_LAYER_KHRONOS_validation";

const std::vector<const char *> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *data, void *userData) {

  const char *level = (severity * VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
                          ? "ERROR"
                          : "WARNING";

  std::cerr << "[validation: " << level << "] " << data->pMessage << '\n';

  return VK_FALSE;
}



} // namespace VulkanOrbitViewer
