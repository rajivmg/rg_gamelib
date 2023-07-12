#if defined(RG_METAL_RNDR)
#include "rg_gfx.h"

#include <simd/simd.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>
#include <EASTL/hash_map.h>

#import <Metal/Metal.h>
#import <Metal/MTLArgumentEncoder.h>
#import <Metal/MTLBuffer.h>
#include <QuartzCore/CAMetalLayer.h>

#include <sstream>
#include <string>

#include <wchar.h>
#include "ComPtr.hpp"
#include "dxcapi.h"

#include "spirv_cross.hpp"
#include "spirv_parser.hpp"
#include "spirv_msl.hpp"

RG_BEGIN_RG_NAMESPACE
RG_BEGIN_GFX_NAMESPACE

#include "shaders/metal/imm_shader.inl"
#include "shaders/metal/histogram_shader.inl"

gfx::Mtl* mtl = nullptr;

static const rgU32 kBindlessTextureSetBinding = 7;
static const rgU32 kFrameParamsSetBinding = 6;
static rgU32 const kMaxMTLArgumentTableBufferSlot = 30;

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static id<MTLDevice> getMTLDevice()
{
    return (__bridge id<MTLDevice>)(mtl->device);
}

id<MTLRenderCommandEncoder> getMTLRenderEncoder()
{
    return (__bridge id<MTLRenderCommandEncoder>)mtl->renderEncoder;
}

id<MTLCommandBuffer> getMTLCommandBuffer()
{
    return (__bridge id<MTLCommandBuffer>)mtl->commandBuffer;
}

id<MTLRenderCommandEncoder> getMTLRenderCommandEncoder()
{
    return (__bridge id<MTLRenderCommandEncoder>)currentRenderCmdEncoder->renderCmdEncoder;
}

id<MTLTexture> getMTLTexture(const GfxTexture2D* obj)
{
     return (__bridge id<MTLTexture>)(obj->mtlTexture);
}

id<MTLBuffer> getMTLBuffer(const GfxBuffer* obj)
{
    return (__bridge id<MTLBuffer>)(obj->mtlBuffer);
}

id<MTLSamplerState> getMTLSamplerState(const GfxSampler* obj)
{
    return (__bridge id<MTLSamplerState>)(obj->mtlSampler);
}

id<MTLRenderPipelineState> getMTLRenderPipelineState(const GfxGraphicsPSO* obj)
{
    return (__bridge id<MTLRenderPipelineState>)(obj->mtlPSO);
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

id<MTLBlitCommandEncoder> asMTLBlitCommandEncoder(void* ptr)
{
    return (__bridge id<MTLBlitCommandEncoder>)ptr;
}

id<MTLSamplerState> asMTLSamplerState(void* ptr)
{
    return (__bridge id<MTLSamplerState>)(ptr);
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

GfxTexture2D createGfxTexture2DFromMTLDrawable(id<CAMetalDrawable> drawable)
{
    GfxTexture2D texture2d;
    texture2d.mtlTexture = (__bridge MTL::Texture*)drawable.texture;
    return texture2d;
}

void copyMatrix4ToFloatArray(rgFloat* dstArray, Matrix4 const& srcMatrix)
{
    rgFloat const* ptr = toFloatPtr(srcMatrix);
    for(rgInt i = 0; i < 16; ++i)
    {
        dstArray[i] = ptr[i];
    }
}

//-----------------------------------------------------------------------------
// Frame Resource Allocator
//-----------------------------------------------------------------------------

struct AllocationResult
{
    rgU32 offset;
    void* ptr;
    
    GfxBuffer bufferFacade;
    id<MTLBuffer> parentBuffer;
};

class FrameAllocator
{
public:
    FrameAllocator(id<MTLDevice> device_, rgU32 capacity_, MTLResourceOptions resourceOptions_)
    {
        offset = 0;
        capacity = capacity_;
        buffer = [device_ newBufferWithLength:capacity options:resourceOptions_];
        buffer.label = @"FrameAllocator";
        bufferPtr = (rgU8*)[buffer contents];
    }
    
    ~FrameAllocator()
    {
        [buffer release];
    }
    
    void reset()
    {
        offset = 0;
    }
    
    // TODO: convert to void allocate(char const*tag, rgU32 size, rgU32 outOffset, void** outPtr, ... )
    AllocationResult allocate(char const* tag, rgU32 size)
    {
        rgAssert(offset + size <= capacity);

        void* ptr = (bufferPtr + offset);
        
        AllocationResult result;
        result.offset = offset;
        result.ptr = ptr;
        result.parentBuffer = buffer;
        
        result.bufferFacade.mtlBuffer = (__bridge MTL::Buffer*)buffer;
        std::strcpy(result.bufferFacade.tag, tag);

        rgU32 alignment = 4;
        offset += (size + alignment - 1) & ~(alignment - 1);
        
        [buffer addDebugMarker:[NSString stringWithUTF8String:tag] range:NSMakeRange(result.offset, (offset - result.offset))];
        
        return result;
    }
    
    AllocationResult allocate(char const* tag, rgU32 size, void* initialData)
    {
        AllocationResult result = allocate(tag, size);
        memcpy(result.ptr, initialData, size);
        return result;
    }
    
    id<MTLBuffer> mtlBuffer()
    {
        return buffer;
    }
    
protected:
    rgU32 offset;
    rgU32 capacity;
    id<MTLBuffer> buffer;
    rgU8* bufferPtr;
};

struct SimpleVertexFormat1
{
    simd::float3 position;
    simd::float2 texcoord;
    simd::float4 color;
};

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
    
    TextureRef histoTex = rg::loadTexture("histogram_test.png");
    gfx::texture2D->create("histogramTest", histoTex->buf, histoTex->width, histoTex->height, histoTex->format, false, GfxTextureUsage_ShaderRead);
    gfx::buffer->create("histogramBuffer", nullptr, sizeof(rgUInt)*255*3, GfxBufferUsage_ShaderRW);
}

void testComputeAtomicsRun()
{
    id<MTLComputeCommandEncoder> computeEncoder = [getMTLCommandBuffer() computeCommandEncoder];
    [computeEncoder setComputePipelineState:histogramComputePipeline];
    [computeEncoder setTexture:getMTLTexture(gfx::texture2D->find(rgCRC32("histogramTest"))) atIndex:0];
    [computeEncoder setBuffer:getMTLBuffer(gfx::buffer->find(rgCRC32("histogramBuffer"))) offset:0 atIndex:0];
    //[computeEncoder setTexture:(id<MTLTexture>)getActiveMTLBuffer(gfx::findBuffer("histogramBuffer")) atIndex:1];
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

//-----------------------------------------------------------------------------
// MTL backend states & instances
//-----------------------------------------------------------------------------

static FrameAllocator* frameAllocators[RG_MAX_FRAMES_IN_FLIGHT];

static id<MTLHeap> bindlessTextureHeap;
static id<MTLArgumentEncoder> bindlessTextureArgEncoder;
static id<MTLBuffer> bindlessTextureArgBuffer;
static AllocationResult bindlessTextureArgBufferAlloc; // TODO: Needed here?

// Fencing and sync variables
static id<MTLSharedEvent> frameFenceEvent;
static MTLSharedEventListener* frameFenceEventListener;
static dispatch_queue_t frameFenceDispatchQueue; // TODO: Needed at all/here?
static rgU64 frameFenceValues[RG_MAX_FRAMES_IN_FLIGHT];

// Current state
static GfxGraphicsPSO* boundGraphicsPSO;

FrameAllocator* getFrameAllocator()
{
    return frameAllocators[g_FrameIndex];
}

//-----------------------------------------------------------------------------
// Call from main | loop init() destroy() startNextFrame() endFrame()
//-----------------------------------------------------------------------------

rgInt init()
{
    mtl = rgNew(gfx::Mtl);
    
    rgAssert(gfx::mainWindow);
    mtl->view = (NS::View*)SDL_Metal_CreateView(gfx::mainWindow);
    mtl->layer = SDL_Metal_GetLayer(mtl->view);
    mtl->device = MTL::CreateSystemDefaultDevice();
    mtl->commandQueue = mtl->device->newCommandQueue();

    CAMetalLayer* mtlLayer = (CAMetalLayer*)mtl->layer;
    mtlLayer.device = (__bridge id<MTLDevice>)(mtl->device);
    mtlLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    mtlLayer.maximumDrawableCount = RG_MAX_FRAMES_IN_FLIGHT;
    mtlLayer.framebufferOnly = false;
    //mtlLayer.displaySyncEnabled = false;
    //

    mtl->framesInFlightSemaphore = dispatch_semaphore_create(RG_MAX_FRAMES_IN_FLIGHT);
    
    @autoreleasepool
    {
        // Crate frame sync events
        frameFenceEvent = [getMTLDevice() newSharedEvent];
        frameFenceDispatchQueue = dispatch_queue_create("rg.gamelib.mtl.frameFenceDispatchQueue", NULL);
        frameFenceEventListener = [[MTLSharedEventListener alloc] initWithDispatchQueue:frameFenceDispatchQueue];
        
        // Initialize frame buffer allocators
        MTLResourceOptions frameBuffersResourceOptions = MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined | MTLResourceHazardTrackingModeTracked;
        for(rgS32 i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
        {
            frameAllocators[i] = rgNew(FrameAllocator)(getMTLDevice(), rgMEGABYTE(16), frameBuffersResourceOptions);
        }
        
        // Initialize memory for bindless textures
        MTLHeapDescriptor* bindlessTextureHeapDesc = [MTLHeapDescriptor new];
        bindlessTextureHeapDesc.type = MTLHeapTypeAutomatic;
        bindlessTextureHeapDesc.storageMode = MTLStorageModeShared;
        bindlessTextureHeapDesc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
        bindlessTextureHeapDesc.hazardTrackingMode = MTLHazardTrackingModeTracked;
        bindlessTextureHeapDesc.size = rgMEGABYTE(128);
        bindlessTextureHeap = [getMTLDevice() newHeapWithDescriptor:bindlessTextureHeapDesc];
        // TODO: is manual release of descriptor needed?
        
        // Create render targets
        for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
        {
            std::string tag = "renderTarget" + std::to_string(i);
            
            gfx::renderTarget[i] = gfx::texture2D->create(tag.c_str(), nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_B8G8R8A8_UNORM, false, GfxTextureUsage_RenderTarget);
        }
        gfx::depthStencilBuffer = gfx::texture2D->create("depthStencilBuffer", nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D16_UNORM, false, GfxTextureUsage_DepthStencil);
        
        
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
    mtl->autoReleasePool = NS::AutoreleasePool::alloc()->init(); // TODO: replace this with something better
    
    CAMetalLayer* metalLayer = (CAMetalLayer*)mtl->layer;
    id<CAMetalDrawable> metalDrawable = [metalLayer nextDrawable];
    rgAssert(metalDrawable != nullptr);
    mtl->caMetalDrawable = metalDrawable;
    
    mtl->commandBuffer = (__bridge id<MTLCommandBuffer>)mtl->commandQueue->commandBuffer();
}

void endFrame()
{
    //testComputeAtomicsRun();
    
    // TODO: Add assert if the currentRenderCmdEncoder is not ended
    if(!currentRenderCmdEncoder->hasEnded)
    {
        currentRenderCmdEncoder->end();
    }
    
    // blit renderTarget to MTLDrawable
    GfxTexture2D drawableTexture2D = createGfxTexture2DFromMTLDrawable((__bridge id<CAMetalDrawable>)mtl->caMetalDrawable);
    GfxBlitCmdEncoder* blitCmdEncoder = gfx::setBlitPass("CopyRTtoMTLDrawable");
    blitCmdEncoder->copyTexture(gfx::renderTarget[g_FrameIndex], &drawableTexture2D, 0, 0, 1);
    blitCmdEncoder->end();
    
    [getMTLCommandBuffer() presentDrawable:((__bridge id<CAMetalDrawable>)mtl->caMetalDrawable)];
    
    //__block dispatch_semaphore_t blockSemaphore = mtl->framesInFlightSemaphore;
    //[getMTLCommandBuffer() addCompletedHandler:^(id<MTLCommandBuffer> commandBuffer)
    //{
    //    dispatch_semaphore_signal(blockSemaphore);
    //}];
    
    rgU64 fenceValueToSignal = frameFenceValues[g_FrameIndex];
    [getMTLCommandBuffer() encodeSignalEvent:frameFenceEvent value:fenceValueToSignal];
    
    [getMTLCommandBuffer() commit];
    
    // Autorelease pool END
    mtl->autoReleasePool->release();
    
    return 0;
}

void onSizeChanged()
{
    
}

void setterBindlessResource(rgU32 slot, GfxTexture2D* ptr)
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

// TODO: remove this
using namespace gfx;

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
    
    id<MTLBuffer> mtlBuffer;
    if(buf != NULL)
    {
        mtlBuffer = [getMTLDevice() newBufferWithBytes:buf length:(NSUInteger)size options:options];
    }
    else
    {
        mtlBuffer = [getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
    }
    rgAssert(mtlBuffer != nil);
    
    mtlBuffer.label = [NSString stringWithUTF8String:tag];
    
    obj->mtlBuffer = (__bridge MTL::Buffer*)mtlBuffer;
}

void GfxBuffer::destroy(GfxBuffer* obj)
{
    obj->mtlBuffer->release();
}

//-----------------------------------------------------------------------------
// GfxSampler Implementation
//-----------------------------------------------------------------------------

void GfxSampler::create(char const* tag, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSampler* obj)
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

void GfxSampler::destroy(GfxSampler* obj)
{
    [asMTLSamplerState(obj->mtlSampler) release];
}

//-----------------------------------------------------------------------------
// GfxTexture2D Implementation
//-----------------------------------------------------------------------------

void GfxTexture2D::create(char const* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, rgBool genMips, GfxTextureUsage usage, GfxTexture2D* obj)
{
    MTLTextureDescriptor* texDesc = [[MTLTextureDescriptor alloc] init];
    texDesc.width = width;
    texDesc.height = height;
    texDesc.pixelFormat = toMTLPixelFormat(format);
    texDesc.textureType = MTLTextureType2D;
    texDesc.storageMode = MTLStorageModeShared;
    texDesc.usage = toMTLTextureUsage(usage);
    //texDesc.resourceOptions = toMTLResourceOptions(usage);
    
    //id<MTLTexture> mtlTexture = [getMTLDevice() newTextureWithDescriptor:texDesc];
    id<MTLTexture> mtlTexture = [bindlessTextureHeap newTextureWithDescriptor:texDesc];
    mtlTexture.label = [NSString stringWithUTF8String:tag];
    [texDesc release];
    
    // copy the texture data
    if(buf != NULL)
    {
        MTLRegion region = MTLRegionMake2D(0, 0, width, height);
        [mtlTexture replaceRegion:region mipmapLevel:0 withBytes:buf bytesPerRow:width * TinyImageFormat_ChannelCount(format)];
    }

    obj->mtlTexture = (__bridge MTL::Texture*)mtlTexture;
}

void GfxTexture2D::destroy(GfxTexture2D* obj)
{
    obj->mtlTexture->release();
}

//-----------------------------------------------------------------------------
// GfxGraphicsPSO Implementation
//-----------------------------------------------------------------------------

id<MTLFunction> buildShader(char const* filename, GfxStage stage, char const* entrypoint, char const* defines, GfxGraphicsPSO* obj)
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

    // create default include handler
    ComPtr<IDxcUtils> utils;
    checkHR(DxcCreateInstance(CLSID_DxcUtils, __uuidof(IDxcUtils), (void**)&utils));
    ComPtr<IDxcIncludeHandler> includeHandler;
    IDxcIncludeHandler* pIncludeHandler = includeHandler.Get();
    checkHR(utils->CreateDefaultIncludeHandler(&pIncludeHandler));

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
    checkHR(compiler3->Compile(&shaderSource, dxcArgs.data(), (UINT32)dxcArgs.size(), includeHandler.Get(), __uuidof(IDxcResult), (void**)&result));

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
    bindlessTextureBinding.desc_set = 7;
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
    
    auto pushMSLResourceInfo = [&](uint32_t varId, GfxGraphicsPSO::ResourceInfo::Type type) -> void
    {
        uint32_t mslBinding = msl.get_automatic_msl_resource_binding(varId);
        if(mslBinding != uint32_t(-1))
        {
            std::string const name = msl.get_name(varId);
            eastl::string const ourName = eastl::string(name.c_str());
            
            auto existingInfoIter = obj->mtlResourceInfo.find(ourName);
            if(existingInfoIter != obj->mtlResourceInfo.end())
            {
                GfxGraphicsPSO::ResourceInfo& existingInfo = existingInfoIter->second;
                
                rgAssert(existingInfo.type == type);
                rgAssert(existingInfo.mslBinding == mslBinding);
                
                existingInfo.stage = (GfxStage)((rgU32)existingInfo.stage | (rgU32)stage);
                return;
            }
            
            GfxGraphicsPSO::ResourceInfo info;
            info.type = type;
            info.mslBinding = mslBinding;
            info.stage = stage;
            
            rgLog("%s %s -> %d", obj->tag, name.c_str(), mslBinding);
            
            obj->mtlResourceInfo[eastl::string(name.c_str())] = info;
        }
    };
    
    for(auto& r : shaderResources.uniform_buffers)
    {
        pushMSLResourceInfo(r.id, GfxGraphicsPSO::ResourceInfo::Type_ConstantBuffer);
    }
    for(auto& r : shaderResources.separate_images)
    {
        pushMSLResourceInfo(r.id, GfxGraphicsPSO::ResourceInfo::Type_Texture2D);
    }
    for(auto& r : shaderResources.separate_samplers)
    {
        pushMSLResourceInfo(r.id, GfxGraphicsPSO::ResourceInfo::Type_Sampler);
    }
    for(auto& r : shaderResources.storage_buffers) // RW Bigger than CBuffer like StructuredBuffer
    {
        pushMSLResourceInfo(r.id, GfxGraphicsPSO::ResourceInfo::Type_ConstantBuffer);
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
        obj->mtlDepthStencilState = [gfx::getMTLDevice() newDepthStencilStateWithDescriptor:depthStencilDesc];
        [depthStencilDesc release];

        // vertex attributes
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

//-----------------------------------------------------------------------------
// GfxRenderCmdEncoder Implementation
//-----------------------------------------------------------------------------

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
        
        colorAttachmentDesc.texture     = gfx::getMTLTexture(colorAttachment->texture);
        colorAttachmentDesc.loadAction  = gfx::toMTLLoadAction(colorAttachment->loadAction);
        colorAttachmentDesc.storeAction = gfx::toMTLStoreAction(colorAttachment->storeAction);
        colorAttachmentDesc.clearColor  = gfx::toMTLClearColor(&colorAttachment->clearColor);
    }
    MTLRenderPassDepthAttachmentDescriptor* depthAttachmentDesc = [renderPassDesc depthAttachment];
    depthAttachmentDesc.texture     = gfx::getMTLTexture(renderPass->depthStencilAttachmentTexture);
    depthAttachmentDesc.loadAction  = gfx::toMTLLoadAction(renderPass->depthStencilAttachmentLoadAction);
    depthAttachmentDesc.storeAction = gfx::toMTLStoreAction(renderPass->depthStencilAttachmentStoreAction);
    depthAttachmentDesc.clearDepth  = renderPass->clearDepth;

    id<MTLRenderCommandEncoder> mtlRenderEncoder = [gfx::getMTLCommandBuffer() renderCommandEncoderWithDescriptor:renderPassDesc];
    rgAssert(mtlRenderEncoder != nil);
    [mtlRenderEncoder pushDebugGroup:[NSString stringWithUTF8String:tag]];
    
    [renderPassDesc autorelease];

    [mtlRenderEncoder useHeap:bindlessTextureHeap stages:MTLRenderStageFragment];
    [mtlRenderEncoder setFragmentBuffer:bindlessTextureArgBuffer offset:0 atIndex:kBindlessTextureSetBinding];
    
    renderCmdEncoder = (__bridge void*)mtlRenderEncoder;
    hasEnded = false;
}

void GfxRenderCmdEncoder::end()
{
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) endEncoding];
    hasEnded = true;
}

void GfxRenderCmdEncoder::pushDebugTag(const char* tag)
{
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) pushDebugGroup:[NSString stringWithUTF8String:tag]];
}

void GfxRenderCmdEncoder::popDebugTag()
{
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) popDebugGroup];
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
    
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setViewport:vp];
}

void GfxRenderCmdEncoder::setGraphicsPSO(GfxGraphicsPSO* pso)
{
    id<MTLRenderCommandEncoder> cmdEncoder = gfx::asMTLRenderCommandEncoder(renderCmdEncoder);
    
    [cmdEncoder setRenderPipelineState:gfx::getMTLRenderPipelineState(pso)];
    [cmdEncoder setDepthStencilState:gfx::getMTLDepthStencilState(pso)];
    [cmdEncoder setFrontFacingWinding:gfx::getMTLWinding(pso)];
    [cmdEncoder setCullMode:gfx::getMTLCullMode(pso)];
    [cmdEncoder setTriangleFillMode:gfx::getMTLTriangleFillMode(pso)];
    
    boundGraphicsPSO = pso;
}

void GfxRenderCmdEncoder::setVertexBuffer(const GfxBuffer* buffer, rgU32 offset, rgU32 slot)
{
    rgU32 const maxBufferBindIndex = 30;
    rgU32 bindpoint = maxBufferBindIndex - slot;
    rgAssert(bindpoint > 0 && bindpoint < 31);
    [asMTLRenderCommandEncoder(renderCmdEncoder) setVertexBuffer:getMTLBuffer(buffer) offset:offset atIndex:bindpoint];
}

void GfxRenderCmdEncoder::bindBuffer(GfxBuffer* buffer, rgU32 offset, char const* bindingTag)
{
    rgAssert(boundGraphicsPSO != nullptr);
    rgAssert(buffer != nullptr);
    
    auto infoIter = boundGraphicsPSO->mtlResourceInfo.find(bindingTag);
    rgAssert(infoIter != boundGraphicsPSO->mtlResourceInfo.end());

    GfxGraphicsPSO::ResourceInfo& info = infoIter->second;
    
    // TODO: Assert check valid Stage
    
    id<MTLRenderCommandEncoder> encoder = asMTLRenderCommandEncoder(renderCmdEncoder);
    if((info.stage & GfxStage_VS) == GfxStage_VS)
    {
        [encoder setVertexBuffer:getMTLBuffer(buffer) offset:offset atIndex:info.mslBinding];
    }
    if((info.stage & GfxStage_FS) == GfxStage_FS)
    {
        [encoder setFragmentBuffer:getMTLBuffer(buffer) offset:offset atIndex:info.mslBinding];
    }
}

void GfxRenderCmdEncoder::bindTexture2D(GfxTexture2D* texture, char const* bindingTag)
{
    rgAssert(boundGraphicsPSO != nullptr);
    rgAssert(texture != nullptr);
    
    auto infoIter = boundGraphicsPSO->mtlResourceInfo.find(bindingTag);
    rgAssert(infoIter != boundGraphicsPSO->mtlResourceInfo.end());

    GfxGraphicsPSO::ResourceInfo& info = infoIter->second;

    id<MTLRenderCommandEncoder> encoder = asMTLRenderCommandEncoder(renderCmdEncoder);
    if((info.stage & GfxStage_VS) == GfxStage_VS)
    {
        [encoder setVertexTexture:getMTLTexture(texture) atIndex:info.mslBinding];
    }
    if((info.stage & GfxStage_FS) == GfxStage_FS)
    {
        [encoder setFragmentTexture:getMTLTexture(texture) atIndex:info.mslBinding];
    }
    else
    {
        rgAssert(!"Not valid");
    }
}

void GfxRenderCmdEncoder::bindSampler(GfxSampler* sampler, char const* bindingTag)
{
    rgAssert(boundGraphicsPSO != nullptr);
    rgAssert(sampler != nullptr);
    
    auto infoIter = boundGraphicsPSO->mtlResourceInfo.find(bindingTag);
    rgAssert(infoIter != boundGraphicsPSO->mtlResourceInfo.end());
    GfxGraphicsPSO::ResourceInfo& info = infoIter->second;
    
    id<MTLRenderCommandEncoder> encoder = asMTLRenderCommandEncoder(renderCmdEncoder);
    if((info.stage & GfxStage_VS) == GfxStage_VS)
    {
        [encoder setVertexSamplerState:getMTLSamplerState(sampler) atIndex:info.mslBinding];
    }
    if((info.stage & GfxStage_FS) == GfxStage_FS)
    {
        [encoder setFragmentSamplerState:getMTLSamplerState(sampler) atIndex:info.mslBinding];
    }
}

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
    
    gfx::AllocationResult vertexBufAllocation = gfx::getFrameAllocator()->allocate("drawTexturedQuadsVertexBuf", (rgU32)vertices.size() * sizeof(gfx::SimpleVertexFormat), vertices.data());
    gfx::AllocationResult instanceParamsBuffer = gfx::getFrameAllocator()->allocate("instanceParamsCBuffer", sizeof(gfx::SimpleInstanceParams), &instanceParams);

    //-
    // camera
    struct
    {
        rgFloat projection2d[16];
        rgFloat view2d[16];
    } cameraParams;
    
    copyMatrix4ToFloatArray(cameraParams.projection2d, gfx::orthographicMatrix);
    copyMatrix4ToFloatArray(cameraParams.view2d, gfx::viewMatrix);

    gfx::AllocationResult cameraBuffer = gfx::getFrameAllocator()->allocate("cameraCBuffer", sizeof(cameraParams), (void*)&cameraParams);
    //std::memcpy(cameraBuffer.ptr, &cameraParams, sizeof(cameraParams));
    
    // --
    id<MTLRenderCommandEncoder> cmdEncoder = asMTLRenderCommandEncoder(renderCmdEncoder);
    [cmdEncoder useResource:getFrameAllocator()->mtlBuffer() usage:MTLResourceUsageRead stages:MTLRenderStageVertex | MTLRenderStageFragment];
    
    bindBuffer(&(cameraBuffer.bufferFacade), cameraBuffer.offset, "camera");
    bindBuffer(&(instanceParamsBuffer.bufferFacade), instanceParamsBuffer.offset, "instanceParams");
    bindSampler(gfx::sampler->find("bilinearSampler"_rh), "simpleSampler");
    [cmdEncoder setFragmentBuffer:bindlessTextureArgBuffer offset:0 atIndex:kBindlessTextureSetBinding];
    // TODO: Bindless texture binding
    
    ///[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setVertexBuffer:vertexBufAllocation.parentBuffer offset:vertexBufAllocation.offset atIndex:21];
    setVertexBuffer(&vertexBufAllocation.bufferFacade, vertexBufAllocation.offset, 0);
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setCullMode:MTLCullModeNone];

    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size() instanceCount:1];
}

// TODO: replace with generalized modular functions
#include "../3rdparty/obj2header/bunny_model.h"
const rgUInt bunnyModelIndexCount = sizeof(bunnyModelIndices)/sizeof(bunnyModelIndices[0]);
const rgUInt bunnyModelVertexCount = sizeof(bunnyModelVertices)/sizeof(bunnyModelVertices[0]);

Matrix4 makePerspectiveProjection(rgFloat fovDeg, rgFloat aspect, rgFloat nearValue, rgFloat farValue)
{
    // NOTE: In Metal NDC

    float angle = (0.5 * fovDeg) * (M_PI / 180.0f); // deg to rad
    float yScale = 1.0f / tanf(angle);
    float xScale = yScale / aspect;
    float zScale = farValue / (farValue - nearValue);

    return Matrix4(Vector4(xScale, 0.0f, 0.0f, 0.0f),
        Vector4(0.0f, yScale, 0.0f, 0.0f),
        Vector4(0.0f, 0.0f, zScale, 1.0f),
        Vector4(0.0f, 0.0f, -nearValue * zScale, 0.0f));
}

void GfxRenderCmdEncoder::drawBunny()
{
    gfx::AllocationResult vertexBufAllocation = gfx::getFrameAllocator()->allocate("drawBunnyVertexBuf", (rgU32)bunnyModelVertexCount * sizeof(Obj2HeaderModelVertex), (void*)bunnyModelVertices);
    gfx::AllocationResult indexBufAllocation = gfx::getFrameAllocator()->allocate("drawBunnyIndexBuf", (rgU32)bunnyModelIndexCount * sizeof(rgU32), (void*)bunnyModelIndices);
    
    // camera
    struct
    {
        rgFloat projectionPerspective[16];
        rgFloat viewCamera[16];  
    } cameraParams;

    copyMatrix4ToFloatArray(cameraParams.projectionPerspective, createPerspectiveProjectionMatrix(1.0f, g_WindowInfo.width/g_WindowInfo.height, 0.01f, 1000.0f));
    copyMatrix4ToFloatArray(cameraParams.viewCamera, Matrix4::lookAt(Point3(0.0f, 1.0f, -1.2f), Point3(-0.2, 0.9f, 0), Vector3(0, 1.0f, 0)));

    // instance
    struct
    {
        rgFloat worldXform[256][16];
        rgFloat invTposeWorldXform[256][16];
    } instanceParams;

    Matrix4 xform = Matrix4(Transform3::rotationY(sinf(g_Time * 0.5f + 2.0f))).setTranslation(Vector3(0.0f, 0.0f, 0.5f));
    copyMatrix4ToFloatArray(&instanceParams.worldXform[0][0], xform);
    copyMatrix4ToFloatArray(&instanceParams.invTposeWorldXform[0][0], transpose(inverse(xform)));
    
    gfx::AllocationResult cameraParamsBuffer = gfx::getFrameAllocator()->allocate("cameraParamsCBufferBunny", sizeof(cameraParams), &cameraParams);
    gfx::AllocationResult instanceParamsBuffer = gfx::getFrameAllocator()->allocate("instanceParamsCBufferBunny", sizeof(instanceParams), &instanceParams);

    id<MTLRenderCommandEncoder> rce = gfx::asMTLRenderCommandEncoder(renderCmdEncoder);
    
    bindBuffer(&(cameraParamsBuffer.bufferFacade), cameraParamsBuffer.offset, "camera");
    bindBuffer(&(instanceParamsBuffer.bufferFacade), instanceParamsBuffer.offset, "instanceParams");
    
    //[rce setVertexBytes:&cameraParams length:sizeof(cameraParams) atIndex:0];
    //[rce setVertexBytes:&instanceParams length:sizeof(instanceParams) atIndex:1];
    //[rce setFragmentBytes:&instanceParams length:sizeof(instanceParams) atIndex:1];
    [rce setVertexBuffer:vertexBufAllocation.parentBuffer offset:vertexBufAllocation.offset atIndex:30];
    [rce drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:bunnyModelIndexCount indexType:MTLIndexTypeUInt32 indexBuffer:indexBufAllocation.parentBuffer indexBufferOffset:indexBufAllocation.offset instanceCount:1];
}

//-----------------------------------------------------------------------------
// GfxBlitCmdEncoder Implementation
//-----------------------------------------------------------------------------

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

void GfxBlitCmdEncoder::genMips(GfxTexture2D* srcTexture)
{
    [asMTLBlitCommandEncoder(mtlBlitCommandEncoder) generateMipmapsForTexture:getMTLTexture(srcTexture)];
}

void GfxBlitCmdEncoder::copyTexture(GfxTexture2D* srcTexture, GfxTexture2D* dstTexture, rgU32 srcMipLevel, rgU32 dstMipLevel, rgU32 mipLevelCount)
{
    id<MTLTexture> src = getMTLTexture(srcTexture);
    id<MTLTexture> dst = getMTLTexture(dstTexture);
    
    [asMTLBlitCommandEncoder(mtlBlitCommandEncoder) copyFromTexture:src sourceSlice:0 sourceLevel:srcMipLevel toTexture:dst destinationSlice:0 destinationLevel:dstMipLevel sliceCount:1 levelCount:mipLevelCount];
}

RG_END_RG_NAMESPACE
#endif
