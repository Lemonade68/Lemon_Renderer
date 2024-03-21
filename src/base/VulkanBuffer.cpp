#include "VulkanBuffer.h"
#include <assert.h>
#include <string.h>

namespace LR {

    /**
    * @brief Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
    *
    * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete buffer range.
    * @param offset (Optional) Byte offset from beginning
    *
    * @retval VkResult of the buffer mapping call
    */
    VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset) {
        return vkMapMemory(device, memory, offset, size, 0, &mapped);
    }

    /**
    * @brief Unmap a mapped memory range
    *
    * @note Does not return a result as vkUnmapMemory can't fail
    */
    void Buffer::unmap() {
        if (mapped) {
            vkUnmapMemory(device, memory);
            mapped = nullptr;
        }
    }

    /**
    * @brief Attach the allocated memory block to the buffer
    *
    * @param offset (Optional) Byte offset (from the beginning) for the memory region to bind
    *
    * @retval VkResult of the bindBufferMemory call
    */
    VkResult Buffer::bind(VkDeviceSize offset) {
        return vkBindBufferMemory(device, buffer, memory, offset);
    }

    /**
    * @brief Setup the default descriptor for this buffer
    *
    * @param range (Optional) Size of the memory range of the descriptor
	* @param offset (Optional) Byte offset from beginning
    */
    void Buffer::setUpDescriptorInfo(VkDeviceSize range, VkDeviceSize offset) {
        descriptorInfo.buffer = buffer;
        descriptorInfo.offset = offset;
        descriptorInfo.range  = range;
    }

    /**
    * @brief Copies the specified data to the mapped buffer
    *
    * @param data Pointer to the data to copy
	* @param size Size of the data to copy in machine units
    */
    void Buffer::copyFrom(void* data, VkDeviceSize size) {
        assert(mapped && "Buffer hasn't been mapped!");
        memcpy(mapped, data, size);
    }

    /**
    * @brief Release all Vulkan resources held by this buffer
    */
    void Buffer::destroy(){
        if(buffer)
            vkDestroyBuffer(device, buffer, nullptr);
        if(memory)
            vkFreeMemory(device, memory, nullptr);
    }

    /** 
	* Flush a memory range of the buffer to make it visible to the device
	*
	* @note Only required for non-coherent memory
	*
	* @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @retval VkResult of the flush call
	*/
	VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
		VkMappedMemoryRange mappedRange{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
		mappedRange.memory = memory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		return vkFlushMappedMemoryRanges(device, 1, &mappedRange);
	}

	/**
	* Invalidate a memory range of the buffer to make it invisible to the host
	*
	* @note Only required for non-coherent memory
	*
	* @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate the complete buffer range.
	* @param offset (Optional) Byte offset from beginning
	*
	* @retval VkResult of the invalidate call
	*/
	VkResult Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
		VkMappedMemoryRange mappedRange{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
		mappedRange.memory = memory;
		mappedRange.offset = offset;
		mappedRange.size = size;
		return vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
	}
    

}// namespace LR