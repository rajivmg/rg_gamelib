#ifndef __RG_H__
#define __RG_H__

#include <stdint.h>

#include <SDL2/SDL.h>
#include <tiny_imageformat/tinyimageformat.h>
#include <vectormath/vectormath.hpp>
#include <compile_time_crc.h>

#define RG_BEGIN_NAMESPACE namespace rg {
#define RG_END_NAMESPACE }

/*
typedef int64_t     S64;
typedef int32_t     S32;
typedef int16_t     S16;
typedef int8_t      S8;

typedef uint64_t    U64;
typedef uint32_t    U32;
typedef uint16_t    U16;
typedef uint8_t     U8;

typedef float       R32;
typedef double      R64;

typedef intptr_t    IPtr;
typedef uintptr_t   UPtr;

typedef uint32_t    UInt;
typedef int32_t     Int;
typedef float       Float;
typedef double      Double;
typedef bool        Bool;
*/
//--
typedef int64_t     rgS64;
typedef int32_t     rgS32;
typedef int16_t     rgS16;
typedef int8_t      rgS8;

typedef uint64_t    rgU64;
typedef uint32_t    rgU32;
typedef uint16_t    rgU16;
typedef uint8_t     rgU8;

typedef float       rgR32;
typedef double      rgR64;

typedef intptr_t    rgIPtr;
typedef uintptr_t   rgUPtr;

typedef size_t      rgSize;

typedef uint32_t    rgUInt;
typedef int32_t     rgInt;
typedef float       rgFloat;
typedef double      rgDouble;
typedef bool        rgBool;

typedef char        rgChar;

typedef uint32_t    rgCRC32;

//

#define rgKILOBYTE(x) 1024LL * (x)
#define rgMEGABYTE(x) 1024LL * rgKILOBYTE(x)
#define rgGIGABYTE(x) 1024LL * rgMEGABYTE(x)
#define rgARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define rgOFFSET_OF(type, member) ((uintptr_t)&(((type *)0)->member))
#define rgAssert(exp) SDL_assert(exp)
#define rgMalloc(s) malloc((s))
#define rgFree(p) free((p))
#define rgNew(objectType) new objectType
#define rgPlacementNew(objectType, placementAddress) new(placementAddress) objectType
#define rgDelete(object) delete object

void _rgLogImpl(char const* fmt, ...);
#define rgLog(...) _rgLogImpl(__VA_ARGS__)

#define rgLogWarn(...) SDL_LogWarn(SDL_LOG_CATEGORY_TEST, __VA_ARGS__)
#define rgLogError(...) SDL_LogError(SDL_LOG_CATEGORY_TEST, __VA_ARGS__)

union rgFloat4
{
    float v[4];
    struct
    {
        float x;
        float y;
        float z;
        float w;
    };
    struct
    {
        float r;
        float g;
        float b;
        float a;
    };
};

/// ----- Implementation
#ifdef RG_H_IMPLEMENTATION
// TODO: Add filename and line number, try to remove the need of fmt
void _rgLogImpl(char const* fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_TEST, SDL_LOG_PRIORITY_DEBUG, fmt, argList);
    va_end(argList);
}
#endif // RG_H_IMPLEMENTATION
#endif // __RG_H__
