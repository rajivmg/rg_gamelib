#if defined(RG_METAL_RNDR)
#include "rg_gfx.h"

#include <simd/simd.h>

#include <EASTL/fixed_vector.h>
#include <EASTL/string.h>

#import <Metal/Metal.h>
#import <Metal/MTLArgumentEncoder.h>
#import <Metal/MTLBuffer.h>
#include <QuartzCore/CAMetalLayer.h>

#include <sstream>

RG_BEGIN_NAMESPACE
RG_GFX_BEGIN_NAMESPACE

#include "shaders/metal/imm_shader.inl"
#include "shaders/metal/histogram_shader.inl"

gfx::Mtl* mtl = nullptr;

static const rgU32 kBindlessTextureSetBinding = 7;
static const rgU32 kFrameParamsSetBinding = 6;

static id<MTLDevice> getMTLDevice()
{
    return (__bridge id<MTLDevice>)(mtl->device);
}

MTLResourceOptions toMTLResourceOptions(GfxBufferUsage usage, rgBool dynamic)
{
    MTLResourceOptions options = MTLResourceStorageModeManaged;
    if(dynamic)
    {
        options |= MTLResourceCPUCacheModeWriteCombined;
    }
    return options;

    /*
    // TODO: recheck the values
    switch(usage)
    {
        case GfxBufferUsage_VertexBuffer:
        case GfxBufferUsage_IndexBuffer:
        case GfxBufferUsage_StructuredBuffer:
        {
            return MTLResourceStorageModeManaged;
        } break;
            
        case GfxBufferUsage_ShaderRW:
        case GfxBufferUsage_ConstantBuffer:
        {
            return MTLResourceCPUCacheModeWriteCombined | MTLResourceStorageModeManaged;
        } break;
            
        default:
            rgAssert(!"Invalid resource usage option");
    }
     */
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
        result |= MTLTextureUsageShaderWrite;
    }
    
    if(usage == GfxTextureUsage_RenderTarget)
    {
        result |= MTLTextureUsageRenderTarget;
    }
    
    if(usage == GfxTextureUsage_DepthStencil)
    {
        result |= MTLTextureUsageRenderTarget;
    }
    
    return result;
}

MTLResourceUsage toMTLResourceOptions(GfxTextureUsage usage)
{
    MTLResourceOptions options = MTLStorageModeManaged;
    
    if(usage == GfxTextureUsage_ShaderReadWrite)
    {
        options |= MTLResourceCPUCacheModeWriteCombined;
    }

    return options;
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

id<MTLTexture> getMTLTexture(GfxTexture2D* obj)
{
     return (__bridge id<MTLTexture>)(obj->mtlTexture);
}

id<MTLBuffer> getActiveMTLBuffer(GfxBuffer* ptr)
{
    return (__bridge id<MTLBuffer>)(ptr->mtlBuffers[ptr->activeSlot]);
}

MTLClearColor toMTLClearColor(rgFloat4* color)
{
    return MTLClearColorMake(color->r, color->g, color->b, color->a);
}

id<MTLRenderPipelineState> toMTLRenderPipelineState(GfxGraphicsPSO* obj)
{
    return (__bridge id<MTLRenderPipelineState>)(obj->mtlPSO);
}

id<MTLRenderCommandEncoder> getMTLRenderEncoder()
{
    return (__bridge id<MTLRenderCommandEncoder>)mtl->renderEncoder;
}

id<MTLCommandBuffer> getMTLCommandBuffer()
{
    return (__bridge id<MTLCommandBuffer>)mtl->commandBuffer;
}

id<MTLRenderCommandEncoder> asMTLRenderCommandEncoder(void* ptr)
{
    return (__bridge id<MTLRenderCommandEncoder>)ptr;
}

id<MTLBlitCommandEncoder> asMTLBlitCommandEncoder(void* ptr)
{
    return (__bridge id<MTLBlitCommandEncoder>)ptr;
}

id<MTLRenderCommandEncoder> getMTLRenderCommandEncoder()
{
    return (__bridge id<MTLRenderCommandEncoder>)currentRenderCmdEncoder->renderCmdEncoder;
}

MTLDataType toMTLDataType(GfxShaderArgType argType)
{
    switch(argType)
    {
        case GfxShaderArgType_ConstantBuffer:
            return MTLDataTypePointer;
            break;
        case GfxShaderArgType_ROTexture:
        case GfxShaderArgType_RWTexture:
            return MTLDataTypeTexture;
            break;
        case GfxShaderArgType_ROBuffer:
        case GfxShaderArgType_RWBuffer:
            return MTLDataTypePointer;
            break;
        case GfxShaderArgType_SamplerState:
            return MTLDataTypeSampler;
            break;
        default:
            rgAssert(!"MTL: Invalid argType");
            return;
    }
}

MTLArgumentAccess toMTLArgumentAccess(GfxShaderArgType argType)
{
    switch(argType)
    {
        case GfxShaderArgType_ConstantBuffer:
        case GfxShaderArgType_ROTexture:
        case GfxShaderArgType_ROBuffer:
        case GfxShaderArgType_SamplerState:
            return MTLArgumentAccessReadOnly;
            break;

        case GfxShaderArgType_RWTexture:
        case GfxShaderArgType_RWBuffer:
            return MTLArgumentAccessReadWrite;
            break;
            
        default:
            rgAssert(!"MTL: Invalid argType");
            return;
    }
}

GfxTexture2D createGfxTexture2DFromMTLDrawable(id<CAMetalDrawable> drawable)
{
    GfxTexture2D texture2d;
    texture2d.mtlTexture = (__bridge MTL::Texture*)drawable.texture;
    return texture2d;
}

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
    
    gfx::createTexture2D("histogramTest", rg::loadTexture("histogram_test.png"), false, GfxTextureUsage_ShaderRead);
    gfx::createBuffer("histogramBuffer", nullptr, sizeof(rgUInt)*255*3, GfxBufferUsage_ShaderRW, false);
}

void testComputeAtomicsRun()
{
    id<MTLComputeCommandEncoder> computeEncoder = [getMTLCommandBuffer() computeCommandEncoder];
    [computeEncoder setComputePipelineState:histogramComputePipeline];
    [computeEncoder setTexture:getMTLTexture(gfx::findTexture2D("histogramTest")) atIndex:0];
    [computeEncoder setBuffer:getActiveMTLBuffer(gfx::findBuffer("histogramBuffer")) offset:0 atIndex:0];
    //[computeEncoder setTexture:(id<MTLTexture>)getActiveMTLBuffer(gfx::findBuffer("histogramBuffer")) atIndex:1];
    [computeEncoder setBuffer:getActiveMTLBuffer(gfx::findBuffer("histogramBuffer")) offset:0 atIndex:1];
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

struct AllocationResult
{
    void* ptr;
    rgU32 offset;
};

class FrameAllocator
{
public:
    FrameAllocator(id<MTLDevice> device_, rgU32 capacity_, MTLResourceOptions resourceOptions_)
    {
        offset = 0;
        capacity = capacity_;
        buffer = [device_ newBufferWithLength:capacity options:resourceOptions_];
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
    
    AllocationResult allocate(char const* tag, rgU32 size)
    {
        rgAssert(offset + size <= capacity);

        void* ptr = (bufferPtr + offset);
        
        AllocationResult result;
        result.offset = offset;
        result.ptr = ptr;

        rgU32 alignment = 4;
        offset += (size + alignment - 1) & ~(alignment - 1);
        
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

static FrameAllocator* frameAllocators[RG_MAX_FRAMES_IN_FLIGHT];
static id<MTLHeap> bindlessTextureHeap;
static id<MTLArgumentEncoder> bindlessTextureArgEncoder;
static id<MTLBuffer> bindlessTextureArgBuffer;

static id<MTLSharedEvent> frameFenceEvent;
static MTLSharedEventListener* frameFenceEventListener;
static rgU64 frameFenceValues[RG_MAX_FRAMES_IN_FLIGHT];

FrameAllocator* getFrameAllocator()
{
    return frameAllocators[g_FrameIndex];
}

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
    //

    mtl->framesInFlightSemaphore = dispatch_semaphore_create(RG_MAX_FRAMES_IN_FLIGHT);

    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // Make handle value start from 1. Default 0 should be uninitialized handle
    
    //GfxTexture2D t2dptr = gfxNewTexture2D(rg::loadTexture("T.tga"), GfxTextureUsage_ShaderRead);
    
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    
    @autoreleasepool
    {
        // Crate frame sync events
        frameFenceEvent = [getMTLDevice() newSharedEvent];
        dispatch_queue_t frameFenceDispatchQueue = dispatch_queue_create("rg.gamelib.mtl.frameFenceDispatchQueue", NULL);
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
            eastl::string tag = "renderTarget";
            gfx::renderTarget[i] = createTexture2D(tag.c_str(), nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_B8G8R8A8_UNORM, false, GfxTextureUsage_RenderTarget);
        }
        gfx::depthStencilBuffer = createTexture2D("depthStencilBuffer", nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D16_UNORM, false, GfxTextureUsage_DepthStencil);
        
        
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
        
        // Create a common argument encoder for all rendering needs
        NSMutableArray<MTLArgumentDescriptor*>* descriptorLayout[GfxUpdateFreq_COUNT];
        
        for(rgU32 frameFreq = 0; frameFreq < GfxUpdateFreq_COUNT; ++frameFreq)
        {
            descriptorLayout[frameFreq] = [NSMutableArray<MTLArgumentDescriptor*> new];
            
            rgU32 argIndex = 0;
            for(rgU32 argType = 0; argType < (GfxShaderArgType_COUNT - 0); ++argType)
            {
                MTLDataType dataType = toMTLDataType((GfxShaderArgType)argType);
                MTLArgumentAccess argAccess = toMTLArgumentAccess((GfxShaderArgType)argType);
                for(rgU32 i = 0; i < shaderArgsLayout[argType][frameFreq]; ++i)
                {
                    MTLArgumentDescriptor* arg = [MTLArgumentDescriptor argumentDescriptor];
                    arg.index = argIndex;
                    arg.dataType = dataType;
                    arg.access = argAccess;
                    if(argType == (rgU32)GfxShaderArgType_ROTexture
                       || argType == (rgU32)GfxShaderArgType_RWTexture)
                    {
                        arg.textureType = MTLTextureType2D;
                    }
                    [descriptorLayout[frameFreq] addObject:arg];
                    ++argIndex;
                }
            }
            
            mtl->argumentEncoder[frameFreq] = [getMTLDevice() newArgumentEncoderWithArguments:descriptorLayout[frameFreq]];
        }
        
        // Test argument encoder layout issues
#if 1
        MTLArgumentDescriptor* frameConstDescriptor1 = [MTLArgumentDescriptor argumentDescriptor];
        frameConstDescriptor1.index = 0;
        frameConstDescriptor1.dataType = MTLDataTypePointer;
        frameConstDescriptor1.access = MTLArgumentAccessReadOnly;
        
        MTLArgumentDescriptor* frameConstDescriptor2 = [MTLArgumentDescriptor argumentDescriptor];
        frameConstDescriptor2.index = 1;
        frameConstDescriptor2.dataType = MTLDataTypePointer;
        frameConstDescriptor2.access = MTLArgumentAccessReadOnly;
        
        MTLArgumentDescriptor* frameConstDescriptor3 = [MTLArgumentDescriptor argumentDescriptor];
        frameConstDescriptor3.index = 10;
        frameConstDescriptor3.dataType = MTLDataTypePointer;
        frameConstDescriptor3.access = MTLArgumentAccessReadOnly;
        frameConstBufferArgEncoder = [getMTLDevice() newArgumentEncoderWithArguments:@[frameConstDescriptor1, frameConstDescriptor2, frameConstDescriptor3]];
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

AllocationResult bindlessTextureArgBufferAlloc;

void startNextFrame()
{
    //dispatch_semaphore_wait(mtl->framesInFlightSemaphore, DISPATCH_TIME_FOREVER);
    
    rgU64 prevFrameFenceValue = (g_FrameIndex != -1) ? frameFenceValues[g_FrameIndex] : 0;
    
    g_FrameIndex = (g_FrameIndex + 1) % RG_MAX_FRAMES_IN_FLIGHT;
    
    // check if this next frame is finised on the GPU
    rgU64 nextFrameFenceValueToWaitFor = frameFenceValues[g_FrameIndex];
    while(frameFenceEvent.signaledValue < nextFrameFenceValueToWaitFor)
    {
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
    blitCmdEncoder->copyTexture(gfx::renderTarget[g_FrameIndex],
                                &drawableTexture2D, 0, 0, 1);
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
    
}

void creatorGfxBuffer(char const* tag, void* buf, rgU32 size, GfxBufferUsage usage, rgBool dynamic, GfxBuffer* obj)
{
    if(dynamic == false && usage != GfxBufferUsage_ShaderRW)
    {
        rgAssert(buf != NULL);
    }
    
    MTLResourceOptions options = toMTLResourceOptions(usage, dynamic);
    
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        id<MTLBuffer> mtlBuffer;
        if(buf != NULL)
        {
            mtlBuffer = [getMTLDevice() newBufferWithBytes:buf length:(NSUInteger)size options:options];
        }
        else
        {
            mtlBuffer = [getMTLDevice() newBufferWithLength:(NSUInteger)size options:options];
        }
        mtlBuffer.label = [NSString stringWithUTF8String:tag];
        
        obj->mtlBuffers[i] = (__bridge MTL::Buffer*)mtlBuffer;
        
        if(dynamic == false)
        {
            break;
        }
    }
}

void updaterGfxBuffer(void* data, rgUInt size, rgUInt offset, GfxBuffer* buffer)
{
    rgAssert(buffer);
    
    if(buffer->dynamic == true)
    {
        if(++buffer->activeSlot >= RG_MAX_FRAMES_IN_FLIGHT)
        {
            buffer->activeSlot = 0;
        }
        
        memcpy((rgU8*)buffer->mtlBuffers[buffer->activeSlot]->contents() + offset, data, size);
        buffer->mtlBuffers[buffer->activeSlot]->didModifyRange(NS::Range(offset, size));
    }
    else
    {
        rgAssert(!"Unimplemented or incorrect usages mode");
    }
}

void destroyerGfxBuffer(GfxBuffer* obj)
{
    for(rgInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        obj->mtlBuffers[i]->release();
        
        if(obj->dynamic == false)
        {
            break;
        }
    }
}

NSDictionary* macrosToNSDictionary(char const* macros)
{
    // TODO: Remember to free macroDictionary
    NSMutableDictionary *macroDictionary = [[NSMutableDictionary alloc] init];
    
    char const* cursor = macros;
    std::stringstream macrostring;
    while(1)
    {
        if(*cursor == ' ' || *cursor == ',' || *cursor == '\0')
        {
            if(macrostring.str().length() > 0)
            {
                [macroDictionary setValue:@"1" forKey:[NSString stringWithUTF8String:macrostring.str().c_str()]];
                macrostring.str(std::string());
            }
            if(*cursor == '\0')
            {
                break;
            }
            //ppDict->dictionary(NS::UInteger(1), NS::String::string(macro.str().c_str(), NS::UTF8StringEncoding));
        }
        else
        {
            macrostring.put(*cursor);
        }
        ++cursor;
    }
    
    //NSLog(@"PreprocessorDict Keys: %@\n", [macroDictionary allKeys]);
    //NSLog(@"PreprocessorDict Vals: %@\n", [macroDictionary allValues]);
    NSLog(@"PreprocessorDict KeyVals: %@\n", macroDictionary);
    
    return (NSDictionary*)(macroDictionary);
}

void creatorGfxGraphicsPSO(char const* tag, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO* obj)
{
    @autoreleasepool
    {
        // --- compile shader
        NSDictionary* shaderMacros = macrosToNSDictionary(shaderDesc->macros);
        
        MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
        [compileOptions setPreprocessorMacros:shaderMacros];
        [shaderMacros autorelease];
        
        NSError* err;
        id<MTLLibrary> shaderLib = [getMTLDevice() newLibraryWithSource:[NSString stringWithUTF8String:shaderDesc->shaderSrcCode] options:compileOptions error:&err];
        if(err)
        {
            printf("%s\n", [[err localizedDescription]UTF8String]);
            rgAssert(!"newLibrary error");
        }
        [compileOptions release];
    
        // --- create PSO
        id<MTLFunction> vs, fs;
        if(shaderDesc->vsEntryPoint)
        {
            vs = [shaderLib newFunctionWithName:[NSString stringWithUTF8String: shaderDesc->vsEntryPoint]];
        }
        if(shaderDesc->fsEntryPoint)
        {
            fs = [shaderLib newFunctionWithName:[NSString stringWithUTF8String: shaderDesc->fsEntryPoint]];
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
        
        // TODO TODO TODO TODO REMOVE
        // TODO TODO TODO TODO REMOVE
        MTLAutoreleasedRenderPipelineReflection reflectionInfo;
        // TODO TODO TODO TODO REMOVE
        // TODO TODO TODO TODO REMOVE

        //[getMTLDevice() newRenderPipelineStateWithDescriptor:psoDesc error:&err];
        id<MTLRenderPipelineState> pso = [getMTLDevice() newRenderPipelineStateWithDescriptor:psoDesc options:MTLPipelineOptionArgumentInfo | MTLPipelineOptionBufferTypeInfo reflection:&reflectionInfo error:&err];
        
        if(err)
        {
            printf("%s\n", err.localizedDescription.UTF8String);
            rgAssert(!"newRenderPipelineState error");
        }
        
        obj->mtlPSO = pso;
        
        [psoDesc release];
        [fs release];
        [vs release];
        [shaderLib release];
    }
}

void destroyerGfxGraphicsPSO(GfxGraphicsPSO* obj)
{
    [toMTLRenderPipelineState(obj) release];
}

void creatorGfxTexture2D(char const* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, rgBool genMips, GfxTextureUsage usage, GfxTexture2D* obj)
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

void destroyerGfxTexture2D(GfxTexture2D* obj)
{
    obj->mtlTexture->release();
}

///

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

id<MTLSamplerState> asMTLSamplerState(void* ptr)
{
    return (__bridge id<MTLSamplerState>)(ptr);
}

void creatorGfxSamplerState(char const* tag, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSamplerState* obj)
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
    
    obj->mtlSamplerState = (__bridge void*)samplerState;
}

void destroyerGfxSamplerState(GfxSamplerState* obj)
{
    [asMTLSamplerState(obj->mtlSamplerState) release];
}

RG_GFX_END_NAMESPACE

using namespace gfx;

void GfxRenderCmdEncoder::begin(GfxRenderPass* renderPass)
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
    [renderPassDesc autorelease];
    
    MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc] init];
    depthStencilDesc.depthWriteEnabled = true;
    id<MTLDepthStencilState> dsState = [gfx::getMTLDevice() newDepthStencilStateWithDescriptor:depthStencilDesc];
    [depthStencilDesc release];
    
    [mtlRenderEncoder setDepthStencilState:dsState];
    
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
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setRenderPipelineState:gfx::toMTLRenderPipelineState(pso)];
}

void GfxRenderCmdEncoder::drawTexturedQuads(TexturedQuads* quads)
{
    if(quads->size() < 1)
    {
        return;
        rgAssert("No textured quads to draw");
    }
    
    eastl::vector<gfx::SimpleVertexFormat> vertices;
    eastl::vector<gfx::SimpleInstanceParams> instanceParams;
    
    genTexturedQuadVertices(quads, &vertices, &instanceParams);
    
    if(gfx::rcTexturedQuadsVB == NULL)
    {
        gfx::rcTexturedQuadsVB = gfx::createBuffer("texturedQuadsVB", nullptr, rgMEGABYTE(16), GfxBufferUsage_VertexBuffer, true);
    }
        
    if(gfx::rcTexturedQuadsInstParams == NULL)
    {
        gfx::rcTexturedQuadsInstParams = gfx::createBuffer("texturedQuadInstParams", nullptr, rgMEGABYTE(4), GfxBufferUsage_StructuredBuffer, true);
    
        gfx::Camera cam;
        
        rgFloat* orthoMatrix = toFloatPtr(gfx::orthographicMatrix);
        rgFloat* viewMatrix = toFloatPtr(gfx::viewMatrix);
        
        for(rgInt i = 0; i < 16; ++i)
        {
            cam.projection[i] = orthoMatrix[i];
            cam.view[i] = viewMatrix[i];
        }
        
        gfx::cameraBuffer = [gfx::getMTLDevice() newBufferWithLength:sizeof(gfx::Camera) options:gfx::toMTLResourceOptions(GfxBufferUsage_ConstantBuffer, true)];
        gfx::Camera* p = (gfx::Camera*)[gfx::cameraBuffer contents];
        *p = cam;
        [gfx::cameraBuffer didModifyRange:NSMakeRange(0, sizeof(gfx::Camera))];
        
        [gfx::frameConstBufferArgEncoder setArgumentBuffer:gfx::frameConstBufferArgBuffer offset:0];
        [gfx::frameConstBufferArgEncoder setBuffer:gfx::cameraBuffer offset:0 atIndex:10];
    }
    
    // TODO: use FrameAllocators
    // TODO: use FrameAllocators
    // TODO: use FrameAllocators
    
    GfxBuffer* texturesQuadVB = gfx::rcTexturedQuadsVB;
    GfxBuffer* texturedQuadInstParams = gfx::rcTexturedQuadsInstParams;
    updateBuffer("texturedQuadsVB", &vertices.front(), vertices.size() * sizeof(gfx::SimpleVertexFormat), 0);
    updateBuffer("texturedQuadInstParams", &instanceParams.front(), instanceParams.size() * sizeof(gfx::SimpleInstanceParams), 0);
    
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) useResource:gfx::cameraBuffer usage:MTLResourceUsageRead stages: MTLRenderStageVertex];
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) useResource:gfx::frameConstBufferArgBuffer usage:MTLResourceUsageRead stages: MTLRenderStageVertex];
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) useResource:gfx::getActiveMTLBuffer(texturesQuadVB) usage:MTLResourceUsageRead stages: MTLRenderStageVertex];
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) useResource:gfx::getActiveMTLBuffer(texturedQuadInstParams) usage:MTLResourceUsageRead stages: MTLRenderStageFragment];
    
    
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setVertexBuffer:gfx::frameConstBufferArgBuffer offset:0 atIndex:0];
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setFragmentBuffer:gfx::getActiveMTLBuffer(texturedQuadInstParams) offset:0 atIndex:4];
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setVertexBuffer:gfx::getActiveMTLBuffer(texturesQuadVB) offset:0 atIndex:1];
    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setCullMode:MTLCullModeNone];

    [gfx::asMTLRenderCommandEncoder(renderCmdEncoder) drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6 instanceCount:vertices.size()/6];
}

// -------------------------------------------

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

RG_END_NAMESPACE
#endif
