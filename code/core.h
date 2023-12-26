#ifndef __CORE_H__
#define __CORE_H__

#include <stdint.h>

#include <SDL2/SDL.h>
#include <tiny_imageformat/tinyimageformat.h>
#include <vectormath/vectormath.hpp>
#include <compile_time_crc.h>

// Primitive data types
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
typedef uint8_t     rgByte;

typedef uint32_t    rgHash;

static_assert(sizeof(rgHash) == sizeof(uint32_t), "sizeof(rgU32) != sizeof(uint32_t)");

#define RG_INLINE inline

#define rgKilobyte(x) 1024LL * (x)
#define rgMegabyte(x) 1024LL * rgKilobyte(x)
#define rgGigabyte(x) 1024LL * rgMegabyte(x)

#define rgArrayCount(a) (sizeof(a)/sizeof((a)[0]))
#define rgOffsetOf(type, member) ((uintptr_t)&(((type *)0)->member))
#define rgSizeOfU32(x) ((rgU32)sizeof(x))

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

#define RG_DEFINE_ENUM_FLAGS_OPERATOR(T) \
inline T operator~ (T a) { return static_cast<T>( ~static_cast<std::underlying_type<T>::type>(a) ); } \
inline T operator| (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) | static_cast<std::underlying_type<T>::type>(b) ); } \
inline T operator& (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) & static_cast<std::underlying_type<T>::type>(b) ); } \
inline T operator^ (T a, T b) { return static_cast<T>( static_cast<std::underlying_type<T>::type>(a) ^ static_cast<std::underlying_type<T>::type>(b) ); } \
inline T& operator|= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) |= static_cast<std::underlying_type<T>::type>(b) ); } \
inline T& operator&= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) &= static_cast<std::underlying_type<T>::type>(b) ); } \
inline T& operator^= (T& a, T b) { return reinterpret_cast<T&>( reinterpret_cast<std::underlying_type<T>::type&>(a) ^= static_cast<std::underlying_type<T>::type>(b) ); }


union rgFloat2
{
    rgFloat v[2];
    struct
    {
        rgFloat x;
        rgFloat y;
    };
};

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
    struct
    {
        rgFloat2 xy;
        rgFloat2 zw;
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

struct PhysicSystem;

struct FileData
{
    rgBool  isValid;
    rgU8*   data;
    rgSize  dataSize;
};

FileData readFile(const char* filepath);
void     freeFileData(FileData* fd);
rgBool   writeFile(char const* filepath, void* bufferPtr, rgSize bufferSizeInBytes);

struct WindowInfo
{
    rgUInt width;
    rgUInt height;
};

char* getPrefPath();

extern rgDouble g_DeltaTime;
extern rgDouble g_Time;

extern rgInt g_FrameIndex;

extern WindowInfo g_WindowInfo;
extern PhysicSystem* g_PhysicSystem;

#endif // __CORE_H__
