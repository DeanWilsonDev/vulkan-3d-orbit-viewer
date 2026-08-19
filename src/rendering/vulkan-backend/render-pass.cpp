#include "render-pass.hpp"

namespace Rendering::VulkanBackend {

namespace {


VkFormat ChooseDepthFormat(VkPhysicalDevice physicalDevice)
{
  VkFormat candidates[] = {
      VK_FORMAT_D32_SFLOAT,
      VK_FORMAT_D32_SFLOAT_S8_UINT,
      VK_FORMAT_D24_UNORM_S8_UINT,
  };
  for (VkFormat format : candidates) {
  }
}

}  // namespace

VkRenderPass RenderPass::GetRenderPassHandle() const
{
  return this->renderPass;
}

VkFramebuffer RenderPass::GetFramebuffer(uint32_t imageIndex) const
{
  return this->framebuffers[imageIndex];
};

VkFormat RenderPass::GetDepthFormat() const
{
  return this->depthFormat;
};

const std::array<VkClearValue, 2>& RenderPass::GetClearValues() const
{
  return this->clearValues;
}

}  // namespace Rendering::VulkanBackend
