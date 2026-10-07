#include <SDL.h>

/* A separately compiled C consumer verifies the public SDL2 C surface. */
int symbian_sdl2_c_api_probe(void) {
  SDL_version version;
  SDL_GetVersion(&version);
  return version.major == 2 && version.minor >= 0;
}
