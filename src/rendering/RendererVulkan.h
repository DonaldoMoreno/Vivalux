#pragma once

#include "Renderer.h"
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class RendererVulkan : public Renderer {
public:
    RendererVulkan();
    ~RendererVulkan() override;

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
    
    // Vulkan instance & device
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    VkCommandPool m_cmd_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_cmd_buffer = VK_NULL_HANDLE;
    VkSemaphore m_image_available_semaphore = VK_NULL_HANDLE;
    VkSemaphore m_render_finished_semaphore = VK_NULL_HANDLE;
    VkFence m_fence = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchain_images;
    std::vector<VkImageView> m_swapchain_image_views;
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    struct TextureImpl {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        uint32_t width = 0, height = 0;
    };

    struct ShaderImpl {
        VkShaderModule vs = VK_NULL_HANDLE;
        VkShaderModule fs = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
    };

    std::unordered_map<TextureHandle, TextureImpl> m_textures;
    std::unordered_map<ShaderHandle, ShaderImpl> m_shaders;
    TextureHandle m_next_texture_id = 1;
    ShaderHandle m_next_shader_id = 1;

    // Inicialización privada
    bool initializeVulkan();
    void cleanupVulkan();
    VkShaderModule createShaderModule(const std::vector<uint32_t>& spirv_code);
};
