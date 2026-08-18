#include "vulkan-context.hpp"

#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace Rendering::VulkanBackend {

namespace {

const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

const std::vector<const char*> kRequiredDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData
)
{
  const char* level =
      (severity * VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ? "ERROR" : "WARNING";

  std::cerr << "[validation: " << level << "] " << data->pMessage << '\n';

  return VK_FALSE;
}

VkResult CreateDebugUtilsMessenger(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger
)
{
  auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
  );
  if (fn == nullptr) return VK_ERROR_EXTENSION_NOT_PRESENT;
  return fn(instance, createInfo, nullptr, messenger);
}

void DestroyDebugUtilsMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
  auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
  );
  if (fn != nullptr) fn(instance, messenger, nullptr);
}

void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& info)
{
  info = {};

  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

  info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  info.pfnUserCallback = DebugCallback;
}

bool ValidationLayerAvailable()
{
  uint32_t count = 0;
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> layers(count);
  vkEnumerateInstanceLayerProperties(&count, layers.data());
  for (const auto& layer : layers) {
    if (std::strcmp(layer.layerName, kValidationLayerName) == 0) return true;
  }
  return false;
}

VkApplicationInfo CreateVulkanApplicationInfo(const ApplicationInfo& applicationInfo)
{
  VkApplicationInfo vkAppInfo{};

  vkAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  vkAppInfo.pApplicationName = applicationInfo.applicationName.value_or("3D Renderer");
  vkAppInfo.applicationVersion =
      applicationInfo.applicationVersion.value_or(VK_MAKE_VERSION(0, 1, 0));
  vkAppInfo.pEngineName = applicationInfo.engineName.value_or("No Engine");
  vkAppInfo.engineVersion = applicationInfo.engineVersion.value_or(VK_MAKE_VERSION(0, 1, 0));
  vkAppInfo.apiVersion = applicationInfo.apiVersion.value_or(VK_API_VERSION_1_3);
  return vkAppInfo;
}

}  // namespace

VulkanContext::VulkanContext(
    SDL_Window* window, const ApplicationInfo& appInfo, bool enableValidation
)
    : enableValidation(enableValidation)
{
  CreateInstance(window, appInfo);
  SetupDebugMessenger();
  CreateSurface(window);
  PickPhysicalDevice();
  CreateLogicalDevice();
}

VulkanContext::~VulkanContext()
{
  // Destroy in reverse order of creation

  if (this->logicalDevice != VK_NULL_HANDLE) {
    vkDestroyDevice(this->logicalDevice, nullptr);
  }

  if (this->surface != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
  }

  if (this->debugMessenger != VK_NULL_HANDLE) {
    DestroyDebugUtilsMessenger(this->instance, this->debugMessenger);
  }

  if (this->instance != VK_NULL_HANDLE) {
    vkDestroyInstance(this->instance, nullptr);
  }
}

void VulkanContext::CreateInstance(SDL_Window* window, const ApplicationInfo& applicationInfo)
{
  if (this->enableValidation && !ValidationLayerAvailable()) {
    std::cerr << "[vulkan] validation layers requested but not avilable; continuing without\n";
    this->enableValidation = false;
  }

  VkApplicationInfo vkAppInfo = CreateVulkanApplicationInfo(applicationInfo);

  // Instance Level Extensions
  uint32_t sdlExtCount = 0;
  const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);

  if (sdlExtensions == nullptr) {
    throw std::runtime_error(
        std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError()
    );
  }

  std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtCount);

  if (this->enableValidation) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  VkInstanceCreateFlags flags = 0;

#if __APPLE__
  // MoltenVK Extensions for macOS

  extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
  flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

  // Create Instance Info

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.flags = flags;
  createInfo.pApplicationInfo = &vkAppInfo;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.data();

  // Create Debug Messenger Info
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
  if (this->enableValidation) {
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = &kValidationLayerName;
    PopulateDebugMessengerCreateInfo(debugCreateInfo);
    createInfo.pNext = &debugCreateInfo;
  }

  // Create Instance
  if (vkCreateInstance(&createInfo, nullptr, &this->instance) != VK_SUCCESS) {
    throw std::runtime_error("vkCreateInstance failed");
  }
  std::cout << "[vulkan] instance created (validation: " << (this->enableValidation ? "on" : "off")
            << ")\n";
}

void VulkanContext::SetupDebugMessenger()
{
  if (!this->enableValidation) return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  PopulateDebugMessengerCreateInfo(createInfo);
  if (CreateDebugUtilsMessenger(this->instance, &createInfo, &this->debugMessenger) != VK_SUCCESS) {
    throw std::runtime_error("failed to setup debug messenger");
  }
}

void VulkanContext::CreateSurface(SDL_Window* window)
{
  if (!SDL_Vulkan_CreateSurface(window, this->instance, nullptr, &this->surface)) {
    throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
  }
}

static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
  QueueFamilyIndices indices;

  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

  for (uint32_t i = 0; i < count; ++i) {
    if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics = i;
    }

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
    if (presentSupport) {
      indices.present = i;
    }
    if (indices.IsComplete()) break;
  }
  return indices;
}

static bool DeviceSupportsRequiredExtensions(VkPhysicalDevice device)
{
  uint32_t count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> available(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

  std::set<std::string> required(
      kRequiredDeviceExtensions.begin(), kRequiredDeviceExtensions.end()
  );
  for (const auto& ext : available) {
    required.erase(ext.extensionName);
  }
  return required.empty();
}

static bool DeviceHasExtension(VkPhysicalDevice device, const char* name)
{
  uint32_t count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> available(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
  for (const auto& extension : available) {
    if (std::strcmp(extension.extensionName, name) == 0) return true;
  }
  return false;
}

void VulkanContext::PickPhysicalDevice()
{
  uint32_t count = 0;
  vkEnumeratePhysicalDevices(this->instance, &count, nullptr);
  if (count == 0) {
    throw std::runtime_error("no Vulkan-capable GPU found");
  }
  std::vector<VkPhysicalDevice> devices(count);
  vkEnumeratePhysicalDevices(this->instance, &count, devices.data());

  // SIDE QUEST: Implement device scoring to prefer a descrete GPU.
  for (VkPhysicalDevice device : devices) {
    QueueFamilyIndices indices = FindQueueFamilies(device, this->surface);

    if (indices.IsComplete() && DeviceSupportsRequiredExtensions(device)) {
      this->physicalDevice = device;
      this->queueFamilies = indices;
      break;
    }
  }
  if (this->physicalDevice == VK_NULL_HANDLE) {
    throw std::runtime_error(
        "no suitable GPU (needs graphics + present queues and swapchain support)"
    );
  }

  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(this->physicalDevice, &props);
  std::cout << "[vulkan] using GPU: " << props.deviceName << "\n";
  std::cout << "[vulkan] graphics queue family " << *this->queueFamilies.graphics
            << ", present queue family " << *this->queueFamilies.present << '\n';
}

void VulkanContext::CreateLogicalDevice()
{
  std::set<uint32_t> uniqueFamilies = {
      *this->queueFamilies.graphics,
      *this->queueFamilies.present,
  };

  // NOTE: Relative scheduling priority
  // if more queues are added, this needs adjusting
  float queuePriority = 1.0f;

  std::vector<VkDeviceQueueCreateInfo> queueInfos;
  for (uint32_t family : uniqueFamilies) {
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = family;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;
    queueInfos.push_back(queueInfo);
  }

  VkPhysicalDeviceFeatures features{};

  std::vector<const char*> deviceExtensions = kRequiredDeviceExtensions;
  if (DeviceHasExtension(this->physicalDevice, "VK_KHR_portability_subset")) {
    deviceExtensions.push_back("VK_KHR_portability_subset");
  }

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
  createInfo.pQueueCreateInfos = queueInfos.data();
  createInfo.pEnabledFeatures = &features;
  createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  if (vkCreateDevice(this->physicalDevice, &createInfo, nullptr, &this->logicalDevice) !=
      VK_SUCCESS) {
    throw std::runtime_error("vkCreateDevice failed");
  }

  vkGetDeviceQueue(this->logicalDevice, *this->queueFamilies.graphics, 0, &this->graphicsQueue);
  vkGetDeviceQueue(this->logicalDevice, *this->queueFamilies.present, 0, &this->presentQueue);

  std::cout << "[vulkan] logical device and queues created\n";
}

void VulkanContext::WaitIdle() const
{
  vkDeviceWaitIdle(this->logicalDevice);
}

}  // namespace Rendering::VulkanBackend
