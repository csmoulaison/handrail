#ifndef handrail_core_h_INCLUDED
#define handrail_core_h_INCLUDED

#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <math.h>
#include <inttypes.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

typedef size_t   usize;

#define PLATFORM_WINDOWS 0
#define PLATFORM_LINUX   1
#define PLATFORM_WEB     2

#ifdef _MSC_VER
#define PLATFORM PLATFORM_WINDOWS
#include <malloc.h>
#include <windows.h>
#endif

#ifdef __GNUC__
#define PLATFORM PLATFORM_LINUX
#include <unistd.h>
#include <dlfcn.h>
#include <alloca.h>
#include <dirent.h>
#endif

#ifdef __EMSCRIPTEN__
#define PLATFORM PLATFORM_WEB
#endif

#define KILOBYTE 1000
#define MEGABYTE 1000000
#define GIGABYTE 1000000000

#include "handrail/assert.h"
#include "handrail/string.h"
#include "handrail/log.h"
#include "handrail/buffer.h"
#include "handrail/stack.h"
#include "handrail/random.h"
#include "handrail/file.h"
#include "handrail/math.h"

#ifdef CSM_INCLUDE_GL
#include "handrail/gpu/gl.h"
#endif

#include "handrail/platform.h"
#include "handrail/fiedler.h"
#include "handrail/dynamic_library.h"
#include "handrail/game.h"

#include "handrail/media/asset_builder.h"
#include "handrail/media/mesh.h"
#include "handrail/media/texture.h"
#include "handrail/media/sprite.h"
#include "handrail/media/aseprite.h"
#include "handrail/media/blender.h"
#include "handrail/media/synth.h"
#include "handrail/media/font.h"

#endif
