#include "ShaderCompiler.h"
#include <iostream>
// Try to use shaderc if available
#ifdef USE_SHADERC
#include <shaderc/shaderc.hpp>

static shaderc_shader_kind toShadercKind(ShaderCompiler::ShaderStage s) {
    switch (s) {
        case ShaderCompiler::ShaderStage::Vertex: return shaderc_vertex_shader;
        case ShaderCompiler::ShaderStage::Fragment: return shaderc_fragment_shader;
        case ShaderCompiler::ShaderStage::Compute: return shaderc_compute_shader;
    }
    return shaderc_glsl_infer_from_source;
}

ShaderCompiler::CompileResult ShaderCompiler::compileGLSLtoSPIRV(const std::string& glsl_code, ShaderStage stage, const std::string& entry_point) {
    CompileResult r;
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    shaderc_shader_kind kind = toShadercKind(stage);
    shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(glsl_code, kind, "shader.glsl", entry_point.c_str(), options);
    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        r.success = false;
        r.error_message = module.GetErrorMessage();
        return r;
    }

    r.success = true;
    r.spirv.assign(module.cbegin(), module.cend());
    return r;
}

ShaderCompiler::CompileResult ShaderCompiler::compileWithShaderc(const std::string& glsl_code, ShaderStage stage, const std::string& entry_point) {
    return compileGLSLtoSPIRV(glsl_code, stage, entry_point);
}
#else

// Fallback if shaderc is not enabled
ShaderCompiler::CompileResult ShaderCompiler::compileGLSLtoSPIRV(const std::string& glsl_code, ShaderStage stage, const std::string& entry_point) {
    CompileResult r;
    r.success = false;
    r.error_message = "GLSL to SPIR-V compilation not available in this build. Enable shaderc in CMake.";
    return r;
}

ShaderCompiler::CompileResult ShaderCompiler::compileWithShaderc(const std::string& glsl_code, ShaderStage stage, const std::string& entry_point) {
    CompileResult r;
    r.success = false;
    r.error_message = "Shaderc-based compilation not enabled.";
    return r;
}

#endif
