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

// Generic min and max for any mix of number types. windows.h defines the same
// macros, so these only fill in where it doesn't. Arguments may be evaluated twice.
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define KILOBYTE 1000
#define MEGABYTE 1000000
#define GIGABYTE 1000000000

// Core types used by pointer across headers, declared up front so headers
// don't depend on each other's include order for them.
typedef struct Buffer Buffer;
typedef struct Stack  Stack;

// Headers are included twice. The first pass declares types, prototypes, and
// macros. The second pass compiles implementations, so every implementation
// can use anything from any header. Include order only matters for types
// that contain other types by value.
#include "handrail/assert.h"
#include "handrail/string.h"
#include "handrail/buffer.h"
#include "handrail/stack.h"
#include "handrail/random.h"
#include "handrail/file.h"
#include "handrail/log.h"
#include "handrail/profile.h"
#include "handrail/math.h"

#ifdef HANDRAIL_INCLUDE_GL
#include "handrail/gpu/gl.h"
#endif

#include "handrail/coff.h"
#include "handrail/platform.h"
#include "handrail/fiedler.h"
#include "handrail/dynamic_library.h"

#include "handrail/media/asset_builder.h"
#include "handrail/media/mesh.h"
#include "handrail/media/texture.h"
#include "handrail/media/sprite.h"
#include "handrail/media/aseprite.h"
#include "handrail/media/blender.h"
#include "handrail/media/synth.h"
#include "handrail/media/font.h"
#include "handrail/media/shader.h"
#include "handrail/media/pcm.h"
#include "handrail/ui.h"
#include "handrail/debug_view.h"

#include "handrail/render.h"
#include "handrail/game.h"

#ifdef HANDRAIL_INCLUDE_VK
#include "handrail/gpu/vk.h"
#endif

#ifdef HANDRAIL_IMPLEMENTATION
#define HANDRAIL_IMPLEMENTATION_PASS
#include "handrail/assert.h"
#include "handrail/string.h"
#include "handrail/buffer.h"
#include "handrail/stack.h"
#include "handrail/random.h"
#include "handrail/file.h"
#include "handrail/log.h"
#include "handrail/profile.h"
#include "handrail/math.h"

#ifdef HANDRAIL_INCLUDE_GL
#include "handrail/gpu/gl.h"
#endif

#include "handrail/coff.h"
#include "handrail/platform.h"
#include "handrail/fiedler.h"
#include "handrail/dynamic_library.h"

#include "handrail/media/asset_builder.h"
#include "handrail/media/mesh.h"
#include "handrail/media/texture.h"
#include "handrail/media/sprite.h"
#include "handrail/media/aseprite.h"
#include "handrail/media/blender.h"
#include "handrail/media/synth.h"
#include "handrail/media/font.h"
#include "handrail/media/shader.h"
#include "handrail/media/pcm.h"
#include "handrail/ui.h"
#include "handrail/debug_view.h"

#include "handrail/render.h"
#include "handrail/game.h"

#ifdef HANDRAIL_INCLUDE_VK
#include "handrail/gpu/vk.h"
#endif
#endif

#endif
