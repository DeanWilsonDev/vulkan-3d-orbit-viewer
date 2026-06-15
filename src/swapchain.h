#pragma once

// Swapchain — owns the chain of images that Vulkan renders into and presents to
// the screen, plus an image view for each one. The swapchain is tightly coupled
// to the surface and to the window's size, so it must be torn down and rebuilt
// whenever the window is resized; this class encapsulates both its creation and
// that recreation. See Glossary: SWAPCHAIN
//
// Nothing is drawn yet in this chunk — the swapchain is the destination the
// renderer will eventually present to, built here so the rest of the pipeline
// has something concrete to target.

#include <vulkan/vulkan.h>   // Vk* swapchain/image/image-view types
#include <SDL3/SDL_video.h>  // SDL_Window — queried for the drawable pixel size

#include <cstdint>
#include <vector>

class VulkanContext;  // forward-declared; we only need a reference to it

class Swapchain {
public:
    // Build the swapchain for the given Vulkan context and window. Throws
    // std::runtime_error on failure.
    Swapchain(VulkanContext& context, SDL_Window* window);

    // Destroy the image views and the swapchain.
    ~Swapchain();

    // Owns unique GPU resources — not copyable.
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Destroy and rebuild the swapchain for the window's current size. Called
    // when the window is resized; the old size's images no longer match the
    // surface, so they must be replaced. Safely does nothing while the window is
    // minimised (zero drawable size). See Glossary: SWAPCHAIN
    void recreate();

    // Accessors used by later chunks (render pass, framebuffers, the frame loop).
    VkSwapchainKHR                   handle() const { return m_swapchain; }
    VkFormat                         imageFormat() const { return m_imageFormat; }
    VkExtent2D                       extent() const { return m_extent; }
    const std::vector<VkImage>&      images() const { return m_images; }
    const std::vector<VkImageView>&  imageViews() const { return m_imageViews; }
    uint32_t                         imageCount() const { return static_cast<uint32_t>(m_images.size()); }

private:
    void create();    // build the swapchain + image views for the current size
    void destroy();   // tear them down (used by the destructor and recreate())

    VulkanContext& m_context;          // device, physical device, surface, queues
    SDL_Window*    m_window = nullptr;  // source of the drawable pixel size

    VkSwapchainKHR           m_swapchain = VK_NULL_HANDLE;  // the swapchain object
    std::vector<VkImage>     m_images;       // images owned BY the swapchain (not destroyed individually)
    std::vector<VkImageView> m_imageViews;   // one view per image (we own and destroy these)
    VkFormat                 m_imageFormat{}; // colour format the images were created with
    VkExtent2D               m_extent{};      // pixel size the images were created at
};
