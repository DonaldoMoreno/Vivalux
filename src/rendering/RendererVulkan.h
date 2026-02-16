#pragma once

#include "Renderer.h"
#include <volk.h>
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
    
    // Staging buffer for texture uploads
    VkBuffer m_staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_staging_buffer_memory = VK_NULL_HANDLE;
    VkDeviceSize m_staging_buffer_size = 0;
    VkDeviceSize m_staging_buffer_used = 0;

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
        VkDescriptorSetLayout desc_set_layout = VK_NULL_HANDLE;
    };

    struct Vertex {
        glm::vec2 pos;
        glm::vec2 uv;
    };

    std::unordered_map<TextureHandle, TextureImpl> m_textures;
    std::unordered_map<ShaderHandle, ShaderImpl> m_shaders;
    TextureHandle m_next_texture_id = 1;
    ShaderHandle m_next_shader_id = 1;

    // Quad mesh (vertex and index buffers)
    VkBuffer m_quad_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_quad_vertex_buffer_memory = VK_NULL_HANDLE;
    VkBuffer m_quad_index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_quad_index_buffer_memory = VK_NULL_HANDLE;
    uint32_t m_quad_index_count = 0;

    // Descriptor pool for texture samplers
    VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;

    // Current state (track bound shader and texture for drawQuad)
    ShaderHandle m_current_shader = 0;
    TextureHandle m_current_texture = 0;
    VkDescriptorSet m_current_descriptor_set = VK_NULL_HANDLE;

    // Inicialización privada
    bool initializeVulkan();
    void cleanupVulkan();
    VkShaderModule createShaderModule(const std::vector<uint32_t>& spirv_code);
    
    // Texture and memory helpers
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                     VkImage& image, VkDeviceMemory& memory);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    bool createImageView(VkImage image, VkFormat format, VkImageView& view);
    bool createSampler(VkSampler& sampler);
    bool ensureStagingBuffer(VkDeviceSize size);

    // Buffer helpers
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                     VkMemoryPropertyFlags properties, VkBuffer& buffer,
                     VkDeviceMemory& memory);
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);
    
    // Pipeline helpers
    bool createQuadMesh();
    bool createDescriptorPool();
    bool createGraphicsPipeline(ShaderHandle handle, const ShaderImpl& shader);

    // Swapchain helpers
    bool createSwapchain();
    bool recreateSwapchain();
    void cleanupSwapchain();
};
