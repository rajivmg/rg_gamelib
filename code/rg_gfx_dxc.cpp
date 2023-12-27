#include "rg_gfx_dxc.h"

#ifndef _WIN32
#include "ComPtr.hpp"
#endif

#include "dxcapi.h"

#include <EASTL/hash_set.h>

#include <filesystem>

static ComPtr<IDxcUtils> g_DxcUtils;

static void checkResult(HRESULT hr)
{
    if(FAILED(hr))
    {
        rgAssert(!"Operation failed");
    }
}

char const* getGfxStageString(GfxStage stage)
{
    switch(stage)
    {
    case GfxStage_VS:
        return "vs";
        break;
    case GfxStage_FS:
        return "fs";
        break;
    case GfxStage_CS:
        return "cs";
        break;
    }
    return nullptr;
}

class CustomIncludeHandler : public IDxcIncludeHandler
{
public:
    HRESULT LoadSource(LPCWSTR pFilename, IDxcBlob **ppIncludeSource) override
    {
        
        ComPtr<IDxcBlobEncoding> blobEncoding;
        std::filesystem::path filepath = std::filesystem::absolute(std::filesystem::path(pFilename));
        
        if(alreadyIncludedFiles.count(eastl::wstring(filepath.string().c_str())) > 0)
        {
            static const char emptyStr[] = " ";
            g_DxcUtils->CreateBlobFromPinned(emptyStr, rgArrayCount(emptyStr), DXC_CP_ACP, &blobEncoding);
            return S_OK;
        }
        
        HRESULT hr = g_DxcUtils->LoadFile(pFilename, nullptr, &blobEncoding);
        if(SUCCEEDED(hr))
        {
            alreadyIncludedFiles.insert(eastl::wstring(pFilename));
            *ppIncludeSource = blobEncoding.Detach();
        }
        
        return hr;
    }
    
    HRESULT QueryInterface(REFIID riid,  void** ppvObject) override { return E_NOINTERFACE; }
    ULONG   AddRef(void) override     { return 0; }
    ULONG   Release(void) override    { return 0; }
    
    eastl::hash_set<eastl::wstring> alreadyIncludedFiles;
};

ShaderBlobRef createShaderBlob(char const* filename, GfxStage stage, char const* entrypoint, char const* defines, bool genSPIRV)
{
    rgAssert(entrypoint);
    rgAssert(filename);
    rgAssert(entrypoint);

    // construct hash from shader info
    rgHash hash = rgCRC32(filename);
    hash = rgCRC32(getGfxStageString(stage), 2, hash);
    hash = rgCRC32(entrypoint, (rgU32)strlen(entrypoint), hash);
    if(defines != nullptr)
    {
        hash = rgCRC32(defines, (rgU32)strlen(defines), hash);
    }

    eastl::vector<LPCWSTR> dxcArgs; // raname to dxcOpt
    
    // construct array of options for dxc
    
    //      1. give entrypoint
    wchar_t entrypointWide[256];
    std::mbstowcs(entrypointWide, entrypoint, 256);
    
    dxcArgs.push_back(L"-E");
    dxcArgs.push_back(entrypointWide);
    
    //      2. give shader model
    dxcArgs.push_back(L"-T");
    if(stage == GfxStage_VS)
    {
        dxcArgs.push_back(L"vs_6_0");
    }
    else if(stage == GfxStage_FS)
    {
        dxcArgs.push_back(L"ps_6_0");
    }
    else if(stage == GfxStage_CS)
    {
        dxcArgs.push_back(L"cs_6_0");
    }
    
    //      3. give hlsl include directory
    dxcArgs.push_back(L"-I");
    dxcArgs.push_back(L"../code/shaders");

    //      4. give macro defines
    eastl::vector<eastl::wstring> defineArgs;
    if(defines != nullptr)
    {
        char const* definesCursorA = defines;
        char const* definesCursorB = definesCursorA;
        while(*definesCursorB != '\0')
        {
            definesCursorB = definesCursorB + 1;
            if(*definesCursorB == ' ' || *definesCursorB == '\0')
            {
                if(*definesCursorB == '\0' && (definesCursorA == definesCursorB))
                {
                    break;
                }

                wchar_t d[256];
                wchar_t wterm = 0;
                rgUPtr lenWithoutNull = definesCursorB - definesCursorA;
                std::mbstowcs(d, definesCursorA, lenWithoutNull);
                wcsncpy(&d[lenWithoutNull], &wterm, 1);
                defineArgs.push_back(eastl::wstring(d));
                definesCursorA = definesCursorB + 1;
            }
        }
    }
    
    if(defineArgs.size() > 0)
    {
        dxcArgs.push_back(L"-D");
        for(auto& s : defineArgs)
        {
            dxcArgs.push_back(s.c_str());
        }
    }

    //      5. give options to geneate spirv
    if(genSPIRV)
    {
        dxcArgs.push_back(L"-spirv");
        dxcArgs.push_back(L"-fspv-target-env=vulkan1.1");
        dxcArgs.push_back(L"-fvk-use-dx-layout");
    }

    //      6. generate debug symbols
    dxcArgs.push_back(L"-Zi");

    // create include handler
    if(!g_DxcUtils)
    {
        checkResult(DxcCreateInstance(CLSID_DxcUtils, __uuidof(IDxcUtils), (void**)&g_DxcUtils));
    }

    ComPtr<CustomIncludeHandler> customIncludeHandler(rgNew(CustomIncludeHandler));

    // create dxc instance
    ComPtr<IDxcCompiler3> compiler3;
    checkResult(DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void**)&compiler3));

    // load the shader file from shaders directory
    char filepath[512];
    strcpy(filepath, "../code/shaders/");
    strncat(filepath, filename, 490);

    FileData shaderFileData = fileRead(filepath); // TODO: destructor deleter for FileData
    rgAssert(shaderFileData.isValid);
    
    // prepare for passing shader to dxc
    DxcBuffer shaderSource;
    shaderSource.Ptr = shaderFileData.data;
    shaderSource.Size = shaderFileData.dataSize;
    shaderSource.Encoding = 0;

    // compile the shader
    ComPtr<IDxcResult> result;
    checkResult(compiler3->Compile(&shaderSource, dxcArgs.data(), (UINT32)dxcArgs.size(), customIncludeHandler.Get(), __uuidof(IDxcResult), (void**)&result));

    // free shader file data
    fileFree(&shaderFileData);

    // print warnings and errors from compilation result
    ComPtr<IDxcBlobUtf8> errorMsg;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorMsg), nullptr);
    if(errorMsg && errorMsg->GetStringLength())
    {
        rgLogError("***Shader Compile Warn/Error(%s, %s),Defines:%s***\n%s", filename, entrypoint, defines, errorMsg->GetStringPointer());
    }

    // get the compiled shader blob from compilation result
    IDxcBlob* shaderObjectBlob;
    checkResult(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderObjectBlob), nullptr));
    rgAssert(shaderObjectBlob->GetBufferSize());

    // copy to output
    ShaderBlobRef output = eastl::shared_ptr<ShaderBlob>(rgNew(ShaderBlob), destroyShaderBlob);
    output->shaderObjectBufferPtr = shaderObjectBlob->GetBufferPointer();
    output->shaderObjectBufferSize = shaderObjectBlob->GetBufferSize();
    output->dxcBlobShaderObject = shaderObjectBlob;
    
#if defined(RG_D3D12_RNDR)
    // save shader pdb
    char pdbFilepath[512];
    ComPtr<IDxcBlob> shaderPdbBlob;
    ComPtr<IDxcBlobUtf16> shaderPdbPath;
    checkResult(result->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&shaderPdbBlob), shaderPdbPath.GetAddressOf()));
    wcstombs(pdbFilepath, shaderPdbPath->GetStringPointer(), 512);
    writeFile(pdbFilepath, shaderPdbBlob->GetBufferPointer(), shaderPdbBlob->GetBufferSize());

    // create shader reflection
    ComPtr<IDxcBlob> shaderReflectionBlob;
    checkResult(result->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&shaderReflectionBlob), nullptr));
    rgAssert(shaderReflectionBlob->GetBufferSize());

    DxcBuffer shaderReflectionBuffer;
    shaderReflectionBuffer.Ptr = shaderReflectionBlob->GetBufferPointer();
    shaderReflectionBuffer.Size = shaderReflectionBlob->GetBufferSize();
    shaderReflectionBuffer.Encoding = 0;

    ID3D12ShaderReflection* shaderReflection;
    g_DxcUtils->CreateReflection(&shaderReflectionBuffer, IID_PPV_ARGS(&shaderReflection));
    output->d3d12ShaderReflection = shaderReflection;
#endif
    
    return output;
}


void destroyShaderBlob(ShaderBlob* shaderBlob)
{
    IDxcBlob* dxcBlobShaderObject = (IDxcBlob*)shaderBlob->dxcBlobShaderObject;
    dxcBlobShaderObject->Release();

#if defined(RG_D3D12_RNDR)
    ID3D12ShaderReflection* d3d12ShaderReflection = (ID3D12ShaderReflection*)shaderBlob->d3d12ShaderReflection;
    d3d12ShaderReflection->Release();
#endif
}
