#if defined(RG_METAL_RNDR)
#include "rg_gfx.h"

#include <simd/simd.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>
#include <EASTL/hash_map.h>
#include <EASTL/hash_set.h>

#import <Metal/Metal.h>
#import <Metal/MTLArgumentEncoder.h>
#import <Metal/MTLBuffer.h>
#import <AppKit/AppKit.h>
#include <QuartzCore/CAMetalLayer.h>

#include <sstream>
#include <string>

#include <wchar.h>
#include "ComPtr.hpp"
#include "dxcapi.h"

#include "spirv_cross.hpp"
#include "spirv_parser.hpp"
#include "spirv_msl.hpp"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_metal.h"

RG_BEGIN_RG_NAMESPACE

#include "shaders/metal/imm_shader.inl"
#include "shaders/metal/histogram_shader.inl"


static const rgU32 kBindlessTextureSetBinding = 7; // TODO: change this to some thing higher like 26
static const rgU32 kFrameParamsSetBinding = 6;
static rgU32 const kMaxMTLArgumentTableBufferSlot = 30;

RG_BEGIN_GFX_NAMESPACE
    static NSView* appView;
    static CAMetalLayer* metalLayer;
    static id<CAMetalDrawable> caMetalDrawable;

    static GfxTexture currentMTLDrawableTexture;

    static NSAutoreleasePool* autoReleasePool;
    static dispatch_semaphore_t framesInFlightSemaphore;

    static id<MTLDevice> mtlDevice;
    static id<MTLCommandQueue> mtlCommandQueue;
    static id<MTLCommandBuffer> mtlCommandBuffer;
    
    static id<MTLHeap> bindlessTextureHeap;
    static id<MTLArgumentEncoder> bindlessTextureArgEncoder;
    static id<MTLBuffer> bindlessTextureArgBuffer;
RG_END_GFX_NAMESPACE

//*****************************************************************************
// Helper Functions
//*****************************************************************************

static id<MTLDevice> getMTLDevice()
{
    return gfx::mtlDevice;
}

id<MTLCommandBuffer> getMTLCommandBuffer()
{
    return gfx::mtlCommandBuffer;
}

id<MTLRenderCommandEncoder> getMTLRenderCommandEncoder()
{
    return (__bridge id<MTLRenderCommandEncoder>)gfx::currentRenderCmdEncoder->mtlRenderCommandEncoder;
}

id<MTLTexture> getMTLTexture(const GfxTexture* obj)
{
     return (__bridge id<MTLTexture>)(obj->mtlTexture);
}

id<MTLBuffer> getMTLBuffer(const GfxBuffer* obj)
{
    return (__bridge id<MTLBuffer>)(obj->mtlBuffer);
}

id<MTLSamplerState> getMTLSamplerState(const GfxSamplerState* obj)
{
    return (__bridge id<MTLSamplerState>)(obj->mtlSampler);
}

id<MTLRenderPipelineState> getMTLRenderPipelineState(const GfxGraphicsPSO* obj)
{
    return (__bridge id<MTLRenderPipelineState>)(obj->mtlPSO);
}

id<MTLComputePipelineState> getMTLComputePipelineState(const GfxComputePSO* obj)
{
    return (__bridge id<MTLComputePipelineState>)(obj->mtlPSO);
}

id<MTLDepthStencilState> getMTLDepthStencilState(const GfxGraphicsPSO* obj)
{
    return (__bridge id<MTLDepthStencilState>)(obj->mtlDepthStencilState);
}

MTLWinding getMTLWinding(GfxGraphicsPSO* obj)
{
   return (obj->winding == GfxWinding_CCW) ? MTLWindingCounterClockwise : MTLWindingClockwise;
}

MTLCullMode getMTLCullMode(GfxGraphicsPSO* obj)
{
    MTLCullMode result = MTLCullModeNone;
    if(obj->cullMode == GfxCullMode_Front)
    {
        result = MTLCullModeFront;
    }
    else if(obj->cullMode == GfxCullMode_Back)
    {
        result = MTLCullModeBack;
    }
    return result;
}

MTLTriangleFillMode getMTLTriangleFillMode(GfxGraphicsPSO* obj)
{
    return (obj->triangleFillMode == GfxTriangleFillMode_Fill) ? MTLTriangleFillModeFill : MTLTriangleFillModeLines;
}

id<MTLRenderCommandEncoder> asMTLRenderCommandEncoder(void* ptr)
{
    return (__bridge id<MTLRenderCommandEncoder>)ptr;
}

id<MTLComputeCommandEncoder> asMTLComputeCommandEncoder(void* ptr)
{
    return (__bridge id<MTLComputeCommandEncoder>)ptr;
}

id<MTLBlitCommandEncoder> asMTLBlitCommandEncoder(void* ptr)
{
    return (__bridge id<MTLBlitCommandEncoder>)ptr;
}

id<MTLTexture> asMTLTexture(void* ptr)
{
    return (__bridge id<MTLTexture>)ptr;
}

id<MTLBuffer> asMTLBuffer(void* ptr)
{
    return (__bridge id<MTLBuffer>)ptr;
}

id<MTLSamplerState> asMTLSamplerState(void* ptr)
{
    return (__bridge id<MTLSamplerState>)(ptr);
}

id<MTLHeap> asMTLHeap(void* ptr)
{
    return (__bridge id<MTLHeap>)(ptr);
}

MTLResourceOptions toMTLResourceOptions(GfxBufferUsage usage, rgBool dynamic)
{
    MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceHazardTrackingModeTracked;
    if(dynamic)
    {
        options |= MTLResourceCPUCacheModeWriteCombined;
    }
    return options;
}

MTLTextureUsage toMTLTextureUsage(GfxTextureUsage usage)
{
    MTLTextureUsage result = MTLTextureUsageUnknown;
    
    if(usage == GfxTextureUsage_ShaderRead)
    {
        result |= MTLTextureUsageShaderRead;
    }
    
    if(usage == GfxTextureUsage_ShaderReadWrite)
    {
        result = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;
    }
    
    if(usage == GfxTextureUsage_RenderTarget)
    {
        result = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    }
    
    if(usage == GfxTextureUsage_DepthStencil)
    {
        result = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    }
    
    return result;
}

MTLTextureType toMTLTextureType(GfxTextureDim dim)
{
    switch(dim)
    {
        case GfxTextureDim_2D:
            return MTLTextureType2D;
            break;
        case GfxTextureDim_Cube:
            return MTLTextureTypeCube;
            break;
        case GfxTextureDim_Buffer:
            return MTLTextureTypeTextureBuffer;
            break;
        default:
            rgAssert(!"Invalid dim");
            return MTLTextureType1D;
            break;
    }
}

MTLLoadAction toMTLLoadAction(GfxLoadAction action)
{
    MTLLoadAction result = MTLLoadActionDontCare;
    
    if(action == GfxLoadAction_Clear)
    {
        result = MTLLoadActionClear;
    }
    else if(action == GfxLoadAction_Load)
    {
        result = MTLLoadActionLoad;
    }
    
    return result;
}

MTLStoreAction toMTLStoreAction(GfxStoreAction action)
{
    MTLStoreAction result = MTLStoreActionDontCare;
    
    if(action == GfxStoreAction_Store)
    {
        result = MTLStoreActionStore;
    }
    
    return result;
};

MTLPixelFormat toMTLPixelFormat(TinyImageFormat fmt)
{
    return (MTLPixelFormat)TinyImageFormat_ToMTLPixelFormat(fmt);
}

MTLClearColor toMTLClearColor(rgFloat4* color)
{
    return MTLClearColorMake(color->r, color->g, color->b, color->a);
}

MTLVertexFormat toMTLVertexFormat(TinyImageFormat fmt)
{
    MTLVertexFormat result = MTLVertexFormatInvalid;
    switch(fmt)
    {
        case TinyImageFormat_R32G32B32A32_SFLOAT:
            result = MTLVertexFormatFloat4;
            break;
        case TinyImageFormat_R32G32B32_SFLOAT:
            result = MTLVertexFormatFloat3;
            break;
        case TinyImageFormat_R32G32_SFLOAT:
            result = MTLVertexFormatFloat2;
            break;
        case TinyImageFormat_R8G8B8A8_SRGB:
            result = MTLVertexFormatUChar4Normalized_BGRA;
        default:
            rgAssert(!"Not implemented");
            break;
    }
    return result;
}

MTLCompareFunction toMTLCompareFunction(GfxCompareFunc func)
{
    MTLCompareFunction result = MTLCompareFunctionLess;
    switch(func)
    {
    case GfxCompareFunc_Always:
        result = MTLCompareFunctionAlways;
        break;
    case GfxCompareFunc_Never:
        result = MTLCompareFunctionNever;
        break;
    case GfxCompareFunc_Equal:
        result = MTLCompareFunctionEqual;
        break;
    case GfxCompareFunc_NotEqual:
        result = MTLCompareFunctionNotEqual;
        break;
    case GfxCompareFunc_Less:
        result = MTLCompareFunctionLess;
        break;
    case GfxCompareFunc_LessEqual:
        result = MTLCompareFunctionLessEqual;
        break;
    case GfxCompareFunc_Greater:
        result = MTLCompareFunctionGreater;
        break;
    case GfxCompareFunc_GreaterEqual:
        result = MTLCompareFunctionGreaterEqual;
        break;
    }
    return result;
}

MTLSamplerAddressMode toMTLSamplerAddressMode(GfxSamplerAddressMode addressMode)
{
    MTLSamplerAddressMode result;
    switch(addressMode)
    {
        case GfxSamplerAddressMode_Repeat:
            result = MTLSamplerAddressModeRepeat;
            break;
        case GfxSamplerAddressMode_ClampToEdge:
            result = MTLSamplerAddressModeClampToEdge;
            break;
        case GfxSamplerAddressMode_ClampToZero:
            result = MTLSamplerAddressModeClampToZero;
            break;
        default:
            rgAssert(!"Invalid sampler address mode");
    }
    return result;
}

MTLSamplerMinMagFilter toMTLSamplerMinMagFilter(GfxSamplerMinMagFilter filter)
{
    return filter == GfxSamplerMinMagFilter_Nearest ? MTLSamplerMinMagFilterNearest : MTLSamplerMinMagFilterLinear;
}

MTLSamplerMipFilter toMTLSamplerMipFilter(GfxSamplerMipFilter filter)
{
    MTLSamplerMipFilter result;
    switch(filter)
    {
        case GfxSamplerMipFilter_NotMipped:
            result = MTLSamplerMipFilterNotMipmapped;
            break;
        case GfxSamplerMipFilter_Nearest:
            result = MTLSamplerMipFilterNearest;
            break;
        case GfxSamplerMipFilter_Linear:
            result = MTLSamplerMipFilterLinear;
            break;
        default:
            rgAssert(!"Invalid sample mip filter");
    }
    return result;
}

GfxTexture createGfxTexture2DFromMTLDrawable(id<CAMetalDrawable> drawable)
{
    // TODO: Add more info to texture2d
    GfxTexture texture;
    texture.dim = GfxTextureDim_2D;
    texture.width = (rgUInt)[drawable.texture width];
    texture.height = (rgUInt)[drawable.texture height];
    texture.format = TinyImageFormat_FromMTLPixelFormat((TinyImageFormat_MTLPixelFormat)[drawable.texture pixelFormat]);
    texture.mipCount = 1;
    texture.usage = GfxTextureUsage_RenderTarget;
    texture.mtlTexture = drawable.texture;
    return texture;
}

struct SimpleVertexFormat1
{
    simd::float3 position;
    simd::float2 texcoord;
    simd::float4 color;
};

//-----------------------------------------------------------------------------
// GfxBuffer Implementation
//-----------------------------------------------------------------------------

void GfxBuffer::create(char const* tag, void* buf, rgU32 size, GfxBufferUsage usage, GfxBuffer* obj)
{
    if(usage != GfxBufferUsage_ShaderRW)
    {
        rgAssert(buf != NULL);
    }
    
    MTLResourceOptions options = toMTLResourceOptions(usage, false);
    
    id<MTLBuffer> br;
    if(buf != NULL)
    {
        br = [getMTLDevice() newBufferWithBytes:buf length:(NSUInteger)size options:options];
    }
    else
    {
        br = [getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
    }
    rgAssert(br != nil);
    
    br.label = [NSString stringWithUTF8String:tag];
    
    obj->mtlBuffer = br;
}

void GfxBuffer::destroy(GfxBuffer* obj)
{
    [asMTLBuffer(obj->mtlBuffer) release];
}

//*****************************************************************************
// GfxSampler Implementation
//*****************************************************************************

void GfxSamplerState::create(char const* tag, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSamplerState* obj)
{
    MTLSamplerDescriptor* desc = [MTLSamplerDescriptor new];
    desc.rAddressMode = toMTLSamplerAddressMode(rstAddressMode);
    desc.sAddressMode = toMTLSamplerAddressMode(rstAddressMode);
    desc.tAddressMode = toMTLSamplerAddressMode(rstAddressMode);
    
    desc.minFilter = toMTLSamplerMinMagFilter(minFilter);
    desc.magFilter = toMTLSamplerMinMagFilter(magFilter);
    desc.mipFilter = toMTLSamplerMipFilter(mipFilter);
    desc.maxAnisotropy = anisotropy ? 16 : 1;

    desc.label = [NSString stringWithUTF8String:tag];
    
    id<MTLSamplerState> samplerState = [getMTLDevice() newSamplerStateWithDescriptor:desc];
    [desc release];
    
    obj->mtlSampler = (__bridge void*)samplerState;
}

void GfxSamplerState::destroy(GfxSamplerState* obj)
{
    [asMTLSamplerState(obj->mtlSampler) release];
}

//*****************************************************************************
// GfxTexture2D Implementation
//*****************************************************************************

void GfxTexture::create(char const* tag, GfxTextureDim dim, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureMipFlag mipFlag, GfxTextureUsage usage, ImageSlice* slices, GfxTexture* obj)
{
    MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
    texDesc.width = width;
    texDesc.height = height;
    texDesc.pixelFormat = toMTLPixelFormat(format);
    texDesc.textureType = toMTLTextureType(dim);
    texDesc.storageMode = MTLStorageModeShared;
    texDesc.usage = toMTLTextureUsage(usage);
    
    id<MTLTexture> te = [gfx::bindlessTextureHeap newTextureWithDescriptor:texDesc];
    te.label = [NSString stringWithUTF8String:tag];
    [texDesc release];
    
    // copy the texture data
    if(slices && slices[0].data != NULL)
    {
        rgInt sliceCount = dim == GfxTextureDim_Cube ? 6 : 1;
        for(rgInt s = 0; s < sliceCount; ++s)
        {
            MTLRegion region = MTLRegionMake2D(0, 0, width, height);
            [te replaceRegion:region mipmapLevel:0 slice:s withBytes:slices[s].data bytesPerRow:(width * (TinyImageFormat_BitSizeOfBlock(format) / 8)) bytesPerImage:0];
            //[te replaceRegion:region mipmapLevel:0 withBytes:slices[s].data bytesPerRow:width * TinyImageFormat_ChannelCount(format)];
        }
    }

    obj->mtlTexture = te;
}

void GfxTexture::destroy(GfxTexture* obj)
{
    [asMTLTexture(obj->mtlTexture) release];
}

//*****************************************************************************
// GfxGraphicsPSO Implementation
//*****************************************************************************
static ComPtr<IDxcUtils> g_DxcUtils;

template<typename PSOType>
id<MTLFunction> buildShader(char const* filename, GfxStage stage, char const* entrypoint, char const* defines, PSOType* obj)
{
    rgAssert(entrypoint);
    
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

    // generate SPIRV
    dxcArgs.push_back(L"-spirv");
    dxcArgs.push_back(L"-fspv-target-env=vulkan1.1");
    dxcArgs.push_back(L"-fvk-use-dx-layout");

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
        
        HRESULT QueryInterface(REFIID riid,  void** ppvObject) override { return E_NOINTERFACE; }
        ULONG   AddRef(void) override     { return 0; }
        ULONG   Release(void) override    { return 0; }
        
        eastl::hash_set<eastl::wstring> alreadyIncludedFiles;
    };
    
    ComPtr<CustomIncludeHandler> customIncludeHandler(rgNew(CustomIncludeHandler));
    
    // load shader file
    char filepath[512];
    strcpy(filepath, "../code/shaders/");
    strncat(filepath, filename, 490);

    rg::FileData shaderFileData = rg::readFile(filepath); // TODO: destructor deleter for FileData
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

    rg::freeFileData(&shaderFileData);

    // process errors
    ComPtr<IDxcBlobUtf8> errorMsg;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorMsg), nullptr);
    if(errorMsg && errorMsg->GetStringLength())
    {
        rgLogError("***Shader Compile Warn/Error(%s, %s),Defines:%s***\n%s", filename, entrypoint, defines, errorMsg->GetStringPointer());
    }

    // shader blob
    ComPtr<IDxcBlob> shaderBlob;
    checkHR(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr));
    rgAssert(shaderBlob->GetBufferSize());
    
    // Convert spirv to msl
    uint32_t* spvPtr = (uint32_t*)shaderBlob->GetBufferPointer();
    size_t spvWordCount = (size_t)(shaderBlob->GetBufferSize()/sizeof(uint32_t));
    
    spirv_cross::Parser spirvParser(spvPtr, spvWordCount);
    spirvParser.parse();
    
    spirv_cross::CompilerMSL::Options mslOptions;
    mslOptions.platform = spirv_cross::CompilerMSL::Options::Platform::iOS;
    mslOptions.set_msl_version(2, 3, 0);
    mslOptions.texture_buffer_native = true;
    mslOptions.argument_buffers = true;
    mslOptions.argument_buffers_tier = spirv_cross::CompilerMSL::Options::ArgumentBuffersTier::Tier2;
    //mslOptions.enable_decoration_binding = true;

    spirv_cross::CompilerMSL msl(spirvParser.get_parsed_ir());
    msl.set_msl_options(mslOptions);
    
    for(uint32_t d = 0; d < 7; ++d)
    {
        msl.add_discrete_descriptor_set(d);
    }
    msl.set_argument_buffer_device_address_space(7, true);
    
    // handle unsized texture array
    spirv_cross::MSLResourceBinding bindlessTextureBinding;
    bindlessTextureBinding.basetype = spirv_cross::SPIRType::Image;
    bindlessTextureBinding.desc_set = kBindlessTextureSetBinding;
    bindlessTextureBinding.binding = 0;
    bindlessTextureBinding.count = 256; // TODO: increase
    
    bindlessTextureBinding.stage = spv::ExecutionModelVertex;
    msl.add_msl_resource_binding(bindlessTextureBinding);
    bindlessTextureBinding.stage = spv::ExecutionModelFragment;
    msl.add_msl_resource_binding(bindlessTextureBinding);
    bindlessTextureBinding.stage = spv::ExecutionModelKernel;
    msl.add_msl_resource_binding(bindlessTextureBinding);
    
    std::string mslShaderSource = msl.compile();
    
    // perform reflection
    spirv_cross::ShaderResources shaderResources = msl.get_shader_resources();
    
    auto pushMSLResourceInfo = [&](uint32_t varId, GfxObjectBinding::Type type) -> void
    {
        uint32_t mslBinding = msl.get_automatic_msl_resource_binding(varId);
        if(mslBinding != uint32_t(-1))
        {
            std::string const name = msl.get_name(varId);
            eastl::string const ourName = eastl::string(name.c_str());
            
            auto existingInfoIter = obj->reflection.find(ourName);
            if(existingInfoIter != obj->reflection.end())
            {
                GfxObjectBinding& existingInfo = existingInfoIter->second;
                
                rgAssert(existingInfo.type == type);
                rgAssert(existingInfo.registerIndex == mslBinding);
                
                existingInfo.stages = (GfxStage)((rgU32)existingInfo.stages | (rgU32)stage);
                return;
            }
            
            GfxObjectBinding info;
            info.type = type;
            info.registerIndex = mslBinding;
            info.stages = stage;
            
            rgLog("%s %s -> %d", obj->tag, name.c_str(), mslBinding);
            
            obj->reflection[eastl::string(name.c_str())] = info;
        }
    };
    
    for(auto& r : shaderResources.uniform_buffers)
    {
        pushMSLResourceInfo(r.id, GfxObjectBinding::Type_ConstantBuffer);
    }
    for(auto& r : shaderResources.separate_images)
    {
        pushMSLResourceInfo(r.id, GfxObjectBinding::Type_Texture2D);
    }
    for(auto& r : shaderResources.separate_samplers)
    {
        pushMSLResourceInfo(r.id, GfxObjectBinding::Type_Sampler);
    }
    for(auto& r : shaderResources.storage_buffers) // RW Bigger than CBuffer like StructuredBuffer
    {
        pushMSLResourceInfo(r.id, GfxObjectBinding::Type_ConstantBuffer);
    }
    
    NSError* err;
    MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
    id<MTLLibrary> shaderLibrary = [getMTLDevice() newLibraryWithSource:[NSString stringWithUTF8String:mslShaderSource.c_str()] options:compileOptions error:&err];
    [compileOptions release];
     
    if(err)
    {
        printf("Shader Library Compilation Error:\n%s\n", [[err localizedDescription]UTF8String]);
        rgAssert(!"newLibraryWithSource error");
    }
    
    id<MTLFunction> shaderFunction = [shaderLibrary newFunctionWithName:[NSString stringWithUTF8String: entrypoint]];
    
    return shaderFunction;

    // TODO: release shader lib
}

void GfxGraphicsPSO::create(char const* tag, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO* obj)
{
    @autoreleasepool
    {
        // create PSO
        id<MTLFunction> vs, fs;
        if(shaderDesc->vsEntrypoint)
        {
            vs = buildShader(shaderDesc->shaderSrc, GfxStage_VS, shaderDesc->vsEntrypoint, shaderDesc->defines, obj);
        }
        if(shaderDesc->fsEntrypoint)
        {
            fs = buildShader(shaderDesc->shaderSrc, GfxStage_FS, shaderDesc->fsEntrypoint, shaderDesc->defines, obj);
        }
    
        MTLRenderPipelineDescriptor* psoDesc = [[MTLRenderPipelineDescriptor alloc] init];
        psoDesc.label = [NSString stringWithUTF8String:tag];
        
        [psoDesc setVertexFunction:vs];
        [psoDesc setFragmentFunction:fs];
        
        rgAssert(renderStateDesc != nullptr);
        for(rgInt i = 0; i < kMaxColorAttachments; ++i)
        {
            TinyImageFormat colorAttFormat = renderStateDesc->colorAttachments[i].pixelFormat;
            if(colorAttFormat != TinyImageFormat_UNDEFINED)
            {
                rgBool blendingEnabled = renderStateDesc->colorAttachments[i].blendingEnabled;
                psoDesc.colorAttachments[i].pixelFormat = toMTLPixelFormat(colorAttFormat);
                psoDesc.colorAttachments[i].blendingEnabled = NO;
                if(blendingEnabled)
                {
                    psoDesc.colorAttachments[i].blendingEnabled = YES;
                    psoDesc.colorAttachments[i].rgbBlendOperation = MTLBlendOperationAdd;
                    psoDesc.colorAttachments[i].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
                    psoDesc.colorAttachments[i].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
                    
                    psoDesc.colorAttachments[i].alphaBlendOperation = MTLBlendOperationAdd;
                    psoDesc.colorAttachments[i].sourceAlphaBlendFactor = MTLBlendFactorOne;
                    psoDesc.colorAttachments[i].destinationAlphaBlendFactor = MTLBlendFactorZero;
                }
            }
        }
        
        if(renderStateDesc->depthStencilAttachmentFormat != TinyImageFormat_UNDEFINED)
        {
            psoDesc.depthAttachmentPixelFormat = toMTLPixelFormat(renderStateDesc->depthStencilAttachmentFormat);
        }

        MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
        depthStencilDesc.depthWriteEnabled = renderStateDesc->depthWriteEnabled;
        depthStencilDesc.depthCompareFunction = toMTLCompareFunction(renderStateDesc->depthCompareFunc);
        obj->mtlDepthStencilState = [getMTLDevice() newDepthStencilStateWithDescriptor:depthStencilDesc];
        [depthStencilDesc release];

        // vertex attributes
        if(vertexInputDesc && vertexInputDesc->elementCount > 0)
        {
            MTLVertexDescriptor* vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
            eastl::hash_map<rgUInt, eastl::pair<rgUInt, GfxVertexStepFunc>> layoutSet;
            for(rgInt a = 0; a < vertexInputDesc->elementCount; ++a)
            {
                vertexDescriptor.attributes[a].format = toMTLVertexFormat(vertexInputDesc->elements[a].format);
                vertexDescriptor.attributes[a].offset = vertexInputDesc->elements[a].offset;
                
                rgUInt bufferIndex = kMaxMTLArgumentTableBufferSlot - vertexInputDesc->elements[a].bufferIndex;
                rgAssert(bufferIndex > 0 && bufferIndex <= kMaxMTLArgumentTableBufferSlot);
                if(layoutSet.count(bufferIndex) == 0)
                {
                    layoutSet[bufferIndex].first = 0;
                }
                layoutSet[bufferIndex].first = layoutSet[bufferIndex].first + (TinyImageFormat_BitSizeOfBlock(vertexInputDesc->elements[a].format) / 8);
                layoutSet[bufferIndex].second = vertexInputDesc->elements[a].stepFunc;
                
                vertexDescriptor.attributes[a].bufferIndex = bufferIndex;
            }
            for(auto& layout : layoutSet)
            {
                vertexDescriptor.layouts[layout.first].stepRate = 1;
                vertexDescriptor.layouts[layout.first].stride = layout.second.first;
                vertexDescriptor.layouts[layout.first].stepFunction = layout.second.second == GfxVertexStepFunc_PerInstance ? MTLVertexStepFunctionPerInstance : MTLVertexStepFunctionPerVertex;
            }
            psoDesc.vertexDescriptor = vertexDescriptor;
        }
    
        NSError* err;
        id<MTLRenderPipelineState> pso = [getMTLDevice() newRenderPipelineStateWithDescriptor:psoDesc error:&err];
        
        if(err)
        {
            printf("%s\n", err.localizedDescription.UTF8String);
            rgAssert(!"newRenderPipelineState error");
        }
        
        obj->mtlPSO = pso;
        
        // TODO: release vertexDescriptor??
        [psoDesc release];
        [fs release];
        [vs release];
        //[shaderLib release];
    }
}

void GfxGraphicsPSO::destroy(GfxGraphicsPSO* obj)
{
    [getMTLRenderPipelineState(obj) release];
}

//*****************************************************************************
// GfxComputePSO Implementation
//*****************************************************************************

void GfxComputePSO::create(const char* tag, GfxShaderDesc* shaderDesc, GfxComputePSO* obj)
{
    @autoreleasepool
    {
        rgAssert(shaderDesc->csEntrypoint);
        id<MTLFunction> cs = buildShader(shaderDesc->shaderSrc, GfxStage_CS, shaderDesc->csEntrypoint, shaderDesc->defines, obj);
        rgAssert(cs != nil);
        
        NSError* err;
        id<MTLComputePipelineState> ce = [getMTLDevice() newComputePipelineStateWithFunction:cs error:&err];
        if(err)
        {
            printf("%s\n", err.localizedDescription.UTF8String);
            rgAssert(!"newComputePipelineStateWithFunction call failed");
        }
        
        rgAssert(ce != nil);
        obj->mtlPSO = (__bridge void*)ce;
        
        [cs release];
    }
}

void GfxComputePSO::destroy(GfxComputePSO* obj)
{
    [getMTLComputePipelineState(obj) release];
}

//*****************************************************************************
// GfxRenderCmdEncoder Implementation
//*****************************************************************************

void GfxRenderCmdEncoder::begin(char const* tag, GfxRenderPass* renderPass)
{
    // create RenderCommandEncoder
    MTLRenderPassDescriptor* renderPassDesc = [[MTLRenderPassDescriptor alloc] init];

    for(rgInt c = 0; c < kMaxColorAttachments; ++c)
    {
        if(renderPass->colorAttachments[c].texture == NULL)
        {
            continue;
        }
        
        MTLRenderPassColorAttachmentDescriptor* colorAttachmentDesc = [renderPassDesc colorAttachments][c];
        GfxColorAttachmentDesc* colorAttachment = &renderPass->colorAttachments[c];
        
        colorAttachmentDesc.texture     = getMTLTexture(colorAttachment->texture);
        colorAttachmentDesc.loadAction  = toMTLLoadAction(colorAttachment->loadAction);
        colorAttachmentDesc.storeAction = toMTLStoreAction(colorAttachment->storeAction);
        colorAttachmentDesc.clearColor  = toMTLClearColor(&colorAttachment->clearColor);
    }
    
    if(renderPass->depthStencilAttachmentTexture != NULL)
    {
        MTLRenderPassDepthAttachmentDescriptor* depthAttachmentDesc = [renderPassDesc depthAttachment];
        depthAttachmentDesc.texture     = getMTLTexture(renderPass->depthStencilAttachmentTexture);
        depthAttachmentDesc.loadAction  = toMTLLoadAction(renderPass->depthStencilAttachmentLoadAction);
        depthAttachmentDesc.storeAction = toMTLStoreAction(renderPass->depthStencilAttachmentStoreAction);
        depthAttachmentDesc.clearDepth  = renderPass->clearDepth;
    }
    
    id<MTLRenderCommandEncoder> re = [getMTLCommandBuffer() renderCommandEncoderWithDescriptor:renderPassDesc];
    rgAssert(re != nil);
    [re pushDebugGroup:[NSString stringWithUTF8String:tag]];
    
    [renderPassDesc autorelease];

    [re useHeap:gfx::bindlessTextureHeap stages:MTLRenderStageFragment];
    [re setFragmentBuffer:gfx::bindlessTextureArgBuffer offset:0 atIndex:kBindlessTextureSetBinding];
    
    [re useHeap:asMTLHeap(gfx::getFrameAllocator()->getHeap()) stages:MTLRenderStageVertex|MTLRenderStageFragment];
    
    mtlRenderCommandEncoder = (__bridge void*)re;
    hasEnded = false;
}

void GfxRenderCmdEncoder::end()
{
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) endEncoding];
    hasEnded = true;
}

void GfxRenderCmdEncoder::pushDebugTag(const char* tag)
{
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) pushDebugGroup:[NSString stringWithUTF8String:tag]];
}

void GfxRenderCmdEncoder::popDebugTag()
{
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) popDebugGroup];
}

void GfxRenderCmdEncoder::setViewport(rgFloat4 viewport)
{
    setViewport(viewport.x, viewport.y, viewport.z, viewport.w);
}

void GfxRenderCmdEncoder::setViewport(rgFloat originX, rgFloat originY, rgFloat width, rgFloat height)
{
    MTLViewport vp;
    vp.originX = originX;
    vp.originY = originY;
    vp.width   = width;
    vp.height  = height;
    vp.znear   = 0.0;
    vp.zfar    = 1.0;
    
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) setViewport:vp];
}

void GfxRenderCmdEncoder::setScissorRect(rgU32 xPixels, rgU32 yPixels, rgU32 widthPixels, rgU32 heightPixels)
{
    MTLScissorRect rect;
    rect.x = xPixels;
    rect.y = yPixels;
    rect.width = widthPixels;
    rect.height = heightPixels;
    
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) setScissorRect:rect];
}

void GfxRenderCmdEncoder::setGraphicsPSO(GfxGraphicsPSO* pso)
{
    id<MTLRenderCommandEncoder> cmdEncoder = asMTLRenderCommandEncoder(mtlRenderCommandEncoder);
    
    [cmdEncoder setRenderPipelineState:getMTLRenderPipelineState(pso)];
    [cmdEncoder setDepthStencilState:getMTLDepthStencilState(pso)];
    [cmdEncoder setFrontFacingWinding:getMTLWinding(pso)];
    [cmdEncoder setCullMode:getMTLCullMode(pso)];
    [cmdEncoder setTriangleFillMode:getMTLTriangleFillMode(pso)];
    
    gfx::currentGraphicsPSO = pso;
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
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) setVertexBuffer:getMTLBuffer(buffer) offset:offset atIndex:slotToVertexBinding(slot)];
}

void GfxRenderCmdEncoder::setVertexBuffer(GfxFrameResource const* resource, rgU32 slot)
{
    rgAssert(resource && resource->type == GfxFrameResource::Type_Buffer);
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) setVertexBuffer:asMTLBuffer(resource->mtlBuffer) offset:0 atIndex:slotToVertexBinding(slot)];
}

//-----------------------------------------------------------------------------
GfxObjectBinding& getResourceBindingInfo(char const* bindingTag)
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
    
    return info;
}

void GfxRenderCmdEncoder::bindBuffer(char const* bindingTag, GfxBuffer* buffer, rgU32 offset)
{
    rgAssert(buffer != nullptr);

    GfxObjectBinding& info = getResourceBindingInfo(bindingTag);
    
    // TODO: Assert check valid Stage
    
    // TODO: Look into the logic of binding on both stages..
    // binding should be same.. type should be same... etc.
    id<MTLRenderCommandEncoder> encoder = asMTLRenderCommandEncoder(mtlRenderCommandEncoder);
    if((info.stages & GfxStage_VS) == GfxStage_VS)
    {
        [encoder setVertexBuffer:getMTLBuffer(buffer) offset:offset atIndex:info.registerIndex];
    }
    if((info.stages & GfxStage_FS) == GfxStage_FS)
    {
        [encoder setFragmentBuffer:getMTLBuffer(buffer) offset:offset atIndex:info.registerIndex];
    }
}

void GfxRenderCmdEncoder::bindBuffer(char const* bindingTag, GfxFrameResource const* resource)
{
    rgAssert(resource && resource->type == GfxFrameResource::Type_Buffer);
    
    GfxObjectBinding& info = getResourceBindingInfo(bindingTag);
    
    id<MTLRenderCommandEncoder> encoder = asMTLRenderCommandEncoder(mtlRenderCommandEncoder);
    
    // TODO: Look into the logic of binding on both stages..
    // binding should be same.. type should be same... etc.
    if((info.stages & GfxStage_VS) == GfxStage_VS)
    {
        [encoder setVertexBuffer:asMTLBuffer(resource->mtlBuffer) offset:0 atIndex:info.registerIndex];
    }
    if((info.stages & GfxStage_FS) == GfxStage_FS)
    {
        [encoder setFragmentBuffer:asMTLBuffer(resource->mtlBuffer) offset:0 atIndex:info.registerIndex];
    }
}

//-----------------------------------------------------------------------------
void GfxRenderCmdEncoder::bindSamplerState(char const* bindingTag, GfxSamplerState* sampler)
{
    rgAssert(sampler != nullptr);
    
    GfxObjectBinding& info = getResourceBindingInfo(bindingTag);
    
    id<MTLRenderCommandEncoder> encoder = asMTLRenderCommandEncoder(mtlRenderCommandEncoder);
    if((info.stages & GfxStage_VS) == GfxStage_VS)
    {
        [encoder setVertexSamplerState:getMTLSamplerState(sampler) atIndex:info.registerIndex];
    }
    if((info.stages & GfxStage_FS) == GfxStage_FS)
    {
        [encoder setFragmentSamplerState:getMTLSamplerState(sampler) atIndex:info.registerIndex];
    }
}

//-----------------------------------------------------------------------------
void GfxRenderCmdEncoder::bindTexture(char const* bindingTag, GfxTexture* texture)
{
    rgAssert(texture != nullptr);
    
    GfxObjectBinding& info = getResourceBindingInfo(bindingTag);

    id<MTLRenderCommandEncoder> encoder = asMTLRenderCommandEncoder(mtlRenderCommandEncoder);
    // TODO: Look into the logic of binding on both stages..
    // binding should be same.. type should be same... etc.
    if((info.stages & GfxStage_VS) == GfxStage_VS)
    {
        [encoder setVertexTexture:getMTLTexture(texture) atIndex:info.registerIndex];
    }
    if((info.stages & GfxStage_FS) == GfxStage_FS)
    {
        [encoder setFragmentTexture:getMTLTexture(texture) atIndex:info.registerIndex];
    }
    else
    {
        rgAssert(!"Not valid");
    }
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
    
    copyMatrix4ToFloatArray(cameraParams.projection2d, gfx::makeOrthographicProjectionMatrix(0.0f, g_WindowInfo.width, g_WindowInfo.height, 0.0f, 0.1f, 1000.0f));
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
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertexCount instanceCount:instanceCount];
}

void GfxRenderCmdEncoder::drawIndexedTriangles(rgU32 indexCount, rgBool is32bitIndex, GfxBuffer const* indexBuffer, rgU32 bufferOffset, rgU32 instanceCount)
{
    rgAssert(mtlRenderCommandEncoder);
    MTLIndexType indexElementType = is32bitIndex ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:indexCount indexType:indexElementType indexBuffer:getMTLBuffer(indexBuffer) indexBufferOffset:bufferOffset instanceCount:instanceCount];
}

void GfxRenderCmdEncoder::drawIndexedTriangles(rgU32 indexCount, rgBool is32bitIndex, GfxFrameResource const* indexBufferResource, rgU32 instanceCount)
{
    rgAssert(mtlRenderCommandEncoder);
    rgAssert(indexBufferResource && indexBufferResource->type == GfxFrameResource::Type_Buffer);
    MTLIndexType indexElementType = is32bitIndex ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    [asMTLRenderCommandEncoder(mtlRenderCommandEncoder) drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:indexCount indexType:indexElementType indexBuffer:asMTLBuffer(indexBufferResource->mtlBuffer) indexBufferOffset:0 instanceCount:instanceCount];
}

//*****************************************************************************
// GfxComputeCmdEncoder Implementation
//*****************************************************************************

void GfxComputeCmdEncoder::begin(char const* tag)
{
    rgAssert(tag);
    id<MTLComputeCommandEncoder> cce = [getMTLCommandBuffer() computeCommandEncoder];
    rgAssert(cce != nil);
    [cce pushDebugGroup:[NSString stringWithUTF8String:tag]];
    [cce useHeap:gfx::bindlessTextureHeap];
    [cce useHeap:asMTLHeap(gfx::getFrameAllocator()->getHeap())];
    mtlComputeCommandEncoder = (__bridge void*)cce;
    hasEnded = false;
}

void GfxComputeCmdEncoder::end()
{
    [asMTLComputeCommandEncoder(mtlComputeCommandEncoder) endEncoding];
    hasEnded = true;
}

void GfxComputeCmdEncoder::pushDebugTag(char const* tag)
{
    [asMTLComputeCommandEncoder(mtlComputeCommandEncoder) pushDebugGroup:[NSString stringWithUTF8String:tag]];
}

void GfxComputeCmdEncoder::popDebugTag()
{
    [asMTLComputeCommandEncoder(mtlComputeCommandEncoder) popDebugGroup];
}

void GfxComputeCmdEncoder::setComputePSO(GfxComputePSO* pso)
{
    rgAssert(pso);
    [asMTLComputeCommandEncoder(mtlComputeCommandEncoder) setComputePipelineState:getMTLComputePipelineState(pso)];
    gfx::currentComputePSO = pso;
}

//*****************************************************************************
// GfxBlitCmdEncoder Implementation
//*****************************************************************************

void GfxBlitCmdEncoder::begin()
{
    id<MTLBlitCommandEncoder> blitCmdEncoder = [getMTLCommandBuffer() blitCommandEncoder];
    mtlBlitCommandEncoder = (__bridge void*)blitCmdEncoder;
    hasEnded = false;
}

void GfxBlitCmdEncoder::end()
{
    rgAssert(!hasEnded);
    [asMTLBlitCommandEncoder(mtlBlitCommandEncoder) endEncoding];
    hasEnded = true;
}

void GfxBlitCmdEncoder::pushDebugTag(const char* tag)
{
    [asMTLBlitCommandEncoder(mtlBlitCommandEncoder) pushDebugGroup:[NSString stringWithUTF8String:tag]];
}

void GfxBlitCmdEncoder::genMips(GfxTexture* srcTexture)
{
    [asMTLBlitCommandEncoder(mtlBlitCommandEncoder) generateMipmapsForTexture:getMTLTexture(srcTexture)];
}

void GfxBlitCmdEncoder::copyTexture(GfxTexture* srcTexture, GfxTexture* dstTexture, rgU32 srcMipLevel, rgU32 dstMipLevel, rgU32 mipLevelCount)
{
    id<MTLTexture> src = getMTLTexture(srcTexture);
    id<MTLTexture> dst = getMTLTexture(dstTexture);
    
    [asMTLBlitCommandEncoder(mtlBlitCommandEncoder) copyFromTexture:src sourceSlice:0 sourceLevel:srcMipLevel toTexture:dst destinationSlice:0 destinationLevel:dstMipLevel sliceCount:1 levelCount:mipLevelCount];
}

//*****************************************************************************
// Frame Resource Allocator
//*****************************************************************************

void GfxFrameAllocator::create(rgU32 sizeInBytes)
{
    MTLHeapDescriptor* heapDesc = [MTLHeapDescriptor new];
    heapDesc.type = MTLHeapTypePlacement;
    heapDesc.storageMode = MTLStorageModeShared;
    heapDesc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
    heapDesc.hazardTrackingMode = MTLHazardTrackingModeTracked; // TODO: use untracked for better perf. Similar to D3D12
    heapDesc.resourceOptions = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined | MTLResourceHazardTrackingModeTracked;
    heapDesc.size = sizeInBytes;
    
    id<MTLHeap> hp = [getMTLDevice() newHeapWithDescriptor:heapDesc];
    rgAssert(hp != nil);
    [heapDesc release];
    hp.label = @"FrameAllocator";
    
    heap = (__bridge void*)hp;
}

void GfxFrameAllocator::destroy()
{
    [asMTLHeap(heap) release];
}

void GfxFrameAllocator::releaseResources()
{
    rgInt l = mtlResources.size();
    for(rgInt i = 0; i < l; ++i)
    {
        id<MTLResource> re = (__bridge id<MTLResource>)mtlResources[i];
        [re release];
    }
    mtlResources.clear();
}

GfxFrameResource GfxFrameAllocator::newBuffer(const char* tag, rgU32 size, void* initialData)
{
    MTLResourceOptions options = MTLResourceStorageModeShared | MTLResourceHazardTrackingModeTracked | MTLResourceCPUCacheModeWriteCombined;
    MTLSizeAndAlign sizeAndAlign = [getMTLDevice() heapBufferSizeAndAlignWithLength:size options:options];
    
    rgU32 alignedStartOffset = bumpStorageAligned(sizeAndAlign.size, sizeAndAlign.align);
    
    id<MTLBuffer> br = [asMTLHeap(heap) newBufferWithLength:size options:options offset:alignedStartOffset];
    rgAssert(br != nil);
    br.label = [NSString stringWithUTF8String:tag];
    
    if(initialData != nullptr)
    {
        void* mappedPtr = [br contents];
        std::memcpy(mappedPtr, initialData, size);
    }
    
    mtlResources.push_back((__bridge void*)br);
    
    GfxFrameResource output;
    output.type = GfxFrameResource::Type_Buffer;
    output.mtlBuffer = (__bridge void*)br;
    output.mtlTexture = nullptr;
    
    return output;
}

GfxFrameResource GfxFrameAllocator::newTexture2D(const char* tag, void* initialData, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage)
{
    if(initialData != nullptr && (usage & GfxTextureUsage_MemorylessRenderTarget))
    {
        rgAssert(!"When texture usage is MemorylessRenderTraget, texture initial data can't be used");
    }
    
    MTLTextureDescriptor* texDesc = [MTLTextureDescriptor new];
    texDesc.width = width;
    texDesc.height = height;
    texDesc.pixelFormat = toMTLPixelFormat(format);
    texDesc.textureType = MTLTextureType2D;
    texDesc.storageMode = (usage & GfxTextureUsage_MemorylessRenderTarget) ? MTLStorageModeMemoryless : MTLStorageModeShared;
    texDesc.cpuCacheMode = MTLCPUCacheModeWriteCombined;
    texDesc.usage = toMTLTextureUsage(usage);
    
    MTLSizeAndAlign sizeAndAlign = [getMTLDevice() heapTextureSizeAndAlignWithDescriptor:texDesc];
    rgU32 alignedStartOffset = bumpStorageAligned(sizeAndAlign.size, sizeAndAlign.align);
    
    id<MTLTexture> te = [asMTLHeap(heap) newTextureWithDescriptor:texDesc offset:alignedStartOffset];
    te.label = [NSString stringWithUTF8String:tag];
    [texDesc release];
    
    // copy the texture data
    if(initialData != nullptr)
    {
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        [te replaceRegion:region mipmapLevel:0 withBytes:initialData bytesPerRow:width * TinyImageFormat_ChannelCount(format)];
    }
    
    mtlResources.push_back((__bridge void*)te);

    GfxFrameResource output;
    output.type = GfxFrameResource::Type_Texture;
    output.mtlTexture = (__bridge void*)te;
    output.mtlBuffer = nullptr;
    
    return output;
}

//***********************************************************************
//***********************************************************************
//***********************************************************************
RG_BEGIN_GFX_NAMESPACE
// TEST ATOMIC ---------
id<MTLComputePipelineState> histogramComputePipeline;

void testComputeAtomicsSetup()
{
    MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
    
    NSError* err;
    id<MTLLibrary> histogramLibrary = [getMTLDevice() newLibraryWithSource:[NSString stringWithUTF8String:g_HistogramShaderSrcCode] options:compileOptions error:&err];
    if(err)
    {
        printf("%s\n", [[err localizedDescription]UTF8String]);
        rgAssert(!"newLibrary error");
    }
    [compileOptions release];
    
    id<MTLFunction> computeHistogram = [histogramLibrary newFunctionWithName:@"computeHistogram_CS"];
    
    histogramComputePipeline = [getMTLDevice() newComputePipelineStateWithFunction:computeHistogram error:&err];
    
    [computeHistogram release];
    [histogramLibrary release];
    
    ImageRef histoTex = rg::loadImage("histogram_test.png");
    gfx::texture->create("histogramTest", GfxTextureDim_2D, histoTex->width, histoTex->height, histoTex->format, GfxTextureMipFlag_NoMips, GfxTextureUsage_ShaderRead, histoTex->slices);
    gfx::buffer->create("histogramBuffer", nullptr, sizeof(rgUInt)*255*3, GfxBufferUsage_ShaderRW);
}

void testComputeAtomicsRun()
{
    id<MTLComputeCommandEncoder> computeEncoder = [getMTLCommandBuffer() computeCommandEncoder];
    [computeEncoder setComputePipelineState:histogramComputePipeline];
    [computeEncoder setTexture:getMTLTexture(gfx::texture->find(rgCRC32("histogramTest"))) atIndex:0];
    [computeEncoder setBuffer:getMTLBuffer(gfx::buffer->find(rgCRC32("histogramBuffer"))) offset:0 atIndex:0];
    [computeEncoder setBuffer:getMTLBuffer(gfx::buffer->find(rgCRC32("histogramBuffer"))) offset:0 atIndex:1];
    [computeEncoder dispatchThreads:MTLSizeMake(4, 4, 1) threadsPerThreadgroup:MTLSizeMake(4, 4, 1)];
    [computeEncoder endEncoding];
}
// -------- TEST ATOMIC

id<MTLArgumentEncoder> frameConstBufferArgEncoder;
id<MTLBuffer> frameConstBufferArgBuffer;
id<MTLBuffer> cameraBuffer;
struct Camera
{
    float projection[16];
    float view[16];
};
///

//*****************************************************************************
// MTL backend states & instances
//*****************************************************************************

// Fencing and sync variables
static id<MTLSharedEvent> frameFenceEvent;
static MTLSharedEventListener* frameFenceEventListener;
static dispatch_queue_t frameFenceDispatchQueue; // TODO: Needed at all/here?
static rgU64 frameFenceValues[RG_MAX_FRAMES_IN_FLIGHT];

// Current state

//*****************************************************************************
// Call from main | loop init() destroy() startNextFrame() endFrame()
//*****************************************************************************

rgInt init()
{
    rgAssert(gfx::mainWindow);
    appView = (NSView*)SDL_Metal_CreateView(gfx::mainWindow);
    metalLayer = (CAMetalLayer*)SDL_Metal_GetLayer(appView);
    mtlDevice = MTLCreateSystemDefaultDevice();
    mtlCommandQueue = [mtlDevice newCommandQueue];

    metalLayer.device = mtlDevice;
    metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    metalLayer.maximumDrawableCount = RG_MAX_FRAMES_IN_FLIGHT;
    metalLayer.framebufferOnly = false;
    //metalLayer.displaySyncEnabled = false;
    //

    framesInFlightSemaphore = dispatch_semaphore_create(RG_MAX_FRAMES_IN_FLIGHT);
    
    @autoreleasepool
    {
        // Crate frame sync events
        frameFenceEvent = [getMTLDevice() newSharedEvent];
        frameFenceDispatchQueue = dispatch_queue_create("rg.gamelib.mtl.frameFenceDispatchQueue", NULL);
        frameFenceEventListener = [[MTLSharedEventListener alloc] initWithDispatchQueue:frameFenceDispatchQueue];
        
        // Initialize memory for bindless textures
        MTLHeapDescriptor* bindlessTextureHeapDesc = [MTLHeapDescriptor new];
        bindlessTextureHeapDesc.type = MTLHeapTypeAutomatic;
        bindlessTextureHeapDesc.storageMode = MTLStorageModeShared;
        bindlessTextureHeapDesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        bindlessTextureHeapDesc.hazardTrackingMode = MTLHazardTrackingModeTracked;
        bindlessTextureHeapDesc.size = rgMEGABYTE(128);
        bindlessTextureHeap = [getMTLDevice() newHeapWithDescriptor:bindlessTextureHeapDesc];
        // TODO: is manual release of descriptor needed?
        
        // Create bindless texture argument encoder and buffer
        MTLArgumentDescriptor* argDesc = [MTLArgumentDescriptor argumentDescriptor];
        argDesc.index = 0;
        argDesc.dataType = MTLDataTypeTexture;
        argDesc.arrayLength = 99999;
        argDesc.textureType = MTLTextureType2D;
        argDesc.access = MTLArgumentAccessReadOnly;
        
        bindlessTextureArgEncoder = [getMTLDevice() newArgumentEncoderWithArguments: @[argDesc]];
        bindlessTextureArgBuffer  = [getMTLDevice() newBufferWithLength:[bindlessTextureArgEncoder encodedLength]
                                                                options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined | MTLResourceHazardTrackingModeTracked];
        [bindlessTextureArgEncoder setArgumentBuffer:bindlessTextureArgBuffer offset:0];
        
        // Test argument encoder layout issues
#if 1
        MTLArgumentDescriptor* frameConstDescriptor1 = [MTLArgumentDescriptor argumentDescriptor];
        frameConstDescriptor1.index = 0;
        frameConstDescriptor1.dataType = MTLDataTypePointer;
        frameConstDescriptor1.access = MTLArgumentAccessReadOnly;
        
        MTLArgumentDescriptor* frameConstDescriptor2 = [MTLArgumentDescriptor argumentDescriptor];
        frameConstDescriptor2.index = 1;
        frameConstDescriptor2.dataType = MTLDataTypeTexture;
        frameConstDescriptor2.textureType = MTLTextureType2D;
        frameConstDescriptor2.access = MTLArgumentAccessReadOnly;

        frameConstBufferArgEncoder = [getMTLDevice() newArgumentEncoderWithArguments:@[frameConstDescriptor1]];
        frameConstBufferArgBuffer = [getMTLDevice() newBufferWithLength:[frameConstBufferArgEncoder encodedLength] options:toMTLResourceOptions(GfxBufferUsage_ConstantBuffer, true)];
#endif

        // TODO: allocate in frame allocator per frame
        cameraBuffer = [getMTLDevice() newBufferWithLength:sizeof(Camera) options:toMTLResourceOptions(GfxBufferUsage_ConstantBuffer, true)];
        
        testComputeAtomicsSetup();
    }
    return 0;
}

void destroy()
{
    
}

void startNextFrame()
{
    //dispatch_semaphore_wait(mtl->framesInFlightSemaphore, DISPATCH_TIME_FOREVER);
    
    rgU64 prevFrameFenceValue = (g_FrameIndex != -1) ? frameFenceValues[g_FrameIndex] : 0;
    
    g_FrameIndex = (g_FrameIndex + 1) % RG_MAX_FRAMES_IN_FLIGHT;
    
    // check if this next frame is finised on the GPU
    rgU64 nextFrameFenceValueToWaitFor = frameFenceValues[g_FrameIndex];
    while(frameFenceEvent.signaledValue < nextFrameFenceValueToWaitFor)
    {
        // This means we have no free backbuffers. we must wait
        
        //BreakIfFail(d3d.frameFence->SetEventOnCompletion(nextFrameFenceValueToWaitFor, d3d.frameFenceEvent));
        //::WaitForSingleObject(d3d.frameFenceEvent, INFINITE);
    }

    // This frame fence value is one more than prev frame fence value
    frameFenceValues[g_FrameIndex] = prevFrameFenceValue + 1;
    
    gfx::atFrameStart();
    
    // Reset finished frame allocator
    // TODO: Move this as common gfx class
    frameAllocators[getFinishedFrameIndex()]->reset();
    
    // Autorelease pool BEGIN
    autoReleasePool = [[NSAutoreleasePool alloc]init];
    
    id<CAMetalDrawable> metalDrawable = [metalLayer nextDrawable];
    rgAssert(metalDrawable != nil);
    caMetalDrawable = metalDrawable;
    
    currentMTLDrawableTexture = createGfxTexture2DFromMTLDrawable(caMetalDrawable);
    
    mtlCommandBuffer = [mtlCommandQueue commandBuffer];
}

void endFrame()
{
    //testComputeAtomicsRun();
    
    // TODO: Add assert if the currentRenderCmdEncoder is not ended
    if(!currentRenderCmdEncoder->hasEnded)
    {
        currentRenderCmdEncoder->end();
    }
    
    [getMTLCommandBuffer() presentDrawable:(caMetalDrawable)];
    
    //__block dispatch_semaphore_t blockSemaphore = mtl->framesInFlightSemaphore;
    //[getMTLCommandBuffer() addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer)
    //{
    //    dispatch_semaphore_signal(blockSemaphore);
    //}];
    
    rgU64 fenceValueToSignal = frameFenceValues[g_FrameIndex];
    [getMTLCommandBuffer() encodeSignalEvent:frameFenceEvent value:fenceValueToSignal];
    
    [getMTLCommandBuffer() commit];
    
    // Autorelease pool END
    [autoReleasePool release];
    
    return 0;
}

void rendererImGuiInit()
{
    ImGui_ImplMetal_Init(getMTLDevice());
    ImGui_ImplSDL2_InitForMetal(gfx::mainWindow);
}

void rendererImGuiNewFrame()
{
    MTLRenderPassDescriptor* renderPassDesc = [MTLRenderPassDescriptor new];
    renderPassDesc.colorAttachments[0].texture = asMTLTexture(getCurrentRenderTargetColorBuffer()->mtlTexture);
    renderPassDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    
    ImGui_ImplMetal_NewFrame(renderPassDesc);
    [renderPassDesc autorelease];
}

void rendererImGuiRenderDrawData()
{
    GfxRenderPass imguiRenderPass = {};
    imguiRenderPass.colorAttachments[0].texture = gfx::getCurrentRenderTargetColorBuffer();
    imguiRenderPass.colorAttachments[0].loadAction = GfxLoadAction_Load;
    imguiRenderPass.colorAttachments[0].storeAction = GfxStoreAction_Store;
    GfxRenderCmdEncoder* cmdEncoder = gfx::setRenderPass("ImGui Pass", &imguiRenderPass);
    
    ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), getMTLCommandBuffer(), asMTLRenderCommandEncoder(cmdEncoder->mtlRenderCommandEncoder));
}

void onSizeChanged()
{
    
}

GfxTexture* getCurrentRenderTargetColorBuffer()
{
    return &currentMTLDrawableTexture;
}

void setterBindlessResource(rgU32 slot, GfxTexture* ptr)
{
    [bindlessTextureArgEncoder setTexture:getMTLTexture(ptr) atIndex:slot];
}

void checkerWaitTillFrameCompleted(rgInt frameIndex)
{
    rgAssert(frameIndex >= 0);
    rgU64 valueToWaitFor = frameFenceValues[frameIndex];
    while(frameFenceEvent.signaledValue < valueToWaitFor)
    {
        // TODO: cpu friendly way to wait
        int ans = 42;
    }
}

RG_END_GFX_NAMESPACE
//***********************************************************************

RG_END_RG_NAMESPACE
#endif
