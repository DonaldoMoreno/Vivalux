#pragma once

#include "Renderer.h"
#include <memory>

// Factory function to create a platform-specific renderer
// Returns OpenGL renderer on Windows/Linux, Vulkan renderer on macOS
std::unique_ptr<Renderer> createPlatformRenderer();
