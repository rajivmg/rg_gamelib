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

GfxCtx::D3d* d3d()
{
    return &g_GfxCtx->d3d;
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
    BreakIfFail(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, __uuidof(ID3D12Device), (void**)&(d3d()->device)));
    
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugInterface;
    BreakIfFail(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
#endif

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