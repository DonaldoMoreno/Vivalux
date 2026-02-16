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
                surfaceFormat = f; break;
            }
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &presentModeCount, presentModes.data());

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // guaranteed
        for (const auto& pm : presentModes) {
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = pm; break; }
            if (pm == VK_PRESENT_MODE_IMMEDIATE_KHR) { presentMode = pm; }
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
        uint32_t queueFamilyIndices[] = { indices.graphics, indices.present };
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
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        res = vkCreateSwapchainKHR(m_device, &swapchainInfo, nullptr, &swapchain);
        if (res != VK_SUCCESS) {
            std::cerr << "Failed to create swapchain: " << res << std::endl;
            return false;
        }
        m_swapchain = swapchain;

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

            VkImageView iv = VK_NULL_HANDLE;
            if (vkCreateImageView(m_device, &ivInfo, nullptr, &iv) != VK_SUCCESS) {
                std::cerr << "Failed to create image view" << std::endl;
                return false;
            }
            m_swapchain_image_views[i] = iv;
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
