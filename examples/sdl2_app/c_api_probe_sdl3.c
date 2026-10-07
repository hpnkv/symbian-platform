#include <SDL3/SDL.h>

/* Independently compiled C consumer for the SDL3 SDK API. */
int symbian_sdl2_c_api_probe(void) {
  return SDL_VERSIONNUM_MAJOR(SDL_GetVersion()) == 3;
}
