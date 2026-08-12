#pragma once

#include "SDL3/SDL_video.h"
#include <SDL3/SDL.h>
#include <string>

namespace VulkanOrbitViewer {

class SdlContext {
public:
  SdlContext(const std::string &title, int width, int height);

  ~SdlContext();

  SdlContext(const SdlContext &) = delete;
  SdlContext &operator=(const SdlContext &) = delete;

  bool processEvents();

  SDL_Window *GetWindow() const { return this->window; }

private:
  SDL_Window *window = nullptr;
};
} // namespace VulkanOrbitViewer
