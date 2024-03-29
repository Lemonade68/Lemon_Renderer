#ifndef RENDER_BASE_H
#define RENDER_BASE_H

#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>//automatically include vulkan.h
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>
#include <unordered_map>

#define GLM_FORCE_RADIANS                 // force using radians
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES// force using align
#define GLM_FORCE_DEPTH_ZERO_TO_ONE       // depth(0,1) in vulkan
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "base/Camera.h"
#include "base/initializer.h"
#include "base/VulkanTools.h"
#include "base/VulkanBuffer.h"
#include "base/VulkanDevice.h"
#include "base/VulkanTexture.h"
#include "base/VulkanSwapChain.h"
#include "base/VulkanFrameBuffer.hpp"
#include "base/UI.h"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif// NDEBUG

void processInput(GLFWwindow* window);

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;// Basic surface capabilities
    std::vector<VkSurfaceFormatKHR> formats;     // Surface formats(image format & color space)
    std::vector<VkPresentModeKHR>   presentModes;// Available presentation modes
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;

    static std::vector<VkVertexInputBindingDescription>   getBindingDescriptions();
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

    // used by unordered map
    bool operator==(const Vertex& other) const {
        return position == other.position && color == other.color && texCoord == other.texCoord;
    }
};

// 注意对齐要求(mat4的大小为 4byte*(4行*4列) = 64byte)，使用c++11标准的alignas来保证对齐 —— 比较好的做法：全都显示对齐内存
struct ShaderData {
    // glm::vec2 foo;		//vec2大小为8byte，导致后面的mat的offset都不为16的整数倍
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
};

bool checkValidationLayerSupport();
bool checkDeviceExtensionSupport(VkPhysicalDevice device);

// 因为是拓展，所以vkCreateDebugUtilsMessengerEXT没有自动加载，需要我们自己写好代理函数后将其装载进vulkan
// PFN: pointer to function，vkGetInstanceProcAddr从实例中获取后面名字的函数的地址，并装载到func上
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void     DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

// unordered map使用的hash函数，结合结构体的内容来创建一个哈希函数
namespace std {
    template<>
    struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.position) ^
                     (hash<glm::vec3>()(vertex.color) << 1)) >>
                    1) ^
                   (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}// namespace std

// 两个头文件相互包含，需要相互前置声明
namespace LR{
    class ImGUI;
}

// Renderer Class
class Renderer {
public:
    void run() {
        initWindow();
        compileShader();
        initVulkan();
        prepare();
        mainLoop();
    }

    Renderer() = default;
    ~Renderer() { cleanup(); }

public:
    LR::VulkanDevice*        vulkanDevice;// 设置为指针，从而可以控制何时对其销毁，进而触发析构函数
    VkPhysicalDevice         physicalDevice{VK_NULL_HANDLE};
    VkDevice                 device{VK_NULL_HANDLE};
    VkPhysicalDeviceFeatures enabledDeviceFeatures{};//Set of physical device features to be enabled
    std::vector<const char*> enabledDeviceExtensions;

    GLFWwindow*              window{nullptr};
    VkSurfaceKHR             surface{VK_NULL_HANDLE};// 窗口对象
    VkInstance               instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger{VK_NULL_HANDLE};// 用于调用回调函数
    VkQueue                  graphicsQueue{VK_NULL_HANDLE}; // to handle the graphics queue, determined when creating logical device
    VkQueue                  presentQueue{VK_NULL_HANDLE};  // to handle the presentation queue

    LR::VulkanSwapChain        swapchain;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass     renderPass{VK_NULL_HANDLE};
    VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    VkPipeline       graphicsPipeline{VK_NULL_HANDLE};

    VkCommandPool                commandPool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> drawCmdBuffers;

    LR::VulkanBuffer vertexBuffer;
    LR::VulkanBuffer indexBuffer;

    VkDescriptorSetLayout        descriptorSetLayout{VK_NULL_HANDLE};
    VkDescriptorPool             descriptorPool{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptorSets;

    // textures等之后添加了gltf以及ktx的时候再换
    VkImage        textureImage{VK_NULL_HANDLE};
    VkDeviceMemory textureImageMemory{VK_NULL_HANDLE};
    VkImageView    textureImageView{VK_NULL_HANDLE};
    VkSampler      textureSampler{VK_NULL_HANDLE};
    uint32_t       mipLevels{1};// mipmap等级

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    // 整个程序只需要一个depth Image，因为一次只有一个frame在跑，且swapchain不会自动生成depth image，需要自己写
    // 窗口改变时,分辨率改变，即与swapchain一同改变，因此也与其一同清除
    VkImage        depthImage{VK_NULL_HANDLE};
    VkDeviceMemory depthImageMemory{VK_NULL_HANDLE};
    VkImageView    depthImageView{VK_NULL_HANDLE};

    // for msaa
    // VkImage        colorImage;
    // VkDeviceMemory colorImageMemory;
    // VkImageView    colorImageView;

    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;// 32而不是16：因为会有比65535多很多的顶点

    std::vector<LR::VulkanBuffer> uniformBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;// signal that an image has been acquired from the swapchain and is ready for rendering
    std::vector<VkSemaphore> renderFinishedSemaphores;// signal that rendering has finished and presentation can happen
    std::vector<VkFence>     inFlightFences;          // make sure only one frame is rendering at a time

    // bool framebufferResized = false;// 显式处理窗口更改（以防有些电脑不支持自动响应窗口更改）

    std::array<VkClearValue, 2> clearValues{};  //depth & color clear value

    // ui handle
    LR::ImGUI* imGui;

    // 

    // 回滚函数为类的成员时，GLFW并不会使用this指针来指代这个类，因此设置为static函数(不能调用this)
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    // EXT表示extension（非vk核心部分，而是官方拓展，messenger这种的都要加），utils表示utilities（工具）
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,// 信息重要性
        VkDebugUtilsMessageTypeFlagsEXT             messageType,    // 信息类型
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,  // 包含message的结构体指针
        void*                                       pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    // initialize window configurations and inputs
    void initWindow();

    // compile new shader file  (注意使用绝对路径！)
    void compileShader();

    // initializations
    void initVulkan() {
        createInstance();     //创建实例（初始化vk library）
        setupDebugMessenger();//初始化debug messenger
        createSurface();
        pickPhysicalCard();   //选择physical device
        createLogicalDevice();//创建device来与上面进行interface
        initSwapChain();      //初始化swap chain（connect以及queue/format设置）
        createSyncObjects();
    }

    // preparations(common & unique)
    void prepare() {
        // correspond to VulkanExampleBase::prepare，基础设置
        setupSwapChain();
        createCommandPool();
        createCommandBuffers();
        createDepthResources();
        createRenderPass();
        createFramebuffers();
        setupImGUI();

        // correspond to VulkanExample::prepare，渲染不同的场景时的独特设置
        loadModel();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createTextureImage();//image, view, sampler
        setupDescriptors();
        createGraphicsPipeline();
        // createColorResources();//for multisample
        
        // 改成每帧record一次
        // recordCommandBuffers();
    }

    // receiving input & draw frames
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            // 交由程序进行输入的处理
            processInput(window);
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device);//等待logical device结束运行所有操作
    }

    // destroy vkObjects & other resources
    void cleanup();

    // functional funcs
    QueueFamilyIndices       findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails  querySwapChainSupport(VkPhysicalDevice device);
    uint32_t                 findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool                     isDeviceSuitable(VkPhysicalDevice device);
    void                     copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);// 暂时给image那里留着，等image类写好后删掉
    void                     setDeviceEnabledFeatures();
    void                     setDeviceEnabledExtensions();
    std::vector<const char*> getInstanceExtensions();
    void                     setupDebugMessenger();
    void                     populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static std::vector<char> readFile(const std::string& filename);               // 读入binary文件的helper function
    VkCommandBuffer          beginSingleTimeCommands();                           // 分配并开始单次的cmd录制
    void                     endSingleTimeCommands(VkCommandBuffer commandBuffer);// 结束并提交与释放（因为是单次）
    bool                     hasStencilComponent(VkFormat format);
    void                     loadModel();
    VkFormat                 findDepthFormat();
    VkFormat                 findSupportedFormat(const std::vector<VkFormat>& candidates,
                                                 VkImageTiling                tiling,
                                                 VkFormatFeatureFlags         features);

    void           pickPhysicalCard();
    void           createInstance();
    void           createLogicalDevice();
    void           createSurface();
    void           initSwapChain();
    void           setupSwapChain();
    void           cleanupSwapChain();
    void           recreateSwapChain();
    void           createFramebuffers();
    void           createRenderPass();
    void           setupImGUI();
    void           updateImGUI(float deltaTime);    // 交由Imgui进行输入处理
    void           createGraphicsPipeline();
    void           createCommandPool();
    void           createCommandBuffers();
    void           destoryCommandBuffers();
    // void           recordCommandBuffers();  // 用于初始设定好cmdBuffers，然后需要时再更新，不然就复用
    void           recordCommandBuffer(VkCommandBuffer& cmdBuffer, uint32_t imageIndex);
    VkShaderModule createShaderModule(const std::vector<char>& code);// 创建shader module的helper function
    void           createSyncObjects();
    void           createVertexBuffer();
    void           createIndexBuffer();
    void           createUniformBuffers();
    void           updateUniformBuffer(uint32_t currentImage);
    void           setupDescriptors();
    void           createTextureImage();
    void           createDepthResources();

    void createImage(uint32_t              width,
                     uint32_t              height,
                     uint32_t              mipLevels,
                     VkSampleCountFlagBits numSamples,
                     VkFormat              format,
                     VkImageTiling         tiling,
                     VkImageUsageFlags     usage,
                     VkMemoryPropertyFlags properties,
                     VkImage&              image,
                     VkDeviceMemory&       imageMemory);

    void transitionImageLayout(VkImage       image,
                               VkFormat      format,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               uint32_t      mipLevels);// perform image layout using a single command buffer

    VkImageView createImageView(VkImage            image,
                                VkFormat           format,
                                VkImageAspectFlags aspectFlags,
                                uint32_t           mipLevels);

    void generateMipmaps(VkImage  image,
                         VkFormat imageFormat,
                         int32_t  texWidth,
                         int32_t  texHeight,
                         uint32_t mipLevels);

    void drawFrame();

    // VkSampleCountFlagBits getMaxUsableSampleCount();
    // void createColorResources();  // 创建用于存放multisample的color image
};

#endif