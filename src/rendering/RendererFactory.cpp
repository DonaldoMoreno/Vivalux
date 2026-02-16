#include "RendererFactory.h"
#include "RendererOpenGL.h"

#ifdef __APPLE__
    #include "RendererVulkan.h"
    #define USE_VULKAN_ON_MAC
#endif

std::unique_ptr<Renderer> createPlatformRenderer() {
#ifdef __APPLE__
    // macOS: usar Vulkan + MoltenVK
    return std::make_unique<RendererVulkan>();
#else
    // Windows/Linux: usar OpenGL
    return std::make_unique<RendererOpenGL>();
#endif
}
