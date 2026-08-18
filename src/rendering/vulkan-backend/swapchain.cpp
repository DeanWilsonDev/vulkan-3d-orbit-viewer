#include "swapchain.hpp"

#include "vulkan-context.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Rendering::VulkanBackend {

namespace {

struct SwapchainSupport {
  VkSurfaceCapabilitiesKHR capabilties{};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

SwapchainSupport QuerySupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
  SwapchainSupport support;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilties);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
  support.formats.resize(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());

  uint32_t modeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, nullptr);
  support.presentModes.resize(modeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(
      device, surface, &modeCount, support.presentModes.data()
  );

  return support;
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
  for (const auto& f : formats) {
    if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return f;
    }
  }
  return formats[0];
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
{
  for (const auto& mode : modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return VK_PRESENT_MODE_MAILBOX_KHR;
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, SDL_Window* window)
{
  if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return caps.currentExtent;
  }
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  VkExtent2D extent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
  extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
  extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
  return extent;
}

const char* FormatName(VkFormat format)
{
  switch (format) {
    case VK_FORMAT_B8G8R8A8_SRGB:
      return "B8G8R8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM:
      return "B8G8R8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_SRGB:
      return "R8G8B8A8_SRGB";
    case VK_FORMAT_R8G8B8A8_UNORM:
      return "R8G8B8A8_UNORM";
    default:
      return "other";
  }
}
}  // namespace

Swapchain::Swapchain(VulkanContext& context, SDL_Window* window) : context(context), window(window)
{
  this->Create();
}

Swapchain::~Swapchain()
{
  this->Destroy();
}

void Swapchain::Recreate()
{
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(this->window, &w, &h);
  if (w == 0 || h == 0) return;

  this->context.WaitIdle();

  this->Destroy();
  this->Create();
}

void Swapchain::Create()
{
  SwapchainSupport support =
      QuerySupport(this->context.GetPhysicalDevice(), this->context.GetSurface());
  VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(support.formats);
  VkPresentModeKHR presentMode = ChoosePresentMode(support.presentModes);
  VkExtent2D extent = ChooseExtent(support.capabilties, this->window);

  uint32_t imageCount = support.capabilties.minImageCount + 1;
  if (support.capabilties.maxImageCount > 0 && imageCount > support.capabilties.maxImageCount) {
    imageCount = support.capabilties.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = this->context.GetSurface();
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  const QueueFamilyIndices& queueFamilies = this->context.GetQueueFamilies();
  uint32_t familyIndices[] = {*queueFamilies.graphics, *queueFamilies.present};
  if (queueFamilies.graphics != queueFamilies.present) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = familyIndices;
  }
  else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = support.capabilties.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(
          this->context.GetLogicalDevice(), &createInfo, nullptr, &this->swapchain
      ) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateSwapchainKHR failed");
  }

  uint32_t actualCount = 0;
  vkGetSwapchainImagesKHR(this->context.GetLogicalDevice(), this->swapchain, &actualCount, nullptr);
  this->images.resize(actualCount);
  vkGetSwapchainImagesKHR(
      this->context.GetLogicalDevice(), this->swapchain, &actualCount, this->images.data()
  );

  this->imageFormat = surfaceFormat.format;
  this->extent = extent;

  this->imageViews.resize(this->images.size());
  for (size_t i = 0; i < this->images.size(); ++i) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = this->images[i];
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = this->imageFormat;
    viewInfo.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(
            this->context.GetLogicalDevice(), &viewInfo, nullptr, &this->imageViews[i]
        ) != VK_SUCCESS) {
      throw std::runtime_error("vkCreateImageView failed for a swapchain image");
    }
  }

  std::cout << "[vulkan] swapchain created: " << this->images.size() << " images, format "
            << FormatName(this->imageFormat) << ", extent " << this->extent.width << "x"
            << this->extent.height << ", present mode "
            << (presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO") << '\n';
}

VkSwapchainKHR Swapchain::GetSwapchainHandle() const
{
  return this->swapchain;
}

VkFormat Swapchain::GetImageFormat() const
{
  return this->imageFormat;
}

VkExtent2D Swapchain::GetExtent() const
{
  return this->extent;
}

const std::vector<VkImage>& Swapchain::GetImages() const
{
  return this->images;
}
const std::vector<VkImageView>& Swapchain::GetImageViews() const
{
  return this->imageViews;
}
uint32_t Swapchain::ImageCount() const
{
  return static_cast<uint32_t>(this->images.size());
}

}  // namespace Rendering::VulkanBackend
