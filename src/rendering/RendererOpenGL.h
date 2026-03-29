#pragma once

#include "Renderer.h"
#include <unordered_map>
#include <glm/glm.hpp>

class RendererOpenGL : public Renderer {
public:
    RendererOpenGL();
    ~RendererOpenGL() override;

    bool initialize(uint32_t width, uint32_t height, void* nativeWindow = nullptr) override;
    void shutdown() override;

    TextureHandle createTexture(const TextureSpec& spec) override;
    void updateTexture(TextureHandle handle, const uint8_t* data, uint32_t data_size) override;
    void deleteTexture(TextureHandle handle) override;

    ShaderHandle createShader(const std::string& vs_code, const std::string& fs_code) override;
    void deleteShader(ShaderHandle handle) override;
    void useShader(ShaderHandle handle) override;

    void setUniformFloat(const std::string& name, float value) override;
    void setUniformVec2(const std::string& name, const glm::vec2& value) override;
    void setUniformVec3(const std::string& name, const glm::vec3& value) override;
    void setUniformVec4(const std::string& name, const glm::vec4& value) override;
    void setUniformMat4(const std::string& name, const glm::mat4& value) override;
    void setUniformInt(const std::string& name, int value) override;

    void clear(float r, float g, float b, float a) override;
    void drawQuad(const Quad& quad, TextureHandle texture, float opacity = 1.0f, BlendMode blend = BlendMode::Alpha) override;
    void present() override;

    void resize(uint32_t width, uint32_t height) override;
    bool shouldClose() override;

private:
    uint32_t m_width = 0, m_height = 0;
    uint32_t m_current_shader = 0;
    uint32_t m_quad_vao = 0, m_quad_vbo = 0;

    struct TextureImpl {
        uint32_t gl_handle;
        uint32_t width, height;
    };

    struct ShaderImpl {
        uint32_t program;
    };

    std::unordered_map<TextureHandle, TextureImpl> m_textures;
    std::unordered_map<ShaderHandle, ShaderImpl> m_shaders;
    TextureHandle m_next_texture_id = 1;
    ShaderHandle m_next_shader_id = 1;

    uint32_t compileShader(const std::string& code, uint32_t type);
    void setupQuadGeometry();
};
