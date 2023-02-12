//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

#include <string.h>

RG_BEGIN_NAMESPACE

// --- Low-level Graphics Functions
//-----------------------------------------------------------------------------
// Render Commands
//-----------------------------------------------------------------------------

RenderCmdList::RenderCmdList(rgU32 _BufferSize, RenderResource _ShaderProgram, Matrix4 *_ViewMatrix, Matrix4 *_ProjMatrix) :
    bufferSize(_BufferSize),
    ShaderProgram(_ShaderProgram),
    ViewMatrix(_ViewMatrix),
    ProjMatrix(_ProjMatrix),
    baseOffset(0),
    current(0)
{
    const rgU32 MaxPacketsInList = 2048;

    buffer = rgMalloc(bufferSize);
    memset(buffer, 0, bufferSize);
    keys = (rgU32 *)rgMalloc(sizeof(rgU32) * MaxPacketsInList);
    packets = (CmdPacket **)rgMalloc(sizeof(CmdPacket*) * (MaxPacketsInList));
}

RenderCmdList::~RenderCmdList()
{
    rgFree(buffer);
}

void* RenderCmdList::AllocateMemory(rgU32 MemorySize)
{
    rgAssert((bufferSize - baseOffset) >= MemorySize);
    void *Mem = ((rgU8 *)buffer + baseOffset);
    baseOffset += MemorySize;
    return Mem; 
}

void RenderCmdList::Sort()
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

void RenderCmdList::Submit()
{
    //rndr::UseShaderProgram(ShaderProgram);
    //rndr::SetViewMatrix(*ViewMatrix);
    //rndr::SetProjectionMatrix(*ProjMatrix);
    for(rgU32 I = 0; I < current; ++I)
    {
        CmdPacket *Packet = packets[I];

        for(;;)
        {
            DispatchFnT *DispatchFn = Packet->dispatchFn;
            void *Cmd = Packet->cmd;
            DispatchFn(Cmd);

            Packet = Packet->nextCmdPacket;

            if(Packet == nullptr)
                break;
        }
    }
}

void RenderCmdList::Flush()
{
    memset(buffer, 0, baseOffset);
    current = 0;
    baseOffset = 0;
}

// --- Game Graphics APIs

TextureQuad::TextureQuad(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx)
{
    uvTopLeft[0] = (rgFloat)xPx / refWidthPx;
    uvTopLeft[1] = (rgFloat)yPx / refHeightPx;
    
    uvBottomRight[0] = (xPx + widthPx) / (rgFloat)refWidthPx;
    uvBottomRight[1] = (yPx + heightPx) / (rgFloat)refHeightPx;
}

TextureQuad::TextureQuad(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, Texture* refTexture)
    : TextureQuad(xPx, yPx, widthPx, heightPx, refTexture->width, refTexture->height)
{

}

void immTexturedQuad2(Texture* texture, TextureQuad* quad, rgFloat x, rgFloat y, rgFloat orientationRad, rgFloat scaleX, rgFloat scaleY, rgFloat offsetX, rgFloat offsetY)
{
    
}

RG_END_NAMESPACE
