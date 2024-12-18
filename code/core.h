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

typedef uint32_t    uint32;
typedef int32_t     int32;

static_assert(sizeof(rgHash) == sizeof(uint32_t), "sizeof(rgU32) != sizeof(uint32_t)");

void  engineLogfImpl(char const* fmt, ...);

// MACROS
// ------

#define RG_INLINE inline

#define rgKilobyte(x) 1024LL * (x)
#define rgMegabyte(x) 1024LL * rgKilobyte(x)
#define rgGigabyte(x) 1024LL * rgMegabyte(x)

#define rgArrayCount(a) (sizeof(a)/sizeof((a)[0]))
#define rgOffsetOf(type, member) ((uintptr_t)&(((type *)0)->member))
#define rgSizeOfU32(x) ((rgU32)sizeof(x))

#define rgAssert(exp) SDL_assert(exp)
#define rgCriticalCheck(exp)

#define INVALID_DEFAULT_CASE default: rgAssert(!"Not implemented"); break;

#define rgMalloc(s) malloc((s))
#define rgFree(p) free((p))
#define rgNew(objectType) new objectType
#define rgPlacementNew(objectType, placementAddress) new(placementAddress) objectType
#define rgDelete(object) delete object

#define rgLog(...) engineLogfImpl(__VA_ARGS__)

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

// INPUT
// -----

// TODO: struct AppControllerInput + AppMouseState = AppInput

struct GameButtonState
{
    rgBool endedDown;
    rgUInt halfTransitionCount;
};

struct GameMouseState
{
    rgS32 x, y, relX, relY;
    
    union
    {
        GameButtonState buttons[3];
        struct
        {
            GameButtonState left;
            GameButtonState middle;
            GameButtonState right;
        };
    };
};

struct GameControllerInput
{
    union
    {
        // IMPORTANT: update the array count if a button is added or removed
        GameButtonState buttons[7];
        struct
        {
            GameButtonState forward;
            GameButtonState backward;
            GameButtonState left;
            GameButtonState right;
            GameButtonState up;
            GameButtonState down;
            GameButtonState jump;
            // IMPORTANT: update the array count if a button is added or removed
        };
    };
};

#define RG_MAX_GAME_CONTROLLERS 4

struct AppInput
{
    GameMouseState mouse;
    GameControllerInput controllers[RG_MAX_GAME_CONTROLLERS];

    double deltaTime;
    double time;
};

extern AppInput* theAppInput;


// FILE IO
// -------

struct FileData
{
    rgBool  isValid;
    rgU8*   data;
    rgSize  dataSize;
};

FileData    fileRead(const char* filepath);
rgBool      fileWrite(char const* filepath, void* bufferPtr, rgSize bufferSizeInBytes);
void        fileFree(FileData* fd);

// UTILS
// -----

char*       getSaveDataPath();


//
// ---

struct WindowInfo
{
    rgUInt width;
    rgUInt height;
};

extern WindowInfo g_WindowInfo;

extern rgUInt g_FrameNumber; // Frame number since app started

struct PhysicSystem;
extern PhysicSystem* g_PhysicSystem;

extern rgBool g_ShouldAppQuit;
extern SDL_Window* g_AppMainWindow;

// APP
// ---

class TheApp
{
public:
    int  beginApp();
    void beforeUpdateAndDraw();
    void afterUpdateAndDraw();
    void endApp();
    
    virtual ~TheApp() {}
    virtual void onCreateApp() {}
    virtual void setup() {}
    virtual void updateAndDraw() {}
    
protected:
    void setTitle(const char * _title);
    size_t width = 1280;
    size_t height = 720;
    bool fullscreen = false;
    char title[64] = "SdlApp";
    bool vsync = true;
    
    AppInput inputs[2];

    Uint64 currentPerfCounter;
    Uint64 previousPerfCounter;
};

#define THE_APP_MAIN(x) int main(int argc, char *argv[]) {          \
    TheApp *app = new x();                                          \
    app->onCreateApp(); app->beginApp(); app->setup();              \
    while(!g_ShouldAppQuit)                                         \
    { app->beforeUpdateAndDraw(); app->updateAndDraw(); app->afterUpdateAndDraw(); }  \
    app->endApp();              \
    delete app; return 0; }

#endif // __CORE_H__
