#include "VulkanDevice.h"

namespace LR {

    /**
    * @brief default constructor to set up basics of a physical device
    *
    * @param physicalDevice physical device being used
    */
    VulkanDevice::VulkanDevice(VkPhysicalDevice physicalDevice) {
        assert(physicalDevice && "physical device invalid!");
        this->physicalDevice = physicalDevice;

        vkGetPhysicalDeviceProperties(physicalDevice, &physicalProperties);
        vkGetPhysicalDeviceFeatures(physicalDevice, &physicalFeatures);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

        uint32_t queueFamilyCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        assert(queueFamilyCount > 0 && "device lack of queueFamily!");
        queueFamilyProperties.resize(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

        // get supported extensions
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
        if (extCount > 0) {
            std::vector<VkExtensionProperties> extensions(extCount);
            if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, extensions.data())) {
                for (auto extension : extensions)
                    supportedExtensions.push_back(extension.extensionName);
            }
        }
    }

    VulkanDevice::~VulkanDevice() {
        if (commandPool)
            vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
        if (logicalDevice)
            vkDestroyDevice(logicalDevice, nullptr);
    }

    uint32_t VulkanDevice::getMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound) const {
        for (uint32_t i = 0; i < static_cast<uint32_t>(memoryProperties.memoryTypeCount); ++i) {
            // == 的优先级高于 &
            if ((typeBits & 1) == 1) {
                if ((properties & memoryProperties.memoryTypes[i].propertyFlags) == properties) {
                    if (memTypeFound)
                        *memTypeFound = true;
                    return i;
                }
            }
            typeBits >>= 1;//右移1位
        }
        if (memTypeFound) {
            *memTypeFound = false;
            return 0;
        } else {
            throw std::runtime_error("couldn't find a matching memory type!");
        }
    }

    // 先找只负责一个的队列族的目的：尽可能地将不同类型的任务分配到不同的队列族，以实现任务的并行执行，从而提高整体性能
    // 理解位与：只有设备支持的包含要求的才行，因此&后是要求的
    uint32_t VulkanDevice::getQueueFamilyIndex(VkQueueFlags queueFlags) const {
        // Dedicated queue for compute
        // Try to find a queue family index that supports compute but not graphics
        if ((queueFlags & VK_QUEUE_COMPUTE_BIT) == queueFlags) {
            for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); ++i) {
                if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
                    return i;
            }
        }

        // Dedicated queue for transfer
        // Try to find a queue family index that supports transfer but not graphics and compute
        if ((queueFlags & VK_QUEUE_TRANSFER_BIT) == queueFlags) {
            for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
                if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0)) {
                    return i;
                }
            }
        }

        // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++) {
            if ((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags) {
                return i;
            }
        }

        throw std::runtime_error("failed to find a queue family index");
    }

    /**
    * @note queue是在这一步创建好的（与logicalDevice一起）
    */
    VkResult VulkanDevice::createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures,
                                               std::vector<const char*> enabledExtensions,
                                               void*                    pNextChain,
                                               bool                     useSwapChain,
                                               VkQueueFlags             requestedQueueTypes) {
        // Desired queues need to be requested upon logical device creation
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

        const float defaultPriority = 0.f;

        if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT) {
            queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
            VkDeviceQueueCreateInfo queueCI{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueCI.queueFamilyIndex = queueFamilyIndices.graphics;
            queueCI.queueCount       = 1;
            queueCI.pQueuePriorities = &defaultPriority;
            queueCreateInfos.push_back(queueCI);
        }

        if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) {
            queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
            if (queueFamilyIndices.compute != queueFamilyIndices.graphics) {
                VkDeviceQueueCreateInfo queueCI{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                queueCI.queueFamilyIndex = queueFamilyIndices.compute;
                queueCI.queueCount       = 1;
                queueCI.pQueuePriorities = &defaultPriority;
                queueCreateInfos.push_back(queueCI);
            }
            // 等于的话就不用加了
        }

        if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT) {
            queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
            if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute)) {
                VkDeviceQueueCreateInfo queueCI{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
                queueCI.queueFamilyIndex = queueFamilyIndices.transfer;
                queueCI.queueCount       = 1;
                queueCI.pQueuePriorities = &defaultPriority;
                queueCreateInfos.push_back(queueCI);
            }
            // 等于的话就不用加了
        }

        // 添加swapchain扩展
        std::vector<const char*> deviceExtensions(enabledExtensions);
        if (useSwapChain)
            deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        // 创建logical device
        VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos    = queueCreateInfos.data();
        deviceCreateInfo.pEnabledFeatures     = &enabledFeatures;

        // If a pNext(Chain) has been passed, we need to add it to the device creation info
        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
        if (pNextChain) {
            physicalDeviceFeatures2.sType     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physicalDeviceFeatures2.features  = enabledFeatures;
            physicalDeviceFeatures2.pNext     = pNextChain;
            deviceCreateInfo.pEnabledFeatures = nullptr;
            deviceCreateInfo.pNext            = &physicalDeviceFeatures2;
        }

        if (deviceExtensions.size() > 0) {
            for (const char* enabledExtension : deviceExtensions) {
                if (!extensionSupported(enabledExtension)) {
                    std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
                }
            }
            deviceCreateInfo.enabledExtensionCount   = (uint32_t)deviceExtensions.size();
            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
        }

        this->enabledFeatures = enabledFeatures;

        VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
        if (result != VK_SUCCESS) {
            return result;
        }

        // create a default command pool for graphics command buffers
        commandPool = createCommandPool(queueFamilyIndices.graphics);

        return result;
    }

    /**
	* Create a buffer on the device
	*
	* @param usageFlags Usage flag bit mask for the buffer (i.e. index, vertex, uniform buffer)
	* @param memoryPropertyFlags Memory properties for this buffer (i.e. device local, host visible, coherent)
	* @param size Size of the buffer in byes
	* @param buffer Pointer to the buffer handle acquired by the function
	* @param memory Pointer to the memory handle acquired by the function
	* @param data Pointer to the data that should be copied to the buffer after creation (optional, if not set, no data is copied over)
	*
	* @retval VK_SUCCESS if buffer handle and memory have been created and (optionally passed) data has been copied
	*/
    VkResult VulkanDevice::createBuffer(VkBufferUsageFlags    usageFlags,
                                        VkMemoryPropertyFlags memoryPropertyFlags,
                                        VkDeviceSize          size,
                                        VkBuffer*             buffer,
                                        VkDeviceMemory*       memory,
                                        void*                 data) {
        // create buffer handle
        VkBufferCreateInfo bufferCI = LR::initializers::bufferCreateInfo(usageFlags, size);
        bufferCI.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
        VK_CHECK(vkCreateBuffer(logicalDevice, &bufferCI, nullptr, buffer));

        //create memory backup
        VkMemoryAllocateInfo memAlloc = LR::initializers::memoryAllocateInfo();
        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(logicalDevice, *buffer, &memReqs);
        memAlloc.allocationSize  = memReqs.size;
        memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, memoryPropertyFlags);

        // 用于支持让shader直接访问这块内存
        // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
        if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            memAlloc.pNext       = &allocFlagsInfo;
        }

        VK_CHECK(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, memory));

        // If a pointer to the buffer data has been passed, map the buffer and copy over the data
        if (data) {
            void* mapped;
            VK_CHECK(vkMapMemory(logicalDevice, *memory, 0, size, 0, &mapped));
            memcpy(mapped, data, size);
            // If host coherency hasn't been requested, do a manual flush to make writes visible
            if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
                VkMappedMemoryRange mappedRange = LR::initializers::mappedMemoryRange();
                mappedRange.memory              = *memory;
                mappedRange.offset              = 0;
                mappedRange.size                = size;
                vkFlushMappedMemoryRanges(logicalDevice, 1, &mappedRange);//将缓存中的数据刷新到设备内存中，从而设备可以读取这部分映射的数据
            }
            vkUnmapMemory(logicalDevice, *memory);
        }

        VK_CHECK(vkBindBufferMemory(logicalDevice, *buffer, *memory, 0));

        return VK_SUCCESS;
    }

    // 使用VulkanBuffer的版本
    VkResult VulkanDevice::createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, LR::Buffer* buffer, VkDeviceSize size, void* data) {
        buffer->device = logicalDevice;

        // Create the buffer handle
        VkBufferCreateInfo bufferCreateInfo = LR::initializers::bufferCreateInfo(usageFlags, size);
        VK_CHECK(vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer));

        // Create the memory backing up the buffer handle
        VkMemoryRequirements memReqs;
        VkMemoryAllocateInfo memAlloc = LR::initializers::memoryAllocateInfo();
        vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memReqs);
        memAlloc.allocationSize = memReqs.size;
        // Find a memory type index that fits the properties of the buffer
        memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, memoryPropertyFlags);
        // If the buffer has VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT set we also need to enable the appropriate flag during allocation
        VkMemoryAllocateFlagsInfoKHR allocFlagsInfo{};
        if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO_KHR;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;
            memAlloc.pNext       = &allocFlagsInfo;
        }
        VK_CHECK(vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &buffer->memory));

        buffer->alignment           = memReqs.alignment;
        buffer->size                = size;
        buffer->usageFlags          = usageFlags;
        buffer->memoryPropertyFlags = memoryPropertyFlags;

        // If a pointer to the buffer data has been passed, map the buffer and copy over the data
        if (data != nullptr) {
            VK_CHECK(buffer->map());
            memcpy(buffer->mapped, data, size);
            if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
                buffer->flush();

            buffer->unmap();
        }

        // Initialize a default descriptor that covers the whole buffer size
        buffer->setUpDescriptorInfo();

        // Attach the memory to the buffer object
        return buffer->bind();
    }

    /**
	* Copy buffer data from src to dst using VkCmdCopyBuffer
	* 
	* @param src Pointer to the source buffer to copy from
	* @param dst Pointer to the destination buffer to copy to
	* @param queue Pointer
	* @param copyRegion (Optional) Pointer to a copy region, if NULL, the whole buffer is copied
	*
	* @note Source and destination pointers must have the appropriate transfer usage flags set (TRANSFER_SRC / TRANSFER_DST)
	*/
    void VulkanDevice::copyBuffer(LR::Buffer* src, LR::Buffer* dst, VkQueue queue, VkBufferCopy* copyRegion) {
        // 拷贝整个或部分，因此小于等于
        assert(dst->size <= src->size);
        assert(src->buffer);
        VkCommandBuffer copyCmd = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
        VkBufferCopy    bufferCopy{};
        if (copyRegion == nullptr)
            bufferCopy.size = src->size;
        else
            bufferCopy = *copyRegion;
        vkCmdCopyBuffer(copyCmd, src->buffer, dst->buffer, 1, &bufferCopy);
        flushCommandBuffer(copyCmd, queue);
    }

    /** 
	* Create a command pool for allocation command buffers from
	* 
	* @param queueFamilyIndex Family index of the queue to create the command pool for
	* @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
	*
	* @note Command buffers allocated from the created pool can only be submitted to a queue with the same family index
	*
	* @retval A handle to the created command buffer
	*/
    VkCommandPool VulkanDevice::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags) {
        VkCommandPoolCreateInfo cmdPoolInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        cmdPoolInfo.queueFamilyIndex        = queueFamilyIndex;
        cmdPoolInfo.flags                   = createFlags;
        VkCommandPool cmdPool;
        VK_CHECK(vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
        return cmdPool;
    }

    VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, VkCommandPool pool, bool begin) {
        VkCommandBufferAllocateInfo cmdBufAllocateInfo = LR::initializers::commandBufferAllocateInfo(pool, level, 1);
        VkCommandBuffer             cmdBuffer;
        VK_CHECK(vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer));
        // If requested, also start recording for the new command buffer
        if (begin) {
            VkCommandBufferBeginInfo cmdBufInfo = LR::initializers::commandBufferBeginInfo();
            VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
        }
        return cmdBuffer;
    }

    VkCommandBuffer VulkanDevice::createCommandBuffer(VkCommandBufferLevel level, bool begin) {
        return createCommandBuffer(level, commandPool, begin);
    }

    /**
	* Finish command buffer recording and submit it to a queue
	*
	* @param commandBuffer Command buffer to flush
	* @param queue Queue to submit the command buffer to
	* @param pool Command pool on which the command buffer has been created
	* @param free (Optional) Free the command buffer once it has been submitted (Defaults to true)
	*
	* @note The queue that the command buffer is submitted to must be from the same family index as the pool it was allocated from
	* @note Uses a fence to ensure command buffer has finished executing
	*/
    void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, VkCommandPool pool, bool free) {
        if (commandBuffer == VK_NULL_HANDLE) {
            return;
        }

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkSubmitInfo submitInfo       = LR::initializers::submitInfo();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;
        // Create fence to ensure that the command buffer has finished executing
        VkFenceCreateInfo fenceInfo = LR::initializers::fenceCreateInfo(VK_FLAGS_NONE);
        VkFence           fence;
        VK_CHECK(vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence));
        // Submit to the queue
        VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, fence));
        // Wait for the fence to signal that command buffer has finished executing
        VK_CHECK(vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
        vkDestroyFence(logicalDevice, fence, nullptr);
        if (free) {
            vkFreeCommandBuffers(logicalDevice, pool, 1, &commandBuffer);
        }
    }

    void VulkanDevice::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free) {
        return flushCommandBuffer(commandBuffer, queue, commandPool, free);
    }

    /**
	* Check if an extension is supported by the (physical device)
	*
	* @param extension Name of the extension to check
	*
	* @retval True if the extension is supported (present in the list read at device creation time)
	*/
    bool VulkanDevice::extensionSupported(std::string extension) const {
        return (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end());
    }

        /**
	* Select the best-fit depth format for this device from a list of possible depth (and stencil) formats
	*
	* @param checkSamplingSupport Check if the format can be sampled from (e.g. for shader reads)
	*
	* @retval The depth format that best fits for the current device
	*
	* @throw Throws an exception if no depth format fits the requirements
	*/
    VkFormat VulkanDevice::getSupportedDepthFormat(bool checkSamplingSupport) const {
        // All depth formats may be optional, so we need to find a suitable depth format to use
        std::vector<VkFormat> depthFormats = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
        for (auto& format : depthFormats) {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
            // Format must support depth stencil attachment for optimal tiling
            if (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                if (checkSamplingSupport) {
                    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
                        continue;
                    }
                }
                return format;
            }
        }
        throw std::runtime_error("Could not find a matching depth format");
    }

}// namespace LR