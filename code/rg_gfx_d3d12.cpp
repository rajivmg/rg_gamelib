#if defined(RG_D3D12_RNDR)
#include "rg_gfx.h"

#include "rg_gfx_dxc.h"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_dx12.h"

#include "ResourceUploadBatch.h"
#include "DescriptorHeap.h"
using namespace DirectX;

#include <EASTL/string.h>
#include <EASTL/hash_set.h>

#include "shaders/shaderinterop_common.h"

RG_BEGIN_CORE_NAMESPACE

static rgUInt const MAX_RTV_DESCRIPTOR = 1024;
static rgUInt const MAX_CBVSRVUAV_DESCRIPTOR = 400000;
static rgUInt const MAX_SAMPLER_DESCRIPTOR = 1024;
static rgUInt const CBVSRVUAV_ROOT_PARAMETER_INDEX = 0;
static rgUInt const SAMPLER_ROOT_PARAMETER_INDEX = 1;
static rgUInt const BINDLESS_CBVSRVUAV_ROOT_PARAMETER_INDEX = 2;

ComPtr<ID3D12Device2> device;
ComPtr<ID3D12CommandQueue> commandQueue;
ComPtr<IDXGISwapChain4> dxgiSwapchain;
ComPtr<IDXGIFactory4> dxgiFactory;

ComPtr<ID3D12Fence> frameFence;
UINT64 frameFenceValues[RG_MAX_FRAMES_IN_FLIGHT];
HANDLE frameFenceEvent;

ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap;
rgUInt rtvDescriptorSize;

ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap;
rgUInt dsvDescriptorSize;

rgU32  pipelineCbvSrvUavDescriptorRangeOffset;
rgU32  pipelineSamplerDescriptorRangeOffset;
rgU32  descriptorRangeOffsetBindlessTexture2D;

ComPtr<ID3D12CommandAllocator> commandAllocator[RG_MAX_FRAMES_IN_FLIGHT];
ComPtr<ID3D12GraphicsCommandList> currentCommandList;

Texture* swapchainTextures[RG_MAX_FRAMES_IN_FLIGHT];
Texture* depthStencilTexture;

struct ResourceCopyTask
{
    enum ResourceType
    {
        ResourceType_Buffer,
        ResourceType_Texture,
    };

    ResourceType  type;
    ComPtr<ID3D12Resource> src;
    ComPtr<ID3D12Resource> dst;
};

eastl::vector<ResourceCopyTask> pendingBufferCopyTasks;
eastl::vector<ResourceCopyTask> pendingTextureCopyTasks;

ResourceUploadBatch* resourceUploader;

//*****************************************************************************
// Helper Functions
//*****************************************************************************

inline void _BreakIfFail(HRESULT hr)
{
    if(FAILED(hr))
    {
        rgAssert("D3D call failed!");
    }
}

#define BreakIfFail(x) _BreakIfFail(x)

static ComPtr<ID3D12Device> getDevice()
{
    return device;
}

static inline DXGI_FORMAT toDXGIFormat(TinyImageFormat fmt)
{
    return (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(fmt);
}

static inline D3D12_COMPARISON_FUNC toD3DCompareFunc(CompareFunc func)
{
    D3D12_COMPARISON_FUNC result = D3D12_COMPARISON_FUNC_NONE;
    switch(func)
    {
    case GfxCompareFunc_Always:
        result = D3D12_COMPARISON_FUNC_ALWAYS;
        break;
    case GfxCompareFunc_Never:
        result = D3D12_COMPARISON_FUNC_NEVER;
        break;
    case GfxCompareFunc_Equal:
        result = D3D12_COMPARISON_FUNC_EQUAL;
        break;
    case GfxCompareFunc_NotEqual:
        result = D3D12_COMPARISON_FUNC_NOT_EQUAL;
        break;
    case GfxCompareFunc_Less:
        result = D3D12_COMPARISON_FUNC_LESS;
        break;
    case GfxCompareFunc_LessEqual:
        result = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        break;
    case GfxCompareFunc_Greater:
        result = D3D12_COMPARISON_FUNC_GREATER;
        break;
    case GfxCompareFunc_GreaterEqual:
        result = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        break;
    }
    return result;
}

static D3D12_TEXTURE_ADDRESS_MODE toD3DTextureAddressMode(SamplerAddressMode rstAddressMode)
{
    D3D12_TEXTURE_ADDRESS_MODE result = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    switch(rstAddressMode)
    {
    case GfxSamplerAddressMode_Repeat:
    {
        result = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    } break;
    case GfxSamplerAddressMode_ClampToEdge:
    {
        result = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    } break;
    case GfxSamplerAddressMode_ClampToZero:
    {
        result = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    } break;
    case GfxSamplerAddressMode_ClampToBorderColor:
    {
        result = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    } break;
    default:
    {
        rgAssert(!"Invalid SamplerAddressMode value");
    }
    }
    return result;
}

static PipelineArgument::Type toGfxPipelineArgumentType(D3D12_SHADER_INPUT_BIND_DESC const& d3dShaderInputBindDesc)
{
    PipelineArgument::Type outType = PipelineArgument::Type_Unknown;
    switch(d3dShaderInputBindDesc.Type)
    {
    case D3D_SIT_CBUFFER:
    {
        outType = PipelineArgument::Type_ConstantBuffer;
    } break;
    case D3D_SIT_TEXTURE:
    {
        if(d3dShaderInputBindDesc.Dimension == D3D_SRV_DIMENSION_TEXTURE2D)
        {
            outType = PipelineArgument::Type_Texture2D;
        }
        else if(d3dShaderInputBindDesc.Dimension == D3D_SRV_DIMENSION_TEXTURECUBE)
        {
            outType = PipelineArgument::Type_TextureCube;
        }
        else
        {
            rgAssert("Unsupported Texture dimension");
        }
    } break;
    default:
    {
        rgAssert("Unssported Shader Input Type");
    } break;
    }

    return outType;
};

static ComPtr<ID3D12CommandAllocator> getCommandAllocator()
{
    return commandAllocator[g_FrameIndex];
}

static ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, rgUInt descriptorsCount, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = descriptorsCount;
    desc.Flags = flags;

    BreakIfFail(getDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

static ComPtr<ID3D12CommandAllocator> createCommandAllocator(D3D12_COMMAND_LIST_TYPE commandListType)
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    BreakIfFail(getDevice()->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

static ComPtr<ID3D12GraphicsCommandList> createGraphicsCommandList(ComPtr<ID3D12CommandAllocator> commandAllocator, ID3D12PipelineState* pipelineState)
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    BreakIfFail(getDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState, IID_PPV_ARGS(&commandList)));
    BreakIfFail(commandList->Close());
    return commandList;
}

static void setDebugName(ComPtr<ID3D12Object> d3dObject, char const* name)
{
    const rgUInt maxWCharLength = 1024;
    wchar_t wc[maxWCharLength];

    size_t size = eastl::CharStrlen(name) + 1;
    rgAssert(size <= maxWCharLength);

    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wc, maxWCharLength, name, _TRUNCATE);

    BreakIfFail(d3dObject->SetName(wc));
};

static void waitForGpu()
{
    BreakIfFail(commandQueue->Signal(frameFence.Get(), frameFenceValues[g_FrameIndex]));
    BreakIfFail(frameFence->SetEventOnCompletion(frameFenceValues[g_FrameIndex], frameFenceEvent));
    ::WaitForSingleObject(frameFenceEvent, INFINITE);

    frameFenceValues[g_FrameIndex] += 1;
}

//*****************************************************************************
class DescriptorAllocator
{
public:
    DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, rgU32 count, rgU32 persistentCount)
        : descriptorCount(count)
        , persistentDescriptorCount(persistentCount)
        , persistentHead(0)
        , maxPerFrameDescriptorCount(count)
        , frameUsedDescriptorCount()
        , cpuDescriptorHandle()
        , gpuDescriptorHandle()
    {
        totalDescriptorCount = persistentCount + (count * RG_MAX_FRAMES_IN_FLIGHT);
        descriptorHeap = createDescriptorHeap(type, totalDescriptorCount, flags);

        cpuDescriptorHandle = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
        if(flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        {
            gpuDescriptorHandle = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
        }

        descriptorIncrementSize = getDevice()->GetDescriptorHandleIncrementSize(type);
    }

    void beginFrame()
    {
        // Free and release unused indices back to allocation pool
        frameUsedDescriptorCount[g_FrameIndex] = 0;

        for(rgInt i = 0, l = (rgInt)persistentIndicesToFree[g_FrameIndex].size(); i < l; ++i)
        {
            releasePersistentDescriptorIndex(persistentIndicesToFree[g_FrameIndex][i]);
        }
        persistentIndicesToFree[g_FrameIndex].clear();

        // 
        tailOffset = persistentDescriptorCount + (maxPerFrameDescriptorCount * g_FrameIndex);
        headOffset = tailOffset;
    }

    rgU32 allocatePersistentDescriptor()
    {
        rgU32 index = allocatePersistentDescriptorIndex();
        return index;
    }

    rgU32 allocatePersistentDescriptorRange(rgU32 count)
    {
        rgU32 descriptorStartOffset;

        // TODO: We can't free range when count > 1
        // Create a Range(startoffset, count) based allocation system
        // where we can also break free ranges into smaller ranges.
        if(freePersistentIndices.empty() || count > 1)
        {
            // check if the persistent range is full
            rgAssert(persistentHead <= persistentDescriptorCount);

            descriptorStartOffset = persistentHead;
            persistentHead += count;
        }
        else
        {
            descriptorStartOffset = freePersistentIndices.back();
            freePersistentIndices.pop_back();
        }
        return descriptorStartOffset;
    }

    rgU32 allocateDescriptorRange(rgU32 count)
    {
        // TODO: verify these checks don't allow descriptor overwrite
        rgAssert(frameUsedDescriptorCount[g_FrameIndex] + count <= maxPerFrameDescriptorCount);
        rgAssert(headOffset + count <= (persistentDescriptorCount + (maxPerFrameDescriptorCount * (g_FrameIndex + 1))));

        rgU32 descriptorStartOffset = headOffset;
        headOffset += count;
        frameUsedDescriptorCount[g_FrameIndex] += count;

        return descriptorStartOffset;
    }

    void releasePersistentDescriptor(rgU32 index)
    {
        releasePersistentDescriptorIndex(index);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle(rgU32 index)
    {
        if(index >= totalDescriptorCount)
        {
            rgAssert(!"Descriptor index out of range");
        }

        D3D12_GPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = gpuDescriptorHandle.ptr + index * descriptorIncrementSize;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle(rgU32 index)
    {
        if(index >= totalDescriptorCount)
        {
            rgAssert(!"Descriptor index out of range");
        }

        D3D12_CPU_DESCRIPTOR_HANDLE handle;
        handle.ptr = (SIZE_T)(cpuDescriptorHandle.ptr + index * descriptorIncrementSize);
        return handle;
    }

    ID3D12DescriptorHeap* getHeap()
    {
        return descriptorHeap.Get();
    }

protected:
    
    rgU32 allocatePersistentDescriptorIndex()
    {
        rgU32 descriptorOffset;
        if(freePersistentIndices.empty())
        {
            // check if the persistent range is full
            rgAssert(persistentHead <= persistentDescriptorCount);

            descriptorOffset = persistentHead;
            persistentHead += 1;
        }
        else
        {
            descriptorOffset = freePersistentIndices.back();
            freePersistentIndices.pop_back();
        }
        return descriptorOffset;
    }

    void releasePersistentDescriptorIndex(rgU32 index)
    {
        freePersistentIndices.push_back(index);
    }
    
    ComPtr<ID3D12DescriptorHeap>    descriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE     cpuDescriptorHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE     gpuDescriptorHandle;
    uint32_t                        descriptorIncrementSize;

    rgU32                   descriptorCount;
    rgU32                   persistentDescriptorCount;
    
    rgU32                   headOffset;
    rgU32                   tailOffset;
    rgU32                   persistentHead;

    eastl::vector<rgU32>    freePersistentIndices;
    eastl::vector<rgU32>    persistentIndicesToFree[RG_MAX_FRAMES_IN_FLIGHT];

    rgU32                   totalDescriptorCount;
    rgU32                   maxPerFrameDescriptorCount;
    rgU32                   frameUsedDescriptorCount[RG_MAX_FRAMES_IN_FLIGHT];
};

DescriptorAllocator* cbvSrvUavDescriptorAllocator;
DescriptorAllocator* samplerDescriptorAllocator;

DescriptorAllocator* stagedCbvSrvUavDescriptorAllocator;
DescriptorAllocator* stagedSamplerDescriptorAllocator;

//*****************************************************************************
// Buffer Implementation
//*****************************************************************************

void Buffer::create(char const* tag, MemoryType memoryType, void* buf, rgSize size, BufferUsage usage, Buffer* obj)
{
    rgAssert(size > 0);

    ComPtr<ID3D12Resource> bufferResource;

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
    D3D12_RESOURCE_STATES afterState = D3D12_RESOURCE_STATE_COMMON;
    if(memoryType == GfxMemoryType_Default)
    {
        if(buf == nullptr)
        {
            initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        else
        {
            initialState = D3D12_RESOURCE_STATE_COPY_DEST;
        }
    }
    else if(memoryType == GfxMemoryType_Upload)
    {
        initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
    }

    CD3DX12_HEAP_PROPERTIES heapProps(memoryType == GfxMemoryType_Default ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size, initialState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE);
    BreakIfFail(getDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&bufferResource)
    ));

    setDebugName(bufferResource, tag);

    if(buf != nullptr)
    {
        if(memoryType == GfxMemoryType_Upload)
        {
            void* mappedPtr = nullptr;
            CD3DX12_RANGE mapRange(0, 0); // map whole buffer
            BreakIfFail(bufferResource->Map(0, &mapRange, &mappedPtr));
            memcpy(mappedPtr, buf, size);
            bufferResource->Unmap(0, nullptr);
        }
        else if(memoryType == GfxMemoryType_Default)
        {
            D3D12_SUBRESOURCE_DATA initData = { buf, 0, 0 };
            resourceUploader->Upload(bufferResource.Get(), 0, &initData, 1);
        }
    }

    obj->d3dResource = bufferResource;
}

void Buffer::destroy(Buffer* obj)
{
#if defined(ENABLE_SLOW_GFX_RESOURCE_VALIDATIONS)
    // Check is this resource has pending copy task
    for(auto& itr : pendingBufferCopyTasks)
    {
        if(itr.dst == obj->d3dResource)
        {
            rgAssert(!"Buffer resource has a pending copy task");
        }
    }
#endif
}

void* Buffer::map(rgU32 rangeBeginOffset, rgU32 rangeSizeInBytes)
{
    void* mappedPtr = nullptr;
    mappedRange = CD3DX12_RANGE(rangeBeginOffset, rangeBeginOffset + rangeSizeInBytes);
    BreakIfFail(d3dResource->Map(0, &mappedRange, &mappedPtr));
    return mappedPtr;
}

void Buffer::unmap()
{
    d3dResource->Unmap(0, &mappedRange);
}


//*****************************************************************************
// SamplerState Implementation
//*****************************************************************************

void SamplerState::create(char const* tag, SamplerAddressMode rstAddressMode, SamplerMinMagFilter minFilter, SamplerMinMagFilter magFilter, SamplerMipFilter mipFilter, rgBool anisotropy, SamplerState* obj)
{
    auto toD3DFilter = [minFilter, magFilter, mipFilter, anisotropy]() -> D3D12_FILTER
    {
        if(!anisotropy)
        {
            if(minFilter == GfxSamplerMinMagFilter_Nearest)
            {
                if(magFilter == GfxSamplerMinMagFilter_Nearest)
                {
                    if(mipFilter == GfxSamplerMipFilter_Nearest)
                    {
                        return D3D12_FILTER_MIN_MAG_MIP_POINT;
                    }
                    else if(mipFilter == GfxSamplerMipFilter_Linear)
                    {
                        return D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
                    }
                }
                else if(magFilter == GfxSamplerMinMagFilter_Linear)
                {
                    if(mipFilter == GfxSamplerMipFilter_Nearest)
                    {
                        return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
                    }
                    else if(mipFilter == GfxSamplerMipFilter_Linear)
                    {
                        return D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;
                    }
                }
            }
            else if(minFilter == GfxSamplerMinMagFilter_Linear)
            {
                if(magFilter == GfxSamplerMinMagFilter_Nearest)
                {
                    if(mipFilter == GfxSamplerMipFilter_Nearest)
                    {
                        return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
                    }
                    else if(mipFilter == GfxSamplerMipFilter_Linear)
                    {
                        return D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
                    }
                }
                else if(magFilter == GfxSamplerMinMagFilter_Linear)
                {
                    if(mipFilter == GfxSamplerMipFilter_Nearest)
                    {
                        return D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
                    }
                    else if(mipFilter == GfxSamplerMipFilter_Linear)
                    {
                        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                    }
                }
            }
        }
        else
        {
            // TODO: Review the anisotropy related enums
            // TODO: Review the anisotropy related enums
            if(minFilter == GfxSamplerMinMagFilter_Nearest
                && magFilter == GfxSamplerMinMagFilter_Nearest
                && mipFilter == GfxSamplerMipFilter_Nearest)
            {
                return D3D12_FILTER_MIN_MAG_ANISOTROPIC_MIP_POINT;
            }
            else if(minFilter == GfxSamplerMinMagFilter_Linear
                && magFilter == GfxSamplerMinMagFilter_Linear
                && mipFilter == GfxSamplerMipFilter_Linear)
            {
                return D3D12_FILTER_ANISOTROPIC;
            }
            else
            {
                rgAssert(!"Unrecognized filtering and anisotropy combination");
            }
        }

        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    };

    D3D12_TEXTURE_ADDRESS_MODE addressMode = toD3DTextureAddressMode(rstAddressMode);
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = toD3DFilter();
    samplerDesc.AddressU = addressMode;
    samplerDesc.AddressV = addressMode;
    samplerDesc.AddressW = addressMode;
    samplerDesc.MaxAnisotropy = anisotropy ? 16 : 1;

    rgU32 descriptorIndex = stagedSamplerDescriptorAllocator->allocatePersistentDescriptor();
    getDevice()->CreateSampler(&samplerDesc, stagedSamplerDescriptorAllocator->getCpuHandle(descriptorIndex));
    obj->d3dStagedDescriptorIndex = descriptorIndex;
}

void SamplerState::destroy(SamplerState* obj)
{
    stagedSamplerDescriptorAllocator->releasePersistentDescriptor(obj->d3dStagedDescriptorIndex);
}


//*****************************************************************************
// Texture Implementation
//*****************************************************************************

ComPtr<ID3D12Resource> createTextureResource(char const* tag, TextureDim dim, rgUInt width, rgUInt height, TinyImageFormat format, TextureMipFlag mipFlag, TextureUsage usage, ImageSlice* slices)
{
    DXGI_FORMAT textureFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(format);
    rgUInt mipmapLevelCount = Texture::calcMipmapCount(mipFlag, width, height);

    D3D12_CLEAR_VALUE* clearValue = nullptr;
    D3D12_CLEAR_VALUE initialClearValue = {};
    initialClearValue.Format = textureFormat;

    if(usage & GfxTextureUsage_DepthStencil)
    {
        initialClearValue.DepthStencil = { 1.0f, 0 };
        clearValue = &initialClearValue;
    }
    else if(usage & GfxTextureUsage_RenderTarget)
    {
        initialClearValue.Color[0] = 0;
        initialClearValue.Color[1] = 0;
        initialClearValue.Color[2] = 0;
        initialClearValue.Color[3] = 0;
        clearValue = &initialClearValue;
    }

    rgBool isUAV = (usage & GfxTextureUsage_ShaderReadWrite);
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES initialState = (slices == nullptr) ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_COPY_DEST;

    if(usage & GfxTextureUsage_RenderTarget)
    {
        rgAssert(slices == nullptr);
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        initialState |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if(usage & GfxTextureUsage_DepthStencil)
    {
        rgAssert(slices == nullptr);
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        initialState |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }

    if(usage & GfxTextureUsage_ShaderReadWrite)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        initialState |= (slices == nullptr) ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_COMMON;
    }

    CD3DX12_RESOURCE_DESC resourceDesc = {};
    switch(dim)
    {
    case GfxTextureDim_2D:
    case GfxTextureDim_Cube:
    {
        rgU16 arraySize = (dim == GfxTextureDim_Cube) ? 6 : 1;
        resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(textureFormat, width, height, arraySize, mipmapLevelCount, 1, 0, resourceFlags);
    } break;

    case GfxTextureDim_Buffer:
    {
        resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(width, resourceFlags);
    } break;

    default:
        rgAssert(!"Unsupported texture dim");
    }

    ComPtr<ID3D12Resource> texRes;
    BreakIfFail(getDevice()->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        clearValue,
        IID_PPV_ARGS(&texRes)));

    setDebugName(texRes, tag);

    if(slices != nullptr)
    {
        // TODO: handle when mipmapLevelCount > 1 but mipFlag is not GenMips
        // i.e. copy mip data from slices to texture memory
        if(mipmapLevelCount > 1)
        {
            rgAssert(mipFlag == GfxTextureMipFlag_GenMips);
        }
        rgUInt sliceCount = dim == GfxTextureDim_Cube ? 6 : 1;

        D3D12_SUBRESOURCE_DATA subResourceData[6] = {};
        for(rgUInt s = 0; s < sliceCount; ++s)
        {
            subResourceData[s].pData = slices[s].pixels;
            subResourceData[s].RowPitch = slices[s].rowPitch;
            subResourceData[s].SlicePitch = slices[s].slicePitch;
        }

        resourceUploader->Upload(texRes.Get(), 0, subResourceData, sliceCount);

        if(mipFlag == GfxTextureMipFlag_GenMips)
        {
            resourceUploader->Transition(texRes.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            resourceUploader->GenerateMips(texRes.Get());
        }

        if(isUAV)
        {
            rgAssert(!"Unhandled case!");
            //gfx::resourceUploader->Transition(texRes.Get(), initialState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }

    return texRes;
}

void Texture::create(char const* tag, TextureDim dim, rgUInt width, rgUInt height, TinyImageFormat format, TextureMipFlag mipFlag, TextureUsage usage, ImageSlice* slices, Texture* obj)
{
    obj->d3dTexture = createTextureResource(tag, dim, width, height, format, mipFlag, usage, slices);
}

void Texture::destroy(Texture* obj)
{
}


//*****************************************************************************
// GraphicsPSO Implementation
//*****************************************************************************

void reflectShader(ID3D12ShaderReflection* shaderReflection, eastl::vector<CD3DX12_DESCRIPTOR_RANGE1>& cbvSrvUavDescTableRanges, eastl::vector<CD3DX12_DESCRIPTOR_RANGE1>& samplerDescTableRanges, eastl::hash_map<eastl::string, PipelineArgument>* outArguments, rgBool* outHasBindlessTexture2D)
{
    D3D12_SHADER_DESC shaderDesc = { 0 };
    shaderReflection->GetDesc(&shaderDesc);

    auto isDescriptorAlreadyInRange = [&outArguments](D3D12_SHADER_INPUT_BIND_DESC const& shaderInputBindDesc) -> bool
    {
        if(outArguments->count(shaderInputBindDesc.Name))
        {
            auto existingSameNameResInfoIter = outArguments->find(shaderInputBindDesc.Name);
            if(existingSameNameResInfoIter->second.type == toGfxPipelineArgumentType(shaderInputBindDesc))
            {
                return true;
            }
        }

        return false;
    };

    auto shaderInputTypeToDescriptorRangeType = [](D3D_SHADER_INPUT_TYPE sit) -> D3D12_DESCRIPTOR_RANGE_TYPE
    {
        D3D12_DESCRIPTOR_RANGE_TYPE out;
        switch(sit)
        {
        case D3D_SIT_CBUFFER:
        {
            out = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        } break;

        case D3D_SIT_TEXTURE:
        case D3D_SIT_TBUFFER:
        case D3D_SIT_STRUCTURED:
        case D3D_SIT_BYTEADDRESS:
        case D3D_SIT_RTACCELERATIONSTRUCTURE:
        {
            out = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        } break;

        case D3D_SIT_UAV_RWTYPED:
        case D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SIT_UAV_RWBYTEADDRESS:
        case D3D_SIT_UAV_APPEND_STRUCTURED:
        case D3D_SIT_UAV_CONSUME_STRUCTURED:
        case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
        case D3D_SIT_UAV_FEEDBACKTEXTURE:
        {
            out = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        } break;

        case D3D_SIT_SAMPLER:
        {
            out = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        } break;

        default:
        {
            rgAssert("Unknown shader input type");
        } break;
        }

        return out;
    };

    // https://rtarun9.github.io/blogs/shader_reflection/
    // https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html

    for(rgU32 i = 0; i < shaderDesc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc = { 0 };
        BreakIfFail(shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc));
        D3D12_DESCRIPTOR_RANGE_TYPE descRangeType = shaderInputTypeToDescriptorRangeType(shaderInputBindDesc.Type);

        // NOTE: descriptor are visible to all shader stages.
        // so we don't need to create separate range for all the 
        // stages the resource binding appears in.

        if(isDescriptorAlreadyInRange(shaderInputBindDesc))
        {
            continue;
        }

        if(shaderInputBindDesc.BindCount < 1)
        {
            if(shaderInputBindDesc.Type == D3D_SIT_TEXTURE && shaderInputBindDesc.Dimension == D3D_SRV_DIMENSION_TEXTURE2D)
            {
                *outHasBindlessTexture2D = true;
                continue;
            }

            rgAssert("Only Texture2D bindless resources are supported");
        }

        PipelineArgument arg = {};
        strncpy(arg.tag, shaderInputBindDesc.Name, rgARRAY_COUNT(PipelineArgument::tag));
        arg.stages = GfxStage_VS; // TODO: fill correct stage info
        arg.type = toGfxPipelineArgumentType(shaderInputBindDesc);
        arg.registerIndex = shaderInputBindDesc.BindPoint;
        arg.spaceIndex = shaderInputBindDesc.Space;
        arg.d3dOffsetInDescriptorTable = (descRangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER) ? (rgU32)samplerDescTableRanges.size() : (rgU32)cbvSrvUavDescTableRanges.size();

        (*outArguments)[shaderInputBindDesc.Name] = arg;

        // PERF: this flag can be optimized
        // https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#descriptor-range-flags
        D3D12_DESCRIPTOR_RANGE_FLAGS rangeFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
        if(descRangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
        {
            rangeFlags |= D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
        }

        CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(descRangeType,
            shaderInputBindDesc.BindCount,
            shaderInputBindDesc.BindPoint,
            shaderInputBindDesc.Space,
            rangeFlags);

        switch(descRangeType)
        {
            case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
            case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
            case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
            {
                cbvSrvUavDescTableRanges.push_back(descriptorRange);
            } break;

            case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
            {
                samplerDescTableRanges.push_back(descriptorRange);
            } break;
        }
    }
}

void GraphicsPSO::create(char const* tag, VertexInputDesc* vertexInputDesc, ShaderDesc* shaderDesc, RenderStateDesc* renderStateDesc, GraphicsPSO* obj)
{
    // compile shader
    gfx::ShaderBlobRef vertexShader, fragmentShader;
    if(shaderDesc->vsEntrypoint && shaderDesc->fsEntrypoint)
    {
        vertexShader = gfx::createShaderBlob(shaderDesc->shaderSrc, GfxStage_VS, shaderDesc->vsEntrypoint, shaderDesc->defines, false);
        fragmentShader = gfx::createShaderBlob(shaderDesc->shaderSrc, GfxStage_FS, shaderDesc->fsEntrypoint, shaderDesc->defines, false);
    }

    // do shader reflection
    eastl::vector<D3D12_ROOT_PARAMETER1> rootParameters;
    eastl::vector<CD3DX12_DESCRIPTOR_RANGE1> cbvSrvUavDescTableRanges;
    eastl::vector<CD3DX12_DESCRIPTOR_RANGE1> samplerDescTableRanges;
    rgBool hasBindlessTexture2D;

    reflectShader((ID3D12ShaderReflection*)vertexShader->d3d12ShaderReflection, cbvSrvUavDescTableRanges, samplerDescTableRanges, &obj->arguments, &hasBindlessTexture2D);
    reflectShader((ID3D12ShaderReflection*)fragmentShader->d3d12ShaderReflection, cbvSrvUavDescTableRanges, samplerDescTableRanges, &obj->arguments, &hasBindlessTexture2D);

    // ROOT PARAMETER INDEX 0
    if(cbvSrvUavDescTableRanges.size() > 0)
    {
        D3D12_ROOT_PARAMETER1 rootParameter = {};
        rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameter.DescriptorTable.NumDescriptorRanges = (UINT)cbvSrvUavDescTableRanges.size();
        rootParameter.DescriptorTable.pDescriptorRanges = cbvSrvUavDescTableRanges.data();
        rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(rootParameter);

        obj->d3dHasCBVSRVUAVs = true;
    }

    // ROOT PARAMETER INDEX 1
    if(samplerDescTableRanges.size() > 0)
    {
        D3D12_ROOT_PARAMETER1 rootParameter = {};
        rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameter.DescriptorTable.NumDescriptorRanges = (UINT)samplerDescTableRanges.size();
        rootParameter.DescriptorTable.pDescriptorRanges = samplerDescTableRanges.data();
        rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(rootParameter);

        obj->d3dHasSamplers = true;
    }

    // ROOT PARAMETER INDEX 2
    // bindless texture descriptor range and root parameter
    if(hasBindlessTexture2D)
    {
        CD3DX12_DESCRIPTOR_RANGE1 descRangeBindlessTexture2D(D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            RG_MAX_BINDLESS_TEXTURE_RESOURCES,
            0,
            kBindlessTexture2DBindSpace,
            // PERF: this flag can be optimized
            D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        D3D12_ROOT_PARAMETER1 bindlessTexture2DRootParam = {};
        bindlessTexture2DRootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        bindlessTexture2DRootParam.DescriptorTable.NumDescriptorRanges = 1;
        bindlessTexture2DRootParam.DescriptorTable.pDescriptorRanges = &descRangeBindlessTexture2D;
        bindlessTexture2DRootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(bindlessTexture2DRootParam);

        obj->d3dHasBindlessResources = hasBindlessTexture2D;
    }

    // This works because each range has only 1 descriptor
    obj->d3dCbvSrvUavDescriptorCount = cbvSrvUavDescTableRanges.size();
    obj->d3dSamplerDescriptorCount = samplerDescTableRanges.size();

    // create root signature
    ComPtr<ID3D12RootSignature> rootSig;
    {
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc((UINT)rootParameters.size(),
            rootParameters.data(),
            0,
            nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        BreakIfFail(D3D12SerializeVersionedRootSignature(&rootSigDesc, &signature, &error));
        if(error)
        {
            rgLogError("RootSignature serialization error: %s", error->GetBufferPointer());
        }
        BreakIfFail(getDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(rootSig), (void**)&(rootSig)));
        
        eastl::string rootSigDebugName = eastl::string(tag) + "_rootsig";
        setDebugName(rootSig, rootSigDebugName.c_str());
    }
    obj->d3dRootSignature = rootSig;

    // create vertex input desc
    eastl::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc;
    if(vertexInputDesc && vertexInputDesc->elementCount > 0)
    {
        inputElementDesc.reserve(vertexInputDesc->elementCount);
        {
            rgU32 vertexStrideInBytes = 0;
            for(rgInt i = 0; i < vertexInputDesc->elementCount; ++i)
            {
                auto& elementDesc = vertexInputDesc->elements[i];
                vertexStrideInBytes += TinyImageFormat_BitSizeOfBlock(elementDesc.format) / 8;

                D3D12_INPUT_ELEMENT_DESC e = {};
                e.SemanticName = elementDesc.semanticName;
                e.SemanticIndex = elementDesc.semanticIndex;
                e.Format = toDXGIFormat(elementDesc.format);
                e.InputSlot = elementDesc.bufferIndex;
                e.AlignedByteOffset = elementDesc.offset;
                e.InputSlotClass = elementDesc.stepFunc == GfxVertexStepFunc_PerInstance ?
                    D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                e.InstanceDataStepRate = elementDesc.stepFunc == GfxVertexStepFunc_PerInstance ? 1 : 0;
                inputElementDesc.push_back(e);
            }
            obj->d3dVertexStrideInBytes = vertexStrideInBytes;
        }
    }

    // create pso
    ComPtr<ID3D12PipelineState> pso;
    D3D12_FILL_MODE fillMode = renderStateDesc->triangleFillMode == GfxTriangleFillMode_Fill ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
    D3D12_CULL_MODE cullMode = renderStateDesc->cullMode == GfxCullMode_None ? D3D12_CULL_MODE_NONE : (renderStateDesc->cullMode == GfxCullMode_Back ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT);
    BOOL frontCounterClockwise = renderStateDesc->winding == GfxWinding_CCW ? TRUE : FALSE;

    CD3DX12_RASTERIZER_DESC rasterDesc(D3D12_DEFAULT);
    rasterDesc.FillMode = fillMode;
    rasterDesc.CullMode = cullMode;
    rasterDesc.FrontCounterClockwise = frontCounterClockwise;

    BOOL depthTestEnabled = renderStateDesc->depthStencilAttachmentFormat != TinyImageFormat_UNDEFINED ? TRUE : FALSE;
    D3D12_DEPTH_WRITE_MASK depthWriteMask = renderStateDesc->depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    D3D12_COMPARISON_FUNC depthComparisonFunc = toD3DCompareFunc(renderStateDesc->depthCompareFunc);

    // d3d12 pso create start
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSig.Get();
    if(inputElementDesc.size() > 0)
    {
        psoDesc.InputLayout = { &inputElementDesc.front(), (rgUInt)inputElementDesc.size() };
    }

    // shaders
    psoDesc.VS.pShaderBytecode = vertexShader->shaderObjectBufferPtr;
    psoDesc.VS.BytecodeLength = vertexShader->shaderObjectBufferSize;
    psoDesc.PS.pShaderBytecode = fragmentShader->shaderObjectBufferPtr;
    psoDesc.PS.BytecodeLength = fragmentShader->shaderObjectBufferSize;

    // TODO: Blendstate
    D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
    renderTargetBlendDesc.BlendEnable = TRUE;
    renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    renderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
    renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
    renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
    renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    D3D12_BLEND_DESC blendDesc = {};
    for(rgInt i = 0; i < kMaxColorAttachments; ++i)
    {
        blendDesc.RenderTarget[i] = renderTargetBlendDesc;
    }

    // render state
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.BlendState = blendDesc;
    psoDesc.DepthStencilState.DepthEnable = depthTestEnabled;
    psoDesc.DepthStencilState.DepthWriteMask = depthWriteMask;
    psoDesc.DepthStencilState.DepthFunc = depthComparisonFunc;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = toDXGIFormat(renderStateDesc->depthStencilAttachmentFormat);

    // pso rendertarget
    psoDesc.NumRenderTargets = 0;
    for(rgInt i = 0; i < rgARRAY_COUNT(RenderStateDesc::colorAttachments); ++i)
    {
        if(renderStateDesc->colorAttachments[i].pixelFormat != TinyImageFormat_UNDEFINED)
        {
            psoDesc.RTVFormats[i] = toDXGIFormat(renderStateDesc->colorAttachments[i].pixelFormat);
            ++psoDesc.NumRenderTargets;
        }
        else
        {
            break;
        }
    }

    BreakIfFail(getDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
    setDebugName(pso.Get(), tag);

    obj->d3dPSO = pso;
}

void GraphicsPSO::destroy(GraphicsPSO* obj)
{
}

//*****************************************************************************
// ComputePSO Implementation
//*****************************************************************************

void ComputePSO::create(const char* tag, ShaderDesc* shaderDesc, ComputePSO* obj)
{
}

void ComputePSO::destroy(ComputePSO* obj)
{
}

//*****************************************************************************
// RenderCmdEncoder Implementation
//*****************************************************************************

void RenderCmdEncoder::begin(char const* tag, RenderPass* renderPass)
{
    // enabled this vvv later
    //d3dGraphicsCommandlist = createGraphicsCommandList(getCommandAllocator(), nullptr);

    // TODO REF: https://github.com/gpuweb/gpuweb/issues/23
    //d3dGraphicsCommandlist->OMSetRenderTargets()
}

void RenderCmdEncoder::end()
{
}

//-----------------------------------------------------------------------------
void RenderCmdEncoder::pushDebugTag(const char* tag)
{
}

void RenderCmdEncoder::popDebugTag()
{
}

//-----------------------------------------------------------------------------
void RenderCmdEncoder::setViewport(rgFloat4 viewport)
{
    setViewport(viewport.x, viewport.y, viewport.z, viewport.w);
}

void RenderCmdEncoder::setViewport(rgFloat originX, rgFloat originY, rgFloat width, rgFloat height)
{
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = originX;
    viewport.TopLeftY = originY;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    currentCommandList->RSSetViewports(1, &viewport);
}

void RenderCmdEncoder::setScissorRect(rgU32 xPixels, rgU32 yPixels, rgU32 widthPixels, rgU32 heightPixels)
{
}

//-----------------------------------------------------------------------------
void RenderCmdEncoder::setGraphicsPSO(GraphicsPSO* pso)
{
    gfx::currentGraphicsPSO = pso;

    currentCommandList->SetPipelineState(pso->d3dPSO.Get());
    currentCommandList->SetGraphicsRootSignature(pso->d3dRootSignature.Get());

    pipelineCbvSrvUavDescriptorRangeOffset = cbvSrvUavDescriptorAllocator->allocateDescriptorRange(pso->d3dCbvSrvUavDescriptorCount);
    pipelineSamplerDescriptorRangeOffset = samplerDescriptorAllocator->allocateDescriptorRange(pso->d3dSamplerDescriptorCount);

    // cbv srv uav descriptor table
    if(gfx::currentGraphicsPSO->d3dHasCBVSRVUAVs)
    {
        currentCommandList->SetGraphicsRootDescriptorTable(CBVSRVUAV_ROOT_PARAMETER_INDEX, cbvSrvUavDescriptorAllocator->getGpuHandle(pipelineCbvSrvUavDescriptorRangeOffset));
    }
    // sampler descriptor table
    if(gfx::currentGraphicsPSO->d3dHasSamplers)
    {
        currentCommandList->SetGraphicsRootDescriptorTable(SAMPLER_ROOT_PARAMETER_INDEX, samplerDescriptorAllocator->getGpuHandle(pipelineSamplerDescriptorRangeOffset));
    }
    // bindless resources are stored in persistent part
    if(gfx::currentGraphicsPSO->d3dHasBindlessResources)
    {
        currentCommandList->SetGraphicsRootDescriptorTable(BINDLESS_CBVSRVUAV_ROOT_PARAMETER_INDEX, cbvSrvUavDescriptorAllocator->getGpuHandle(descriptorRangeOffsetBindlessTexture2D));
    }
}

//-----------------------------------------------------------------------------
void RenderCmdEncoder::setVertexBuffer(const Buffer* buffer, rgU32 offset, rgU32 slot)
{
    // We might be able to directly add offset to gpu address
    // https://www.milty.nl/grad_guide/basic_implementation/d3d12/buffers.html
}

void RenderCmdEncoder::setVertexBuffer(GfxFrameResource const* resource, rgU32 slot)
{
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    vertexBufferView.BufferLocation = resource->d3dResource->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = resource->sizeInBytes;
    vertexBufferView.StrideInBytes = gfx::currentGraphicsPSO->d3dVertexStrideInBytes;
    currentCommandList->IASetVertexBuffers(slot, 1, &vertexBufferView);
}

//-----------------------------------------------------------------------------
PipelineArgument& RenderCmdEncoder::getPipelineArgument(char const* bindingTag)
{
    rgAssert(gfx::currentGraphicsPSO != nullptr);
    rgAssert(bindingTag);

    auto infoIter = gfx::currentGraphicsPSO->arguments.find(bindingTag);
    if(infoIter == gfx::currentGraphicsPSO->arguments.end())
    {
        rgLogError("Can't find the specified bindingTag(%s) in the shaders", bindingTag);
        rgAssert(false);
    }

    PipelineArgument& info = infoIter->second;

    if(((info.stages & GfxStage_VS) != GfxStage_VS) && (info.stages & GfxStage_FS) != GfxStage_FS)
    {
        rgLogError("Resource/Binding(%s) cannot be found in the current pipeline(%s)", bindingTag, gfx::currentGraphicsPSO->tag);
        rgAssert(!"TODO: LogError should stop the execution");
    }

    return info;
}

void RenderCmdEncoder::bindBuffer(char const* bindingTag, Buffer* buffer, rgU32 offset)
{
}

void RenderCmdEncoder::bindBuffer(char const* bindingTag, GfxFrameResource const* resource)
{
    rgAssert(resource != nullptr);
    PipelineArgument& argument = getPipelineArgument(bindingTag);

    if(argument.type = PipelineArgument::Type_ConstantBuffer)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = resource->d3dResource->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = resource->sizeInBytes;

        getDevice()->CreateConstantBufferView(&cbvDesc, cbvSrvUavDescriptorAllocator->getCpuHandle(pipelineCbvSrvUavDescriptorRangeOffset + argument.d3dOffsetInDescriptorTable));
    }
}

//-----------------------------------------------------------------------------
void RenderCmdEncoder::bindSamplerState(char const* bindingTag, SamplerState* sampler)
{
    rgAssert(sampler != nullptr);
    PipelineArgument& argument = getPipelineArgument(bindingTag);

    D3D12_CPU_DESCRIPTOR_HANDLE destDescriptorHandle = samplerDescriptorAllocator->getCpuHandle(pipelineSamplerDescriptorRangeOffset + argument.d3dOffsetInDescriptorTable);
    D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptorhandle = stagedSamplerDescriptorAllocator->getCpuHandle(sampler->d3dStagedDescriptorIndex);
    getDevice()->CopyDescriptorsSimple(1, destDescriptorHandle, srcDescriptorhandle, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

//-----------------------------------------------------------------------------
void RenderCmdEncoder::bindTexture(char const* bindingTag, Texture* texture)
{
}

//-----------------------------------------------------------------------------
void RenderCmdEncoder::drawTexturedQuads(TexturedQuads* quads)
{
    if(quads->size() < 1)
    {
        return;
        rgAssert("No textured quads to draw");
    }

    eastl::vector<gfx::SimpleVertexFormat> vertices;
    gfx::SimpleInstanceParams instanceParams;

    genTexturedQuadVertices(quads, &vertices, &instanceParams);

    GfxFrameResource vertexBufAllocation = gfx::getFrameAllocator()->newBuffer("drawTexturedQuadsVertexBuf", (rgU32)vertices.size() * sizeof(gfx::SimpleVertexFormat), vertices.data());
    GfxFrameResource instanceParamsBuffer = gfx::getFrameAllocator()->newBuffer("instanceParamsCBuffer", sizeof(gfx::SimpleInstanceParams), &instanceParams);

    //-
    // camera
    struct
    {
        rgFloat projection2d[16];
        rgFloat view2d[16];
    } cameraParams;

    copyMatrix4ToFloatArray(cameraParams.projection2d, gfx::makeOrthographicProjectionMatrix(0.0f, (rgFloat)g_WindowInfo.width, (rgFloat)g_WindowInfo.height, 0.0f, 0.1f, 1000.0f));
    copyMatrix4ToFloatArray(cameraParams.view2d, Matrix4::lookAt(Point3(0, 0, 0), Point3(0, 0, -1000.0f), Vector3(0, 1.0f, 0)));

    GfxFrameResource cameraBuffer = gfx::getFrameAllocator()->newBuffer("cameraCBuffer", sizeof(cameraParams), (void*)&cameraParams);

    // --

    bindBuffer("camera", &cameraBuffer);
    bindBuffer("instanceParams", &instanceParamsBuffer);
    bindSamplerState("simpleSampler", gfx::samplerBilinearRepeat);

    setVertexBuffer(&vertexBufAllocation, 0);

    drawTriangles(0, (rgU32)vertices.size(), 1);
}

//-----------------------------------------------------------------------------
void RenderCmdEncoder::drawTriangles(rgU32 vertexStart, rgU32 vertexCount, rgU32 instanceCount)
{
    currentCommandList->DrawInstanced(vertexCount, instanceCount, vertexStart, 0);
}

void RenderCmdEncoder::drawIndexedTriangles(rgU32 indexCount, rgBool is32bitIndex, Buffer const* indexBuffer, rgU32 bufferOffset, rgU32 instanceCount)
{
}

void RenderCmdEncoder::drawIndexedTriangles(rgU32 indexCount, rgBool is32bitIndex, GfxFrameResource const* indexBufferResource, rgU32 instanceCount)
{
}

//*****************************************************************************
// ComputeCmdEncoder Implementation
//*****************************************************************************

void ComputeCmdEncoder::begin(char const* tag)
{
}

void ComputeCmdEncoder::end()
{
}

//-----------------------------------------------------------------------------
void ComputeCmdEncoder::pushDebugTag(char const* tag)
{
}

//-----------------------------------------------------------------------------
void ComputeCmdEncoder::popDebugTag()
{
}

//-----------------------------------------------------------------------------
void ComputeCmdEncoder::setComputePSO(ComputePSO* pso)
{
}

//-----------------------------------------------------------------------------
PipelineArgument* ComputeCmdEncoder::getPipelineArgument(char const* bindingTag)
{
    rgAssert(gfx::currentComputePSO != nullptr);
    rgAssert(bindingTag);

    auto infoIter = gfx::currentComputePSO->arguments.find(bindingTag);
    if(infoIter == gfx::currentComputePSO->arguments.end())
    {
        rgLogWarn("Resource/Binding(%s) cannot be found in the current pipeline(%s)", bindingTag, gfx::currentComputePSO->tag);
        return nullptr;
    }

    rgAssert((infoIter->second.stages & GfxStage_CS) == GfxStage_CS);

    return &infoIter->second;
}

//-----------------------------------------------------------------------------
void ComputeCmdEncoder::bindBuffer(char const* bindingTag, Buffer* buffer, rgU32 offset)
{
}

void ComputeCmdEncoder::bindBuffer(char const* bindingTag, GfxFrameResource const* resource)
{
}

void ComputeCmdEncoder::bindBufferFromData(char const* bindingTag, rgU32 sizeInBytes, void* data)
{
}

void ComputeCmdEncoder::bindTexture(char const* bindingTag, Texture* texture)
{
}

void ComputeCmdEncoder::bindSamplerState(char const* bindingTag, SamplerState* sampler)
{
}

void ComputeCmdEncoder::dispatch(rgU32 threadgroupsGridX, rgU32 threadgroupsGridY, rgU32 threadgroupsGridZ)
{
}

//*****************************************************************************
// BlitCmdEncoder Implementation
//*****************************************************************************

void BlitCmdEncoder::begin(char const* tag)
{
}

void BlitCmdEncoder::end()
{
}

void BlitCmdEncoder::pushDebugTag(const char* tag)
{
}

void BlitCmdEncoder::genMips(Texture* srcTexture)
{
}

void BlitCmdEncoder::copyTexture(Texture* srcTexture, Texture* dstTexture, rgU32 srcMipLevel, rgU32 dstMipLevel, rgU32 mipLevelCount)
{
}

//*****************************************************************************
// Frame Resource Allocator
//*****************************************************************************

void GfxFrameAllocator::create(rgU32 bufferHeapSize, rgU32 nonRTDSTextureHeapSize, rgU32 rtDSTextureHeapSize)
{
    D3D12_HEAP_DESC bufferHeapDesc = {};
    bufferHeapDesc.SizeInBytes = bufferHeapSize;
    bufferHeapDesc.Properties.Type = D3D12_HEAP_TYPE_CUSTOM;
    bufferHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
    bufferHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    bufferHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    bufferHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    BreakIfFail(getDevice()->CreateHeap(&bufferHeapDesc, IID_PPV_ARGS(&d3dBufferHeap)));
    d3dBufferHeap->SetName(L"GfxFrameAllocator::d3dBufferHeap");

    D3D12_HEAP_DESC nonRTDSTextureHeapDesc = {};
    nonRTDSTextureHeapDesc.SizeInBytes = bufferHeapSize;
    nonRTDSTextureHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    nonRTDSTextureHeapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    nonRTDSTextureHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
    BreakIfFail(getDevice()->CreateHeap(&nonRTDSTextureHeapDesc, IID_PPV_ARGS(&d3dNonRTDSTextureHeap)));
    d3dNonRTDSTextureHeap->SetName(L"GfxFrameAllocator::d3dNonRTDSTextureHeap");

    D3D12_HEAP_DESC rtDSTextureHeapDesc = {};
    rtDSTextureHeapDesc.SizeInBytes = bufferHeapSize;
    rtDSTextureHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    rtDSTextureHeapDesc.Alignment = D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT;
    rtDSTextureHeapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
    BreakIfFail(getDevice()->CreateHeap(&rtDSTextureHeapDesc, IID_PPV_ARGS(&d3dRTDSTextureHeap)));
    d3dRTDSTextureHeap->SetName(L"GfxFrameAllocator::d3dRTDSTextureHeap");
}

void GfxFrameAllocator::destroy()
{
}

void GfxFrameAllocator::releaseResources()
{
    rgInt l = (rgInt)d3dResources.size();
    for(rgInt i = 0; i < l; ++i)
    {
        d3dResources[i]->Release();
    }
    d3dResources.clear();
}

GfxFrameResource GfxFrameAllocator::newBuffer(const char* tag, rgU32 size, void* initialData)
{
    rgAssert(size > 0);

    rgU32 alignedSize = core::roundUp(size, 256); // CBVs size needs to be 256 bytes multiple
    rgU32 alignedStartOffset = bumpStorageAligned(alignedSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(alignedSize, D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> bufferResource;
    BreakIfFail(getDevice()->CreatePlacedResource(
        d3dBufferHeap.Get(),
        alignedStartOffset,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&bufferResource)
    ));

    setDebugName(bufferResource, tag);

    if(initialData != nullptr)
    {
        void* mappedPtr = nullptr;
        CD3DX12_RANGE mapRange(0, 0);
        BreakIfFail(bufferResource->Map(0, &mapRange, &mappedPtr));
        memcpy(mappedPtr, initialData, size);
        bufferResource->Unmap(0, nullptr);
    }

    d3dResources.push_back(bufferResource);

    GfxFrameResource output;
    output.type = GfxFrameResource::Type_Buffer;
    output.sizeInBytes = alignedSize;
    output.d3dResource = bufferResource.Get();
    return output;
}

GfxFrameResource GfxFrameAllocator::newTexture2D(const char* tag, void* initialData, rgUInt width, rgUInt height, TinyImageFormat format, TextureUsage usage)
{
    ComPtr<ID3D12Resource> texRes = createTextureResource(tag, GfxTextureDim_2D, width, height, format, GfxTextureMipFlag_1Mip, usage, nullptr);
    d3dResources.push_back(texRes);

    GfxFrameResource output;
    output.type = GfxFrameResource::Type_Texture;
    output.d3dResource = texRes.Get();

    return output;
}

RG_BEGIN_GFX_NAMESPACE
//*****************************************************************************
// Call from main | loop init() destroy() startNextFrame() endFrame()
//*****************************************************************************

void getHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if(SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for(
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter)));
            ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if(SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    if(adapter.Get() == nullptr)
    {
        for(UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if(SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}

rgInt init()
{
    // TODO: correctly initialize d3d12 device https://walbourn.github.io/anatomy-of-direct3d-12-create-device/

    // create factory
    UINT factoryCreateFlag = 0;
#if defined(_DEBUG)
    factoryCreateFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif
    BreakIfFail(CreateDXGIFactory2(factoryCreateFlag, __uuidof(dxgiFactory), (void**)&(dxgiFactory)));

    // create debug validation interface
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugInterface;
    BreakIfFail(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();

    //ComPtr<ID3D12InfoQueue> debugInfoQueue;
    //if(SUCCEEDED(d3d.device.As(&debugInfoQueue)))
    //{
    //    debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    //    debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    //    debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    //}
#endif

    // create device
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    getHardwareAdapter(dxgiFactory.Get(), &hardwareAdapter, true);
    BreakIfFail(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), (void**)&(device)));
    
    // query available features from the driver
    D3D12_FEATURE_DATA_D3D12_OPTIONS features = {};
    BreakIfFail(getDevice()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(features)));
    
    // create command queue
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    BreakIfFail(getDevice()->CreateCommandQueue(&commandQueueDesc, __uuidof(ID3D12CommandQueue), (void**)&(commandQueue)));

    // create swapchain
    DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
    swapchainDesc.Width = g_WindowInfo.width;
    swapchainDesc.Height = g_WindowInfo.height;
    swapchainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchainDesc.Stereo = FALSE;
    swapchainDesc.SampleDesc = { 1, 0 };
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchainDesc.BufferCount = RG_MAX_FRAMES_IN_FLIGHT;
    swapchainDesc.Scaling = DXGI_SCALING_NONE;
    swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapchainDesc.Flags = 0;
    HWND hWnd = ::GetActiveWindow();

    ComPtr<IDXGISwapChain1> dxgiSwapchain1;
    BreakIfFail(dxgiFactory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hWnd,
        &swapchainDesc,
        nullptr,
        nullptr,
        &dxgiSwapchain1
    ));
    BreakIfFail(dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
    BreakIfFail(dxgiSwapchain1.As(&dxgiSwapchain));

    // create swapchain RTV
    rtvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_RTV_DESCRIPTOR, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    rtvDescriptorSize = getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for(rgUInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        ComPtr<ID3D12Resource> texResource;
        BreakIfFail(dxgiSwapchain->GetBuffer(i, IID_PPV_ARGS(&texResource)));
        getDevice()->CreateRenderTargetView(texResource.Get(), nullptr, rtvDescriptorHandle);
        rtvDescriptorHandle.Offset(1, rtvDescriptorSize);

        D3D12_RESOURCE_DESC desc = texResource->GetDesc();
        Texture* texRT = rgNew(Texture);
        strncpy(texRT->tag, "RenderTarget", 32);
        texRT->dim = GfxTextureDim_2D;
        texRT->width = (rgUInt)desc.Width;
        texRT->height = (rgUInt)desc.Height;
        texRT->usage = GfxTextureUsage_RenderTarget;
        texRT->format = TinyImageFormat_FromDXGI_FORMAT((TinyImageFormat_DXGI_FORMAT)desc.Format);
        texRT->d3dTexture = texResource;
        swapchainTextures[i] = texRT;
    }

    dsvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    depthStencilTexture = gfx::texture->create("DepthStencilTarget", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D32_SFLOAT, GfxTextureMipFlag_1Mip, GfxTextureUsage_DepthStencil, nullptr);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
    dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsDesc.Texture2D.MipSlice = 0;
    dsDesc.Flags = D3D12_DSV_FLAG_NONE;
    getDevice()->CreateDepthStencilView(depthStencilTexture->d3dTexture.Get(), &dsDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // create descriptor heaps
    cbvSrvUavDescriptorAllocator = rgNew(DescriptorAllocator)(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 30000, RG_MAX_BINDLESS_TEXTURE_RESOURCES + 100);
    samplerDescriptorAllocator = rgNew(DescriptorAllocator)(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 256, 0);
    stagedCbvSrvUavDescriptorAllocator = rgNew(DescriptorAllocator)(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0, 100000);
    stagedSamplerDescriptorAllocator = rgNew(DescriptorAllocator)(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0, 1024);

    descriptorRangeOffsetBindlessTexture2D = cbvSrvUavDescriptorAllocator->allocatePersistentDescriptorRange(RG_MAX_BINDLESS_TEXTURE_RESOURCES);

    // create command allocators
    for(rgUInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        commandAllocator[i] = createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    // create commandlist
    currentCommandList = createGraphicsCommandList(commandAllocator[0], nullptr);

    // create resource uploader
    resourceUploader = rgNew(ResourceUploadBatch)(getDevice().Get());
    resourceUploader->Begin();

    {
        // Create a fence with initial value 0 which is equal to d3d.nextFrameFenceValues[g_FrameIndex=0]
        BreakIfFail(getDevice()->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(frameFence), (void**)&(frameFence)));
        
        // Create an framefence event
        frameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if(frameFenceEvent == nullptr)
        {
            BreakIfFail(HRESULT_FROM_WIN32(::GetLastError()));
        }
    }

    return 0;
}

void destroy()
{
    waitForGpu();
    ::CloseHandle(frameFenceEvent);
}

rgInt draw()
{
    CD3DX12_VIEWPORT vp(0.0f, 0.0f, (rgFloat)g_WindowInfo.width, (rgFloat)g_WindowInfo.height);
    currentCommandList->RSSetViewports(1, &vp);
    CD3DX12_RECT scissorRect(0, 0, g_WindowInfo.width, g_WindowInfo.height);
    currentCommandList->RSSetScissorRects(1, &scissorRect);

    Texture* currentRenderTarget = swapchainTextures[g_FrameIndex];
    currentCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentRenderTarget->d3dTexture.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), g_FrameIndex, rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    currentCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    currentCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

    const rgFloat clearColor[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    currentCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    return 0;
}

void checkerWaitTillFrameCompleted(rgInt frameIndex)
{
    rgAssert(frameIndex >= 0 && frameIndex < RG_MAX_FRAMES_IN_FLIGHT);
    UINT64 valueToWaitFor = frameFenceValues[frameIndex];
    while(frameFence->GetCompletedValue() < valueToWaitFor)
    {
        BreakIfFail(frameFence->SetEventOnCompletion(valueToWaitFor, frameFenceEvent));
        ::WaitForSingleObject(frameFenceEvent, INFINITE);
    }
}

void startNextFrame()
{
    UINT64 prevFrameFenceValue = (g_FrameIndex != -1) ? frameFenceValues[g_FrameIndex] : 0;

    g_FrameIndex = dxgiSwapchain->GetCurrentBackBufferIndex();

    // check if this next frame is finised on the GPU
    UINT64 nextFrameFenceValueToWaitFor = frameFenceValues[g_FrameIndex];
    while(frameFence->GetCompletedValue() < nextFrameFenceValueToWaitFor)
    {
        BreakIfFail(frameFence->SetEventOnCompletion(nextFrameFenceValueToWaitFor, frameFenceEvent));
        ::WaitForSingleObject(frameFenceEvent, INFINITE);
    }

    // This frame fence value is one more than prev frame fence value
    frameFenceValues[g_FrameIndex] = prevFrameFenceValue + 1;

    gfx::atFrameStart();

    // Reset command allocator and command list
    BreakIfFail(commandAllocator[g_FrameIndex]->Reset());
    BreakIfFail(currentCommandList->Reset(commandAllocator[g_FrameIndex].Get(), NULL));

    // set descriptor related stuff
    cbvSrvUavDescriptorAllocator->beginFrame();
    samplerDescriptorAllocator->beginFrame();
    stagedCbvSrvUavDescriptorAllocator->beginFrame();
    stagedSamplerDescriptorAllocator->beginFrame();

    ID3D12DescriptorHeap* descHeaps[] = { cbvSrvUavDescriptorAllocator->getHeap(), samplerDescriptorAllocator->getHeap() };
    currentCommandList->SetDescriptorHeaps(rgARRAY_COUNT(descHeaps), descHeaps);

    draw();
}

void endFrame()
{
    Texture* currentRenderTarget = swapchainTextures[g_FrameIndex];
    currentCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentRenderTarget->d3dTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    BreakIfFail(currentCommandList->Close());

    auto resourceUploaderFinish = resourceUploader->End(commandQueue.Get());
    resourceUploaderFinish.wait();

    ID3D12CommandList* commandLists[] = { currentCommandList.Get() };
    commandQueue->ExecuteCommandLists(1, commandLists);
    BreakIfFail(dxgiSwapchain->Present(1, 0));

    UINT64 fenceValueToSignal = frameFenceValues[g_FrameIndex];
    BreakIfFail(commandQueue->Signal(frameFence.Get(), fenceValueToSignal));

    resourceUploader->Begin();
}

void onSizeChanged()
{
    waitForGpu();
}

void runOnFrameBeginJob()
{
}

void setterBindlessResource(rgU32 slot, Texture* resource)
{
    // Only 2D bindless textures are supported
    rgAssert(resource->dim == GfxTextureDim_2D);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(resource->format);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = resource->mipmapCount;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    getDevice()->CreateShaderResourceView(resource->d3dTexture.Get(), nullptr, cbvSrvUavDescriptorAllocator->getCpuHandle(descriptorRangeOffsetBindlessTexture2D + slot));
}

void rendererImGuiInit()
{
    rgU32 fontSrvDescriptorindex = cbvSrvUavDescriptorAllocator->allocatePersistentDescriptor();
    ImGui_ImplDX12_Init(getDevice().Get(), RG_MAX_FRAMES_IN_FLIGHT, DXGI_FORMAT_B8G8R8A8_UNORM, cbvSrvUavDescriptorAllocator->getHeap(), cbvSrvUavDescriptorAllocator->getCpuHandle(fontSrvDescriptorindex), cbvSrvUavDescriptorAllocator->getGpuHandle(fontSrvDescriptorindex));
    ImGui_ImplSDL2_InitForD3D(gfx::mainWindow);
}

void rendererImGuiNewFrame()
{
    ImGui_ImplDX12_NewFrame();
}

void rendererImGuiRenderDrawData()
{
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), currentCommandList.Get());
}

Texture* getCurrentRenderTargetColorBuffer()
{
    return swapchainTextures[g_FrameIndex];
}

RG_END_GFX_NAMESPACE

#undef BreakIfFail
RG_END_CORE_NAMESPACE
#endif
