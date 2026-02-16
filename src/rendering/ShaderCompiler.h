#pragma once

#include <vector>
#include <string>
#include <cstdint>

// Compilador GLSL a SPIR-V
class ShaderCompiler {
public:
    enum class ShaderStage {
        Vertex,
        Fragment,
        Compute
    };

    struct CompileResult {
        bool success = false;
        std::vector<uint32_t> spirv;
        std::string error_message;
    };

    // Compilar código GLSL a SPIR-V
    // Nota: requiere que glslang esté instalado o usar runtime compilation
    static CompileResult compileGLSLtoSPIRV(
        const std::string& glsl_code,
        ShaderStage stage,
        const std::string& entry_point = "main"
    );

    // Versión inline con shaderc (si está disponible en CMake)
    static CompileResult compileWithShaderc(
        const std::string& glsl_code,
        ShaderStage stage,
        const std::string& entry_point = "main"
    );
};
