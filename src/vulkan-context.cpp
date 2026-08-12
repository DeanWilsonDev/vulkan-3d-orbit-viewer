#include "vulkan-context.hpp"

#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Renderer3D {

namespace {

const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

const std::vector<const char*> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData
)
{
  const char* level =
      (severity * VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" : "WARNING";

  std::cerr << "[validation: " << level << "] " << data->pMessage << '\n';

  return VK_FALSE;
}

VkResult CreateDebugUtilsMessenger(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger
)
{
  auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
  );
  if (fn == nullptr) return VK_ERROR_EXTENSION_NOT_PRESENT;
  return fn(instance, createInfo, nullptr, messenger);
}

void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
  auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
  );
  if (fn != nullptr) fn(instance, messenger, nullptr);
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info)
{
  info = {};

  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

  info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  info.pfnUserCallback = DebugCallback;
}

bool ValidationLayerAvailable()
{
  uint32_t count = 0;
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> layers(count);
  vkEnumerateInstanceLayerProperties(&count, layers.data());
  for (const auto& layer : layers) {
    if (std::strcmp(layer.layerName, kValidationLayerName) == 0) return true;
  }
  return false;
}

VkApplicationInfo CreateVulkanApplicationInfo(const ApplicationInfo& applicationInfo)
{
  VkApplicationInfo vkAppInfo{};
  vkAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  vkAppInfo.pApplicationName = applicationInfo.applicationName;
  vkAppInfo.applicationVersion = applicationInfo.applicationVersion;
  vkAppInfo.pEngineName = applicationInfo.engineName;
  vkAppInfo.engineVersion = applicationInfo.engineVersion;
  vkAppInfo.apiVersion = applicationInfo.apiVersion;
  return vkAppInfo;
}

}  // namespace

VulkanContext::VulkanContext(SDL_Window* window, bool enableValidation)
    : enableValidation(enableValidation)
{
  CreateInstance(window);
  SetupDebugMessenger();
  CreateSurface(window);
  PickPhysicalDevice();
  CreateLogicalDevice();
}

VulkanContext::~VulkanContext()
{
  // Destroy in reverse order of creation

  if (this->logicalDevice != VK_NULL_HANDLE) {
    vkDestroyDevice(this->logicalDevice, nullptr);
  }

  if (this->surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
  }

  if (this->debugMessenger != VK_NULL_HANDLE) {
    DestroyDebugUtilsMessenger(this->instance, this->debugMessenger);
  }

  if (this->instance != VK_NULL_HANDLE) {
    vkDestroyInstance(this->instance, nullptr);
  }
}

void VulkanContext::CreateInstance(SDL_Window* window, const ApplicationInfo& applicationInfo)
{
  if (this->enableValidation && !ValidationLayerAvailable()) {
    std::cerr << "[vulkan] validation layers requested but not avilable; continuing without\n";
    this->enableValidation = false;
  }

  VkApplicationInfo vkAppInfo = CreateVulkanApplicationInfo(applicationInfo);

  // Instance Level Extensions
  uint32_t sdlExtCount = 0;
  const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);

  if (sdlExtensions == nullptr) {
    throw std::runtime_error(
        std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError()
    );
  }

  std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtCount);

  if (this->enableValidation) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  VkInstanceCreateFlags flags = 0;

#ifdef __APPLE__
  // MoltenVK Extensions for macOS

  extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENTION_NAME);
  flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
}

}  // namespace Renderer3D
