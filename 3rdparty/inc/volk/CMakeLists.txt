cmake_minimum_required(VERSION 3.0)
cmake_policy(PUSH)
cmake_policy(SET CMP0048 NEW) # project(... VERSION ...) support

project(volk VERSION 
# VOLK_GENERATE_VERSION
246
# VOLK_GENERATE_VERSION
  LANGUAGES C
)

# CMake 3.12 changes the default behaviour of option() to leave local variables
# unchanged if they exist (which we want), but we must work with older CMake versions.
if(NOT DEFINED VOLK_STATIC_DEFINES)
  option(VOLK_STATIC_DEFINES "Additional defines for building the volk static library, e.g. Vulkan platform defines" "")
endif()
if(NOT DEFINED VOLK_PULL_IN_VULKAN)
  option(VOLK_PULL_IN_VULKAN "Vulkan as a transitive dependency" ON)
endif()
if(NOT DEFINED VOLK_INSTALL)
  option(VOLK_INSTALL "Create installation targets" OFF)
endif()
if(NOT DEFINED VOLK_HEADERS_ONLY)
  option(VOLK_HEADERS_ONLY "Add interface library only" OFF)
endif()
if(NOT DEFINED VULKAN_HEADERS_INSTALL_DIR)
  option(VULKAN_HEADERS_INSTALL_DIR "Where to get the Vulkan headers" "")
endif()

# -----------------------------------------------------
# Static library

if(NOT VOLK_HEADERS_ONLY OR VOLK_INSTALL)
  add_library(volk STATIC volk.h volk.c)
  add_library(volk::volk ALIAS volk)
  target_include_directories(volk PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<INSTALL_INTERFACE:include>
  )
  if(VOLK_STATIC_DEFINES)
    target_compile_definitions(volk PUBLIC ${VOLK_STATIC_DEFINES})
  endif()
  if (NOT WIN32)
    target_link_libraries(volk PUBLIC dl)
  endif()
endif()

# -----------------------------------------------------
# Interface library

add_library(volk_headers INTERFACE)
add_library(volk::volk_headers ALIAS volk_headers)
target_include_directories(volk_headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
  $<INSTALL_INTERFACE:include>
)
if (NOT WIN32)
  target_link_libraries(volk_headers INTERFACE dl)
endif()

# -----------------------------------------------------
# Vulkan transitive dependency

if(VOLK_PULL_IN_VULKAN)
  # If CMake has the FindVulkan module and it works, use it.
  find_package(Vulkan QUIET)

  # Try an explicit CMake variable first, then any Vulkan targets
  # discovered by FindVulkan.cmake, then the $VULKAN_SDK environment
  # variable if nothing else works.
  if(VULKAN_HEADERS_INSTALL_DIR)
    message(" Vulkan as path")
    if(TARGET volk)
      target_include_directories(volk PUBLIC "${VULKAN_HEADERS_INSTALL_DIR}/include")
    endif()
    target_include_directories(volk_headers INTERFACE "${VULKAN_HEADERS_INSTALL_DIR}/include")
  elseif(TARGET Vulkan::Vulkan)
    message(" Vulkan as target")
    if(TARGET volk)
      target_link_libraries(volk PUBLIC Vulkan::Vulkan)
    endif()
    target_link_libraries(volk_headers INTERFACE Vulkan::Vulkan)
  elseif(TARGET Vulkan::Headers)
    message(" Vulkan headers as another cmake target")
    if(TARGET volk)
      target_link_libraries(volk PUBLIC Vulkan::Headers)
    endif()
    target_link_libraries(volk_headers INTERFACE Vulkan::Headers)
  elseif(DEFINED ENV{VULKAN_SDK})
    message(" Vulkan as VULKAN_SDK path")
    if(TARGET volk)
      target_include_directories(volk PUBLIC "$ENV{VULKAN_SDK}/include")
    endif()
    target_include_directories(volk_headers INTERFACE "$ENV{VULKAN_SDK}/include")
  endif()
endif()

# -----------------------------------------------------
# Installation

if(VOLK_INSTALL)

  include(GNUInstallDirs)
  set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/volk)

  # Install files
  install(FILES volk.h volk.c DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # Install library target and add it and any dependencies to export set.
  install(TARGETS volk volk_headers
    EXPORT volk-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )

  # Actually write exported config w/ imported targets
  install(EXPORT volk-targets
    FILE volkTargets.cmake
    NAMESPACE volk::
    DESTINATION ${INSTALL_CONFIGDIR}
  )

  # Create a ConfigVersion.cmake file:
  include(CMakePackageConfigHelpers)
  write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/volkConfigVersion.cmake
    COMPATIBILITY AnyNewerVersion
  )

  # Configure config file
  configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/cmake/volkConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/volkConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
  )

  # Install the fully generated config and configVersion files
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/volkConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/volkConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
  )

endif()
cmake_policy(POP)
