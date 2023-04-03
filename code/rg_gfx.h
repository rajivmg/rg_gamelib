#ifndef __RG_GFX_H__
#define __RG_GFX_H__

#if defined(RG_VULKAN_RNDR)
//#include <vulkan/vulkan.h>
#include "volk/volk.h"
#include "vk-bootstrap/VkBootstrap.h"
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

#define RG_MAX_FRAMES_IN_FLIGHT 3

RG_BEGIN_NAMESPACE

#ifdef RG_VULKAN_RNDR
#define rgVK_CHECK(x) do { VkResult errCode = x; if(errCode) { rgLog("%s errCode:%d(0x%x)", #x, errCode, errCode); SDL_assert(!"Vulkan API call failed"); } } while(0)
#endif

// --- Game Graphics APIs
struct Texture
{
    rgHash  hash;
    rgU8*   buf;
    rgUInt  width;
    rgUInt  height;
    TinyImageFormat format;
    
    // -- remove in final build
    rgChar name[32];
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
#if 0
    Vector2 pos;   // <-- Combine these two?
    Vector2 scale; // <-- ^^^^^^^ Vector4
#else
    //Vector4 posScale;
    rgFloat4 posSize;
#endif
    
#if 0
    Vector2 offset;
    rgFloat orientationRad;
#else
    //Vector4 offsetOrientation;
    rgFloat4 offsetOrientation;
#endif
    
    rgU32 texID;
    //TexturedQuad(QuadUV uv_, Vector4 posScale_, Vector4 offsetOrientation_);
};

typedef eastl::vector<TexturedQuad> TexturedQuads;

// Is this possible? this will be deleted by the time RenderCmd is processed
template <rgSize N>
using InplaceTexturedQuads = eastl::fixed_vector<TexturedQuad, N>;

/*
inline TexturedQuad* pushTexturedQuad(TexturedQuads* quadList, QuadUV uv, Vector4 posScale, Vector4 offsetOrientation)
{
    TexturedQuad& tQuad = quadList->emplace_back(uv, posScale, offsetOrientation);
    return &tQuad;
}

template <rgSize N>
inline TexturedQuad* pushTexturedQuad(InplaceTexturedQuads<N>* quadList, QuadUV uv, Vector4 posScale, Vector4 offsetOrientation)
{
    TexturedQuad& tQuad = quadList->emplace_back({uv, posScale, offsetOrientation});
    return &tQuad;
}
 */

inline void pushTexturedQuad(TexturedQuads* quadList, QuadUV uv, rgFloat4 posSize, rgFloat4 offsetOrientation, rgU32 texID)
{
    TexturedQuad& q = quadList->push_back();
    q.uv = uv;
    q.posSize = posSize;
    q.offsetOrientation = offsetOrientation;
    q.texID = texID;
}

template <rgSize N>
inline void pushTexturedQuad(InplaceTexturedQuads<N>* quadList, QuadUV uv, rgFloat4 posSize, rgFloat4 offsetOrientation, rgU32 texID)
{
    TexturedQuad& q = quadList->push_back();
    q.uv = uv;
    q.posSize = posSize;
    q.offsetOrientation = offsetOrientation;
    q.texID = texID;
}

//-----------------------------------------------------------------------------
// General Common Stuff
//-----------------------------------------------------------------------------
rgInt gfxCommonInit();
rgInt setup();
rgInt updateAndDraw(rgDouble dt);

//-----------------------------------------------------------------------------
// STUFF BELOW NEEDS TO BE IMPLEMENTED FOR EACH GRAPHICS BACKEND
//-----------------------------------------------------------------------------

static const rgU32 kInvalidHandle = ~(0x0);

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
    GfxResourceUsage_ReadWrite,
    
    GfxResourceUsage_Static,    // Immutable, once created content cannot be updated
    GfxResourceUsage_Dynamic,   // Content will be updated infrequently
    GfxResourceUsage_Stream     // Content will be updated every frame
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
    GfxResourceUsage usageMode;
    rgSize size;
    rgInt activeIdx;
#if defined(RG_METAL_RNDR)
    MTL::Buffer* mtlBuffers[RG_MAX_FRAMES_IN_FLIGHT];
#elif defined(RG_VULKAN_RNDR)

#endif
};
typedef eastl::shared_ptr<GfxBuffer> GfxBufferPtr;

GfxBuffer*  gfxNewBuffer(void* data, rgSize size, GfxResourceUsage usage);
void        gfxUpdateBuffer(GfxBuffer* buffer, void* data, rgSize size, rgU32 offset);
void        gfxDeleteBuffer(GfxBuffer* buffer);

//-----------------------------------------------------------------------------
// Gfx Vertex Format
//-----------------------------------------------------------------------------
struct ImmVertexFormat
{
    rgFloat position[3];
    rgFloat color[4];
};

struct SimpleVertexFormat
{
    rgFloat pos[2];
    rgFloat texcoord[2];
    rgFloat color[4];
};

struct SimpleInstanceParams
{
    rgU32 texID;
};

void genTexturedQuadVertices(TexturedQuads* quadList, eastl::vector<SimpleVertexFormat>* vertices, eastl::vector<SimpleInstanceParams>* instanceParams);

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
typedef rgU32 GfxTexture2DHandle;
static_assert(sizeof(GfxTexture2DHandle) == sizeof(TexturedQuad::texID), "sizeof(GfxTexture2DHandle) == sizeof(TexturedQuad::texID)");

struct GfxTexture2D
{
    rgChar name[32];
    rgUInt width;
    rgUInt height;
    TinyImageFormat pixelFormat;
    GfxTexture2DHandle handle;
    
#if defined(RG_METAL_RNDR)
    MTL::Texture* mtlTexture;
#elif defined(RG_VULKAN_RNDR)

#elif defined(RG_SDL_RNDR)
    SDL_Texture* sdlTexture;
#endif
};
typedef eastl::shared_ptr<GfxTexture2D> GfxTexture2DRef;
typedef GfxTexture2D* GfxTexture2DPtr;

GfxTexture2DRef creatorGfxTexture2D(GfxTexture2DHandle handle, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage, char const* name);
void deleterGfxTexture2D(GfxTexture2D* t2d);

//-----------------------------------------------------------------------------
// Graphic Context
//-----------------------------------------------------------------------------
struct GfxCtx
{
    GfxCtx() {}
    ~GfxCtx() {}

    SDL_Window* mainWindow;
    rgUInt frameNumber;
    rgS32 frameIndex;
    
    RenderCmdList* graphicCmdLists[RG_MAX_FRAMES_IN_FLIGHT];

    typedef eastl::vector<GfxTexture2DRef> HandleListGfxTexture2D;
    HandleListGfxTexture2D textures2D;
    eastl::vector<GfxTexture2DHandle> textures2DFreeHandles;
    
    Matrix4 orthographicMatrix;
    
    eastl::vector<GfxTexture2DHandle> debugTextureHandles; // test only
    
    // RenderCmdTexturedQuads
    GfxBuffer* rcTexturedQuadsVB;

#if defined(RG_SDL_RNDR)
    struct SDLGfxCtx
    {
        SDL_Renderer* renderer;
        GfxTexture2DRef tTex;
    } sdl;
#elif defined(RG_METAL_RNDR)
    struct Mtl
    {
        void* layer; // type: id<CAMetalLayer>
        NS::View *view;
        MTL::Device *device;
        MTL::CommandQueue* commandQueue;
        
        dispatch_semaphore_t framesInFlightSemaphore;
        
        MTL::RenderCommandEncoder* currentRenderEncoder;

        // -- immediate mode resources
        GfxBuffer* immVertexBuffer;
        rgUInt immCurrentVertexCount;
        GfxBuffer* immIndexBuffer;
        rgUInt immCurrentIndexCount;
        GfxGraphicsPSO* immPSO;
        
        GfxGraphicsPSO* simple2dPSO;

        GfxTexture2DHandle birdTexture;
        
        // arg buffers
        MTL::ArgumentEncoder* largeArrayTex2DArgEncoder;
        GfxBuffer* largeArrayTex2DArgBuffer;
    } mtl;
#elif defined(RG_VULKAN_RNDR)
    struct VkGfxCtx
    {
        vkb::Instance vkbInstance;
        vkb::Device vkbDevice;
        vkb::Swapchain vkbSwapchain;

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
        std::vector<VkImage> swapchainImages;
        std::vector<VkImageView> swapchainImageViews;
        eastl::vector<VkFramebuffer> framebuffers;

        VkRenderPass globalRenderPass;

        //VkFormat swapchainDesc; // is this the right way to store this info?

        PFN_vkCreateDebugReportCallbackEXT fnCreateDbgReportCallback;
        PFN_vkDestroyDebugReportCallbackEXT fnDestroyDbgReportCallback;
        VkDebugReportCallbackEXT dbgReportCallback;
    } vk;
#endif
};
extern rg::GfxCtx* g_GfxCtx;

inline GfxCtx* gfxCtx() { return g_GfxCtx; }

GfxTexture2DHandle gfxNewTexture2D(TexturePtr texture, GfxResourceUsage usage);
GfxTexture2DHandle gfxNewTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage, char const* name);
GfxTexture2DPtr gfxGetTexture2DPtr(GfxTexture2DHandle handle);

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
/*
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
*/
struct RenderCmdTexturedQuads
{
    //RenderCmdHeader header; // TODO: instead of header, only store static const type?
    static const RenderCmdType type = RenderCmdType_TexturedQuads;
    
    GfxTexture2DPtr texture; //replace this with unbound texture array index
    TexturedQuads* quads;
    
    static CmdDispatchFnT* dispatchFn;
};

// struct RenderCmd_NewTransientResource
// struct RenderCmd_DeleteTransientResource

void gfxHandleRenderCmdTexturedQuad(void const* cmd);
void gfxHandleRenderCmdTexturedQuads(void const* cmd);
//void gfxDestroyRenderCmdTexturedQuad(void* cmd);

RG_END_NAMESPACE

#endif
