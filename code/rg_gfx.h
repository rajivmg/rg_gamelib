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
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#define RG_MAX_FRAMES_IN_FLIGHT 3

#define RG_BEGIN_GFX_NAMESPACE namespace gfx {
#define RG_END_GFX_NAMESPACE }

RG_BEGIN_RG_NAMESPACE

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

// Note:
// GfxBuffer will always be for static data or shader writable data,
// cpu updatable dynamic data can be stored in FrameAllocator
struct GfxBuffer
{
    rgChar       tag[32];
    rgU32           size;
    GfxBufferUsage usage;
#if defined(RG_D3D12_RNDR)
#elif defined(RG_METAL_RNDR)
    MTL::Buffer* mtlBuffer;
#elif defined(RG_VULKAN_RNDR)
    VkBuffer vkBuffers[RG_MAX_FRAMES_IN_FLIGHT];
    VmaAllocation vmaAlloc;
#endif

    static void fillStruct(void* buf, rgU32 size, GfxBufferUsage usage, GfxBuffer* obj)
    {
        obj->size = size;
        obj->usage = usage;
    }
    
    static void create(const char* tag, void* buf, rgU32 size, GfxBufferUsage usage, GfxBuffer* obj);
    static void destroy(GfxBuffer* obj);
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
#endif

    static void fillStruct(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, rgBool genMips, GfxTextureUsage usage, GfxTexture2D* obj)
    {
        obj->width = width;
        obj->height = height;
        obj->format = format;
        obj->mipCount = genMips ? 8 : 1;
        obj->usage = usage;
    }

    static void create(char const* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, rgBool genMips, GfxTextureUsage usage, GfxTexture2D* obj);
    //https://www.ibm.com/docs/en/zos/2.1.0?topic=only-variadic-templates-c11
    static void destroy(GfxTexture2D* obj);
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

struct GfxSampler
{
    rgChar tag[32];
    GfxSamplerAddressMode rstAddressMode;
    GfxSamplerMinMagFilter  minFilter;
    GfxSamplerMinMagFilter  magFilter;
    GfxSamplerMipFilter     mipFilter;
    rgBool                 anisotropy;
    
#if defined(RG_METAL_RNDR)
    void* mtlSampler;
#else
#endif

    static void fillStruct(GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSampler* obj)
    {
        obj->rstAddressMode = rstAddressMode;
        obj->minFilter = minFilter;
        obj->magFilter = magFilter;
        obj->mipFilter = mipFilter;
        obj->anisotropy = anisotropy;
    }

    static void create(const char* tag, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSampler* obj);
    static void destroy(GfxSampler* obj);
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
    GfxTriangleFillMode_Lines,
};

enum GfxStage
{
    GfxStage_VS = 0x1,
    GfxStage_FS = 0x2,
    GfxStage_CS = 0x4,
};

enum GfxVertexStepFunc
{
    GfxVertexStepFunc_PerVertex,
    GfxVertexStepFunc_PerInstance,
};

struct GfxVertexInputDesc
{
    struct 
    {
        const char*   semanticName;
        rgUInt       semanticIndex;
        TinyImageFormat     format;
        rgUInt         bufferIndex;
        rgUInt              offset;
        GfxVertexStepFunc stepFunc;
    } elements[8];
    rgInt elementCount;
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
    char const* shaderSrc;
    char const* vsEntrypoint;
    char const* fsEntrypoint;
    char const* csEntrypoint;
    char const* defines;
};

struct GfxShaderLibrary // TODO: remove
{
#if defined(RG_D3D12_RNDR)
    ComPtr<ID3DBlob> d3dShaderBlob;
#elif defined(RG_METAL_RNDR)
    void* mtlLibData; // type: dispatch_data_t
#endif
};

struct GfxGraphicsPSO
{
    rgChar tag[32];
    GfxCullMode cullMode;
    GfxWinding winding;
    GfxTriangleFillMode triangleFillMode;
#if defined(RG_D3D12_RNDR)
    struct ResourceInfo // TODO: --> ?? GfxResourceBindingInfo with per api info
    {
        D3D_SHADER_INPUT_TYPE type;
        rgU16 offsetInDescTable;
    };
    eastl::hash_map<eastl::string, ResourceInfo> d3dResourceInfo; // TODO: reserve a big enough space
    ComPtr<ID3D12RootSignature> d3dRootSignature;
    ComPtr<ID3D12PipelineState> d3dPSO;
#elif defined(RG_METAL_RNDR)
    struct ResourceInfo
    {
        enum Type
        {
            Type_ConstantBuffer,
            Type_Texture2D,
            Type_Sampler,
        };
        Type type;
        rgU16 mslBinding;
        GfxStage stage;
    };
    eastl::hash_map<eastl::string, ResourceInfo> mtlResourceInfo;
    void* mtlPSO; // type: id<MTLRenderPipelineState>
    void* mtlDepthStencilState; // type: id<MTLDepthStencilState>
#elif defined(RG_VULKAN_RNDR)
#endif

    static void fillStruct(GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO* obj)
    {
        obj->cullMode = renderStateDesc->cullMode;
        obj->winding = renderStateDesc->winding;
        obj->triangleFillMode = renderStateDesc->triangleFillMode;
    }

    static void create(const char* tag, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO* obj);
    static void destroy(GfxGraphicsPSO* obj);
};

//-----------------------------------------------------------------------------
// Gfx Helper Classes
//-----------------------------------------------------------------------------

namespace gfx {
void setterBindlessResource(rgU32 slot, GfxTexture2D* ptr);
void checkerWaitTillFrameCompleted(rgInt frameIndex);
GfxGraphicsPSO* findGraphicsPSO(char const* tag);
rgInt getFrameIndex();
}

template<typename Type>
class GfxBindlessResourceManager
{
    typedef eastl::vector<Type*> ResourceList;
    ResourceList resources; // Hold reference to the resources until no longer needed

    typedef eastl::vector<rgU32> SlotList;
    SlotList freeSlots; // Indexes in referenceList which are unused

    rgU32 getFreeSlot()
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

    void releaseSlot(rgU32 slot)
    {
        rgAssert(slot < resources.size());

        resources[slot] = nullptr;
        freeSlots.push_back(slot);
    }

    void setResourcePtrInSlot(rgU32 slot, Type* ptr)
    {
        resources[slot] = ptr;
        gfx::setterBindlessResource(slot, ptr);
        // TODO: make sure we're not updating a resource in flight
        // this should be implictly handled from _releaseSlot
    }

    rgU32 getSlotOfResourcePtr(Type* ptr)
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

public:
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
        rgU32 slot = getSlotOfResourcePtr(ptr);
        if(slot == kInvalidValue)
        {
            slot = getFreeSlot();
            setResourcePtrInSlot(slot, ptr);
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

// Gfx Object Registry
// -------------------

template<typename Type>
struct GfxObjectRegistry
{
    // TODO: Is it better to directly use the string as key
    // doing that we will lose the ability to use pre-computed
    // hash.
    typedef eastl::hash_map<rgHash, Type*> ObjectMap;
    ObjectMap objects;
    eastl::vector<Type*> objectsToDestroy[RG_MAX_FRAMES_IN_FLIGHT];
    
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
        objects.insert_or_assign(hash, ptr);
    }

    void markForDestroy(rgHash hash)
    {
        typename ObjectMap::iterator itr = objects.find(hash);
        rgAssert(itr != objects.end());
        objectsToDestroy[gfx::getFrameIndex()].push_back(itr->second);
    }

    void destroyMarkedObjects()
    {
        for(auto itr : objectsToDestroy[g_FrameIndex])
        {
            // TODO: Which one is better here
            Type::destroy(itr);
            //Type::destroy(&(*itr));
            rgDelete(&(*itr));
        }
        objectsToDestroy[g_FrameIndex].clear();
    }

    template<typename... Args>
    Type* create(const char* tag, Args... args)
    {
        rgAssert(tag != nullptr);

        Type* obj = rgNew(Type);
        strncpy(obj->tag, tag, rgARRAY_COUNT(Type::tag));
        Type::fillStruct(args..., obj);
        Type::create(tag, args..., obj);
        insert(rgCRC32(tag), obj);
        return obj;
    }

    template <typename... Args>
    Type* findOrCreate(const char* tag, Args... args)
    {
        rgAssert(tag != nullptr);
        Type* obj = find(rgCRC32(tag));
        obj = (obj == nullptr) ? create(tag, args...) : obj;
        return obj;
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
    rgU32 texID;
    rgFloat3 pos;
    rgFloat4 offsetOrientation;
    rgFloat2 size;
};
typedef eastl::vector<TexturedQuad> TexturedQuads;

void pushTexturedQuad(TexturedQuads* quadList, QuadUV uv, rgFloat4 posSize, rgFloat4 offsetOrientation, GfxTexture2D* tex);

//-----------------------------------------------------------------------------
// Gfx Commands
//-----------------------------------------------------------------------------

struct GfxRenderCmdEncoder
{
    void begin(char const* tag, GfxRenderPass* renderPass);
    void end();

    void pushDebugTag(const char* tag);
    void popDebugTag();
    
    void setViewport(rgFloat4 viewport);
    void setViewport(rgFloat originX, rgFloat originY, rgFloat width, rgFloat height);

    void setScissor(); // TODO
    
    void setGraphicsPSO(GfxGraphicsPSO* pso);
    void setGraphicsPSO(char const* tag)
    {
        GfxGraphicsPSO* ptr = gfx::findGraphicsPSO(tag);
        rgAssert(ptr != NULL);
        setGraphicsPSO(ptr);
    }
    
    void setBuffer(GfxBuffer* buffer, rgU32 offset, char const* bindingTag);
    void setBuffer(char const* bufferTag, rgU32 offset, char const* bindingTag);
    void setTexture2D(GfxTexture2D* texture, char const* bindingTag);
    void setTexture2D(char const* textureTag, char const* bindingTag);
    void setSampler(GfxSampler* sampler, char const* bindingTag);
    void setSampler(char const* samplerTag, char const* bindingTag);
    
    void drawTexturedQuads(TexturedQuads* quads);
    void drawBunny();

    rgBool hasEnded;
#if defined(RG_METAL_RNDR)
    void* renderCmdEncoder; // type: id<MTLRenderCommandEncoder>
#endif
};

struct GfxBlitCmdEncoder
{
    // TODO: Add a deferred cmds, they run just before the end of the current frame 
    // TODO: Add a deferred cmds, they run just before the end of the current frame 
    // TODO: Add a deferred cmds, they run just before the end of the current frame 

    struct Cmd
    {
        enum Type
        {
            Type_UploadTexture,
            Type_GenMips,
            Type_CopyBufferResource,
        };

        Type type;
        union
        {
            struct
            {
                GfxTexture2D* tex;
                rgU32 uploadBufferOffset;
            } uploadTexture;
            struct
            {
                GfxTexture2D* tex;
            } genMips;
        };
    };

    void begin();
    void end();
    void pushDebugTag(const char* tag);
    void genMips(GfxTexture2D* srcTexture);
    void copyTexture(GfxTexture2D* srcTexture, GfxTexture2D* dstTexture, rgU32 srcMipLevel, rgU32 dstMipLevel, rgU32 mipLevelCount);

    rgBool hasEnded;
    eastl::vector<Cmd> cmds;

#if defined(RG_METAL_RNDR)
    void* mtlBlitCommandEncoder; // type: id<MTLBlitCommandEncoder>
#elif defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource> d3dUploadBuffer;
#endif
};


//-----------------------------------------------------------------------------
// STUFF BELOW OPERATE ON THE CONTENT OF GFX'CTX' DATA, IF IT DOESN'T REQUIRE
// ACCESS TO GFXCTX CONTENTS, DO NOT PUT IT BELOW THIS LINE.
//-----------------------------------------------------------------------------

RG_BEGIN_GFX_NAMESPACE

#ifdef RG_VULKAN_RNDR
#define rgVK_CHECK(x) do { VkResult errCode = x; if(errCode) { rgLog("%s errCode:%d(0x%x)", #x, errCode, errCode); SDL_assert(!"Vulkan API call failed"); } } while(0)
#endif

//-----------------------------------------------------------------------------
// General Common Stuff
//-----------------------------------------------------------------------------
rgInt           preInit();
rgInt           initCommonStuff(); // TODO: merge with preInit()
void            atFrameStart();
rgInt           getFrameIndex(); // Returns 0 if g_FrameIndex is -1
rgInt           getFinishedFrameIndex();
GfxTexture2D*   getCurrentRenderTargetColorBuffer();
GfxTexture2D*   getRenderTargetDepthBuffer();
Matrix4         createPerspectiveProjectionMatrix(rgFloat focalLength, rgFloat aspectRatio, rgFloat nearPlane, rgFloat farPlane);

//-----------------------------------------------------------------------------
// Gfx function declarations
//-----------------------------------------------------------------------------
rgInt           init();
void            destroy();
void            startNextFrame();
void            endFrame();

void            onSizeChanged();

GfxRenderCmdEncoder* setRenderPass(GfxRenderPass* renderPass, char const* tag);
GfxBlitCmdEncoder* setBlitPass(char const* tag);

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

DeclareGfxObjectFunctions(Buffer, void* buf, rgU32 size, GfxBufferUsage usage);

GfxTexture2D* createTexture2D(char const* tag, TextureRef texture, rgBool genMips, GfxTextureUsage usage);
void setterBindlessResource(rgU32 slot, GfxTexture2D* ptr);
DeclareGfxObjectFunctions(Texture2D, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, rgBool genMips, GfxTextureUsage usage);

DeclareGfxObjectFunctions(GraphicsPSO, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc);

DeclareGfxObjectFunctions(Sampler, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy);

//-----------------------------------------------------------------------------
// Gfx Vertex Format
//-----------------------------------------------------------------------------

struct SimpleVertexFormat
{
    rgFloat pos[3];
    rgFloat texcoord[2];
    rgFloat color[4];
};

struct SimpleInstanceParams
{
    rgU32 texParam[1024][4];
};

void genTexturedQuadVertices(TexturedQuads* quadList, eastl::vector<SimpleVertexFormat>* vertices, SimpleInstanceParams* instanceParams);


//-----------------------------------------------------------------------------
// Resource Binding
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Graphic Context Data
//-----------------------------------------------------------------------------
extern SDL_Window* mainWindow;
extern rgUInt frameNumber;

extern GfxRenderCmdEncoder* currentRenderCmdEncoder;
extern GfxBlitCmdEncoder* currentBlitCmdEncoder;

extern GfxObjectRegistry<GfxTexture2D>* registryTexture2D;
extern GfxObjectRegistry<GfxBuffer>* registryBuffer;
extern GfxObjectRegistry<GfxGraphicsPSO>* registryGraphicsPSO;
extern GfxObjectRegistry<GfxSampler>* registrySampler;
//extern GfxObjectRegistry<GfxShaderLibrary, gfx::destroyerGfxShaderLibrary>* registryShaderLibrary;
    
extern GfxBindlessResourceManager<GfxTexture2D>* bindlessManagerTexture2D;

extern Matrix4 orthographicMatrix; // TODO: Rename projection2d, projection3d
extern Matrix4 viewMatrix; // TODO: Rename view2d
    
extern eastl::vector<GfxTexture2D*> debugTextureHandles; // test only
    
extern GfxTexture2D* renderTarget[RG_MAX_FRAMES_IN_FLIGHT];
extern GfxTexture2D* depthStencilBuffer;

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

RG_END_GFX_NAMESPACE
RG_END_RG_NAMESPACE

#endif
