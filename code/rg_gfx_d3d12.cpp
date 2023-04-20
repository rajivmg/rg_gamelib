#if defined(RG_D3D12_RNDR)
#include "rg_gfx.h"

RG_BEGIN_NAMESPACE

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

static GfxCtx::D3d* d3d()
{
    return &g_GfxCtx->d3d;
}

static ComPtr<ID3D12Device> device()
{
    return g_GfxCtx->d3d.device;
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

void waitForPreviousFrame()
{
    rgU64 fence = d3d()->dummyFenceValue;
    BreakIfFail(d3d()->commandQueue->Signal(d3d()->dummyFence.Get(), fence));
    ++d3d()->dummyFenceValue;

    while(d3d()->dummyFence->GetCompletedValue() < fence)
    {
        BreakIfFail(d3d()->dummyFence->SetEventOnCompletion(fence, d3d()->dummyFenceEvent));
        WaitForSingleObject(d3d()->dummyFenceEvent, INFINITE);
    }

    g_FrameIndex = d3d()->dxgiSwapchain->GetCurrentBackBufferIndex();
}

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
rgInt gfxInit()
{
    // TODO: correctly initialize d3d12 device https://walbourn.github.io/anatomy-of-direct3d-12-create-device/

    // create factory
    UINT factoryCreateFlag = 0;
#if defined(_DEBUG)
    factoryCreateFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif
    BreakIfFail(CreateDXGIFactory2(factoryCreateFlag, __uuidof(d3d()->dxgiFactory), (void**)&(d3d()->dxgiFactory)));

    // create debug validation interface
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugInterface;
    BreakIfFail(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();

    //ComPtr<ID3D12InfoQueue> debugInfoQueue;
    //if(SUCCEEDED(d3d()->device.As(&debugInfoQueue)))
    //{
    //    debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    //    debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    //    debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    //}
#endif

    // create device
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    getHardwareAdapter(d3d()->dxgiFactory.Get(), &hardwareAdapter, false);
    BreakIfFail(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&(d3d()->device)));
    
    // create command queue
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    BreakIfFail(device()->CreateCommandQueue(&commandQueueDesc, __uuidof(ID3D12CommandQueue), (void**)&(d3d()->commandQueue)));

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
    BreakIfFail(d3d()->dxgiFactory->CreateSwapChainForHwnd(
        d3d()->commandQueue.Get(),
        hWnd,
        &swapchainDesc,
        nullptr,
        nullptr,
        &dxgiSwapchain1
    ));
    BreakIfFail(d3d()->dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
    BreakIfFail(dxgiSwapchain1.As(&d3d()->dxgiSwapchain));

    // create swapchain RTV
    
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // Make handle value start from 1. Default 0 should be uninitialized handle

    HGfxTexture2D t2dptr = gfxNewTexture2D(rg::loadTexture("T.tga"), GfxTextureUsage_ShaderRead);

    // TODO TODO TODO TODO
    // TODO TODO TODO TODO
    // TODO TODO TODO TODO

    d3d()->rtvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RG_MAX_FRAMES_IN_FLIGHT, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    d3d()->rtvDescriptorSize = device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptorHandle(d3d()->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    for(rgUInt i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        ComPtr<ID3D12Resource> texResource;
        BreakIfFail(d3d()->dxgiSwapchain->GetBuffer(i, IID_PPV_ARGS(&texResource)));
        device()->CreateRenderTargetView(texResource.Get(), nullptr, rtvDescriptorHandle);
        rtvDescriptorHandle.Offset(1, d3d()->rtvDescriptorSize);

        D3D12_RESOURCE_DESC desc = texResource->GetDesc();
        GfxTexture2D* tex2d = rgNew(GfxTexture2D);
        strncpy(tex2d->name, "RenderTarget", 32);
        tex2d->width = (rgUInt)desc.Width;
        tex2d->height = (rgUInt)desc.Height;
        tex2d->usage = GfxTextureUsage_RenderTarget;
        tex2d->pixelFormat = TinyImageFormat_FromDXGI_FORMAT((TinyImageFormat_DXGI_FORMAT)desc.Format);
        tex2d->d3dTexture = texResource;
        gfxCtx()->renderTarget[i] = gfxNewTexture2D(tex2d);
    }

    // create depthstencil
    ComPtr<ID3D12Resource> dsResource;

    D3D12_CLEAR_VALUE depthStencilClearValue = {};
    depthStencilClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilClearValue.DepthStencil = { 1.0f, 0 };

    BreakIfFail(device()->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, g_WindowInfo.width, g_WindowInfo.height, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthStencilClearValue,
        IID_PPV_ARGS(&dsResource)));

    d3d()->dsvDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsDesc = {};
    dsDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsDesc.Texture2D.MipSlice = 0;
    dsDesc.Flags = D3D12_DSV_FLAG_NONE;
    device()->CreateDepthStencilView(dsResource.Get(), &dsDesc, d3d()->dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_RESOURCE_DESC dsResdesc = dsResource->GetDesc();
    GfxTexture2D* dsTex = rgNew(GfxTexture2D);
    strncpy(dsTex->name, "DepthStencilTarget", 32);
    dsTex->width = (rgUInt)dsResdesc.Width;
    dsTex->height = (rgUInt)dsResdesc.Height;
    dsTex->usage = GfxTextureUsage_RenderTarget;
    dsTex->pixelFormat = TinyImageFormat_FromDXGI_FORMAT((TinyImageFormat_DXGI_FORMAT)dsResdesc.Format);
    dsTex->d3dTexture = dsResource;
    gfxCtx()->depthStencilBuffer = gfxNewTexture2D(dsTex);

    ///// 
    // empty root signature
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
        rootSigDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        
        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        BreakIfFail(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        BreakIfFail(device()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), __uuidof(d3d()->dummyRootSignature), (void**)&(d3d()->dummyRootSignature)));
    }

    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        UINT shaderCompileFlag = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT shaderCompileFlag = 0;
#endif

        BreakIfFail(D3DCompileFromFile(L"shaders/dx12/simple.hlsl", nullptr, nullptr, "VS_Main", "vs_5_0", shaderCompileFlag, 0, &vertexShader, nullptr));
        BreakIfFail(D3DCompileFromFile(L"shaders/dx12/simple.hlsl", nullptr, nullptr, "PS_Main", "ps_5_0", shaderCompileFlag, 0, &pixelShader, nullptr));

        D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDesc, rgARRAY_COUNT(inputElementDesc) };
        psoDesc.pRootSignature = d3d()->dummyRootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        //psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;
        BreakIfFail(device()->CreateGraphicsPipelineState(&psoDesc, __uuidof(d3d()->dummyPSO), (void**)&(d3d()->dummyPSO)));
    }
    
    d3d()->commandAllocator = createCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    d3d()->commandList = createGraphicsCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, d3d()->commandAllocator, d3d()->dummyPSO.Get());

    {
        rgFloat triangleVertices[] =
        {
            0.0f, 0.25f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            0.25f, -0.25f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
            -0.25f, -0.25f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f
        };

        rgUInt vbSize = sizeof(triangleVertices);

        BreakIfFail(device()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vbSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            __uuidof(d3d()->triVB),
            (void**)&(d3d()->triVB)
        ));

        rgU8* vbPtr;;
        CD3DX12_RANGE readRange(0, 0);
        BreakIfFail(d3d()->triVB->Map(0, &readRange, (void**)&vbPtr));
        memcpy(vbPtr, triangleVertices, vbSize);
        d3d()->triVB->Unmap(0, nullptr);

        d3d()->triVBView.BufferLocation = d3d()->triVB->GetGPUVirtualAddress();
        d3d()->triVBView.StrideInBytes = 28;
        d3d()->triVBView.SizeInBytes = vbSize;
    }

    {
        BreakIfFail(device()->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(d3d()->dummyFence), (void**)&(d3d()->dummyFence)));
        d3d()->dummyFenceValue = 1;
        d3d()->dummyFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if(d3d()->dummyFenceEvent == nullptr)
        {
            BreakIfFail(HRESULT_FROM_WIN32(::GetLastError()));
        }

        waitForPreviousFrame();
    }

    return 0;
}

void gfxDestroy()
{
    waitForPreviousFrame();
    ::CloseHandle(d3d()->dummyFenceEvent);
}

rgInt gfxDraw()
{
    gfxGetRenderCmdList()->draw();
    gfxGetRenderCmdList()->afterDraw();

    BreakIfFail(d3d()->commandAllocator->Reset());
    BreakIfFail(d3d()->commandList->Reset(d3d()->commandAllocator.Get(), d3d()->dummyPSO.Get()));

    ID3D12GraphicsCommandList* commandList = d3d()->commandList.Get();
    commandList->SetGraphicsRootSignature(d3d()->dummyRootSignature.Get());

    CD3DX12_VIEWPORT vp(0.0f, 0.0f, (rgFloat)g_WindowInfo.width, (rgFloat)g_WindowInfo.height);
    commandList->RSSetViewports(1, &vp);
    CD3DX12_RECT scissorRect(0, 0, g_WindowInfo.width, g_WindowInfo.height);
    commandList->RSSetScissorRects(1, &scissorRect);

    GfxTexture2D* activeRenderTarget = gfxTexture2DPtr(gfxCtx()->renderTarget[g_FrameIndex]);
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(activeRenderTarget->d3dTexture.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(d3d()->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), g_FrameIndex, d3d()->rtvDescriptorSize);
    commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    const rgFloat clearColor[] = { 0.5f, 0.5f, 0.5f, 1.0f };
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &d3d()->triVBView);
    commandList->DrawInstanced(3, 1, 0, 0);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(activeRenderTarget->d3dTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    BreakIfFail(commandList->Close());

    ID3D12CommandList* commandLists[] = { d3d()->commandList.Get() };
    d3d()->commandQueue->ExecuteCommandLists(1, commandLists);
    BreakIfFail(d3d()->dxgiSwapchain->Present(1, 0));

    waitForPreviousFrame();

    return 0;
}

// -----------------------------------------------
// GFX Function
// -----------------------------------------------
// SECTION ENDS -




// SECTION BEGIN -
// -----------------------------------------------
// GPU Resource Creators Deleters and Modifers
// -----------------------------------------------

// Buffer
GfxBuffer* creatorGfxBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    return rgNew(GfxBuffer);
}

void updaterGfxBuffer(GfxBuffer* buffer, void* data, rgSize size, rgU32 offset)
{

}

void deleterGfxBuffer(GfxBuffer* buffer)
{

}

// Texture
GfxTexture2D* creatorGfxTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, char const* name)
{
    return rgNew(GfxTexture2D);
}

void deleterGfxTexture2D(GfxTexture2D* t2d)
{

}

// PSO
GfxGraphicsPSO* creatorGfxGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    return rgNew(GfxGraphicsPSO);
}

void deleterGfxGraphicsPSO(GfxGraphicsPSO* pso)
{

}

// -----------------------------------------------
// GPU Resource Creators Deleters and Modifers
// -----------------------------------------------
// SECTION ENDS -




// SECTION BEGIN -
// -----------------------------------------------
// RenderCmd Handlers
// -----------------------------------------------

void gfxHandleRenderCmd_SetViewport(void const* cmd)
{

}

void gfxHandleRenderCmd_SetRenderPass(void const* cmd)
{

}

void gfxHandleRenderCmd_SetGraphicsPSO(void const* cmd)
{

}

void gfxHandleRenderCmd_DrawTexturedQuads(void const* cmd)
{

}

void gfxHandleRenderCmd_DrawTriangles(void const* cmd)
{

}

// -----------------------------------------------
// RenderCmd Handlers
// -----------------------------------------------
// SECTION ENDS -

#undef BreakIfFail
RG_END_NAMESPACE
#endif