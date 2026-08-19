#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <array>
#include <vector>

namespace Rendering::VulkanBackend {

class VulkanContext;
class Swapchain;

class RenderPass {
 public:
  RenderPass(VulkanContext& context, Swapchain& swapchain);

  ~RenderPass();

  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  VkRenderPass GetRenderPassHandle() const;
  VkFramebuffer GetFramebuffer(uint32_t imageIndex) const;
  VkFormat GetDepthFormat() const;

  const std::array<VkClearValue, 2>& GetClearValues() const;

 private:
  void CreateRenderPass();
  void CreateDepthResources();
  void CreateFramebuffers();
  void DestroyTargets();

  VulkanContext& context;
  Swapchain& swapchain;
  VkRenderPass renderPass = VK_NULL_HANDLE;
  VkFormat depthFormat{};
  VkImage depthImage = VK_NULL_HANDLE;
  VkDeviceMemory depthMemory = VK_NULL_HANDLE;
  VkImageView depthView = VK_NULL_HANDLE;

  std::vector<VkFramebuffer> framebuffers;
  std::array<VkClearValue, 2> clearValues{};
};

}  // namespace Rendering::VulkanBackend
