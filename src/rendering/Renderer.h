#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <memory>

// Abstracción de backend gráfico (OpenGL / Vulkan)

enum class PixelFormat {
    RGBA8,
    RGB8,
    BGRA8
};

enum class BlendMode {
    Alpha,
    Add,
    Multiply,
    Screen
};

struct TextureSpec {
    uint32_t width;
    uint32_t height;
    PixelFormat format;
    const uint8_t* data; // nullptr si no hay datos iniciales
};

struct Quad {
    glm::vec2 corners[4]; // TL, TR, BR, BL en coordenadas de pantalla normalizado [0, 1]
    float u_min = 0.0f, u_max = 1.0f;
    float v_min = 0.0f, v_max = 1.0f;
};

// Handle opaco para recursos gráficos
using TextureHandle = uint64_t;
using ShaderHandle = uint64_t;

constexpr TextureHandle INVALID_TEXTURE = 0;
constexpr ShaderHandle INVALID_SHADER = 0;

class Renderer {
public:
    virtual ~Renderer() = default;

    // Inicialización
    // If `nativeWindow` is a `GLFWwindow*` it will be used to create surfaces (Vulkan).
    virtual bool initialize(uint32_t width, uint32_t height, void* nativeWindow = nullptr) = 0;
    virtual void shutdown() = 0;

    // Texturas
    virtual TextureHandle createTexture(const TextureSpec& spec) = 0;
    virtual void updateTexture(TextureHandle handle, const uint8_t* data, uint32_t data_size) = 0;
    virtual void deleteTexture(TextureHandle handle) = 0;

    // Shaders
    virtual ShaderHandle createShader(const std::string& vs_code, const std::string& fs_code) = 0;
    virtual void deleteShader(ShaderHandle handle) = 0;
    virtual void useShader(ShaderHandle handle) = 0;

    // Uniforms
    virtual void setUniformFloat(const std::string& name, float value) = 0;
    virtual void setUniformVec2(const std::string& name, const glm::vec2& value) = 0;
    virtual void setUniformVec3(const std::string& name, const glm::vec3& value) = 0;
    virtual void setUniformVec4(const std::string& name, const glm::vec4& value) = 0;
    virtual void setUniformMat4(const std::string& name, const glm::mat4& value) = 0;
    virtual void setUniformInt(const std::string& name, int value) = 0;

    // Renderizado
    virtual void clear(float r, float g, float b, float a) = 0;
    virtual void drawQuad(const Quad& quad, TextureHandle texture, float opacity = 1.0f, BlendMode blend = BlendMode::Alpha) = 0;
    virtual void present() = 0;

    // Window
    virtual void resize(uint32_t width, uint32_t height) = 0;
    virtual bool shouldClose() = 0;
};

// Factory para crear el renderer apropiado según la plataforma
std::unique_ptr<Renderer> createPlatformRenderer();
