#include "vulkan_context.h"

#include <SDL3/SDL_vulkan.h>   // SDL_Vulkan_GetInstanceExtensions, SDL_Vulkan_CreateSurface

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// The one validation layer we care about. It is a meta-layer bundling all of
// Khronos' validation checks: it intercepts every Vulkan call, verifies the
// parameters and object usage against the spec, and reports anything wrong via
// the debug messenger. You always develop with it on and ship with it off.
// See Glossary: VALIDATION_LAYERS
const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

// Device-level extensions the program requires. VK_KHR_swapchain is what lets a
// device present rendered images to a surface. We do not build the swapchain
// until Chunk 3, but the logical device is created once, here, so we require and
// enable the extension now. See Glossary: VULKAN_EXTENSIONS
const std::vector<const char*> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

// The debug messenger invokes this for every message the validation layers
// produce. We print warnings and errors to stderr so they are impossible to
// miss. Returning VK_FALSE means "do not abort the Vulkan call that triggered
// this message" — the callback only reports, it does not interfere.
// See Glossary: VALIDATION_LAYERS
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void* /*userData*/) {
    const char* level =
        (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" : "WARNING";
    std::cerr << "[validation:" << level << "] " << data->pMessage << '\n';
    return VK_FALSE;
}

// vkCreateDebugUtilsMessengerEXT belongs to an extension, so it is not exported
// by the loader directly — its address must be looked up at runtime. These two
// wrappers do that lookup and forward the call. See Glossary: VULKAN_EXTENSIONS
VkResult createDebugUtilsMessenger(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger) {
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (fn == nullptr) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return fn(instance, createInfo, nullptr, messenger);
}

void destroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
    auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (fn != nullptr) fn(instance, messenger, nullptr);
}

// Fill in a debug messenger description. Shared between two uses: a temporary
// one attached to instance creation (so the creation/destruction of the
// instance itself is validated) and the persistent messenger set up afterward.
void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info) {
    info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // Which severities we want to hear about: warnings and errors. (Verbose and
    // info are available too but would flood the terminal.)
    info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // Which categories of message: general, spec-validation, and performance.
    info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
}

// True if the requested validation layer is present in this Vulkan runtime.
bool validationLayerAvailable() {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& layer : layers) {
        if (std::strcmp(layer.layerName, kValidationLayerName) == 0) return true;
    }
    return false;
}

} // namespace

VulkanContext::VulkanContext(SDL_Window* window, bool enableValidation)
    : m_enableValidation(enableValidation) {
    // Startup reads as a checklist. Each step depends on the previous one, which
    // is exactly why teardown must run in reverse. See Glossary: VULKAN_INSTANCE
    createInstance(window);
    setupDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    createUploadPool();
}

VulkanContext::~VulkanContext() {
    // Destroy in the exact reverse of creation: children before parents. The
    // device is destroyed before the instance; the surface (owned by the
    // instance) before the instance; the debug messenger last among
    // instance-children. The physical device and queues are not destroyed —
    // they are owned by the instance and device respectively. Across the whole
    // program this same discipline is enforced by the declaration order of the
    // RAII objects in main(): they are destroyed in reverse, so every Vulkan
    // object outlives its children. See Glossary: LOGICAL_DEVICE,
    // VULKAN_TEARDOWN_ORDER, RESOURCE_LIFETIME
    if (m_uploadPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_uploadPool, nullptr);
    }
    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
    }
    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }
    if (m_debugMessenger != VK_NULL_HANDLE) {
        destroyDebugUtilsMessenger(m_instance, m_debugMessenger);
    }
    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
    }
}

void VulkanContext::createInstance(SDL_Window* /*window*/) {
    // If validation was requested but the layer is not installed, degrade
    // gracefully rather than failing: print a notice and carry on without it.
    if (m_enableValidation && !validationLayerAvailable()) {
        std::cerr << "[vulkan] validation layers requested but not available; continuing without\n";
        m_enableValidation = false;
    }

    // Application metadata. Drivers can use this to apply game-specific
    // workarounds; apiVersion is the highest Vulkan version we promise to use.
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Orbit Viewer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Instance-level extensions. The instance is Vulkan's connection to the host
    // and to the windowing system, so the extensions it needs are mostly about
    // presenting to a window. SDL tells us exactly which surface extensions this
    // platform requires. See Glossary: VULKAN_EXTENSIONS, SURFACE
    uint32_t sdlExtCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (sdlExtensions == nullptr) {
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    }
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtCount);

    // VK_EXT_debug_utils is what lets us install the debug messenger, so it is
    // only needed when validation is on.
    if (m_enableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateFlags flags = 0;
#ifdef __APPLE__
    // On macOS, Vulkan is provided by MoltenVK, which is a "portability" driver
    // (it translates Vulkan to Metal and is not 100% conformant). Such drivers
    // are hidden unless we opt in: we enable the portability-enumeration
    // extension and set the matching flag so MoltenVK's device is enumerated.
    // VK_KHR_get_physical_device_properties2 is a prerequisite of the
    // portability-subset device extension we enable later.
    // See Glossary: PHYSICAL_DEVICE
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = flags;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Chaining a debug messenger create-info onto pNext makes validation cover
    // vkCreateInstance and vkDestroyInstance themselves — the only two calls
    // that happen outside the lifetime of the persistent messenger.
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_enableValidation) {
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &kValidationLayerName;
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    // Actually create the instance — Vulkan's root object. Everything else is
    // created from it or from a device created from it. See Glossary: VULKAN_INSTANCE
    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateInstance failed");
    }
    std::cout << "[vulkan] instance created (validation: "
              << (m_enableValidation ? "on" : "off") << ")\n";
}

void VulkanContext::setupDebugMessenger() {
    // Without validation there is nothing to route, so there is no messenger.
    if (!m_enableValidation) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    if (createDebugUtilsMessenger(m_instance, &createInfo, &m_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger");
    }
}

void VulkanContext::createSurface(SDL_Window* window) {
    // The surface is the platform-agnostic handle Vulkan uses to talk to a
    // specific OS window. SDL knows the per-platform details (which Vulkan WSI
    // extension, how to wrap the native window) and creates it for us, so this
    // single call works identically on Linux, macOS, and Windows.
    // See Glossary: SURFACE
    if (!SDL_Vulkan_CreateSurface(window, m_instance, nullptr, &m_surface)) {
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
    }
}

// Locate the queue families this physical device offers that can do graphics
// and that can present to our surface. See Glossary: QUEUE_FAMILY
static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        // A family advertising the graphics bit can record draw/compute work.
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }
        // Presentation support is a property of (family, surface), not of the
        // family alone — it depends on the window system — so it is queried
        // separately. See Glossary: SURFACE
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.present = i;
        }
        if (indices.isComplete()) break;
    }
    return indices;
}

// True if the device exposes every extension in kRequiredDeviceExtensions.
static bool deviceSupportsRequiredExtensions(VkPhysicalDevice device) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    std::set<std::string> required(kRequiredDeviceExtensions.begin(),
                                   kRequiredDeviceExtensions.end());
    for (const auto& ext : available) {
        required.erase(ext.extensionName);
    }
    return required.empty();
}

// True if a device advertises the named extension (used for portability_subset,
// whose name is not a guaranteed-defined macro without beta headers).
static bool deviceHasExtension(VkPhysicalDevice device, const char* name) {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
    for (const auto& ext : available) {
        if (std::strcmp(ext.extensionName, name) == 0) return true;
    }
    return false;
}

void VulkanContext::pickPhysicalDevice() {
    // A physical device is a GPU (or software renderer) the instance can see. We
    // do not create it — we choose one and obtain a handle. See Glossary: PHYSICAL_DEVICE
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (count == 0) {
        throw std::runtime_error("no Vulkan-capable GPU found");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

    // Pick the first device that satisfies our needs: required queue families
    // and required device extensions. A production engine would score devices
    // (preferring a discrete GPU); for a single-mesh viewer, first-fit is fine.
    for (VkPhysicalDevice device : devices) {
        QueueFamilyIndices indices = findQueueFamilies(device, m_surface);
        if (indices.isComplete() && deviceSupportsRequiredExtensions(device)) {
            m_physicalDevice = device;
            m_queueFamilies = indices;
            break;
        }
    }
    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("no suitable GPU (needs graphics + present queues and swapchain support)");
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
    std::cout << "[vulkan] using GPU: " << props.deviceName << '\n';
    std::cout << "[vulkan] graphics queue family " << *m_queueFamilies.graphics
              << ", present queue family " << *m_queueFamilies.present << '\n';
}

void VulkanContext::createLogicalDevice() {
    // The logical device is our own handle to the chosen GPU: the object through
    // which we allocate memory, create pipelines, and submit work. Two programs
    // can each hold their own logical device for the same physical device.
    // See Glossary: LOGICAL_DEVICE

    // One VkDeviceQueueCreateInfo per unique family we need. Using a set
    // collapses the common case where graphics and present are the same family
    // into a single request (Vulkan rejects duplicate family indices).
    std::set<uint32_t> uniqueFamilies = {
        *m_queueFamilies.graphics,
        *m_queueFamilies.present,
    };

    float queuePriority = 1.0f;  // relative scheduling priority; one queue, so it is moot
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount = 1;
        qi.pQueuePriorities = &queuePriority;
        queueInfos.push_back(qi);
    }

    // We request no optional GPU features yet; this is filled in later chunks.
    VkPhysicalDeviceFeatures features{};

    // The device extensions to enable: the required set, plus the macOS
    // portability-subset extension when present. On portability drivers the
    // spec *requires* enabling it if the device exposes it.
    std::vector<const char*> deviceExtensions = kRequiredDeviceExtensions;
    if (deviceHasExtension(m_physicalDevice, "VK_KHR_portability_subset")) {
        deviceExtensions.push_back("VK_KHR_portability_subset");
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.pEnabledFeatures = &features;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    // Note: device-level layers are intentionally left at zero. They were
    // deprecated in Vulkan 1.0 — only instance layers exist now — and current
    // validation layers actively flag a non-zero enabledLayerCount here as an
    // error. The validation layer enabled at instance creation already covers
    // all device calls. See Glossary: VALIDATION_LAYERS

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateDevice failed");
    }

    // Queues are created together with the device; we retrieve handles to them
    // (index 0, since we asked for one queue per family). They are not destroyed
    // separately — they vanish when the device does. See Glossary: QUEUE_FAMILY
    vkGetDeviceQueue(m_device, *m_queueFamilies.graphics, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, *m_queueFamilies.present, 0, &m_presentQueue);

    std::cout << "[vulkan] logical device and queues created\n";
}

void VulkanContext::createUploadPool() {
    // A small command pool for the one-time copies that move data into
    // device-local memory. TRANSIENT hints that its buffers are short-lived.
    // See Glossary: COMMAND_POOL, STAGING_BUFFER
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = *m_queueFamilies.graphics;
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_uploadPool) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateCommandPool (upload) failed");
    }
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    // The GPU exposes a set of memory types, each with a combination of property
    // flags (device-local, host-visible, …). We pick the first that the resource
    // allows (typeFilter) and that has every property we asked for.
    // See Glossary: MEMORY_TYPES, GPU_MEMORY
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        const bool typeAllowed = typeFilter & (1u << i);
        const bool hasProps = (memProps.memoryTypes[i].propertyFlags & properties) == properties;
        if (typeAllowed && hasProps) return i;
    }
    throw std::runtime_error("no suitable memory type found");
}

void VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties,
                                 VkBuffer& buffer, VkDeviceMemory& memory) const {
    // Describe and create the buffer (just a sized, typed region — no memory yet).
    // See Glossary: VERTEX_BUFFER
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;                       // how the buffer will be used (vertex/index/transfer)
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // only the graphics family touches it
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateBuffer failed");
    }

    // Allocate memory matching the buffer's requirements + requested properties,
    // then bind it. See Glossary: GPU_MEMORY, MEMORY_TYPES
    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(m_device, buffer, &memReq);

    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, properties);
    if (vkAllocateMemory(m_device, &alloc, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory (buffer) failed");
    }
    vkBindBufferMemory(m_device, buffer, memory, 0);
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() const {
    // Allocate a throwaway primary command buffer from the upload pool and start
    // recording, flagged as used-once. See Glossary: COMMAND_BUFFER
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_uploadPool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // used once then discarded
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer cmd) const {
    vkEndCommandBuffer(cmd);

    // Submit and block until done — uploads are one-off setup work, not per-frame,
    // so a simple wait is fine. See Glossary: SUBMISSION_QUEUE
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_uploadPool, 1, &cmd);
}

void VulkanContext::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const {
    // Record a single copy command, submit it, and wait. The transfer half of the
    // staging pattern. See Glossary: STAGING_BUFFER, COMMAND_BUFFER
    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, src, dst, 1, &region);
    endSingleTimeCommands(cmd);
}

void VulkanContext::createImage(uint32_t width, uint32_t height, VkFormat format,
                                VkImageUsageFlags usage, VkImage& image, VkDeviceMemory& memory) const {
    // Describe a 2D image: its size, format, tiling, and intended usage. OPTIMAL
    // tiling lets the driver lay texels out for fast sampling (the layout is opaque,
    // which is why we upload via a staging buffer + copy rather than writing direct).
    // See Glossary: TEXTURE, IMAGE_LAYOUT_TRANSITION
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // contents are undefined until uploaded
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("vkCreateImage failed");
    }

    // Allocate device-local memory matching the image and bind it.
    // See Glossary: GPU_MEMORY, DEVICE_LOCAL_MEMORY
    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(m_device, image, &memReq);
    VkMemoryAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize = memReq.size;
    alloc.memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(m_device, &alloc, nullptr, &memory) != VK_SUCCESS) {
        throw std::runtime_error("vkAllocateMemory (image) failed");
    }
    vkBindImageMemory(m_device, image, memory, 0);
}

void VulkanContext::transitionImageLayout(VkImage image, VkImageLayout oldLayout,
                                          VkImageLayout newLayout) const {
    // An image memory barrier both moves the image to a new layout and orders memory
    // access around it. The source/destination pipeline stages + access masks tell
    // the GPU which work must finish before, and wait until after, the transition.
    // See Glossary: IMAGE_LAYOUT_TRANSITION, PIPELINE_BARRIER
    VkCommandBuffer cmd = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = 0;
    VkPipelineStageFlags dstStage = 0;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // Before the copy: nothing to wait on, make the image writable by transfer.
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // After the copy: the transfer write must finish before the fragment shader
        // reads the texture.
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("unsupported image layout transition");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    endSingleTimeCommands(cmd);
}

void VulkanContext::copyBufferToImage(VkBuffer buffer, VkImage image,
                                      uint32_t width, uint32_t height) const {
    // Copy the whole staging buffer into the image's single mip level. bufferRowLength
    // = 0 means the rows are tightly packed (no padding). See Glossary: STAGING_BUFFER
    VkCommandBuffer cmd = beginSingleTimeCommands();
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    endSingleTimeCommands(cmd);
}
