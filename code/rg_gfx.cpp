//#include <mmgr/mmgr.cpp>
#include "rg_gfx.h"

#include <string.h>
#include <EASTL/string.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "pugixml.hpp"
#include "DirectXTex.h"

// DEFAULT RESOURCES
QuadUV defaultQuadUV = { 0.0f, 0.0f, 1.0f, 1.0f };


//-----------------------------------------------------------------------------
// Helper functions
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

//-----------------------------------------------------------------------------
// Bitmaps and Images
//-----------------------------------------------------------------------------

ImageRef loadImage(char const* filename)
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
        output->mipCount = (rgUInt)metadata.mipLevels;
        output->sliceCount = (rgUInt)metadata.arraySize;
        output->isDDS = true;
        output->memory = (rgU8*)rgMalloc(scratchImage.GetPixelsSize());
        
        rgU8* basememory = output->memory;
        rgU32 offset = 0;
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
            return output;
        }
        
        output = eastl::shared_ptr<Image>(rgNew(Image), unloadImage);
        strncpy(output->tag, filename, rgArrayCount(Image::tag));
        output->tag[rgArrayCount(Image::tag) - 1] = '\0';
        output->width = width;
        output->height = height;
        output->format = TinyImageFormat_R8G8B8A8_UNORM;
        output->mipCount = 1;
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
    }
    else
    {
        stbi_image_free(ptr->memory);
    }
    rgDelete(ptr);
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

QuadUV createQuadUV(rgU32 xPx, rgU32 yPx, rgU32 widthPx, rgU32 heightPx, ImageRef image)
{
    return createQuadUV(xPx, yPx, widthPx, heightPx, image->width, image->height);
}

void pushTexturedQuad(TexturedQuads* quadList, QuadUV uv, rgFloat4 posSize, rgFloat4 offsetOrientation, GfxTexture* tex)
{
    TexturedQuad& q = quadList->push_back();
    q.uv = uv;
    q.pos = { posSize.x, posSize.y, 1.0f };
    q.size = posSize.zw;
    q.offsetOrientation = offsetOrientation;
    q.texID = gfx::bindlessManagerTexture->getBindlessIndex(tex);
}

//-----------------------------------------------------------------------------
// Model
//-----------------------------------------------------------------------------

ModelRef loadModel(char const* filename)
{
    FileData xmlFileData = fileRead(filename);
    if(!xmlFileData.isValid)
    {
        return nullptr;
    }
    
    pugi::xml_document modelDoc;
    pugi::xml_parse_result parseResult = modelDoc.load_buffer(xmlFileData.data, xmlFileData.dataSize);
    
    rgAssert(parseResult.status == pugi::xml_parse_status::status_ok);
    
    ModelRef outModel = eastl::shared_ptr<Model>(rgNew(Model), unloadModel);
    
    pugi::xml_node modelNode = modelDoc.first_child();
    
    strncpy(outModel->tag, modelNode.attribute("name").as_string(), sizeof(Model::tag));
    
    const char* binFilename = modelNode.attribute("bufferName").as_string();
    FileData binFileData = fileRead(binFilename);
    rgAssert(binFileData.isValid);
    
    outModel->vertexIndexBuffer = GfxBuffer::create(binFilename, GfxMemoryType_Default, binFileData.data, binFileData.dataSize, GfxBufferUsage_VertexBuffer | GfxBufferUsage_IndexBuffer);
    
    outModel->vertexBufferOffset = modelNode.attribute("vertexBufferOffset").as_uint();
    outModel->index32BufferOffset = modelNode.attribute("index32BufferOffset").as_uint();
    outModel->index16BufferOffset = modelNode.attribute("index16BufferOffset").as_uint();
    
    rgU32 meshCount = modelNode.attribute("meshCount").as_uint();
    
    for(auto meshNode : modelNode.children("mesh"))
    {
        Mesh m = {};
        strncpy(m.tag, meshNode.attribute("name").as_string(), sizeof(Mesh::tag));
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
        if(meshNode.attribute("hasBinormal").as_bool() == true)
        {
            m.properties |= MeshProperties_HasBinormal;
        }
        outModel->meshes.push_back(m);
    }
    
    return outModel;
}

void unloadModel(Model* ptr)
{
    
}


namespace gfx {
rgUInt frameNumber;

// CURRENT STATE
GfxRenderPass*          currentRenderPass;
GfxRenderCmdEncoder*    currentRenderCmdEncoder;
GfxComputeCmdEncoder*   currentComputeCmdEncoder;
GfxBlitCmdEncoder*      currentBlitCmdEncoder;
GfxGraphicsPSO*         currentGraphicsPSO;
GfxComputePSO*          currentComputePSO; // TODO: See if this is really necessary

GfxBindlessResourceManager<GfxTexture>*   bindlessManagerTexture;

// DEFAULT RESOURCES
GfxSamplerState*    bilinearSampler;
GfxFrameAllocator*  frameAllocators[RG_MAX_FRAMES_IN_FLIGHT];

eastl::vector<GfxTexture*> frameBeginJobGenTextureMipmaps;

GfxSamplerState*    samplerBilinearRepeat;
GfxSamplerState*    samplerBilinearClampEdge;
GfxSamplerState*    samplerTrilinearRepeatAniso;
GfxSamplerState*    samplerTrilinearClampEdgeAniso;
GfxSamplerState*    samplerNearestRepeat;
GfxSamplerState*    samplerNearestClampEdge;

// MISC
eastl::vector<GfxTexture*> debugTextureHandles; // test only
}

rgInt gfxPreInit()
{
    gfx::bindlessManagerTexture = rgNew(GfxBindlessResourceManager<GfxTexture>);
    
    return 0;
}

void styleImGui()
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

rgInt gfxPostInit()
{
    // Initialize frame buffer allocators
    for(rgS32 i = 0; i < RG_MAX_FRAMES_IN_FLIGHT; ++i)
    {
        gfx::frameAllocators[i] = rgNew(GfxFrameAllocator)(rgMegabyte(8), rgMegabyte(64), rgMegabyte(64));
    }
    
    // Initialize IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& imguiIO = ImGui::GetIO();
    imguiIO.Fonts->AddFontFromFileTTF("fonts/Noto_Sans/NotoSans-Regular.ttf", 18);
    styleImGui();

    gfxRendererImGuiInit();
    
    gfx::samplerBilinearRepeat = GfxSamplerState::create("samplerBilinearRepeat", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Nearest, false);
    gfx::samplerBilinearClampEdge = GfxSamplerState::create("samplerBilinearClampEdge", GfxSamplerAddressMode_ClampToEdge, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Nearest, false);
    gfx::samplerTrilinearRepeatAniso = GfxSamplerState::create("samplerTrilinearRepeatAniso", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Linear, true);
    gfx::samplerTrilinearClampEdgeAniso = GfxSamplerState::create("samplerTrilinearClampEdgeAniso", GfxSamplerAddressMode_ClampToEdge, GfxSamplerMinMagFilter_Linear, GfxSamplerMinMagFilter_Linear, GfxSamplerMipFilter_Linear, true);
    gfx::samplerNearestRepeat = GfxSamplerState::create("samplerNearestRepeat", GfxSamplerAddressMode_Repeat, GfxSamplerMinMagFilter_Nearest, GfxSamplerMinMagFilter_Nearest, GfxSamplerMipFilter_Nearest, false);
    gfx::samplerNearestClampEdge = GfxSamplerState::create("samplerNearestClampEdge", GfxSamplerAddressMode_ClampToEdge, GfxSamplerMinMagFilter_Nearest, GfxSamplerMinMagFilter_Nearest, GfxSamplerMipFilter_Nearest, false);

    
    return 0;
}

void gfxAtFrameStart()
{
    GfxBuffer::destroyMarkedObjects();
    GfxTexture::destroyMarkedObjects();
    GfxSamplerState::destroyMarkedObjects();
    GfxGraphicsPSO::destroyMarkedObjects();
    GfxComputePSO::destroyMarkedObjects();
    
    // Reset render pass
    gfx::currentRenderPass = nullptr;
    if(gfx::currentRenderCmdEncoder != nullptr)
    {
        rgDelete(gfx::currentRenderCmdEncoder);
        gfx::currentRenderCmdEncoder = nullptr;
    }
    
    // reset this frame's allocations
    gfx::frameAllocators[g_FrameIndex]->reset();
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
    return gfx::frameAllocators[g_FrameIndex];
}

static void endCurrentCmdEncoder()
{
    if(gfx::currentRenderCmdEncoder != nullptr)
    {
        if(!gfx::currentRenderCmdEncoder->hasEnded)
        {
            gfx::currentRenderCmdEncoder->end();
        }
        rgDelete(gfx::currentRenderCmdEncoder);
        gfx::currentRenderCmdEncoder = nullptr;
    }
    
    if(gfx::currentComputeCmdEncoder != nullptr)
    {
        if(!gfx::currentComputeCmdEncoder->hasEnded)
        {
            gfx::currentComputeCmdEncoder->end();
        }
        rgDelete(gfx::currentComputeCmdEncoder);
        gfx::currentComputeCmdEncoder = nullptr;
    }
    
    if(gfx::currentBlitCmdEncoder != nullptr)
    {
        if(!gfx::currentBlitCmdEncoder->hasEnded)
        {
            gfx::currentBlitCmdEncoder->end();
        }
        rgDelete(gfx::currentBlitCmdEncoder);
        gfx::currentBlitCmdEncoder = nullptr;
    }
}

GfxRenderCmdEncoder* gfxSetRenderPass(char const* tag, GfxRenderPass* renderPass)
{
    endCurrentCmdEncoder();

    gfx::currentRenderCmdEncoder = rgNew(GfxRenderCmdEncoder);
    gfx::currentRenderCmdEncoder->begin(tag, renderPass);

    gfx::currentRenderPass = renderPass;
    
    return gfx::currentRenderCmdEncoder;
}

GfxComputeCmdEncoder* gfxSetComputePass(char const* tag)
{
    endCurrentCmdEncoder();
    
    gfx::currentComputeCmdEncoder = rgNew(GfxComputeCmdEncoder);
    gfx::currentComputeCmdEncoder->begin(tag);
    
    return gfx::currentComputeCmdEncoder;
}

GfxBlitCmdEncoder* gfxSetBlitPass(char const* tag)
{
    endCurrentCmdEncoder();
    
    gfx::currentBlitCmdEncoder = rgNew(GfxBlitCmdEncoder);
    gfx::currentBlitCmdEncoder->begin(tag);
    gfx::currentBlitCmdEncoder->pushDebugTag(tag);
    
    return gfx::currentBlitCmdEncoder;
}

// --------------------

void genTexturedQuadVertices(TexturedQuads* quadList, eastl::vector<SimpleVertexFormat>* vertices, SimpleInstanceParams* instanceParams)
{
    rgUInt listSize = (rgUInt)quadList->size();
    for(rgUInt i = 0; i < listSize; ++i)
    {
        TexturedQuad& t = (*quadList)[i];
        
        SimpleVertexFormat v[4];
        // 0 - 1
        // 3 - 2
        v[0].pos[0] = t.pos.x;
        v[0].pos[1] = t.pos.y;
        v[0].pos[2] = t.pos.z;
        v[0].texcoord[0] = t.uv.uvTopLeft[0];
        v[0].texcoord[1] = t.uv.uvTopLeft[1];
        v[0].color[0] = 1.0f;
        v[0].color[1] = 1.0f;
        v[0].color[2] = 1.0f;
        v[0].color[3] = 1.0f;
        
        v[1].pos[0] = t.pos.x + t.size.x;
        v[1].pos[1] = t.pos.y;
        v[1].pos[2] = t.pos.z;
        v[1].texcoord[0] = t.uv.uvBottomRight[0];
        v[1].texcoord[1] = t.uv.uvTopLeft[1];
        v[1].color[0] = 1.0f;
        v[1].color[1] = 1.0f;
        v[1].color[2] = 1.0f;
        v[1].color[3] = 1.0f;
        
        v[2].pos[0] = t.pos.x + t.size.x;
        v[2].pos[1] = t.pos.y + t.size.y;
        v[2].pos[2] = t.pos.z;
        v[2].texcoord[0] = t.uv.uvBottomRight[0];
        v[2].texcoord[1] = t.uv.uvBottomRight[1];
        v[2].color[0] = 1.0f;
        v[2].color[1] = 1.0f;
        v[2].color[2] = 1.0f;
        v[2].color[3] = 1.0f;
        
        v[3].pos[0] = t.pos.x;
        v[3].pos[1] = t.pos.y + t.size.y;
        v[3].pos[2] = t.pos.z;
        v[3].texcoord[0] = t.uv.uvTopLeft[0];
        v[3].texcoord[1] = t.uv.uvBottomRight[1];
        v[3].color[0] = 1.0f;
        v[3].color[1] = 1.0f;
        v[3].color[2] = 1.0f;
        v[3].color[3] = 1.0f;
        
        vertices->push_back(v[0]);
        vertices->push_back(v[3]);
        vertices->push_back(v[1]);
        
        vertices->push_back(v[1]);
        vertices->push_back(v[3]);
        vertices->push_back(v[2]);
        
        instanceParams->texParam[i][0] = t.texID;
    }
}
