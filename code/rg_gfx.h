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
#include <EASTL/shared_ptr.h>

RG_BEGIN_NAMESPACE

#ifdef RG_VULKAN_RNDR
#define rgVK_CHECK(x) do { VkResult errCode = x; if(errCode) { rgLog("%s errCode:%d(0x%x)", #x, errCode, errCode); SDL_assert(!"Vulkan API call failed"); } } while(0)
#endif

// --- Game Graphics APIs
struct GfxTexture2D;

/*
struct Texture
{
// -- data
    rgUInt width;
    rgUInt height;
    TinyImageFormat pixelFormat;
    GfxTexture2D* texture;
};
*/
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

struct QuadUV
{
    rgFloat uvTopLeft[2];
    rgFloat uvBottomRight[2];
};

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx);
QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, Texture* refTexture);

extern QuadUV defaultQuadUV;

void immTexturedQuad2(Texture* texture, QuadUV* quad, rgFloat x, rgFloat y, rgFloat orientationRad = 0.0f, rgFloat scaleX = 1.0f, rgFloat scaleY = 1.0f, rgFloat offsetX = 0.0f, rgFloat offsetY = 0.0f);


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
    rgChar name[32];
    rgUInt width;
    rgUInt height;
    TinyImageFormat pixelFormat;

#if defined(RG_SDL_RNDR)
    SDL_Texture* sdlTexture;
#elif defined(RG_METAL_RNDR)
    MTL::Texture* mtlTexture;
#elif defined(RG_VULKAN_RNDR)
#endif
};

typedef eastl::shared_ptr<GfxTexture2D> GfxTexture2DPtr;

struct RenderCmdList;
#define MAX_FRAMES_IN_QUEUE 2

struct GfxCtx
{
    struct // vars common to all type of contexts
    {
        SDL_Window* mainWindow;
        rgUInt frameNumber;
        RenderCmdList* graphicCmdLists[MAX_FRAMES_IN_QUEUE];
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

extern rg::GfxCtx* g_GfxCtx;
inline GfxCtx* gfxCtx() { return g_GfxCtx; }

struct ImmVertexFormat
{
    rgFloat position[3];
    rgFloat color[4];
};

//-----------------------------------------------------------------------------
// Render Commands
//-----------------------------------------------------------------------------
typedef void (DispatchFnT)(void const* cmd);

/*
                                        sizeof(cmd_type)
                                            |
+---------+sizeof(cmd_packet)+---------+    |
+-------------------------------------------v---------------------+
|NextCmdPacket|DispatchFn|Cmd|AuxMemory|ActualCmd|Actual AuxMemory|
+--------------------------+------+---------^-------------^-------+
                           |      |         |             |
                           +----------------+             |
                                  |                       |
                                  +-----------------------+
*/
struct CmdPacket
{
    CmdPacket       *nextCmdPacket;
    DispatchFnT     *dispatchFn;
    void            *cmd;
    void            *auxMemory;
};

// NOTE: As actual cmd is stored right after the end of the struct cmd_packet.
// We subtract the sizeof(cmd_packet) from the address of the actual cmd to get
// address of the parent cmd_packet.
inline CmdPacket *getCmdPacket(void *cmd)
{
    return (CmdPacket*)((rgU8 *)cmd - sizeof(CmdPacket));
}

// TEMP
typedef rgU32 RenderResource;

struct RenderCmdList
{
    CmdPacket** packets;    /// List of packets
    // TODO: Make is a constant size 10240
    rgU32*      keys;       /// Keys of packets
    rgU32       current;    /// Number of packets in the list
    void*       buffer;     /// Memory buffer
    rgU32       bufferSize; /// Size of memory buffer in bytes
    rgU32       baseOffset; /// Number of bytes used in the memory buffer

    // TODO: Move this to per RenderCmd
    RenderResource ShaderProgram;

    // TODO: Move these to a RenderCmd
    Matrix4    *ViewMatrix;
    Matrix4    *ProjMatrix; 

    RenderCmdList(char const* nametag);
    ~RenderCmdList();

    template <typename U>
    rgU32 GetCmdPacketSize(rgU32 AuxMemorySize);

    // Create a cmd_packet and initialise it's members.
    template <typename U>
    CmdPacket* createCmdPacket(rgU32 auxMemorySize);

    template <typename U>
    U* addCmd(rgU32 key, rgU32 auxMemorySize = 0);

    template <typename U, typename V>
    U* appendCmd(V *cmd, rgU32 auxMemorySize = 0);

    void *AllocateMemory(rgU32 MemorySize); // TODO: gfxBeginRenderCmdList() { allocCmdBuffer();  }
    void Sort(); // TODO: gfxEndRenderCmdList() { sortCmdList(); }
    void Submit(); // TODO: use gfxSubmitRenderCmdList()
    void Flush();
};

template <typename U>
inline rgU32 RenderCmdList::GetCmdPacketSize(rgU32 auxMemorySize)
{
    return sizeof(CmdPacket) + sizeof(U) + auxMemorySize;
}

template <typename U>
inline CmdPacket* RenderCmdList::createCmdPacket(rgU32 auxMemorySize)
{
    CmdPacket *packet = (CmdPacket*)AllocateMemory(GetCmdPacketSize<U>(auxMemorySize));
    packet->nextCmdPacket = nullptr;
    packet->dispatchFn = nullptr;
    packet->cmd = (rgU8*)packet + sizeof(CmdPacket);
    packet->auxMemory = auxMemorySize > 0 ? (rgU8*)packet->cmd + sizeof(U) : nullptr;
    return packet;
}

template <typename U>
inline U* RenderCmdList::addCmd(rgU32 key, rgU32 auxMemorySize)
{
    CmdPacket* packet = createCmdPacket<U>(auxMemorySize);

    const rgU32 I = current++;
    packets[I] = packet;
    keys[I] = key;

    packet->nextCmdPacket = nullptr;
    packet->dispatchFn = U::dispatchFn;

    return (U*)(packet->cmd);
}

template <typename U, typename V>
inline U* RenderCmdList::appendCmd(V* cmd, rgU32 auxMemorySize)
{
    CmdPacket *packet = CreateCmdPacket<U>(auxMemorySize);

    GetCmdPacket(cmd)->nextCmdPacket = packet;
    packet->nextCmdPacket = nullptr;
    packet->dispatchFn = U::dispatchFn;

    return (U*)(packet->cmd);
}


enum class vert_format
{
    P1C1UV1,
    P1C1,
    P1N1UV1
};

enum RenderCmdType
{
    RenderCmdType_ColoredQuad,
    RenderCmdType_TexturedQuad
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

    static DispatchFnT* dispatchFn;
};

// struct RenderCmd_NewTransientResource
// struct RenderCmd_DeleteTransientResource

//struct vert_P1C1UV1
//{
//    vec3 Position;
//    vec2 UV;
//    vec4 Color;
//};
//
//struct vert_P1C1
//{
//    vec3 Position;
//    vec4 Color;
//};
//
//struct vert_P1N1UV1
//{
//    vec3 Position;
//    vec3 Normal;
//    vec2 UV;
//};

namespace cmd
{


    //struct draw
    //{
    //    render_resource     VertexBuffer;
    //    vert_format         VertexFormat;
    //    u32                 StartVertex;
    //    u32                 VertexCount;
    //    render_resource     Textures[8];

    //    static dispatch_fn  *dispatchFn;
    //};
    //static_assert(std::is_pod<draw>::value == true, "Must be a POD.");

    //struct draw_indexed
    //{
    //    render_resource     VertexBuffer;
    //    vert_format         VertexFormat;
    //    render_resource     IndexBuffer;
    //    u32                 IndexCount;
    //    render_resource     Textures[8];

    //    static dispatch_fn  *dispatchFn;
    //};

    //struct copy_const_buffer
    //{
    //    render_resource ConstantBuffer;
    //    void            *Data;
    //    u32             Size;

    //    static dispatch_fn  *dispatchFn;
    //};

    //struct draw_debug_lines
    //{
    //    render_resource     VertexBuffer;
    //    vert_format         VertexFormat;
    //    u32                 StartVertex;
    //    u32                 VertexCount;

    //    static dispatch_fn  *dispatchFn;
    //};
}
//


rgInt updateAndDraw(rg::GfxCtx* gtxCtx, rgDouble dt);

rgInt gfxCommonInit();
rgInt gfxInit();
rgInt gfxDraw();
void  gfxDstry();

RenderCmdList* gfxGetRenderCmdList();

GfxBuffer* gfxNewBuffer(void* data, rgU32 length, GfxMemoryUsage usage);
void gfxUpdateBuffer(GfxBuffer* dstBuffer, void* data, rgU32 length, rgU32 offset = 0);
void gfxDeleteBuffer(GfxBuffer* bufferResource);

GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc);
void gfxDeleleGraphicsPSO(GfxGraphicsPSO* pso);

GfxTexture2D* gfxNewTexture2D(char const* filename, GfxResourceUsage usage); // TODO: remove this
GfxTexture2DPtr gfxNewTexture2D(TexturePtr texture, GfxResourceUsage usage);
GfxTexture2D* gfxNewTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxResourceUsage usage);
void gfxDeleteTexture2D(GfxTexture2D* t2d);

RenderCmdList* gfxBeginRenderCmdList(char const* nametag);
void gfxEndRenderCmdList(RenderCmdList* cmdList);

void gfxSubmitRenderCmdList(RenderCmdList* cmdList); // Submits the list to the GPU, and create a fence to know when it's processed
void gfxDeleteRenderCmdList(RenderCmdList* cmdList);

void gfxHandleRenderCmdTexturedQuad(void const* cmd);

RG_END_NAMESPACE

#endif
