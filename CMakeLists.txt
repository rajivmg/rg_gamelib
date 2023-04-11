cmake_minimum_required(VERSION 3.8)

# name of project
project(rg_gamelib)

set(EXECUTABLE_NAME "rg_gamelib" CACHE STRING "Output executable name")

option(VULKAN "Enable Vulkan Graphics API (Windows)" OFF)

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

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(WIN32)

    set(3RD_PARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty/win64/")

    # set source files
    file(GLOB SRC_FILES "code/*.cpp" "code/*.h")
    if(VULKAN)
        file(GLOB IMGUI_BACKEND_SRC_FILES "${IMGUI_BACKEND_DIR}/imgui_impl_vulkan.cpp" "${IMGUI_BACKEND_DIR}/imgui_impl_vulkan.h")
    else()
        set(IMGUI_BACKEND_SRC_FILES "")
    endif()

    # set include and lib directories
    include_directories(${CMAKE_SOURCE_DIR}/3rdparty/inc ${3RD_PARTY_DIR}/inc)
    link_directories(${3RD_PARTY_DIR}/lib)
    set(EASTL_LIBRARY debug ${EASTL_ROOT_DIR}/build-win64/Debug/EASTL.lib optimized ${EASTL_ROOT_DIR}/build-win64/Release/EASTL.lib)
    
    if(VULKAN)
        include_directories(${CMAKE_SOURCE_DIR}/3rdparty/inc/Vulkan-Headers/include)
        add_compile_definitions(RG_VULKAN_RNDR VK_NO_PROTOTYPES)
        set(GRAPHICS_LIBS )
        set(GRAPHICS_SRC  )
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

    # !!!!!!! TODO FIX AUTO Copy
    # copy dlls to executable's folder
    file(GLOB DLL_FILES "${3RD_PARTY_DIR}/dll/*.dll")
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/Release NO_SOURCE_PERMISSIONS)
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/Debug NO_SOURCE_PERMISSIONS)
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/RelWithDebInfo NO_SOURCE_PERMISSIONS)

    add_executable(rg_gamelib ${SRC_FILES} ${GAME_SRC_FILES} ${IMGUI_SRC_FILES} ${IMGUI_BACKEND_SRC_FILES} ${BOX2D_SRC_FILES} ${GRAPHICS_SRC})
    target_link_libraries(rg_gamelib SDL2main SDL2 ${EASTL_LIBRARY} ${GRAPHICS_LIBS})
    set_target_properties(rg_gamelib PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})

    set_target_properties(rg_gamelib PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
    set_target_properties(rg_gamelib PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data")
    set_target_properties(rg_gamelib PROPERTIES VS_STARTUP_PROJECT rg_gamelib)

    add_custom_target(NatVis SOURCES ${EASTL_ROOT_DIR}/doc/EASTL.natvis)

elseif(APPLE)

    set(CMAKE_CXX_STANDARD 17)
    set(3RD_PARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty/apple/")

    find_library(APPL_FOUNDATION Foundation REQUIRED)
    find_library(APPL_QUARTZ QuartzCore REQUIRED)
    find_library(APPL_METAL Metal REQUIRED)
    find_library(SDL2 SDL2 REQUIRED)

    set(EASTL_LIBRARY ${EASTL_ROOT_DIR}/build-apple/libEASTL.a)

    # set source files
    file(GLOB SRC_FILES "code/*.cpp" "code/*.mm" "code/*.h" "code/shaders/metal/*.inl")
    file(GLOB SHADER_SRC_FILES "code/shaders/metal/*.metal")

    add_compile_definitions(RG_METAL_RNDR)
    include_directories(${CMAKE_SOURCE_DIR}/3rdparty/inc ${3RD_PARTY_DIR}/inc ${3RD_PARTY_DIR}/inc/metal-cpp ${3RD_PARTY_DIR}/inc/metal-cpp-extensions)

    add_executable(rg_gamelib ${SRC_FILES} ${GAME_SRC_FILES} ${SHADER_SRC_FILES} ${BOX2D_SRC_FILES})
    target_link_libraries(rg_gamelib ${APPL_FOUNDATION} ${APPL_QUARTZ} ${APPL_METAL} ${SDL2} ${EASTL_LIBRARY})

    set_source_files_properties(${SHADER_SRC_FILES} PROPERTIES LANGUAGE METAL)
    
    set_target_properties(rg_gamelib PROPERTIES XCODE_GENERATE_SCHEME TRUE
                                                XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data")
endif()
