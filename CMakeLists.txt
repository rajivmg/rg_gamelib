cmake_minimum_required(VERSION 3.8)

# name of project
project(rg_gamelib)

set(EXECUTABLE_NAME "rg_gamelib" CACHE STRING "Output executable name")

# set output folder
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(WIN32)

    set(3RD_PARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty/win64/")
    
    # set vulkan sdk paths
    set(VULKAN_SDK_ROOT_DIR "C:/VulkanSDK/1.2.162.1" CACHE PATH "Vulkan SDK root dir") 

    # set source files
    file(GLOB SRC_FILES "code/*.cpp" "code/*.h")

    # set include and lib directories
    include_directories(${CMAKE_SOURCE_DIR}/3rdparty/inc ${3RD_PARTY_DIR}/inc ${VULKAN_SDK_ROOT_DIR}/Include)
    link_directories(${3RD_PARTY_DIR}/lib ${VULKAN_SDK_ROOT_DIR}/Lib)
    
    add_compile_definitions(RG_VULKAN_RNDR)

    # set compiler flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4201 /wd4100 /wd4189 /wd4505 /wd4127 /D_CRT_SECURE_NO_WARNINGS")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd /DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /DNDEBUG")

    # !!!!!!! TODO FIX AUTO Copy
    # copy dlls to executable's folder
    file(GLOB DLL_FILES "${3RD_PARTY_DIR}/dll/*.dll")
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/Release NO_SOURCE_PERMISSIONS)
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/Debug NO_SOURCE_PERMISSIONS)
    file(COPY ${DLL_FILES} DESTINATION ${CMAKE_BINARY_DIR}/RelWithDebInfo NO_SOURCE_PERMISSIONS)

    add_executable(rg_gamelib ${SRC_FILES})
    target_link_libraries(rg_gamelib SDL2main SDL2 vulkan-1)
    set_target_properties(rg_gamelib PROPERTIES OUTPUT_NAME ${EXECUTABLE_NAME})

    set_target_properties(rg_gamelib PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
    set_target_properties(rg_gamelib PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data")
    set_target_properties(rg_gamelib PROPERTIES VS_STARTUP_PROJECT rg_gamelib)

elseif(APPLE)

    set(CMAKE_CXX_STANDARD 17)
    set(3RD_PARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty/apple/")

    find_library(APPL_FOUNDATION Foundation REQUIRED)
    find_library(APPL_QUARTZ QuartzCore REQUIRED)
    find_library(APPL_METAL Metal REQUIRED)
    find_library(SDL2 SDL2 REQUIRED)

    # set source files
    file(GLOB SRC_FILES "code/*.cpp" "code/*.mm" "code/*.h" "code/shaders/metal/*.inl")
    file(GLOB SHADER_SRC_FILES "code/shaders/metal/*.metal")

    add_compile_definitions(RG_METAL_RNDR)
    include_directories(${CMAKE_SOURCE_DIR}/3rdparty/inc ${3RD_PARTY_DIR}/inc ${3RD_PARTY_DIR}/inc/metal-cpp ${3RD_PARTY_DIR}/inc/metal-cpp-extensions)

    add_executable(rg_gamelib ${SRC_FILES} ${SHADER_SRC_FILES})
    target_link_libraries(rg_gamelib ${APPL_FOUNDATION} ${APPL_QUARTZ} ${APPL_METAL} ${SDL2})

    set_source_files_properties(${SHADER_SRC_FILES} PROPERTIES LANGUAGE METAL)
endif()
