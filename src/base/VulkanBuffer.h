#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include "vulkan.h"

namespace LR {

    struct VulkanBuffer {
        // members
        VkDevice               device;
        /** @brief 用于在write descriptor中传递buffer信息，实际的descriptor声明在renderer里 */
        VkDescriptorBufferInfo descriptorInfo;
        VkBuffer               buffer = VK_NULL_HANDLE;
        VkDeviceMemory         memory = VK_NULL_HANDLE;
        VkDeviceSize           size   = 0;
        VkDeviceSize           alignment = 0;
        void*                  mapped = nullptr;

        // flags
        VkBufferUsageFlags    usageFlags;
        VkMemoryPropertyFlags memoryPropertyFlags;

        //funcs
        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void     unmap();
        VkResult bind(VkDeviceSize offset = 0);
        void     setUpDescriptorInfo(VkDeviceSize range = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void     copyFrom(void* data, VkDeviceSize size);
        
        VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        void     destroy();
    };

}// namespace LR

#endif