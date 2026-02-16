#include "RendererOpenGL.h"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

RendererOpenGL::RendererOpenGL() = default;

RendererOpenGL::~RendererOpenGL() {
    shutdown();
}

bool RendererOpenGL::initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    if (!gladLoadGL()) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glViewport(0, 0, width, height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    setupQuadGeometry();
    return true;
}

void RendererOpenGL::shutdown() {
    if (m_quad_vao) {
        glDeleteVertexArrays(1, &m_quad_vao);
    }
    if (m_quad_vbo) {
        glDeleteBuffers(1, &m_quad_vbo);
    }

    for (auto& [handle, tex] : m_textures) {
        glDeleteTextures(1, &tex.gl_handle);
    }
    m_textures.clear();

    for (auto& [handle, shader] : m_shaders) {
        glDeleteProgram(shader.program);
    }
    m_shaders.clear();
}

void RendererOpenGL::setupQuadGeometry() {
    float quad_vertices[] = {
        // pos          uv
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f, -1.0f,  0.0f, 0.0f
    };

    glGenVertexArrays(1, &m_quad_vao);
    glGenBuffers(1, &m_quad_vbo);

    glBindVertexArray(m_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

TextureHandle RendererOpenGL::createTexture(const TextureSpec& spec) {
    GLuint gl_texture;
    glGenTextures(1, &gl_texture);
    glBindTexture(GL_TEXTURE_2D, gl_texture);

    GLenum gl_format = (spec.format == PixelFormat::RGBA8) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, gl_format, spec.width, spec.height, 0, gl_format, GL_UNSIGNED_BYTE, spec.data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);

    TextureHandle handle = m_next_texture_id++;
    m_textures[handle] = {gl_texture, spec.width, spec.height};
    return handle;
}

void RendererOpenGL::updateTexture(TextureHandle handle, const uint8_t* data, uint32_t data_size) {
    auto it = m_textures.find(handle);
    if (it == m_textures.end()) return;

    glBindTexture(GL_TEXTURE_2D, it->second.gl_handle);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, it->second.width, it->second.height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RendererOpenGL::deleteTexture(TextureHandle handle) {
    auto it = m_textures.find(handle);
    if (it != m_textures.end()) {
        glDeleteTextures(1, &it->second.gl_handle);
        m_textures.erase(it);
    }
}

uint32_t RendererOpenGL::compileShader(const std::string& code, uint32_t type) {
    GLuint shader = glCreateShader(type);
    const char* src = code.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation failed: " << infoLog << std::endl;
    }
    return shader;
}

ShaderHandle RendererOpenGL::createShader(const std::string& vs_code, const std::string& fs_code) {
    GLuint vs = compileShader(vs_code, GL_VERTEX_SHADER);
    GLuint fs = compileShader(fs_code, GL_FRAGMENT_SHADER);

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "Program linking failed: " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    ShaderHandle handle = m_next_shader_id++;
    m_shaders[handle] = {program};
    return handle;
}

void RendererOpenGL::deleteShader(ShaderHandle handle) {
    auto it = m_shaders.find(handle);
    if (it != m_shaders.end()) {
        glDeleteProgram(it->second.program);
        m_shaders.erase(it);
    }
}

void RendererOpenGL::useShader(ShaderHandle handle) {
    auto it = m_shaders.find(handle);
    if (it != m_shaders.end()) {
        glUseProgram(it->second.program);
        m_current_shader = it->second.program;
    }
}

void RendererOpenGL::setUniformFloat(const std::string& name, float value) {
    GLint loc = glGetUniformLocation(m_current_shader, name.c_str());
    if (loc != -1) glUniform1f(loc, value);
}

void RendererOpenGL::setUniformVec2(const std::string& name, const glm::vec2& value) {
    GLint loc = glGetUniformLocation(m_current_shader, name.c_str());
    if (loc != -1) glUniform2fv(loc, 1, glm::value_ptr(value));
}

void RendererOpenGL::setUniformVec3(const std::string& name, const glm::vec3& value) {
    GLint loc = glGetUniformLocation(m_current_shader, name.c_str());
    if (loc != -1) glUniform3fv(loc, 1, glm::value_ptr(value));
}

void RendererOpenGL::setUniformVec4(const std::string& name, const glm::vec4& value) {
    GLint loc = glGetUniformLocation(m_current_shader, name.c_str());
    if (loc != -1) glUniform4fv(loc, 1, glm::value_ptr(value));
}

void RendererOpenGL::setUniformMat4(const std::string& name, const glm::mat4& value) {
    GLint loc = glGetUniformLocation(m_current_shader, name.c_str());
    if (loc != -1) glUniformMatrix4fv(loc, 1, GL_FALSE, glm::value_ptr(value));
}

void RendererOpenGL::setUniformInt(const std::string& name, int value) {
    GLint loc = glGetUniformLocation(m_current_shader, name.c_str());
    if (loc != -1) glUniform1i(loc, value);
}

void RendererOpenGL::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RendererOpenGL::drawQuad(const Quad& quad, TextureHandle texture, float opacity, BlendMode blend) {
    auto tex_it = m_textures.find(texture);
    if (tex_it == m_textures.end()) return;

    // Set blend mode
    switch (blend) {
        case BlendMode::Alpha:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            break;
        case BlendMode::Add:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
        case BlendMode::Multiply:
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            break;
        case BlendMode::Screen:
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            break;
    }

    glBindTexture(GL_TEXTURE_2D, tex_it->second.gl_handle);
    setUniformFloat("opacity", opacity);

    // Compute perspective transform matrix from quad corners
    glm::mat4 transform = glm::identity<glm::mat4>();
    // TODO: implement quad corner to matrix transformation

    setUniformMat4("transform", transform);

    glBindVertexArray(m_quad_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void RendererOpenGL::present() {
    // SwapBuffers is handled by GLFW in the main loop
}

void RendererOpenGL::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    glViewport(0, 0, width, height);
}

bool RendererOpenGL::shouldClose() {
    return false; // GLFW window close check is done in main loop
}
