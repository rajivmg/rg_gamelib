#ifndef _RG_GFX_H_
#define _RG_GFX_H_

#if defined(RG_VULKAN)
#include <vulkan/vulkan.h>
#elif defined(RG_METAL)
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
#endif

#include <stdint.h>
#include <SDL2/SDL.h>
#include <tiny_imageformat/tinyimageformat.h>
#include <vectormath/vectormath.hpp>

#define RG_BEGIN_NAMESPACE namespace rg {
#define RG_END_NAMESPACE }

RG_BEGIN_NAMESPACE

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

#define rgKILOBYTE(x) 1024LL * (x)
#define rgMEGABYTE(x) 1024LL * Kilobyte(x)
#define rgGIGABYTE(x) 1024LL * Megabyte(x)
#define rgARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define rgOFFSET_OF(type, member) ((uintptr_t)&(((type *)0)->member))
#define rgAssert(exp) SDL_assert(exp)
#define rgMalloc(s) malloc((s))
#define rgFree(p) free((p))

void _LogImpl(char const *fmt, ...);
#define rgLog(...) _LogImpl(__VA_ARGS__)

#ifdef RG_VULKAN
#define rgVK_CHECK(x) do { VkResult errCode = x; if(errCode) { rgLog("%s errCode:%d(0x%x)", #x, errCode, errCode); SDL_assert(!"Vulkan API call failed"); } } while(0)
#endif

// --- Game Graphics APIs
struct GfxTexture2D;

struct Texture
{
// -- data
    UInt width;
    UInt height;
    TinyImageFormat pixelFormat;
    GfxTexture2D* texture;
#ifdef RG_METAL
    MTL::PixelFormat mtlPixelFormat;
    MTL::Texture* mtlTexture;
#endif
};

struct TextureQuad
{
    // -- data
    R32 uvTopLeft[2];
    R32 uvBottomRight[2];
    
    // -- func
    TextureQuad(U32 xPx, U32 yPx, U32 widthPx, U32 heightPx, U32 refWidthPx, U32 refHeightPx);
    TextureQuad(U32 xPx, U32 yPx, U32 widthPx, U32 heightPx, Texture* refTexture);
};

void immTexturedQuad2(Texture* texture, TextureQuad* quad, R32 x, R32 y, R32 orientationRad = 0.0f, R32 scaleX = 1.0f, R32 scaleY = 1.0f, R32 offsetX = 0.0f, R32 offsetY = 0.0f);


// --- Low-level Graphics Functions

enum GfxMemoryUsage
{
    GfxMemoryUsage_CPUToGPU,
    GfxMemoryUsage_GPUOnly,
    GfxMemoryUsage_CPUOnly
};

enum GfxResourceUsage
{
    GfxResourceUsage_Read,
    GfxResourceUsage_Write,
    GfxResourceUsage_ReadWrite
};

struct GfxBuffer
{
    GfxMemoryUsage usageMode;
    U32 capacity;
#if defined(RG_METAL)
    MTL::Buffer* mtlBuffer;
#elif defined(RG_VULKAN)
#endif
};

enum GfxShaderType
{
    GfxShaderType_Vertex,
    GfxShaderType_Fragment,
    GfxShaderType_Compute
};

struct GfxColorAttachementDesc
{
    TinyImageFormat pixelFormat;
};

struct GfxRenderStateDesc
{
    static const UInt MAX_COLOR_ATTACHMENTS = 8;
 
    GfxColorAttachementDesc colorAttachments[MAX_COLOR_ATTACHMENTS];
    TinyImageFormat depthAttachementFormat;
    TinyImageFormat stencilAttachementFormat;
};

struct GfxShaderDesc
{
    char const* shaderSrcCode;
    char const* vsEntryPoint;
    char const* fsEntryPoint;
    char const* csEntryPoint;
    char const* macros;
};

struct GfxGraphicsPSO
{
#if defined(RG_METAL)
    MTL::RenderPipelineState* mtlPSO;
#elif defined(RG_VULKAN)
#endif
};

struct GfxTexture2D
{
#if defined(RG_METAL)
    MTL::Texture* mtlTexture;
#elif defined(RG_VULKAN)
#endif
};

struct GfxCtx
{
    struct // vars common to all type of contexts
    {
        SDL_Window* mainWindow;
        SDL_Renderer* sdlRenderer;
    };

    union
    {
#if defined(RG_METAL)
        struct
        {
            void* layer; // type: id<CAMetalLayer>
            NS::View *view;
            MTL::Device *device;
            MTL::CommandQueue* commandQueue;
            
            // -- immedia  te mode resources
            GfxBuffer* immVertexBuffer;
            U32 immCurrentVertexCount;
            GfxBuffer* immIndexBuffer;
            U32 immCurrentIndexCount;
            GfxGraphicsPSO* immPSO;
            
            GfxTexture2D* birdTexture;
        } mtl;
#elif defined(RG_VULKAN)
        struct
        {
            VkInstance inst;
            VkPhysicalDevice physicalDevice;
            UInt graphicsQueueIndex;

            VkDevice device;
            UInt deviceExtCount;
            char const** deviceExtNames;
            VkQueue graphicsQueue;

            VkCommandPool graphicsCmdPool;
            VkCommandBuffer graphicsCmdBuffer;

            VkSurfaceKHR surface;
            VkSwapchainKHR swapchain;
            VkImageView* swapchainImageViews;

            //VkFormat swapchainDesc; // is this the right way to store this info?

            PFN_vkCreateDebugReportCallbackEXT fnCreateDbgReportCallback;
            PFN_vkDestroyDebugReportCallbackEXT fnDestroyDbgReportCallback;
            VkDebugReportCallbackEXT dbgReportCallback;
        } vk;
#endif
    };
};
extern rg::GfxCtx g_GfxCtx;

struct ImmVertexFormat
{
    R32 position[3];
    R32 color[4];
};

rg::Int updateAndDraw(rg::GfxCtx* gtxCtx, rg::Double dt);

GfxCtx* gfxCtx();

Int gfxInit();
Int gfxDraw();

GfxBuffer* gfxNewBuffer(void* data, U32 length, GfxMemoryUsage usage);
void gfxUpdateBuffer(GfxBuffer* dstBuffer, void* data, U32 length, U32 offset = 0);
void gfxDeleteBuffer(GfxBuffer* bufferResource);

GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc);
void gfxDeleleGraphicsPSO(GfxGraphicsPSO* pso);

GfxTexture2D* gfxNewTexture2D(char const* filename, GfxResourceUsage usage);
void gfxDeleteTexture2D(GfxTexture2D* t2d);

RG_END_NAMESPACE

#endif
