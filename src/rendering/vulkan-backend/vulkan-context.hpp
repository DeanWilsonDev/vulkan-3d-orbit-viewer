#pragma once

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>
#include "application-info.hpp"

#include <string>
#include <cstdint>
#include <optional>

namespace Rendering::VulkanBackend {

struct QueueFamilyIndices {
  std::optional<uint32_t> graphics;
  std::optional<uint32_t> present;

  bool IsComplete() const { return this->graphics.has_value() && this->present.has_value(); }
};

class VulkanContext {
 public:
  VulkanContext(SDL_Window* window, const ApplicationInfo& appInfo, bool enableValidation);

  ~VulkanContext();

  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;

  VkInstance GetInstance() const { return this->instance; }
  VkSurfaceKHR GetSurface() const { return this->surface; }
  VkPhysicalDevice GetPhysicalDevice() const { return this->physicalDevice; }
  VkDevice GetLogicalDevice() const { return this->logicalDevice; }
  VkQueue GetGraphicsQueue() const { return this->graphicsQueue; }
  VkQueue GetPresentQueue() const { return this->presentQueue; }
  const QueueFamilyIndices& GetQueueFamilies() const { return this->queueFamilies; }

  void WaitIdle() const;

 private:
  void CreateInstance(SDL_Window* window, const ApplicationInfo& applicationInfo);
  void SetupDebugMessenger();
  void CreateSurface(SDL_Window* window);
  void PickPhysicalDevice();
  void CreateLogicalDevice();

  bool enableValidation = false;

  VkInstance instance = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice logicalDevice = VK_NULL_HANDLE;
  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue presentQueue = VK_NULL_HANDLE;

  QueueFamilyIndices queueFamilies;
};
}  // namespace Rendering::VulkanBackend
