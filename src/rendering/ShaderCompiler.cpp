#include "ShaderCompiler.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <memory>
#include <cstring>

// Helper function to invoke glslangValidator as external tool
// This avoids complex C++ API dependencies while still providing proper compilation
static std::vector<uint32_t> compile_with_glslangValidator(
    const std::string& glsl_code,
    ShaderCompiler::ShaderStage stage,
    std::string& error_message) {
    
    std::vector<uint32_t> empty_spirv;
    
    // Determine file extension and glslang stage name
    const char* stage_str = nullptr;
    const char* file_ext = nullptr;
    switch (stage) {
        case ShaderCompiler::ShaderStage::Vertex:
            stage_str = "vert";
            file_ext = ".vert";
            break;
        case ShaderCompiler::ShaderStage::Fragment:
            stage_str = "frag";
            file_ext = ".frag";
            break;
        case ShaderCompiler::ShaderStage::Compute:
            stage_str = "comp";
            file_ext = ".comp";
            break;
        default:
            error_message = "Unknown shader stage";
            return empty_spirv;
    }
    
    // Create temporary files for input and output
    // Use environment variable TMPDIR if available, else fall back to /tmp
    const char* tmpdir = std::getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    
    std::string temp_base = std::string(tmpdir) + "/vivalux_shader_";
    std::string temp_glsl_file = temp_base + "XXXXXX" + std::string(file_ext);
    std::string temp_spirv_file = temp_base + "XXXXXX.spv";
    
    // Create unique file names using mktemp-like approach
    // For simplicity, use timestamp + random number
    static unsigned int shader_counter = 0;
    temp_glsl_file = temp_base + std::to_string(shader_counter++) + std::string(file_ext);
    temp_spirv_file = temp_base + std::to_string(shader_counter) + ".spv";
    
    // Write GLSL source to temporary file
    {
        std::ofstream out(temp_glsl_file);
        if (!out) {
            error_message = "Failed to create temporary GLSL file";
            return empty_spirv;
        }
        out << glsl_code;
    }
    
    // Invoke glslangValidator
    std::string cmd = std::string("glslangValidator -V -o ") + temp_spirv_file + " " + temp_glsl_file + " 2>&1";
    int ret = system(cmd.c_str());
    
    // Check if glslangValidator succeeded
    if (ret != 0) {
        error_message = "glslangValidator compilation failed (return code: " + std::to_string(ret) + ")";
        // Try to read error details from a log file if available
        std::ifstream err_file(temp_glsl_file + ".err");
        if (err_file) {
            std::stringstream ss;
            ss << err_file.rdbuf();
            error_message += "\n" + ss.str();
        }
        std::remove(temp_glsl_file.c_str());
        return empty_spirv;
    }
    
    // Read SPIR-V binary
    std::ifstream spirv_file(temp_spirv_file, std::ios::binary);
    if (!spirv_file) {
        error_message = "Failed to read compiled SPIR-V file";
        std::remove(temp_glsl_file.c_str());
        return empty_spirv;
    }
    
    // Read SPIR-V as uint32_t array
    std::vector<uint32_t> spirv;
    uint32_t word;
    while (spirv_file.read(reinterpret_cast<char*>(&word), sizeof(uint32_t))) {
        spirv.push_back(word);
    }
    spirv_file.close();
    
    // Clean up temporary files
    std::remove(temp_glsl_file.c_str());
    std::remove(temp_spirv_file.c_str());
    
    if (spirv.empty()) {
        error_message = "SPIR-V output was empty";
        return empty_spirv;
    }
    
    return spirv;
}

ShaderCompiler::CompileResult ShaderCompiler::compileGLSLtoSPIRV(
    const std::string& glsl_code,
    ShaderStage stage,
    const std::string& entry_point) {
    
    CompileResult result;
    result.success = false;
    
    // Try to compile using glslangValidator external tool
    result.spirv = compile_with_glslangValidator(glsl_code, stage, result.error_message);
    
    if (result.spirv.empty()) {
        // Fallback error message
        if (result.error_message.empty()) {
            result.error_message = "GLSL to SPIR-V compilation failed";
        }
        return result;
    }
    
    result.success = true;
    return result;
}

ShaderCompiler::CompileResult ShaderCompiler::compileWithShaderc(
    const std::string& glsl_code,
    ShaderStage stage,
    const std::string& entry_point) {
    
    // Delegate to main compilation function
    return compileGLSLtoSPIRV(glsl_code, stage, entry_point);
}
