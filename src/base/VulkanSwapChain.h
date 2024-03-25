#ifndef VULKAN_SWAPCHAIN_H
#define VULKAN_SWAPCHAIN_H

#include <stdlib.h>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <algorithm>

#include <vulkan/vulkan.h>
#include "VulkanTools.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>//automatically include vulkan.h

namespace LR {
    typedef struct _SwapChainBuffers {
        VkImage     image;
        VkImageView view;
    } SwapChainBuffer;

    class VulkanSwapChain {
    private:
        VkInstance       instance;
        VkDevice         device;
        VkPhysicalDevice physicalDevice;
        VkSurfaceKHR     surface;

    public:
        VkSwapchainKHR               swapChain = VK_NULL_HANDLE;	// 初始赋为NULL_HANDLE，使得第一次中oldSwapChain那边不触发
        VkExtent2D                   extent;
        VkFormat                     colorFormat;
        VkColorSpaceKHR              colorSpace;
        uint32_t                     imageCount;
        std::vector<VkImage>         images;// 获取的swapChain中的image（不是自己创建的，因此不需要自己销毁）
        std::vector<SwapChainBuffer> buffers;
        uint32_t                     queueNodeIndex = UINT32_MAX;// 用于swapchain进行present和graphics的queue

        void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);// 先connect
        void initSurfaceAndQueue(VkSurfaceKHR surface, GLFWwindow* window);                                       // 再Init surface

        void     create(uint32_t* width, uint32_t* height, bool vsync = false, bool fullscreen = false);
        VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
        VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
        void     cleanup();
    };
}// namespace LR

#endif