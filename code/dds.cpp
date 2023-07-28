#include "rg_gfx.h"
#include "DirectXTex.h"

using namespace DirectX;

void loadDDS(const char* filename)
{
    rg::FileData file = rg::readFile(filename);
    rgAssert(file.isValid);
    
    TexMetadata metadata;
    ScratchImage image;
    HRESULT result = LoadFromDDSMemory(file.data, file.dataSize, DDS_FLAGS_NONE, &metadata, image);
    rg::freeFileData(&file);
    
    if(FAILED(result))
    {
        rgLog("Error");
    }
    
}
