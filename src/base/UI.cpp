#include "UI.h"
#include "renderer/Renderer.h"

namespace LR {
    ImGUI::ImGUI(Renderer* renderer, float scale) : renderer(renderer) {
        device           = renderer->vulkanDevice;
        uiSettings.scale = scale;
        ImGui::CreateContext();

        //SRS - Set ImGui font and style scale factors to handle retina and other HiDPI displays
        ImGuiIO& io        = ImGui::GetIO();
        io.FontGlobalScale = scale;
        ImGuiStyle& style  = ImGui::GetStyle();
        style.ScaleAllSizes(scale);

        // enable keyboard input
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    };

    ImGUI::~ImGUI() {
        ImGui::DestroyContext();
        // Release all Vulkan resources required for rendering imGui
        for (auto vertexBuffer : vertexBuffers)
            vertexBuffer.destroy();
        for (auto indexBuffer : indexBuffers)
            indexBuffer.destroy();
        vkDestroyImage(device->logicalDevice, fontImage, nullptr);
        vkDestroyImageView(device->logicalDevice, fontView, nullptr);
        vkFreeMemory(device->logicalDevice, fontMemory, nullptr);
        vkDestroySampler(device->logicalDevice, sampler, nullptr);
        vkDestroyPipelineCache(device->logicalDevice, pipelineCache, nullptr);
        vkDestroyPipeline(device->logicalDevice, pipeline, nullptr);
        vkDestroyPipelineLayout(device->logicalDevice, pipelineLayout, nullptr);
        vkDestroyDescriptorPool(device->logicalDevice, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device->logicalDevice, descriptorSetLayout, nullptr);
    }

    // Initialize styles, keys, etc.
    void ImGUI::init(float width, float height) {
        // Color scheme
        vulkanStyle                                = ImGui::GetStyle();// getSytle那里如果是引用的话就会更改真正的style
        vulkanStyle.Colors[ImGuiCol_TitleBg]       = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
        vulkanStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        vulkanStyle.Colors[ImGuiCol_MenuBarBg]     = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        vulkanStyle.Colors[ImGuiCol_Header]        = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
        vulkanStyle.Colors[ImGuiCol_CheckMark]     = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);

        setStyle(0);// 设置io的style为vulkanstyle

        // Dimensions
        ImGuiIO& io                = ImGui::GetIO();
        io.DisplaySize             = ImVec2(width, height);
        io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
#if defined(_WIN32)
        // If we directly work with os specific key codes, we need to map special key types like tab
        io.KeyMap[ImGuiKey_Tab]        = VK_TAB;
        io.KeyMap[ImGuiKey_LeftArrow]  = VK_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow]    = VK_UP;
        io.KeyMap[ImGuiKey_DownArrow]  = VK_DOWN;
        io.KeyMap[ImGuiKey_Backspace]  = VK_BACK;
        io.KeyMap[ImGuiKey_Enter]      = VK_RETURN;
        io.KeyMap[ImGuiKey_Space]      = VK_SPACE;
        io.KeyMap[ImGuiKey_Delete]     = VK_DELETE;
#endif
    }

    void ImGUI::setStyle(uint32_t index) {
        switch (index) {
            case 0: {
                ImGuiStyle& style = ImGui::GetStyle();
                style             = vulkanStyle;
                break;
            }
            case 1:
                ImGui::StyleColorsClassic();
                break;
            case 2:
                ImGui::StyleColorsDark();
                break;
            case 3:
                ImGui::StyleColorsLight();
                break;
        }
    }

    // Initialize all Vulkan resources used by the ui
    void ImGUI::initResources(VkRenderPass renderPass, VkQueue copyQueue, uint32_t maxFrameCount) {
        ImGuiIO& io = ImGui::GetIO();

        // init vectors
        vertexBuffers.resize(static_cast<size_t>(maxFrameCount));
        indexBuffers.resize(static_cast<size_t>(maxFrameCount));
        vertexCounts.resize(static_cast<size_t>(maxFrameCount));
        indexCounts.resize(static_cast<size_t>(maxFrameCount));

        // Create font texture
        unsigned char* fontData;
        int            texWidth, texHeight;
        const std::string filename = "../../../assets/Roboto-Medium.ttf";
        io.Fonts->AddFontFromFileTTF(filename.c_str(), 16.0f * uiSettings.scale);
        io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
        VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

        //SRS - Set ImGui style scale factor to handle retina and other HiDPI displays (same as font scaling above)
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(uiSettings.scale);

        //SRS - Get Vulkan device driver information if available, use later for display
        if (device->extensionSupported(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
            VkPhysicalDeviceProperties2 deviceProperties2 = {};
            deviceProperties2.sType                       = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProperties2.pNext                       = &driverProperties;
            driverProperties.sType                        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
            vkGetPhysicalDeviceProperties2(device->physicalDevice, &deviceProperties2);
        }

        // Create target font image for copy
        VkImageCreateInfo imageInfo = LR::initializers::imageCreateInfo();
        imageInfo.imageType         = VK_IMAGE_TYPE_2D;
        imageInfo.format            = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent.width      = texWidth;
        imageInfo.extent.height     = texHeight;
        imageInfo.extent.depth      = 1;
        imageInfo.mipLevels         = 1;
        imageInfo.arrayLayers       = 1;
        imageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        VK_CHECK(vkCreateImage(device->logicalDevice, &imageInfo, nullptr, &fontImage));
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(device->logicalDevice, fontImage, &memReqs);
        VkMemoryAllocateInfo memAllocInfo = LR::initializers::memoryAllocateInfo();
        memAllocInfo.allocationSize       = memReqs.size;
        memAllocInfo.memoryTypeIndex      = device->getMemoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_CHECK(vkAllocateMemory(device->logicalDevice, &memAllocInfo, nullptr, &fontMemory));
        VK_CHECK(vkBindImageMemory(device->logicalDevice, fontImage, fontMemory, 0));

        // Image view
        VkImageViewCreateInfo viewInfo       = LR::initializers::imageViewCreateInfo();
        viewInfo.image                       = fontImage;
        viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                      = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device->logicalDevice, &viewInfo, nullptr, &fontView));

        // Staging buffers for font data upload
        LR::VulkanBuffer stagingBuffer;

        VK_CHECK(device->createBuffer(
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &stagingBuffer,
            uploadSize,
            fontData));

        // Copy buffer data to font image
        VkCommandBuffer copyCmd = device->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

        // Prepare for transfer
        LR::tools::setImageLayout(
            copyCmd,
            fontImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

        // Copy
        VkBufferImageCopy bufferCopyRegion           = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageExtent.width           = texWidth;
        bufferCopyRegion.imageExtent.height          = texHeight;
        bufferCopyRegion.imageExtent.depth           = 1;

        vkCmdCopyBufferToImage(
            copyCmd,
            stagingBuffer.buffer,
            fontImage,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &bufferCopyRegion);

        // Prepare for shader read
        LR::tools::setImageLayout(
            copyCmd,
            fontImage,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

        device->flushCommandBuffer(copyCmd, copyQueue, true);

        stagingBuffer.destroy();

        // Font texture Sampler
        VkSamplerCreateInfo samplerInfo = LR::initializers::samplerCreateInfo();
        samplerInfo.magFilter           = VK_FILTER_LINEAR;
        samplerInfo.minFilter           = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode          = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW        = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.borderColor         = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        VK_CHECK(vkCreateSampler(device->logicalDevice, &samplerInfo, nullptr, &sampler));

        // Descriptor pool
        std::vector<VkDescriptorPoolSize> poolSizes = {
            LR::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1)};
        VkDescriptorPoolCreateInfo descriptorPoolInfo = LR::initializers::descriptorPoolCreateInfo(poolSizes, 2);
        VK_CHECK(vkCreateDescriptorPool(device->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));

        // Descriptor set layout
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
            LR::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0)};
        VkDescriptorSetLayoutCreateInfo descriptorLayout = LR::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
        VK_CHECK(vkCreateDescriptorSetLayout(device->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout));

        // Descriptor set
        VkDescriptorSetAllocateInfo allocInfo = LR::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
        VK_CHECK(vkAllocateDescriptorSets(device->logicalDevice, &allocInfo, &descriptorSet));
        VkDescriptorImageInfo fontDescriptor = LR::initializers::descriptorImageInfo(
            sampler,
            fontView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            LR::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &fontDescriptor)};
        vkUpdateDescriptorSets(device->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

        // Pipeline cache
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
        pipelineCacheCreateInfo.sType                     = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        VK_CHECK(vkCreatePipelineCache(device->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache));

        // Pipeline layout
        // Push constants for UI rendering parameters
        VkPushConstantRange        pushConstantRange        = LR::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), 0);
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = LR::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
        pipelineLayoutCreateInfo.pushConstantRangeCount     = 1;
        pipelineLayoutCreateInfo.pPushConstantRanges        = &pushConstantRange;
        VK_CHECK(vkCreatePipelineLayout(device->logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

        // Setup graphics pipeline for UI rendering
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
            LR::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

        VkPipelineRasterizationStateCreateInfo rasterizationState =
            LR::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

        // Enable blending
        VkPipelineColorBlendAttachmentState blendAttachmentState{};
        blendAttachmentState.blendEnable         = VK_TRUE;
        blendAttachmentState.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
        blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blendAttachmentState.alphaBlendOp        = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlendState =
            LR::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

        VkPipelineDepthStencilStateCreateInfo depthStencilState =
            LR::initializers::pipelineDepthStencilStateCreateInfo(VK_FALSE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

        VkPipelineViewportStateCreateInfo viewportState =
            LR::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

        VkPipelineMultisampleStateCreateInfo multisampleState =
            LR::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

        std::vector<VkDynamicState> dynamicStateEnables = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamicState =
            LR::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

        auto vertShaderCode = renderer->readFile("../../../shaders/uivert.spv");
        auto fragShaderCode = renderer->readFile("../../../shaders/uifrag.spv");

        VkShaderModule vertShaderModule = renderer->createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = renderer->createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName  = "main";//入口函数(实际invoke(调用)的函数)
        //vertShaderStageInfo.pSpecializationInfo = nullptr;		//optional 但重要(看教程)

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName  = "main";

        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertShaderStageInfo, fragShaderStageInfo};

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = LR::initializers::pipelineCreateInfo(pipelineLayout, renderPass);

        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pRasterizationState = &rasterizationState;
        pipelineCreateInfo.pColorBlendState    = &colorBlendState;
        pipelineCreateInfo.pMultisampleState   = &multisampleState;
        pipelineCreateInfo.pViewportState      = &viewportState;
        pipelineCreateInfo.pDepthStencilState  = &depthStencilState;
        pipelineCreateInfo.pDynamicState       = &dynamicState;
        pipelineCreateInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
        pipelineCreateInfo.pStages             = shaderStages.data();

        // Vertex bindings an attributes based on ImGui vertex definition
        std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            LR::initializers::vertexInputBindingDescription(0, sizeof(ImDrawVert), VK_VERTEX_INPUT_RATE_VERTEX),
        };
        std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            LR::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos)), // Location 0: Position
            LR::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv)),  // Location 1: UV
            LR::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col)),// Location 0: Color
        };
        VkPipelineVertexInputStateCreateInfo vertexInputState = LR::initializers::pipelineVertexInputStateCreateInfo();
        vertexInputState.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertexInputBindings.size());
        vertexInputState.pVertexBindingDescriptions           = vertexInputBindings.data();
        vertexInputState.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertexInputAttributes.size());
        vertexInputState.pVertexAttributeDescriptions         = vertexInputAttributes.data();

        pipelineCreateInfo.pVertexInputState = &vertexInputState;

        VK_CHECK(vkCreateGraphicsPipelines(device->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));

        vkDestroyShaderModule(device->logicalDevice, vertShaderModule, nullptr);
        vkDestroyShaderModule(device->logicalDevice, fragShaderModule, nullptr);
    }

    // Starts a new imGui frame and sets up windows and ui elements
    void ImGUI::newFrame() {
        // 开始新的imgui的一帧
        ImGui::NewFrame();//会自动创建 debug window

        // Init imGui windows and elements
        // Debug window (那个叫 debug 的 window)
        ImGui::SetWindowPos(ImVec2(20 * uiSettings.scale, 20 * uiSettings.scale), ImGuiCond_FirstUseEver);
        ImGui::SetWindowSize(ImVec2(300 * uiSettings.scale, 300 * uiSettings.scale), ImGuiCond_FirstUseEver);
        // ImGui::SetWindowSize(ImVec2(300 * scale, 300 * scale), ImGuiSetCond_Always);
        ImGui::TextUnformatted(device->physicalProperties.deviceName);

        //SRS - Display Vulkan API version and device driver information if available (otherwise blank)
        ImGui::Text("Vulkan API %i.%i.%i", VK_API_VERSION_MAJOR(device->physicalProperties.apiVersion), VK_API_VERSION_MINOR(device->physicalProperties.apiVersion), VK_API_VERSION_PATCH(device->physicalProperties.apiVersion));
        ImGui::Text("%s %s", driverProperties.driverName, driverProperties.driverInfo);

        // if (updateFrameGraph) {
        //     std::rotate(uiSettings.frameTimes.begin(), uiSettings.frameTimes.begin() + 1, uiSettings.frameTimes.end());//每次更新其中一个
        //     float frameTime              = 1000.0f / (renderer->frameTimer * 1000.0f);
        //     uiSettings.frameTimes.back() = frameTime;
        //     if (frameTime < uiSettings.frameTimeMin) {
        //         uiSettings.frameTimeMin = frameTime;//记录谷值
        //     }
        //     if (frameTime > uiSettings.frameTimeMax) {
        //         uiSettings.frameTimeMax = frameTime;//记录峰值
        //     }
        // }

        // ImGui::PlotLines("Frame Times", &uiSettings.frameTimes[0], 50, 0, "", uiSettings.frameTimeMin, uiSettings.frameTimeMax, ImVec2(0, 80));

        // ImGui::Text("Camera");
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            // ImGui::InputFloat3("position", &camera.position.x, 2);//最后一个数字是小数点后位数
            if (ImGui::DragFloat3("position", &camera.position.x, 0.05f, -9999.f, 9999.f)) { camera.changedView = true; }
            if (ImGui::DragFloat("pitch", &camera.pitch, 0.25f, -89.f, 89.f)) { camera.changedView = true; }
            if (ImGui::DragFloat("yaw", &camera.yaw, 0.25f, -180.f, 180.f)) { camera.changedView = true; }
            if (ImGui::DragFloat("fovY", &camera.fov_y, .5f, 20.f, 120.f)) { camera.changedPerspect = true; }
            if (ImGui::DragFloat("zNear", &camera.zNear, 0.005f, 0.001f, 5.f)) { camera.changedPerspect = true; }
            if (ImGui::DragFloat("zFar", &camera.zFar, 0.5f, 5.f, 1000.f)) { camera.changedPerspect = true; }
            ImGui::Separator();
        }

        // // Example settings window
        // ImGui::SetNextWindowPos(ImVec2(20 * scale, 360 * scale), ImGuiSetCond_FirstUseEver);
        // ImGui::SetNextWindowSize(ImVec2(300 * scale, 200 * scale), ImGuiSetCond_FirstUseEver);
        // ImGui::Begin("Example settings");
        // ImGui::Checkbox("Render models", &uiSettings.displayModels);
        // ImGui::Checkbox("Display logos", &uiSettings.displayLogos);
        // ImGui::Checkbox("Display background", &uiSettings.displayBackground);
        // ImGui::Checkbox("Animate light", &uiSettings.animateLight);
        // ImGui::SliderFloat("Light speed", &uiSettings.lightSpeed, 0.1f, 1.0f);
        // // ImGui::ShowStyleSelector("UI style");

        if (ImGui::CollapsingHeader("UI")) {
            if (ImGui::Combo("UI style", &uiSettings.selectedStyle, "Vulkan\0Classic\0Dark\0Light\0")) {
                setStyle(uiSettings.selectedStyle);
            }
            ImGui::DragFloat("uiScale", &uiSettings.scale, .05f, .5f, 3.f);
            ImGui::Separator();
        }

        // skybox
        ImGui::ColorEdit3("clear color", renderer->clearValues[0].color.float32);
        ImGui::Checkbox("skybox", &uiSettings.skybox);


        // 后续优化
        // if(ImGui::Checkbox("enterScreen", &camera.enterCam)){
        // }

        // 默认的debug窗口不用end，自己创建的窗口需要end，debug时调用currentWindowStack.size在NewFrame中+1，Begin中+1，End中-1
        // ImGui::End();

        //SRS - ShowDemoWindow() sets its own initial position and size, cannot override here
        ImGui::ShowDemoWindow();

        // Render to generate draw buffers
        ImGui::Render();
    }

    // Update vertex and index buffer containing the imGui elements when required
    bool ImGUI::updateBuffers(uint32_t currentFrame) {
        bool        updateCmdBuffers = false;
        ImDrawData* imDrawData       = ImGui::GetDrawData();

        // Note: Alignment is done inside buffer creation
        VkDeviceSize vertexBufferSize = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
        VkDeviceSize indexBufferSize  = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

        if ((vertexBufferSize == 0) || (indexBufferSize == 0)) {
            return false;
        }

        // Update buffers only if vertex or index count has been changed compared to current buffer size

        // Vertex buffer
        if ((vertexBuffers[currentFrame].buffer == VK_NULL_HANDLE) || (vertexCounts[currentFrame] != imDrawData->TotalVtxCount)) {
            // vkDeviceWaitIdle(device->logicalDevice);    // 防止ui中的buffer还在被使用时就被删除(后续改成多个vertexBuffer和indexBuffer，因为多个frame共享时删除会导致别的frame中出问题)
            // vkWaitForFences(device->logicalDevice, 1, fence, VK_TRUE, UINT64_MAX);// 防止ui中的buffer还在被使用时就被删除(后续改成多个vertexBuffer和indexBuffer，因为多个frame共享时删除会导致别的frame中出问题)
            vertexBuffers[currentFrame].unmap();
            vertexBuffers[currentFrame].destroy();
            VK_CHECK(device->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &vertexBuffers[currentFrame], vertexBufferSize));
            vertexCounts[currentFrame] = imDrawData->TotalVtxCount;
            vertexBuffers[currentFrame].map();
            updateCmdBuffers = true;
        }

        // Index buffer
        if ((indexBuffers[currentFrame].buffer == VK_NULL_HANDLE) || (indexCounts[currentFrame] < imDrawData->TotalIdxCount)) {
            // vkDeviceWaitIdle(device->logicalDevice);    // 同上
            // vkWaitForFences(device->logicalDevice, 1, fence, VK_TRUE, UINT64_MAX);// 防止ui中的buffer还在被使用时就被删除(后续改成多个vertexBuffer和indexBuffer，因为多个frame共享时删除会导致别的frame中出问题)
            indexBuffers[currentFrame].unmap();
            indexBuffers[currentFrame].destroy();
            VK_CHECK(device->createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &indexBuffers[currentFrame], indexBufferSize));
            indexCounts[currentFrame] = imDrawData->TotalIdxCount;
            indexBuffers[currentFrame].map();
            updateCmdBuffers = true;
        }

        // Upload data
        ImDrawVert* vtxDst = (ImDrawVert*)vertexBuffers[currentFrame].mapped;
        ImDrawIdx*  idxDst = (ImDrawIdx*)indexBuffers[currentFrame].mapped;

        // 每个imgui window都有自己的imDrawList(这里的cmdlist)，因此要将所有window的顶点数据全部传进来
        for (int n = 0; n < imDrawData->CmdListsCount; n++) {
            const ImDrawList* cmd_list = imDrawData->CmdLists[n];
            memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmd_list->VtxBuffer.Size;
            idxDst += cmd_list->IdxBuffer.Size;
        }

        // Flush to make writes visible to GPU
        vertexBuffers[currentFrame].flush();
        indexBuffers[currentFrame].flush();

        return updateCmdBuffers;
    }

    // Draw current imGui frame into a command buffer
    void ImGUI::drawFrame(VkCommandBuffer& commandBuffer, uint32_t currentFrame) {
        ImGuiIO& io = ImGui::GetIO();

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        VkViewport viewport = LR::initializers::viewport(0.f, 0.f, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y, 0.0f, 1.0f);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        //scissor跟外面一样

        // UI scale and translate via push constants
        pushConstBlock.scale     = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
        pushConstBlock.translate = glm::vec2(-1.0f);
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &pushConstBlock);

        // Render commands
        ImDrawData* imDrawData   = ImGui::GetDrawData();
        int32_t     vertexOffset = 0;
        int32_t     indexOffset  = 0;

        if (imDrawData->CmdListsCount > 0) {

            VkDeviceSize offsets[1] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffers[currentFrame].buffer, offsets);
            vkCmdBindIndexBuffer(commandBuffer, indexBuffers[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT16);

            for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
                const ImDrawList* cmd_list = imDrawData->CmdLists[i];
                for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                    const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
                    VkRect2D         scissorRect;
                    scissorRect.offset.x      = std::max((int32_t)(pcmd->ClipRect.x), 0);
                    scissorRect.offset.y      = std::max((int32_t)(pcmd->ClipRect.y), 0);
                    scissorRect.extent.width  = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                    scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                    vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
                    vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                    indexOffset += pcmd->ElemCount;
                }
                vertexOffset += cmd_list->VtxBuffer.Size;
            }
        }
    }

}// namespace LR
