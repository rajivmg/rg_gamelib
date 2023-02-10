#ifndef __RG_GFX_H__
#define __RG_GFX_H__

#if defined(RG_VULKAN_RNDR)
#include <vulkan/vulkan.h>
#elif defined(RG_METAL_RNDR)
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
#endif

#include "rg.h"

RG_BEGIN_NAMESPACE

#ifdef RG_VULKAN_RNDR
#define rgVK_CHECK(x) do { VkResult errCode = x; if(errCode) { rgLog("%s errCode:%d(0x%x)", #x, errCode, errCode); SDL_assert(!"Vulkan API call failed"); } } while(0)
#endif

// --- Game Graphics APIs
struct GfxTexture2D;

struct Texture
{
// -- data
    rgUInt width;
    rgUInt height;
    TinyImageFormat pixelFormat;
    GfxTexture2D* texture;
};

struct TextureQuad
{
    // -- data
    rgFloat uvTopLeft[2];
    rgFloat uvBottomRight[2];
    
    // -- func
    TextureQuad(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx);
    TextureQuad(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, Texture* refTexture);
};

void immTexturedQuad2(Texture* texture, TextureQuad* quad, rgFloat x, rgFloat y, rgFloat orientationRad = 0.0f, rgFloat scaleX = 1.0f, rgFloat scaleY = 1.0f, rgFloat offsetX = 0.0f, rgFloat offsetY = 0.0f);


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
    rgU32 capacity;
#if defined(RG_METAL_RNDR)
    MTL::Buffer* mtlBuffer;
#elif defined(RG_VULKAN_RNDR)
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
    static const rgUInt MAX_COLOR_ATTACHMENTS = 8;
 
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
#if defined(RG_METAL_RNDR)
    MTL::RenderPipelineState* mtlPSO;
#elif defined(RG_VULKAN_RNDR)
#endif
};

struct GfxTexture2D
{
#if defined(RG_SDL_RNDR)
    SDL_Texture* sdlTexture;
#elif defined(RG_METAL_RNDR)
    MTL::Texture* mtlTexture;
#elif defined(RG_VULKAN_RNDR)
#endif
};

struct GfxCtx
{
    struct // vars common to all type of contexts
    {
        SDL_Window* mainWindow;
        //SDL_Renderer* sdlRenderer;
    };

    union
    {
#if defined(RG_SDL_RNDR)
        struct SDLGfxCtx
        {
            SDL_Renderer* renderer;
        } sdl;
#elif defined(RG_METAL_RNDR)
        struct
        {
            void* layer; // type: id<CAMetalLayer>
            NS::View *view;
            MTL::Device *device;
            MTL::CommandQueue* commandQueue;
            
            // -- immediate mode resources
            GfxBuffer* immVertexBuffer;
            rgUInt immCurrentVertexCount;
            GfxBuffer* immIndexBuffer;
            rgUInt immCurrentIndexCount;
            GfxGraphicsPSO* immPSO;
            
            GfxTexture2D* birdTexture;
        } mtl;
#elif defined(RG_VULKAN_RNDR)
        struct
        {
            VkInstance inst;
            VkPhysicalDevice physicalDevice;
            rgUInt graphicsQueueIndex;

            VkDevice device;
            rgUInt deviceExtCount;
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
inline GfxCtx* gfxCtx() { return &g_GfxCtx; }

struct ImmVertexFormat
{
    rgFloat position[3];
    rgFloat color[4];
};


rgInt updateAndDraw(rg::GfxCtx* gtxCtx, rgDouble dt);

rgInt gfxInit();
rgInt gfxDraw();

GfxBuffer* gfxNewBuffer(void* data, rgU32 length, GfxMemoryUsage usage);
void gfxUpdateBuffer(GfxBuffer* dstBuffer, void* data, rgU32 length, rgU32 offset = 0);
void gfxDeleteBuffer(GfxBuffer* bufferResource);

GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc);
void gfxDeleleGraphicsPSO(GfxGraphicsPSO* pso);

GfxTexture2D* gfxNewTexture2D(char const* filename, GfxResourceUsage usage);
void gfxDeleteTexture2D(GfxTexture2D* t2d);

RG_END_NAMESPACE

#endif
