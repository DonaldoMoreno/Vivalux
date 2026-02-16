#include "RendererVulkan.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>
#include "VulkanUtils.h"

#include <cstring>

#ifdef __APPLE__
    #define VK_USE_PLATFORM_METAL_KHR
#endif

RendererVulkan::RendererVulkan() = default;

RendererVulkan::~RendererVulkan() {
    shutdown();
}

bool RendererVulkan::initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    
    if (!initializeVulkan()) {
        std::cerr << "Failed to initialize Vulkan" << std::endl;
        return false;
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

    std::cout << "Vulkan initialized (instance, device, queue, command pool, sync)" << std::endl;
    return true;
}

void RendererVulkan::shutdown() {
    cleanupVulkan();
}

void RendererVulkan::cleanupVulkan() {
    // TODO: Cleanup Vulkan resources
}

TextureHandle RendererVulkan::createTexture(const TextureSpec& spec) {
    // TODO: Crear VkImage + VkImageView + sampler
    TextureHandle handle = m_next_texture_id++;
    TextureImpl impl;
    impl.width = spec.width;
    impl.height = spec.height;
    m_textures[handle] = impl;
    return handle;
}

void RendererVulkan::updateTexture(TextureHandle handle, const uint8_t* data, uint32_t data_size) {
    // TODO: Copiar datos a VkImage con staging buffer
}

void RendererVulkan::deleteTexture(TextureHandle handle) {
    auto it = m_textures.find(handle);
    if (it != m_textures.end()) {
        // TODO: Destruir VkImage, VkImageView, sampler
        m_textures.erase(it);
    }
}

VkShaderModule RendererVulkan::createShaderModule(const std::vector<uint32_t>& spirv_code) {
    // TODO: Crear VkShaderModule desde código SPIR-V
    return VK_NULL_HANDLE;
}

ShaderHandle RendererVulkan::createShader(const std::string& vs_code, const std::string& fs_code) {
    // TODO: Compilar GLSL a SPIR-V y crear VkPipeline
    ShaderHandle handle = m_next_shader_id++;
    ShaderImpl impl;
    m_shaders[handle] = impl;
    return handle;
}

void RendererVulkan::deleteShader(ShaderHandle handle) {
    auto it = m_shaders.find(handle);
    if (it != m_shaders.end()) {
        // TODO: Destruir VkShaderModule, VkPipeline, VkPipelineLayout
        m_shaders.erase(it);
    }
}

void RendererVulkan::useShader(ShaderHandle handle) {
    // TODO: Bind VkPipeline
}

void RendererVulkan::setUniformFloat(const std::string& name, float value) {
    // TODO: Actualizar uniform buffer
}

void RendererVulkan::setUniformVec2(const std::string& name, const glm::vec2& value) {
    // TODO: Actualizar uniform buffer
}

void RendererVulkan::setUniformVec3(const std::string& name, const glm::vec3& value) {
    // TODO: Actualizar uniform buffer
}

void RendererVulkan::setUniformVec4(const std::string& name, const glm::vec4& value) {
    // TODO: Actualizar uniform buffer
}

void RendererVulkan::setUniformMat4(const std::string& name, const glm::mat4& value) {
    // TODO: Actualizar uniform buffer
}

void RendererVulkan::setUniformInt(const std::string& name, int value) {
    // TODO: Actualizar uniform buffer
}

void RendererVulkan::clear(float r, float g, float b, float a) {
    // TODO: Implementar clear command
}

void RendererVulkan::drawQuad(const Quad& quad, TextureHandle texture, float opacity, BlendMode blend) {
    // TODO: Grabar comandos de draw en command buffer
}

void RendererVulkan::present() {
    // TODO: Submitir command buffers y presentar
}

void RendererVulkan::resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    // TODO: Recrear swapchain
}

bool RendererVulkan::shouldClose() {
    return false;
}
