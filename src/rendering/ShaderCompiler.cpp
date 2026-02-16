#include "ShaderCompiler.h"
#include <iostream>

// Nota: Implementación placeholder que devuelve error si no hay shaderc/glslang enlaces.
ShaderCompiler::CompileResult ShaderCompiler::compileGLSLtoSPIRV(const std::string& glsl_code, ShaderStage stage, const std::string& entry_point) {
    CompileResult r;
    r.success = false;
    r.error_message = "GLSL to SPIR-V compilation not available in this build. Install shaderc or glslang and enable it in CMake.";
    return r;
}

ShaderCompiler::CompileResult ShaderCompiler::compileWithShaderc(const std::string& glsl_code, ShaderStage stage, const std::string& entry_point) {
    CompileResult r;
    r.success = false;
    r.error_message = "Shaderc-based compilation not enabled.";
    return r;
}
