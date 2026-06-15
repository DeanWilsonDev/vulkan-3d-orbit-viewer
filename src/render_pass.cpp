#include "render_pass.h"

#include "swapchain.h"
#include "vulkan_context.h"

#include <iostream>
#include <stdexcept>

namespace {

// Pick a depth format the GPU supports as a depth/stencil attachment with
// optimal tiling. We prefer a pure 32-bit float depth; the combined
// depth+stencil formats are common fallbacks. See Glossary: DEPTH_BUFFER
VkFormat chooseDepthFormat(VkPhysicalDevice physicalDevice) {
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }
    throw std::runtime_error("no supported depth format found");
}

// True if a depth format also carries a stencil component (affects which image
// aspect the view must expose).
bool hasStencil(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

// Find a memory type index that matches the image's requirements and has the
// requested properties (here, device-local). A full treatment of Vulkan memory
// types lives in Chunk 7; this is the minimum the depth image needs.
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                        VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool typeAllowed = typeFilter & (1u << i);
        const bool hasProps = (memProps.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeAllowed && hasProps) return i;
    }
    throw std::runtime_error("no suitable memory type for the depth buffer");
}

} // namespace

RenderPass::RenderPass(VulkanContext& context, Swapchain& swapchain)
    : m_context(context), m_swapchain(swapchain) {
    // A dark, slightly blue grey so "we cleared the screen" is unmistakably
    // different from black (an uncleared / broken frame). Values are linear;
    // the sRGB swapchain encodes them on write. See Glossary: ATTACHMENT
    m_clearValues[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};
    // Depth clears to 1.0 — the far plane — because depth testing keeps the
    // *smallest* depth (nearest fragment), so everything starts "infinitely
    // far". See Glossary: DEPTH_TESTING
    m_clearValues[1].depthStencil = {1.0f, 0};

    createRenderPass();
    createDepthResources();
    createFramebuffers();
}

RenderPass::~RenderPass() {
    destroyTargets();
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_context.device(), m_renderPass, nullptr);
    }
}

void RenderPass::recreate() {
    // The render pass is format-only and survives a resize; just the size-
    // dependent targets are rebuilt against the new swapchain extent/images.
    m_context.waitIdle();
    destroyTargets();
    createDepthResources();
    createFramebuffers();
}

void RenderPass::createRenderPass() {
    // Each attachment is declared up front: its format, sample count, and what
    // happens to it at the start (loadOp) and end (storeOp) of the pass. Vulkan
    // wants this whole structure known in advance so the driver can schedule and
    // optimise the pass. See Glossary: RENDER_PASS, ATTACHMENT

    // Colour attachment = the swapchain image we present.
    VkAttachmentDescription colour{};
    colour.format = m_swapchain.imageFormat();
    colour.samples = VK_SAMPLE_COUNT_1_BIT;            // no multisampling
    colour.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;        // clear to m_clearValues[0] at start
    colour.storeOp = VK_ATTACHMENT_STORE_OP_STORE;      // keep the result so it can be shown
    colour.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colour.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colour.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;   // we don't care about prior contents
    colour.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // leave it ready to present

    // Depth attachment = the depth buffer. Its result is not needed after the
    // pass (we never read it back), so storeOp is DONT_CARE.
    // See Glossary: DEPTH_BUFFER
    VkAttachmentDescription depth{};
    depth.format = m_depthFormat = chooseDepthFormat(m_context.physicalDevice());
    depth.samples = VK_SAMPLE_COUNT_1_BIT;
    depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // References tie an attachment index to the layout it should be in *during*
    // the subpass. The driver transitions images to these layouts for us.
    VkAttachmentReference colourRef{};
    colourRef.attachment = 0;  // index into the pAttachments array below
    colourRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // A subpass is one phase of rendering that reads/writes a subset of the
    // attachments. This project has exactly one. See Glossary: SUBPASS
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colourRef;
    subpass.pDepthStencilAttachment = &depthRef;

    // A dependency makes the layout transition into the subpass happen at the
    // right moment: after the image is available and before we write colour or
    // depth. It is the bridge between work outside the render pass (EXTERNAL) and
    // our subpass — effectively an implicit pipeline barrier that also performs
    // the attachments' image-layout transitions (UNDEFINED → attachment-optimal,
    // and colour → PRESENT_SRC at the end), so this project needs no explicit
    // vkCmdPipelineBarrier. See Glossary: SUBPASS, PIPELINE_BARRIER,
    // IMAGE_LAYOUT_TRANSITION
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    const VkAttachmentDescription attachments[] = {colour, depth};

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 2;
    info.pAttachments = attachments;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(m_context.device(), &info, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateRenderPass failed");
    }
    std::cout << "[vulkan] render pass created (colour + depth attachments)\n";
}

void RenderPass::createDepthResources() {
    const VkExtent2D extent = m_swapchain.extent();

    // The depth buffer is an off-screen image the GPU writes a per-pixel depth
    // into; it is never presented. See Glossary: DEPTH_BUFFER
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;          // GPU-friendly layout
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(m_context.device(), &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImage (depth) failed");
    }

    // A VkImage has no memory of its own; we query what it needs, allocate
    // device-local memory (fast GPU memory), and bind it. (Memory types are
    // covered fully in Chunk 7.)
    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(m_context.device(), m_depthImage, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = findMemoryType(m_context.physicalDevice(), memReq.memoryTypeBits,
                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_context.device(), &alloc, nullptr, &m_depthMemory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory (depth) failed");
    }
    vkBindImageMemory(m_context.device(), m_depthImage, m_depthMemory, 0);

    // View exposing the depth aspect of the image so it can be attached.
    // See Glossary: IMAGE_VIEW
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT |
        (hasStencil(m_depthFormat) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_context.device(), &viewInfo, nullptr, &m_depthView) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImageView (depth) failed");
    }
}

void RenderPass::createFramebuffers() {
    // A framebuffer binds the render pass's attachment slots to concrete image
    // views: slot 0 = this swapchain image's colour view, slot 1 = the shared
    // depth view. One framebuffer per swapchain image. See Glossary: FRAMEBUFFER
    const auto& colourViews = m_swapchain.imageViews();
    const VkExtent2D extent = m_swapchain.extent();
    m_framebuffers.resize(colourViews.size());

    for (size_t i = 0; i < colourViews.size(); ++i) {
        const VkImageView attachments[] = {colourViews[i], m_depthView};

        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = m_renderPass;             // must be compatible with this render pass
        info.attachmentCount = 2;
        info.pAttachments = attachments;
        info.width = extent.width;
        info.height = extent.height;
        info.layers = 1;
        if (vkCreateFramebuffer(m_context.device(), &info, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("vkCreateFramebuffer failed");
        }
    }
    std::cout << "[vulkan] " << m_framebuffers.size() << " framebuffers created ("
              << extent.width << "x" << extent.height << "), depth buffer ready\n";
}

void RenderPass::destroyTargets() {
    for (VkFramebuffer fb : m_framebuffers) {
        vkDestroyFramebuffer(m_context.device(), fb, nullptr);
    }
    m_framebuffers.clear();

    if (m_depthView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_context.device(), m_depthView, nullptr);
        m_depthView = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vkDestroyImage(m_context.device(), m_depthImage, nullptr);
        m_depthImage = VK_NULL_HANDLE;
    }
    if (m_depthMemory != VK_NULL_HANDLE) {
        vkFreeMemory(m_context.device(), m_depthMemory, nullptr);
        m_depthMemory = VK_NULL_HANDLE;
    }
}
