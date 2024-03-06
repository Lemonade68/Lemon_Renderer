#ifndef RENDER_BASE_H
#define RENDER_BASE_H

#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h>							//定义上面的宏后，会自动include vulkan.h

#include<iostream>
#include<cstdlib>
#include<vector>
#include<cstring>
#include<map>
#include<optional>
#include<set>
#include<limits>
#include<algorithm>
#include<fstream>
#include<array>
#include<chrono>
#include<unordered_map>

#define GLM_FORCE_RADIANS						//确保glm中类似rotate的函数使用的都是radians而不是角度
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES		//强制glm使用内存对齐要求，解决如vec2等类型带来的问题（解决不了自定义类型的对齐，因此最好还是显式对齐）
#define GLM_FORCE_DEPTH_ZERO_TO_ONE				//vulkan中depth范围为0到1，而不是opengl中的-1到1
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "base/Camera.h"


const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif // NDEBUG

const int SCR_WIDTH = 800;
const int SCR_HEIGHT = 600;

const std::string MODEL_PATH = "../../../assets/models/viking_room/viking_room.obj";
const std::string TEXTURE_PATH = "../../../assets/models/viking_room/viking_room.png";

const int MAX_FRAMES_IN_FLIGHT = 2;	            //最大同时处理的frameBuffer数量



void processInput(GLFWwindow* window);

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;		//图形操作
	std::optional<uint32_t> presentFamily;		//画到屏幕上的操作

	bool isComplete() {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;			//Basic surface capabilities
	std::vector<VkSurfaceFormatKHR> formats;		//Surface formats(image format & color space)
	std::vector<VkPresentModeKHR> presentModes;		//Available presentation modes
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 color;
	glm::vec2 texCoord;

	//数据怎么分成一个个顶点
    static VkVertexInputBindingDescription getBindingDescription();

    //顶点数据怎么分成具体哪些部分(VertexAttributePointer)
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();

    //unordered map使用的equality test函数
    bool operator==(const Vertex& other) const {
		return position == other.position && color == other.color && texCoord == other.texCoord;
	}
};

//注意对齐要求(mat4的大小为 4byte*(4行*4列) = 64byte)，使用c++11标准的alignas来保证对齐
//比较好的做法：全都显示对齐内存
struct ShaderData {
	//glm::vec2 foo;		//vec2大小为8byte，导致后面的mat的offset都不为16的整数倍
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};


//检测需要的校验层是否可用
bool checkValidationLayerSupport();

//检验要求的设备拓展是否可用（与上面是两种方法，都可行）
bool checkDeviceExtensionSupport(VkPhysicalDevice device);

//因为是拓展，所以vkCreateDebugUtilsMessengerEXT没有自动加载，需要我们自己写好代理函数后将其装载进vulkan
//PFN: pointer to function，vkGetInstanceProcAddr从实例中获取后面名字的函数的地址，并装载到func上
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger);

//destroy debug messenger的函数同上
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator);

//unordered map使用的hash函数，结合结构体的内容来创建一个哈希函数
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

// //注意对齐要求(mat4的大小为 4byte*(4行*4列) = 64byte)，使用c++11标准的alignas来保证对齐
// //比较好的做法：全都显示对齐内存
// struct ShaderData {
// 	//glm::vec2 foo;		//vec2大小为8byte，导致后面的mat的offset都不为16的整数倍
// 	alignas(16) glm::mat4 model;
// 	alignas(16) glm::mat4 view;
// 	alignas(16) glm::mat4 projection;
// };


//所有renderer的基础属性及方法
class Renderer {
public:
    void run() {
        initWindow();
		compileShader();	//程序运行时自动compile shader
		initVulkan();
		mainLoop();
		cleanup();
    }

protected:
    GLFWwindow* window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;	        //用于调用回调函数
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;			                        //logical device
	VkQueue graphicsQueue;		                        //to handle the graphics queue
	VkSurfaceKHR surface;		                        //窗口对象
	VkQueue presentQueue;		                        //to handle the presentation queue
	VkSwapchainKHR swapChain;	                        //交换链
	std::vector<VkImage> swapChainImages;	            //交换链中的图像
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;		                    //画面长宽
	std::vector<VkImageView> swapChainImageViews;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;
	VkImage textureImage;
	VkDeviceMemory textureImageMemory;
	VkImageView textureImageView;
	VkSampler textureSampler;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;
	uint32_t mipLevels;		                            //mipmap等级
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	VkImage colorImage;
	VkDeviceMemory colorImageMemory;
	VkImageView colorImageView;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;		//32而不是16：因为会有比65535多很多的顶点

	std::vector<VkBuffer> uniformBuffers;		//为了对所有in flight的frames分开进行记录ubo
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;	//干啥？指向映射内存区域的指针的vector，和那个data性质一样

	std::vector<VkSemaphore> imageAvailableSemaphores;	//signal that an image has been acquired from the swapchain and is ready for rendering
	std::vector<VkSemaphore> renderFinishedSemaphores;	//signal that rendering has finished and presentation can happen
	std::vector<VkFence> inFlightFences;		//make sure only one frame is rendering at a time

	bool framebufferResized = false;		//显式处理窗口更改

    //回滚函数为类的成员时，GLFW并不会使用this指针来指代这个类，因此设置为static函数(不能调用this)
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

    //run中的成员
    void initWindow();          //initialize window configurations and inputs
    void compileShader();       //compile new shader file  (注意使用绝对路径！)
    void initVulkan();          //preparation
    void mainLoop();            //receiving input & draw frames
    void cleanup();             //destroy vkObjects & other resources


    void createInstance();

    std::vector<const char*> getRequiredExtensions();

    //EXT表示extension（非vk核心部分，而是官方拓展，messenger这种的都要加），utils表示utilities（工具）
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,    // 信息重要性
        VkDebugUtilsMessageTypeFlagsEXT messageType,               // 信息类型
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, // 包含message的结构体指针
        void *pUserData){
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	    return VK_FALSE;
    }
    
    void setupDebugMessenger();

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

    void pickPhysicalCard();

    bool isDeviceSuitable(VkPhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    void createLogicalDevice();

    void createSurface();

    //Querying details of swap chain support
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    //Choosing the right settings for the swap chain(color depth formats)
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    //conditions for "swapping" images to the screen（如何阻止画面撕裂感）
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    //resolution of images in swap chain
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    void createSwapChain();

    //为了使用vkImage，必须创建ImageView对象（描述了怎么获取图像，处理图像的什么部分，mipmap等）
    void createImageViews();

    //读入binary文件的helper function
    static std::vector<char> readFile(const std::string &filename);

    //创建shader module的helper function
	VkShaderModule createShaderModule(const std::vector<char>& code);

    void createGraphicsPipeline();

    //specify how many color and depth buffers there will be, 
	// how many samples to use for each of them, 
	// and how their contents should be handled throughout the rendering operations
    void createRenderPass();

    //为swapchain中的所有image对应的imageview(attachment)创建framebuffer
    void createFramebuffers();

    void createCommandPool();

    void createCommandBuffers();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();

    void cleanupSwapChain();

    void recreateSwapChain();

    void createVertexBuffer();

    //找到合适的memoryType来让graphics card进行分配
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createBuffer(VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer &buffer,
                      VkDeviceMemory &bufferMemory);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    void createIndexBuffer();

    void createUniformBuffers();

    void updateUniformBuffer(uint32_t currentImage);

    void createDescriptorSetLayout();

    void createDescriptorPool();

    void createDescriptorSet();

    void createImage(uint32_t width,
                     uint32_t height,
                     uint32_t mipLevels,
                     VkSampleCountFlagBits numSamples,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkImage &image,
                     VkDeviceMemory &imageMemory);

    void createTextureImage();

    //分配并开始单次的cmd录制
	VkCommandBuffer beginSingleTimeCommands();

    //结束并提交与释放（因为是单次）
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    //perform image layout transitions（处理Image时需要额外考虑layout的过渡问题）
    void transitionImageLayout(VkImage image,
                               VkFormat format,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout,
                               uint32_t mipLevels);

    void copyBufferToImage(VkBuffer buffer,
                           VkImage image,
                           uint32_t width,
                           uint32_t height);

    VkImageView createImageView(VkImage image,
                                VkFormat format,
                                VkImageAspectFlags aspectFlags,
                                uint32_t mipLevels);

    void createTextureImageView();

    void createTextureSampler();

    void createDepthResources();

    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates,
                                 VkImageTiling tiling,
                                 VkFormatFeatureFlags features);

    VkFormat findDepthFormat();

    bool hasStencilComponent(VkFormat format);

    void loadModel();

    void generateMipmaps(VkImage image,
                         VkFormat imageFormat,
                         int32_t texWidth,
                         int32_t texHeight,
                         uint32_t mipLevels);

    VkSampleCountFlagBits getMaxUsableSampleCount();

    //创建用于存放multisample的color image
	void createColorResources();

    void drawFrame();
};

#endif