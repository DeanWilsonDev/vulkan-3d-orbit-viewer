#include "swapchain.h"

#include "vulkan_context.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

// Everything the surface tells us about what swapchains it will accept: how many
// images, at what sizes, in which colour formats, and with which present modes.
// All three are properties of the (physical device, surface) pair.
// See Glossary: SURFACE, COLOUR_FORMAT, PRESENT_MODE
struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR        capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

SwapchainSupport querySupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    support.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, nullptr);
    support.presentModes.resize(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount, support.presentModes.data());
    return support;
}

// Choose the pixel layout and colour space of the swapchain images. We prefer
// 8-bit BGRA in the sRGB colour space: sRGB matches how displays actually emit
// light, so colours look correct without us hand-correcting them. If it is not
// offered we fall back to whatever the surface lists first.
// See Glossary: COLOUR_FORMAT, COLOUR_SPACE
VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    return formats[0];
}

// Choose how finished images are handed to the display. MAILBOX keeps the most
// recent image and replaces any waiting one — low latency with no tearing, but
// it costs an extra image (triple buffering). FIFO is a strict queue synced to
// vblank — the only mode the spec guarantees, equivalent to classic v-sync. We
// prefer MAILBOX and fall back to FIFO. See Glossary: PRESENT_MODE,
// TRIPLE_BUFFERING, SCREEN_TEARING
VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (const auto& m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    return VK_PRESENT_MODE_FIFO_KHR;  // guaranteed to exist
}

// Choose the swapchain image size in pixels. Usually the surface dictates it
// exactly (currentExtent); when it instead reports the sentinel 0xFFFFFFFF, the
// window manager is leaving the choice to us, so we use the window's drawable
// pixel size clamped to the surface's allowed range. Drawable pixels (not
// logical points) matter on HiDPI/Retina displays where they differ.
VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, SDL_Window* window) {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return caps.currentExtent;
    }
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    VkExtent2D extent{static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return extent;
}

// A readable name for the handful of formats we are likely to get, so the
// startup log is meaningful rather than a bare enum number.
const char* formatName(VkFormat format) {
    switch (format) {
        case VK_FORMAT_B8G8R8A8_SRGB:  return "B8G8R8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SRGB:  return "R8G8B8A8_SRGB";
        case VK_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        default:                       return "other";
    }
}

} // namespace

Swapchain::Swapchain(VulkanContext& context, SDL_Window* window)
    : m_context(context), m_window(window) {
    create();
}

Swapchain::~Swapchain() {
    destroy();
}

void Swapchain::recreate() {
    // The window may be minimised mid-resize, giving a zero drawable size. A
    // zero-extent swapchain is invalid, so skip recreation until the window has
    // a real size again (a later resize event will trigger us once restored).
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    if (w == 0 || h == 0) return;

    // The GPU may still be reading the current images, so we must wait until the
    // device is idle before destroying them. (Once we have a frame loop, this
    // is the one heavy stall we accept on the rare event of a resize.)
    // See Glossary: SWAPCHAIN
    m_context.waitIdle();

    destroy();
    create();
}

void Swapchain::create() {
    SwapchainSupport support = querySupport(m_context.physicalDevice(), m_context.surface());

    VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(support.formats);
    VkPresentModeKHR   presentMode   = choosePresentMode(support.presentModes);
    VkExtent2D         extent        = chooseExtent(support.capabilities, m_window);

    // Request one more image than the minimum so the application always has an
    // image to draw into while the presentation engine displays another — i.e.
    // at least double buffering. Respect the surface's maximum (0 means "no
    // maximum"). See Glossary: DOUBLE_BUFFERING, TRIPLE_BUFFERING
    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 &&
        imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_context.surface();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;  // not stereoscopic, so a single layer per image
    // The images are render targets (colour attachments) we draw into.
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // If the graphics and present queue families differ, the images are accessed
    // by two families and must be shared (CONCURRENT). When they are the same
    // family (the common case, and true on this Mac) EXCLUSIVE is faster because
    // it avoids ownership-transfer bookkeeping. See Glossary: QUEUE_FAMILY
    const QueueFamilyIndices& qf = m_context.queueFamilies();
    uint32_t familyIndices[] = {*qf.graphics, *qf.present};
    if (qf.graphics != qf.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = familyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    // Apply no extra rotation/flip (use whatever the surface currently has), and
    // do not blend the window with others behind it (opaque).
    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    // clipped = true lets the driver skip shading pixels hidden by another
    // window — a free optimisation since we never read presented pixels back.
    createInfo.clipped = VK_TRUE;
    // No old swapchain to hand over: recreate() fully destroys the previous one
    // before calling create(), so this is always a fresh build.
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_context.device(), &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateSwapchainKHR failed");
    }

    // The driver may create more images than we asked for, so query the real
    // count. These VkImages are owned by the swapchain and must NOT be destroyed
    // by us — only the views we wrap around them are ours to destroy.
    uint32_t actualCount = 0;
    vkGetSwapchainImagesKHR(m_context.device(), m_swapchain, &actualCount, nullptr);
    m_images.resize(actualCount);
    vkGetSwapchainImagesKHR(m_context.device(), m_swapchain, &actualCount, m_images.data());

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    // An image view describes how to interpret an image: which part, which
    // format, which channels. You never bind a raw VkImage to the pipeline — you
    // bind a view of it. Here each view exposes the whole 2D colour image.
    // See Glossary: IMAGE_VIEW
    m_imageViews.resize(m_images.size());
    for (size_t i = 0; i < m_images.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;   // a flat 2D image
        viewInfo.format = m_imageFormat;
        // Identity swizzle: channels map straight through, no remapping.
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                               VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        // We view the colour data, the single mip level, and the single layer.
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_context.device(), &viewInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateImageView failed for a swapchain image");
        }
    }

    std::cout << "[vulkan] swapchain created: " << m_images.size() << " images, format "
              << formatName(m_imageFormat) << ", extent " << m_extent.width << "x"
              << m_extent.height << ", present mode "
              << (presentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : "FIFO") << '\n';
}

void Swapchain::destroy() {
    // Destroy the views we created (the images themselves belong to the
    // swapchain and are freed when the swapchain is destroyed).
    for (VkImageView view : m_imageViews) {
        vkDestroyImageView(m_context.device(), view, nullptr);
    }
    m_imageViews.clear();
    m_images.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_context.device(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}
