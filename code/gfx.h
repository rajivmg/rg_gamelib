#ifndef __GFX_H__
#define __GFX_H__

// NOTES:

// Swapchain texture format is sRGB, and pixels written/copied to it
// will be converter to sRGB. Therefore it expects pixel value to be
// in linear space.

// Follows left-handed coordinate system
// Clip coordinates are left-handed
// Cubemaps are left-handed

#if defined(RG_D3D12_RNDR)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <wrl.h>
    #include <d3d12.h>
    #include <d3dcompiler.h>
    #include <d3dx12.h>
    #include <dxgi1_6.h>
    using namespace Microsoft::WRL;
#elif defined(RG_VULKAN_RNDR)
    #include "volk/volk.h"
    #include "vk-bootstrap/VkBootstrap.h"
    #include "vk_mem_alloc.h"
#endif

#include "core.h"
#include "imgui.h"
#include <EASTL/shared_ptr.h>
#include <EASTL/hash_map.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_vector.h>

#define RG_MAX_FRAMES_IN_FLIGHT 3
#define RG_MAX_BINDLESS_TEXTURE_RESOURCES 100000
#define RG_MAX_COLOR_ATTACHMENTS 4
#define RG_GFX_OBJECT_TAG_LENGTH 32

#define ENABLE_GFX_OBJECT_INVALID_TAG_OP_ASSERT
#define ENABLE_SLOW_GFX_RESOURCE_VALIDATIONS

static const rgU32 kInvalidValue = ~(0x0);
static const rgU32 kUninitializedValue = 0;

// FORWARD DECLARATIONS
struct  GfxTexture;
struct  GfxFrameResource;
class   GfxFrameAllocator;
struct  TexturedQuad;

typedef eastl::vector<TexturedQuad> TexturedQuads;

rgInt   gfxGetFrameIndex();
void    gfxSetBindlessResource(rgU32 slot, GfxTexture* ptr);

// Gfx Object Registry
// -------------------
// NOTE: A base class for every Gfx resource type

template<typename Type>
struct GfxObjectRegistry
{
    // TODO: Is it better to directly use the string as key, but
    // doing that we will lose the ability to use pre-computed hash.
    
    typedef eastl::hash_map<rgHash, Type*> ObjectMapType;
    
    static ObjectMapType objects;
    static eastl::vector<Type*> objectsToDestroy[RG_MAX_FRAMES_IN_FLIGHT];
    
    static Type* find(rgHash hash)
    {
        typename ObjectMapType::iterator itr = objects.find(hash);
        return (itr != objects.end()) ? itr->second : nullptr;
    }

    template<typename... Args>
    static Type* create(const char* tag, Args... args)
    {
        rgAssert(tag != nullptr);

        Type* obj = rgNew(Type);
        strncpy(obj->tag, tag, rgArrayCount(Type::tag));
        Type::fillStruct(args..., obj);
        Type::createGfxObject(tag, args..., obj);
        insert(rgCRC32(tag), obj);
        return obj;
    }

    template <typename... Args>
    static Type* findOrCreate(const char* tag, Args... args)
    {
        rgAssert(tag != nullptr);
        Type* obj = find(rgCRC32(tag));
        obj = (obj == nullptr) ? create(tag, args...) : obj;
        return obj;
    }
    
    static void destroy(rgHash hash)
    {
        typename ObjectMapType::iterator itr = objects.find(hash);
        rgAssert(itr != objects.end());
        objectsToDestroy[gfxGetFrameIndex()].push_back(itr->second);
    }
    
    static typename ObjectMapType::iterator begin() EA_NOEXCEPT
    {
        return objects.begin();
    }

    static typename ObjectMapType::iterator end() EA_NOEXCEPT
    {
        return objects.end();
    }
    
//protected:
    static void insert(rgHash hash, Type* ptr)
    {
#if defined(ENABLE_GFX_OBJECT_INVALID_TAG_OP_ASSERT)
        typename ObjectMapType::iterator itr = objects.find(hash);
        rgAssert(itr == objects.end());
#endif
        objects.insert_or_assign(hash, ptr);
    }

    static void destroyMarkedObjects()
    {
        rgInt frameIndex = gfxGetFrameIndex();
        for(auto itr : objectsToDestroy[frameIndex])
        {
            // TODO: Which one is better here
            Type::destroyGfxObject(itr);
            //Type::destroy(&(*itr));
            rgDelete(&(*itr));
        }
        objectsToDestroy[frameIndex].clear();
    }
};

template<typename Type>
typename GfxObjectRegistry<Type>::ObjectMapType GfxObjectRegistry<Type>::objects;

template<typename Type>
eastl::vector<Type*> GfxObjectRegistry<Type>::objectsToDestroy[RG_MAX_FRAMES_IN_FLIGHT];


//-----------------------------------------------------------------------------
// GFX OBJECT TYPES
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

RG_DEFINE_ENUM_FLAGS_OPERATOR(GfxBufferUsage);

enum GfxMemoryType
{
    GfxMemoryType_Default  = 0, // DefaultHeap
    GfxMemoryType_Upload   = 1, // UploadHeap
    GfxMemoryType_Readback = 2, // ReadbackHeap
};

// Note:
// GfxBuffer will always be for static data or shader writable data,
// cpu updatable dynamic data can be stored in FrameAllocator
struct GfxBuffer : GfxObjectRegistry<GfxBuffer>
{
    rgChar          tag[RG_GFX_OBJECT_TAG_LENGTH];
    rgSize          size;
    GfxBufferUsage  usage; // TODO: This doesn't seem to be required in Metal & D3D12 backends.. remove?
    GfxMemoryType   memoryType;
#if defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource>  d3dResource;
    CD3DX12_RANGE           mappedRange;
#elif defined(RG_METAL_RNDR)
    void*           mtlBuffer; // type: id<MTLBuffer>
#elif defined(RG_VULKAN_RNDR)
    VkBuffer        vkBuffers[RG_MAX_FRAMES_IN_FLIGHT];
    VmaAllocation   vmaAlloc;
#endif

    static void fillStruct(GfxMemoryType memoryType, void* buf, rgSize size, GfxBufferUsage usage, GfxBuffer* obj)
    {
        obj->size = size;
        obj->usage = usage;
        obj->mappedMemory = nullptr;
        obj->memoryType = memoryType;
    }
    
    // TODO: should change to first size then buffer??
    static void createGfxObject(const char* tag, GfxMemoryType memoryType, void* buf, rgSize size, GfxBufferUsage usage, GfxBuffer* obj);
    static void destroyGfxObject(GfxBuffer* obj);
    
    void*   mappedMemory;
    void*   map(rgU32 rangeBeginOffset, rgU32 rangeSizeInBytes); // TODO: MTL D3D12 - handle ranges
    void    unmap();
};

// Texture types
// -------------------

struct ImageSlice;

enum GfxTextureUsage
{
    GfxTextureUsage_ShaderRead = (0 << 0),
    GfxTextureUsage_ShaderReadWrite = (1 << 0),
    GfxTextureUsage_RenderTarget = (1 << 1),
    GfxTextureUsage_DepthStencil = (1 << 2),
    GfxTextureUsage_CopyDst = (1 << 3),
    GfxTextureUsage_CopySrc = (1 << 4),
    GfxTextureUsage_MemorylessRenderTarget = ( 1 << 5),
};

enum GfxTextureDim
{
    GfxTextureDim_2D,
    GfxTextureDim_Cube,
    GfxTextureDim_Buffer,
};

enum GfxTextureMipFlag
{
    GfxTextureMipFlag_BEGIN_MIPS = 0u,
    GfxTextureMipFlag_1Mip,
    GfxTextureMipFlag_2Mips,
    GfxTextureMipFlag_3Mips,
    GfxTextureMipFlag_4Mips,
    GfxTextureMipFlag_5Mips,
    GfxTextureMipFlag_6Mips,
    GfxTextureMipFlag_7Mips,
    GfxTextureMipFlag_8Mips,
    GfxTextureMipFlag_9Mips,
    GfxTextureMipFlag_10Mips,
    GfxTextureMipFlag_11Mips,
    GfxTextureMipFlag_12Mips,
    GfxTextureMipFlag_END_MIPS,
    GfxTextureMipFlag_GenMips,
};

struct GfxTexture : GfxObjectRegistry<GfxTexture>
{
    rgChar          tag[RG_GFX_OBJECT_TAG_LENGTH];
    GfxTextureDim   dim;
    rgUInt          width;
    rgUInt          height;
    TinyImageFormat format;
    rgUInt          mipmapCount;
    GfxTextureUsage usage;
    rgU32           texID;

#if defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource>          d3dTexture;
    CD3DX12_CPU_DESCRIPTOR_HANDLE   d3dTextureView;
#elif defined(RG_METAL_RNDR)
    void*           mtlTexture; // type: id<MTLTexture>
#elif defined(RG_VULKAN_RNDR)
    VkImage vkTexture;
    VmaAllocation vmaAlloc;
#endif
    
    static rgUInt calcMipmapCount(GfxTextureMipFlag mipFlag, rgUInt width, rgUInt height)
    {
        rgUInt mip = 1;
        if(mipFlag == GfxTextureMipFlag_GenMips)
        {
            mip = ((rgUInt)floor(log2(eastl::max(width, height)))) + 1;
        }
        else
        {
            rgAssert(mipFlag > GfxTextureMipFlag_BEGIN_MIPS && mipFlag < GfxTextureMipFlag_END_MIPS);
            mip = (rgUInt)mipFlag;
        }
        return mip;
    }

    static void fillStruct(GfxTextureDim dim, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureMipFlag mipFlag, GfxTextureUsage usage, ImageSlice* slices, GfxTexture* obj)
    {
        obj->dim = dim;
        obj->width = width;
        obj->height = height;
        obj->format = format;
        obj->mipmapCount = calcMipmapCount(mipFlag, width, height);
        obj->usage = usage;
        
        if(dim != GfxTextureDim_2D)
        {
            rgAssert(obj->mipmapCount == 1);
            // TODO: handle cube & 3D mips
        }
    }

    static void createGfxObject(char const* tag, GfxTextureDim dim, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureMipFlag mipFlag, GfxTextureUsage usage, ImageSlice* slices, GfxTexture* obj);
    static void destroyGfxObject(GfxTexture* obj);
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

struct GfxSamplerState : GfxObjectRegistry<GfxSamplerState>
{
    rgChar                  tag[RG_GFX_OBJECT_TAG_LENGTH];
    GfxSamplerAddressMode   rstAddressMode;
    GfxSamplerMinMagFilter  minFilter;
    GfxSamplerMinMagFilter  magFilter;
    GfxSamplerMipFilter     mipFilter;
    rgBool                  anisotropy;
    
#if defined(RG_METAL_RNDR)
    void*                   mtlSampler;
#elif defined(RG_D3D12_RNDR)
    rgU32                   d3dStagedDescriptorIndex;
#endif

    static void fillStruct(GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSamplerState* obj)
    {
        obj->rstAddressMode = rstAddressMode;
        obj->minFilter = minFilter;
        obj->magFilter = magFilter;
        obj->mipFilter = mipFilter;
        obj->anisotropy = anisotropy;
    }

    static void createGfxObject(const char* tag, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSamplerState* obj);
    static void destroyGfxObject(GfxSamplerState* obj);
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
    GfxTexture*     texture;
    GfxLoadAction   loadAction;
    GfxStoreAction  storeAction;
    rgFloat4        clearColor;
};

struct GfxRenderPass
{
    GfxColorAttachmentDesc  colorAttachments[RG_MAX_COLOR_ATTACHMENTS];

    GfxTexture*             depthStencilAttachmentTexture;
    GfxLoadAction           depthStencilAttachmentLoadAction;
    GfxStoreAction          depthStencilAttachmentStoreAction;
    rgFloat                 clearDepth;
    rgU32                   clearStencil;
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
        const char*         semanticName;
        rgUInt              semanticIndex;
        TinyImageFormat     format;
        rgUInt              bufferIndex;
        rgUInt              offset;
        GfxVertexStepFunc   stepFunc;
    } elements[8];
    rgInt elementCount;
};

struct GfxColorAttachementStateDesc
{
    TinyImageFormat pixelFormat;
    rgBool          blendingEnabled;

    // Create predefined common blending operations instead of giving fine control like vv
    //GfxBlendOp blendOp;
    //GfxSourceBlendFacter srcBlendFactor;
};

struct GfxRenderStateDesc
{
    GfxColorAttachementStateDesc    colorAttachments[RG_MAX_COLOR_ATTACHMENTS];
    TinyImageFormat                 depthStencilAttachmentFormat;

    rgBool              depthWriteEnabled;
    GfxCompareFunc      depthCompareFunc;

    GfxCullMode         cullMode;
    GfxWinding          winding;
    GfxTriangleFillMode triangleFillMode;

    // Primitive Type, Line, LineStrip, Triangle, TriangleStrip
    // Index Type, U16, U32 ??
};

struct GfxShaderDesc
{
    char const* shaderSrc;
    char const* vsEntrypoint;
    char const* fsEntrypoint;
    char const* csEntrypoint;
    char const* defines;
};

// Resources/States in a Pipeline
struct GfxPipelineArgument
{
    char tag[32];
    
    GfxStage stages;
    
    enum Type
    {
        Type_Unknown,
        Type_ConstantBuffer,
        Type_Texture2D,
        Type_TextureCube,
        Type_RWTexture2D,
        Type_Sampler,
    };
    Type type;
    
    rgU16 registerIndex;
    rgU16 spaceIndex;

#if defined(RG_D3D12_RNDR)
    rgU32 d3dOffsetInDescriptorTable;
#endif
};

struct GfxGraphicsPSO : GfxObjectRegistry<GfxGraphicsPSO>
{
    rgChar              tag[RG_GFX_OBJECT_TAG_LENGTH];
    
    // State info
    GfxCullMode         cullMode;
    GfxWinding          winding;
    GfxTriangleFillMode triangleFillMode;

    eastl::hash_map<eastl::string, GfxPipelineArgument> arguments;

#if defined(RG_D3D12_RNDR)
    rgBool                      d3dHasCBVSRVUAVs;
    rgBool                      d3dHasSamplers;
    rgBool                      d3dHasBindlessResources;
    rgU32                       d3dVertexStrideInBytes;
    rgU32                       d3dCbvSrvUavDescriptorCount;
    rgU32                       d3dSamplerDescriptorCount;
    ComPtr<ID3D12RootSignature> d3dRootSignature;
    ComPtr<ID3D12PipelineState> d3dPSO;
#elif defined(RG_METAL_RNDR)
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

    static void createGfxObject(const char* tag, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO* obj);
    static void destroyGfxObject(GfxGraphicsPSO* obj);
};

struct GfxComputePSO : GfxObjectRegistry<GfxComputePSO>
{
    rgChar tag[32];
    
    // TODO: This info might only be needed for metal
    rgU32 threadsPerThreadgroupX;
    rgU32 threadsPerThreadgroupY;
    rgU32 threadsPerThreadgroupZ;

    eastl::hash_map<eastl::string, GfxPipelineArgument> arguments;
        
#if defined(RG_METAL_RNDR)
    void* mtlPSO; // type: id<MTLComputePipelineState>
#endif
    
    static void fillStruct(GfxShaderDesc* shaderDesc, GfxComputePSO* obj)
    {
    }
    
    static void createGfxObject(const char* tag, GfxShaderDesc* shaderDesc, GfxComputePSO* obj);
    static void destroyGfxObject(GfxComputePSO* obj);
};


//-----------------------------------------------------------------------------
// GFX COMMAND ENCODERS
//-----------------------------------------------------------------------------

// Render Encoder
// --------------

struct GfxRenderCmdEncoder
{
    void begin(char const* tag, GfxRenderPass* renderPass);
    void end();

    void pushDebugTag(const char* tag);
    void popDebugTag();
    
    void setViewport(rgFloat4 viewport);
    void setViewport(rgFloat originX, rgFloat originY, rgFloat width, rgFloat height);
    void setScissorRect(rgU32 xPixels, rgU32 yPixels, rgU32 widthPixels, rgU32 heightPixels);
    
    void setGraphicsPSO(GfxGraphicsPSO* pso);

    void setVertexBuffer(GfxBuffer const* buffer, rgU32 offset, rgU32 slot);
    void setVertexBuffer(GfxFrameResource const* resource, rgU32 slot);

    void bindBuffer(char const* bindingTag, GfxBuffer* buffer, rgU32 offset);
    void bindBuffer(char const* bindingTag, GfxFrameResource const* resource);
    void bindTexture(char const* bindingTag, GfxTexture* texture);
    void bindSamplerState(char const* bindingTag, GfxSamplerState* sampler);
    
    void drawTexturedQuads(TexturedQuads* quads);
    void drawTriangles(rgU32 vertexStart, rgU32 vertexCount, rgU32 instanceCount);
    void drawIndexedTriangles(rgU32 indexCount, rgBool is32bitIndex, GfxBuffer const* indexBuffer, rgU32 bufferOffset, rgU32 instanceCount);
    void drawIndexedTriangles(rgU32 indexCount, rgBool is32bitIndex, GfxFrameResource const* indexBufferResource, rgU32 instanceCount);

    GfxPipelineArgument& getPipelineArgument(char const* bindingTag);
    
    rgBool hasEnded;
#if defined(RG_METAL_RNDR)
    void* mtlRenderCommandEncoder; // type: id<MTLRenderCommandEncoder>
#elif defined(RG_D3D12_RNDR)
    ComPtr<ID3D12GraphicsCommandList> d3dGraphicsCommandlist;
#endif
};

struct GfxComputeCmdEncoder
{
    void begin(char const* tag);
    void end();
    
    void pushDebugTag(char const* tag);
    void popDebugTag();
    
    void setComputePSO(GfxComputePSO* pso);
    
    void bindBuffer(char const* bindingTag, GfxBuffer* buffer, rgU32 offset);
    void bindBuffer(char const* bindingTag, GfxFrameResource const* resource);
    void bindBufferFromData(char const* bindingTag, rgU32 sizeInBytes, void* data);
    void bindTexture(char const* bindingTag, GfxTexture* texture);
    void bindSamplerState(char const* bindingTag, GfxSamplerState* sampler);
    
    void dispatch(rgU32 threadgroupsGridX, rgU32 threadgroupsGridY, rgU32 threadgroupsGridZ);
    
    GfxPipelineArgument* getPipelineArgument(char const* bindingTag);
    
    rgBool hasEnded;
#if defined(RG_METAL_RNDR)
    void* mtlComputeCommandEncoder; // type: id<MTLComputeCommandEncoder>
#endif
};

struct GfxBlitCmdEncoder
{
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
                GfxTexture* tex;
                rgU32 uploadBufferOffset;
            } uploadTexture;
            struct
            {
                GfxTexture* tex;
            } genMips;
        };
    };

    void begin(char const* tag);
    void end();
    void pushDebugTag(const char* tag);
    void genMips(GfxTexture* srcTexture); // TODO: Handle non 2D types
    // TODO: Handle non 2D types
    void copyTexture(GfxTexture* srcTexture, GfxTexture* dstTexture, rgU32 srcMipLevel, rgU32 dstMipLevel, rgU32 mipLevelCount);

    rgBool hasEnded;
    eastl::vector<Cmd> cmds;

#if defined(RG_METAL_RNDR)
    void* mtlBlitCommandEncoder; // type: id<MTLBlitCommandEncoder>
#elif defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource> d3dUploadBuffer;
#endif
};


//-----------------------------------------------------------------------------
// GFX FUNCTIONS
//-----------------------------------------------------------------------------

// Common functions
// ----------------

rgInt                   gfxPreInit();
rgInt                   gfxPostInit();
void                    gfxAtFrameStart();
rgInt                   gfxGetFrameIndex(); // Returns 0 if g_FrameIndex is -1
rgInt                   gfxGetPrevFrameIndex();
GfxFrameAllocator*      gfxGetFrameAllocator();

GfxRenderCmdEncoder*    gfxSetRenderPass(char const* tag, GfxRenderPass* renderPass);
GfxComputeCmdEncoder*   gfxSetComputePass(char const* tag);
GfxBlitCmdEncoder*      gfxSetBlitPass(char const* tag);


// API specific implementation functions
// -------------------------------------

rgInt           gfxInit();
void            gfxDestroy();
void            gfxStartNextFrame();
void            gfxEndFrame();

void            gfxRunOnFrameBeginJob();

void            gfxRendererImGuiInit();
void            gfxRendererImGuiNewFrame();
void            gfxRendererImGuiRenderDrawData();

void            gfxOnSizeChanged();

GfxTexture*     gfxGetCurrentRenderTargetColorBuffer();
GfxTexture*     gfxGetBackbufferTextureLinear();


//-----------------------------------------------------------------------------
// GFX HELPER CLASSES
//-----------------------------------------------------------------------------

// Bindless-Resource Manager
// -------------------------

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
        gfxSetBindlessResource(slot, ptr);
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


// Frame Resource Allocator
//-----------------------------

struct GfxFrameResource
{
    enum Type
    {
        Type_Undefined,
        Type_Buffer,
        Type_Texture,
    };
    
    Type    type;
    rgU32   sizeInBytes;

#if defined(RG_METAL_RNDR)
    void* mtlBuffer; //type: id<MTLBuffer>
    void* mtlTexture; //type: id<MTLTexture>
#elif defined(RG_D3D12_RNDR)
    ID3D12Resource* d3dResource;
#endif
};

class GfxFrameAllocator
{
protected:
    rgU32 offset;
    rgU32 capacity;
#if defined(RG_METAL_RNDR)
    void* heap; //type: id<MTLHeap>
    eastl::vector<void*> mtlResources;
#elif defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Heap> d3dBufferHeap;
    ComPtr<ID3D12Heap> d3dNonRTDSTextureHeap;
    ComPtr<ID3D12Heap> d3dRTDSTextureHeap;
    eastl::vector<ComPtr<ID3D12Resource>> d3dResources;
#else
    void* heap;
#endif
    
    void create(rgU32 bufferHeapSize, rgU32 nonRTDSTextureHeapSize, rgU32 rtDSTextureHeapSize);
    void destroy();
    void releaseResources();
public:
    GfxFrameAllocator(rgU32 bufferHeapSize, rgU32 nonRTDSTextureHeapSize, rgU32 rtDSTextureHeapSize)
    : offset(0)
    , capacity(0) // TODO: Use this to prevent overflow.. // TODO: Use this to prevent overflow
    {
        create(bufferHeapSize, nonRTDSTextureHeapSize, rtDSTextureHeapSize);
    }
    
    ~GfxFrameAllocator()
    {
        destroy();
    }
    
    rgU32 bumpStorageAligned(rgU32 s, rgU32 a)
    {
        rgU32 unalignedStartOffset = offset;
        rgU32 alignedStartOffset = (unalignedStartOffset + a - 1) & ~(a - 1);
        
        // bump 'offset'
        offset = alignedStartOffset + s;
        
        return alignedStartOffset;
    }
    
    GfxFrameResource newBuffer(const char* tag, rgU32 size, void* initialData);
    GfxFrameResource newTexture2D(const char* tag, void* initialData, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage);

    void reset()
    {
        offset = 0;
        releaseResources();
    }
    
    // TODO: Remove this function. We can directly access the respective heap resource var
    void* getHeap()
    {
        // TODO: rework this function
#if !defined(RG_D3D12_RNDR)
        return (void*)heap;
#else
        return nullptr;
#endif
    }
};

//-----------------------------------------------------------------------------
// GFX STATE
//-----------------------------------------------------------------------------

struct GfxState
{
    static GfxGraphicsPSO* graphicsPSO;
    static GfxComputePSO*  computePSO;
    
    static GfxSamplerState* samplerBilinearRepeat;
    static GfxSamplerState* samplerBilinearClampEdge;
    static GfxSamplerState* samplerTrilinearRepeatAniso;
    static GfxSamplerState* samplerTrilinearClampEdgeAniso;
    static GfxSamplerState* samplerNearestRepeat;
    static GfxSamplerState* samplerNearestClampEdge;
};

extern GfxBindlessResourceManager<GfxTexture>* g_BindlessTextureManager;

// TODO: move to GfxState?
// TODO: gfxFrameIndex() instead of gfxGetFrameIndex() ?
extern rgInt g_FrameIndex; // Frame index in swapchain, i.e. 0, 1, 2, 0, 1, 2


//-----------------------------------------------------------------------------
// IMAGE/BITMAP AND MODEL/MESH
//-----------------------------------------------------------------------------

// Image type
// -------------
struct ImageSlice
{
    rgU16  width;
    rgU16  height;
    rgU32  rowPitch;
    rgU32  slicePitch;
    rgU8*  pixels;
};

struct Image
{
    rgChar          tag[32];
    rgU16           width;
    rgU16           height;
    TinyImageFormat format;
    rgBool          isDDS;
    rgU8            mipCount;
    rgU8            sliceCount;
    ImageSlice      slices[12];
    rgU8*           memory;

    void*           dxTexScratchImage;
};
typedef eastl::shared_ptr<Image> ImageRef;

ImageRef    loadImage(char const* filename, bool srgbFormat = true);
void        unloadImage(Image* ptr);


// Model and Mesh
//---------------

enum MeshProperties
{
    MeshProperties_None = (0 << 0),
    MeshProperties_Has32BitIndices = (1 << 0),
    MeshProperties_HasTexCoord = (1 << 1),
    MeshProperties_HasNormal = (1 << 2),
    MeshProperties_HasTangent = (1 << 3),
};

RG_DEFINE_ENUM_FLAGS_OPERATOR(MeshProperties);

struct Mesh
{
    char tag[32];
    Matrix4 transform;
    MeshProperties properties;
    rgU32 vertexCount;
    rgU32 vertexDataOffset;
    rgU32 indexCount;
    rgU32 indexDataOffset;
};

struct Model
{
    char tag[32];
    eastl::vector<Mesh> meshes;
    rgU32 vertexBufferOffset;
    rgU32 index32BufferOffset;
    rgU32 index16BufferOffset;
    GfxBuffer* vertexIndexBuffer;
};

typedef eastl::shared_ptr<Model> ModelRef;

ModelRef loadModel(char const* filename);
void     unloadModel(Model* ptr);


//-----------------------------------------------------------------------------
// 2D RENDERING HELPERS
//-----------------------------------------------------------------------------

// Texture Quad UV helper
// ----------------------
struct QuadUV
{
    // TODO: use Vector4 here?
    rgFloat uvTopLeft[2];
    rgFloat uvBottomRight[2];
};
 
extern QuadUV defaultQuadUV;

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx);
QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, ImageRef image);


// Textured Quad
// -------------

struct TexturedQuad
{
    QuadUV uv;
    rgU32 texID;
    rgFloat3 pos;
    rgFloat4 offsetOrientation;
    rgFloat2 size;
};

void pushTexturedQuad(TexturedQuads* quadList, QuadUV uv, rgFloat4 posSize, rgFloat4 offsetOrientation, GfxTexture* tex);

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
// HELPER FUNCTIONS
//-----------------------------------------------------------------------------

void    copyMatrix4ToFloatArray(rgFloat* dstArray, Matrix4 const& srcMatrix);
void    copyMatrix3ToFloatArray(rgFloat* dstArray, Matrix3 const& srcMatrix);

Matrix4 makeOrthographicProjectionMatrix(rgFloat left, rgFloat right, rgFloat bottom, rgFloat top, rgFloat nearValue, rgFloat farValue);
Matrix4 makePerspectiveProjectionMatrix(rgFloat focalLength, rgFloat aspectRatio, rgFloat nearPlane, rgFloat farPlane);

TinyImageFormat convertSRGBToLinearFormat(TinyImageFormat srgbFormat);
TinyImageFormat convertLinearToSRGBFormat(TinyImageFormat linearFormat);

#endif
