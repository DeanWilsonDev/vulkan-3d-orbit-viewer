#include "sdl-context.hpp"
#include "SDL3/SDL_error.h"
#include "SDL3/SDL_events.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"

#include <stdexcept>
#include <string>

SdlContext::SdlContext(const std::string &title, int width, int height) {

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
  }

  this->window =
      SDL_CreateWindow(title.c_str(), width, height, SDL_WINDOW_RESIZABLE);

  if (this->window == nullptr) {
    SDL_Quit();
    throw std::runtime_error(std::string("SDL_CreateWindow failed: ") +
                             SDL_GetError());
  }
}

SdlContext::~SdlContext() {
  if (this->window != nullptr) {
    SDL_DestroyWindow(this->window);
  }
  SDL_Quit();
}

bool SdlContext::processEvents() {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      return false;
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
      return false;
    default:
      break;
    }
  }

  return true;
}
