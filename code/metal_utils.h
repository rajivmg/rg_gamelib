#include "rg_gfx.h"

namespace metalutils
{
void initMetal(rg::GfxCtx* ctx);
CA::MetalDrawable* nextDrawable(rg::GfxCtx* ctx);
NS::Dictionary* getPreprocessorMacrosDict(char const* macros);
}
