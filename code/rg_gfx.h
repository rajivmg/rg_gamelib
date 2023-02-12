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

    RenderCmdList(rgU32 _BufferSize, RenderResource _ShaderProgram, Matrix4 *_ViewMatrix, Matrix4 *_ProjMatrix);
    ~RenderCmdList();

    template <typename U>
    rgU32 GetCmdPacketSize(rgU32 AuxMemorySize);

    // Create a cmd_packet and initialise it's members.
    template <typename U>
    CmdPacket* CreateCmdPacket(rgU32 AuxMemorySize);

    template <typename U>
    U* AddCommand(rgU32 Key, rgU32 AuxMemorySize = 0);

    template <typename U, typename V>
    U *AppendCommand(V *Cmd, rgU32 AuxMemorySize = 0);

    void *AllocateMemory(rgU32 MemorySize); // TODO: gfxBeginRenderCmdList() { allocCmdBuffer();  }
    void Sort(); // TODO: gfxEndRenderCmdList() { sortCmdList(); }
    void Submit(); // TODO: use gfxSubmitRenderCmdList()
    void Flush();
};

template <typename U>
inline rgU32 RenderCmdList::GetCmdPacketSize(rgU32 AuxMemorySize)
{
    return sizeof(cmd_packet) + sizeof(U) + AuxMemorySize;
}

template <typename U>
inline CmdPacket* RenderCmdList::CreateCmdPacket(rgU32 AuxMemorySize)
{
    cmd_packet *Packet = (cmd_packet *)AllocateMemory(GetCmdPacketSize<U>(AuxMemorySize));
    Packet->NextCmdPacket = nullptr;
    Packet->DispatchFn = nullptr;
    Packet->Cmd = (u8 *)Packet + sizeof(cmd_packet);
    Packet->AuxMemory = AuxMemorySize > 0 ? (u8 *)Packet->Cmd + sizeof(U) : nullptr;

    return Packet;
}

template <typename U>
inline U* RenderCmdList::AddCommand(rgU32 Key, rgU32 AuxMemorySize)
{
    cmd_packet *Packet = CreateCmdPacket<U>(AuxMemorySize);

    const u32 I = Current++;
    Packets[I] = Packet;
    Keys[I] = Key;

    Packet->NextCmdPacket = nullptr;
    Packet->DispatchFn = U::DISPATCH_FUNCTION;

    return (U *)(Packet->Cmd);
}

template <typename U, typename V>
inline U* RenderCmdList::AppendCommand(V* Cmd, rgU32 AuxMemorySize)
{
    cmd_packet *Packet = CreateCmdPacket<U>(AuxMemorySize);

    GetCmdPacket(Cmd)->NextCmdPacket = Packet;
    Packet->NextCmdPacket = nullptr;
    Packet->DispatchFn = U::DISPATCH_FUNCTION;

    return (U *)(Packet->Cmd);
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

struct RenderCmd_TexturedQuad
{
    RenderCmdHeader header;

    Texture* texture;
    TextureQuad* quad;
    rgFloat x;
    rgFloat y;
    rgFloat orientationRad;
    rgFloat scaleX;
    rgFloat scaleY;
    rgFloat offsetX;
    rgFloat offsetY;

    static DispatchFnT* DISPATCH_FUNCTION;
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

    //    static dispatch_fn  *DISPATCH_FUNCTION;
    //};
    //static_assert(std::is_pod<draw>::value == true, "Must be a POD.");

    //struct draw_indexed
    //{
    //    render_resource     VertexBuffer;
    //    vert_format         VertexFormat;
    //    render_resource     IndexBuffer;
    //    u32                 IndexCount;
    //    render_resource     Textures[8];

    //    static dispatch_fn  *DISPATCH_FUNCTION;
    //};

    //struct copy_const_buffer
    //{
    //    render_resource ConstantBuffer;
    //    void            *Data;
    //    u32             Size;

    //    static dispatch_fn  *DISPATCH_FUNCTION;
    //};

    //struct draw_debug_lines
    //{
    //    render_resource     VertexBuffer;
    //    vert_format         VertexFormat;
    //    u32                 StartVertex;
    //    u32                 VertexCount;

    //    static dispatch_fn  *DISPATCH_FUNCTION;
    //};
}
//

rgInt updateAndDraw(rg::GfxCtx* gtxCtx, rgDouble dt);

rgInt gfxInit();
rgInt gfxDraw();
void  gfxDstry();

GfxBuffer* gfxNewBuffer(void* data, rgU32 length, GfxMemoryUsage usage);
void gfxUpdateBuffer(GfxBuffer* dstBuffer, void* data, rgU32 length, rgU32 offset = 0);
void gfxDeleteBuffer(GfxBuffer* bufferResource);

GfxGraphicsPSO* gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc);
void gfxDeleleGraphicsPSO(GfxGraphicsPSO* pso);

GfxTexture2D* gfxNewTexture2D(char const* filename, GfxResourceUsage usage);
void gfxDeleteTexture2D(GfxTexture2D* t2d);

void gfxHandleRenderCmdTexturedQuad(void const* cmd);

RG_END_NAMESPACE

#endif
