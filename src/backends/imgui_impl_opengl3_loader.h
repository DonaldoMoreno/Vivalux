// Use GLAD as the OpenGL loader for ImGui backend
#ifndef IMGUI_IMPL_OPENGL3_LOADER_H
#define IMGUI_IMPL_OPENGL3_LOADER_H

// If the imgui backend previously defaulted to another loader (e.g. imgl3w),
// override it here and select GLAD instead.
#ifdef IMGUI_IMPL_OPENGL_LOADER_IMGL3W
#undef IMGUI_IMPL_OPENGL_LOADER_IMGL3W
#endif
#ifndef IMGUI_IMPL_OPENGL_LOADER_GLAD
#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#endif

#include <glad/glad.h>

#endif // IMGUI_IMPL_OPENGL3_LOADER_H
