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
        VkSwapchainKHR               swapChain = VK_NULL_HANDLE;	// ��ʼ��ΪNULL_HANDLE��ʹ�õ�һ����oldSwapChain�Ǳ߲�����
        VkExtent2D                   extent;
        VkFormat                     colorFormat;
        VkColorSpaceKHR              colorSpace;
        uint32_t                     imageCount;
        std::vector<VkImage>         images;// ��ȡ��swapChain�е�image�������Լ������ģ���˲���Ҫ�Լ����٣�
        std::vector<SwapChainBuffer> buffers;
        uint32_t                     queueNodeIndex = UINT32_MAX;// ����swapchain����present��graphics��queue

        void connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);// ��connect
        void initSurfaceAndQueue(VkSurfaceKHR surface, GLFWwindow* window);                                       // ��Init surface

        void     create(uint32_t* width, uint32_t* height, bool vsync = false, bool fullscreen = false);
        VkResult acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex);
        VkResult queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);
        void     cleanup();
    };
}// namespace LR

#endif