#include "rg_gfx_rendercmd.h"
#include "rg_gfx.h"

RG_BEGIN_NAMESPACE
//-----------------------------------------------------------------------------
// Render Commands
//-----------------------------------------------------------------------------

//void gfxDestroyRenderCmdTexturedQuad(void* cmd)
//{
//    ((RenderCmdTexturedQuad*)cmd)->~RenderCmdTexturedQuad();
//}

//#define RG_GEN_RENDER_CMD_DESTRUCTOR(type) void gfxDestroy ## type(void* cmd) { ((type*)cmd)->~type(); } \
//CmdDestructorFnT* type::destructorFn = gfxDestroy ## type

#define RG_DECL_RENDER_CMD_HANDLER(type) CmdDispatchFnT* type::dispatchFn = gfx::gfxHandle ## type

//CmdDispatchFnT* RenderCmdTexturedQuad::dispatchFn = gfxHandleRenderCmdTexturedQuad;
//CmdDestructorFnT* RenderCmdTexturedQuad::destructorFn = gfxDestroyRenderCmdTexturedQuad;

RG_DECL_RENDER_CMD_HANDLER(RenderCmd_SetViewport);
RG_DECL_RENDER_CMD_HANDLER(RenderCmd_SetRenderPass);
RG_DECL_RENDER_CMD_HANDLER(RenderCmd_SetGraphicsPSO);
RG_DECL_RENDER_CMD_HANDLER(RenderCmd_DrawTexturedQuads);
RG_DECL_RENDER_CMD_HANDLER(RenderCmd_DrawTriangles);
//RG_GEN_RENDER_CMD_DESTRUCTOR(RenderCmdTexturedQuad);

RenderCmdList::RenderCmdList(char const* nametag) :
    bufferSize(rgMEGABYTE(1)),
    baseOffset(0),
    current(0)
{
    const rgU32 MaxPacketsInList = 10240;

    buffer = rgMalloc(bufferSize);
    memset(buffer, 0, bufferSize);
    keys = (rgU32 *)rgMalloc(sizeof(rgU32) * MaxPacketsInList);
    packets = (CmdPacket **)rgMalloc(sizeof(CmdPacket*) * (MaxPacketsInList));
}

RenderCmdList::~RenderCmdList()
{
    rgFree(buffer);
}

//void* RenderCmdList::AllocateMemory(rgU32 MemorySize)
//{
//    rgAssert((bufferSize - baseOffset) >= MemorySize);
//    void *Mem = ((rgU8 *)buffer + baseOffset);
//    baseOffset += MemorySize;
//    return Mem; 
//}

void RenderCmdList::sort()
{
    // Sort using insertion sort.
    // TODO: Use Radix sort

    rgU32 i, j;
    for(i = 1; i < current; ++i)
    {
        j = i;
        while((j > 0) && keys[j - 1] > keys[j])
        {
            CmdPacket *Temp = packets[j];
            packets[j] = packets[j - 1];
            packets[j - 1] = Temp;
            --j;
        }
    }
}

void RenderCmdList::draw()
{
    for(rgU32 i = 0; i < current; ++i)
    {
        CmdPacket* pkt = packets[i];
        for(;;)
        {
            CmdDispatchFnT* DispatchFn = pkt->dispatchFn;
            DispatchFn(pkt->cmd);

            pkt = pkt->nextCmdPacket;
            if(pkt == NULL)
            {
                break;
            }
        }
    }
}

void RenderCmdList::afterDraw()
{
    memset(buffer, 0, baseOffset);
    current = 0;
    baseOffset = 0;
}

RG_END_NAMESPACE
