#ifndef IMGUI_VULKAN_UI_H
#define IMGUI_VULKAN_UI_H

#include "renderer/Renderer.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui/examples/imgui_impl_glfw.h"

// UI.h �� Renderer.h�໥��������Ҫǰ������
class Renderer;

namespace LR {

    class ImGUI {
    // private:
    public:
        // Vulkan resources for rendering the UI
        VkSampler sampler;

        // �������ÿ��Frame_in_flight��������ÿ��frame_in_flight��ȡ����һ��
        std::vector<LR::VulkanBuffer> vertexBuffers;
        std::vector<LR::VulkanBuffer> indexBuffers;
        std::vector<int32_t>          vertexCounts;
        std::vector<int32_t>          indexCounts;

        VkDeviceMemory                   fontMemory{VK_NULL_HANDLE};
        VkImage                          fontImage{VK_NULL_HANDLE};
        VkImageView                      fontView{VK_NULL_HANDLE};
        VkPipelineCache                  pipelineCache{VK_NULL_HANDLE};
        VkPipelineLayout                 pipelineLayout{VK_NULL_HANDLE};
        VkPipeline                       pipeline{VK_NULL_HANDLE};
        VkDescriptorPool                 descriptorPool{VK_NULL_HANDLE};
        VkDescriptorSetLayout            descriptorSetLayout{VK_NULL_HANDLE};
        VkDescriptorSet                  descriptorSet{VK_NULL_HANDLE};
        LR::VulkanDevice*                device;
        VkPhysicalDeviceDriverProperties driverProperties{};
        Renderer*                        renderer;
        ImGuiStyle                       vulkanStyle;
        Camera&                          camera = Camera::GetInstance();//ע�⴫��������
        struct {
            int   selectedStyle = 0;    // ui style
            float scale;                // ui window scale
            bool  skybox = false;       // skybox enabled
        } uiSettings;

    public:
        // UI params are set via push constants
        struct PushConstBlock {
            glm::vec2 scale;
            glm::vec2 translate;
        } pushConstBlock;

        ImGUI(Renderer* renderer, float scale = 1.f);

        ~ImGUI();

        // Initialize styles, keys, etc.
        void init(float width, float height);

        void setStyle(uint32_t index);

        // Initialize all Vulkan resources used by the ui
        void initResources(VkRenderPass renderPass, VkQueue copyQueue, uint32_t maxFrameCount);

        // Starts a new imGui frame and sets up windows and ui elements
        void newFrame();

        // Update vertex and index buffer containing the imGui elements when required
        bool updateBuffers(uint32_t currentFrame);

        // Draw current imGui frame into a command buffer(�������cmdBuffer��һ����������Ӱ��ԭ����cmdBuffer)
        void drawFrame(VkCommandBuffer& commandBuffer, uint32_t currentFrame);
    };

}// namespace LR

#endif