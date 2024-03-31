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

// ע�����Ҫ��(mat4�Ĵ�СΪ 4byte*(4��*4��) = 64byte)��ʹ��c++11��׼��alignas����֤���� ���� �ȽϺõ�������ȫ����ʾ�����ڴ�
struct ShaderData {
    // glm::vec2 foo;		//vec2��СΪ8byte�����º����mat��offset����Ϊ16��������
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 projection;
};

bool checkValidationLayerSupport();
bool checkDeviceExtensionSupport(VkPhysicalDevice device);

// ��Ϊ����չ������vkCreateDebugUtilsMessengerEXTû���Զ����أ���Ҫ�����Լ�д�ô���������װ�ؽ�vulkan
// PFN: pointer to function��vkGetInstanceProcAddr��ʵ���л�ȡ�������ֵĺ����ĵ�ַ����װ�ص�func��
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
void     DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

// unordered mapʹ�õ�hash��������Ͻṹ�������������һ����ϣ����
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

// ����ͷ�ļ��໥��������Ҫ�໥ǰ������
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
    LR::VulkanDevice*        vulkanDevice;// ����Ϊָ�룬�Ӷ����Կ��ƺ�ʱ�������٣�����������������
    VkPhysicalDevice         physicalDevice{VK_NULL_HANDLE};
    VkDevice                 device{VK_NULL_HANDLE};
    VkPhysicalDeviceFeatures enabledDeviceFeatures{};//Set of physical device features to be enabled
    std::vector<const char*> enabledDeviceExtensions;

    GLFWwindow*              window{nullptr};
    VkSurfaceKHR             surface{VK_NULL_HANDLE};// ���ڶ���
    VkInstance               instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger{VK_NULL_HANDLE};// ���ڵ��ûص�����
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

    // textures��֮�������gltf�Լ�ktx��ʱ���ٻ�
    VkImage        textureImage{VK_NULL_HANDLE};
    VkDeviceMemory textureImageMemory{VK_NULL_HANDLE};
    VkImageView    textureImageView{VK_NULL_HANDLE};
    VkSampler      textureSampler{VK_NULL_HANDLE};
    uint32_t       mipLevels{1};// mipmap�ȼ�

    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    // ��������ֻ��Ҫһ��depth Image����Ϊһ��ֻ��һ��frame���ܣ���swapchain�����Զ�����depth image����Ҫ�Լ�д
    // ���ڸı�ʱ,�ֱ��ʸı䣬����swapchainһͬ�ı䣬���Ҳ����һͬ���
    VkImage        depthImage{VK_NULL_HANDLE};
    VkDeviceMemory depthImageMemory{VK_NULL_HANDLE};
    VkImageView    depthImageView{VK_NULL_HANDLE};

    // for msaa
    // VkImage        colorImage;
    // VkDeviceMemory colorImageMemory;
    // VkImageView    colorImageView;

    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;// 32������16����Ϊ���б�65535��ܶ�Ķ���

    std::vector<LR::VulkanBuffer> uniformBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;// signal that an image has been acquired from the swapchain and is ready for rendering
    std::vector<VkSemaphore> renderFinishedSemaphores;// signal that rendering has finished and presentation can happen
    std::vector<VkFence>     inFlightFences;          // make sure only one frame is rendering at a time

    // bool framebufferResized = false;// ��ʽ�����ڸ��ģ��Է���Щ���Բ�֧���Զ���Ӧ���ڸ��ģ�

    std::array<VkClearValue, 2> clearValues{};  //depth & color clear value

    // ui handle
    LR::ImGUI* imGui;

    // 

    // �ع�����Ϊ��ĳ�Աʱ��GLFW������ʹ��thisָ����ָ������࣬�������Ϊstatic����(���ܵ���this)
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    // EXT��ʾextension����vk���Ĳ��֣����ǹٷ���չ��messenger���ֵĶ�Ҫ�ӣ���utils��ʾutilities�����ߣ�
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,// ��Ϣ��Ҫ��
        VkDebugUtilsMessageTypeFlagsEXT             messageType,    // ��Ϣ����
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,  // ����message�Ľṹ��ָ��
        void*                                       pUserData) {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    // initialize window configurations and inputs
    void initWindow();

    // compile new shader file  (ע��ʹ�þ���·����)
    void compileShader();

    // initializations
    void initVulkan() {
        createInstance();     //����ʵ������ʼ��vk library��
        setupDebugMessenger();//��ʼ��debug messenger
        createSurface();
        pickPhysicalCard();   //ѡ��physical device
        createLogicalDevice();//����device�����������interface
        initSwapChain();      //��ʼ��swap chain��connect�Լ�queue/format���ã�
        createSyncObjects();
    }

    // preparations(common & unique)
    void prepare() {
        // correspond to VulkanExampleBase::prepare����������
        setupSwapChain();
        createCommandPool();
        createCommandBuffers();
        createDepthResources();
        createRenderPass();
        createFramebuffers();
        setupImGUI();

        // correspond to VulkanExample::prepare����Ⱦ��ͬ�ĳ���ʱ�Ķ�������
        loadModel();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createTextureImage();//image, view, sampler
        setupDescriptors();
        createGraphicsPipeline();
        // createColorResources();//for multisample
        
        // �ĳ�ÿ֡recordһ��
        // recordCommandBuffers();
    }

    // receiving input & draw frames
    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            // ���ɳ����������Ĵ���
            processInput(window);
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device);//�ȴ�logical device�����������в���
    }

    // destroy vkObjects & other resources
    void cleanup();

    // functional funcs
    QueueFamilyIndices       findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails  querySwapChainSupport(VkPhysicalDevice device);
    uint32_t                 findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool                     isDeviceSuitable(VkPhysicalDevice device);
    void                     copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);// ��ʱ��image�������ţ���image��д�ú�ɾ��
    void                     setDeviceEnabledFeatures();
    void                     setDeviceEnabledExtensions();
    std::vector<const char*> getInstanceExtensions();
    void                     setupDebugMessenger();
    void                     populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
    static std::vector<char> readFile(const std::string& filename);               // ����binary�ļ���helper function
    VkCommandBuffer          beginSingleTimeCommands();                           // ���䲢��ʼ���ε�cmd¼��
    void                     endSingleTimeCommands(VkCommandBuffer commandBuffer);// �������ύ���ͷţ���Ϊ�ǵ��Σ�
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
    void           updateImGUI(float deltaTime);    // ����Imgui�������봦��
    void           createGraphicsPipeline();
    void           createCommandPool();
    void           createCommandBuffers();
    void           destoryCommandBuffers();
    // void           recordCommandBuffers();  // ���ڳ�ʼ�趨��cmdBuffers��Ȼ����Ҫʱ�ٸ��£���Ȼ�͸���
    void           recordCommandBuffer(VkCommandBuffer& cmdBuffer, uint32_t imageIndex);
    VkShaderModule createShaderModule(const std::vector<char>& code);// ����shader module��helper function
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
    // void createColorResources();  // �������ڴ��multisample��color image
};

#endif