#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <algorithm>

namespace vk {

// RAII wrapper para VkInstance
class Instance {
public:
    Instance() = default;
    explicit Instance(VkInstance handle) : m_handle(handle) {}
    ~Instance() { destroy(); }
    
    Instance(const Instance&) = delete;
    Instance& operator=(const Instance&) = delete;
    
    Instance(Instance&& other) noexcept : m_handle(other.release()) {}
    Instance& operator=(Instance&& other) noexcept {
        destroy();
        m_handle = other.release();
        return *this;
    }
    
    explicit operator bool() const { return m_handle != VK_NULL_HANDLE; }
    VkInstance get() const { return m_handle; }
    VkInstance operator*() const { return m_handle; }
    
private:
    VkInstance m_handle = VK_NULL_HANDLE;
    
    VkInstance release() noexcept {
        VkInstance tmp = m_handle;
        m_handle = VK_NULL_HANDLE;
        return tmp;
    }
    
    void destroy() {
        if (m_handle) {
            vkDestroyInstance(m_handle, nullptr);
        }
    }
};

// RAII wrapper para VkDevice
class Device {
public:
    Device() = default;
    explicit Device(VkDevice handle) : m_handle(handle) {}
    ~Device() { destroy(); }
    
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    
    Device(Device&& other) noexcept : m_handle(other.release()) {}
    Device& operator=(Device&& other) noexcept {
        destroy();
        m_handle = other.release();
        return *this;
    }
    
    explicit operator bool() const { return m_handle != VK_NULL_HANDLE; }
    VkDevice get() const { return m_handle; }
    VkDevice operator*() const { return m_handle; }
    
private:
    VkDevice m_handle = VK_NULL_HANDLE;
    
    VkDevice release() noexcept {
        VkDevice tmp = m_handle;
        m_handle = VK_NULL_HANDLE;
        return tmp;
    }
    
    void destroy() {
        if (m_handle) {
            vkDestroyDevice(m_handle, nullptr);
        }
    }
};

// RAII wrapper para VkCommandPool
class CommandPool {
public:
    CommandPool() = default;
    CommandPool(VkDevice device, VkCommandPool handle) : m_device(device), m_handle(handle) {}
    ~CommandPool() { destroy(); }
    
    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;
    
    CommandPool(CommandPool&& other) noexcept 
        : m_device(other.m_device), m_handle(other.release()) {}
    CommandPool& operator=(CommandPool&& other) noexcept {
        destroy();
        m_device = other.m_device;
        m_handle = other.release();
        return *this;
    }
    
    explicit operator bool() const { return m_handle != VK_NULL_HANDLE; }
    VkCommandPool get() const { return m_handle; }
    VkCommandPool operator*() const { return m_handle; }
    
private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandPool m_handle = VK_NULL_HANDLE;
    
    VkCommandPool release() noexcept {
        VkCommandPool tmp = m_handle;
        m_handle = VK_NULL_HANDLE;
        return tmp;
    }
    
    void destroy() {
        if (m_handle && m_device) {
            vkDestroyCommandPool(m_device, m_handle, nullptr);
        }
    }
};

// Utilidad para encontrar queue family
struct QueueFamilyIndices {
    uint32_t graphics = UINT32_MAX;
    uint32_t present = UINT32_MAX;
    
    bool isComplete() const {
        return graphics != UINT32_MAX && present != UINT32_MAX;
    }
};

inline QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface = VK_NULL_HANDLE) {
    QueueFamilyIndices indices;
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }
        
        if (surface != VK_NULL_HANDLE) {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.present = i;
            }
        }
    }
    
    return indices;
}

} // namespace vk
