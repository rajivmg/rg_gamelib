//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

RG_BEGIN_NAMESPACE

// --- Low-level Graphics Functions

//-----------------------------------------------------------------------------
// Render Commands
//-----------------------------------------------------------------------------

DispatchFnT* RenderCmdTexturedQuad::dispatchFn = gfxHandleRenderCmdTexturedQuad;

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

//void RenderCmdList::Submit()
//{
//    //rndr::UseShaderProgram(ShaderProgram);
//    //rndr::SetViewMatrix(*ViewMatrix);
//    //rndr::SetProjectionMatrix(*ProjMatrix);
//    for(rgU32 I = 0; I < current; ++I)
//    {
//        CmdPacket *Packet = packets[I];
//
//        for(;;)
//        {
//            DispatchFnT *DispatchFn = Packet->dispatchFn;
//            void *Cmd = Packet->cmd;
//            DispatchFn(Cmd);
//
//            Packet = Packet->nextCmdPacket;
//
//            if(Packet == nullptr)
//                break;
//        }
//    }
//}

void RenderCmdList::draw()
{
    for(rgU32 i = 0; i < current; ++i)
    {
        CmdPacket* pkt = packets[i];
        for(;;)
        {
            DispatchFnT* DispatchFn = pkt->dispatchFn;
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
    for(rgU32 i = 0; i < current; ++i)
    {
        CmdPacket* pkt = packets[i];
        for(;;)
        {
            delete pkt->cmd;

            pkt = pkt->nextCmdPacket;
            if(pkt == NULL)
            {
                break;
            }
        }
    }

    memset(buffer, 0, baseOffset);
    current = 0;
    baseOffset = 0;
}

//void gfxSubmitRenderCmdList(RenderCmdList* cmdList)
//{
//
//}

//void RenderCmdList::Flush()
//{
//    memset(buffer, 0, baseOffset);
//    current = 0;
//    baseOffset = 0;
//}

// --- Game Graphics APIs
QuadUV defaultQuadUV = { 0.0f, 0.0f, 1.0f, 1.0f };

rgInt gfxCommonInit()
{
    GfxCtx* ctx = gfxCtx();

    ctx->graphicCmdLists[0] = rgNew(RenderCmdList)("graphic cmdlist 0");
    ctx->graphicCmdLists[1] = rgNew(RenderCmdList)("graphic cmdlist 1");

    return 0;
}

RenderCmdList* gfxGetRenderCmdList()
{
    return gfxCtx()->graphicCmdLists[gfxCtx()->frameNumber & 1];
}

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx)
{
    QuadUV r;

    r.uvTopLeft[0] = (rgFloat)xPx / refWidthPx;
    r.uvTopLeft[1] = (rgFloat)yPx / refHeightPx;

    r.uvBottomRight[0] = (xPx + widthPx) / (rgFloat)refWidthPx;
    r.uvBottomRight[1] = (yPx + heightPx) / (rgFloat)refHeightPx;

    return r;
}

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, Texture* refTexture)
{
    return createQuadUV(xPx, yPx, widthPx, heightPx, refTexture->width, refTexture->height);
}

TexturePtr loadTexture(char const* filename)
{
    rgInt width, height, texChnl;
    unsigned char* texData = stbi_load(filename, &width, &height, &texChnl, 4);
    
    if(texData == NULL)
    {
        return nullptr;
    }

    //TexturePtr tptr = eastl::make_shared<Texture>(unloadTexture);
    TexturePtr tptr = eastl::shared_ptr<Texture>(rgNew(Texture), unloadTexture);
    strcpy(tptr->name, "[NONAME]");
    tptr->width = width;
    tptr->height = height;
    tptr->format = TinyImageFormat_R8G8B8A8_UNORM;
    tptr->buf = texData;

    return tptr;
}

void unloadTexture(Texture* t)
{
    stbi_image_free(t->buf);
    rgDelete(t);
}

void immTexturedQuad2(Texture* texture, QuadUV* quad, rgFloat x, rgFloat y, rgFloat orientationRad, rgFloat scaleX, rgFloat scaleY, rgFloat offsetX, rgFloat offsetY)
{
    
}

RG_END_NAMESPACE
