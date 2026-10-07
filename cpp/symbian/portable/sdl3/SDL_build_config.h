/* SDL3 3.2.22 guest configuration. Upstream SDL license applies. */
#ifndef SDL_build_config_h_
#define SDL_build_config_h_

#include <SDL3/SDL_platform_defines.h>

/* Static guest archive: private platform disables SDL3 dynapi. */
#define SDL_PLATFORM_PRIVATE 1
#define SDL_PLATFORM_PRIVATE_NAME "Symbian"

#define HAVE_STDARG_H 1
#define HAVE_STDDEF_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_STDIO_H 1
#define HAVE_MATH_H 1
#define HAVE_LIBC 1
#define HAVE_MALLOC 1
#define HAVE_CALLOC 1
#define HAVE_REALLOC 1
#define HAVE_FREE 1
#define HAVE_MEMSET 1
#define HAVE_MEMCPY 1
#define HAVE_MEMMOVE 1
#define HAVE_MEMCMP 1
#define HAVE_STRLEN 1
#define HAVE_STRCMP 1
#define HAVE_STRNCMP 1
#define HAVE_STRCHR 1
#define HAVE_STRRCHR 1
#define HAVE_STRSTR 1
#define HAVE_STRTOL 1
#define HAVE_STRTOUL 1
#define HAVE_STRTOD 1
#define HAVE_ATOI 1
#define HAVE_ATOF 1
#define HAVE_ABS 1
#define HAVE_QSORT 1
#define HAVE_BSEARCH 1
#define HAVE_VSNPRINTF 1
#define HAVE_GCC_SYNC_LOCK_TEST_AND_SET 1

#define SDL_AUDIO_DRIVER_DUMMY 1
#define SDL_VIDEO_DRIVER_SYMBIAN 1
#define SDL_VIDEO_RENDER_SW 1
#define SDL_JOYSTICK_DISABLED 1
#define SDL_HAPTIC_DISABLED 1
#define SDL_HIDAPI_DISABLED 1
#define SDL_SENSOR_DISABLED 1
#define SDL_CAMERA_DRIVER_DUMMY 1
#define SDL_DIALOG_DUMMY 1
#define SDL_TRAY_DUMMY 1
#define SDL_PROCESS_DUMMY 1
#define SDL_LOADSO_DUMMY 1
#define SDL_FILESYSTEM_DUMMY 1
#define SDL_FSOPS_DUMMY 1
#define SDL_THREADS_DISABLED 1
#define SDL_TIMERS_DISABLED 1

#endif
