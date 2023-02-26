#pragma once
#include "rg.h"

RG_BEGIN_NAMESPACE

//-----------------------------------------------------------------------------
// Render Commands
//-----------------------------------------------------------------------------
/*
                                        sizeof(cmd_type)
                                            |
+---------+sizeof(cmd_packet)+---------+    |
+-------------------------------------------v---------------------+
|NextCmdPacket|DispatchFn|Cmd|AuxMemory|ActualCmd|Actual AuxMemory|
+--------------------------+------+---------^-------------^-------+
                           |      |         |             |
                           +----------------+             |
                                  |                       |
                                  +-----------------------+
*/

typedef void (CmdDispatchFnT)(void const* cmd);
//typedef void (CmdDestructorFnT)(void* cmd);

struct CmdPacket
{
    CmdPacket* nextCmdPacket;
    CmdDispatchFnT* dispatchFn;
    //CmdDestructorFnT* destructorFn;
    void* cmd;
    void* auxMemory;
};

template <typename U>
inline rgU32 getCmdPacketSize(rgU32 auxBufSize)
{
    return sizeof(CmdPacket) + sizeof(U) + auxBufSize;
}

// NOTE: As actual cmd is stored right after the end of the struct cmd_packet.
// We subtract the sizeof(cmd_packet) from the address of the actual cmd to get
// address of the parent cmd_packet.
inline CmdPacket* getCmdPacket(void* cmd)
{
    return (CmdPacket*)((rgU8*)cmd - sizeof(CmdPacket));
}

class RenderCmdList
{
public:
    CmdPacket** packets;    /// List of packets
    // TODO: Make is a constant size 10240
    rgU32* keys;       /// Keys of packets
    rgU32       current;    /// Number of packets in the list
    void* buffer;     /// Memory buffer
    rgU32       bufferSize; /// Size of memory buffer in bytes
    rgU32       baseOffset; /// Number of bytes used in the memory buffer

    RenderCmdList(char const* nametag);
    ~RenderCmdList();

    template <typename U>
    U* addCmd(rgU32 key, rgU32 auxBufSize)
    {
        CmdPacket* packet = createCmdPacket<U>(auxBufSize);

        const rgU32 I = current++;
        packets[I] = packet;
        keys[I] = key;

        packet->nextCmdPacket = nullptr;
        packet->dispatchFn = U::dispatchFn;
        //packet->destructorFn = U::destructorFn;

        //U* c = new(packet->cmd) U; // Should we do this?? What are the disadvantages?
        //U* c = rgPlacementNew(U, packet->cmd); // Should we do this?? What are the disadvantages?
        return (U*)packet->cmd;
    }

    // TODO: don't need V here, as getCmdPacket is not dep on cmd type. use void*
    template <typename U, typename V>
    U* appendCmd(V* cmd, rgU32 auxBufSize)
    {
        CmdPacket* packet = createCmdPacket<U>(auxBufSize);

        getCmdPacket(cmd)->nextCmdPacket = packet;
        packet->nextCmdPacket = nullptr;
        packet->dispatchFn = U::dispatchFn; // did you forget to create dispatch function ptr in the cmd struct
        packet->destructorFn = U::destructorFn;

        return (U*)(packet->cmd);
    }

    void sort();
    void draw();
    void afterDraw();

private:
    // Create a CmdPacket and initialise it's members
    template <typename U>
    CmdPacket* createCmdPacket(rgU32 auxBufSize)
    {
        CmdPacket* packet = (CmdPacket*)getMemory(getCmdPacketSize<U>(auxBufSize));
        packet->nextCmdPacket = nullptr;
        packet->dispatchFn = nullptr;
        packet->cmd = (rgU8*)packet + sizeof(CmdPacket);
        packet->auxMemory = auxBufSize > 0 ? (rgU8*)packet->cmd + sizeof(U) : nullptr;
        return packet;
    }
    
    void* getMemory(rgU32 memSize)
    {
        rgAssert((bufferSize - baseOffset) >= memSize);
        void* memory = ((rgU8*)buffer + baseOffset);
        baseOffset += memSize;
        return memory;
    }
};

RenderCmdList* gfxGetRenderCmdList();
// Submits the list to the GPU, and create a fence to know when it's processed
void gfxSubmitRenderCmdList(RenderCmdList* cmdList);

RG_END_NAMESPACE