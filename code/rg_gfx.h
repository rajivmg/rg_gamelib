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
#include <EASTL/shared_ptr.h>
#include <EASTL/hash_map.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#define RG_MAX_FRAMES_IN_FLIGHT 3

#define RG_GFX_BEGIN_NAMESPACE namespace gfx {
#define RG_GFX_END_NAMESPACE }

RG_BEGIN_NAMESPACE

static const rgU32 kInvalidValue = ~(0x0);
static const rgU32 kUninitializedValue = 0;
static const rgU32 kMaxColorAttachments = 4;

//-----------------------------------------------------------------------------
// Game functions
//-----------------------------------------------------------------------------
rgInt setup();
rgInt updateAndDraw(rgDouble dt);

//-----------------------------------------------------------------------------
// Gfx Objects Types
//-----------------------------------------------------------------------------

// Buffer type
// -------------------

enum GfxBufferUsage
{
    GfxBufferUsage_ShaderRW = (0 << 0),
    GfxBufferUsage_VertexBuffer = (1 << 0),
    GfxBufferUsage_IndexBuffer = (1 << 1),
    GfxBufferUsage_ConstantBuffer = (1 << 2),
    GfxBufferUsage_StructuredBuffer = (1 << 3),
    GfxBufferUsage_CopySrc = (1 << 4),
    GfxBufferUsage_CopyDst = (1 << 5),
};

enum GfxMemoryType
{
    GfxMemoryType_CPUToGPU, // UploadHeap
    GfxMemoryType_GPUToCPU, // ReadbackHeap
    GfxMemoryType_GPUOnly,  // DefaultHeap
    //GfxMemoryUsage_None     =   (0 << 0),
    //GfxMemoryUsage_GPURead  =   (1 << 0),
    //GfxMemoryUsage_GPUWrite =   (1 << 1),
    //GfxMemoryUsage_GPUReadWrite = (GfxMemoryUsage_GPURead | GfxMemoryUsage_GPUWrite),
    //GfxMemoryUsage_CPURead  =   (1 << 2),
    //GfxMemoryUsage_CPUWrite =   (1 << 3),
    //GfxMemoryUsage_CPUReadWrite = (GfxMemoryUsage_CPURead | GfxMemoryUsage_CPUWrite),
};

struct GfxBuffer
{
    rgChar       tag[32];
    rgU32           size;
    GfxBufferUsage usage;
    rgBool       dynamic;
    rgInt     activeSlot;
#if defined(RG_D3D12_RNDR)
#elif defined(RG_METAL_RNDR)
    MTL::Buffer* mtlBuffers[RG_MAX_FRAMES_IN_FLIGHT];
#elif defined(RG_VULKAN_RNDR)
    VkBuffer vkBuffers[RG_MAX_FRAMES_IN_FLIGHT];
    VmaAllocation vmaAlloc;
#endif
};

// Texture type
// -------------------

enum GfxTextureUsage
{
    GfxTextureUsage_ShaderRead = (0 << 0),
    GfxTextureUsage_ShaderReadWrite = (1 << 0),
    GfxTextureUsage_RenderTarget = (1 << 1),
    GfxTextureUsage_DepthStencil = (1 << 2),
    GfxTextureUsage_CopyDst = (1 << 3),
    GfxTextureUsage_CopySrc = (1 << 4),
};

enum GfxTextureDim
{
    GfxTextureDim_2D,
    GfxTextureDim_1D,
    GfxTextureDim_3D,
    GfxTextureDim_Buffer,
};

struct GfxTexture2D
{
    rgChar          tag[32];
    rgUInt            width;
    rgUInt           height;
    TinyImageFormat  format;
    rgUInt         mipCount;
    GfxTextureUsage   usage;
    rgU32             texID;

#if defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource> d3dTexture;
    CD3DX12_CPU_DESCRIPTOR_HANDLE d3dTextureView;
#elif defined(RG_METAL_RNDR)
    MTL::Texture* mtlTexture;
#elif defined(RG_VULKAN_RNDR)
    VkImage vkTexture;
    VmaAllocation vmaAlloc;
#elif defined(RG_OPENGL_RNDR)
    GLuint glTexture;
#endif
};

// Sampler
// ----------

enum GfxSamplerAddressMode
{
    GfxSamplerAddressMode_Repeat,
    GfxSamplerAddressMode_ClampToEdge,
    GfxSamplerAddressMode_ClampToZero,
    GfxSamplerAddressMode_ClampToBorderColor,
};

enum GfxSamplerMinMagFilter
{
    GfxSamplerMinMagFilter_Nearest,
    GfxSamplerMinMagFilter_Linear,
};

enum GfxSamplerMipFilter
{
    GfxSamplerMipFilter_NotMipped,
    GfxSamplerMipFilter_Nearest,
    GfxSamplerMipFilter_Linear,
};

struct GfxSamplerState
{
    rgChar tag[32];
    GfxSamplerAddressMode rstAddressMode;
    GfxSamplerMinMagFilter  minFilter;
    GfxSamplerMinMagFilter  magFilter;
    GfxSamplerMipFilter     mipFilter;
    rgBool                 anisotropy;
    
#if defined(RG_METAL_RNDR)
    void* mtlSamplerState;
#else
#endif
};

// RenderPass
// ------------

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

struct GfxColorAttachmentDesc
{
    GfxTexture2D*      texture;
    GfxLoadAction   loadAction;
    GfxStoreAction storeAction;
    rgFloat4        clearColor;
};

struct GfxRenderPass
{
    GfxColorAttachmentDesc colorAttachments[kMaxColorAttachments];

    GfxTexture2D* depthStencilAttachmentTexture;
    GfxLoadAction depthStencilAttachmentLoadAction;
    GfxStoreAction depthStencilAttachmentStoreAction;
    rgFloat  clearDepth;
    rgU32    clearStencil;
};

// PSO and State types
// -------------------

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

enum GfxStage
{
    GfxStage_VS,
    GfxStage_FS,
    GfxStage_CS,
};

struct GfxVertexInputDesc
{
    struct 
    {
        const char* semanticName;
        rgUInt     semanticIndex;
        TinyImageFormat   format;
        rgUInt              slot;
        rgUInt            offset;
    } elements[8];
    rgInt elementsCount;
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

    // Viewport <--- not part of static state
    // ScissorRect <--- not part of static state
    // Primitive Type, Line, LineStrip, Triangle, TriangleStrip
    // Index Type, U16, U32
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
    rgChar tag[32];
    // TODO: No vertex attrib, only index attrib. Shader fetch vertex data from buffers directly.
    GfxRenderStateDesc renderState;
#if defined(RG_D3D12_RNDR)
    ComPtr<ID3D12PipelineState> d3dPSO;
#elif defined(RG_METAL_RNDR)
    void* mtlPSO; // type: id<MTLRenderPipelineState>
#elif defined(RG_VULKAN_RNDR)
#endif
};

// Shader Arguments
// -----------------

enum GfxUpdateFreq
{
    GfxUpdateFreq_Frame,
    GfxUpdateFreq_Pass,
    GfxUpdateFreq_Draw,
    GfxUpdateFreq_COUNT,
};

enum GfxShaderArgType
{
    GfxShaderArgType_ConstantBuffer,
    GfxShaderArgType_ROTexture,
    GfxShaderArgType_ROBuffer,
    GfxShaderArgType_RWTexture,
    GfxShaderArgType_RWBuffer,
    GfxShaderArgType_SamplerState,
    GfxShaderArgType_COUNT,
};

//-----------------------------------------------------------------------------
// Gfx Helper Classes
//-----------------------------------------------------------------------------

template <typename Type>
struct GfxBindlessResourceManager
{
    typedef eastl::vector<Type*> ResourceList;
    ResourceList resources; // Hold reference to the resources until no longer needed

    typedef eastl::vector<rgU32> SlotList;
    SlotList freeSlots; // Indexes in referenceList which are unused

    rgU32 _getFreeSlot()
    {
        rgU32 result = kInvalidValue;

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

        rgAssert(result != kInvalidValue);
        return result;
    }

    void _releaseSlot(rgU32 slot)
    {
        rgAssert(slot < resources.size());

        resources[slot] = nullptr;
        freeSlots.push_back(slot);
    }

    void _setResourcePtrInSlot(rgU32 slot, Type* ptr)
    {
        resources[slot] = ptr;
    }

    rgU32 _getSlotOfResourcePtr(Type* ptr)
    {
        for(rgUInt i = 0, size = (rgUInt)resources.size(); i < size; ++i)
        {
            if(resources[i] == ptr)
            {
                return i;
            }
        }
        return kInvalidValue;
    }

    typename ResourceList::iterator begin() EA_NOEXCEPT
    {
        return resources.begin();
    }

    typename ResourceList::iterator end() EA_NOEXCEPT
    {
        return resources.end();
    }

    rgU32 getBindlessIndex(Type* ptr)
    {
        rgU32 slot = _getSlotOfResourcePtr(ptr);
        if(slot == kInvalidValue)
        {
            slot = _getFreeSlot();
            _setResourcePtrInSlot(slot, ptr);
        }
        return slot;
    }

    Type* getPtr(rgU32 slot)
    {
        rgAssert(slot < resources.size());

        Type* ptr = resources[slot];
        return ptr;
    }

    // TODO: rgU32 getContiguousFreeSlots(int count);
};

template<typename Type>
struct GfxObjectRegistry
{
    typedef eastl::hash_map<rgHash, Type*> ObjectMap;
    ObjectMap objects;
    ObjectMap objectsToRemove;

    Type* find(rgHash hash)
    {
        typename ObjectMap::iterator itr = objects.find(hash);
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
        typename ObjectMap::iterator itr = objects.find(hash);
        rgAssert(itr != objects.end());
        //objectsToRemove.insert_or_assign(eastl::make_pair(hash, *itr))
        objectsToRemove.insert_or_assign(hash, itr->second);
    }

    void removeMarkedObjects()
    {
        for(auto itr : objectsToRemove)
        {

        }
    }
    
    typename ObjectMap::iterator begin() EA_NOEXCEPT
    {
        return objects.begin();
    }

    typename ObjectMap::iterator end() EA_NOEXCEPT
    {
        return objects.end();
    }
};

//-----------------------------------------------------------------------------
// Texture & Utils
//-----------------------------------------------------------------------------

// Texture type
// -------------
struct Texture
{
    rgHash  hash;
    rgU8*   buf;
    rgUInt  width;
    rgUInt  height;
    TinyImageFormat format;

    // -- remove in final build
    rgChar name[32]; // TODO: rename tag
};
typedef eastl::shared_ptr<Texture> TextureRef;

TextureRef  loadTexture(char const* filename);
void        unloadTexture(Texture* t);

// Texture UV helper
// -----------------
struct QuadUV
{
    // TODO: use Vector4 here?
    rgFloat uvTopLeft[2];
    rgFloat uvBottomRight[2];
};
 
extern QuadUV defaultQuadUV;

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx);
QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, Texture* refTexture);

// Textured quad
// ---------------
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
// TODO: remove this
template <rgSize N>
using InplaceTexturedQuads = eastl::fixed_vector<TexturedQuad, N>;

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
// Gfx Commands
//-----------------------------------------------------------------------------

struct GfxRenderCmdEncoder
{
    void begin(GfxRenderPass* renderPass);
    void end();

    void pushDebugTag(const char* tag);
    void setViewport(rgFloat4 viewport);
    void setViewport(rgFloat originX, rgFloat originY, rgFloat width, rgFloat height);
    void setGraphicsPSO(GfxGraphicsPSO* pso);
    void drawTexturedQuads(TexturedQuads* quads);

#if defined(RG_METAL_RNDR)
    void* renderCmdEncoder; // type: id<MTLRenderCommandEncoder>
    rgBool hasEnded;
#endif
};

struct GfxBlitCmdList
{
    void genMips(GfxTexture2D* obj);
};


//-----------------------------------------------------------------------------
// STUFF BELOW OPERATE ON THE CONTENT OF GFX'CTX' DATA, IF IT DOESN'T REQUIRE
// ACCESS TO GFXCTX CONTENTS, DO NOT PUT IT BELOW THIS LINE.
//-----------------------------------------------------------------------------

RG_GFX_BEGIN_NAMESPACE

#ifdef RG_VULKAN_RNDR
#define rgVK_CHECK(x) do { VkResult errCode = x; if(errCode) { rgLog("%s errCode:%d(0x%x)", #x, errCode, errCode); SDL_assert(!"Vulkan API call failed"); } } while(0)
#endif

//-----------------------------------------------------------------------------
// General Common Stuff
//-----------------------------------------------------------------------------
rgInt           initCommonStuff();
void            atFrameStart();

//-----------------------------------------------------------------------------
// Gfx function declarations
//-----------------------------------------------------------------------------

rgInt           preInit();
rgInt           init();
void            destroy();
void            startNextFrame();
void            endFrame();

void            onSizeChanged();

GfxRenderCmdEncoder* setRenderPass(GfxRenderPass* renderPass, char const* tag);

// Helper macros
// ---------------
#define ENABLE_GFX_OBJECT_INVALID_TAG_OP_ASSERT

#define DeclareGfxObjectFunctions(type, ...) Gfx##type* create##type(const char* tag, __VA_ARGS__); \
        Gfx##type* findOrCreate##type(const char* tag, __VA_ARGS__); \
        Gfx##type* find##type(rgHash tagHash);  \
        Gfx##type* find##type(char const* tag); \
        void destroy##type(rgHash tagHash); \
        void destroy##type(char const* tag); \
         void allocAndFill##type##Struct(const char* tag, __VA_ARGS__, Gfx##type** obj); \
        void dealloc##type##Struct(Gfx##type* obj); \
        void creatorGfx##type(char const* tag, __VA_ARGS__, Gfx##type* obj); \
        void destroyerGfx##type(Gfx##type* obj)

void updateBuffer(rgHash tagHash, void* buf, rgU32 size, rgU32 offset);
void updateBuffer(char const* tag, void* buf, rgU32 size, rgU32 offset);
DeclareGfxObjectFunctions(Buffer, void* buf, rgU32 size, GfxBufferUsage usage, rgBool dynamic);
void updaterGfxBuffer(void* buf, rgU32 size, rgU32 offset, GfxBuffer* obj);

GfxTexture2D* createTexture2D(char const* tag, TextureRef texture, rgBool genMips, GfxTextureUsage usage);
DeclareGfxObjectFunctions(Texture2D, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, rgBool genMips, GfxTextureUsage usage);

DeclareGfxObjectFunctions(GraphicsPSO, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc);

DeclareGfxObjectFunctions(SamplerState, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy);


//-----------------------------------------------------------------------------
// Gfx Vertex Format
//-----------------------------------------------------------------------------
struct ImmVertexFormat_ // TODO: remove
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
// Resource Binding
//-----------------------------------------------------------------------------

struct ResourceBindingInfo
{
#if defined(RG_METAL_RNDR)
    void* frameArgEncoder;
    void* passArgEncoder;
    void* drawArgEncoder;
    
    void* commonArgBuffer;
#endif
};

void setConstBuffer(GfxUpdateFreq updateFreq, rgU32 bindpoint, GfxBuffer* buffer);
void setROBuffer(GfxUpdateFreq updateFreq, rgU32 bindpoint, GfxBuffer* buffer);
void setRWBuffer(GfxUpdateFreq updateFreq, rgU32 bindpoint, GfxBuffer* buffer);
void setROTexture(GfxUpdateFreq updateFreq, rgU32 bindpoint, GfxTexture2D* texture);
void setRWTexture(GfxUpdateFreq updateFreq, rgU32 bindpoint, GfxTexture2D* texture);
void setSamplerState(rgU32 bindpoint, GfxSamplerState* samplerState);

//-----------------------------------------------------------------------------
// Graphic Context Data
//-----------------------------------------------------------------------------
extern SDL_Window* mainWindow;
extern rgUInt frameNumber;

extern GfxRenderCmdEncoder* currentRenderCmdEncoder;

extern GfxObjectRegistry<GfxTexture2D>* registryTexture2D;
extern GfxObjectRegistry<GfxBuffer>* registryBuffer;
extern GfxObjectRegistry<GfxGraphicsPSO>* registryGraphicsPSO;
extern GfxObjectRegistry<GfxSamplerState>* registrySamplerState;
    
extern GfxBindlessResourceManager<GfxTexture2D>* bindlessManagerTexture2D;

extern rgU32 shaderArgsLayout[GfxShaderArgType_COUNT][GfxUpdateFreq_COUNT];

extern Matrix4 orthographicMatrix;
extern Matrix4 viewMatrix;
    
extern eastl::vector<GfxTexture2D*> debugTextureHandles; // test only
    
extern GfxTexture2D* renderTarget[RG_MAX_FRAMES_IN_FLIGHT];
extern GfxTexture2D* depthStencilBuffer;
    
// RenderCmdTexturedQuads
extern GfxBuffer* rcTexturedQuadsVB;
extern GfxBuffer* rcTexturedQuadsInstParams;

//-----------------------------------------------------------------------------
// API Specific Graphic Context Data
//-----------------------------------------------------------------------------

// DX12
// -----------
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

    ComPtr<ID3D12DescriptorHeap> cbvSrvUavDescriptorHeap;
    rgUInt cbvSrvUavDescriptorSize;

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
};
#elif defined(RG_METAL_RNDR)
struct Mtl
{
    void* layer; // type: id<CAMetalLayer>
    NS::View* view;
    MTL::Device* device;
    MTL::CommandQueue* commandQueue;
    void* caMetalDrawable; // type: id<CAMetalDrawable>

    dispatch_semaphore_t framesInFlightSemaphore;
    NS::AutoreleasePool* autoReleasePool;
    
    void* renderEncoder; // type: id<MTLRenderCommandEncoder>
    void* commandBuffer; // type: id<MTLCommandBuffer>

    // arg buffers
    MTL::ArgumentEncoder* largeArrayTex2DArgEncoder;
    GfxBuffer* largeArrayTex2DArgBuffer;
    void* argumentEncoder[GfxUpdateFreq_COUNT]; // type: id<MTLArgumentEncoder>
};
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

RG_GFX_END_NAMESPACE
RG_END_NAMESPACE

#endif
