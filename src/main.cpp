#include "sdl-context.hpp"
#include "vulkan-context.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <ostream>

using namespace VulkanOrbitViewer;

int main()
{
  try {
    SdlContext sdl("Vulkan Orbit Viewer", 1280, 720);

    bool running = true;
    while (running) {
      running = sdl.processEvents();
    }
  }
  catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
