cmake_minimum_required(VERSION 3.8)

# name of project
project(rg_gamelib)

set(EXECUTABLE_NAME "rg_gamelib" CACHE STRING "Output executable name")

option(VULKAN "Enable Vulkan Graphics API (Windows)" OFF)
option(D3D12 "Enable D3D12 Graphics API (Windows)" ON)

option(MACOS_COMPILE_ONLY_NO_LINKING "To only compile the src files and not link. Used on CI Runners" OFF)

# set output folder
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(EASTL_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/EASTL)
include_directories(${EASTL_ROOT_DIR}/include)
include_directories(${EASTL_ROOT_DIR}/test/packages/EAAssert/include)
include_directories(${EASTL_ROOT_DIR}/test/packages/EABase/include/Common)
include_directories(${EASTL_ROOT_DIR}/test/packages/EAMain/include)
include_directories(${EASTL_ROOT_DIR}/test/packages/EAStdC/include)
include_directories(${EASTL_ROOT_DIR}/test/packages/EATest/include)
include_directories(${EASTL_ROOT_DIR}/test/packages/EAThread/include)

set(GLM_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/glm)
#include_directories(${GLM_ROOT_DIR})

set(IMGUI_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/imgui-1.89.4)
set(IMGUI_BACKEND_DIR ${IMGUI_ROOT_DIR}/backends)
include_directories(${IMGUI_ROOT_DIR})

file(GLOB GAME_SRC_FILES "code/game/*.cpp" "code/game/*.h")
file(GLOB IMGUI_SRC_FILES "${IMGUI_ROOT_DIR}/*.cpp" "${IMGUI_ROOT_DIR}/*.h")
source_group("dear-imgui" FILES ${IMGUI_SRC_FILES})

set(BOX2D_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/box2d)
include_directories(${BOX2D_ROOT_DIR}/include)
file(GLOB_RECURSE BOX2D_SRC_FILES "${BOX2D_ROOT_DIR}/src/*.cpp")
source_group("Box2D" FILES ${BOX2D_SRC_FILES})

set(GLEW_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/glew-2.2.0)

set(CGLTF_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/cgltf)
include_directories(${CGLTF_ROOT_DIR})

set(PUGIXML_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/pugixml)
include_directories(${PUGIXML_ROOT_DIR}/src)
file(GLOB PUGIXML_SRC_FILES "${PUGIXML_ROOT_DIR}/src/*.cpp" "${PUGIXML_ROOT_DIR}/src/*.hpp")
source_group("pugixml" FILES ${PUGIXML_SRC_FILES})

set(DIRECXHEADERS_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/DirectX-Headers-1.610.0)

set(DIRECTXMATH_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/DirectXMath-dec2022)

set(DIRECTXTEX_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/DirectXTex-jun2023/DirectXTex)
file(GLOB DIRECTXTEX_SRC_FILES "${DIRECTXTEX_ROOT_DIR}/*.cpp" "${DIRECTXTEX_ROOT_DIR}/*.h" "${DIRECTXTEX_ROOT_DIR}/*.inl")
set(DIRECTXTEX_INCLUDE_DIR ${DIRECTXTEX_ROOT_DIR}/Shaders/Compiled)
source_group("DirectXTex" FILES ${DIRECTXTEX_SRC_FILES})

set(DIRECTXTK12_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/DirectXTK12-sept2023)
file(GLOB_RECURSE DIRECTXTK12_SRC_FILES "${DIRECTXTK12_ROOT_DIR}/Src/*.cpp" "${DIRECTXTK12_ROOT_DIR}/Src/*.h" "${DIRECTXTK12_ROOT_DIR}/Inc/*.h")
list(REMOVE_ITEM DIRECTXTK12_SRC_FILES "${DIRECTXTK12_ROOT_DIR}/Src/ScreenGrab.cpp" "${DIRECTXTK12_ROOT_DIR}/Src/WICTextureLoader.cpp" "${DIRECTXTK12_ROOT_DIR}/Src/XboxDDSTextureLoader.cpp" "${DIRECTXTK12_ROOT_DIR}/Src/EffectTextureFactory.cpp" "${DIRECTXTK12_ROOT_DIR}/Src/Model.cpp" "${DIRECTXTK12_ROOT_DIR}/Src/ModelLoadCMO.cpp" "${DIRECTXTK12_ROOT_DIR}/Src/ModelLoadSDKMESH.cpp" "${DIRECTXTK12_ROOT_DIR}/Src/ModelLoadVBO.cpp")
set(DIRECTXTK12_INCLUDE_DIR ${DIRECTXTK12_ROOT_DIR}/Inc ${DIRECTXTK12_ROOT_DIR}/Src/Shaders/Compiled)
source_group("DirectXTK12" FILES ${DIRECTXTK12_SRC_FILES})

set(DXC_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/dxc_preview_2023_06_21)
set(DXC_INCLUDE_DIR ${DXC_ROOT_DIR}/inc)
set(DXC_LIB_DIR ${DXC_ROOT_DIR}/lib)

set(SPIRVCROSS_ROOT_DIR ${CMAKE_SOURCE_DIR}/3rdparty/SPIRV-Cross)
file(GLOB_RECURSE SPIRVCROSS_SRC_FILES "${SPIRVCROSS_ROOT_DIR}/*.cpp" "${SPIRVCROSS_ROOT_DIR}/*.hpp" "${SPIRVCROSS_ROOT_DIR}/*.h")
source_group("SPIRV-Cross" FILES ${SPIRVCROSS_SRC_FILES})

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB SHADER_FILES "${CMAKE_SOURCE_DIR}/code/shaders/*.hlsl" "${CMAKE_SOURCE_DIR}/code/shaders/*.h")
source_group("Shaders" FILES ${SHADER_FILES})

include_directories(${CMAKE_SOURCE_DIR}/code)

if(WIN32)

    set(3RD_PARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty/win64/")

    # set source files
    file(GLOB SRC_FILES "code/*.cpp" "code/*.h")
    if(VULKAN)
        file(GLOB IMGUI_BACKEND_SRC_FILES "${IMGUI_BACKEND_DIR}/imgui_impl_vulkan.cpp" "${IMGUI_BACKEND_DIR}/imgui_impl_vulkan.h")
    elseif(D3D12)
        file(GLOB IMGUI_BACKEND_SRC_FILES "${IMGUI_BACKEND_DIR}/imgui_impl_dx12.cpp" "${IMGUI_BACKEND_DIR}/imgui_impl_dx12.h" "${IMGUI_BACKEND_DIR}/imgui_impl_sdl2.h" "${IMGUI_BACKEND_DIR}/imgui_impl_sdl2.cpp")
    else()
        set(IMGUI_BACKEND_SRC_FILES "")
    endif()

    # set include and lib directories
    include_directories(${CMAKE_SOURCE_DIR}/3rdparty/inc ${3RD_PARTY_DIR}/inc ${DIRECTXTEX_ROOT_DIR})
    link_directories(${3RD_PARTY_DIR}/lib)
    set(EASTL_LIBRARY debug ${EASTL_ROOT_DIR}/build-win64/Debug/EASTL.lib optimized ${EASTL_ROOT_DIR}/build-win64/Release/EASTL.lib)
    
    if(VULKAN)
        include_directories(${CMAKE_SOURCE_DIR}/3rdparty/inc/Vulkan-Headers/include)
        add_compile_definitions(RG_VULKAN_RNDR VK_NO_PROTOTYPES)
    elseif(D3D12)
        include_directories(${CMAKE_SOURCE_DIR}/3rdparty/DirectX-Headers-1.610.0/include/directx ${DXC_INCLUDE_DIR} ${DIRECTXTEX_INCLUDE_DIR} ${DIRECTXTK12_INCLUDE_DIR})
        link_directories(${DXC_LIB_DIR}/x64)
        add_compile_definitions(RG_D3D12_RNDR)
        set(GRAPHICS_LIBS D3d12.lib d3dcompiler.lib DXGI.lib dxcompiler.lib)
    else()
        include_directories(${GLEW_ROOT_DIR}/include)
        add_compile_definitions(RG_OPENGL_RNDR GLEW_STATIC)
        set(GRAPHICS_LIBS opengl32.lib)
        set(GRAPHICS_SRC ${GLEW_ROOT_DIR}/src/glew.c)
    endif()
    

    # set compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4201 /wd4100 /wd4189 /wd4505 /wd4127 /D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD /DNDEBUG")

    # copy dlls to executable's folder
    set(DLLS_FOLDER_TO_COPY debug "${3RD_PARTY_DIR}/dll/debug/*.dll" optimized "${3RD_PARTY_DIR}/dll/release/*.dll")
    file(GLOB DLL_FILES ${DLLS_FOLDER_TO_COPY} "${DXC_ROOT_DIR}/bin/x64/*.dll")
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/Release NO_SOURCE_PERMISSIONS)
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/Debug NO_SOURCE_PERMISSIONS)
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/RelWithDebInfo NO_SOURCE_PERMISSIONS)

    add_executable(rg_gamelib ${SRC_FILES} ${GAME_SRC_FILES} ${IMGUI_SRC_FILES} ${IMGUI_BACKEND_SRC_FILES} ${BOX2D_SRC_FILES} ${DIRECTXTEX_SRC_FILES} ${DIRECTXTK12_SRC_FILES} ${PUGIXML_SRC_FILES} ${GRAPHICS_SRC} ${SHADER_FILES})
    target_link_libraries(rg_gamelib SDL2main SDL2 ${EASTL_LIBRARY} ${GRAPHICS_LIBS})
    set_target_properties(rg_gamelib PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})
    set_target_properties(rg_gamelib PROPERTIES CXX_STANDARD 17)
    set_target_properties(rg_gamelib PROPERTIES CXX_STANDARD_REQUIRED On)

    set_target_properties(rg_gamelib PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
    set_target_properties(rg_gamelib PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data")
    set_target_properties(rg_gamelib PROPERTIES VS_STARTUP_PROJECT rg_gamelib)

    add_executable(rg_assetprocessor "code/tools/rg_assetprocessor.cpp" ${PUGIXML_SRC_FILES})
    set_target_properties(rg_assetprocessor PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data")

    add_custom_target(NatVis SOURCES ${EASTL_ROOT_DIR}/doc/EASTL.natvis ${PUGIXML_ROOT_DIR}/scripts/natvis/pugixml.natvis)

    set_source_files_properties(${SHADER_FILES} PROPERTIES VS_TOOL_OVERRIDE "None")

elseif(APPLE)

    set(CMAKE_CXX_STANDARD 17)
    set(3RD_PARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty/apple/")

    find_library(APPL_FOUNDATION Foundation REQUIRED)
    find_library(APPL_QUARTZ QuartzCore REQUIRED)
    find_library(APPL_METAL Metal REQUIRED)
    find_library(SDL2 NAMES SDL2 PATHS ${3RD_PARTY_DIR} REQUIRED NO_DEFAULT_PATH)

    set(EASTL_LIBRARY ${EASTL_ROOT_DIR}/build-apple/libEASTL.a)
    set(DXC_LIBRARY ${DXC_LIB_DIR}/arm64/libdxcompiler.dylib)

    # set source files
    set(IMGUI_METAL_SRC ${IMGUI_ROOT_DIR}/backends/imgui_impl_metal.mm ${IMGUI_ROOT_DIR}/backends/imgui_impl_metal.h ${IMGUI_ROOT_DIR}/backends/imgui_impl_sdl2.h ${IMGUI_ROOT_DIR}/backends/imgui_impl_sdl2.cpp ${IMGUI_SRC_FILES})
    file(GLOB SRC_FILES "code/*.cpp" "code/*.mm" "code/*.h" ${IMGUI_METAL_SRC})

    add_compile_definitions(RG_METAL_RNDR __EMULATE_UUID)
    include_directories(${CMAKE_SOURCE_DIR}/3rdparty/inc ${3RD_PARTY_DIR}/inc ${3RD_PARTY_DIR}/inc/metal-cpp ${3RD_PARTY_DIR}/inc/metal-cpp-extensions ${DXC_INCLUDE_DIR})
    link_directories(${DXC_LIB_DIR}/arm64)

    list(REMOVE_ITEM DIRECTXTEX_SRC_FILES "${DIRECTXTEX_ROOT_DIR}/DirectXTexD3D11.cpp" "${DIRECTXTEX_ROOT_DIR}/DirectXTexD3D12.cpp" "${DIRECTXTEX_ROOT_DIR}/DirectXTexWIC.cpp" "${DIRECTXTEX_ROOT_DIR}/BCDirectCompute.cpp" "${DIRECTXTEX_ROOT_DIR}/BCDirectCompute.h" "${DIRECTXTEX_ROOT_DIR}/DirectXTexCompressGPU.cpp" "${DIRECTXTEX_ROOT_DIR}/DirectXTexFlipRotate.cpp")

    include_directories(${SPIRVCROSS_ROOT_DIR} ${DIRECXHEADERS_ROOT_DIR}/include ${DIRECXHEADERS_ROOT_DIR}/include/wsl/stubs ${DIRECTXMATH_ROOT_DIR}/Inc ${DIRECTXTEX_ROOT_DIR})
    
    if(MACOS_COMPILE_ONLY_NO_LINKING)
        add_library(rg_gamelib OBJECT ${SRC_FILES} ${GAME_SRC_FILES} ${BOX2D_SRC_FILES} ${SPIRVCROSS_SRC_FILES} ${PUGIXML_SRC_FILES} ${DIRECTXTEX_SRC_FILES} ${SHADER_FILES})
    else()
        add_executable(rg_gamelib ${SRC_FILES} ${GAME_SRC_FILES} ${BOX2D_SRC_FILES} ${SPIRVCROSS_SRC_FILES} ${PUGIXML_SRC_FILES} ${DIRECTXTEX_SRC_FILES} ${SHADER_FILES})
        target_link_libraries(rg_gamelib ${APPL_FOUNDATION} ${APPL_QUARTZ} ${APPL_METAL} ${SDL2} ${EASTL_LIBRARY} ${DXC_LIBRARY})
    endif()

    set_source_files_properties(${SHADER_FILES} PROPERTIES XCODE_EXPLICIT_FILE_TYPE HLSL)
    
    set_target_properties(rg_gamelib PROPERTIES XCODE_GENERATE_SCHEME TRUE
                                                XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data")
endif()
