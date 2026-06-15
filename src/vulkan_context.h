#pragma once

// VulkanContext — owns the foundational Vulkan objects that the entire renderer
// is built on top of: the instance, the debug messenger, the window surface,
// the chosen physical device (GPU), and the logical device with its queues.
// These objects are created once at startup and live for the whole program, so
// one class owns them and tears them down in the correct reverse order.
//
// Vulkan is explicit by design: nothing is created for you, and every object you
// create you must describe in full and later destroy yourself. This file is
// where that begins. See Glossary: VULKAN_INSTANCE, LOGICAL_DEVICE

#include <vulkan/vulkan.h>   // Core Vulkan API: all Vk* types and functions
#include <SDL3/SDL_video.h>  // SDL_Window — the window we create a surface for

#include <cstdint>
#include <optional>

// The indices of the queue families this program needs. A device exposes its
// queues grouped into families by capability; we must locate a family that can
// do graphics work and one that can present to our surface. They are frequently
// the same family, but Vulkan does not guarantee it. std::optional makes
// "not found yet" a distinct, checkable state. See Glossary: QUEUE_FAMILY
struct QueueFamilyIndices {
    std::optional<uint32_t> graphics;  // family that supports VK_QUEUE_GRAPHICS_BIT
    std::optional<uint32_t> present;   // family that can present to the surface

    // True once both required families have been found.
    bool isComplete() const { return graphics.has_value() && present.has_value(); }
};

class VulkanContext {
public:
    // Build the whole Vulkan foundation for the given window. When
    // enableValidation is true (debug builds), validation layers and a debug
    // messenger are installed so mistakes are reported in the terminal.
    // Throws std::runtime_error on any failure. See Glossary: VALIDATION_LAYERS
    VulkanContext(SDL_Window* window, bool enableValidation);

    // Destroy every Vulkan object in reverse order of creation.
    ~VulkanContext();

    // Owns unique GPU resources, so it cannot be copied.
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    // Accessors used by later chunks (swapchain, pipeline, command buffers).
    VkInstance       instance() const { return m_instance; }
    VkSurfaceKHR     surface() const { return m_surface; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkDevice         device() const { return m_device; }
    VkQueue          graphicsQueue() const { return m_graphicsQueue; }
    VkQueue          presentQueue() const { return m_presentQueue; }
    const QueueFamilyIndices& queueFamilies() const { return m_queueFamilies; }

    // Block until the GPU has finished all outstanding work on this device. Used
    // before destroying resources the GPU might still be using — e.g. the old
    // swapchain when the window is resized, and during teardown. This is a heavy
    // stall, so it is only ever used on rare, non-per-frame events; Chunk 6
    // introduces the fine-grained synchronisation used per frame instead.
    void waitIdle() const { vkDeviceWaitIdle(m_device); }

    // --- Memory + buffer helpers (used from Chunk 7) -------------------------

    // Find a memory type index that is allowed by typeFilter (a bitmask from a
    // resource's memory requirements) and has all the requested properties (e.g.
    // device-local, or host-visible). See Glossary: MEMORY_TYPES
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    // Create a VkBuffer and back it with memory of the requested properties.
    // See Glossary: VERTEX_BUFFER, GPU_MEMORY
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory) const;

    // Copy `size` bytes from one buffer to another using a one-time command on
    // the graphics queue. This is how data reaches device-local memory the CPU
    // cannot write to directly. See Glossary: STAGING_BUFFER
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;

private:
    // Each of these is one step of construction, kept as a named function so the
    // constructor reads like a checklist of what Vulkan startup requires.
    void createInstance(SDL_Window* window);
    void setupDebugMessenger();
    void createSurface(SDL_Window* window);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createUploadPool();   // small command pool for one-time transfer commands

    bool m_enableValidation = false;

    // Created in dependency order; destroyed in the reverse.
    VkInstance               m_instance = VK_NULL_HANDLE;        // connection to the Vulkan library
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;  // routes validation messages to our callback
    VkSurfaceKHR             m_surface = VK_NULL_HANDLE;         // bridge between Vulkan and the OS window
    VkPhysicalDevice         m_physicalDevice = VK_NULL_HANDLE;  // the GPU we chose (not owned — owned by the instance)
    VkDevice                 m_device = VK_NULL_HANDLE;          // our logical handle to that GPU
    VkQueue                  m_graphicsQueue = VK_NULL_HANDLE;   // retrieved from the device (not separately destroyed)
    VkQueue                  m_presentQueue = VK_NULL_HANDLE;

    // Command pool dedicated to short-lived, one-time transfer commands (the
    // staging copies in Chunk 7). Separate from the renderer's per-frame pool.
    // See Glossary: COMMAND_POOL
    VkCommandPool m_uploadPool = VK_NULL_HANDLE;

    QueueFamilyIndices m_queueFamilies;
};
