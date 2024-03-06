#ifndef RENDER_BASE_H
#define RENDER_BASE_H

#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h>							//��������ĺ�󣬻��Զ�include vulkan.h

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

#define GLM_FORCE_RADIANS						//ȷ��glm������rotate�ĺ���ʹ�õĶ���radians�����ǽǶ�
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES		//ǿ��glmʹ���ڴ����Ҫ�󣬽����vec2�����ʹ��������⣨��������Զ������͵Ķ��룬�����û�����ʽ���룩
#define GLM_FORCE_DEPTH_ZERO_TO_ONE				//vulkan��depth��ΧΪ0��1��������opengl�е�-1��1
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

const int MAX_FRAMES_IN_FLIGHT = 2;	            //���ͬʱ�����frameBuffer����



void processInput(GLFWwindow* window);

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;		//ͼ�β���
	std::optional<uint32_t> presentFamily;		//������Ļ�ϵĲ���

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

	//������ô�ֳ�һ��������
    static VkVertexInputBindingDescription getBindingDescription();

    //����������ô�ֳɾ�����Щ����(VertexAttributePointer)
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();

    //unordered mapʹ�õ�equality test����
    bool operator==(const Vertex& other) const {
		return position == other.position && color == other.color && texCoord == other.texCoord;
	}
};

//ע�����Ҫ��(mat4�Ĵ�СΪ 4byte*(4��*4��) = 64byte)��ʹ��c++11��׼��alignas����֤����
//�ȽϺõ�������ȫ����ʾ�����ڴ�
struct ShaderData {
	//glm::vec2 foo;		//vec2��СΪ8byte�����º����mat��offset����Ϊ16��������
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};


//�����Ҫ��У����Ƿ����
bool checkValidationLayerSupport();

//����Ҫ����豸��չ�Ƿ���ã������������ַ����������У�
bool checkDeviceExtensionSupport(VkPhysicalDevice device);

//��Ϊ����չ������vkCreateDebugUtilsMessengerEXTû���Զ����أ���Ҫ�����Լ�д�ô���������װ�ؽ�vulkan
//PFN: pointer to function��vkGetInstanceProcAddr��ʵ���л�ȡ�������ֵĺ����ĵ�ַ����װ�ص�func��
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger);

//destroy debug messenger�ĺ���ͬ��
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator);

//unordered mapʹ�õ�hash��������Ͻṹ�������������һ����ϣ����
namespace std {
	template<> struct hash<Vertex> {
		size_t operator()(Vertex const& vertex) const {
			return ((hash<glm::vec3>()(vertex.position) ^
				(hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}

// //ע�����Ҫ��(mat4�Ĵ�СΪ 4byte*(4��*4��) = 64byte)��ʹ��c++11��׼��alignas����֤����
// //�ȽϺõ�������ȫ����ʾ�����ڴ�
// struct ShaderData {
// 	//glm::vec2 foo;		//vec2��СΪ8byte�����º����mat��offset����Ϊ16��������
// 	alignas(16) glm::mat4 model;
// 	alignas(16) glm::mat4 view;
// 	alignas(16) glm::mat4 projection;
// };


//����renderer�Ļ������Լ�����
class Renderer {
public:
    void run() {
        initWindow();
		compileShader();	//��������ʱ�Զ�compile shader
		initVulkan();
		mainLoop();
		cleanup();
    }

protected:
    GLFWwindow* window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;	        //���ڵ��ûص�����
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;			                        //logical device
	VkQueue graphicsQueue;		                        //to handle the graphics queue
	VkSurfaceKHR surface;		                        //���ڶ���
	VkQueue presentQueue;		                        //to handle the presentation queue
	VkSwapchainKHR swapChain;	                        //������
	std::vector<VkImage> swapChainImages;	            //�������е�ͼ��
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;		                    //���泤��
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
	uint32_t mipLevels;		                            //mipmap�ȼ�
	VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
	VkImage colorImage;
	VkDeviceMemory colorImageMemory;
	VkImageView colorImageView;

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;		//32������16����Ϊ���б�65535��ܶ�Ķ���

	std::vector<VkBuffer> uniformBuffers;		//Ϊ�˶�����in flight��frames�ֿ����м�¼ubo
	std::vector<VkDeviceMemory> uniformBuffersMemory;
	std::vector<void*> uniformBuffersMapped;	//��ɶ��ָ��ӳ���ڴ������ָ���vector�����Ǹ�data����һ��

	std::vector<VkSemaphore> imageAvailableSemaphores;	//signal that an image has been acquired from the swapchain and is ready for rendering
	std::vector<VkSemaphore> renderFinishedSemaphores;	//signal that rendering has finished and presentation can happen
	std::vector<VkFence> inFlightFences;		//make sure only one frame is rendering at a time

	bool framebufferResized = false;		//��ʽ�����ڸ���

    //�ع�����Ϊ��ĳ�Աʱ��GLFW������ʹ��thisָ����ָ������࣬�������Ϊstatic����(���ܵ���this)
    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

    //run�еĳ�Ա
    void initWindow();          //initialize window configurations and inputs
    void compileShader();       //compile new shader file  (ע��ʹ�þ���·����)
    void initVulkan();          //preparation
    void mainLoop();            //receiving input & draw frames
    void cleanup();             //destroy vkObjects & other resources


    void createInstance();

    std::vector<const char*> getRequiredExtensions();

    //EXT��ʾextension����vk���Ĳ��֣����ǹٷ���չ��messenger���ֵĶ�Ҫ�ӣ���utils��ʾutilities�����ߣ�
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,    // ��Ϣ��Ҫ��
        VkDebugUtilsMessageTypeFlagsEXT messageType,               // ��Ϣ����
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, // ����message�Ľṹ��ָ��
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

    //conditions for "swapping" images to the screen�������ֹ����˺�ѸУ�
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    //resolution of images in swap chain
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    void createSwapChain();

    //Ϊ��ʹ��vkImage�����봴��ImageView������������ô��ȡͼ�񣬴���ͼ���ʲô���֣�mipmap�ȣ�
    void createImageViews();

    //����binary�ļ���helper function
    static std::vector<char> readFile(const std::string &filename);

    //����shader module��helper function
	VkShaderModule createShaderModule(const std::vector<char>& code);

    void createGraphicsPipeline();

    //specify how many color and depth buffers there will be, 
	// how many samples to use for each of them, 
	// and how their contents should be handled throughout the rendering operations
    void createRenderPass();

    //Ϊswapchain�е�����image��Ӧ��imageview(attachment)����framebuffer
    void createFramebuffers();

    void createCommandPool();

    void createCommandBuffers();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void createSyncObjects();

    void cleanupSwapChain();

    void recreateSwapChain();

    void createVertexBuffer();

    //�ҵ����ʵ�memoryType����graphics card���з���
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

    //���䲢��ʼ���ε�cmd¼��
	VkCommandBuffer beginSingleTimeCommands();

    //�������ύ���ͷţ���Ϊ�ǵ��Σ�
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    //perform image layout transitions������Imageʱ��Ҫ���⿼��layout�Ĺ������⣩
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

    //�������ڴ��multisample��color image
	void createColorResources();

    void drawFrame();
};

#endif