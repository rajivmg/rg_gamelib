#ifndef __RG_GFX_TECHNIC_H__
#define __RG_GFX_TECHNIC_H__

#include "rg_gfx.h"

RG_BEGIN_CORE_NAMESPACE
RG_BEGIN_GFX_NAMESPACE

struct BaseMaterialTemplate
{
    GraphicsPSO* pso;
};

/*--------
    gfx::LitMaterialTemplate litMaterialTemplate("lit.hlsl", );

 --------*/

RG_END_GFX_NAMESPACE
RG_END_CORE_NAMESPACE

#endif
