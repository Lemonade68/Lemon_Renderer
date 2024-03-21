#ifndef VULKAN_TOOLS_H
#define VULKAN_TOOLS_H

#include "vulkan.h"
#include "initializer.h"
#include <assert.h>
#include <iostream>

#define VK_FLAGS_NONE 0

#define DEFAULT_FENCE_TIMEOUT 100000000000

#define VK_CHECK(f)                             \
    {                                           \
        VkResult result = (f);                  \
        if (result != VK_SUCCESS) {             \
            std::cout << "Fatal error happens!" \
                      << "\n";                  \
            assert(result == VK_SUCCESS);       \
        }                                       \
    }

namespace LR {
namespace tools {

    /**
    * @brief 使用一个完整的subresourcerange来setImageLayout（可针对多个mipmap和layer），Put an image memory barrier for setting an image layout on the sub resource into the given command buffer
    */
    void setImageLayout(
        VkCommandBuffer         cmdbuffer,
        VkImage                 image,
        VkImageLayout           oldImageLayout,
        VkImageLayout           newImageLayout,
        VkImageSubresourceRange subresourceRange,
        VkPipelineStageFlags    srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags    dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    /**
    * @brief 直接使用第一层mipmap和第一个layer信息的subresource来进行setImageLayout，因此只额外需要aspectMask参数即可（Uses a fixed sub resource layout with first mip level and layer）
    */
    void setImageLayout(
        VkCommandBuffer      cmdbuffer,
        VkImage              image,
        VkImageAspectFlags   aspectMask,
        VkImageLayout        oldImageLayout,
        VkImageLayout        newImageLayout,
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

}
}// namespace LR::tools

#endif