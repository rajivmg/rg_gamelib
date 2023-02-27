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
#include "rg_gfx_rendercmd.h"
#include <EASTL/shared_ptr.h>
#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#define MAX_FRAMES_IN_QUEUE 2

RG_BEGIN_NAMESPACE

#ifdef RG_VULKAN_RNDR
#define rgVK_CHECK(x) do { VkResult errCode = x; if(errCode) { rgLog("%s errCode:%d(0x%x)", #x, errCode, errCode); SDL_assert(!"Vulkan API call failed"); } } while(0)
#endif

// --- Game Graphics APIs
struct Texture
{
    rgChar name[32];
    rgUInt width;
    rgUInt height;
    TinyImageFormat format;
    rgU8* buf;
};

typedef eastl::shared_ptr<Texture> TexturePtr;

TexturePtr loadTexture(char const* filename);
void unloadTexture(Texture* t);

//-----------------------------------------------------------------------------
// Quads
//-----------------------------------------------------------------------------
struct QuadUV
{
    // TODO: use Vector4 here?
    rgFloat uvTopLeft[2];
    rgFloat uvBottomRight[2];
};

extern QuadUV defaultQuadUV;

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx);
QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, Texture* refTexture);

//-----------------------------------------------------------------------------
// Textured Quad
//-----------------------------------------------------------------------------
struct TexturedQuad
{
    QuadUV uv;
    Vector2 pos;   // <-- Combine these two?
    Vector2 scale; // <-- ^^^^^^^ Vector4
    Vector2 offset;
    rgFloat orientationRad;
};

typedef eastl::vector<TexturedQuad> TexturedQuads;

template <rgSize N>
using InplaceTexturedQuads = eastl::fixed_vector<TexturedQuad, N>;

TexturedQuad createTexturedQuad(); // TODO: implement

void immTexturedQuad2(Texture* texture, QuadUV* quad, rgFloat x, rgFloat y, rgFloat orientationRad = 0.0f, rgFloat scaleX = 1.0f, rgFloat scaleY = 1.0f, rgFloat offsetX = 0.0f, rgFloat offsetY = 0.0f);

//-----------------------------------------------------------------------------
// General Common Stuff
//-----------------------------------------------------------------------------
rgInt gfxCommonInit();
rgInt setup();
rgInt updateAndDraw(rgDouble dt);

//-----------------------------------------------------------------------------
// STUFF BELOW NEEDS TO BE IMPLEMENTED FOR EACH GRAPHICS BACKEND
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// Gfx Setup
//-----------------------------------------------------------------------------
rgInt gfxInit();
void  gfxDestroy();
rgInt gfxDraw();

//-----------------------------------------------------------------------------
// Gfx Buffers
//-----------------------------------------------------------------------------
struct GfxBuffer
{
    GfxMemoryUsage usageMode;
    rgU32 capacity;

#if defined(RG_METAL_RNDR)
    MTL::Buffer* mtlBuffer;
#elif defined(RG_VULKAN_RNDR)

#endif
};
typedef eastl::shared_ptr<GfxBuffer> GfxBufferPtr;

GfxBuffer*  gfxNewBuffer(void* data, rgU32 length, GfxMemoryUsage usage);
void        gfxUpdateBuffer(GfxBuffer* dstBuffer, void* data, rgU32 length, rgU32 offset);
void        gfxDeleteBuffer(GfxBuffer* bufferResource);

//-----------------------------------------------------------------------------
// Gfx Vertex Format
//-----------------------------------------------------------------------------
struct ImmVertexFormat
{
    rgFloat position[3];
    rgFloat color[4];
};

//-----------------------------------------------------------------------------
// Gfx Pipeline
//-----------------------------------------------------------------------------
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

GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc);
void            gfxDeleleGraphicsPSO(GfxGraphicsPSO* pso);

//-----------------------------------------------------------------------------
// Gfx Texture
//-----------------------------------------------------------------------------
struct GfxTexture2D
{
    rgChar name[32];
    rgUInt width;
    rgUInt height;
    TinyImageFormat pixelFormat;

#if defined(RG_METAL_RNDR)
    MTL::Texture* mtlTexture;
#elif defined(RG_VULKAN_RNDR)

#elif defined(RG_SDL_RNDR)
    SDL_Texture* sdlTexture;
#endif
};
typedef eastl::shared_ptr<GfxTexture2D> GfxTexture2DRef;
typedef GfxTexture2D* GfxTexture2DPtr;

GfxTexture2DRef gfxNewTexture2D(TexturePtr texture, GfxResourceUsage usage);
GfxTexture2DRef gfxNewTexture2D(void* buf, char const* name, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage);
void gfxDeleteTexture2D(GfxTexture2D* t2d);

//-----------------------------------------------------------------------------
// Graphic Context
//-----------------------------------------------------------------------------
struct GfxCtx
{
    GfxCtx() {}
    ~GfxCtx() {}

    SDL_Window* mainWindow;
    rgUInt frameNumber;
    RenderCmdList* graphicCmdLists[MAX_FRAMES_IN_QUEUE];

    // crc32 - GfxTexture2DRef
    typedef eastl::hash_map<rgCRC32, GfxTexture2DRef> HashMapCrc32vsGfxTexture2D;
    HashMapCrc32vsGfxTexture2D textures2D; // create helper functions insert/delete getter as GfxTexture2D

#if defined(RG_SDL_RNDR)
    struct SDLGfxCtx
    {
        SDL_Renderer* renderer;
        GfxTexture2DRef tTex;
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

        GfxTexture2DRef birdTexture;
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
extern rg::GfxCtx* g_GfxCtx;

inline GfxCtx* gfxCtx() { return g_GfxCtx; }

GfxTexture2DPtr gfxGetTexture2DPtr(rgCRC32 id);

//-----------------------------------------------------------------------------
// Gfx Render Command
//-----------------------------------------------------------------------------
enum RenderCmdType
{
    RenderCmdType_ColoredQuad,
    RenderCmdType_TexturedQuad,
    RenderCmdType_TexturedQuads
};

struct RenderCmdHeader
{
    RenderCmdType type;
};

// NOTE: RenderCmds are not allowed to own any resources (smartptrs)
// Should this be allowed?? If yes, then addCmd should "new" the RenderCmd, and Flush should "delete"
// pros vs cons?

struct RenderCmdTexturedQuad
{
    RenderCmdHeader header;

    GfxTexture2DPtr texture;
    QuadUV* quad;
    rgFloat x;
    rgFloat y;
    rgFloat orientationRad;
    rgFloat scaleX;
    rgFloat scaleY;
    rgFloat offsetX;
    rgFloat offsetY;

    static CmdDispatchFnT* dispatchFn;
    //static CmdDestructorFnT* destructorFn;
};

struct RenderCmdTexturedQuads
{
    //RenderCmdHeader header; // TODO: instead of header, only store static const type?
    static const RenderCmdType type = RenderCmdType_TexturedQuads;
    
    GfxTexture2DPtr texture;
    
};

// struct RenderCmd_NewTransientResource
// struct RenderCmd_DeleteTransientResource

void gfxHandleRenderCmdTexturedQuad(void const* cmd);
//void gfxDestroyRenderCmdTexturedQuad(void* cmd);

RG_END_NAMESPACE

#endif
