// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_PORTABLE_GLES2_LEGACY_COMPAT_H_
#define SYMBIAN_PORTABLE_GLES2_LEGACY_COMPAT_H_

#include <GLES2/gl2.h>

// Belle's GLES2 header predates these declarations used by SDL renderers.
typedef char GLchar;
#ifndef GL_MIN_EXT
#define GL_MIN_EXT 0x8007
#endif
#ifndef GL_MAX_EXT
#define GL_MAX_EXT 0x8008
#endif

#endif  // SYMBIAN_PORTABLE_GLES2_LEGACY_COMPAT_H_
