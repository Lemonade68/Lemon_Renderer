#include "VulkanSwapChain.h"

/** @brief Creates the platform specific surface abstraction of glfw window used for presentation */
void VulkanSwapChain::initSurfaceAndQueue(GLFWwindow* window) {
    // Create the glfw surface
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");

    // 1. init queueIndex
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, NULL);
    assert(queueCount >= 1);
    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProps.data());

    // Iterate over each queue to learn whether it supports presenting:
    // Find a queue with present support
    // Will be used to present the swap chain images to the windowing system
    std::vector<VkBool32> supportsPresent(queueCount);
    for (uint32_t i = 0; i < queueCount; i++) {
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex  = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; i++) {
        if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) {
            if (graphicsQueueNodeIndex == UINT32_MAX) {
                graphicsQueueNodeIndex = i;
            }

            if (supportsPresent[i] == VK_TRUE) {
                graphicsQueueNodeIndex = i;
                presentQueueNodeIndex  = i;
                break;
            }
        }
    }
    if (presentQueueNodeIndex == UINT32_MAX) {
        // If there's no queue that supports both present and graphics
        // try to find a separate present queue
        for (uint32_t i = 0; i < queueCount; ++i) {
            if (supportsPresent[i] == VK_TRUE) {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }

    // Exit if either a graphics or a presenting queue hasn't been found
    if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX) {
        throw std::runtime_error("Could not find a graphics and/or presenting queue!");
    }
    // 暂时先找到一个支持两个操作的queueIndex
    if (graphicsQueueNodeIndex != presentQueueNodeIndex) {
        throw std::runtime_error("Separate graphics and presenting queues are not supported yet!");
    }

    queueNodeIndex = graphicsQueueNodeIndex;

    // 2. init format
    uint32_t formatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, NULL));
    assert(formatCount > 0);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats.data()));

    // We want to get a format that best suits our needs, so we try to get one from a set of preferred formats
    // Initialize the format to the first one returned by the implementation in case we can't find one of the preffered formats
    VkSurfaceFormatKHR    selectedFormat        = surfaceFormats[0];
    std::vector<VkFormat> preferredImageFormats = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_A8B8G8R8_UNORM_PACK32};

    for (auto& availableFormat : surfaceFormats) {
        if (std::find(preferredImageFormats.begin(), preferredImageFormats.end(), availableFormat.format) != preferredImageFormats.end()) {
            selectedFormat = availableFormat;
            break;
        }
    }

    colorFormat = selectedFormat.format;
    colorSpace  = selectedFormat.colorSpace;
}

/**
* Set instance, physical and logical device to use for the swapchain and get all required function pointers
* 
* @param instance Vulkan instance to use
* @param physicalDevice Physical device used to query properties and formats relevant to the swapchain
* @param device Logical representation of the device to create the swapchain for
*
*/
void VulkanSwapChain::connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device) {
    this->instance       = instance;
    this->physicalDevice = physicalDevice;
    this->device         = device;
}

/** 
* Create the swapchain and get its images with given width and height
* 
* @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
* @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
* @param vsync (Optional) Can be used to force vsync-ed rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
*/
void VulkanSwapChain::create(uint32_t* width, uint32_t* height, bool vsync, bool fullscreen) {
    // Store the current swap chain handle so we can use it later on to ease up recreation
    VkSwapchainKHR oldSwapchain = swapChain;

    // Get physical device surface properties and formats
    VkSurfaceCapabilitiesKHR surfCaps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps));

    // set extent
    VkExtent2D swapchainExtent = {};
    // If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
    if (surfCaps.currentExtent.width == (uint32_t)-1) {
        // If the surface size is undefined, the size is set to
        // the size of the images requested.
        swapchainExtent.width  = *width;
        swapchainExtent.height = *height;
    } else {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCaps.currentExtent;
        *width          = surfCaps.currentExtent.width;
        *height         = surfCaps.currentExtent.height;
    }

    // Select a present mode for the swapchain
    // Get available present modes
    uint32_t presentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL));
    assert(presentModeCount > 0);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()));

    // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
    // This mode waits for the vertical blank ("v-sync")
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

    // If v-sync is not requested, try to find a mailbox mode
    // It's the lowest latency non-tearing present mode available
    if (!vsync) {
        for (size_t i = 0; i < presentModeCount; i++) {
            if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
            if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }

    // Determine the number of images
    uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
    if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount)) {
        desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
    }

    // Find the transformation of the surface
    VkSurfaceTransformFlagsKHR preTransform;
    if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) {
        // We prefer a non-rotated transform
        preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    } else {
        preTransform = surfCaps.currentTransform;
    }

    // Find a supported composite alpha format (not all devices support alpha opaque)
    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    // Simply select the first composite alpha format available
    std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (auto& compositeAlphaFlag : compositeAlphaFlags) {
        if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
            compositeAlpha = compositeAlphaFlag;
            break;
        };
    }

    VkSwapchainCreateInfoKHR swapchainCI = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapchainCI.surface                  = surface;
    swapchainCI.minImageCount            = desiredNumberOfSwapchainImages;
    swapchainCI.imageFormat              = colorFormat;
    swapchainCI.imageColorSpace          = colorSpace;
    swapchainCI.imageExtent              = {swapchainExtent.width, swapchainExtent.height};
    swapchainCI.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCI.preTransform             = (VkSurfaceTransformFlagBitsKHR)preTransform;
    swapchainCI.imageArrayLayers         = 1;
    swapchainCI.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    // queueFamilyIndexCount is the number of queue families having access to the image(s) of the swapchain when imageSharingMode is VK_SHARING_MODE_CONCURRENT
    swapchainCI.queueFamilyIndexCount = 0;// 0的原因见上
    swapchainCI.presentMode           = swapchainPresentMode;
    // Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
    swapchainCI.oldSwapchain = oldSwapchain;
    // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
    swapchainCI.clipped        = VK_TRUE;
    swapchainCI.compositeAlpha = compositeAlpha;

    // Enable transfer source on swap chain images if supported
    if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
        swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    // Enable transfer destination on swap chain images if supported
    if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
        swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCI, nullptr, &swapChain));

    // If an existing swap chain is re-created, destroy the old swap chain
    // This also cleans up all the presentable images
    if (oldSwapchain != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < imageCount; i++) {
            vkDestroyImageView(device, buffers[i].view, nullptr);
        }
        vkDestroySwapchainKHR(device, oldSwapchain, nullptr);   // 会自动把image给销毁
    }

    // Get the swap chain images
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, NULL));
    images.resize(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapChain, &imageCount, images.data()));

    // Get the swap chain buffers containing the image and imageview
    buffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo colorAttachmentView = {};
        colorAttachmentView.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        colorAttachmentView.pNext                 = NULL;
        colorAttachmentView.format                = colorFormat;
        colorAttachmentView.components            = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A};
        colorAttachmentView.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        colorAttachmentView.subresourceRange.baseMipLevel   = 0;
        colorAttachmentView.subresourceRange.levelCount     = 1;
        colorAttachmentView.subresourceRange.baseArrayLayer = 0;
        colorAttachmentView.subresourceRange.layerCount     = 1;
        colorAttachmentView.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        colorAttachmentView.flags                           = 0;

        buffers[i].image          = images[i];
        colorAttachmentView.image = buffers[i].image;

        VK_CHECK(vkCreateImageView(device, &colorAttachmentView, nullptr, &buffers[i].view));
    }
}

/** 
* Acquires the next image in the swap chain
*
* @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
* @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
*
* @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
*
* @return VkResult of the image acquisition
*/
VkResult VulkanSwapChain::acquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t* imageIndex) {
    // By setting timeout to UINT64_MAX we will always wait until the next image has been acquired or an actual error is thrown
    // With that we don't have to handle VK_NOT_READY
    return vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, presentCompleteSemaphore, (VkFence) nullptr, imageIndex);
}

/**
* Queue an image for presentation
*
* @param queue Presentation queue for presenting the image
* @param imageIndex Index of the swapchain image to queue for presentation
* @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if != VK_NULL_HANDLE)
*
* @return VkResult of the queue presentation
*/
VkResult VulkanSwapChain::queuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.pNext            = NULL;
    presentInfo.swapchainCount   = 1;
    presentInfo.pSwapchains      = &swapChain;
    presentInfo.pImageIndices    = &imageIndex;
    // Check if a wait semaphore has been specified to wait for before presenting the image
    if (waitSemaphore != VK_NULL_HANDLE) {
        presentInfo.pWaitSemaphores    = &waitSemaphore;
        presentInfo.waitSemaphoreCount = 1;
    }
    return vkQueuePresentKHR(queue, &presentInfo);
}

/**
* Destroy and free Vulkan resources used for the swapchain
*/
void VulkanSwapChain::cleanup() {
    if (swapChain != VK_NULL_HANDLE) {
        for (uint32_t i = 0; i < imageCount; i++) {
            vkDestroyImageView(device, buffers[i].view, nullptr);
        }
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);  //这里会自动把vkImage给销毁
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    surface   = VK_NULL_HANDLE;
    swapChain = VK_NULL_HANDLE;
}