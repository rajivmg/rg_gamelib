#ifndef __GFX_H__
#define __GFX_H__

// NOTES:

// Swapchain texture format is sRGB, and pixels written/copied to it
// will be converter to sRGB. Therefore it expects pixel value to be
// in linear space.

// Shader ops are assumed to be in linear colorspace. So using a sRGB
// format means texture fetches will convert the pixels it read from
// sRGB to linear colorspace.

// Writing from a fragment shader to a render target will perform linear
// to sRGB colorspace conversion and handle the blending. But writing to
// a sRGB texture UAV will not perform the linear to sRGB conversion, but
// it can be done manully in shader. See:
// https://wickedengine.net/2022/11/graphics-api-secrets-format-casting/

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
#include <EASTL/vector_multimap.h>
#include <EASTL/hash_map.h>

#define RG_MAX_FRAMES_IN_FLIGHT 3
#define RG_MAX_BINDLESS_TEXTURE_RESOURCES 100000
#define RG_MAX_COLOR_ATTACHMENTS 4
#define RG_GFX_OBJECT_TAG_LENGTH 32

#define ENABLE_GFX_OBJECT_INVALID_TAG_OP_ASSERT
#define ENABLE_SLOW_GFX_RESOURCE_VALIDATIONS

static const u32 kInvalidValue = ~(0x0);
static const u32 kUninitializedValue = 0;

// FORWARD DECLARATIONS
struct  GfxTexture;
struct  GfxFrameResource;
class   GfxFrameAllocator;
struct  TexturedQuad;
enum    SpriteLayer : u8;

// PERF: check performance with normal multimap
typedef eastl::vector_multimap<SpriteLayer, TexturedQuad> TexturedQuads;

i32     gfxGetFrameIndex();
void    gfxSetBindlessResource(u32 slot, GfxTexture* ptr);

// Gfx Object Registry
// -------------------
// NOTE: A base class for every Gfx resource type
// TODO: Add a destroyAllObjectsNow() function to be
// called at the last frame of the application, because 
// there will not be more frames to free and destroy objects.
template<typename Type, typename... Args>
struct GfxObjectRegistry
{
    rgChar  tag[RG_GFX_OBJECT_TAG_LENGTH];

    static eastl::vector<Type*> objectsToDestroy[RG_MAX_FRAMES_IN_FLIGHT];
    
    static Type* create(const char* name, Args... args)
    {
        Type* obj = rgNew(Type);
        if(name != nullptr)
        {
            strncpy(obj->tag, name, rgArrayCount(tag));
        }
        
        Type::fillStruct(args..., obj);
        Type::createGfxObject(name, args..., obj);

        return obj;
    }
    
    static void destroy(Type* obj)
    {
        objectsToDestroy[gfxGetFrameIndex()].push_back(obj);
    }

    static void destroyMarkedObjects()
    {
        i32 frameIndex = gfxGetFrameIndex();
        for(auto itr : objectsToDestroy[frameIndex])
        {
            Type::destroyGfxObject(itr);
            rgDelete(&(*itr));
        }
        objectsToDestroy[frameIndex].clear();
    }
};

template<typename Type, typename... Args>
eastl::vector<Type*> GfxObjectRegistry<Type, Args...>::objectsToDestroy[RG_MAX_FRAMES_IN_FLIGHT];


//-----------------------------------------------------------------------------
// GFX OBJECT TYPES
//-----------------------------------------------------------------------------

// Only D3D12: Descriptor/View stuff
#ifdef RG_D3D12_RNDR
enum GfxD3DViewType
{
    GfxD3DViewType_CBV,
    GfxD3DViewType_SRV,
    GfxD3DViewType_UAV,
    GfxD3DViewType_RTV,
    GfxD3DViewType_DSV,
    GfxD3DViewType_COUNT
};

struct GfxD3DView
{
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor;
    u32 descriptorIndex;
    rgBool isValid;
};
#endif

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
struct GfxBuffer : GfxObjectRegistry<GfxBuffer, GfxMemoryType, void*, rgSize, GfxBufferUsage>
{
    rgSize          size;
    GfxBufferUsage  usage; // TODO: This doesn't seem to be required in Metal & D3D12 backends.. remove?
    GfxMemoryType   memoryType;
#if defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource>  d3dResource;
    GfxD3DView              d3dViews[GfxD3DViewType_COUNT];
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
    void*   map(u32 rangeBeginOffset, u32 rangeSizeInBytes); // TODO: MTL D3D12 - handle ranges
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
    GfxTextureMipFlag_13Mips,
    GfxTextureMipFlag_14Mips,
    GfxTextureMipFlag_15Mips,
    GfxTextureMipFlag_16Mips,
    GfxTextureMipFlag_END_MIPS,
    GfxTextureMipFlag_GenMips,
};

struct GfxTexture : GfxObjectRegistry<GfxTexture, GfxTextureDim, u32, u32, TinyImageFormat, GfxTextureMipFlag, GfxTextureUsage, ImageSlice*>
{
    GfxTextureDim   dim;
    u32          width;
    u32          height;
    TinyImageFormat format;
    u32          mipmapCount;
    GfxTextureUsage usage;
    u32           texID;

#if defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource>          d3dResource;
    GfxD3DView                      d3dViews[GfxD3DViewType_COUNT];
#elif defined(RG_METAL_RNDR)
    void*           mtlTexture; // type: id<MTLTexture>
#elif defined(RG_VULKAN_RNDR)
    VkImage vkTexture;
    VmaAllocation vmaAlloc;
#endif
    
    static u32 calcMipmapCount(GfxTextureMipFlag mipFlag, u32 width, u32 height)
    {
        u32 mip = 1;
        if(mipFlag == GfxTextureMipFlag_GenMips)
        {
            mip = ((u32)floor(log2(eastl::max(width, height)))) + 1;
        }
        else
        {
            rgAssert(mipFlag > GfxTextureMipFlag_BEGIN_MIPS && mipFlag < GfxTextureMipFlag_END_MIPS);
            mip = (u32)mipFlag;
        }
        return mip;
    }

    static void fillStruct(GfxTextureDim dim, u32 width, u32 height, TinyImageFormat format, GfxTextureMipFlag mipFlag, GfxTextureUsage usage, ImageSlice* slices, GfxTexture* obj)
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

    static void createGfxObject(char const* tag, GfxTextureDim dim, u32 width, u32 height, TinyImageFormat format, GfxTextureMipFlag mipFlag, GfxTextureUsage usage, ImageSlice* slices, GfxTexture* obj);
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

struct GfxSamplerState : GfxObjectRegistry<GfxSamplerState, GfxSamplerAddressMode, GfxSamplerMinMagFilter, GfxSamplerMinMagFilter, GfxSamplerMipFilter, rgBool>
{
    GfxSamplerAddressMode   rstAddressMode;
    GfxSamplerMinMagFilter  minFilter;
    GfxSamplerMinMagFilter  magFilter;
    GfxSamplerMipFilter     mipFilter;
    rgBool                  anisotropy;
    
#if defined(RG_METAL_RNDR)
    void*                   mtlSampler;
#elif defined(RG_D3D12_RNDR)
    u32                   d3dStagedDescriptorIndex;
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
    f32                     clearDepth;
    u8                      clearStencil;
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
        u32                 semanticIndex;
        TinyImageFormat     format;
        u32                 bufferIndex;
        u32                 offset;
        GfxVertexStepFunc   stepFunc;
    } elements[8];
    i32 elementCount;
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
    
    u16 registerIndex;
    u16 spaceIndex;

#if defined(RG_D3D12_RNDR)
    u32 d3dOffsetInDescriptorTable;
#endif
};

struct GfxGraphicsPSO : GfxObjectRegistry<GfxGraphicsPSO, GfxVertexInputDesc*, GfxShaderDesc*, GfxRenderStateDesc*>
{
    // State info
    GfxCullMode         cullMode;
    GfxWinding          winding;
    GfxTriangleFillMode triangleFillMode;

    eastl::hash_map<eastl::string, GfxPipelineArgument> arguments;

#if defined(RG_D3D12_RNDR)
    rgBool                      d3dHasCBVSRVUAVs;
    rgBool                      d3dHasSamplers;
    rgBool                      d3dHasBindlessResources;
    u32                         d3dVertexStrideInBytes;
    u32                         d3dCbvSrvUavDescriptorCount;
    u32                         d3dSamplerDescriptorCount;
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

struct GfxComputePSO : GfxObjectRegistry<GfxComputePSO, GfxShaderDesc*>
{
    // TODO: This info might only be needed for metal
    u32 threadsPerThreadgroupX;
    u32 threadsPerThreadgroupY;
    u32 threadsPerThreadgroupZ;

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
    void setViewport(f32 originX, f32 originY, f32 width, f32 height);
    void setScissorRect(u32 xPixels, u32 yPixels, u32 widthPixels, u32 heightPixels);
    
    void setGraphicsPSO(GfxGraphicsPSO* pso);

    void setVertexBuffer(GfxBuffer const* buffer, u32 offset, u32 slot);
    void setVertexBuffer(GfxFrameResource const* resource, u32 slot);

    void bindBuffer(char const* bindingTag, GfxBuffer* buffer, u32 offset);
    void bindBuffer(char const* bindingTag, GfxFrameResource const* resource);
    void bindTexture(char const* bindingTag, GfxTexture* texture);
    void bindSamplerState(char const* bindingTag, GfxSamplerState* sampler);
    
    void drawTexturedQuads(TexturedQuads* quads, Matrix4 const* viewMatrix, Matrix4 const* projectionMatrix);
    void drawTriangles(u32 vertexStart, u32 vertexCount, u32 instanceCount);
    void drawIndexedTriangles(u32 indexCount, rgBool is32bitIndex, GfxBuffer const* indexBuffer, u32 bufferOffset, u32 instanceCount);
    void drawIndexedTriangles(u32 indexCount, rgBool is32bitIndex, GfxFrameResource const* indexBufferResource, u32 instanceCount);

    GfxPipelineArgument* getPipelineArgument(char const* bindingTag);
    
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
    
    void bindBuffer(char const* bindingTag, GfxBuffer* buffer, u32 offset);
    void bindBuffer(char const* bindingTag, GfxFrameResource const* resource);
    void bindBufferFromData(char const* bindingTag, u32 sizeInBytes, void* data);
    void bindTexture(char const* bindingTag, GfxTexture* texture);
    void bindSamplerState(char const* bindingTag, GfxSamplerState* sampler);
    
    void dispatch(u32 threadgroupsGridX, u32 threadgroupsGridY, u32 threadgroupsGridZ);
    
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
                u32 uploadBufferOffset;
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
    void copyTexture(GfxTexture* srcTexture, GfxTexture* dstTexture, u32 srcMipLevel, u32 dstMipLevel, u32 mipLevelCount);

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

i32                     gfxPreInit();
i32                     gfxPostInit();
void                    gfxAtFrameStart();
i32                     gfxGetFrameIndex(); // Returns 0 if g_FrameIndex is -1
i32                     gfxGetPrevFrameIndex();
GfxFrameAllocator*      gfxGetFrameAllocator();

GfxRenderCmdEncoder*    gfxSetRenderPass(char const* tag, GfxRenderPass* renderPass);
GfxComputeCmdEncoder*   gfxSetComputePass(char const* tag);
GfxBlitCmdEncoder*      gfxSetBlitPass(char const* tag);

void                    setRenderPass(char const* tag, GfxRenderPass* renderPass);

void                    setViewport(rgFloat4 viewport);
void                    setViewport(f32 originX, f32 originY, f32 width, f32 height);
void                    setScissorRect(u32 xPixels, u32 yPixels, u32 widthPixels, u32 heightPixels);
void                    setGraphicsPSO(GfxGraphicsPSO* pso);

void                    setVertexBuffer(GfxBuffer const* buffer, u32 offset, u32 slot);
void                    setVertexBuffer(GfxFrameResource const* resource, u32 slot);
void                    bindBuffer(char const* bindingTag, GfxBuffer* buffer, u32 offset);
void                    bindBuffer(char const* bindingTag, GfxFrameResource const* resource);
void                    bindTexture(char const* bindingTag, GfxTexture* texture);
void                    bindSamplerState(char const* bindingTag, GfxSamplerState* sampler);

void                    drawTexturedQuads(TexturedQuads* quads, Matrix4 const* viewMatrix, Matrix4 const* projectionMatrix);
void                    drawTriangles(u32 vertexStart, u32 vertexCount, u32 instanceCount);
void                    drawIndexedTriangles(u32 indexCount, rgBool is32bitIndex, GfxBuffer const* indexBuffer, u32 bufferOffset, u32 instanceCount);
void                    drawIndexedTriangles(u32 indexCount, rgBool is32bitIndex, GfxFrameResource const* indexBufferResource, u32 instanceCount);

void                    setComputePSO(GfxComputePSO* pso);

void                    dispatch(u32 threadgroupsGridX, u32 threadgroupsGridY, u32 threadgroupsGridZ);

// API specific implementation functions
// -------------------------------------

i32             gfxInit();
void            gfxDestroy();
void            gfxStartNextFrame();
void            gfxEndFrame();

void            gfxRunOnFrameBeginJob();

void            gfxRendererImGuiInit();
void            gfxRendererImGuiNewFrame();
void            gfxRendererImGuiRenderDrawData();

void            gfxOnSizeChanged();

GfxTexture*     gfxGetBackbufferTexture();
GfxTexture*     gfxGetBackbufferTextureLinear();

TinyImageFormat gfxGetBackbufferFormat();


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

    typedef eastl::vector<u32> SlotList;
    SlotList freeSlots; // Indexes in referenceList which are unused
    
    typedef eastl::hash_map<Type*, u32> ResourcePtrSlotMapType;
    ResourcePtrSlotMapType resourcePtrSlotMap;

    u32 getFreeSlot()
    {
        u32 result = kInvalidValue;

        if(!freeSlots.empty())
        {
            result = freeSlots.back();
            freeSlots.pop_back();
        }
        else
        {
            rgAssert(resources.size() < UINT32_MAX);
            result = (u32)resources.size();
            resources.resize(result + 1); // push_back(nullptr) ?
        }

        rgAssert(result != kInvalidValue);
        return result;
    }

    void releaseSlot(u32 slot)
    {
        rgAssert(slot < resources.size());

        resources[slot] = nullptr;
        freeSlots.push_back(slot);
        typename ResourcePtrSlotMapType::iterator itr = resourcePtrSlotMap.find(slot);
        rgAssert(itr != resourcePtrSlotMap.end());
        resourcePtrSlotMap.erase(itr);
    }

    void setResourcePtrInSlot(u32 slot, Type* ptr)
    {
        rgAssert(ptr);
        rgAssert(slot != UINT32_MAX);
        resources[slot] = ptr;
        resourcePtrSlotMap.insert(eastl::make_pair(ptr, slot));
        gfxSetBindlessResource(slot, ptr);
        // TODO: make sure we're not updating a resource in flight
        // this should be implictly handled from _releaseSlot
    }

    u32 getSlotOfResourcePtr(Type* ptr)
    {
        typename ResourcePtrSlotMapType::iterator itr = resourcePtrSlotMap.find(ptr);
        if(itr != resourcePtrSlotMap.end())
        {
            return itr->second;
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
    
    // When it is known that the resource is not already stored
    u32 store(Type* ptr)
    {
        u32 slot = getFreeSlot();
        setResourcePtrInSlot(slot, ptr);
        return slot;
    }
    
    // When it is not known whether the resource is already stored or not
    u32 storeIfNotPresent(Type* ptr)
    {
        u32 slot = getSlotOfResourcePtr(ptr);
        if(slot == kInvalidValue)
        {
            slot = getFreeSlot();
            setResourcePtrInSlot(slot, ptr);
        }
        return slot;
    }

    Type* getPtr(u32 slot)
    {
        rgAssert(slot < resources.size());

        Type* ptr = resources[slot];
        return ptr;
    }

    // TODO: u32 getContiguousFreeSlots(int count);
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
    u32   sizeInBytes;

#if defined(RG_METAL_RNDR)
    void* mtlBuffer; //type: id<MTLBuffer>
    void* mtlTexture; //type: id<MTLTexture>
#elif defined(RG_D3D12_RNDR)
    ComPtr<ID3D12Resource> d3dResource;
#endif
};

class GfxFrameAllocator
{
protected:
    u32 offset;
    u32 capacity;
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
    
    void create(u32 bufferHeapSize, u32 nonRTDSTextureHeapSize, u32 rtDSTextureHeapSize);
    void destroy();
    void releaseResources();
public:
    GfxFrameAllocator(u32 bufferHeapSize, u32 nonRTDSTextureHeapSize, u32 rtDSTextureHeapSize)
    : offset(0)
    , capacity(0) // TODO: Use this to prevent overflow.. // TODO: Use this to prevent overflow
    {
        create(bufferHeapSize, nonRTDSTextureHeapSize, rtDSTextureHeapSize);
    }
    
    ~GfxFrameAllocator()
    {
        destroy();
    }
    
    u32 bumpStorageAligned(u32 s, u32 a)
    {
        u32 unalignedStartOffset = offset;
        u32 alignedStartOffset = (unalignedStartOffset + a - 1) & ~(a - 1);
        
        // bump 'offset'
        offset = alignedStartOffset + s;
        
        return alignedStartOffset;
    }
    
    GfxFrameResource newBuffer(const char* tag, u32 size, void* initialData);
    GfxFrameResource newTexture2D(const char* tag, void* initialData, u32 width, u32 height, TinyImageFormat format, GfxTextureUsage usage);

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
extern i32 g_FrameIndex; // Frame index in swapchain, i.e. 0, 1, 2, 0, 1, 2


//-----------------------------------------------------------------------------
// IMAGE/BITMAP AND MODEL/MESH
//-----------------------------------------------------------------------------

// Image type
// -------------
struct ImageSlice
{
    u16  width;
    u16  height;
    u32  rowPitch;
    u32  slicePitch;
    u8*  pixels;
};

struct Image
{
    rgChar          tag[32];
    u16           width;
    u16           height;
    TinyImageFormat format;
    rgBool          isDDS;
    GfxTextureMipFlag mipFlag;
    u8            sliceCount;
    ImageSlice      slices[15];
    u8*           memory;

    void*           dxTexScratchImage;
};
typedef eastl::shared_ptr<Image> ImageRef;

ImageRef    loadImage(char const* filename, bool srgbFormat = true);
void        unloadImage(Image* ptr);


// Default Material
// ----------------

struct DefaultMaterial
{
    rgChar tag[32];
    GfxTexture* diffuseAlpha;
    GfxTexture* normal;
    GfxTexture* properties;
    
    typedef eastl::hash_map<rgHash, DefaultMaterial*> LoadedMaterialsType;
    static LoadedMaterialsType s_LoadedMaterials;
};
typedef eastl::shared_ptr<DefaultMaterial> DefaultMaterialRef;


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
    u32 vertexCount;
    u32 vertexDataOffset;
    u32 indexCount;
    u32 indexDataOffset;
    DefaultMaterialRef material;
};

struct Model
{
    char tag[32];
    eastl::vector<Mesh> meshes;
    u32 vertexBufferOffset;
    u32 index32BufferOffset;
    u32 index16BufferOffset;
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
    rgFloat2 uvTopLeft;
    rgFloat2 uvBottomRight;
};
extern QuadUV defaultQuadUV;

QuadUV createQuadUV(u32 xPx, u32 yPx, u32 widthPx, u32 heightPx, u32 refWidthPx, u32 refHeightPx);
QuadUV createQuadUV(u32 xPx, u32 yPx, u32 widthPx, u32 heightPx, ImageRef image);
QuadUV repeatQuadUV(rgFloat2 repeatXY);


// Textured Quad
// -------------

enum SpriteLayer : u8
{
    SpriteLayer_0,
    SpriteLayer_1,
    SpriteLayer_2,
    SpriteLayer_3,
    SpriteLayer_4,
    SpriteLayer_5,
    SpriteLayer_6,
    SpriteLayer_7,
    SpriteLayer_8,
    SpriteLayer_9,
};

struct TexturedQuad
{
    QuadUV uv;
    u32 texID;
    rgFloat3 pos;
    u32 color;
    rgFloat4 offsetOrientation;
    rgFloat2 size;
};

void pushTexturedQuad(TexturedQuads* quadList, SpriteLayer layer, QuadUV uv, rgFloat4 posSize, u32 color, rgFloat4 offsetOrientation, GfxTexture* tex);
void pushTexturedLine(TexturedQuads* quadList, SpriteLayer layer, QuadUV uv, rgFloat2 pointA, rgFloat2 pointB, f32 thickness, u32 color, GfxTexture* tex);

struct SimpleVertexFormat
{
    f32 pos[3];
    f32 texcoord[2];
    unsigned char color[4];
};

struct SimpleInstanceParams
{
    u32 texParam[1024][4];
};

void genTexturedQuadVertices(TexturedQuads* quadList, eastl::vector<SimpleVertexFormat>* vertices, SimpleInstanceParams* instanceParams);


//-----------------------------------------------------------------------------
// HELPER FUNCTIONS
//-----------------------------------------------------------------------------

void    copyMatrix4ToFloatArray(f32* dstArray, Matrix4 const& srcMatrix);
void    copyMatrix3ToFloatArray(f32* dstArray, Matrix3 const& srcMatrix);

Matrix4 makeOrthographicProjectionMatrix(f32 left, f32 right, f32 bottom, f32 top, f32 nearValue, f32 farValue);
Matrix4 makePerspectiveProjectionMatrix(f32 focalLength, f32 aspectRatio, f32 nearPlane, f32 farPlane);

TinyImageFormat convertSRGBToLinearFormat(TinyImageFormat srgbFormat);
TinyImageFormat convertLinearToSRGBFormat(TinyImageFormat linearFormat);

#endif
