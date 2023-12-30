#ifndef __GFX_DXC_H__
#define __GFX_DXC_H__

#include "gfx.h"

struct ShaderBlob
{
    void*   shaderObjectBufferPtr;
    rgSize  shaderObjectBufferSize;

    void*   dxcBlobShaderObject;      // type: IDxcBlob*
#if defined(RG_D3D12_RNDR)
    void*   d3d12ShaderReflection;    // typeL ID3D12ShaderReflection*
#endif
};

typedef eastl::shared_ptr<ShaderBlob> ShaderBlobRef;

ShaderBlobRef createShaderBlob(char const* filename, GfxStage stage, char const* entrypoint, char const* defines, bool genSPIRV);

void destroyShaderBlob(ShaderBlob* shaderBlob);

#endif
