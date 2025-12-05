#ifndef __WORLD_H__
#define __WORLD_H__

struct Chunk
{
    rgU32 tiles[32][32];
};

struct World
{
    
    Chunk* loadedChunks[5][5];
};

World*	CreateWorld();
void	DestroyWorld();

#endif