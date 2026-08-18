#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL_video.h>
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>

namespace Rendering::VulkanBackend {
class VulkanContext;

class Swapchain {
 public:
  // Side Quest: This should really reference the SdlContext to get the window instead of the window
  // directly
  Swapchain(VulkanContext& context, SDL_Window* window);

  ~Swapchain();

  // Owns unique GPU resources.
  // Not copyable
  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  /*
   * Recreate:
   * Destroys and rebuilds the swapchain for the window's current size.
   * Called when the window is resized since the old size's images
   * no longer match the surface, so they must be replaced.
   * Safely does nothing while the window is minimised (zero drawable seize).
   */
  void Recreate();

  VkSwapchainKHR GetSwapchainHandle() const;
  VkFormat GetImageFormat() const;
  VkExtent2D GetExtent() const;
  const std::vector<VkImage>& GetImages() const;
  const std::vector<VkImageView>& GetImageViews() const;
  uint32_t ImageCount() const;

 private:
  /*
   * Create:
   * Build the swapchain + image views for the current size
   */
  void Create();

  /*
   * Destroy:
   * Tear down the swapchain + image views
   * (used by the destructor and Recreate)
   */
  void Destroy();

  VulkanContext& context;
  SDL_Window* window = nullptr;

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  std::vector<VkImage> images;
  std::vector<VkImageView> imageViews;
  // color format the images were created with
  VkFormat imageFormat{};
  // Pixel size the images were created at
  VkExtent2D extent;
};

}  // namespace Renderer3D
