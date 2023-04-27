#ifndef __RG_GFX_H__
#define __RG_GFX_H__

#if defined(RG_D3D12_RNDR)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <wrl.h>
    #include <d3d12.h>
    #include <d3dcompiler.h>
    #include <d3dx12.h>
    #include <dxgi1_6.h>
    using namespace Microsoft::WRL;
#elif defined(RG_METAL_RNDR)
    #include <Metal/Metal.hpp>
    #include <AppKit/AppKit.hpp>
    #include <MetalKit/MetalKit.hpp>
#elif defined(RG_VULKAN_RNDR)
    #include "volk/volk.h"
    #include "vk-bootstrap/VkBootstrap.h"
    #include "vk_mem_alloc.h"
#elif defined(RG_OPENGL_RNDR)
    #include <GL/glew.h>
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
struct GfxTexture2D;

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
    
    GfxTexture2D* tex;
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

inline void pushTexturedQuad(TexturedQuads* quadList, QuadUV uv, rgFloat4 posSize, rgFloat4 offsetOrientation, GfxTexture2D* tex)
{
    TexturedQuad& q = quadList->push_back();
    q.uv = uv;
    q.posSize = posSize;
    q.offsetOrientation = offsetOrientation;
    q.tex = tex;
}

template <rgSize N>
inline void pushTexturedQuad(InplaceTexturedQuads<N>* quadList, QuadUV uv, rgFloat4 posSize, rgFloat4 offsetOrientation, GfxTexture2D* tex)
{
    TexturedQuad& q = quadList->push_back();
    q.uv = uv;
    q.posSize = posSize;
    q.offsetOrientation = offsetOrientation;
    q.tex = tex;
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
static const rgU32 kUninitializedHandle = 0;
static const rgU32 kMaxColorAttachments = 4;

enum GfxMemoryUsage
{
    GfxMemoryUsage_CPUToGPU, // UploadHeap
    GfxMemoryUsage_GPUToCPU, // ReadbackHeap
    GfxMemoryUsage_GPUOnly,  // DefaultHeap
    //GfxMemoryUsage_None     =   (0 << 0),
    //GfxMemoryUsage_GPURead  =   (1 << 0),
    //GfxMemoryUsage_GPUWrite =   (1 << 1),
    //GfxMemoryUsage_GPUReadWrite = (GfxMemoryUsage_GPURead | GfxMemoryUsage_GPUWrite),
    //GfxMemoryUsage_CPURead  =   (1 << 2),
    //GfxMemoryUsage_CPUWrite =   (1 << 3),
    //GfxMemoryUsage_CPUReadWrite = (GfxMemoryUsage_CPURead | GfxMemoryUsage_CPUWrite),
};

enum GfxResourceUsage
{
    GfxResourceUsage_Static,    // Immutable, once created content cannot be modified
    GfxResourceUsage_Dynamic,   // Content will be updated infrequently
    GfxResourceUsage_Stream,    // Content will be updated every frame
};

enum GfxTextureUsage
{
    GfxTextureUsage_ShaderRead,
    GfxTextureUsage_ShaderWrite,
    GfxTextureUsage_RenderTarget,
};

enum GfxCompareFunc
{
    GfxCompareFunc_Always,
    GfxCompareFunc_Never,
    GfxCompareFunc_Equal,
    GfxCompareFunc_NotEqual,
    GfxCompareFunc_Less,
    GfxCompareFunc_LessEqual,
    GfxCompareFunc_Greater,
    GfxCompareFunc_GreaterEqual,
};

enum GfxCullMode
{
    GfxCullMode_None,
    GfxCullMode_Back,
    GfxCullMode_Front,
};

enum GfxWinding
{
    GfxWinding_CCW,
    GfxWinding_CW,
};

enum GfxTriangleFillMode
{
    GfxTriangleFillMode_Fill,
    GfxTriangleFillMode_Wireframe,
};

enum GfxLoadAction
{
    GfxLoadAction_DontCare,
    GfxLoadAction_Load,
    GfxLoadAction_Clear,
};

enum GfxStoreAction
{
    GfxStoreAction_DontCare,
    GfxStoreAction_Store,
};

//-----------------------------------------------------------------------------
// Gfx Setup
//-----------------------------------------------------------------------------
rgInt gfxInit();
void  gfxDestroy();
rgInt gfxDraw();
void  gfxOnSizeChanged();
void  gfxUpdateCurrentBackBufferIndex(); // TODO: Implement

//-----------------------------------------------------------------------------
// Gfx Buffers
//-----------------------------------------------------------------------------
struct GfxBuffer
{
    rgChar tag[32];
    rgU32     size;
    GfxResourceUsage usage;
    rgInt activeSlot;
#if defined(RG_D3D12_RNDR)
#elif defined(RG_METAL_RNDR)
    MTL::Buffer* mtlBuffers[RG_MAX_FRAMES_IN_FLIGHT];
#elif defined(RG_VULKAN_RNDR)
    VkBuffer vkBuffers[RG_MAX_FRAMES_IN_FLIGHT];
    VmaAllocation vmaAlloc;
#endif
};
//typedef rgU32 HGfxBuffer;
//
//HGfxBuffer  gfxNewBuffer(void* data, rgSize size, GfxResourceUsage usage);
//void        gfxUpdateBuffer(HGfxBuffer handle, void* data, rgSize size, rgU32 offset);
//void        gfxDeleteBuffer(HGfxBuffer handle);
//GfxBuffer*  gfxBufferPtr(HGfxBuffer bufferHandle);
//
//GfxBuffer* creatorGfxBuffer(void* data, rgSize size, GfxResourceUsage usage);
//void updaterGfxBuffer(GfxBuffer* buffer, void* data, rgSize size, rgU32 offset);
//void deleterGfxBuffer(GfxBuffer* buffer);

//-----------------------------------------------------------------------------
// Gfx Texture
//-----------------------------------------------------------------------------
struct GfxTexture2D
{
    rgChar tag[32];
    rgUInt width;
    rgUInt height;
    TinyImageFormat format;
    GfxTextureUsage usage;
    rgU32 texID;
   
#if defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource> d3dTexture;
#elif defined(RG_METAL_RNDR)
    MTL::Texture* mtlTexture;
#elif defined(RG_VULKAN_RNDR)
    VkImage vkTexture;
    VmaAllocation vmaAlloc;
#elif defined(RG_OPENGL_RNDR)
    GLuint glTexture;
#endif
};
//typedef rgU32 HGfxTexture2D;
//static_assert(sizeof(HGfxTexture2D) == sizeof(TexturedQuad::texID), "sizeof(HGfxTexture2D) == sizeof(TexturedQuad::texID)");
//
//HGfxTexture2D gfxNewTexture2D(GfxTexture2D* ptr);
//HGfxTexture2D gfxNewTexture2D(TexturePtr texture, GfxTextureUsage usage);
//HGfxTexture2D gfxNewTexture2D(char const* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage);
//void gfxDeleteTexture2D(HGfxTexture2D handle);
//GfxTexture2D* gfxTexture2DPtr(HGfxTexture2D texture2dHandle);

//GfxTexture2D* creatorGfxTexture2D(char const* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage);
//void deleterGfxTexture2D(GfxTexture2D* t2d);

struct GfxRenderTarget
{
    rgChar tag[32]; // myrendermyrendr
    rgU32 width;
    rgU32 height;
    TinyImageFormat format;
#if defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource> d3dRT;
#endif
};

///GfxRenderTarget* creatorGfxRenderTarget(char const* tag, rgU32 width, rgU32 height, TinyImageFormat format, GfxRenderTarget* obj);
//void deleterGfxRenderTarget(GfxRenderTarget* ptr);

#define ENABLE_GFX_OBJECT_INVALID_TAG_OP_ASSERT

#define DeclareGfxObjectFunctions(type, ...) Gfx##type* gfxCreate##type(const char* tag, __VA_ARGS__); \
        Gfx##type* gfxFindOrCreate##type(const char* tag, __VA_ARGS__); \
        Gfx##type* gfxFind##type(rgHash tagHash);  \
        Gfx##type* gfxFind##type(char const* tag); \
        void gfxDestroy##type(rgHash tagHash); \
        void gfxDestroy##type(char const* tag); \
        void allocAndFill##type##Struct(const char* tag, __VA_ARGS__, Gfx##type** obj); \
        void dealloc##type##Struct(Gfx##type* obj); \
        Gfx##type* creatorGfx##type(char const* tag, __VA_ARGS__, Gfx##type* obj); \
        void destroyerGfx##type(Gfx##type* obj)

GfxTexture2D* gfxCreateTexture2D(char const* tag, TexturePtr texture, GfxTextureUsage usage);
DeclareGfxObjectFunctions(Texture2D, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage);
DeclareGfxObjectFunctions(RenderTarget, rgU32 width, rgU32 height, TinyImageFormat format);

void gfxUpdateBuffer(rgHash tagHash, void* buf, rgU32 size, rgU32 offset);
void gfxUpdateBuffer(char const* tag, void* buf, rgU32 size, rgU32 offset);
DeclareGfxObjectFunctions(Buffer, void* buf, rgU32 size, GfxResourceUsage usage);
void updaterGfxBuffer(void* buf, rgU32 size, rgU32 offset, GfxBuffer* obj);

//GfxRenderTarget* gfxCreateRenderTarget(const char* tag, rgU32 width, rgU32 height, TinyImageFormat format);
//GfxRenderTarget* gfxFindOrCreateRenderTarget(const char* tag, rgU32 width, rgU32 height, TinyImageFormat format);
//GfxRenderTarget* gfxFindRenderTarget(rgHash tagHash);
//GfxRenderTarget* gfxFindRenderTarget(char const* tag);
//void             gfxDestroyRenderTarget(rgHash tagHash);
//void             gfxDestroyRenderTarget(char const* tag);

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

struct GfxColorAttachementStateDesc
{
    TinyImageFormat pixelFormat;
    rgBool blendingEnabled;

    // Create predefined common blending operations instead of giving fine control like vv
    //GfxBlendOp blendOp;
    //GfxSourceBlendFacter srcBlendFactor;
};

struct GfxRenderStateDesc
{
    GfxColorAttachementStateDesc colorAttachments[kMaxColorAttachments];
    TinyImageFormat depthStencilAttachmentFormat;

    rgBool depthWriteEnabled;
    GfxCompareFunc depthCompareFunc;
    
    GfxCullMode cullMode;
    GfxWinding winding;
    GfxTriangleFillMode triangleFillMode;
    
    //GfxCullMode
    // TODO
    // DepthStencilState ^^

    
    // Viewport <--- not part of static state
    // ScissorRect <--- not part of static state
    // CullMode
    // WindingMode CW CCW
    // Primitive Type, Line, LineStrip, Triangle, TriangleStrip
    // Index Type, U16, U32
};

struct GfxColorAttachmentDesc
{
    GfxRenderTarget*      texture;
    GfxLoadAction   loadAction;
    GfxStoreAction storeAction;
    rgFloat4        clearColor;
};

struct GfxRenderPass
{
    GfxColorAttachmentDesc colorAttachments[kMaxColorAttachments];
    
    GfxRenderTarget* depthStencilAttachmentTexture;
    GfxLoadAction depthStencilAttachmentLoadAction;
    GfxStoreAction depthStencilAttachmentStoreAction;
    rgFloat  clearDepth;
    rgU32    clearStencil;
};

struct GfxShaderDesc
{
    char const* shaderSrcCode;
    char const* vsEntryPoint;
    char const* fsEntryPoint;
    char const* csEntryPoint;
    char const* macros;
};

// TODO: Not a resource
struct GfxGraphicsPSO 
{
    rgChar tag[32];
    // TODO: No vertex attrib, only index attrib. Shader fetch vertex data from buffers directly.
    GfxRenderStateDesc renderState;
#if defined(RG_D3D12_RNDR)
#elif defined(RG_METAL_RNDR)
    void* mtlPSO; // type: id<MTLRenderPipelineState>
#elif defined(RG_VULKAN_RNDR)
#endif
};

//typedef rgU32 HGfxGraphicsPSO;
//
//HGfxGraphicsPSO gfxNewGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc);
//void            gfxDeleleGraphicsPSO(HGfxGraphicsPSO handle);
//GfxGraphicsPSO* gfxGraphicsPSOPtr(HGfxGraphicsPSO handle);
//
//GfxGraphicsPSO* creatorGfxGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc);
//void deleterGfxGraphicsPSO(GfxGraphicsPSO* pso);

DeclareGfxObjectFunctions(GraphicsPSO, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc);

//-----------------------------------------------------------------------------
// Resource Binding
//-----------------------------------------------------------------------------

enum GfxDataType
{
    GfxDataType_Buffer,
    GfxDataType_Texture,
    GfxDataType_Sampler,
};

enum GfxTextureType
{
    GfxTextureType_2D,
    GfxTextureType_1D,
    GfxTextureType_3D,
};

struct GfxDescriptor
{
    rgU32 index;
    rgU32 arrayLength;
    GfxDataType type;
    GfxTextureType textureType;
};

struct GfxDescriptorBufferEncoder
{
#if defined(RG_D3D12_RNDR)
#elif defined(RG_METAL_RNDR)
    void* mtlArgEncoder; // type: id<MTLArgumentEncoder>
#elif defined(RG_VULKAN_RNDR)

#endif
};


//-----------------------------------------------------------------------------
// Graphic Context
//-----------------------------------------------------------------------------
template <typename Type>
struct GfxBindlessResourceManager
{
    typedef eastl::vector<Type*> ResourceList;
    ResourceList resources; // Hold reference to the resources until no longer needed
    
    typedef eastl::vector<rgU32> SlotList;
    SlotList freeSlots; // Indexes in referenceList which are unused

    rgU32 getFreeSlot()
    {
        rgU32 result = kInvalidHandle;
        
        if(!freeSlots.empty())
        {
            result = freeSlots.back();
            freeSlots.pop_back();
        }
        else
        {
            rgAssert(resources.size() < UINT32_MAX);
            result = (rgU32)resources.size();
            resources.resize(result + 1); // push_back(nullptr) ?
        }
        
        rgAssert(result != kInvalidHandle);
        return result;
    }
    
    void releaseHandle(rgU32 slot)
    {
        rgAssert(slot < resources.size());

        resources[slot] = nullptr;
        freeSlots.push_back(slot);
    }
    
    void setResourcePtrInSlot(rgU32 slot, Type* ptr)
    {
        resources[slot] = ptr;
    }
    
    typename ResourceList::iterator begin() EA_NOEXCEPT
    {
        return resources.begin();
    }
    
    typename ResourceList::iterator end() EA_NOEXCEPT
    {
        return resources.end();
    }
    
    Type* getPtr(rgU32 slot)
    {
        rgAssert(slot < resources.size());
        
        Type* ptr = resources[slot];
        rgAssert(ptr != nullptr);
        return ptr;
    }
    
    // TODO: eastl::vector<HandleType> getFreeHandles(int count);
};

template<typename Type>
struct GfxObjectRegistry
{
    typedef eastl::hash_map<rgHash, Type*> ObjectMap;
    ObjectMap objects;
    ObjectMap objectsToRemove;

    Type* find(rgHash hash)
    {
       ObjectMap::iterator itr = objects.find(hash);
       return (itr != objects.end()) ? itr->second : nullptr;
    }

    void insert(rgHash hash, Type* ptr)
    {
#if defined(ENABLE_GFX_OBJECT_INVALID_TAG_OP_ASSERT)
        ObjectMap::iterator itr = objects.find(hash);
        rgAssert(itr == objects.end());
#endif
        //objects.insert_or_assign(eastl::make_pair(hash, ptr));
        objects.insert_or_assign(hash, ptr);
    }

    void markForRemove(rgHash hash)
    {
#if defined(ENABLE_GFX_OBJECT_INVALID_TAG_OP_ASSERT)
        ObjectMap::iterator itr = objects.find(hash);
        rgAssert(itr != objects.end());
#endif
        //objectsToRemove.insert_or_assign(eastl::make_pair(hash, *itr))
        objectsToRemove.insert_or_assign(hash, itr->second);
    }

    void removeMarkedObjects()
    {
        for(auto itr : objectsToRemove)
        {

        }
    }
};

//template <typename Type>
//struct BindlessResourceManager
//{
//    typedef eastl::vector<Type*> ResourceList;
//    ResourceList resources;
//
//};

struct GfxCtx
{
    GfxCtx() {}
    ~GfxCtx() {}

    SDL_Window* mainWindow;
    rgUInt frameNumber;
    
    RenderCmdList* graphicCmdLists[RG_MAX_FRAMES_IN_FLIGHT];
    
    //GfxResourceManager<GfxTexture2D, HGfxTexture2D> texture2dManager;
    //GfxResourceManager<GfxBuffer, HGfxBuffer> buffersManager;
    //GfxResourceManager<GfxGraphicsPSO, HGfxGraphicsPSO> graphicsPSOManager;

    GfxObjectRegistry<GfxRenderTarget> registryRenderTarget;
    GfxObjectRegistry<GfxTexture2D> registryTexture2D;
    GfxObjectRegistry<GfxBuffer> registryBuffer;
    GfxObjectRegistry<GfxGraphicsPSO> registryGraphicsPSO;
    
    GfxBindlessResourceManager<GfxTexture2D> bindlessManagerTexture2D;

    Matrix4 orthographicMatrix;
    Matrix4 viewMatrix;
    
    eastl::vector<GfxTexture2D*> debugTextureHandles; // test only
    
    GfxRenderTarget* renderTarget[RG_MAX_FRAMES_IN_FLIGHT];
    GfxRenderTarget* depthStencilBuffer;
    
    // RenderCmdTexturedQuads
    //HGfxBuffer rcTexturedQuadsVB;
    //HGfxBuffer rcTexturedQuadsInstParams;

#if defined(RG_D3D12_RNDR)
    struct D3d
    {
        ComPtr<ID3D12Device2> device;
        ComPtr<ID3D12CommandQueue> commandQueue;
        ComPtr<IDXGISwapChain4> dxgiSwapchain;
        ComPtr<IDXGIFactory4> dxgiFactory;

        ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
        rgUInt rtvDescriptorSize;

        ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;

        ComPtr<ID3D12CommandAllocator> commandAllocator[RG_MAX_FRAMES_IN_FLIGHT];
        ComPtr<ID3D12GraphicsCommandList> commandList;

        ComPtr<ID3D12Fence> frameFence;
        UINT64 frameFenceValues[RG_MAX_FRAMES_IN_FLIGHT];
        HANDLE frameFenceEvent;

        /// test
        ComPtr<ID3D12RootSignature> dummyRootSignature;
        ComPtr<ID3D12PipelineState> dummyPSO;
        ComPtr<ID3D12Resource> triVB;
        D3D12_VERTEX_BUFFER_VIEW triVBView;
    } d3d;
#elif defined(RG_METAL_RNDR)
    struct Mtl
    {
        void* layer; // type: id<CAMetalLayer>
        NS::View* view;
        MTL::Device* device;
        MTL::CommandQueue* commandQueue;
        
        dispatch_semaphore_t framesInFlightSemaphore;

        void* renderEncoder; // type: id<MTLRenderCommandEncoder>
        void* commandBuffer; // type: id<MTLCommandBuffer>

        // arg buffers
        MTL::ArgumentEncoder* largeArrayTex2DArgEncoder;
        HGfxBuffer largeArrayTex2DArgBuffer;
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

        VmaAllocator vmaAllocator;

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
#elif defined(RG_OPENGL_RNDR)
    struct GL
    {
        SDL_GLContext context;
    } gl;
#endif
};
extern rg::GfxCtx* g_GfxCtx;

inline GfxCtx* gfxCtx() { return g_GfxCtx; }

//-----------------------------------------------------------------------------
// Gfx Render Command
//-----------------------------------------------------------------------------
enum RenderCmdType
{
    RenderCmdType_SetViewport,
    RenderCmdType_SetRenderPass,
    RenderCmdType_SetGraphicsPSO,
    RenderCmdType_DrawTexturedQuads,
    RenderCmdType_DrawTriangles,
};

struct RenderCmdHeader
{
    RenderCmdType type;
};

// NOTE: RenderCmds are not allowed to own any resources (smartptrs)
// Should this be allowed?? If yes, then addCmd should "new" the RenderCmd, and Flush should "delete"
// pros vs cons?

#define BEGIN_RENDERCMD_STRUCT(cmdName)     struct RenderCmd_##cmdName      \
                                            {                               \
                                                static const RenderCmdType type = RenderCmdType_##cmdName; \
                                                static CmdDispatchFnT* dispatchFn

#define END_RENDERCMD_STRUCT()              }

// ---===---

BEGIN_RENDERCMD_STRUCT(SetViewport);
    rgFloat4 viewport;
END_RENDERCMD_STRUCT();

// ---

BEGIN_RENDERCMD_STRUCT(SetRenderPass);
    GfxRenderPass renderPass;
END_RENDERCMD_STRUCT();

// ---

BEGIN_RENDERCMD_STRUCT(SetGraphicsPSO);
    GfxGraphicsPSO* pso;
END_RENDERCMD_STRUCT();

// ---

BEGIN_RENDERCMD_STRUCT(DrawTexturedQuads);
    GfxGraphicsPSO* pso;
    TexturedQuads* quads;
END_RENDERCMD_STRUCT();

// ---

BEGIN_RENDERCMD_STRUCT(DrawTriangles);
    GfxBuffer* vertexBuffer;
    rgU32 vertexBufferOffset;
    GfxBuffer* indexBuffer;
    rgU32 indexBufferOffset;

    rgU32 vertexCount;
    rgU32 indexCount;
    rgU32 instanceCount;
    rgU32 baseVertex;
    rgU32 baseInstance;
END_RENDERCMD_STRUCT();

// ---===---

#undef BEGIN_RENDERCMD_STRUCT
#undef END_RENDERCMD_STRUCT

void gfxHandleRenderCmd_SetViewport(void const* cmd);
void gfxHandleRenderCmd_SetRenderPass(void const* cmd);
void gfxHandleRenderCmd_SetGraphicsPSO(void const* cmd);
void gfxHandleRenderCmd_DrawTexturedQuads(void const* cmd);
void gfxHandleRenderCmd_DrawTriangles(void const* cmd);

RG_END_NAMESPACE

#endif
