#ifndef VULKAN_DEVICE_H
#define VULKAN_DEVICE_H

#include "vulkan.h"
#include "VulkanBuffer.h"
#include "initializer.h"
#include "VulkanTools.h"
#include <iostream>
#include <vector>
#include <string>
#include <assert.h>
#include <stdexcept>

namespace LR {
    struct VulkanDevice {
        // members
        VkPhysicalDevice physicalDevice;
        VkDevice         logicalDevice;

        /** @brief Properties of the physical device(version, device info, ...), including limits that the application can check against */
        VkPhysicalDeviceProperties physicalProperties;
        /** @brief Features of the physical device that an application can use to check if a feature is supported */
        VkPhysicalDeviceFeatures physicalFeatures;
        VkPhysicalDeviceFeatures enabledFeatures;

        /** @brief Memory types and heaps of the physical device, used regularly for creating all kinds of buffers */
        VkPhysicalDeviceMemoryProperties memoryProperties;
        /** @brief Queue family properties of the physical device, used for setting up requested queues upon device creation */
        std::vector<VkQueueFamilyProperties> queueFamilyProperties;

        std::vector<std::string> supportedExtensions;

        /** @brief default commandPool for graphics queue family */
        VkCommandPool commandPool = VK_NULL_HANDLE;

        /** @brief indices to index queueFamilies */
        struct {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
        } queueFamilyIndices;

        // functions
        explicit VulkanDevice(VkPhysicalDevice physicalDevice);//disable implicit type transition
        ~VulkanDevice();

        VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures,
                                     std::vector<const char*> enabledExtensions,
                                     void*                    pNextChain,
                                     bool                     useSwapChain        = true,
                                     VkQueueFlags             requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

        VkResult createBuffer(VkBufferUsageFlags    usageFlags,
                              VkMemoryPropertyFlags memoryPropertyFlags,
                              VkDeviceSize          size,
                              VkBuffer*             buffer,
                              VkDeviceMemory*       memory,
                              void*                 data = nullptr);

        VkResult createBuffer(VkBufferUsageFlags    usageFlags,
                              VkMemoryPropertyFlags memoryPropertyFlags,
                              LR::Buffer*           buffer,
                              VkDeviceSize          size,
                              void*                 data = nullptr);

        void copyBuffer(LR::Buffer* src, LR::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion = nullptr);

        VkCommandPool   createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin = false);
        VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin = false);
        void            flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free = true);
        void            flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

        // tool funcs
        uint32_t getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound = nullptr) const;
        uint32_t getQueueFamilyIndex(VkQueueFlags queueFlags) const;
        VkFormat getSupportedDepthFormat(bool checkSamplingSupport) const;
        bool     extensionSupported(std::string extension) const;
    };
}// namespace LR

#endif