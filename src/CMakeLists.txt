find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# nativefiledialog-extended doesn't provide CMake config files, so we find it manually
find_path(NFD_INCLUDE_DIR nfd.h)
find_library(NFD_LIBRARY nfd)

# Find dbus for nativefiledialog-extended on Linux
if(NOT APPLE)
  find_package(PkgConfig)
  pkg_check_modules(DBUS IMPORTED_TARGET dbus-1)
endif()

# Try to find Vulkan SDK (optional for macOS)
find_package(Vulkan QUIET)

# Find FFmpeg using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libswscale libavutil)

# Platform-specific rendering sources
set(RENDERING_SOURCES
  rendering/RendererFactory.cpp
  rendering/RendererOpenGL.cpp
  rendering/RendererVulkan.cpp
  rendering/ShaderCompiler.cpp
)

add_executable(VivaLux
  main.cpp
  backends/imgui_impl_glfw.cpp
  backends/imgui_impl_opengl3.cpp
  ${RENDERING_SOURCES}
)

target_compile_features(VivaLux PUBLIC cxx_std_20)

target_include_directories(VivaLux PRIVATE 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${CMAKE_CURRENT_SOURCE_DIR}/backends 
  ${CMAKE_CURRENT_SOURCE_DIR}/rendering
  ${Stb_INCLUDE_DIR}
)

# Add nativefiledialog-extended include directory
if(NFD_INCLUDE_DIR)
  target_include_directories(VivaLux PRIVATE ${NFD_INCLUDE_DIR})
endif()

# Platform-specific compilation flags
if(APPLE)
  # macOS: Use Vulkan + MoltenVK
  target_compile_definitions(VivaLux PRIVATE "__APPLE__" "VK_USE_PLATFORM_METAL_KHR")
  find_package(MoltenVK QUIET)
endif()

target_link_libraries(VivaLux PRIVATE
  glfw
  glad::glad
  imgui::imgui
  OpenGL::GL
  glm::glm
  nlohmann_json::nlohmann_json
  PkgConfig::FFMPEG
  ${NFD_LIBRARY}
)

# Link dbus for nativefiledialog-extended on Linux
if(DBUS_FOUND)
  target_link_libraries(VivaLux PRIVATE PkgConfig::DBUS)
endif()

# macOS-specific frameworks for FFmpeg
if(APPLE)
  target_link_libraries(VivaLux PRIVATE
    "-framework VideoToolbox"
    "-framework CoreFoundation"
    "-framework CoreMedia"
    "-framework CoreVideo"
    "-framework CoreServices"
    "-framework AVFoundation"
    "-framework IOSurface"
  )
endif()

# Vulkan support (link if available)
if(Vulkan_FOUND)
  target_link_libraries(VivaLux PRIVATE Vulkan::Vulkan)
  if (DEFINED Vulkan_INCLUDE_DIRS)
    target_include_directories(VivaLux PRIVATE ${Vulkan_INCLUDE_DIRS})
  endif()
endif()

