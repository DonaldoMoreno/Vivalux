#include "RendererVulkan.h"
#include "ShaderCompiler.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include "VulkanUtils.h"

#include <cstring>
#include <GLFW/glfw3.h>

#ifdef __APPLE__
    #define VK_USE_PLATFORM_METAL_KHR
#endif

RendererVulkan::RendererVulkan() = default;

RendererVulkan::~RendererVulkan() {
    shutdown();
}

bool RendererVulkan::initialize(uint32_t width, uint32_t height, void* nativeWindow) {
    m_width = width;
    m_height = height;
    
    if (!initializeVulkan()) {
        std::cerr << "Failed to initialize Vulkan" << std::endl;
        return false;
    }
    // If a native window (GLFWwindow*) is provided, create a surface and swapchain
    if (nativeWindow) {
        GLFWwindow* window = reinterpret_cast<GLFWwindow*>(nativeWindow);
        if (!window) return true; // nothing to do

        VkResult res = glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface);
        if (res != VK_SUCCESS) {
            std::cerr << "Failed to create window surface: " << res << std::endl;
            return false;
        }

        // Create swapchain with helper method
        if (!createSwapchain()) {
            std::cerr << "Failed to create swapchain" << std::endl;
            return false;
        }
    }

    return true;
}

bool RendererVulkan::initializeVulkan() {
    // Create Vulkan instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "VivaLux";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VivaLuxEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Enable common extensions if available
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef __APPLE__
    // On macOS MoltenVK will provide the metal surface extension
    extensions.push_back("VK_EXT_metal_surface");
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

    VkInstance instance = VK_NULL_HANDLE;
    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance: " << res << std::endl;
        return false;
    }

    m_instance = instance;

    // Load volk if available
#ifdef volkLoadInstance
    volkLoadInstance(m_instance);
#endif

    // Pick physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        std::cerr << "No Vulkan physical devices found" << std::endl;
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Choose first device with graphics queue
    for (auto dev : devices) {
        vk::QueueFamilyIndices qf = vk::findQueueFamilies(dev);
        if (qf.graphics != UINT32_MAX) {
            m_physical_device = dev;
            break;
        }
    }

    if (m_physical_device == VK_NULL_HANDLE) {
        std::cerr << "Failed to find suitable physical device" << std::endl;
        return false;
    }

    // Create logical device and graphics queue
    vk::QueueFamilyIndices indices = vk::findQueueFamilies(m_physical_device);
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphics;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    // Device extensions (swapchain later)
    std::vector<const char*> deviceExtensions;
    deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkDevice device = VK_NULL_HANDLE;
    res = vkCreateDevice(m_physical_device, &deviceCreateInfo, nullptr, &device);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create logical device: " << res << std::endl;
        return false;
    }

    m_device = device;

    vkGetDeviceQueue(m_device, indices.graphics, 0, &m_queue);

    // Create command pool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = indices.graphics;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkCommandPool cmdPool = VK_NULL_HANDLE;
    res = vkCreateCommandPool(m_device, &poolInfo, nullptr, &cmdPool);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create command pool: " << res << std::endl;
        return false;
    }
    m_cmd_pool = cmdPool;

    // Allocate a single command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_cmd_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    res = vkAllocateCommandBuffers(m_device, &allocInfo, &cmdBuf);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffer: " << res << std::endl;
        return false;
    }
    m_cmd_buffer = cmdBuf;

    // Create sync primitives
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(m_device, &semInfo, nullptr, &m_image_available_semaphore) != VK_SUCCESS ||
        vkCreateSemaphore(m_device, &semInfo, nullptr, &m_render_finished_semaphore) != VK_SUCCESS) {
        std::cerr << "Failed to create semaphores" << std::endl;
        return false;
    }

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(m_device, &fenceInfo, nullptr, &m_fence) != VK_SUCCESS) {
        std::cerr << "Failed to create fence" << std::endl;
        return false;
    }

    // Create render pass (once, for swapchain recreation)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;  // Default; will be updated on swapchain creation
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(m_device, &rpInfo, nullptr, &m_render_pass) != VK_SUCCESS) {
        std::cerr << "Failed to create render pass" << std::endl;
        return false;
    }

    // Create quad mesh for rendering textured quads
    if (!createQuadMesh()) {
        std::cerr << "Failed to create quad mesh" << std::endl;
        return false;
    }

    // Create descriptor pool for texture samplers
    if (!createDescriptorPool()) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }

    std::cout << "Vulkan initialized (instance, device, queue, command pool, sync)" << std::endl;
    return true;
}

void RendererVulkan::shutdown() {
    cleanupVulkan();
}

void RendererVulkan::cleanupVulkan() {
    if (m_device != VK_NULL_HANDLE) {
        // Cleanup shaders and pipelines
        for (auto& [handle, shader] : m_shaders) {
            if (shader.pipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, shader.pipeline, nullptr);
            if (shader.layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, shader.layout, nullptr);
            if (shader.desc_set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_device, shader.desc_set_layout, nullptr);
            if (shader.vs != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, shader.vs, nullptr);
            if (shader.fs != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, shader.fs, nullptr);
        }
        m_shaders.clear();

        // Cleanup quad mesh  
        if (m_quad_vertex_buffer) vkDestroyBuffer(m_device, m_quad_vertex_buffer, nullptr);
        if (m_quad_vertex_buffer_memory) vkFreeMemory(m_device, m_quad_vertex_buffer_memory, nullptr);
        if (m_quad_index_buffer) vkDestroyBuffer(m_device, m_quad_index_buffer, nullptr);
        if (m_quad_index_buffer_memory) vkFreeMemory(m_device, m_quad_index_buffer_memory, nullptr);

        // Cleanup descriptor pool
        if (m_descriptor_pool != VK_NULL_HANDLE) vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

        // Cleanup staging buffer
        if (m_staging_buffer) vkDestroyBuffer(m_device, m_staging_buffer, nullptr);
        if (m_staging_buffer_memory) vkFreeMemory(m_device, m_staging_buffer_memory, nullptr);
        
        // Cleanup textures
        for (auto& [handle, tex] : m_textures) {
            if (tex.sampler) vkDestroySampler(m_device, tex.sampler, nullptr);
            if (tex.view) vkDestroyImageView(m_device, tex.view, nullptr);
            if (tex.image) vkDestroyImage(m_device, tex.image, nullptr);
            if (tex.memory) vkFreeMemory(m_device, tex.memory, nullptr);
        }
        m_textures.clear();
        
        for (auto fb : m_framebuffers) {
            if (fb) vkDestroyFramebuffer(m_device, fb, nullptr);
        }
        m_framebuffers.clear();

        if (m_render_pass) vkDestroyRenderPass(m_device, m_render_pass, nullptr);

        for (auto iv : m_swapchain_image_views) {
            if (iv) vkDestroyImageView(m_device, iv, nullptr);
        }
        m_swapchain_image_views.clear();

        if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

        if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

        if (m_cmd_pool) vkDestroyCommandPool(m_device, m_cmd_pool, nullptr);

        if (m_fence) vkDestroyFence(m_device, m_fence, nullptr);
        if (m_image_available_semaphore) vkDestroySemaphore(m_device, m_image_available_semaphore, nullptr);
        if (m_render_finished_semaphore) vkDestroySemaphore(m_device, m_render_finished_semaphore, nullptr);

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_instance) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

TextureHandle RendererVulkan::createTexture(const TextureSpec& spec) {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    
    // Create VkImage with TRANSFER_DST for uploads
    if (!createImage(spec.width, spec.height, VK_FORMAT_R8G8B8A8_UNORM, 
                     VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     image, memory)) {
        std::cerr << "Failed to create texture image" << std::endl;
        return INVALID_TEXTURE;
    }
    
    // Transition to transfer destination
    transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, 
                         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    // Upload data if provided
    if (spec.data) {
        VkDeviceSize imageSize = spec.width * spec.height * 4; // RGBA
        if (!ensureStagingBuffer(imageSize)) {
            std::cerr << "Failed to create staging buffer" << std::endl;
            vkDestroyImage(m_device, image, nullptr);
            vkFreeMemory(m_device, memory, nullptr);
            return INVALID_TEXTURE;
        }
        
        void* data = nullptr;
        if (vkMapMemory(m_device, m_staging_buffer_memory, 0, imageSize, 0, &data) != VK_SUCCESS) {
            std::cerr << "Failed to map staging buffer" << std::endl;
            vkDestroyImage(m_device, image, nullptr);
            vkFreeMemory(m_device, memory, nullptr);
            return INVALID_TEXTURE;
        }
        
        memcpy(data, spec.data, imageSize);
        vkUnmapMemory(m_device, m_staging_buffer_memory);
        
        // Copy from staging to image
        copyBufferToImage(m_staging_buffer, image, spec.width, spec.height);
    }
    
    // Transition to shader read
    transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    // Create image view
    VkImageView imageView = VK_NULL_HANDLE;
    if (!createImageView(image, VK_FORMAT_R8G8B8A8_UNORM, imageView)) {
        std::cerr << "Failed to create image view" << std::endl;
        vkDestroyImage(m_device, image, nullptr);
        vkFreeMemory(m_device, memory, nullptr);
        return INVALID_TEXTURE;
    }
    
    // Create sampler
    VkSampler sampler = VK_NULL_HANDLE;
    if (!createSampler(sampler)) {
        std::cerr << "Failed to create sampler" << std::endl;
        vkDestroyImageView(m_device, imageView, nullptr);
        vkDestroyImage(m_device, image, nullptr);
        vkFreeMemory(m_device, memory, nullptr);
        return INVALID_TEXTURE;
    }
    
    TextureHandle handle = m_next_texture_id++;
    TextureImpl impl;
    impl.image = image;
    impl.memory = memory;
    impl.view = imageView;
    impl.sampler = sampler;
    impl.width = spec.width;
    impl.height = spec.height;
    
    m_textures[handle] = impl;
    return handle;
}

void RendererVulkan::updateTexture(TextureHandle handle, const uint8_t* data, uint32_t data_size) {
    auto it = m_textures.find(handle);
    if (it == m_textures.end()) return;
    
    TextureImpl& tex = it->second;
    VkDeviceSize imageSize = tex.width * tex.height * 4; // RGBA
    
    if (!ensureStagingBuffer(imageSize)) {
        std::cerr << "Failed to create staging buffer for texture update" << std::endl;
        return;
    }
    
    void* mappedData = nullptr;
    if (vkMapMemory(m_device, m_staging_buffer_memory, 0, imageSize, 0, &mappedData) != VK_SUCCESS) {
        std::cerr << "Failed to map staging buffer for texture update" << std::endl;
        return;
    }
    
    memcpy(mappedData, data, imageSize);
    vkUnmapMemory(m_device, m_staging_buffer_memory);
    
    // Transition image to transfer destination
    transitionImageLayout(tex.image, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    // Copy from staging to image
    copyBufferToImage(m_staging_buffer, tex.image, tex.width, tex.height);
    
    // Transition back to shader read
    transitionImageLayout(tex.image, VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void RendererVulkan::deleteTexture(TextureHandle handle) {
    auto it = m_textures.find(handle);
    if (it != m_textures.end()) {
        const TextureImpl& tex = it->second;
        if (tex.sampler) vkDestroySampler(m_device, tex.sampler, nullptr);
        if (tex.view) vkDestroyImageView(m_device, tex.view, nullptr);
        if (tex.image) vkDestroyImage(m_device, tex.image, nullptr);
        if (tex.memory) vkFreeMemory(m_device, tex.memory, nullptr);
        m_textures.erase(it);
    }
}

VkShaderModule RendererVulkan::createShaderModule(const std::vector<uint32_t>& spirv_code) {
    // TODO: Crear VkShaderModule desde código SPIR-V
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv_code.size() * sizeof(uint32_t);
    createInfo.pCode = spirv_code.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

ShaderHandle RendererVulkan::createShader(const std::string& vs_code, const std::string& fs_code) {
    // Compile GLSL to SPIR-V
    ShaderCompiler::CompileResult vs_result = ShaderCompiler::compileGLSLtoSPIRV(vs_code, ShaderCompiler::ShaderStage::Vertex);
    ShaderCompiler::CompileResult fs_result = ShaderCompiler::compileGLSLtoSPIRV(fs_code, ShaderCompiler::ShaderStage::Fragment);

    if (!vs_result.success || !fs_result.success) {
        std::cerr << "Vertex shader compilation failed: " << vs_result.error_message << std::endl;
        std::cerr << "Fragment shader compilation failed: " << fs_result.error_message << std::endl;
        return 0;  // Invalid handle
    }

    // Create shader modules
    VkShaderModule vs_module = createShaderModule(vs_result.spirv);
    VkShaderModule fs_module = createShaderModule(fs_result.spirv);

    if (vs_module == VK_NULL_HANDLE || fs_module == VK_NULL_HANDLE) {
        std::cerr << "Failed to create shader modules" << std::endl;
        if (vs_module != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, vs_module, nullptr);
        if (fs_module != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, fs_module, nullptr);
        return 0;
    }

    // Create the shader handle and impl
    ShaderHandle handle = m_next_shader_id++;
    ShaderImpl impl;
    impl.vs = vs_module;
    impl.fs = fs_module;
    m_shaders[handle] = impl;

    // Create graphics pipeline
    if (!createGraphicsPipeline(handle, m_shaders[handle])) {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        vkDestroyShaderModule(m_device, vs_module, nullptr);
        vkDestroyShaderModule(m_device, fs_module, nullptr);
        m_shaders.erase(handle);
        return 0;
    }

    return handle;
}

void RendererVulkan::deleteShader(ShaderHandle handle) {
    auto it = m_shaders.find(handle);
    if (it != m_shaders.end()) {
        ShaderImpl& impl = it->second;
        if (impl.pipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, impl.pipeline, nullptr);
        if (impl.layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, impl.layout, nullptr);
        if (impl.desc_set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(m_device, impl.desc_set_layout, nullptr);
        if (impl.vs != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, impl.vs, nullptr);
        if (impl.fs != VK_NULL_HANDLE) vkDestroyShaderModule(m_device, impl.fs, nullptr);
        m_shaders.erase(it);
    }
}

void RendererVulkan::useShader(ShaderHandle handle) {
    m_current_shader = handle;
}

void RendererVulkan::setUniformFloat(const std::string& name, float value) {
    // Push constants are handled in drawQuad
}

void RendererVulkan::setUniformVec2(const std::string& name, const glm::vec2& value) {
    // Push constants are handled in drawQuad
}

void RendererVulkan::setUniformVec3(const std::string& name, const glm::vec3& value) {
    // Push constants are handled in drawQuad
}

void RendererVulkan::setUniformVec4(const std::string& name, const glm::vec4& value) {
    // Push constants are handled in drawQuad
}

void RendererVulkan::setUniformMat4(const std::string& name, const glm::mat4& value) {
    // Push constants are handled in drawQuad
}

void RendererVulkan::setUniformInt(const std::string& name, int value) {
    // Push constants are handled in drawQuad
}

void RendererVulkan::clear(float r, float g, float b, float a) {
    // Clear is handled in the render pass; this is a no-op for Vulkan
}

void RendererVulkan::drawQuad(const Quad& quad, TextureHandle texture, float opacity, BlendMode blend) {
    if (m_current_shader == 0) {
        std::cerr << "No shader bound" << std::endl;
        return;
    }

    auto shader_it = m_shaders.find(m_current_shader);
    if (shader_it == m_shaders.end()) {
        std::cerr << "Shader not found" << std::endl;
        return;
    }

    auto tex_it = m_textures.find(texture);
    if (tex_it == m_textures.end()) {
        std::cerr << "Texture not found" << std::endl;
        return;
    }

    ShaderImpl& shader = shader_it->second;
    TextureImpl& tex = tex_it->second;

    if (shader.pipeline == VK_NULL_HANDLE) {
        std::cerr << "Pipeline not initialized" << std::endl;
        return;
    }

    // Create descriptor set for this texture if needed  
    if (m_current_texture != texture) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptor_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &shader.desc_set_layout;

        if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_current_descriptor_set) != VK_SUCCESS) {
            std::cerr << "Failed to allocate descriptor set" << std::endl;
            return;
        }

        // Update descriptor set to point to the texture
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = tex.view;
        imageInfo.sampler = tex.sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_current_descriptor_set;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);

        m_current_texture = texture;
    }

    // Prepare push constants
    struct PushConstants {
        glm::vec2 corners[4];
        glm::vec2 screen_size;
        float opacity;
        int blend_mode;
        float brightness;
        int _pad;
    } pc{};

    for (int i = 0; i < 4; ++i) {
        pc.corners[i] = {quad.corners[i].x, quad.corners[i].y};
    }
    pc.screen_size = {(float)m_width, (float)m_height};
    pc.opacity = opacity;
    pc.blend_mode = (int)blend;
    pc.brightness = 1.0f;

    // Bind pipeline and draw
    vkCmdBindPipeline(m_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.pipeline);
    vkCmdBindDescriptorSets(m_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shader.layout, 0, 1, &m_current_descriptor_set, 0, nullptr);

    vkCmdPushConstants(m_cmd_buffer, shader.layout, 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0, sizeof(PushConstants), &pc);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(m_cmd_buffer, 0, 1, &m_quad_vertex_buffer, &offset);
    vkCmdBindIndexBuffer(m_cmd_buffer, m_quad_index_buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(m_cmd_buffer, m_quad_index_count, 1, 0, 0, 0);
}

void RendererVulkan::present() {
    if (m_swapchain == VK_NULL_HANDLE) return;
    
    // Acquire next image
    uint32_t imageIndex = 0;
    VkResult res = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_image_available_semaphore, VK_NULL_HANDLE, &imageIndex);
    
    // Handle out of date swapchain
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;  // Skip this frame
    }
    
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) return;

    // Wait fence
    vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_fence);

    // Record a simple command buffer that begins the render pass and clears the attachment
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkResetCommandBuffer(m_cmd_buffer, 0);
    vkBeginCommandBuffer(m_cmd_buffer, &beginInfo);

    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = m_render_pass;
    rpBegin.framebuffer = m_framebuffers[imageIndex];
    rpBegin.renderArea.offset = {0,0};
    rpBegin.renderArea.extent = {m_width, m_height};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearColor;

    vkCmdBeginRenderPass(m_cmd_buffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
    // No draw calls yet (pipeline unbound)
    vkCmdEndRenderPass(m_cmd_buffer);

    vkEndCommandBuffer(m_cmd_buffer);

    VkSemaphore waitSemaphores[] = { m_image_available_semaphore };
    VkSemaphore signalSemaphores[] = { m_render_finished_semaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_cmd_buffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_queue, 1, &submitInfo, m_fence) != VK_SUCCESS) {
        std::cerr << "Failed to submit draw command buffer" << std::endl;
        return;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentRes = vkQueuePresentKHR(m_queue, &presentInfo);
    
    // Handle out of date or suboptimal swapchain after present
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    }
}

void RendererVulkan::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    
    // Trigger swapchain recreation on next present
    if (m_surface != VK_NULL_HANDLE && m_swapchain != VK_NULL_HANDLE) {
        recreateSwapchain();
    }
}

bool RendererVulkan::shouldClose() {
    return false;
}

// ============= Texture Helper Functions =============

uint32_t RendererVulkan::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    
    std::cerr << "Failed to find suitable memory type" << std::endl;
    return UINT32_MAX;
}

bool RendererVulkan::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                                VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                                VkImage& image, VkDeviceMemory& memory) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    
    if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        std::cerr << "Failed to create image" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device, image, &memReqs);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);
    
    if (allocInfo.memoryTypeIndex == UINT32_MAX) return false;
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate image memory" << std::endl;
        vkDestroyImage(m_device, image, nullptr);
        return false;
    }
    
    vkBindImageMemory(m_device, image, memory, 0);
    return true;
}

void RendererVulkan::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer cmdBuf = m_cmd_buffer;
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResetCommandBuffer(cmdBuf, 0);
    vkBeginCommandBuffer(cmdBuf, &beginInfo);
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    
    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    
    vkCmdPipelineBarrier(cmdBuf, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    vkEndCommandBuffer(cmdBuf);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    
    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);
}

void RendererVulkan::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer cmdBuf = m_cmd_buffer;
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResetCommandBuffer(cmdBuf, 0);
    vkBeginCommandBuffer(cmdBuf, &beginInfo);
    
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyBufferToImage(cmdBuf, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    vkEndCommandBuffer(cmdBuf);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuf;
    
    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);
}

bool RendererVulkan::createImageView(VkImage image, VkFormat format, VkImageView& view) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        std::cerr << "Failed to create image view" << std::endl;
        return false;
    }
    return true;
}

bool RendererVulkan::createSampler(VkSampler& sampler) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    if (vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
        std::cerr << "Failed to create sampler" << std::endl;
        return false;
    }
    return true;
}

bool RendererVulkan::ensureStagingBuffer(VkDeviceSize size) {
    if (m_staging_buffer != VK_NULL_HANDLE && m_staging_buffer_size >= size) {
        return true;
    }
    
    // Destroy old buffer
    if (m_staging_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(m_device, m_staging_buffer, nullptr);
        vkFreeMemory(m_device, m_staging_buffer_memory, nullptr);
    }
    
    // Create new buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_staging_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create staging buffer" << std::endl;
        return false;
    }
    
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, m_staging_buffer, &memReqs);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, 
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (allocInfo.memoryTypeIndex == UINT32_MAX) return false;
    
    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &m_staging_buffer_memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate staging buffer memory" << std::endl;
        vkDestroyBuffer(m_device, m_staging_buffer, nullptr);
        return false;
    }
    
    vkBindBufferMemory(m_device, m_staging_buffer, m_staging_buffer_memory, 0);
    m_staging_buffer_size = size;
    m_staging_buffer_used = 0;
    return true;
}

// Create a generic buffer (vertex, index, staging, uniform, etc.)
bool RendererVulkan::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                                   VkMemoryPropertyFlags properties, VkBuffer& buffer,
                                   VkDeviceMemory& memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer" << std::endl;
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, properties);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        std::cerr << "Failed to find suitable memory type for buffer" << std::endl;
        vkDestroyBuffer(m_device, buffer, nullptr);
        return false;
    }

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate buffer memory" << std::endl;
        vkDestroyBuffer(m_device, buffer, nullptr);
        return false;
    }

    vkBindBufferMemory(m_device, buffer, memory, 0);
    return true;
}

// Copy data from one buffer to another using a command buffer
void RendererVulkan::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
    // Create a temporary command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_cmd_pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(cmdBuffer, src, dst, 1, &copyRegion);

    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    vkFreeCommandBuffers(m_device, m_cmd_pool, 1, &cmdBuffer);
}

// Create quad mesh (unit quad [0,1]x[0,1])
bool RendererVulkan::createQuadMesh() {
    // Define quad vertices: position (x, y) and UV (u, v)
    // Layout: 0=TL, 1=TR, 2=BR, 3=BL
    const Vertex vertices[] = {
        {{0.0f, 0.0f}, {0.0f, 0.0f}},  // TL
        {{1.0f, 0.0f}, {1.0f, 0.0f}},  // TR
        {{1.0f, 1.0f}, {1.0f, 1.0f}},  // BR
        {{0.0f, 1.0f}, {0.0f, 1.0f}},  // BL
    };

    const uint32_t indices[] = {
        0, 1, 2,  // First triangle
        0, 2, 3   // Second triangle
    };

    m_quad_index_count = 6;

    // Create staging buffers for vertex and index data
    VkBuffer stagingVertexBuffer, stagingIndexBuffer;
    VkDeviceMemory stagingVertexMemory, stagingIndexMemory;

    // Upload vertex buffer
    VkDeviceSize vertexSize = sizeof(vertices);
    if (!createBuffer(vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingVertexBuffer, stagingVertexMemory)) {
        std::cerr << "Failed to create staging vertex buffer" << std::endl;
        return false;
    }

    void* data;
    vkMapMemory(m_device, stagingVertexMemory, 0, vertexSize, 0, &data);
    std::memcpy(data, vertices, vertexSize);
    vkUnmapMemory(m_device, stagingVertexMemory);

    // Create device-local vertex buffer
    if (!createBuffer(vertexSize, 
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_quad_vertex_buffer, m_quad_vertex_buffer_memory)) {
        std::cerr << "Failed to create device vertex buffer" << std::endl;
        vkDestroyBuffer(m_device, stagingVertexBuffer, nullptr);
        vkFreeMemory(m_device, stagingVertexMemory, nullptr);
        return false;
    }

    copyBuffer(stagingVertexBuffer, m_quad_vertex_buffer, vertexSize);

    // Clean up staging vertex buffer
    vkDestroyBuffer(m_device, stagingVertexBuffer, nullptr);
    vkFreeMemory(m_device, stagingVertexMemory, nullptr);

    // Upload index buffer
    VkDeviceSize indexSize = sizeof(indices);
    if (!createBuffer(indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingIndexBuffer, stagingIndexMemory)) {
        std::cerr << "Failed to create staging index buffer" << std::endl;
        vkDestroyBuffer(m_device, m_quad_vertex_buffer, nullptr);
        vkFreeMemory(m_device, m_quad_vertex_buffer_memory, nullptr);
        return false;
    }

    vkMapMemory(m_device, stagingIndexMemory, 0, indexSize, 0, &data);
    std::memcpy(data, indices, indexSize);
    vkUnmapMemory(m_device, stagingIndexMemory);

    // Create device-local index buffer
    if (!createBuffer(indexSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_quad_index_buffer, m_quad_index_buffer_memory)) {
        std::cerr << "Failed to create device index buffer" << std::endl;
        vkDestroyBuffer(m_device, stagingIndexBuffer, nullptr);
        vkFreeMemory(m_device, stagingIndexMemory, nullptr);
        vkDestroyBuffer(m_device, m_quad_vertex_buffer, nullptr);
        vkFreeMemory(m_device, m_quad_vertex_buffer_memory, nullptr);
        return false;
    }

    copyBuffer(stagingIndexBuffer, m_quad_index_buffer, indexSize);

    // Clean up staging index buffer
    vkDestroyBuffer(m_device, stagingIndexBuffer, nullptr);
    vkFreeMemory(m_device, stagingIndexMemory, nullptr);

    return true;
}

// Create descriptor pool for texture samplers
bool RendererVulkan::createDescriptorPool() {
    // Pool can hold multiple descriptor sets (texture samplers)
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1024;  // Allow up to 1024 textures

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1024;

    if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptor_pool) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor pool" << std::endl;
        return false;
    }

    return true;
}

// Create graphics pipeline from shader modules
bool RendererVulkan::createGraphicsPipeline(ShaderHandle handle, const ShaderImpl& shader) {
    if (shader.vs == VK_NULL_HANDLE || shader.fs == VK_NULL_HANDLE) {
        std::cerr << "Shader modules not initialized" << std::endl;
        return false;
    }

    // Vertex input state (position and UV)
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attrDesc[2]{};
    // Position
    attrDesc[0].binding = 0;
    attrDesc[0].location = 0;
    attrDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attrDesc[0].offset = offsetof(Vertex, pos);
    // UV
    attrDesc[1].binding = 0;
    attrDesc[1].location = 1;
    attrDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
    attrDesc[1].offset = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    vertexInputInfo.vertexAttributeDescriptionCount = 2;
    vertexInputInfo.pVertexAttributeDescriptions = attrDesc;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)m_width;
    viewport.height = (float)m_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {m_width, m_height};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Blending (alpha blend)
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &blendAttachment;

    // Push constants for per-draw data
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 4 + sizeof(float) * 2 + sizeof(float) * 3;  // corners[4] + screen_size + opacity/brightness...

    // Pipeline layout with descriptor set and push constants
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
    descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutInfo.bindingCount = 1;
    descriptorSetLayoutInfo.pBindings = &samplerLayoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create descriptor set layout" << std::endl;
        return false;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout" << std::endl;
        vkDestroyDescriptorSetLayout(m_device, descriptorSetLayout, nullptr);
        return false;
    }

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = shader.vs;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = shader.fs;
    shaderStages[1].pName = "main";

    // Create graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = m_render_pass;
    pipelineInfo.subpass = 0;

    VkPipeline graphicsPipeline;
    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create graphics pipeline" << std::endl;
        vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
        vkDestroyDescriptorSetLayout(m_device, descriptorSetLayout, nullptr);
        return false;
    }

    // Store the pipeline and layout in the shader impl
    // Need to const_cast to modify the mutable reference passed in
    ShaderImpl& mutable_shader = const_cast<ShaderImpl&>(shader);
    mutable_shader.layout = pipelineLayout;
    mutable_shader.pipeline = graphicsPipeline;
    mutable_shader.desc_set_layout = descriptorSetLayout;

    return true;
}

// ============= Swapchain Helpers =============

bool RendererVulkan::createSwapchain() {
    if (m_surface == VK_NULL_HANDLE) {
        return false;  // No surface available
    }

    // Query surface capabilities and formats
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &formatCount, nullptr);
    if (formatCount == 0) {
        std::cerr << "No surface formats available" << std::endl;
        return false;
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &formatCount, formats.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed
    for (const auto& pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = pm;
            break;
        }
        if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            presentMode = pm;
        }
    }

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = m_width;
        extent.height = m_height;
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = m_surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    vk::QueueFamilyIndices indices = vk::findQueueFamilies(m_physical_device, m_surface);
    uint32_t queueFamilyIndices[] = {indices.graphics, indices.present};
    if (indices.graphics != indices.present) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 0;
        swapchainInfo.pQueueFamilyIndices = nullptr;
    }

    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = m_swapchain;  // Chain with old swapchain if recreating

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VkResult res = vkCreateSwapchainKHR(m_device, &swapchainInfo, nullptr, &newSwapchain);
    if (res != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain: " << res << std::endl;
        return false;
    }

    // If we had an old swapchain, destroy it now that new one is created
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    }
    m_swapchain = newSwapchain;

    // Retrieve swapchain images
    uint32_t scImageCount = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &scImageCount, nullptr);
    m_swapchain_images.resize(scImageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &scImageCount, m_swapchain_images.data());

    // Create image views
    m_swapchain_image_views.resize(scImageCount);
    for (uint32_t i = 0; i < scImageCount; ++i) {
        VkImageViewCreateInfo ivInfo{};
        ivInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivInfo.image = m_swapchain_images[i];
        ivInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivInfo.format = surfaceFormat.format;
        ivInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ivInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivInfo.subresourceRange.baseMipLevel = 0;
        ivInfo.subresourceRange.levelCount = 1;
        ivInfo.subresourceRange.baseArrayLayer = 0;
        ivInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device, &ivInfo, nullptr, &m_swapchain_image_views[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create image view" << std::endl;
            return false;
        }
    }

    // Recreate framebuffers
    m_framebuffers.resize(scImageCount);
    for (uint32_t i = 0; i < scImageCount; ++i) {
        VkImageView attachments[1] = {m_swapchain_image_views[i]};
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_render_pass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create framebuffer" << std::endl;
            return false;
        }
    }

    std::cout << "Swapchain created with extent " << extent.width << "x" << extent.height << std::endl;
    return true;
}

bool RendererVulkan::recreateSwapchain() {
    // Wait for device idle before recreating
    vkDeviceWaitIdle(m_device);

    // Clean up old swapchain resources
    cleanupSwapchain();

    // Create new swapchain
    if (!createSwapchain()) {
        std::cerr << "Failed to recreate swapchain" << std::endl;
        return false;
    }

    return true;
}

void RendererVulkan::cleanupSwapchain() {
    // Destroy framebuffers
    for (auto fb : m_framebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(m_device, fb, nullptr);
        }
    }
    m_framebuffers.clear();

    // Destroy image views
    for (auto iv : m_swapchain_image_views) {
        if (iv != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device, iv, nullptr);
        }
    }
    m_swapchain_image_views.clear();
    m_swapchain_images.clear();

    // Note: Don't destroy m_swapchain here - it's destroyed in createSwapchain or cleanupVulkan
}
