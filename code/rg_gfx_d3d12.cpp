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




// SECTION BEGIN -
// -----------------------------------------------
// GFX Functions
// -----------------------------------------------
rgInt gfxInit()
{
    // create device
    BreakIfFail(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), (void**)&(d3d()->device)));
    
    // create debug validation interface
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugInterface;
    BreakIfFail(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));

    ComPtr<ID3D12InfoQueue> debugInfoQueue;
    if(SUCCEEDED(d3d()->device.As(&debugInfoQueue)))
    {
        debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        debugInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
    }
#endif

    // create command queue
    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    BreakIfFail(device()->CreateCommandQueue(&commandQueueDesc, __uuidof(ID3D12CommandQueue), (void**)&(d3d()->commandQueue)));

    // create swapchain
    UINT factoryCreateFlag = 0;
#if defined(_DEBUG)
    factoryCreateFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif
    BreakIfFail(CreateDXGIFactory2(factoryCreateFlag, __uuidof(d3d()->dxgiFactory), (void**)&(d3d()->dxgiFactory)));
    
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


    return 0;
}

void gfxDestroy()
{

}

rgInt gfxDraw()
{
    gfxGetRenderCmdList()->draw();
    gfxGetRenderCmdList()->afterDraw();

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