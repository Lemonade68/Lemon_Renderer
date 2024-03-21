#ifndef VULKAN_TEXTURE_H
#define VULKAN_TEXTURE_H

// mostly from Vulkan S.W.

#include "VulkanTools.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "initializer.h"
#include "vulkan.h"
#include "ktx.h"
#include "ktxvulkan.h"

#include <string>
#include <vector>

namespace LR {
    class Texture {
    public:
        LR::VulkanDevice*     device;
        VkImage               image;
        VkImageLayout         imageLayout;
        VkImageView           view;
        VkSampler             sampler;
        VkDeviceMemory        memory;
        VkDescriptorImageInfo descriptorInfo;

        uint32_t width, height;
        uint32_t mipLevels;
        uint32_t layerCount;

    public:
        ktxResult loadKTXFile(std::string filename, ktxTexture** target);
        void      updateDescriptor();
        void      destroy();
    };

    class Texture2D : public Texture {
    public:
        void loadFromFile(
            std::string       filename,
            VkFormat          format,
            LR::VulkanDevice* device,
            VkQueue           copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout     imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            bool              forceLinear     = false);
        void loadFromBuffer(
            void*             buffer,
            VkDeviceSize      bufferSize,
            VkFormat          format,
            uint32_t          texWidth,
            uint32_t          texHeight,
            LR::VulkanDevice* device,
            VkQueue           copyQueue,
            VkFilter          filter          = VK_FILTER_LINEAR,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout     imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    };

    class Texture2DArray : public Texture {
    public:
        void loadFromFile(
            std::string       filename,
            VkFormat          format,
            LR::VulkanDevice* device,
            VkQueue           copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout     imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    };

    class TextureCubeMap : public Texture {
    public:
        void loadFromFile(
            std::string       filename,
            VkFormat          format,
            LR::VulkanDevice* device,
            VkQueue           copyQueue,
            VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout     imageLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    };

}// namespace LR

#endif