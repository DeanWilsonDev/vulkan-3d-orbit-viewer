#include "sdl-context.hpp"
#include "rendering/vulkan-backend/vulkan-context.hpp"
#include "application-info.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <ostream>

using namespace VulkanOrbitViewer;

#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION = false;
#else
constexpr bool ENABLE_VALIDATION = true;
#endif

int main()
{
  try {
    SdlContext sdl("Vulkan Orbit Viewer", 1280, 720);

    ApplicationInfo appInfo{
        .applicationName = "Vulkan Orbit Viewer",
        .engineName = "No Engine",
    };

    Rendering::VulkanBackend::VulkanContext vulkan(sdl.GetWindow(), appInfo, ENABLE_VALIDATION);
    std::cout << "Vulkan initialised successfully";

    bool running = true;
    while (running) {
      running = sdl.ProcessEvents();
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
