//#include <mmgr/mmgr.cpp>
#include "gfx.h"
#include <utils.h>

#include <string.h>
#include <EASTL/string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <SDL2/SDL_filesystem.h>

#include "pugixml.hpp"
#include "DirectXTex.h"

rgInt   g_FrameIndex;
GfxBindlessResourceManager<GfxTexture>* g_BindlessTextureManager;
QuadUV  defaultQuadUV = { 0.0f, 0.0f, 1.0f, 1.0f };


//-----------------------------------------------------------------------------
// GFX STATE
//-----------------------------------------------------------------------------

GfxGraphicsPSO*     GfxState::graphicsPSO;
GfxComputePSO*      GfxState::computePSO;
GfxSamplerState*    GfxState::samplerBilinearRepeat;
GfxSamplerState*    GfxState::samplerBilinearClampEdge;
GfxSamplerState*    GfxState::samplerTrilinearRepeatAniso;
GfxSamplerState*    GfxState::samplerTrilinearClampEdgeAniso;
GfxSamplerState*    GfxState::samplerNearestRepeat;
GfxSamplerState*    GfxState::samplerNearestClampEdge;


//-----------------------------------------------------------------------------
// COMMON GFX FUNCTIONS
//-----------------------------------------------------------------------------

static GfxFrameAllocator*       frameAllocators[RG_MAX_FRAMES_IN_FLIGHT];
static GfxRenderPass*           currentRenderPass;
static GfxRenderCmdEncoder*     currentRenderCmdEncoder;
static GfxComputeCmdEncoder*    currentComputeCmdEncoder;
static GfxBlitCmdEncoder*       currentBlitCmdEncoder;

static void styleImGui()
{
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.14f, 0.16f, 0.11f, 0.52f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.27f, 0.30f, 0.23f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.34f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.29f, 0.34f, 0.26f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.28f, 0.32f, 0.24f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.25f, 0.30f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.23f, 0.27f, 0.21f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
    colors[ImGuiCol_Button] = ImVec4(0.29f, 0.34f, 0.26f, 0.40f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
    colors[ImGuiCol_Header] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.42f, 0.31f, 0.6f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.54f, 0.57f, 0.51f, 0.50f);
    colors[ImGuiCol_Separator] = ImVec4(0.14f, 0.16f, 0.11f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.54f, 0.57f, 0.51f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.19f, 0.23f, 0.18f, 0.00f); // grip invis
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.54f, 0.57f, 0.51f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.54f, 0.57f, 0.51f, 0.78f);
    colors[ImGuiCol_TabActive] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.24f, 0.27f, 0.20f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.35f, 0.42f, 0.31f, 1.00f);
    //colors[ImGuiCol_DockingPreview] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
    //colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.78f, 0.28f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.73f, 0.67f, 0.24f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.59f, 0.54f, 0.18f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
}

rgInt gfxPreInit()
{
    g_BindlessTextureManager = rgNew(GfxBindlessResourceManager<GfxTexture>);
    return 0;
}

rgInt gfxPostInit()
{
    // Initialize frame buffer allocators
    for(rgS32 i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        frameAllocators[i] = rgNew(GfxFrameAllocator)(rgMegabyte(8), rgMegabyte(64), rgMegabyte(64));
    }
    
    // Initialize IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& imguiIO = ImGui::GetIO();
    imguiIO.Fonts->AddFontFromFileTTF("fonts/Noto_Sans/NotoSans-Regular.ttf", 18);
    // To change the imgui styling
    //styleImGui();

    gfxRendererImGuiInit();
    
    GfxState::samplerBilinearRepeat = GfxSamplerState::create("samplerBilinearRepeat", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Nearest, false);
    GfxState::samplerBilinearClampEdge = GfxSamplerState::create("samplerBilinearClampEdge", GfxSamplerAddressMode_ClampToEdge, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Nearest, false);
    GfxState::samplerTrilinearRepeatAniso = GfxSamplerState::create("samplerTrilinearRepeatAniso", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Linear, true);
    GfxState::samplerTrilinearClampEdgeAniso = GfxSamplerState::create("samplerTrilinearClampEdgeAniso", GfxSamplerAddressMode_ClampToEdge, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Linear, true);
    GfxState::samplerNearestRepeat = GfxSamplerState::create("samplerNearestRepeat", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Nearest, GfxSamplerMinMagFilter_Nearest, GfxSamplerMipFilter_Nearest, false);
    GfxState::samplerNearestClampEdge = GfxSamplerState::create("samplerNearestClampEdge", GfxSamplerAddressMode_ClampToEdge, GfxSamplerMinMagFilter_Nearest, GfxSamplerMinMagFilter_Nearest, GfxSamplerMipFilter_Nearest, false);

    return 0;
}

// TODO: refactor
void gfxAtFrameStart()
{
    GfxBuffer::destroyMarkedObjects();
    GfxTexture::destroyMarkedObjects();
    GfxSamplerState::destroyMarkedObjects();
    GfxGraphicsPSO::destroyMarkedObjects();
    GfxComputePSO::destroyMarkedObjects();
    
    // Reset render pass
    // TODO: replace with endEncoder()
    currentRenderPass = nullptr;
    if(currentRenderCmdEncoder != nullptr)
    {
        rgDelete(currentRenderCmdEncoder);
        currentRenderCmdEncoder = nullptr;
    }
    
    // reset this frame's allocations
    frameAllocators[g_FrameIndex]->reset();
}

rgInt gfxGetFrameIndex()
{
    return (g_FrameIndex != -1) ? g_FrameIndex : 0;
}

rgInt gfxGetPrevFrameIndex()
{
    rgInt prevFrameIndex = 0;
    if(g_FrameIndex != -1)
    {
        prevFrameIndex = g_FrameIndex - 1;
        if(prevFrameIndex < 0)
        {
            prevFrameIndex = RG_MAX_FRAMES_IN_FLIGHT - 1;
        }
    }
    return prevFrameIndex;
}

GfxFrameAllocator* gfxGetFrameAllocator()
{
    return frameAllocators[g_FrameIndex];
}

static void endCurrentCmdEncoder()
{
    if(currentRenderCmdEncoder != nullptr)
    {
        if(!currentRenderCmdEncoder->hasEnded)
        {
            currentRenderCmdEncoder->end();
        }
        rgDelete(currentRenderCmdEncoder);
        currentRenderCmdEncoder = nullptr;
    }
    
    if(currentComputeCmdEncoder != nullptr)
    {
        if(!currentComputeCmdEncoder->hasEnded)
        {
            currentComputeCmdEncoder->end();
        }
        rgDelete(currentComputeCmdEncoder);
        currentComputeCmdEncoder = nullptr;
    }
    
    if(currentBlitCmdEncoder != nullptr)
    {
        if(!currentBlitCmdEncoder->hasEnded)
        {
            currentBlitCmdEncoder->end();
        }
        rgDelete(currentBlitCmdEncoder);
        currentBlitCmdEncoder = nullptr;
    }
}

GfxRenderCmdEncoder* gfxSetRenderPass(char const* tag, GfxRenderPass* renderPass)
{
    endCurrentCmdEncoder();

    currentRenderCmdEncoder = rgNew(GfxRenderCmdEncoder);
    currentRenderCmdEncoder->begin(tag, renderPass);

    currentRenderPass = renderPass;
    
    return currentRenderCmdEncoder;
}

GfxComputeCmdEncoder* gfxSetComputePass(char const* tag)
{
    endCurrentCmdEncoder();
    
    currentComputeCmdEncoder = rgNew(GfxComputeCmdEncoder);
    currentComputeCmdEncoder->begin(tag);
    
    return currentComputeCmdEncoder;
}

GfxBlitCmdEncoder* gfxSetBlitPass(char const* tag)
{
    endCurrentCmdEncoder();
    
    currentBlitCmdEncoder = rgNew(GfxBlitCmdEncoder);
    currentBlitCmdEncoder->begin(tag);
    currentBlitCmdEncoder->pushDebugTag(tag);
    
    return currentBlitCmdEncoder;
}


//-----------------------------------------------------------------------------
// IMAGE/BITMAP AND MODEL/MESH
//-----------------------------------------------------------------------------

ImageRef loadImage(char const* filename, bool srgbFormat/* = true*/)
{
    rgSize nullTerminatedPathLength = strlen(filename) + 1;
    char const* extStr = filename + nullTerminatedPathLength - 4;
    
    rgInt width, height, texChnl;
    ImageRef output = eastl::shared_ptr<Image>(rgNew(Image), unloadImage);
    
    if(strcmp(extStr, "dds") == 0 || strcmp(extStr, "DDS") == 0)
    {
        FileData file = fileRead(filename);
        rgAssert(file.isValid);
        
        DirectX::TexMetadata metadata;
        DirectX::ScratchImage scratchImage;
        HRESULT result = DirectX::LoadFromDDSMemory(file.data, file.dataSize, DirectX::DDS_FLAGS_NONE, &metadata, scratchImage);
        fileFree(&file);
        
        if(FAILED(result))
        {
            rgLog("Error");
            return output;
        }
        
        strncpy(output->tag, filename, rgArrayCount(Image::tag));
        output->tag[rgArrayCount(Image::tag) - 1] = '\0';
        output->width = (rgUInt)metadata.width;
        output->height = (rgUInt)metadata.height;
        output->format = TinyImageFormat_FromDXGI_FORMAT((TinyImageFormat_DXGI_FORMAT)metadata.format);
        output->mipFlag = (GfxTextureMipFlag)metadata.mipLevels;
        output->sliceCount = (rgUInt)metadata.arraySize;
        output->isDDS = true;
        output->memory = (rgU8*)rgMalloc(scratchImage.GetPixelsSize());
        
        rgU8* basememory = output->memory;
        rgU32 offset = 0;
        rgAssert(scratchImage.GetImageCount() <= rgArrayCount(Image::slices));
        for(rgInt i = 0; i < scratchImage.GetImageCount(); ++i)
        {
            const DirectX::Image* img = scratchImage.GetImages() + i;
            memcpy(basememory + offset, img->pixels, img->slicePitch);

            output->slices[i].width = img->width;
            output->slices[i].height = img->height;
            output->slices[i].rowPitch = img->rowPitch;
            output->slices[i].slicePitch = img->slicePitch;
            output->slices[i].pixels = basememory + offset;
        
            offset += (rgU32)img->slicePitch;
        }
        rgAssert(offset == scratchImage.GetPixelsSize());
    }
    else
    {
        unsigned char* texData = stbi_load(filename, &width, &height, &texChnl, 4);
        if(texData == NULL)
        {
            rgLogError("Can't load image file %s", filename);
            return output;
        }
        
        strncpy(output->tag, filename, rgArrayCount(Image::tag));
        output->tag[rgArrayCount(Image::tag) - 1] = '\0';
        output->width = width;
        output->height = height;
        output->format = srgbFormat ? TinyImageFormat_R8G8B8A8_SRGB : TinyImageFormat_R8G8B8A8_UNORM;
        output->mipFlag = GfxTextureMipFlag_GenMips;
        output->sliceCount = 1;
        output->isDDS = false;
        output->memory = texData;

        rgU16 bytesPerPixel = TinyImageFormat_BitSizeOfBlock(output->format) / 8;

        output->slices[0].width = width;
        output->slices[0].height = height;
        output->slices[0].rowPitch = width * bytesPerPixel;
        output->slices[0].slicePitch = width * height * bytesPerPixel;
        output->slices[0].pixels = texData;
    }
    return output;
}

void unloadImage(Image* ptr)
{
    if(ptr->isDDS)
    {
        rgFree(ptr->memory);
    }
    else
    {
        stbi_image_free(ptr->memory);
    }
    rgDelete(ptr);
}


static DefaultMaterialRef loadMaterial(eastl::string basePath, pugi::xml_node* matNode)
{
    DefaultMaterial* material = rgNew(DefaultMaterial);
    strncpy(material->tag, matNode->attribute("name").as_string(), sizeof(DefaultMaterial::tag));
    
    eastl::string rootWd = getCurrentWorkingDir();
    changeWorkingDir(basePath);
    
    ImageRef difalphaImg = loadImage(matNode->child("difalpha").attribute("path").as_string());
    ImageRef normImg = loadImage(matNode->child("norm").attribute("path").as_string());
    ImageRef propImg = loadImage(matNode->child("prop").attribute("path").as_string());
    
    changeWorkingDir(rootWd);
}

ModelRef loadModel(char const* filename)
{
    auto fetchMatrixArray = [](pugi::xml_node& matrixNode, float* arr) -> void
    {
        pugi::xml_node matrixMNode = matrixNode.first_child();
        for (int i = 0; i < 16; ++i)
        {
            arr[i] = matrixMNode.text().as_float();
            matrixMNode = matrixMNode.next_sibling();
        }
    };

    FileData xmlFileData = fileRead(filename);
    if(!xmlFileData.isValid)
    {
        return nullptr;
    }
    
    pugi::xml_document modelDoc;
    pugi::xml_parse_result parseResult = modelDoc.load_buffer(xmlFileData.data, xmlFileData.dataSize);
    
    rgAssert(parseResult.status == pugi::xml_parse_status::status_ok);
    
    fileFree(&xmlFileData);
    
    ModelRef outModel = eastl::shared_ptr<Model>(rgNew(Model), unloadModel);
    
    pugi::xml_node modelNode = modelDoc.first_child();
    
    strncpy(outModel->tag, modelNode.attribute("name").as_string(), sizeof(Model::tag));
    
    const char* binFilename = modelNode.attribute("bufferName").as_string();
    FileData binFileData = fileRead(binFilename);
    rgAssert(binFileData.isValid);
    
    outModel->vertexIndexBuffer = GfxBuffer::create(binFilename, GfxMemoryType_Default, binFileData.data, binFileData.dataSize, GfxBufferUsage_VertexBuffer | GfxBufferUsage_IndexBuffer);
    
    fileFree(&binFileData);
    
    outModel->vertexBufferOffset = modelNode.attribute("vertexBufferOffset").as_uint();
    outModel->index32BufferOffset = modelNode.attribute("index32BufferOffset").as_uint();
    outModel->index16BufferOffset = modelNode.attribute("index16BufferOffset").as_uint();
    
    rgU32 meshCount = modelNode.attribute("meshCount").as_uint();
    
    for(auto meshNode : modelNode.children("mesh"))
    {
        Mesh m = {};
        strncpy(m.tag, meshNode.attribute("name").as_string(), sizeof(Mesh::tag));

        //float transformArray[16];
        //pugi::xml_node transformNode = meshNode.child("transform");
        //fetchMatrixArray(transformNode, transformArray);

        m.vertexCount = meshNode.attribute("vertexCount").as_uint();
        m.vertexDataOffset = meshNode.attribute("vertexDataOffset").as_uint();
        m.indexCount = meshNode.attribute("indexCount").as_uint();
        m.indexDataOffset = meshNode.attribute("indexDataOffset").as_uint();
        
        if(meshNode.attribute("has32BitIndices").as_bool() == true)
        {
            m.properties |= MeshProperties_Has32BitIndices;
        }
        if(meshNode.attribute("hasTexCoord").as_bool() == true)
        {
            m.properties |= MeshProperties_HasTexCoord;
        }
        if(meshNode.attribute("hasNormal").as_bool() == true)
        {
            m.properties |= MeshProperties_HasNormal;
        }
        if(meshNode.attribute("hasTangent").as_bool() == true)
        {
            m.properties |= MeshProperties_HasTangent;
        }
        outModel->meshes.push_back(m);
    }
    
    return outModel;
}

void unloadModel(Model* ptr)
{
    // TODO: implement
}


//-----------------------------------------------------------------------------
// 2D RENDERING HELPERS
//-----------------------------------------------------------------------------

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, rgU32 refWidthPx, rgU32 refHeightPx)
{
    QuadUV r;

    r.uvTopLeft.x = (rgFloat)xPx / refWidthPx;
    r.uvTopLeft.y = (rgFloat)yPx / refHeightPx;

    r.uvBottomRight.x = (xPx + widthPx) / (rgFloat)refWidthPx;
    r.uvBottomRight.y = (yPx + heightPx) / (rgFloat)refHeightPx;

    return r;
}

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, ImageRef image)
{
    return createQuadUV(xPx, yPx, widthPx, heightPx, image->width, image->height);
}

QuadUV repeatQuadUV(rgFloat2 repeatXY)
{
    QuadUV r;
    r.uvTopLeft = {0, 0};
    r.uvBottomRight = repeatXY;
    return r;
}

void pushTexturedQuad(TexturedQuads* quadList, SpriteLayer layer, QuadUV uv, rgFloat4 posSize, rgU32 color, rgFloat4 offsetOrientation, GfxTexture* tex)
{
    TexturedQuad q;
    q.uv = uv;
    q.pos = { posSize.x, posSize.y, 1.0f };
    q.color = color;
    q.size = posSize.zw;
    q.offsetOrientation = offsetOrientation;
    q.texID = g_BindlessTextureManager->storeIfNotPresent(tex);
    quadList->insert(eastl::pair<SpriteLayer, TexturedQuad>(layer, q));
}

void pushTexturedLine(TexturedQuads* quadList, SpriteLayer layer, QuadUV uv, rgFloat2 pointA, rgFloat2 pointB, rgFloat thickness, rgU32 color, GfxTexture* tex)
{
    rgFloat2 a = pointA;
    rgFloat2 b = pointB;

    // arctan works between -90 to 90 so swap a and b if b is behind a
    if (b.x < a.x)
    {
        rgFloat2 t = a;
        a = b;
        b = t;
        
        // swap uv. should this be done depends on the situation
        rgFloat2 tempUV = uv.uvTopLeft;
        uv.uvTopLeft = uv.uvBottomRight;
        uv.uvBottomRight = tempUV;
    }
    
    rgFloat2 ab = b - a;
    rgFloat l = length(ab);
    rgFloat tHalf = thickness / 2.0f;

    // adjust the y, so that middle of the thickness it at pointA
    a.y = a.y - tHalf;
    b.y = b.y - tHalf;

    rgFloat ang;
    ang = atan((b.y - a.y) / (b.x - a.x));
    //ImGui::Text("ang %f", ang);
    pushTexturedQuad(quadList, layer, uv, {a.x, a.y, l, thickness}, color, {0, tHalf, ang, 1 }, tex);
}

void genTexturedQuadVertices(TexturedQuads* quadList, eastl::vector<SimpleVertexFormat>* vertices, SimpleInstanceParams* instanceParams)
{
    auto setColor = [](unsigned char* colorArr, rgU32 c)
    {
        colorArr[0] = (unsigned char)(c >> 24 & 0x000000FF);
        colorArr[1] = (unsigned char)(c >> 16 & 0x000000FF);
        colorArr[2] = (unsigned char)(c >> 8 & 0x000000FF);
        colorArr[3] = (unsigned char)(c >> 0 & 0x000000FF);
    };

    rgU32 i = 0;
    for(TexturedQuads::const_iterator iter = quadList->begin(); iter != quadList->end(); ++iter)
    {
        TexturedQuad const& t = iter->second;
        
        SimpleVertexFormat v[4];
        // 0 - 1
        // 3 - 2
#if 1
        rgFloat s, c;
        s = sinf(t.offsetOrientation.z);
        c = cosf(t.offsetOrientation.z);

        rgFloat2 translateAfter{t.pos.x + t.offsetOrientation.x, t.pos.y + t.offsetOrientation.y};

        rgFloat2 p0{-t.offsetOrientation.x, -t.offsetOrientation.y};
        rgFloat2 p1{t.size.x - t.offsetOrientation.x, -t.offsetOrientation.y};
        rgFloat2 p2{t.size.x - t.offsetOrientation.x, t.size.y - t.offsetOrientation.y};
        rgFloat2 p3{-t.offsetOrientation.x, t.size.y - t.offsetOrientation.y};

        // TODO: enable to use scale
        //p0 = p0 * t.offsetOrientation.w;
        //p1 = p1 * t.offsetOrientation.w;
        //p2 = p2 * t.offsetOrientation.w;
        //p3 = p3 * t.offsetOrientation.w;

        v[0].pos[0] = c * p0.x - s * p0.y + translateAfter.x;
        v[0].pos[1] = s * p0.x + c * p0.y + translateAfter.y;
        v[0].pos[2] = t.pos.z;
        v[0].texcoord[0] = t.uv.uvTopLeft.x;
        v[0].texcoord[1] = t.uv.uvTopLeft.y;
        setColor(v[0].color, t.color);

        v[1].pos[0] = c * p1.x - s * p1.y + translateAfter.x;
        v[1].pos[1] = s * p1.x + c * p1.y + translateAfter.y;
        v[1].pos[2] = t.pos.z;
        v[1].texcoord[0] = t.uv.uvBottomRight.x;
        v[1].texcoord[1] = t.uv.uvTopLeft.y;
        setColor(v[1].color, t.color);

        v[2].pos[0] = c * p2.x - s * p2.y + translateAfter.x;
        v[2].pos[1] = s * p2.x + c * p2.y + translateAfter.y;
        v[2].pos[2] = t.pos.z;
        v[2].texcoord[0] = t.uv.uvBottomRight.x;
        v[2].texcoord[1] = t.uv.uvBottomRight.y;
        setColor(v[2].color, t.color);

        v[3].pos[0] = c * p3.x - s * p3.y + translateAfter.x;
        v[3].pos[1] = s * p3.x + c * p3.y + translateAfter.y;
        v[3].pos[2] = t.pos.z;
        v[3].texcoord[0] = t.uv.uvTopLeft.x;
        v[3].texcoord[1] = t.uv.uvBottomRight.y;
        setColor(v[3].color, t.color);
#else
        v[0].pos[0] = t.pos.x;
        v[0].pos[1] = t.pos.y;
        v[0].pos[2] = t.pos.z;
        v[0].texcoord[0] = t.uv.uvTopLeft[0];
        v[0].texcoord[1] = t.uv.uvTopLeft[1];
        setColor(v[0].color, t.color);
        
        v[1].pos[0] = t.pos.x + t.size.x;
        v[1].pos[1] = t.pos.y;
        v[1].pos[2] = t.pos.z;
        v[1].texcoord[0] = t.uv.uvBottomRight[0];
        v[1].texcoord[1] = t.uv.uvTopLeft[1];
        setColor(v[1].color, t.color);
        
        v[2].pos[0] = t.pos.x + t.size.x;
        v[2].pos[1] = t.pos.y + t.size.y;
        v[2].pos[2] = t.pos.z;
        v[2].texcoord[0] = t.uv.uvBottomRight[0];
        v[2].texcoord[1] = t.uv.uvBottomRight[1];
        setColor(v[2].color, t.color);
        
        v[3].pos[0] = t.pos.x;
        v[3].pos[1] = t.pos.y + t.size.y;
        v[3].pos[2] = t.pos.z;
        v[3].texcoord[0] = t.uv.uvTopLeft[0];
        v[3].texcoord[1] = t.uv.uvBottomRight[1];
        setColor(v[3].color, t.color);
#endif
        vertices->push_back(v[0]);
        vertices->push_back(v[3]);
        vertices->push_back(v[1]);
        
        vertices->push_back(v[1]);
        vertices->push_back(v[3]);
        vertices->push_back(v[2]);

        rgAssert(i < 1024); // simple2d.hlsl MAX_INSTANCES
        instanceParams->texParam[i++][0] = t.texID;
    }
}


//-----------------------------------------------------------------------------
// HELPER FUNCTIONS
//-----------------------------------------------------------------------------

void copyMatrix4ToFloatArray(rgFloat* dstArray, Matrix4 const& srcMatrix)
{
    rgFloat const* ptr = toFloatPtr(srcMatrix);
    for(rgInt i = 0; i < 16; ++i)
    {
        dstArray[i] = ptr[i];
    }
}

void copyMatrix3ToFloatArray(rgFloat* dstArray, Matrix3 const& srcMatrix)
{
    rgFloat const* ptr = toFloatPtr(srcMatrix);
    for(rgInt i = 0; i < 9; ++i)
    {
        dstArray[i] = ptr[i];
    }
}

Matrix4 makeOrthographicProjectionMatrix(rgFloat left, rgFloat right, rgFloat bottom, rgFloat top, rgFloat nearPlane, rgFloat farPlane)
{
    rgFloat length = 1.0f / (right - left);
    rgFloat height = 1.0f / (top - bottom);
    rgFloat depth  = 1.0f / (farPlane - nearPlane);
    
    // LH 0 to 1
    return Matrix4(Vector4(2.0f * length, 0, 0, 0),
                   Vector4(0, 2.0f * height, 0, 0),
                   Vector4(0, 0, depth, 0),
                   Vector4(-((right + left) * length), -((top +bottom) * height), -nearPlane * depth, 1.0f));
}

Matrix4 makePerspectiveProjectionMatrix(rgFloat focalLength, rgFloat aspectRatio, rgFloat nearPlane, rgFloat farPlane)
{
    // focal length = 1 / tan(fov/2)

    // https://perry.cz/articles/ProjectionMatrix.xhtml
    // DirectX LH 0 to 1
    // A = ar * (1 / tan(fov/2));
    // B = (1 / tan(fov/2));
    // C = far / (far - near)
    // D = 1;
    // E = -near * (far / (far - near)
    
    // A 0 0 0
    // 0 B 0 0
    // 0 0 C D
    // 0 0 E 0
    
    rgFloat a = focalLength;
    rgFloat b = aspectRatio * focalLength;
    rgFloat c = farPlane / (farPlane - nearPlane);
    rgFloat d = 1;
    rgFloat e = -nearPlane * c;
    
    return Matrix4(Vector4(a, 0, 0, 0),
                   Vector4(0, b, 0, 0),
                   Vector4(0, 0, c, d),
                   Vector4(0, 0, e, 0));
}

TinyImageFormat convertSRGBToLinearFormat(TinyImageFormat srgbFormat)
{
    rgAssert(TinyImageFormat_IsSRGB(srgbFormat) == true);
    
    TinyImageFormat newFormat = TinyImageFormat_UNDEFINED;
    
    switch(srgbFormat)
    {
        case TinyImageFormat_R8G8B8A8_SRGB:
            newFormat = TinyImageFormat_R8G8B8A8_UNORM;
            break;
        case TinyImageFormat_B8G8R8A8_SRGB:
            newFormat = TinyImageFormat_B8G8R8A8_UNORM;
            break;
        INVALID_DEFAULT_CASE;
    }
    
    rgAssert(newFormat != TinyImageFormat_UNDEFINED);
    rgAssert(TinyImageFormat_IsSRGB(newFormat) == false);
    
    return newFormat;
}

TinyImageFormat convertLinearToSRGBFormat(TinyImageFormat linearFormat)
{
    rgAssert(TinyImageFormat_IsSRGB(linearFormat) == false);
    
    TinyImageFormat newFormat = TinyImageFormat_UNDEFINED;
    
    switch(linearFormat)
    {
        case TinyImageFormat_R8G8B8A8_UNORM:
            newFormat = TinyImageFormat_R8G8B8A8_SRGB;
            break;
        case TinyImageFormat_B8G8R8A8_UNORM:
            newFormat = TinyImageFormat_B8G8R8A8_SRGB;
            break;
    }
    
    rgAssert(newFormat != TinyImageFormat_UNDEFINED);
    rgAssert(TinyImageFormat_IsSRGB(newFormat) == true);
    
    return newFormat;
}
