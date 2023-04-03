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

typedef uint32_t    rgHash;

//

#define RG_INLINE inline

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
    rgFloat v[4];
    struct
    {
        rgFloat x;
        rgFloat y;
        rgFloat z;
        rgFloat w;
    };
    struct
    {
        rgFloat r;
        rgFloat g;
        rgFloat b;
        rgFloat a;
    };
};

union rgFloat3
{
    rgFloat v[3];
    struct
    {
        rgFloat x;
        rgFloat y;
        rgFloat z;
    };
    struct
    {
        rgFloat r;
        rgFloat g;
        rgFloat b;
    };
};

RG_INLINE rgFloat3 operator+(const rgFloat3& a, const rgFloat3& b)
{
    return rgFloat3{a.x + b.x, a.y + b.y, a.z + b.z};
}

RG_INLINE rgFloat3 operator-(rgFloat3& a, rgFloat3& b)
{
    return rgFloat3{ a.x - b.x, a.y - b.y, a.z - b.z };
}

RG_INLINE rgFloat3& operator+=(rgFloat3& a, const rgFloat3& b)
{
    a = a + b;
    return a;
}

RG_INLINE rgFloat3 operator*(rgFloat3& a, rgFloat b)
{
    return rgFloat3{a.x * b, a.y * b, a.z * b};
}

#define rgPrint(x) rgPrintImplementation(#x, x)

RG_INLINE void rgPrintImplementation(const char* varName, rgFloat3& a)
{
    rgLog("%s = {%f, %f, %f}\n", varName, a.x, a.y, a.z);
}

extern rgBool g_ShouldQuit;

RG_BEGIN_NAMESPACE
struct PhysicSystem;

struct FileData
{
    rgBool  isValid;
    rgU8*   data;
    rgSize  dataSize;
};

FileData readFile(const char* filepath);
void     freeFileData(FileData* fd);

extern rgDouble g_DeltaTime;
extern rgDouble g_Time;

RG_END_NAMESPACE

extern rg::PhysicSystem* g_PhysicSystem;

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

RG_BEGIN_NAMESPACE

FileData readFile(const char* filepath)
{
    FileData result = {};

    SDL_RWops* fp = SDL_RWFromFile(filepath, "rb");

    if(fp != NULL)
    {
        SDL_RWseek(fp, 0, RW_SEEK_END);
        Sint64 size = (rgSize)SDL_RWtell(fp);

        if(size == -1 || size <= 0)
        {
            SDL_RWclose(fp);
            result.isValid = false;
            return result;
        }
        else
        {
            result.dataSize = (rgSize)size;
        }

        SDL_RWseek(fp, 0, RW_SEEK_SET);

        result.data = (rgU8*)rgMalloc(sizeof(rgU8) * size);
        if(result.data == NULL)
        {
            rgLog("Cannot allocate memory(%dbytes) for reading file %s", size, filepath);
            SDL_RWclose(fp);
            result.isValid = false;
            return result;
        }

        rgSize sizeRead = SDL_RWread(fp, result.data, sizeof(rgU8), size);
        if(sizeRead != size)
        {
            result.isValid = false;
            rgFree(result.data);
        }

        SDL_RWclose(fp);

        result.isValid = true;
    }
    else
    {
        rgLog("Cannot open/read file %s", filepath);
        result.isValid = false;
    }

    return result;
}

void freeFileData(FileData* fd)
{
    rgFree(fd->data);
}

RG_END_NAMESPACE

#endif // RG_H_IMPLEMENTATION
#endif // __RG_H__
