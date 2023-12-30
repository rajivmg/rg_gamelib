#if defined(RG_VULKAN_RNDR)
#include "rg_gfx.h"

#define VOLK_IMPLEMENTATION
#include "volk/volk.h"

#include "vk_mem_alloc.h"

#include "vk-bootstrap/VkBootstrap.cpp"
#include "vk-bootstrap/VkBootstrap.h"
#include "vk-bootstrap/VkBootstrapDispatch.h"

#include <SDL2/SDL_vulkan.h>
#include <EASTL/fixed_vector.h>

// ----------
#define rgVK_CHECK(x) do { VkResult errCode = x; if(errCode) { rgLog("%s errCode:%d(0x%x)", #x, errCode, errCode); SDL_assert(!"Vulkan API call failed"); } } while(0)

struct VkGfxCtx
{
    vkb::Instance vkbInstance;
    vkb::Device vkbDevice;
    vkb::Swapchain vkbSwapchain;

    VkInstance inst;
    VkPhysicalDevice physicalDevice;
    rgUInt graphicsQueueIndex;

    VkDevice device;
    rgUInt deviceExtCount;
    char const** deviceExtNames;
    VkQueue graphicsQueue;

    VmaAllocator vmaAllocator;

    VkCommandPool graphicsCmdPool;
    VkCommandBuffer graphicsCmdBuffer;

    VkSurfaceKHR surface;

    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    eastl::vector<VkFramebuffer> framebuffers;

    VkRenderPass globalRenderPass;

    //VkFormat swapchainDesc; // is this the right way to store this info?

    PFN_vkCreateDebugReportCallbackEXT fnCreateDbgReportCallback;
    PFN_vkDestroyDebugReportCallbackEXT fnDestroyDbgReportCallback;
    VkDebugReportCallbackEXT dbgReportCallback;
} vk;
// ----------

RG_BEGIN_NAMESPACE

GfxCtx::VkGfxCtx* vkCtx()
{
    return &g_GfxCtx->vk;
}

static void createRenderPass(GfxCtx::VkGfxCtx* vk)
{
    VkAttachmentDescription attachment = {};
    attachment.format = vk->vkbSwapchain.image_format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &attachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    rgVK_CHECK(vkCreateRenderPass(vk->device, &rpInfo, NULL, &vk->globalRenderPass));
}

static VkShaderModule createShaderModuleFromSrc(GfxCtx::VkGfxCtx* vk, const char* srcCode)
{
    /*
    F:\rg_gamelib\3rdparty\dxc_2023_03_01\bin\x64>dxc "F:\rg_gamelib\code\shaders\vulkan\triangle.hlsl" -spirv -T vs_6_0 -E VS_triangle -Fo "F:\rg_gamelib\code\shaders\vulkan\vs_triangle.spirv"
    F:\rg_gamelib\3rdparty\dxc_2023_03_01\bin\x64>dxc "F:\rg_gamelib\code\shaders\vulkan\triangle.hlsl" -spirv -T ps_6_0 -E FS_triangle -Fo "F:\rg_gamelib\code\shaders\vulkan\fs_triangle.spirv"
    */
    return VkShaderModule(0);
}

static void createPipeline(GfxCtx::VkGfxCtx* vk)
{
    auto da = vkCreateShadersEXT;

    VkPipelineLayout pipelineLayout;
    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    rgVK_CHECK(vkCreatePipelineLayout(vk->device, &layoutInfo, NULL, &pipelineLayout));

    VkPipelineVertexInputStateCreateInfo vertInput = {};
    vertInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo iaInfo = {};
    iaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineRasterizationStateCreateInfo rasterInfo = {};
    rasterInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterInfo.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttach = {};
    blendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    
    VkPipelineColorBlendStateCreateInfo blendInfo = {};
    blendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendInfo.attachmentCount = 1;
    blendInfo.pAttachments = &blendAttach;

    VkPipelineViewportStateCreateInfo viewportInfo = {};
    viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportInfo.viewportCount = 1;
    viewportInfo.scissorCount = 1;

    VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {};
    depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    
    VkPipelineMultisampleStateCreateInfo multisampleInfo = {};
    multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    eastl::fixed_vector<VkDynamicState, 2> dynStates;
    dynStates.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynStates.push_back(VK_DYNAMIC_STATE_SCISSOR);

    VkPipelineDynamicStateCreateInfo dynInfo = {};
    dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynInfo.pDynamicStates = &dynStates.front();
    dynInfo.dynamicStateCount = (uint32_t)dynStates.size();

    eastl::fixed_vector<VkPipelineShaderStageCreateInfo, 2> shaderStages;
    FileData vsFile = readFile("shaders/vulkan/vs_triangle.spirv");
    rgAssert(vsFile.isValid);
}

rgInt gfxInit()
{
    GfxCtx::VkGfxCtx* vk = &gfxCtx()->vk;

    rgVK_CHECK(volkInitialize()); /// volk

    // -- create vk instance
    vkb::InstanceBuilder instBuilder;
    vkb::Result<vkb::Instance> vkbInstRes = instBuilder.set_app_name("gamelib")
        .request_validation_layers()
        .build();

    if(!vkbInstRes)
    {
        rgLogError("Could not create Vulkan Instance\nReason:%s", vkbInstRes.error().message().c_str());
        return -1;
    }

    vk->vkbInstance = vkbInstRes.value();
    vk->inst = vk->vkbInstance.instance;

    volkLoadInstance(vk->inst); /// volk

    // -- enumerate and select a physical device
    rgAssert(SDL_Vulkan_CreateSurface(gfxCtx()->mainWindow, vk->inst, &(vk->surface)));
    rgAssert(vk->surface);

    vkb::PhysicalDeviceSelector phyDevSelector{ vk->vkbInstance };
    vkb::Result<vkb::PhysicalDevice> phyDevRes = phyDevSelector.set_surface(vk->surface)
        .set_minimum_version(1, 3)
        .require_present(true)
        /*.add_required_extension("VK_EXT_shader_object")*/
        .select();

    if(!phyDevRes)
    {
        rgLogError("Could not select a physical Vulkan supported device\nReason:%s", phyDevRes.error().message().c_str());
        return -1;
    }

    vkb::PhysicalDevice vkbPhyDevice = phyDevRes.value();
    vk->physicalDevice = vkbPhyDevice.physical_device;
    
    // -- create logical device
    vkb::DeviceBuilder devBuilder{ vkbPhyDevice };
    vkb::Result<vkb::Device> devBuilderRes = devBuilder.build();

    if(!devBuilderRes)
    {
        rgLogError("Could not create Vulkan device\nReason:%s", devBuilderRes.error().message().c_str());
        return -1;
    }

    vk->vkbDevice = devBuilderRes.value();
    vk->device = vk->vkbDevice.device;

    volkLoadDevice(vk->device); /// volk

    // -- get graphics queue
    vkb::Result<VkQueue> grQueRes = vk->vkbDevice.get_queue(vkb::QueueType::graphics);
    if(!grQueRes)
    {
        rgLogError("Could not get Vulkan graphics queue\nReason:%s", grQueRes.error().message().c_str());
        return -1;
    }

    vk->graphicsQueue = grQueRes.value();

    // -- create swapchain
    vkb::SwapchainBuilder scBuilder{ vk->vkbDevice };
    vkb::Result<vkb::Swapchain> scBuilderRes = scBuilder.use_default_present_mode_selection()
        .build();

    if(!scBuilderRes)
    {
        rgLogError("Could not create Vulkan swapchain\nReason:%s", scBuilderRes.error().message().c_str());
        return -1;
    }

    vk->vkbSwapchain = scBuilderRes.value();
    vk->swapchain = vk->vkbSwapchain.swapchain;

    createRenderPass(vk);

    // -- create swapchain imageviews
    vk->swapchainImages = vk->vkbSwapchain.get_images().value();
    vk->swapchainImageViews = vk->vkbSwapchain.get_image_views().value();
    
    vk->framebuffers.resize(vk->swapchainImageViews.size());

    for(rgInt i = 0; i < vk->swapchainImageViews.size(); ++i)
    {
        VkImageView attachements[] = { vk->swapchainImageViews[i] };

        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = vk->globalRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachements;
        fbInfo.width = vk->vkbSwapchain.extent.width;
        fbInfo.height = vk->vkbSwapchain.extent.height;
        fbInfo.layers = 1;

        rgVK_CHECK(vkCreateFramebuffer(vk->device, &fbInfo, NULL, &vk->framebuffers[i]));
    }

    VmaAllocatorCreateInfo vmaInfo = {};
    vmaInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    vmaInfo.physicalDevice = vk->physicalDevice;
    vmaInfo.device = vk->device;
    vmaInfo.instance = vk->inst;

    vmaCreateAllocator(&vmaInfo, &vk->vmaAllocator);

    createPipeline(vk);

    return 0;
}

void destroy()
{
    GfxCtx::VkGfxCtx* vk = &gfxCtx()->vk;
    vk->vkbSwapchain.destroy_image_views(vk->swapchainImageViews);
    vkb::destroy_swapchain(vk->vkbSwapchain);
    vkb::destroy_device(vk->vkbDevice);
    vkb::destroy_surface(vk->vkbInstance, vk->surface);
    vkb::destroy_instance(vk->vkbInstance);

    // Destroy SDL Window
}

rgInt gfxDraw()
{
    gfxGetRenderCmdList()->draw();
    gfxGetRenderCmdList()->afterDraw();

    return 0;
}

GfxBuffer* creatorGfxBuffer(void* data, rgSize size, GfxResourceUsage usage)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
        | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
        | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
        | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
        | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
        | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
        | VK_BUFFER_USAGE_CONDITIONAL_RENDERING_BIT_EXT
        | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

    GfxBuffer* bufferPtr = rgNew(GfxBuffer);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.memoryTypeBits = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    vmaCreateBuffer(vkCtx()->vmaAllocator, &bufferInfo, &allocInfo, &bufferPtr->vkBuffers[0], &bufferPtr->vmaAlloc, nullptr);
    
    return nullptr;
}

void updaterGfxBuffer(GfxBuffer* buffer, void* data, rgSize size, rgU32 offset)
{

}

void deleterGfxBuffer(GfxBuffer* buffer)
{

}

GfxTexture2D* creatorGfxTexture2D(void* buf, rgUInt width, rgUInt height, TinyImageFormat format, GfxTextureUsage usage, char const* name)
{
    return rgNew(GfxTexture2D);
}

void deleterGfxTexture2D(GfxTexture2D* t2d)
{

}

GfxGraphicsPSO* creatorGfxGraphicsPSO(GfxShaderDesc *shaderDesc, GfxRenderStateDesc* renderStateDesc)
{
    return nullptr;
}

void deleterGfxGraphicsPSO(GfxGraphicsPSO* pso)
{

}

void gfxHandleRenderCmd_SetViewport(void const* cmd)
{

}

void gfxHandleRenderCmd_DrawTexturedQuads(void const* cmd)
{
}

void gfxHandleRenderCmd_SetRenderPass(void const* cmd)
{

}

void gfxHandleRenderCmd_SetGraphicsPSO(void const* cmd)
{

}

void gfxHandleRenderCmd_DrawTriangles(void const* cmd)
{

}

#if 0
static Bool CreateWindowVK(GfxCtx* ctx)
{
    UInt windowWidth = 640;
    UInt windowHeight = 480;
    if (SDL_Init(SDL_INIT_VIDEO) == 0)
    {
        ctx->mainWindow = SDL_CreateWindow("Gfx",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            100, 100, SDL_WINDOW_VULKAN);
        rgAssert(ctx->mainWindow != NULL);
        return true;
    }
    return false;
}

static void CreateInstanceVK(GfxCtx* ctx)
{
    //layers
    char const* deviceLayers[] = { "VK_LAYER_KHRONOS_validation" };

    // extensions
    UInt sdlExtCount = 0;
    rgAssert(SDL_Vulkan_GetInstanceExtensions(ctx->mainWindow, &sdlExtCount, NULL));
    rgAssert(sdlExtCount > 0);
    
    char const* extraExts[] = { VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
    Int extraExtCount = rgARRAY_COUNT(extraExts);

    ctx->vk.deviceExtCount = sdlExtCount + extraExtCount;
    ctx->vk.deviceExtNames = (char const**)rgMalloc(sizeof(char const*) * ctx->vk.deviceExtCount);

    UInt extCount = sdlExtCount;
    rgAssert(SDL_Vulkan_GetInstanceExtensions(ctx->mainWindow, &extCount, ctx->vk.deviceExtNames));
    rgAssert(extCount == sdlExtCount);
    
    extCount += extraExtCount;
    rgAssert(extCount == ctx->vk.deviceExtCount);
    for (UInt i = sdlExtCount; i < ctx->vk.deviceExtCount; ++i)
    {
        ctx->vk.deviceExtNames[i] = extraExts[i - sdlExtCount];
    }

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.pApplicationName = "GreenStuff";
    appInfo.pEngineName = "RG_GFX";
    appInfo.engineVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instInfo = {};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pNext = NULL;
    instInfo.flags = 0;
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledLayerCount = rgARRAY_COUNT(deviceLayers);
    instInfo.ppEnabledLayerNames = deviceLayers;
    instInfo.enabledExtensionCount = ctx->vk.deviceExtCount;
    instInfo.ppEnabledExtensionNames = ctx->vk.deviceExtNames;
    instInfo.enabledLayerCount = 0;
    instInfo.ppEnabledLayerNames = NULL;

    VkResult createInstanceRes = vkCreateInstance(&instInfo, NULL, &ctx->vk.inst);
    if (createInstanceRes == VK_ERROR_INCOMPATIBLE_DRIVER)
    {
        // rgLogError("vkCreateInstance() ERR: VK_ERROR_INCOMPATIBLE_DRIVER");
        rgAssert(!"vkCreateInstance() ERR: VK_ERROR_INCOMPATIBLE_DRIVER");
    }
    else if (createInstanceRes)
    {
        // rgLogError("vkCreateInstance() ERR: Unknown Error");
        rgAssert(!"vkCreateInstance() ERR: Unknown Error");
    }
    //VK_KHR_DISPLAY_EXTENSION_NAME;
    rgAssert(SDL_Vulkan_CreateSurface(ctx->mainWindow, ctx->vk.inst, &ctx->vk.surface));
    rgAssert(ctx->vk.surface);
}

VkBool32 VkDebugReportCallbackFuntionImpl(
    VkDebugReportFlagsEXT                       flags,
    VkDebugReportObjectTypeEXT                  objectType,
    uint64_t                                    object,
    size_t                                      location,
    int32_t                                     messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
{
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        rgLog("[%s] %d(0x%x):%s", pLayerPrefix, messageCode, messageCode, pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        rgLog("[%s] %d(0x%x):%s", pLayerPrefix, messageCode, messageCode, pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        rgLog("[%s] %d(0x%x):%s", pLayerPrefix, messageCode, messageCode, pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        rgLog("[%s] %d(0x%x):%s", pLayerPrefix, messageCode, messageCode, pMessage);
    }
    else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
    {
        rgLog("[%s] %d(0x%x):%s", pLayerPrefix, messageCode, messageCode, pMessage);
    }

    return true;
}

static void CreateDeviceVK(GfxCtx* ctx)
{
    // Steps:
    // 1. Enumerate all physical devices to select suitable one
    // 2. Create Vulkan Device with the selected physical device

    // 1. Enumerate physical devices
    {
        UInt physicalDeviceCount = 0;
        rgVK_CHECK(vkEnumeratePhysicalDevices(ctx->vk.inst, &physicalDeviceCount, NULL));
        rgAssert(physicalDeviceCount);
        VkPhysicalDevice* physicalDevices = (VkPhysicalDevice*)rgMalloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
        rgVK_CHECK(vkEnumeratePhysicalDevices(ctx->vk.inst, &physicalDeviceCount, physicalDevices));
        rgAssert(physicalDeviceCount > 0);

        auto IsDeviceSuitable = [&ctx](VkPhysicalDevice d, rg::UInt* outQueueFamilyIndex) -> rg::Bool
        {
            rg::Bool result = false;
            VkPhysicalDeviceProperties deviceProp;
            vkGetPhysicalDeviceProperties(d, &deviceProp);
            if (deviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                rg::UInt queueFamilyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(d, &queueFamilyCount, NULL);
                rgAssert(queueFamilyCount > 0);

                VkQueueFamilyProperties* queueFamilies = (VkQueueFamilyProperties*)rgMalloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(d, &queueFamilyCount, queueFamilies);
                rgAssert(queueFamilyCount > 0);

                for (rg::UInt i = 0; i < queueFamilyCount; ++i)
                {
                    VkBool32 supportsPresent = true;
                    vkGetPhysicalDeviceSurfaceSupportKHR(d, i, ctx->vk.surface, &supportsPresent);

                    if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supportsPresent)
                    {
                        *outQueueFamilyIndex = i;
                        result = true;
                        break;
                    }
                }

                rgFree(queueFamilies);
            }
            return result;
        };

        Bool foundSuitablePhysicalDevice = false;
        for (UInt j = 0; j < physicalDeviceCount; ++j)
        {
            UInt queueFamilyIndex;
            if (IsDeviceSuitable(physicalDevices[j], &queueFamilyIndex))
            {
                ctx->vk.physicalDevice = physicalDevices[j];
                ctx->vk.graphicsQueueIndex = queueFamilyIndex;
                foundSuitablePhysicalDevice = true;
                break;
            }
        }
        rgAssert(foundSuitablePhysicalDevice);
        rgAssert(ctx->vk.physicalDevice);

        rgFree(physicalDevices);
    }

    // 2. Create Vulkan device
    {
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.pNext = NULL;
        queueInfo.queueCount = 1;
        queueInfo.queueFamilyIndex = ctx->vk.graphicsQueueIndex;
        rg::Float queuePriorities[1] = { 1.f };
        queueInfo.pQueuePriorities = queuePriorities;

        char const* extName[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        VkDeviceCreateInfo deviceInfo = {};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = NULL;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = rgARRAY_COUNT(extName);
        deviceInfo.ppEnabledExtensionNames = extName;
        deviceInfo.enabledLayerCount = 0;
        deviceInfo.ppEnabledLayerNames = NULL;
        deviceInfo.pEnabledFeatures = NULL;

        rgVK_CHECK(vkCreateDevice(ctx->vk.physicalDevice, &deviceInfo, NULL, &ctx->vk.device));

        vkGetDeviceQueue(ctx->vk.device, ctx->vk.graphicsQueueIndex, 0, &ctx->vk.graphicsQueue);
    }
    // debug report callback    
    {
        ctx->vk.fnCreateDbgReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(ctx->vk.inst, "vkCreateDebugReportCallbackEXT");
        ctx->vk.fnDestroyDbgReportCallback = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(ctx->vk.inst, "vkDestroyDebugReportCallbackEXT");
        rgAssert(ctx->vk.fnCreateDbgReportCallback);
        rgAssert(ctx->vk.fnDestroyDbgReportCallback);

        VkDebugReportCallbackCreateInfoEXT info = {};
        info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        info.pNext = NULL;
        info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        info.pfnCallback = VkDebugReportCallbackFuntionImpl;
        info.pUserData = NULL;
        rgVK_CHECK(ctx->vk.fnCreateDbgReportCallback(ctx->vk.inst, &info, NULL, &ctx->vk.dbgReportCallback));
    }
}

static void CreateSwapchainVK(GfxCtx* ctx)
{
    VkSurfaceCapabilitiesKHR surfaceProp;
    rgVK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx->vk.physicalDevice, ctx->vk.surface, &surfaceProp));
    
    // 1. Select swapchain surface format
    UInt formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->vk.physicalDevice, ctx->vk.surface, &formatCount, NULL);
    rgAssert(formatCount > 0);
    VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*)rgMalloc(sizeof(VkSurfaceFormatKHR) * formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(ctx->vk.physicalDevice, ctx->vk.surface, &formatCount, formats);

    VkSurfaceFormatKHR surfaceFormat;
    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
    {
        surfaceFormat = formats[0];
        surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
    }
    else
    {
        surfaceFormat.format = VK_FORMAT_UNDEFINED;
        for (UInt i = 0; i < formatCount; ++i)
        {
            switch (formats[i].format)
            {
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
                surfaceFormat = formats[i];
                break;
            default:
                break;
            }

            if (surfaceFormat.format != VK_FORMAT_UNDEFINED)
            {
                break;
            }
        }
    }

    rgFree(formats);


    // 2. --
    VkExtent2D swapchainSize;
    if (surfaceProp.currentExtent.width == 0xFFFFFFFF)
    {
        rgAssert(!"Henlo");
    }
    else
    {
        swapchainSize = surfaceProp.currentExtent;
    }

    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    UInt desiredSwapchainImages = surfaceProp.minImageCount + 1;
    if ((surfaceProp.maxImageCount > 0) && (desiredSwapchainImages > surfaceProp.maxImageCount))
    {
        desiredSwapchainImages = surfaceProp.maxImageCount;
    }

    VkSurfaceTransformFlagBitsKHR preTransform;
    if (surfaceProp.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        preTransform = surfaceProp.currentTransform;
    }

    VkSwapchainKHR oldSwapchain = ctx->vk.swapchain;

    VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (surfaceProp.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    {
        composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    }
    else if (surfaceProp.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    {
        composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    }
    else if (surfaceProp.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
    {
        composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }
    else if (surfaceProp.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
    {
        composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
    }

    VkSwapchainCreateInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    info.pNext = NULL;
    info.surface = ctx->vk.surface;
    info.minImageCount = desiredSwapchainImages;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageExtent.width = swapchainSize.width;
    info.imageExtent.height = swapchainSize.height;
    info.preTransform = preTransform;
    info.compositeAlpha = composite;
    info.imageArrayLayers = 1;
    info.presentMode = swapchainPresentMode;
    info.oldSwapchain = oldSwapchain;
    info.clipped = true;
    info.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = 0;
    info.pQueueFamilyIndices = NULL;

    rgVK_CHECK(vkCreateSwapchainKHR(ctx->vk.device, &info, NULL, &ctx->vk.swapchain));

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        rgAssert(!"TODO: destroy prev swapchain & free image resources");
    }

    Uint32 imageCount = 0;
    rgVK_CHECK(vkGetSwapchainImagesKHR(ctx->vk.device, ctx->vk.swapchain, &imageCount, NULL));

    VkImage* swapchainImages = (VkImage*)rgMalloc(sizeof(VkImage) * imageCount);
    rgVK_CHECK(vkGetSwapchainImagesKHR(ctx->vk.device, ctx->vk.swapchain, &imageCount, swapchainImages));
    
    // TODO: Init per-frame resources here

    //

    ctx->vk.swapchainImageViews = (VkImageView*)rgMalloc(sizeof(VkImageView) * imageCount);

    for (UInt i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.pNext = NULL;
        viewInfo.flags = 0;
        viewInfo.image = swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format; // is this correct?
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        rgVK_CHECK(vkCreateImageView(ctx->vk.device, &viewInfo, NULL, &imageView));
        ctx->vk.swapchainImageViews[i] = imageView;
    }
}

void CreateCmdBufferVK(GfxCtx* const ctx)
{
    // 1. Create cmd pool
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.pNext = NULL;
    poolInfo.queueFamilyIndex = ctx->vk.graphicsQueueIndex;
    poolInfo.flags = 0;

    rgVK_CHECK(vkCreateCommandPool(ctx->vk.device, &poolInfo, NULL, &ctx->vk.graphicsCmdPool));

    // 2. Allocate cmd buffer from the cmd pool
    VkCommandBufferAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.pNext = NULL;
    allocateInfo.commandPool = ctx->vk.graphicsCmdPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    rgVK_CHECK(vkAllocateCommandBuffers(ctx->vk.device, &allocateInfo, &ctx->vk.graphicsCmdBuffer));
}

void DestroyCmdBufferVK(GfxCtx* const ctx)
{
    VkCommandBuffer cmdBuffers[] = { ctx->vk.graphicsCmdBuffer };
    vkFreeCommandBuffers(ctx->vk.device, ctx->vk.graphicsCmdPool, rgARRAY_COUNT(cmdBuffers), cmdBuffers);
    vkDestroyCommandPool(ctx->vk.device, ctx->vk.graphicsCmdPool, NULL);
}

Int runApp(GfxCtx* gfxCtx)
{
    SDL_memset(&(ctx->vk), 0, sizeof(ctx->vk));

    if (CreateWindowVK(ctx))
    {
        CreateInstanceVK(ctx);
        CreateDeviceVK(ctx);
        CreateSwapchainVK(ctx);
        CreateCmdBufferVK(ctx);
    }
    else
    {
        return 1;
    }


    // Destroy
    vkDeviceWaitIdle(ctx->vk.device);

    DestroyCmdBufferVK(ctx);

    if (ctx->vk.swapchain)
    {
        vkDestroySwapchainKHR(ctx->vk.device, ctx->vk.swapchain, NULL);
    }

    if (ctx->vk.surface)
    {
        vkDestroySurfaceKHR(ctx->vk.inst, ctx->vk.surface, NULL);
    }

    if (ctx->vk.device)
    {
        vkDestroyDevice(ctx->vk.device, NULL);
    }
    
    if (ctx->vk.deviceExtNames)
    {
        rgFree(ctx->vk.deviceExtNames);
    }

    if (ctx->vk.fnDestroyDbgReportCallback)
    {
        ctx->vk.fnDestroyDbgReportCallback(ctx->vk.inst, ctx->vk.dbgReportCallback, NULL);
    }
    
    if (ctx->vk.inst)
    {
        vkDestroyInstance(ctx->vk.inst, NULL);
    }
    // DestroyWindow
    
    
    return 0;
}
#endif

RG_END_NAMESPACE
#endif
