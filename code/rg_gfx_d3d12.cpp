#if defined(RG_D3D12_RNDR)
#include "rg_gfx.h"

RG_BEGIN_NAMESPACE
RG_GFX_BEGIN_NAMESPACE

D3d d3d; // TODO: Make this a pointer

// SECTION BEGIN -
// -----------------------------------------------
// Helper Functions
// -----------------------------------------------
inline void _BreakIfFail(HRESULT hr)
{
    if(FAILED(hr))
    {
        rgAssert("D3D call failed!");
    }
}

#define BreakIfFail(x) _BreakIfFail(x)

//static D3d* d3d()
//{
//    return &d3d;
//}

static ComPtr<ID3D12Device> device()
{
    return d3d.device;
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

// -----------------------------------------------
// Helper Functions
// -----------------------------------------------
// SECTION ENDS -

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
    BreakIfFail(d3d.commandQueue->Signal(d3d.frameFence.Get(), d3d.frameFenceValues[g_FrameIndex]));
    BreakIfFail(d3d.frameFence->SetEventOnCompletion(d3d.frameFenceValues[g_FrameIndex], d3d.frameFenceEvent));
    ::WaitForSingleObject(d3d.frameFenceEvent, INFINITE);

    d3d.frameFenceValues[g_FrameIndex] += 1;
}

// https://github.com/microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12Fullscreen/src/D3D12Fullscreen.cpp
// TODO: Have this for now.. We won't have to worry about modifying descriptortables at runtime

ComPtr<ID3D12DescriptorHeap> createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, rgUInt descriptorsCount, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = type;
    desc.NumDescriptors = descriptorsCount;
    desc.Flags = flags;

    BreakIfFail(device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

ComPtr<ID3D12CommandAllocator> createCommandAllocator(D3D12_COMMAND_LIST_TYPE commandListType)
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    BreakIfFail(device()->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&commandAllocator)));
    
    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> createGraphicsCommandList(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> commandAllocator, ID3D12PipelineState* pipelineState)
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    BreakIfFail(device()->CreateCommandList(0, type, commandAllocator.Get(), pipelineState, IID_PPV_ARGS(&commandList)));
    BreakIfFail(commandList->Close());
    return commandList;
}

// SECTION BEGIN -
// -----------------------------------------------
// GFX Functions
// -----------------------------------------------
rgInt init()
{
    // TODO: correctly initialize d3d12 device https://walbourn.github.io/anatomy-of-direct3d-12-create-device/

    // create factory
    UINT factoryCreateFlag = 0;
#if defined(_DEBUG)
    factoryCreateFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif
    BreakIfFail(CreateDXGIFactory2(factoryCreateFlag, __uuidof(d3d.dxgiFactory), (void**)&(d3d.dxgiFactory)));

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
    getHardwareAdapter(d3d.dxgiFactory.Get(), &hardwareAdapter, false);
    BreakIfFail(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&(d3d.device)));
    
    // create command queue
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    BreakIfFail(device()->CreateCommandQueue(&commandQueueDesc, __uuidof(ID3D12CommandQueue), (void**)&(d3d.commandQueue)));

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
    BreakIfFail(d3d.dxgiFactory->CreateSwapChainForHwnd(
        d3d.commandQueue.Get(),
        hWnd,
        &swapchainDesc,
        nullptr,
        nullptr,
        &dxgiSwapchain1
    ));
    BreakIfFail(d3d.dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
    BreakIfFail(dxgiSwapchain1.As(&d3d.dxgiSwapchain));

    // create swapchain RTV
    d3d.rtvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RG_MAX_FRAMES_IN_FLIGHT, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    d3d.rtvDescriptorSize = device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(d3d.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for(rgUInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        ComPtr<ID3D12Resource> texResource;
        BreakIfFail(d3d.dxgiSwapchain->GetBuffer(i, IID_PPV_ARGS(&texResource)));
        device()->CreateRenderTargetView(texResource.Get(), nullptr, rtvDescriptorHandle);
        rtvDescriptorHandle.Offset(1, d3d.rtvDescriptorSize);

        D3D12_RESOURCE_DESC desc = texResource->GetDesc();
        GfxTexture2D* tex2d = rgNew(GfxTexture2D);
        strncpy(tex2d->tag, "RenderTarget", 32);
        tex2d->width = (rgUInt)desc.Width;
        tex2d->height = (rgUInt)desc.Height;
        tex2d->usage = GfxTextureUsage_RenderTarget;
        tex2d->format = TinyImageFormat_FromDXGI_FORMAT((TinyImageFormat_DXGI_FORMAT)desc.Format);
        tex2d->d3dTexture = texResource;
        gfx::renderTarget[i] = tex2d;
    }

    d3d.dsvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
    gfx::depthStencilBuffer = createTexture2D("DepthStencilTarget", nullptr, g_WindowInfo.width, g_WindowInfo.height, TinyImageFormat_D32_SFLOAT, false, GfxTextureUsage_DepthStencil);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
    dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsDesc.Texture2D.MipSlice = 0;
    dsDesc.Flags = D3D12_DSV_FLAG_NONE;
    device()->CreateDepthStencilView(gfx::depthStencilBuffer->d3dTexture.Get(), &dsDesc, d3d.dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    d3d.cbvSrvUavDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 400000, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    for(rgUInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        d3d.commandAllocator[i] = createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    d3d.commandList = createGraphicsCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, d3d.commandAllocator[0], nullptr);

    ///// 
    // empty root signature
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
        rootSigDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        BreakIfFail(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        BreakIfFail(device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(d3d.dummyRootSignature), (void**)&(d3d.dummyRootSignature)));
    }

    GfxVertexInputDesc simpleVertexDesc = {};
    simpleVertexDesc.elementsCount = 2;
    simpleVertexDesc.elements[0].semanticName = "POSITION";
    simpleVertexDesc.elements[0].semanticIndex = 0;
    simpleVertexDesc.elements[0].format = TinyImageFormat_R32G32B32_SFLOAT;
    simpleVertexDesc.elements[0].slot = 0;
    simpleVertexDesc.elements[0].offset = 0;
    simpleVertexDesc.elements[1].semanticName = "COLOR";
    simpleVertexDesc.elements[1].semanticIndex = 0;
    simpleVertexDesc.elements[1].format = TinyImageFormat_R32G32B32A32_SFLOAT;
    simpleVertexDesc.elements[1].slot = 0;
    simpleVertexDesc.elements[1].offset = 12;

    GfxShaderDesc simple2dShaderDesc = {};
    simple2dShaderDesc.shaderSrcCode = "";
    simple2dShaderDesc.vsEntryPoint = "simple2d_VS";
    simple2dShaderDesc.fsEntryPoint = "simple2d_FS";
    simple2dShaderDesc.macros = "RIGHT";

    GfxRenderStateDesc simple2dRenderStateDesc = {};
    simple2dRenderStateDesc.colorAttachments[0].pixelFormat = TinyImageFormat_B8G8R8A8_UNORM;
    simple2dRenderStateDesc.colorAttachments[0].blendingEnabled = true;
    simple2dRenderStateDesc.depthStencilAttachmentFormat = TinyImageFormat_D32_SFLOAT;
    simple2dRenderStateDesc.depthWriteEnabled = true;
    simple2dRenderStateDesc.depthCompareFunc = GfxCompareFunc_Less;
    GfxGraphicsPSO* simplePSO = createGraphicsPSO("simple2d", &simpleVertexDesc, &simple2dShaderDesc, &simple2dRenderStateDesc);
    d3d.dummyPSO = simplePSO->d3dPSO;

    {
        rgFloat triangleVertices[] =
        {
            0.0f, 0.3f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.4f, -0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            -0.25f, -0.1f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,

            0.0f, 0.25f, 0.1f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.25f, -0.25f, 0.1f, 0.0f, 1.0f, 0.0f, 1.0f,
            -0.25f, -0.25f, 0.1f, 0.0f, 0.0f, 1.0f, 1.0f
        };

        rgUInt vbSize = sizeof(triangleVertices);

        BreakIfFail(device()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vbSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof(d3d.triVB),
            (void**)&(d3d.triVB)
        ));

        rgU8* vbPtr;;
        CD3DX12_RANGE readRange(0, 0);
        BreakIfFail(d3d.triVB->Map(0, &readRange, (void**)&vbPtr));
        memcpy(vbPtr, triangleVertices, vbSize);
        d3d.triVB->Unmap(0, nullptr);

        d3d.triVBView.BufferLocation = d3d.triVB->GetGPUVirtualAddress();
        d3d.triVBView.StrideInBytes = 28;
        d3d.triVBView.SizeInBytes = vbSize;
    }

    {
        // Create a fence with initial value 0 which is equal to d3d.nextFrameFenceValues[g_FrameIndex=0]
        BreakIfFail(device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(d3d.frameFence), (void**)&(d3d.frameFence)));
        
        // Create an framefence event
        d3d.frameFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if(d3d.frameFenceEvent == nullptr)
        {
            BreakIfFail(HRESULT_FROM_WIN32(::GetLastError()));
        }

        //// when the command queue has finished its work, signal 1 to frameFence
        //BreakIfFail(d3d.commandQueue->Signal(d3d.frameFence.Get(), 1));

        //// Wait until the event is triggered
        //BreakIfFail(d3d.frameFence->SetEventOnCompletion(1, d3d.frameFenceEvent));
        //::WaitForSingleObject(d3d.frameFenceEvent, INFINITE);
    }

    return 0;
}

void destroy()
{
    waitForGpu();
    ::CloseHandle(d3d.frameFenceEvent);
}

rgInt draw()
{
    BreakIfFail(d3d.commandAllocator[g_FrameIndex]->Reset());
    BreakIfFail(d3d.commandList->Reset(d3d.commandAllocator[g_FrameIndex].Get(), d3d.dummyPSO.Get()));

    ID3D12GraphicsCommandList* commandList = d3d.commandList.Get();
    commandList->SetGraphicsRootSignature(d3d.dummyRootSignature.Get());

    CD3DX12_VIEWPORT vp(0.0f, 0.0f, (rgFloat)g_WindowInfo.width, (rgFloat)g_WindowInfo.height);
    commandList->RSSetViewports(1, &vp);
    CD3DX12_RECT scissorRect(0, 0, g_WindowInfo.width, g_WindowInfo.height);
    commandList->RSSetScissorRects(1, &scissorRect);

    GfxTexture2D* activeRenderTarget = gfx::renderTarget[g_FrameIndex];
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(activeRenderTarget->d3dTexture.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(d3d.rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), g_FrameIndex, d3d.rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(d3d.dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

    const rgFloat clearColor[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &d3d.triVBView);
    //commandList->DrawInstanced(6, 1, 0, 0);
    commandList->DrawInstanced(3, 1, 0, 0);
    commandList->DrawInstanced(3, 1, 3, 0);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(activeRenderTarget->d3dTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    BreakIfFail(commandList->Close());

    ID3D12CommandList* commandLists[] = { d3d.commandList.Get() };
    d3d.commandQueue->ExecuteCommandLists(1, commandLists);
    BreakIfFail(d3d.dxgiSwapchain->Present(1, 0));

    return 0;
}

void checkerWaitTillFrameCompleted(rgInt frameIndex)
{
    rgAssert(frameIndex >= 0);
    UINT64 valueToWaitFor = d3d.frameFenceValues[frameIndex];
    while(d3d.frameFence->GetCompletedValue() < valueToWaitFor)
    {
        BreakIfFail(d3d.frameFence->SetEventOnCompletion(valueToWaitFor, d3d.frameFenceEvent));
        ::WaitForSingleObject(d3d.frameFenceEvent, INFINITE);
    }
}

void startNextFrame()
{
    UINT64 prevFrameFenceValue = (g_FrameIndex != -1) ? d3d.frameFenceValues[g_FrameIndex] : 0;

    g_FrameIndex = d3d.dxgiSwapchain->GetCurrentBackBufferIndex();

    // check if this next frame is finised on the GPU
    UINT64 nextFrameFenceValueToWaitFor = d3d.frameFenceValues[g_FrameIndex];
    while(d3d.frameFence->GetCompletedValue() < nextFrameFenceValueToWaitFor)
    {
        BreakIfFail(d3d.frameFence->SetEventOnCompletion(nextFrameFenceValueToWaitFor, d3d.frameFenceEvent));
        ::WaitForSingleObject(d3d.frameFenceEvent, INFINITE);
    }

    // This frame fence value is one more than prev frame fence value
    d3d.frameFenceValues[g_FrameIndex] = prevFrameFenceValue + 1;

    draw();
}

void endFrame()
{
    UINT64 fenceValueToSignal = d3d.frameFenceValues[g_FrameIndex];
    BreakIfFail(d3d.commandQueue->Signal(d3d.frameFence.Get(), fenceValueToSignal));
}

void onSizeChanged()
{
    waitForGpu();


}

// -----------------------------------------------
// GFX Function
// -----------------------------------------------
// SECTION ENDS -




// SECTION BEGIN -
// -----------------------------------------------
// GPU Resource Creators Deleters and Modifers
// -----------------------------------------------

void setterBindlessResource(rgU32 slot, GfxTexture2D* ptr)
{
}

// Buffer
void creatorGfxBuffer(char const* tag, void* buf, rgU32 size, GfxBufferUsage usage, rgBool dynamic, GfxBuffer* obj)
{
    ComPtr<ID3D12Resource> bufferResource;

    rgFloat triangleVertices[] =
    {
        0.0f, 0.3f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.4f, -0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        -0.25f, -0.1f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,

        0.0f, 0.25f, 0.1f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.25f, -0.25f, 0.1f, 0.0f, 1.0f, 0.0f, 1.0f,
        -0.25f, -0.25f, 0.1f, 0.0f, 0.0f, 1.0f, 1.0f
    };

    rgUInt vbSize = sizeof(triangleVertices);

    BreakIfFail(device()->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(vbSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&bufferResource)
    ));

    rgU8* vbPtr;;
    CD3DX12_RANGE readRange(0, 0);
    BreakIfFail(d3d.triVB->Map(0, &readRange, (void**)&vbPtr));
    memcpy(vbPtr, triangleVertices, vbSize);
    d3d.triVB->Unmap(0, nullptr);

    d3d.triVBView.BufferLocation = d3d.triVB->GetGPUVirtualAddress();
    d3d.triVBView.StrideInBytes = 28;
    d3d.triVBView.SizeInBytes = vbSize;
}

void updaterGfxBuffer(void* data, rgUInt size, rgUInt offset, GfxBuffer* buffer)
{

}

void destroyerGfxBuffer(GfxBuffer* obj)
{

}

// Texture
void creatorGfxTexture2D(char const* tag, void* buf, rgUInt width, rgUInt height, TinyImageFormat format, rgBool genMips, GfxTextureUsage usage, GfxTexture2D* obj)
{
    ComPtr<ID3D12Resource> textureResouce;

    DXGI_FORMAT textureFormat = (DXGI_FORMAT)TinyImageFormat_ToDXGI_FORMAT(format);
    
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
    
    D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_COMMON;
    if(usage & GfxTextureUsage_ShaderReadWrite)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resourceState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if(usage & GfxTextureUsage_RenderTarget)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        resourceState |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if(usage & GfxTextureUsage_DepthStencil)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        resourceState |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    if(usage & GfxTextureUsage_ShaderRead)
    {

    }

    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(textureFormat, width, height, 1, 1, 1, 0, resourceFlags);

    BreakIfFail(device()->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        resourceState,
        clearValue,
        IID_PPV_ARGS(&textureResouce)));

    //CD3DX12_CPU_DESCRIPTOR_HANDLE resourceView 

    if(usage & GfxTextureUsage_ShaderReadWrite)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resourceState |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
    if(usage & GfxTextureUsage_RenderTarget)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        resourceState |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    }
    if(usage & GfxTextureUsage_DepthStencil)
    {
        resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        resourceState |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    }
    if(usage & GfxTextureUsage_ShaderRead)
    {

    }

    obj->d3dTexture = textureResouce;

    //GfxTexture2D* dsTex = rgNew(GfxTexture2D);
    //strncpy(dsTex->tag, "DepthStencilTarget", 32);
    //dsTex->width = (rgUInt)dsResdesc.Width;
    //dsTex->height = (rgUInt)dsResdesc.Height;
    //dsTex->usage = GfxTextureUsage_RenderTarget;
    //dsTex->format = TinyImageFormat_FromDXGI_FORMAT((TinyImageFormat_DXGI_FORMAT)dsResdesc.Format);
    //dsTex->d3dTexture = dsResource;
    //gfxCtx()->depthStencilBuffer = gfxNewTexture2D(dsTex);
}

void destroyerGfxTexture2D(GfxTexture2D* obj)
{

}

// PSO
void creatorGfxGraphicsPSO(char const* tag, GfxVertexInputDesc* vertexInputDesc, GfxShaderDesc* shaderDesc, GfxRenderStateDesc* renderStateDesc, GfxGraphicsPSO* obj)
{
    // empty root signature
    ComPtr<ID3D12RootSignature> emptyRootSig;
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
        rootSigDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        BreakIfFail(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        BreakIfFail(device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(emptyRootSig), (void**)&(emptyRootSig)));
    }

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;
    ComPtr<ID3DBlob> computeShader;

    // create shader
    {
#if defined(_DEBUG)
        UINT shaderCompileFlag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT shaderCompileFlag = 0;
#endif

        wchar_t shaderFilePath[256];
        //std::mbstowcs(shaderFilePath, shaderDesc->shaderSrcCode, 256);
        std::mbstowcs(shaderFilePath, "shaders/dx12/simple2d.hlsl", 256);
        
        if(shaderDesc->vsEntryPoint && shaderDesc->fsEntryPoint)
        {
            BreakIfFail(D3DCompileFromFile((LPCWSTR)shaderFilePath, nullptr, nullptr, (LPCSTR)shaderDesc->vsEntryPoint, "vs_5_0", shaderCompileFlag, 0, &vertexShader, nullptr));
            BreakIfFail(D3DCompileFromFile((LPCWSTR)shaderFilePath, nullptr, nullptr, (LPCSTR)shaderDesc->fsEntryPoint, "ps_5_0", shaderCompileFlag, 0, &pixelShader, nullptr));
        }
        else if(shaderDesc->csEntryPoint) // TODO: This is not a part of Graphics PSO
        {
            BreakIfFail(D3DCompileFromFile((LPCWSTR)shaderFilePath, nullptr, nullptr, (LPCSTR)shaderDesc->csEntryPoint, "cs_5_0", shaderCompileFlag, 0, &computeShader, nullptr));
        }
    }

    // create vertex input desc
    eastl::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDesc;
    inputElementDesc.reserve(vertexInputDesc->elementsCount);
    {
        for(rgInt i = 0; i < vertexInputDesc->elementsCount; ++i)
        {
            D3D12_INPUT_ELEMENT_DESC e = {};
            e.SemanticName = vertexInputDesc->elements[i].semanticName;
            e.SemanticIndex = vertexInputDesc->elements[i].semanticIndex;
            e.Format = toDXGIFormat(vertexInputDesc->elements[i].format);
            e.InputSlot = vertexInputDesc->elements[i].slot;
            e.AlignedByteOffset = vertexInputDesc->elements[i].offset;
            e.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            e.InstanceDataStepRate = 0;
            inputElementDesc.push_back(e);
        }
    }

    // create pso
    ComPtr<ID3D12PipelineState> pso;
    {
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

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { &inputElementDesc.front(), (rgUInt)inputElementDesc.size() };
        psoDesc.pRootSignature = emptyRootSig.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
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

        BreakIfFail(device()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));
    }

    obj->d3dPSO = pso;
}

void destroyerGfxGraphicsPSO(GfxGraphicsPSO* obj)
{

}

void creatorGfxSamplerState(char const* tag, GfxSamplerAddressMode rstAddressMode, GfxSamplerMinMagFilter minFilter, GfxSamplerMinMagFilter magFilter, GfxSamplerMipFilter mipFilter, rgBool anisotropy, GfxSamplerState* obj)
{

}

void destroyerGfxSamplerState(GfxSamplerState* obj)
{

}

// -----------------------------------------------
// GPU Resource Creators Deleters and Modifers
// -----------------------------------------------
// SECTION ENDS -

RG_GFX_END_NAMESPACE

// SECTION BEGIN -
// -----------------------------------------------
// RenderCmd Handlers
// -----------------------------------------------

void GfxRenderCmdEncoder::begin(GfxRenderPass* renderPass)
{
    //// create RenderCommandEncoder
    //MTLRenderPassDescriptor* renderPassDesc = [[MTLRenderPassDescriptor alloc]init];

    //for(rgInt c = 0; c < kMaxColorAttachments; ++c)
    //{
    //    if(renderPass->colorAttachments[c].texture == NULL)
    //    {
    //        continue;
    //    }

    //    MTLRenderPassColorAttachmentDescriptor* colorAttachmentDesc = [renderPassDesc colorAttachments][c];
    //    GfxColorAttachmentDesc* colorAttachment = &renderPass->colorAttachments[c];

    //    colorAttachmentDesc.texture = gfx::getMTLTexture(colorAttachment->texture);
    //    colorAttachmentDesc.loadAction = gfx::toMTLLoadAction(colorAttachment->loadAction);
    //    colorAttachmentDesc.storeAction = gfx::toMTLStoreAction(colorAttachment->storeAction);
    //    colorAttachmentDesc.clearColor = gfx::toMTLClearColor(&colorAttachment->clearColor);
    //}
    //MTLRenderPassDepthAttachmentDescriptor* depthAttachmentDesc = [renderPassDesc depthAttachment];
    //depthAttachmentDesc.texture = gfx::getMTLTexture(renderPass->depthStencilAttachmentTexture);
    //depthAttachmentDesc.loadAction = gfx::toMTLLoadAction(renderPass->depthStencilAttachmentLoadAction);
    //depthAttachmentDesc.storeAction = gfx::toMTLStoreAction(renderPass->depthStencilAttachmentStoreAction);
    //depthAttachmentDesc.clearDepth = renderPass->clearDepth;

    //id<MTLRenderCommandEncoder> mtlRenderEncoder = [gfx::getMTLCommandBuffer() renderCommandEncoderWithDescriptor:renderPassDesc];
    //[renderPassDesc autorelease] ;

    //MTLDepthStencilDescriptor* depthStencilDesc = [[MTLDepthStencilDescriptor alloc]init];
    //depthStencilDesc.depthWriteEnabled = true;
    //id<MTLDepthStencilState> dsState = [gfx::getMTLDevice() newDepthStencilStateWithDescriptor:depthStencilDesc];
    //[depthStencilDesc release] ;

    //[mtlRenderEncoder setDepthStencilState : dsState] ;

    //for(GfxTexture2D* texture2d : *gfx::bindlessManagerTexture2D)
    //{
    //    if(texture2d != nullptr)
    //    {
    //        [mtlRenderEncoder useResource : (__bridge id<MTLTexture>)texture2d->mtlTexture usage : MTLResourceUsageRead stages : MTLRenderStageVertex | MTLRenderStageFragment] ;
    //    }
    //}

    //[mtlRenderEncoder setFragmentBuffer : gfx::getActiveMTLBuffer(gfx::mtl->largeArrayTex2DArgBuffer) offset : 0 atIndex : gfx::kBindlessTextureSetBinding] ;

    //renderCmdEncoder = (__bridge void*)mtlRenderEncoder;
    //hasEnded = false;
}

void GfxRenderCmdEncoder::end()
{
    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) endEncoding] ;
    //hasEnded = true;
}

void GfxRenderCmdEncoder::pushDebugTag(const char* tag)
{
    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) pushDebugGroup:[NSString stringWithUTF8String : tag] ] ;
}

void GfxRenderCmdEncoder::setViewport(rgFloat4 viewport)
{
    //setViewport(viewport.x, viewport.y, viewport.z, viewport.w);
}

void GfxRenderCmdEncoder::setViewport(rgFloat originX, rgFloat originY, rgFloat width, rgFloat height)
{
    //MTLViewport vp;
    //vp.originX = originX;
    //vp.originY = originY;
    //vp.width = width;
    //vp.height = height;
    //vp.znear = 0.0;
    //vp.zfar = 1.0;

    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setViewport:vp] ;
}

void GfxRenderCmdEncoder::setGraphicsPSO(GfxGraphicsPSO* pso)
{
    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setRenderPipelineState:gfx::toMTLRenderPipelineState(pso)] ;
}

void GfxRenderCmdEncoder::drawTexturedQuads(TexturedQuads* quads)
{
    //eastl::vector<gfx::SimpleVertexFormat> vertices;
    //eastl::vector<gfx::SimpleInstanceParams> instanceParams;

    //genTexturedQuadVertices(quads, &vertices, &instanceParams);

    //if(gfx::rcTexturedQuadsVB == NULL)
    //{
    //    gfx::rcTexturedQuadsVB = gfx::createBuffer("texturedQuadsVB", nullptr, rgMEGABYTE(16), GfxBufferUsage_VertexBuffer, true);
    //}

    //if(gfx::rcTexturedQuadsInstParams == NULL)
    //{
    //    gfx::rcTexturedQuadsInstParams = gfx::createBuffer("texturedQuadInstParams", nullptr, rgMEGABYTE(4), GfxBufferUsage_StructuredBuffer, true);
    //}

    //GfxBuffer* texturesQuadVB = gfx::rcTexturedQuadsVB;
    //GfxBuffer* texturedQuadInstParams = gfx::rcTexturedQuadsInstParams;
    //updateBuffer("texturedQuadsVB", &vertices.front(), vertices.size() * sizeof(gfx::SimpleVertexFormat), 0);
    //updateBuffer("texturedQuadInstParams", &instanceParams.front(), instanceParams.size() * sizeof(gfx::SimpleInstanceParams), 0);

    ////
    //struct Camera
    //{
    //    float projection[16];
    //    float view[16];
    //} cam;

    //rgFloat* orthoMatrix = toFloatPtr(gfx::orthographicMatrix);
    //rgFloat* viewMatrix = toFloatPtr(gfx::viewMatrix);

    //for(rgInt i = 0; i < 16; ++i)
    //{
    //    cam.projection[i] = orthoMatrix[i];
    //    cam.view[i] = viewMatrix[i];
    //}

    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setVertexBytes:&cam length : sizeof(Camera) atIndex : 0] ;
    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setFragmentBuffer:gfx::getActiveMTLBuffer(texturedQuadInstParams) offset : 0 atIndex : 4] ;
    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setVertexBuffer:gfx::getActiveMTLBuffer(texturesQuadVB) offset : 0 atIndex : 1] ;
    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) setCullMode:MTLCullModeNone] ;

    //[gfx::asMTLRenderCommandEncoder(renderCmdEncoder) drawPrimitives:MTLPrimitiveTypeTriangle vertexStart : 0 vertexCount : 6 instanceCount : vertices.size() / 6] ;
}

// -------------------------------------------

void GfxBlitCmdEncoder::begin()
{
    //id<MTLBlitCommandEncoder> blitCmdEncoder = [getMTLCommandBuffer() blitCommandEncoder];
    //mtlBlitCommandEncoder = (__bridge void*)blitCmdEncoder;
    //hasEnded = false;
}

void GfxBlitCmdEncoder::end()
{
    //rgAssert(!hasEnded);
    //[asMTLBlitCommandEncoder(mtlBlitCommandEncoder) endEncoding] ;
    //hasEnded = true;
}

void GfxBlitCmdEncoder::pushDebugTag(const char* tag)
{
    //[asMTLBlitCommandEncoder(mtlBlitCommandEncoder) pushDebugGroup:[NSString stringWithUTF8String : tag] ] ;
}

void GfxBlitCmdEncoder::genMips(GfxTexture2D* srcTexture)
{
    //[asMTLBlitCommandEncoder(mtlBlitCommandEncoder) generateMipmapsForTexture:getMTLTexture(srcTexture)] ;
}

void GfxBlitCmdEncoder::copyTexture(GfxTexture2D* srcTexture, GfxTexture2D* dstTexture, rgU32 srcMipLevel, rgU32 dstMipLevel, rgU32 mipLevelCount)
{
    //id<MTLTexture> src = getMTLTexture(srcTexture);
    //id<MTLTexture> dst = getMTLTexture(dstTexture);

    //[asMTLBlitCommandEncoder(mtlBlitCommandEncoder) copyFromTexture:src sourceSlice : 0 sourceLevel : srcMipLevel toTexture : dst destinationSlice : 0 destinationLevel : dstMipLevel sliceCount : 1 levelCount : mipLevelCount] ;
}

// -----------------------------------------------
// RenderCmd Handlers
// -----------------------------------------------
// SECTION ENDS -

#undef BreakIfFail

RG_END_NAMESPACE
#endif