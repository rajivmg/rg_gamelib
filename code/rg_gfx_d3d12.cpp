#if defined(RG_D3D12_RNDR)
#include "rg_gfx.h"

#ifndef _WIN32
#include "ComPtr.hpp"
#endif
#include "dxcapi.h"
#include "d3d12shader.h"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_dx12.h"

#include "ResourceUploadBatch.h"
using namespace DirectX;

#include <EASTL/string.h>
#include <EASTL/hash_set.h>

#include <filesystem>

RG_BEGIN_RG_NAMESPACE

RG_BEGIN_GFX_NAMESPACE
    static rgUInt const MAX_RTV_DESCRIPTOR = 1024;
    static rgUInt const MAX_CBVSRVUAV_DESCRIPTOR = 400000;
    static rgUInt const MAX_SAMPLER_DESCRIPTOR = 1024;

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

    ComPtr<ID3D12DescriptorHeap> samplerDescriptorHeap;
    rgUInt samplerDescriptorSize;
    CD3DX12_CPU_DESCRIPTOR_HANDLE samplerNextCPUDescriptorHandle;

    ComPtr<ID3D12DescriptorHeap> cbvSrvUavDescriptorHeap;
    rgUInt cbvSrvUavDescriptorSize;

    ComPtr<ID3D12CommandAllocator> commandAllocator[RG_MAX_FRAMES_IN_FLIGHT];
    ComPtr<ID3D12GraphicsCommandList> currentCommandList;

    GfxTexture* swapchainTextures[RG_MAX_FRAMES_IN_FLIGHT];
    GfxTexture* depthStencilTexture;

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

    // test --
    ComPtr<ID3D12RootSignature> dummyRootSignature;
    ComPtr<ID3D12PipelineState> dummyPSO;
    ComPtr<ID3D12Resource> triVB;
    D3D12_VERTEX_BUFFER_VIEW triVBView;
    // -- 
RG_END_GFX_NAMESPACE

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
    return gfx::device;
}

inline DXGI_FORMAT toDXGIFormat(TinyImageFormat fmt)
{
    return (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(fmt);
}

inline D3D12_COMPARISON_FUNC toD3DCompareFunc(GfxCompareFunc func)
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

D3D12_TEXTURE_ADDRESS_MODE toD3DTextureAddressMode(GfxSamplerAddressMode rstAddressMode)
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
        rgAssert(!"Invalid GfxSamplerAddressMode value");
    }
    }
    return result;
}

ComPtr<ID3D12CommandAllocator> getCommandAllocator()
{
    return gfx::commandAllocator[g_FrameIndex];
}

ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, rgUInt descriptorsCount, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = descriptorsCount;
    desc.Flags = flags;

    BreakIfFail(getDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

ComPtr<ID3D12CommandAllocator> createCommandAllocator(D3D12_COMMAND_LIST_TYPE commandListType)
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    BreakIfFail(getDevice()->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&commandAllocator)));

    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> createGraphicsCommandList(ComPtr<ID3D12CommandAllocator> commandAllocator, ID3D12PipelineState* pipelineState)
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    BreakIfFail(getDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), pipelineState, IID_PPV_ARGS(&commandList)));
    BreakIfFail(commandList->Close());
    return commandList;
}

void setResourceDebugName(ComPtr<ID3D12Resource> resource, char const* name)
{
    const rgUInt maxWCharLength = 1024;
    wchar_t wc[maxWCharLength];

    size_t size = eastl::CharStrlen(name) + 1;
    rgAssert(size <= maxWCharLength);

    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wc, maxWCharLength, name, _TRUNCATE);

    BreakIfFail(resource->SetName(wc));
};

//struct ResourceUploader
//{
//    struct Resource
//    {
//        enum Type
//        {
//            Type_Texture,
//            Type_Buffer,
//        };
//
//        Type type;
//        rgU32 uploadBufferOffset;
//        union
//        {
//            GfxTexture2D* texture;
//            GfxBuffer* buffer;
//        };
//    };
//
//    ResourceUploader(rgU32 uploadHeapSizeInBytes)
//        : uploadHeapSize(uploadHeapSizeInBytes)
//    {
//        BreakIfFail(device()->CreateCommittedResource(
//            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
//            D3D12_HEAP_FLAG_NONE,
//            &CD3DX12_RESOURCE_DESC::Buffer(uploadHeapSize),
//            D3D12_RESOURCE_STATE_GENERIC_READ,
//            nullptr,
//            IID_PPV_ARGS(&uploadHeap)
//        ));
//
//        CD3DX12_RANGE range(0, 0);
//        uploadHeap->Map(0, &range, (void**)&uploadHeapPtr);
//    }
//
//    void uploadBuffer();
//
//    eastl::vector<Resource> resources;
//    ComPtr<ID3D12Resource> uploadHeap;
//    rgU8* uploadHeapPtr;
//    rgU32 uploadHeapSize;
//};

// TODO: Create a frameallocator and use it as upload heap for texture and buffers
// https://learn.microsoft.com/en-us/windows/win32/direct3d12/upload-and-readback-of-texture-data

#if 0 // not needed here anymore
struct GfxFrameResourceMemory
{
    rgU32 offset;
    void* ptr;
    // TODO: SRV, UAV, CBV
    //id<MTLBuffer> parentBuffer; 
};

class GfxFrameResourceAllocator
{
public:
    GfxFrameResourceAllocator(rgU32 heapSizeInBytes)
        : heapSize(heapSizeInBytes)
    {
        offset = 0;

        BreakIfFail(device()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(heapSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&heap)
        ));

        CD3DX12_RANGE range(0, 0);
        heap->Map(0, &range, (void**)&heapPtr);
    }

    ~GfxFrameResourceAllocator()
    {
        heap->Release();
    }

    void reset()
    {
        offset = 0;
    }

    GfxFrameResourceMemory allocate(char const* tag, rgU32 size)
    {
        rgAssert(offset + size <= capacity);

        //void* ptr = (bufferPtr + offset);

        GfxFrameResourceMemory result;
        result.offset = offset;
        //result.ptr = ptr;
        //result.parentBuffer = buffer;

        rgU32 alignment = 4;
        offset += (size + alignment - 1) & ~(alignment - 1);

        //[buffer addDebugMarker : [NSString stringWithUTF8String : tag] range : NSMakeRange(result.offset, (offset - result.offset))] ;

        return result;
    }

    GfxFrameResourceMemory allocate(char const* tag, rgU32 size, void* initialData)
    {
        GfxFrameResourceMemory result = allocate(tag, size);
        memcpy(result.ptr, initialData, size);
        return result;
    }

protected:
    rgU32 offset;
    rgU32 capacity;
    ComPtr<ID3D12Resource> heap;
    rgU8* heapPtr;
    rgU32 heapSize;
};

static GfxFrameResourceMemory* frameAllocators[RG_MAX_FRAMES_IN_FLIGHT];
#endif 

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

void waitForGpu()
{
    BreakIfFail(gfx::commandQueue->Signal(gfx::frameFence.Get(), gfx::frameFenceValues[g_FrameIndex]));
    BreakIfFail(gfx::frameFence->SetEventOnCompletion(gfx::frameFenceValues[g_FrameIndex], gfx::frameFenceEvent));
    ::WaitForSingleObject(gfx::frameFenceEvent, INFINITE);

    gfx::frameFenceValues[g_FrameIndex] += 1;
}

//*****************************************************************************
// GfxBuffer Implementation
//*****************************************************************************

void GfxBuffer::create(char const* tag, GfxMemoryType memoryType, void* buf, rgSize size, GfxBufferUsage usage, GfxBuffer* obj)
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
    setResourceDebugName(bufferResource, tag);

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
            gfx::resourceUploader->Upload(bufferResource.Get(), 0, &initData, 1);
        }
    }

    obj->d3dResource = bufferResource;
}

void GfxBuffer::destroy(GfxBuffer* obj)
{
#if defined(ENABLE_SLOW_GFX_RESOURCE_VALIDATIONS)
    // Check is this resource has pending copy task
    for(auto& itr : gfx::pendingBufferCopyTasks)
    {
        if(itr.dst == obj->d3dResource)
        {
            rgAssert(!"GfxBuffer resource has a pending copy task");
        }
    }
#endif
}

void* GfxBuffer::map(rgU32 rangeBeginOffset, rgU32 rangeSizeInBytes)
{
    void* mappedPtr = nullptr;
    mappedRange = CD3DX12_RANGE(rangeBeginOffset, rangeBeginOffset + rangeSizeInBytes);
    BreakIfFail(d3dResource->Map(0, &mappedRange, &mappedPtr));
    return mappedPtr;
}

void GfxBuffer::unmap()
{
    d3dResource->Unmap(0, &mappedRange);
}


//*****************************************************************************
// GfxSamplerState Implementation
//*****************************************************************************

void GfxSamplerState::create(char const* tag, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSamplerState* obj)
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

    getDevice()->CreateSampler(&samplerDesc, gfx::samplerNextCPUDescriptorHandle);
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuDescriptorHandle = gfx::samplerNextCPUDescriptorHandle;
    gfx::samplerNextCPUDescriptorHandle.Offset(1, gfx::samplerDescriptorSize);

    obj->d3dCPUDescriptorHandle = cpuDescriptorHandle;
}

void GfxSamplerState::destroy(GfxSamplerState* obj)
{
}


//*****************************************************************************
// GfxTexture Implementation
//*****************************************************************************

void GfxTexture::create(char const* tag, GfxTextureDim dim, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureMipFlag mipFlag, GfxTextureUsage usage, ImageSlice* slices, GfxTexture* obj)
{
    ComPtr<ID3D12Resource> textureResource;

    DXGI_FORMAT textureFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(format);
    rgUInt mipmapLevelCount = calcMipmapCount(mipFlag, width, height);

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
        initialState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS; // TODO: handle this case with texture upload
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

    BreakIfFail(getDevice()->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        clearValue,
        IID_PPV_ARGS(&textureResource)));
    setResourceDebugName(textureResource, tag);

    if(slices != nullptr)
    {
        // TODO: handle when mipmapLevelCount > 1 but mipFlag is not GenMips
        // i.e. copy mip data from slices to texture memory
        if(mipmapLevelCount > 1)
        {
            rgAssert(mipFlag == GfxTextureMipFlag_GenMips);
        }
        rgInt sliceCount = dim == GfxTextureDim_Cube ? 6 : 1;
        
        D3D12_SUBRESOURCE_DATA subResourceData[6] = {};
        for(rgUInt s = 0; s < sliceCount; ++s)
        {
            subResourceData[s].pData = slices->pixels;
            subResourceData[s].RowPitch = slices->rowPitch;
            subResourceData[s].SlicePitch = slices->slicePitch;
        }

        gfx::resourceUploader->Upload(textureResource.Get(), 0, subResourceData, sliceCount);
    }

    obj->d3dTexture = textureResource;
}

void GfxTexture::destroy(GfxTexture* obj)
{
}


//*****************************************************************************
// GfxGraphicsPSO Implementation
//*****************************************************************************

struct BuildShaderResult
{
    ComPtr<IDxcBlob> shaderBlob;
    ComPtr<ID3D12ShaderReflection> shaderReflection;
};

static ComPtr<IDxcUtils> g_DxcUtils;

BuildShaderResult buildShaderBlob(char const* filename, GfxStage stage, char const* entrypoint, char const* defines)
{
    auto getStageStr = [](GfxStage s) -> const char*
    {
        switch(s)
        {
            case GfxStage_VS:
                return "vs";
                break;
            case GfxStage_FS:
                return "fs";
                break;
            case GfxStage_CS:
                return "cs";
                break;
        }
        return nullptr;
    };

    auto checkHR = [&](HRESULT hr) -> void
    {
        if(FAILED(hr))
        {
            rgAssert(!"Operation failed");
        }
    };

    rgAssert(filename);
    rgAssert(entrypoint);

    rgHash hash = rgCRC32(filename);
    hash = rgCRC32(getStageStr(stage), 2, hash);
    hash = rgCRC32(entrypoint, (rgU32)strlen(entrypoint), hash);
    if(defines != nullptr)
    {
        hash = rgCRC32(defines, (rgU32)strlen(defines), hash);
    }

    // entrypoint
    eastl::vector<LPCWSTR> dxcArgs;
    dxcArgs.push_back(L"-E");

    wchar_t entrypointWide[256];
    std::mbstowcs(entrypointWide, entrypoint, 256);
    dxcArgs.push_back(entrypointWide);

    // shader model
    dxcArgs.push_back(L"-T");
    if(stage == GfxStage_VS)
    {
        dxcArgs.push_back(L"vs_6_0");
    }
    else if(stage == GfxStage_FS)
    {
        dxcArgs.push_back(L"ps_6_0");
    }
    else if(stage == GfxStage_CS)
    {
        dxcArgs.push_back(L"cs_6_0");
    }

    // include dir
    dxcArgs.push_back(L"-I");
    dxcArgs.push_back(L"../code/shaders");

    // generate debug symbols
    wchar_t debugSymPath[512];
    wchar_t prefPath[256];
    wchar_t hashWString[128];
    _ultow(hash, hashWString, 16);
    std::mbstowcs(prefPath, rg::getPrefPath(), 256);
    wcsncpy(debugSymPath, prefPath, 280);
    //wcsncat(debugSymPath, L"shaderpdb\\", 10);
    wcsncat(debugSymPath, hashWString, 120);
    wcsncat(debugSymPath, L".bin", 8);
    // TODO: check there is no file with same name

    dxcArgs.push_back(L"-Zi");
    dxcArgs.push_back(L"-Fd");
    dxcArgs.push_back(debugSymPath);

    // defines
    eastl::vector<eastl::wstring> defineArgs;
    if(defines != nullptr)
    {
        char const* definesCursorA = defines;
        char const* definesCursorB = definesCursorA;
        while(*definesCursorB != '\0')
        {
            definesCursorB = definesCursorB + 1;
            if(*definesCursorB == ' ' || *definesCursorB == '\0')
            {
                if(*definesCursorB == '\0' && (definesCursorA == definesCursorB))
                {
                    break;
                }

                wchar_t d[256];
                wchar_t wterm = 0;
                rgUPtr lenWithoutNull = definesCursorB - definesCursorA;
                std::mbstowcs(d, definesCursorA, lenWithoutNull);
                wcsncpy(&d[lenWithoutNull], &wterm, 1);
                defineArgs.push_back(eastl::wstring(d));
                definesCursorA = definesCursorB + 1;
            }
        }
        if(defineArgs.size() > 0)
        {
            dxcArgs.push_back(L"-D");
            for(auto& s : defineArgs)
            {
                dxcArgs.push_back(s.c_str());
            }
        }
    }

    // create include handler
    if(!g_DxcUtils)
    {
        checkHR(DxcCreateInstance(CLSID_DxcUtils, __uuidof(IDxcUtils), (void**)&g_DxcUtils));
    }

    class CustomIncludeHandler : public IDxcIncludeHandler
    {
    public:
        HRESULT LoadSource(LPCWSTR pFilename, IDxcBlob **ppIncludeSource) override
        {

            ComPtr<IDxcBlobEncoding> blobEncoding;
            std::filesystem::path filepath = std::filesystem::absolute(std::filesystem::path(pFilename));
            if(alreadyIncludedFiles.count(eastl::wstring(filepath.string().c_str())) > 0)
            {
                static const char emptyStr[] = " ";
                g_DxcUtils->CreateBlobFromPinned(emptyStr, rgARRAY_COUNT(emptyStr), DXC_CP_ACP, &blobEncoding);
                return S_OK;
            }

            HRESULT hr = g_DxcUtils->LoadFile(pFilename, nullptr, &blobEncoding);
            if(SUCCEEDED(hr))
            {
                alreadyIncludedFiles.insert(eastl::wstring(pFilename));
                *ppIncludeSource = blobEncoding.Detach();
            }

            return hr;
        }

        HRESULT QueryInterface(REFIID riid, void** ppvObject) override { return E_NOINTERFACE; }
        ULONG   AddRef(void) override { return 0; }
        ULONG   Release(void) override { return 0; }

        eastl::hash_set<eastl::wstring> alreadyIncludedFiles;
    };

    ComPtr<CustomIncludeHandler> customIncludeHandler(rgNew(CustomIncludeHandler));

    // load shader file
    char filepath[512];
    strcpy(filepath, "../code/shaders/");
    strncat(filepath, filename, 490);

    rg::FileData shaderFileData = rg::readFile(filepath);
    rgAssert(shaderFileData.isValid);

    DxcBuffer shaderSource;
    shaderSource.Ptr = shaderFileData.data;
    shaderSource.Size = shaderFileData.dataSize;
    shaderSource.Encoding = 0;

    // compile shader
    ComPtr<IDxcCompiler3> compiler3;
    checkHR(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void**)&compiler3));

    ComPtr<IDxcResult> result;
    checkHR(compiler3->Compile(&shaderSource, dxcArgs.data(), (UINT32)dxcArgs.size(), customIncludeHandler.Get(), __uuidof(IDxcResult), (void**)&result));

    // process errors
    ComPtr<IDxcBlobUtf8> errorMsg;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorMsg), nullptr);
    if(errorMsg && errorMsg->GetStringLength())
    {
        rgLogError("***Shader Compile Warn/Error(%s, %s),Defines:%s***\n%s", filename, entrypoint, defines, errorMsg->GetStringPointer());
    }

    //ComPtr<IDxcBlob> shaderHashBlob;
    //checkHR(result->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&shaderHashBlob), nullptr));
    //DxcShaderHash* shaderHashBuffer = (DxcShaderHash*)shaderHashBlob->GetBufferPointer();

    // write pdb to preferred write path
    ComPtr<IDxcBlob> shaderPdbBlob;
    ComPtr<IDxcBlobUtf16> shaderPdbPath;
    checkHR(result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&shaderPdbBlob), shaderPdbPath.GetAddressOf()));

    char pdbFilepath[512];
    wcstombs(pdbFilepath, shaderPdbPath->GetStringPointer(), 512);
    rg::writeFile(pdbFilepath, shaderPdbBlob->GetBufferPointer(), shaderPdbBlob->GetBufferSize());

    // shader blob
    ComPtr<IDxcBlob> shaderBlob;
    checkHR(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr));

    // reflection information
    ComPtr<IDxcBlob> shaderReflectionBlob;
    checkHR(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&shaderReflectionBlob), nullptr));

    DxcBuffer shaderReflectionBuffer;
    shaderReflectionBuffer.Ptr = shaderReflectionBlob->GetBufferPointer();
    shaderReflectionBuffer.Size = shaderReflectionBlob->GetBufferSize();
    shaderReflectionBuffer.Encoding = 0;

    ComPtr<ID3D12ShaderReflection> shaderReflection;
    g_DxcUtils->CreateReflection(&shaderReflectionBuffer, IID_PPV_ARGS(&shaderReflection));

    BuildShaderResult output;
    output.shaderBlob = shaderBlob;
    output.shaderReflection = shaderReflection;

    return output;
}

void reflectShader(ComPtr<ID3D12ShaderReflection> shaderReflection, eastl::vector<CD3DX12_DESCRIPTOR_RANGE1>& cbvSrvUavDescTableRanges, eastl::vector<CD3DX12_DESCRIPTOR_RANGE1>& samplerDescTableRanges, eastl::hash_map<eastl::string, GfxGraphicsPSO::ResourceInfo>& resourceInfo)
{
    D3D12_SHADER_DESC shaderDesc = { 0 };
    shaderReflection->GetDesc(&shaderDesc);

    for(rgU32 i = 0; i < shaderDesc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc = { 0 };
        BreakIfFail(shaderReflection->GetResourceBindingDesc(i, &shaderInputBindDesc));
        switch(shaderInputBindDesc.Type)
        {
            case D3D_SIT_CBUFFER:
            case D3D_SIT_TEXTURE:
            {
                // TODO: BEGIN -- Improve this logic
                // TODO: BEGIN -- Improve this logic
                if(resourceInfo.count(shaderInputBindDesc.Name))
                {
                    auto existingSameNameResInfoIter = resourceInfo.find(shaderInputBindDesc.Name);
                    if(existingSameNameResInfoIter->second.type == shaderInputBindDesc.Type)
                    {
                        continue;
                    }
                    else
                    {
                        rgAssert(!"Same name resouce but type is different");
                    }
                }
                // END

                GfxGraphicsPSO::ResourceInfo info;
                info.type = shaderInputBindDesc.Type;
                info.offsetInDescTable = (rgU16)cbvSrvUavDescTableRanges.size();
                resourceInfo[shaderInputBindDesc.Name] = info;

                D3D12_DESCRIPTOR_RANGE_TYPE descRangeType = shaderInputBindDesc.Type == D3D_SIT_CBUFFER ?
                    D3D12_DESCRIPTOR_RANGE_TYPE_CBV : D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

                // zero means bindless
                rgU32 bindCount = shaderInputBindDesc.BindCount ? shaderInputBindDesc.BindCount : 65536;

                CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(descRangeType,
                    bindCount,
                    shaderInputBindDesc.BindPoint,
                    shaderInputBindDesc.Space,
                    D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

                cbvSrvUavDescTableRanges.push_back(descriptorRange);

                // TODO: https://rtarun9.github.io/blogs/shader_reflection/
                // https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html

                //ID3D12ShaderReflectionConstantBuffer* shaderReflectionConstBuffer = shaderReflection->GetConstantBufferByIndex(i);
                //D3D12_SHADER_BUFFER_DESC constBufferDesc = { 0 };
                //shaderReflectionConstBuffer->GetDesc(&constBufferDesc);
            } break;

            case D3D_SIT_SAMPLER:
            {
                GfxGraphicsPSO::ResourceInfo info;
                info.type = shaderInputBindDesc.Type;
                info.offsetInDescTable = (rgU16)samplerDescTableRanges.size();
                resourceInfo[shaderInputBindDesc.Name] = info;

                CD3DX12_DESCRIPTOR_RANGE1 descriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
                    shaderInputBindDesc.BindCount,
                    shaderInputBindDesc.BindPoint,
                    shaderInputBindDesc.Space,
                    D3D12_DESCRIPTOR_RANGE_FLAG_NONE);
                samplerDescTableRanges.push_back(descriptorRange);
            } break;
        }
    }
}

void GfxGraphicsPSO::create(char const* tag, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO* obj)
{
    // compile shader
    BuildShaderResult vertexShader, fragmentShader;
    if(shaderDesc->vsEntrypoint && shaderDesc->fsEntrypoint)
    {
        vertexShader = buildShaderBlob(shaderDesc->shaderSrc, GfxStage_VS, shaderDesc->vsEntrypoint, shaderDesc->defines);
        fragmentShader = buildShaderBlob(shaderDesc->shaderSrc, GfxStage_FS, shaderDesc->fsEntrypoint, shaderDesc->defines);
    }

    // do shader reflection
    eastl::vector<D3D12_ROOT_PARAMETER1> rootParameters;
    eastl::vector<CD3DX12_DESCRIPTOR_RANGE1> cbvSrvUavDescTableRanges;
    eastl::vector<CD3DX12_DESCRIPTOR_RANGE1> samplerDescTableRanges;

    reflectShader(vertexShader.shaderReflection, cbvSrvUavDescTableRanges, samplerDescTableRanges, obj->d3dResourceInfo);
    reflectShader(fragmentShader.shaderReflection, cbvSrvUavDescTableRanges, samplerDescTableRanges, obj->d3dResourceInfo);

    if(cbvSrvUavDescTableRanges.size() > 0)
    {
        D3D12_ROOT_PARAMETER1 rootParameter = {};
        rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameter.DescriptorTable.NumDescriptorRanges = (UINT)cbvSrvUavDescTableRanges.size();
        rootParameter.DescriptorTable.pDescriptorRanges = cbvSrvUavDescTableRanges.data();
        rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(rootParameter);
    }

    if(samplerDescTableRanges.size() > 0)
    {
        D3D12_ROOT_PARAMETER1 rootParameter = {};
        rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameter.DescriptorTable.NumDescriptorRanges = (UINT)samplerDescTableRanges.size();
        rootParameter.DescriptorTable.pDescriptorRanges = samplerDescTableRanges.data();
        rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        rootParameters.push_back(rootParameter);
    }

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
    }
    obj->d3dRootSignature = rootSig;

    // create vertex input desc
    eastl::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc;
    if(vertexInputDesc && vertexInputDesc->elementCount > 0)
    {
        inputElementDesc.reserve(vertexInputDesc->elementCount);
        {
            for(rgInt i = 0; i < vertexInputDesc->elementCount; ++i)
            {
                auto& elementDesc = vertexInputDesc->elements[i];
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
    psoDesc.VS.pShaderBytecode = vertexShader.shaderBlob->GetBufferPointer();
    psoDesc.VS.BytecodeLength = vertexShader.shaderBlob->GetBufferSize();
    psoDesc.PS.pShaderBytecode = fragmentShader.shaderBlob->GetBufferPointer();
    psoDesc.PS.BytecodeLength = fragmentShader.shaderBlob->GetBufferSize();

    // render state
    psoDesc.RasterizerState = rasterDesc;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // TODO: Blendstate
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
    for(rgInt i = 0; i < rgARRAY_COUNT(GfxRenderStateDesc::colorAttachments); ++i)
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

    // explicitly release shaders
    // TODO: Remove as this is released automatically
    //vertexShader.shaderBlob->Release();
    //fragmentShader.shaderBlob->Release();

    obj->d3dPSO = pso;
}

void GfxGraphicsPSO::destroy(GfxGraphicsPSO* obj)
{
}

//*****************************************************************************
// GfxComputePSO Implementation
//*****************************************************************************

void GfxComputePSO::create(const char* tag, GfxShaderDesc* shaderDesc, GfxComputePSO* obj)
{
}

void GfxComputePSO::destroy(GfxComputePSO* obj)
{
}

//*****************************************************************************
// GfxRenderCmdEncoder Implementation
//*****************************************************************************

void GfxRenderCmdEncoder::begin(char const* tag, GfxRenderPass* renderPass)
{
    // enabled this vvv later
    //d3dGraphicsCommandlist = createGraphicsCommandList(getCommandAllocator(), nullptr);

    // TODO REF: https://github.com/gpuweb/gpuweb/issues/23
    //d3dGraphicsCommandlist->OMSetRenderTargets()
}

void GfxRenderCmdEncoder::end()
{
}

//-----------------------------------------------------------------------------
void GfxRenderCmdEncoder::pushDebugTag(const char* tag)
{
}

void GfxRenderCmdEncoder::popDebugTag()
{
}

//-----------------------------------------------------------------------------
void GfxRenderCmdEncoder::setViewport(rgFloat4 viewport)
{
    setViewport(viewport.x, viewport.y, viewport.z, viewport.w);
}

void GfxRenderCmdEncoder::setViewport(rgFloat originX, rgFloat originY, rgFloat width, rgFloat height)
{
}

void GfxRenderCmdEncoder::setScissorRect(rgU32 xPixels, rgU32 yPixels, rgU32 widthPixels, rgU32 heightPixels)
{
}

//-----------------------------------------------------------------------------
void GfxRenderCmdEncoder::setGraphicsPSO(GfxGraphicsPSO* pso)
{
}

//-----------------------------------------------------------------------------
rgU32 slotToVertexBinding(rgU32 slot)
{
    rgU32 const maxBufferBindIndex = 30;
    rgU32 bindpoint = maxBufferBindIndex - slot;
    rgAssert(bindpoint > 0 && bindpoint < 31);
    return bindpoint;
}

void GfxRenderCmdEncoder::setVertexBuffer(const GfxBuffer* buffer, rgU32 offset, rgU32 slot)
{
}

void GfxRenderCmdEncoder::setVertexBuffer(GfxFrameResource const* resource, rgU32 slot)
{
}

//-----------------------------------------------------------------------------
GfxObjectBinding& GfxRenderCmdEncoder::getPipelineArgumentInfo(char const* bindingTag)
{
    rgAssert(gfx::currentGraphicsPSO != nullptr);
    rgAssert(bindingTag);

    auto infoIter = gfx::currentGraphicsPSO->reflection.find(bindingTag);
    if(infoIter == gfx::currentGraphicsPSO->reflection.end())
    {
        rgLogError("Can't find the specified bindingTag(%s) in the shaders", bindingTag);
        rgAssert(false);
    }

    GfxObjectBinding& info = infoIter->second;

    if(((info.stages & GfxStage_VS) != GfxStage_VS) && (info.stages & GfxStage_FS) != GfxStage_FS)
    {
        rgLogError("Resource/Binding(%s) cannot be found in the current pipeline(%s)", bindingTag, gfx::currentGraphicsPSO->tag);
        rgAssert(!"TODO: LogError should stop the execution");
    }

    return info;
}

void GfxRenderCmdEncoder::bindBuffer(char const* bindingTag, GfxBuffer* buffer, rgU32 offset)
{
}

void GfxRenderCmdEncoder::bindBuffer(char const* bindingTag, GfxFrameResource const* resource)
{
}

//-----------------------------------------------------------------------------
void GfxRenderCmdEncoder::bindSamplerState(char const* bindingTag, GfxSamplerState* sampler)
{
}

//-----------------------------------------------------------------------------
void GfxRenderCmdEncoder::bindTexture(char const* bindingTag, GfxTexture* texture)
{
}

//-----------------------------------------------------------------------------
void GfxRenderCmdEncoder::drawTexturedQuads(TexturedQuads* quads)
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
void GfxRenderCmdEncoder::drawTriangles(rgU32 vertexStart, rgU32 vertexCount, rgU32 instanceCount)
{
}

void GfxRenderCmdEncoder::drawIndexedTriangles(rgU32 indexCount, rgBool is32bitIndex, GfxBuffer const* indexBuffer, rgU32 bufferOffset, rgU32 instanceCount)
{
}

void GfxRenderCmdEncoder::drawIndexedTriangles(rgU32 indexCount, rgBool is32bitIndex, GfxFrameResource const* indexBufferResource, rgU32 instanceCount)
{
}

//*****************************************************************************
// GfxComputeCmdEncoder Implementation
//*****************************************************************************

void GfxComputeCmdEncoder::begin(char const* tag)
{
}

void GfxComputeCmdEncoder::end()
{
}

//-----------------------------------------------------------------------------
void GfxComputeCmdEncoder::pushDebugTag(char const* tag)
{
}

//-----------------------------------------------------------------------------
void GfxComputeCmdEncoder::popDebugTag()
{
}

//-----------------------------------------------------------------------------
void GfxComputeCmdEncoder::setComputePSO(GfxComputePSO* pso)
{
}

//-----------------------------------------------------------------------------
GfxObjectBinding* GfxComputeCmdEncoder::getPipelineArgumentInfo(char const* bindingTag)
{
    rgAssert(gfx::currentComputePSO != nullptr);
    rgAssert(bindingTag);

    auto infoIter = gfx::currentComputePSO->reflection.find(bindingTag);
    if(infoIter == gfx::currentComputePSO->reflection.end())
    {
        rgLogWarn("Resource/Binding(%s) cannot be found in the current pipeline(%s)", bindingTag, gfx::currentComputePSO->tag);
        return nullptr;
    }

    rgAssert((infoIter->second.stages & GfxStage_CS) == GfxStage_CS);

    return &infoIter->second;
}

//-----------------------------------------------------------------------------
void GfxComputeCmdEncoder::bindBuffer(char const* bindingTag, GfxBuffer* buffer, rgU32 offset)
{
}

void GfxComputeCmdEncoder::bindBuffer(char const* bindingTag, GfxFrameResource const* resource)
{
}

void GfxComputeCmdEncoder::bindBufferFromData(char const* bindingTag, rgU32 sizeInBytes, void* data)
{
}

void GfxComputeCmdEncoder::bindTexture(char const* bindingTag, GfxTexture* texture)
{
}

void GfxComputeCmdEncoder::bindSamplerState(char const* bindingTag, GfxSamplerState* sampler)
{
}

void GfxComputeCmdEncoder::dispatch(rgU32 threadgroupsGridX, rgU32 threadgroupsGridY, rgU32 threadgroupsGridZ)
{
}

//*****************************************************************************
// GfxBlitCmdEncoder Implementation
//*****************************************************************************

void GfxBlitCmdEncoder::begin(char const* tag)
{
}

void GfxBlitCmdEncoder::end()
{
}

void GfxBlitCmdEncoder::pushDebugTag(const char* tag)
{
}

void GfxBlitCmdEncoder::genMips(GfxTexture* srcTexture)
{
}

void GfxBlitCmdEncoder::copyTexture(GfxTexture* srcTexture, GfxTexture* dstTexture, rgU32 srcMipLevel, rgU32 dstMipLevel, rgU32 mipLevelCount)
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
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_NONE);
    rgU32 alignedStartOffset = bumpStorageAligned(size, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

    ComPtr<ID3D12Resource> bufferResource;
    BreakIfFail(getDevice()->CreatePlacedResource(
        d3dBufferHeap.Get(),
        alignedStartOffset,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&bufferResource)
    ));

    setResourceDebugName(bufferResource, tag);

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
    output.d3dResource = bufferResource.Get();
    return output;
}

GfxFrameResource GfxFrameAllocator::newTexture2D(const char* tag, void* initialData, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage)
{


    // TODO: Incorrect implementation
    GfxFrameResource output;
    output.type = GfxFrameResource::Type_Texture;
    // ....


    return output;
}

RG_BEGIN_GFX_NAMESPACE
//*****************************************************************************
// Call from main | loop init() destroy() startNextFrame() endFrame()
//*****************************************************************************

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

    // create sampler descriptor heap
    samplerDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, MAX_SAMPLER_DESCRIPTOR, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    samplerDescriptorSize = getDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    samplerNextCPUDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(samplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

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
        GfxTexture* texRT = rgNew(GfxTexture);
        strncpy(texRT->tag, "RenderTarget", 32);
        texRT->dim = GfxTextureDim_2D;
        texRT->width = (rgUInt)desc.Width;
        texRT->height = (rgUInt)desc.Height;
        texRT->usage = GfxTextureUsage_RenderTarget;
        texRT->format = TinyImageFormat_FromDXGI_FORMAT((TinyImageFormat_DXGI_FORMAT)desc.Format);
        texRT->d3dTexture = texResource;
        gfx::swapchainTextures[i] = texRT;
    }

    dsvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    gfx::depthStencilTexture = gfx::texture->create("DepthStencilTarget", GfxTextureDim_2D, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D32_SFLOAT, GfxTextureMipFlag_1Mip, GfxTextureUsage_DepthStencil, nullptr);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
    dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsDesc.Texture2D.MipSlice = 0;
    dsDesc.Flags = D3D12_DSV_FLAG_NONE;
    getDevice()->CreateDepthStencilView(gfx::depthStencilTexture->d3dTexture.Get(), &dsDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    cbvSrvUavDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, MAX_CBVSRVUAV_DESCRIPTOR, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    // create command allocators
    for(rgUInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        commandAllocator[i] = createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    // create commandlist
    currentCommandList = createGraphicsCommandList(commandAllocator[0], nullptr);

    // crate resource uploader
    resourceUploader = rgNew(ResourceUploadBatch)(getDevice().Get());
    resourceUploader->Begin();

    GfxVertexInputDesc simpleVertexDesc = {};
    simpleVertexDesc.elementCount = 3;
    simpleVertexDesc.elements[0].semanticName = "POSITION";
    simpleVertexDesc.elements[0].semanticIndex = 0;
    simpleVertexDesc.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
    simpleVertexDesc.elements[0].bufferIndex = 0;
    simpleVertexDesc.elements[0].offset = 0;
    simpleVertexDesc.elements[1].semanticName = "TEXCOORD";
    simpleVertexDesc.elements[1].semanticIndex = 0;
    simpleVertexDesc.elements[1].format = TinyImageFormat_R32G32_SFLOAT;
    simpleVertexDesc.elements[1].bufferIndex = 0;
    simpleVertexDesc.elements[1].offset = 12;
    simpleVertexDesc.elements[2].semanticName = "COLOR";
    simpleVertexDesc.elements[2].semanticIndex = 0;
    simpleVertexDesc.elements[2].format = TinyImageFormat_R32G32B32A32_SFLOAT;
    simpleVertexDesc.elements[2].bufferIndex = 0;
    simpleVertexDesc.elements[2].offset = 20;

    GfxShaderDesc simple2dShaderDesc = {};
    simple2dShaderDesc.shaderSrc = "simple2d.hlsl";
    simple2dShaderDesc.vsEntrypoint = "vsSimple2dTest";
    simple2dShaderDesc.fsEntrypoint = "fsSimple2dTest";
    simple2dShaderDesc.defines = "RIGHT";

    GfxRenderStateDesc simple2dRenderStateDesc = {};
    simple2dRenderStateDesc.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    simple2dRenderStateDesc.colorAttachments[0].blendingEnabled = true;
    simple2dRenderStateDesc.depthStencilAttachmentFormat = TinyImageFormat_D32_SFLOAT;
    simple2dRenderStateDesc.depthWriteEnabled = true;
    simple2dRenderStateDesc.depthCompareFunc = GfxCompareFunc_Less;
    //GfxGraphicsPSO* simplePSO = createGraphicsPSO("simple2d", &simpleVertexDesc, &simple2dShaderDesc, &simple2dRenderStateDesc);
    GfxGraphicsPSO* simplePSO = gfx::graphicsPSO->create("simple2d", &simpleVertexDesc, &simple2dShaderDesc, &simple2dRenderStateDesc);
    dummyPSO = simplePSO->d3dPSO;
    dummyRootSignature = simplePSO->d3dRootSignature;

    {
        rgFloat triangleVertices[] =
        {
            0.0f, 0.4f, 0.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.4f, -0.25f, 0.0f, 0.9f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
            -0.25f, -0.1f, 0.0f, 0.0f, 0.3f, 1.0f, 0.0f, 0.0f, 1.0f,

            0.0f, 1.25f, 0.1f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.25f, -0.25f, 0.1f, 0.9f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f,
            -0.25f, -0.25f, 0.1f, 0.0f, 0.3f, 0.0f, 0.0f, 1.0f, 1.0f
        };

        rgUInt vbSize = sizeof(triangleVertices);

        BreakIfFail(getDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vbSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof(triVB),
            (void**)&(triVB)
        ));
        setResourceDebugName(triVB, "henlo");

        rgU8* vbPtr;;
        CD3DX12_RANGE readRange(0, 0);
        BreakIfFail(triVB->Map(0, &readRange, (void**)&vbPtr));
        memcpy(vbPtr, triangleVertices, vbSize);
        triVB->Unmap(0, nullptr);

        triVBView.BufferLocation = triVB->GetGPUVirtualAddress();
        triVBView.StrideInBytes = 36;
        triVBView.SizeInBytes = vbSize;
    }

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
    currentCommandList->SetGraphicsRootSignature(dummyRootSignature.Get());

    currentCommandList->SetPipelineState(dummyPSO.Get());

    CD3DX12_VIEWPORT vp(0.0f, 0.0f, (rgFloat)g_WindowInfo.width, (rgFloat)g_WindowInfo.height);
    currentCommandList->RSSetViewports(1, &vp);
    CD3DX12_RECT scissorRect(0, 0, g_WindowInfo.width, g_WindowInfo.height);
    currentCommandList->RSSetScissorRects(1, &scissorRect);

    GfxTexture* currentRenderTarget = gfx::swapchainTextures[g_FrameIndex];
    currentCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentRenderTarget->d3dTexture.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), g_FrameIndex, rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    currentCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    currentCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

    const rgFloat clearColor[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    currentCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    currentCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    currentCommandList->IASetVertexBuffers(0, 1, &triVBView);
    //commandList->DrawInstanced(6, 1, 0, 0);
    currentCommandList->DrawInstanced(3, 1, 0, 0);
    currentCommandList->DrawInstanced(3, 1, 3, 0);

    return 0;
}

void checkerWaitTillFrameCompleted(rgInt frameIndex)
{
    rgAssert(frameIndex >= 0);
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

    draw();
}

void endFrame()
{
    GfxTexture* currentRenderTarget = gfx::swapchainTextures[g_FrameIndex];
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

void setterBindlessResource(rgU32 slot, GfxTexture* ptr)
{
}

void rendererImGuiInit()
{
    ImGui_ImplDX12_Init(getDevice().Get(), RG_MAX_FRAMES_IN_FLIGHT, DXGI_FORMAT_B8G8R8A8_UNORM, gfx::cbvSrvUavDescriptorHeap.Get(), gfx::cbvSrvUavDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), gfx::cbvSrvUavDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    ImGui_ImplSDL2_InitForD3D(gfx::mainWindow);
}

void rendererImGuiNewFrame()
{
    ImGui_ImplDX12_NewFrame();
}

void rendererImGuiRenderDrawData()
{
    currentCommandList->SetDescriptorHeaps(1, cbvSrvUavDescriptorHeap.GetAddressOf());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), currentCommandList.Get());
}

GfxTexture* getCurrentRenderTargetColorBuffer()
{
    return swapchainTextures[g_FrameIndex];
}

RG_END_GFX_NAMESPACE

#undef BreakIfFail
RG_END_RG_NAMESPACE
#endif
