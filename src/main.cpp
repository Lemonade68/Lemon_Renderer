#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h>							//��������ĺ�󣬻��Զ�include vulkan.h
// #include<vulkan/vulkan.h>

#include<iostream>
#include<stdexcept>
#include<cstdlib>
#include<vector>
#include<cstring>
#include<map>
#include<optional>
#include<set>
#include<limits>
#include<algorithm>
#include<fstream>
#include<glm/glm.hpp>
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


#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include<tinyobjloader/tiny_obj_loader.h>

void processInput(GLFWwindow* window);

const int SCR_WIDTH = 800;
const int SCR_HEIGHT = 600;

const std::string MODEL_PATH = "../../../assets/models/viking_room/viking_room.obj";
const std::string TEXTURE_PATH = "../../../assets/models/viking_room/viking_room.png";

const int MAX_FRAMES_IN_FLIGHT = 2;	//���ͬʱ�����frameBuffer����
uint32_t currentFrame = 0;			//��ǰ�����frame

float currentTime = 0, lastTime = 0, deltaTime = 0;

class Camera {
private:
	glm::vec3 position;
	glm::vec3 worldUp{ 0.f, 1.f, 0.f };

public:
	struct {
		glm::vec3 right;	//x��cam���ҷ�
		glm::vec3 up;		//y����������Ϸ�
		glm::vec3 back;		//z����Ӧ���������ķ�����
	} cameraWorldCoords;

	bool enterCam = true;
	bool firstEnter = true;

	float fov_y;
	float zNear, zFar;
	float aspect_ratio;
	float pitch = 0.f, yaw = -90.f;				//��������ƫ����(��ʼָ��z��)

	enum class CameraType {
		firstperson,
		lookat
	};
	CameraType type;		

	struct {
		glm::mat4 view;				//view����
		glm::mat4 perspective;		//perspective����
	} matrices;

	struct {
		bool left = false;
		bool right = false;
		bool forward = false;
		bool backward = false;
		bool up = false;
		bool down = false;
	} keys;

	float movementSpeed = 2.f;
	float rotationSpeed = 0.4f;

public:
	Camera(glm::vec3 _position = glm::vec3(0.f, 0.f, 3.f), float _fov = 60.f, float _near = 0.1f, float _far = 256.f, Camera::CameraType _type = Camera::CameraType::firstperson) 
			: type(_type), position(_position), fov_y(_fov), zNear(_near), zFar(_far) {
		glm::vec3 target(0.f);						//��ʼʱ����ԭ��
		cameraWorldCoords.back = glm::normalize(position - target);
		cameraWorldCoords.right = glm::normalize(glm::cross(worldUp, cameraWorldCoords.back));
		cameraWorldCoords.up = glm::normalize(glm::cross(cameraWorldCoords.back, cameraWorldCoords.right));
		updateViewMatrix();
		//updatePerspectiveMatrix(fov_y, SCR_WIDTH / SCR_HEIGHT, zNear, zFar);		//����swapchainʱ��ʼ��
	}

	void updateViewMatrix() {
		if (type == CameraType::firstperson) {
			matrices.view = glm::lookAt(position, position - cameraWorldCoords.back, worldUp);
		}
		else {
			//lookat camera
		}
	}

	void updatePerspectiveMatrix(float fov, float aspect, float znear, float zfar) {
		fov_y = fov;
		zNear = znear;
		zFar = zfar;
		aspect_ratio = aspect;
		matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);

		//Ĭ��ʹ��vulkan����ҪflipY
		matrices.perspective[1][1] *= -1.0f;
	}

	void Tick(float deltaTime) {
		if (enterCam) {
			if (type == CameraType::firstperson) {
				//�޸ĳ�����Ϣ
				cameraWorldCoords.right = glm::normalize(glm::cross(worldUp, cameraWorldCoords.back));
				cameraWorldCoords.up = glm::normalize(glm::cross(cameraWorldCoords.back, cameraWorldCoords.right));

				//�޸�λ����Ϣ
				float moveSpeed = deltaTime * movementSpeed;
				if (keys.forward)
					position -= cameraWorldCoords.back * moveSpeed;
				if (keys.backward)
					position += cameraWorldCoords.back * moveSpeed;
				if (keys.left)
					position -= cameraWorldCoords.right * moveSpeed;
				if (keys.right)
					position += cameraWorldCoords.right * moveSpeed;
				if (keys.up)
					position += cameraWorldCoords.up * moveSpeed;
				if (keys.down)
					position -= cameraWorldCoords.up * moveSpeed;

				updateViewMatrix();		//�޸�camera����Ϣ�ᵼ��view matrix���ؽ�������ֻ�е�fov��aspect_ratio�ı�ʱ�Ż�Ӱ��perspectiveMatrix
			}
		}
	}
};

Camera camera;


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

//�����Ҫ��У����Ƿ����
bool checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	//���
	for (const char* layerName : validationLayers) {
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound)
			return false;
	}
	return true;
}

//����Ҫ����豸��չ�Ƿ���ã������������ַ����������У�
bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	//�����Ҫ����չ�Ƿ񱻿�����չȫ������
	std::set<std::string> unconfirmedRequiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto& extension : availableExtensions)
		unconfirmedRequiredExtensions.erase(extension.extensionName);

	return unconfirmedRequiredExtensions.empty();
}

//��Ϊ����չ������vkCreateDebugUtilsMessengerEXTû���Զ����أ���Ҫ�����Լ�д�ô���������װ�ؽ�vulkan
//PFN: pointer to function
//vkGetInstanceProcAddr��ʵ���л�ȡ�������ֵĺ����ĵ�ַ����װ�ص�func��
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		//װ�سɹ�
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

}

//destroy debug messenger�ĺ���ͬ��
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, pAllocator);
}

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
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;		//��Ӧ�İ����������������е��±ֻ꣨��һ�����飬���Ϊ0��
		bindingDescription.stride = sizeof(Vertex);		//����
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		//Move to the next data entry after each vertex
		return bindingDescription;
	}

	//����������ô�ֳɾ�����Щ����(VertexAttributePointer)
	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {	//2����λ�ú���ɫ��������
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;	//ͬ��
		attributeDescriptions[0].location = 0;	//ͬvertex shader�е�location��Ӧ
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;	//��Ӧvec3
		attributeDescriptions[0].offset = offsetof(Vertex, position);	//offsetof���ؽṹ���Ա�ڽṹ���е�ƫ����

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;	//��Ӧvec3
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}

	//unordered mapʹ�õ�equality test����
	bool operator==(const Vertex& other) const {
		return position == other.position && color == other.color && texCoord == other.texCoord;
	}
};

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


//const std::vector<Vertex> vertices = {
//	{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
//	{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
//	{{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
//	{{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
//
//	{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
//	{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
//	{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
//	{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
//};
//
//const std::vector<uint16_t> indices = {		//uint15��uint32������
//	0, 1, 2, 2, 3, 0,
//	4, 5, 6, 6, 7, 4
//};

//ע�����Ҫ��(mat4�Ĵ�СΪ 4byte*(4��*4��) = 64byte)��ʹ��c++11��׼��alignas����֤����
//�ȽϺõ�������ȫ����ʾ�����ڴ�
struct UniformBufferObject {
	//glm::vec2 foo;		//vec2��СΪ8byte�����º����mat��offset����Ϊ16��������
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		compileShader();	//��������ʱ�Զ�compile shader
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;	//���ڵ��ûص�����
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;			//logical device
	VkQueue graphicsQueue;		//to handle the graphics queue
	VkSurfaceKHR surface;		//���ڶ���
	VkQueue presentQueue;		//to handle the presentation queue
	VkSwapchainKHR swapChain;	//������
	std::vector<VkImage> swapChainImages;	//�������е�ͼ��
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;		//���泤��
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
	uint32_t mipLevels;		//mipmap�ȼ�
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

	void initWindow() {
		glfwInit();		//����glfw lib
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	//��ֹ����openGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);		//������Ĵ��ڴ�С
		window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);			//�ú������Դ������һ��ָ�룬���this����
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);	//Resize�˾ͻ����framebufferResizeCallback����

		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, mouseCallback);
		glfwSetScrollCallback(window, scrollCallback);

	}

	//GLFW������ʹ��thisָ����ָ������࣬�������Ϊstatic����(���ܵ���this)
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;		//��ʽ����Ϊtrue
		//��������������ô����ڱ仯ʱҲ�ܸ��ż�ʹ�仯��glfw��resizeʱ����ͣevent loop��
		//�������ÿ�ƶ�һ�㶼��������call back����������ӴӶ��ﵽʵʱ�ı��Ч��
		app->recreateSwapChain();	//��һ������recreate swap chain
		app->drawFrame();			//�ڶ�������ʵ�ʵ���Ⱦ
	}

	static void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
		static float lastX;
		static float lastY;
		if (camera.enterCam) {
			if (camera.firstEnter) {
				lastX = xpos;
				lastY = ypos;
				camera.firstEnter = false;
			}

			float xoffset = xpos - lastX;
			float yoffset = lastY - ypos;
			lastX = xpos;
			lastY = ypos;

			xoffset *= camera.rotationSpeed;
			yoffset *= camera.rotationSpeed;

			camera.yaw += xoffset;
			camera.pitch += yoffset;

			if (camera.pitch > 89.f)
				camera.pitch = 89.f;
			if (camera.pitch < -89.f)
				camera.pitch = -89.f;
		
			glm::vec3 front;
			front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
			front.y = sin(glm::radians(camera.pitch));
			front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
		
			camera.cameraWorldCoords.back = glm::normalize(-front);
		}
	}

	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
		if (camera.enterCam) {
			if (camera.fov_y >= 1.0f && camera.fov_y <= 100.0f)
				camera.fov_y -= yoffset * 1.5f;
			if (camera.fov_y <= 1.0f)
				camera.fov_y = 1.0f;
			if (camera.fov_y >= 100.0f)
				camera.fov_y = 100.0f;
			camera.updatePerspectiveMatrix(camera.fov_y, camera.aspect_ratio, camera.zNear, camera.zFar);
		}
	}

	//compile new shader file  (ע��ʹ�þ���·����)
	void compileShader() {
		const char* batchFilePath = "C:/Users/Lemonade/Desktop/Lemon_Render/shaders/compile.bat";
		if (system(batchFilePath) != 0)
			throw std::runtime_error("failed to execute shader batch file!");
	}

	void initVulkan() {
		createInstance();		//����ʵ������ʼ��vk library��
		setupDebugMessenger();	//��ʼ��debug messenger
		createSurface();
		pickPhysicalCard();		//ѡ��physical device
		createLogicalDevice();	//����device�����������interface
		createSwapChain();		//����swap chain
		createImageViews();		//��ֱ��ʹ��image��������Ҫimage view
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool();
		createColorResources();	//for multisample
		createDepthResources();
		createFramebuffers();
		createTextureImage();	//load image into vulkan image object
		createTextureImageView();
		createTextureSampler();		//����filtering & transformations to compute final color 
		loadModel();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSet();
		createCommandBuffers();
		createSyncObjects();
	}

	void mainLoop() {
		while (!glfwWindowShouldClose(window)) {
			processInput(window);		//���Ͼ������⣬Ϊɶ��
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle(device);	//�ȴ�logical device�����������в���
	}

	//�ֶ������Ķ���Ҫdestroy�������˳���д����
	void cleanup() {
		cleanupSwapChain();

		vkDestroySampler(device, textureSampler, nullptr);
		vkDestroyImageView(device, textureImageView, nullptr);

		vkDestroyImage(device, textureImage, nullptr);
		vkFreeMemory(device, textureImageMemory, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
			vkDestroyBuffer(device, uniformBuffers[i], nullptr);
			vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
		}

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);

		vkDestroyBuffer(device, indexBuffer, nullptr);
		vkFreeMemory(device, indexBufferMemory, nullptr);

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		vkDestroyRenderPass(device, renderPass, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(device, commandPool, nullptr);

		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers)
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

		vkDestroySurfaceKHR(instance, surface, nullptr);	//���ʵ��ǰ���surface����
		vkDestroyInstance(instance, nullptr);	//���ʵ��

		glfwDestroyWindow(window);

		glfwTerminate();
	}

	//���ã�ָ��application info��extension info
	void createInstance() {
		//validation layer���
		if (enableValidationLayers && !checkValidationLayerSupport())
			throw std::runtime_error("validation layers requested, but not available!");

		VkApplicationInfo appInfo{};		//�Ǳ��룬��֮������õ��ض����棬���������Ż�
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};		//����Ľṹ�壬����driver��Ҫʹ�õ�ȫ����չ��У���
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		//��ȡ��֧�ֵ���չ����ѡ��
		//uint32_t extensionCount = 0;
		//vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);	//��ȡ��֧�ֵ���չ�ĸ���
		//std::vector<VkExtensionProperties> extensions_ava(extensionCount);
		//vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions_ava.data());
		//std::cout << "available extensions:\n";
		//for (const auto& extension : extensions_ava) {
		//	std::cout << '\t' << extension.extensionName << '\n';
		//}

		//ָ����Ҫ��ȫ����չ(global extensions)  �Ľ���
		auto extensions = getRequiredExtensions();		//Ŀǰ�� glfw + validation layer
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		//ָ��ȫ��У���(global validation layers)
		// + vkCreateInstance/vkDestroyInstance ��message��ӡ��debug����ȫû������
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			//����vulkan���ڴ���ʵ����ͬʱ����һ��debug�ص���pNext�����ݶ������Ϣ���Ա�����չ�������ض�����
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		//���������б�Ҫ��Ϣ�����������vkCreateInstance������vkʵ����instance�У�
		//��ͨ������ֵ(����ΪVkResult)��ȷ���Ƿ�ɹ�ִ��
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		//glfwָ������չ����ʹ��
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		//����Ҫ��extensionsת��Ϊ��Ҫ��extension��vector��������������
		//����ʹ�õ�����ʼ�������ͽ����������Ĺ��췽��
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		//У������չ����ѡ���Ƿ����
		if (enableValidationLayers)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);		//��������һ���ַ���

		return extensions;
	}

	//EXT��ʾextension����vk���Ĳ��֣����ǹٷ���չ��messenger���ֵĶ�Ҫ�ӣ���utils��ʾutilities�����ߣ�
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,		//��Ϣ��Ҫ��
		VkDebugUtilsMessageTypeFlagsEXT messageType,				//��Ϣ����
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,	//����message�Ľṹ��ָ��
		void* pUserData) {

		std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

		return VK_FALSE;
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;
		VkDebugUtilsMessengerCreateInfoEXT createInfo{};
		populateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
			throw std::runtime_error("failed to set up debug messenger!");
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		//ָ���ص������������Ϣ����
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		//ָ���ص������������Ϣ����
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		//ָ��ָ��ص�������ָ��
		createInfo.pfnUserCallback = debugCallback;
	}

	void pickPhysicalCard() {
		//listing graphics cards
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		if (deviceCount == 0)
			throw std::runtime_error("failed to find GPUs with vulkan support!");
		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices) {
			if (isDeviceSuitable(device)) {
				physicalDevice = device;
				msaaSamples = getMaxUsableSampleCount();
				break;
			}
		}
		if (physicalDevice == VK_NULL_HANDLE) {
			throw std::runtime_error("failed to find a suitable GPU!");
		}
	}

	bool isDeviceSuitable(VkPhysicalDevice device) {
		//�����physical device��û�ж�Ӧ��֧��ͼ�β����Ķ����岢��¼
		QueueFamilyIndices indices = findQueueFamilies(device);

		//�����physical device֧��֧�ֶ�Ӧ����չ
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		//���extension�е�swap chain�Ƿ�adequate(����֧��һ��ͼƬ��ʽ��һ�ֳ�����ʽ)
		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		//����Ƿ�֧��anisotropic filtering
		VkPhysicalDeviceFeatures supportedFeatures;
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		QueueFamilyIndices indices;
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		VkBool32 presentSupport = false;

		//�����ж��������ҵ�֧��queueFamily��Ҫ������Ķ�������壬�ֱ��¼��index
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)	//ͨ��λ�����ж�
				indices.graphicsFamily = i;

			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport)
				indices.presentFamily = i;

			if (indices.isComplete())		//�ҵ��˾�ֱ���˳�����
				break;
			++i;
		}

		return indices;
	}

	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		//��������壬ֱ��ʹ��vector�����в���
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		//ʹ��set�ĺô�������������±���ͬ����ֻ���������һ���±��set
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.f;  //�������ȼ�
		for (auto queueFamily : uniqueQueueFamilies) {
			//Specifying the queues to be created
			VkDeviceQueueCreateInfo queueCreateInfo{};		//����ϣ��һ���������еĶ��е�����
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;		//p��ʾpointer
			queueCreateInfos.push_back(queueCreateInfo);
		}

		//Specifying used device features
		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;		//����ʹ��anisotropic filtering
		deviceFeatures.sampleRateShading = VK_TRUE; // enable sample shading feature for the device

		//creating logical device
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();		//����Ϊvector������
		createInfo.pEnabledFeatures = &deviceFeatures;

		//���沿����ʵ������������չ��У���Ĳ�������
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());			//����swapchain���ּ���������������
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		//���ڲ�����ʵ�����豸���У��㣬���Բ��ã���Ϊ����ɰ汾���ݣ����Ǽ���
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
			throw std::runtime_error("failed to create logical device!");

		//�ڶ��������Ƕ�������±꣬�����������Ǹö������еĶ�Ӧ�����±�
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	}

	void createSurface() {
		//ֱ����glfw��ʵ�֣����Ҫ�Լ�ʵ�ֵĻ���Ҫ�õ�windows�е���ز��������̳̣�
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
			throw std::runtime_error("failed to create window surface!");
	}

	//Querying details of swap chain support
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;

		//����swap chain��ѯ����������device��surface�������ؼ�����
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);	//get capabilities

		uint32_t formatCount;	//get format
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;	//get presentMode
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	//Choosing the right settings for the swap chain
	//color depth
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
		for (const auto& availableFormat : availableFormats) {
			//�ҵ�֧��SRGB�ľͷ��أ�һ����˵��һ���ͺã�
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return availableFormat;
		}
		return availableFormats[0];		//�Ҳ�����Ҫ�ĸ�ʽ��ֱ�ӷ��ص�һ����ͦ��
	}

	//conditions for "swapping" images to the screen�������ֹ����˺�ѸУ�
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
		for (const auto& availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return availablePresentMode;
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	//resolution of images in swap chain
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
			return capabilities.currentExtent;
		else {
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);	//get the resolution of the surface in pixels

			VkExtent2D actualExtent = {
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width,
				capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height,
				capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	void createSwapChain() {
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;	//ȷ��swap chain���ж���ͼƬ
		//���Ʒ�Χ����ֹ���������ͼƬ��Ŀ��=0��ʾû�������Ŀ���ƣ�
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;	//˵��swapchainӦ�ð󶨵��ĸ�surface
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;		//ָ��ͼ��Ĳ���
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	//ֱ����Ⱦ��ͼƬ�ϣ������к��ڴ���

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		if (indices.graphicsFamily != indices.presentFamily) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			//createInfo.queueFamilyIndexCount = 0;
			//createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;	//ͼ����swap chain�в���Ҫ�仯
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;		//����alphaͨ��
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;		//����ע�ڱ����أ�
		createInfo.oldSwapchain = VK_NULL_HANDLE;	//���細�ڱ任��С���������swap chainʧЧ����������

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
			throw std::runtime_error("failed to create swap chain!");

		//Retrieving the swap chain images�������������е�vkImage��
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		//�����µ�camera��perspective����
		camera.updatePerspectiveMatrix(camera.fov_y, static_cast<float>(extent.width) / extent.height, camera.zNear, camera.zFar);
	}

	//Ϊ��ʹ��vkImage�����봴��ImageView������������ô��ȡͼ�񣬴���ͼ���ʲô���֣�mipmap�ȣ�
	void createImageViews() {
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); ++i) {
			swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		}
	}

	//����binary�ļ���helper function
	static std::vector<char> readFile(const std::string& filename) {
		//ate���ļ�ĩβ��ʼ����������λ���������ļ���С���Ӷ�ָ��buffer����binary����Ϊ2�����ļ�����
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
			throw std::runtime_error("failed to open file!");

		size_t fileSize = file.tellg();
		std::vector<char> buffer(fileSize);		//���ݴ�С���û���

		file.seekg(0);	//seek back to the beginning of the file
		file.read(buffer.data(), fileSize);		//����buffer��

		file.close();
		return buffer;
	}

	//����shader module��helper function
	VkShaderModule createShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());	//Ҫת����uint32_t��ָ��

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
			throw std::runtime_error("failed to create shader module!");

		return shaderModule;
	}

	void createGraphicsPipeline() {
		//* programmable stages(vertex/fragment shader)
		auto vertShaderCode = readFile("../../../shaders/vert.spv");
		auto fragShaderCode = readFile("../../../shaders/frag.spv");

		VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";		//��ں���(ʵ��invoke(����)�ĺ���)
		//vertShaderStageInfo.pSpecializationInfo = nullptr;		//optional ����Ҫ(���̳�)

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo,fragShaderStageInfo };

		//* fixed-function stages
		//dynamic state(�����ڻ���ʱ��̬�ı������������������pipeline���ı�)
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		//vertex input(������������vertex����ʱ����  --���޸�)
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescription = Vertex::getAttributeDescriptions();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;	//optional
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();		//optional
		
		//input assembly�����vertex shader�д���Ķ��㣩
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;	//�������Ҳ�����
		inputAssembly.primitiveRestartEnable = VK_FALSE;	//���_STRIPģʽ�����ڴ�ϸ���

		//viewports and scissors
		VkViewport viewport{};
		viewport.x = .0f;
		viewport.y = .0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = .0f;
		viewport.maxDepth = 1.f;

		VkRect2D scissor{};		//�൱����viewport�Ļ������ٽ���һ�β��У�����ѡ��������
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		//ʣ�µĿ��Խ���drawing timeʱ���ã���̬����Ҳ�����������úã���̬��
		viewportState.pViewports = &viewport;
		viewportState.pScissors = &scissor;

		//rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;		//trueʱ�ڽ�ƽ���Զƽ�����Ƭ�λᱻ�ض���ƽ���϶����Ƕ������ǣ����ܶ�shadowmap����
		rasterizer.rasterizerDiscardEnable = VK_FALSE;	//��ֹ�����framebuffer�ϣ�������ʾ�κζ�����
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;	//��ģʽ/��ģʽ/��ģʽ
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;	//cull the back face����openGL��ͬ���붥��˳�����
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;		//˳ʱ�붨��Ϊfront face
		rasterizer.depthBiasEnable = VK_FALSE;		//���ڵ������Ǹ�������bias����ֹ����
		rasterizer.depthBiasConstantFactor = .0f;
		rasterizer.depthBiasClamp = .0f;
		rasterizer.depthBiasSlopeFactor = .0f;

		//multisampling
		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_TRUE;	// enable sample shading in the pipeline
		multisampling.rasterizationSamples = msaaSamples;
		multisampling.minSampleShading = .2f;	//min fraction for sample shading; closer to one is smoother
		multisampling.pSampleMask = nullptr;	//optional
		multisampling.alphaToCoverageEnable = VK_FALSE;	//optional
		multisampling.alphaToOneEnable = VK_FALSE;		//optional

		//todo: depth & stencil testing
		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = VK_TRUE;		//�Ƿ�����Ȳ�������������ס��fragments
		depthStencil.depthWriteEnable = VK_TRUE;	//ͨ�����Ժ��Ƿ�д��Ϊ�µ����
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;	//lower depth = closer
		depthStencil.depthBoundsTestEnable = VK_FALSE;		//ֻ�����ض�����ڵ�fragments
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		//color blending
		//��ǰ����ȫ������Ϊvk_false����ʾfragment��������ɫ��ֱ��д��framebuffer��
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};		//��Ե���framebufferʹ��
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;		//���룬������������ȷ����Щͨ������ͨ��
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional��blendʹ�õ�operator��
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional��aͨ��ʹ�õ�operator��
		//�����ǳ���Ŀ���͸���ȵ�ʵ�֣�
		//colorBlendAttachment.blendEnable = VK_TRUE;
		//colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		//colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		//colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		//colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		//colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		//colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};	//������framebufferʹ�ã�ȫ�֣���������ʵ�ַ�����ѡһ
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;		//���ó�true�󽫻�ʹ����perframebuffer��ȫ����Ч��
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		//* pipeline layout(ָ��uniform������Ҫ����������   --���޸�)   ���߲���
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;		
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;	//descriptor set layout
		pipelineLayoutInfo.pushConstantRangeCount = 0;	//optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr;	//optional
		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		//* render pass���ڸú���ǰ�Ѿ�������ɣ�

		//����graphics pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		//shader module
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;		//vertex + fragment
		pipelineInfo.pStages = shaderStages;	//��������ָ��
		//fixed function
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;		//�������
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		//pipeline layout
		pipelineInfo.layout = pipelineLayout;
		//depth test attachment
		pipelineInfo.pDepthStencilState = &depthStencil;
		//render pass
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;	//index
		//�����е�Pipeline�����봴���µ�pipeline
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;	//optional
		pipelineInfo.basePipelineIndex = -1;	//optional

		//����ͬʱ�������pipelines
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
			throw std::runtime_error("failed to create graphics pipeline!");

		vkDestroyShaderModule(device, vertShaderModule, nullptr);
		vkDestroyShaderModule(device, fragShaderModule, nullptr);
	}

	//��pipeline��һ���������������õ���attachments�������͸�ʽ
	//specify how many color and depth buffers there will be, 
	// how many samples to use for each of them, 
	// and how their contents should be handled throughout the rendering operations
	void createRenderPass() {
		//Attachment description
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = msaaSamples;	//δʹ��multisampling
		//��color & depth data����
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;	//clear the fb before drawing a new frame
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;	//��Ⱦ���ݴ洢���ڴ��У��ҿɶ�
		//��stencil data����
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	//���漰stencil test
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		//vkImage����(textures & framebuffers�����ڴ��е�layout����)
		//ע��image��Ҫ��ǰ��Ϊ�¸������������Layout����
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	//ָ����Ⱦ��ʼǰͼ�����ڴ��еĲ��ַ�ʽ
		//colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	//ָ����Ⱦ������ͼ����ɵ��Ĳ��ַ�ʽ(Ҫ��present)
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	//ָ����Ⱦ������ͼ����ɵ��Ĳ��ַ�ʽ(Ҫ��Ϊcolor attachment, ms��image����ֱ�ӱ�present)

		//Subpasses and attachment references�������ж�������̣�ÿ���������������ϸ������̵����ݣ������������ڴ���
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;				//subpass���õ�vkAttachmentDescription���±�(��renderpass�л�ȡ)
		//����subpass������ָ����attachment��layout(subpass��ʼʱ�Զ�ת��Ϊ��layout)
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = findDepthFormat();		//��depth image��ͬ
		depthAttachment.samples = msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		//����Ͷ������ô洢
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		//������֮ǰ��depth content
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;		//�ڶ������ϣ����±�Ϊ1
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//����msaa��
		VkAttachmentDescription colorAttachmentResolve{};
		colorAttachmentResolve.format = swapChainImageFormat;
		colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentResolveRef{};
		colorAttachmentResolveRef.attachment = 2;
		colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;	//ָ����graphics subpass
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;		//ֱ�ӱ�layout(locatoin = 0)ʹ��   �����
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;

		//������subpass dependencies(����render pass��ʼtransition��ʱ��)
		VkSubpassDependency dependency{};
		//EXTERNAL��srcSubpassʹ�ñ�ʾ��Ⱦ���̿�ʼǰ�������̣���dstSubpassʹ�ñ�ʾ��Ⱦ���̽������������
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;	//ָ����������subpass������external��ʾ������������
		dependency.dstSubpass = 0;						//ָ����������subpass��subpass������0��ʾ�Լ���subpass
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
			| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;	//ָ��src������ʱ�����ȴ��������е�ͼƬ��ȡ����
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
			| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT 
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;	//ֻ��Ҫд��ɫ��ʱ��ŻῪʼ���subpass?

		std::array<VkAttachmentDescription, 3> attachments = { colorAttachment, depthAttachment, colorAttachmentResolve };

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
			throw std::runtime_error("failed to create render pass!");
	}
	
	//Ϊswapchain�е�����image��Ӧ��imageview(attachment)����framebuffer
	void createFramebuffers() {
		swapChainFramebuffers.resize(swapChainImageViews.size());
		for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
			std::array<VkImageView, 3> attachments = {
				colorImageView,
				depthImageView,
				swapChainImageViews[i]
			};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;	//attachments������������Ҫ��Ӧ
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
				throw std::runtime_error("failed to create framebuffer!");
		}
	}

	void createCommandPool() {
		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
		
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();	//˵�������ڻ���ͼ���command buffer

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
			throw std::runtime_error("failed to create command pool!");
	}

	void createCommandBuffers() {
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;	//��������command pool
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	//PRIMARY��Can be submitted to a queue for execution, but cannot be called from other command buffers.
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate command buffers!");
	}

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {	//index��ʾ��ǰ����������д���ͼƬ�±�
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;	//optional
		beginInfo.pInheritanceInfo = nullptr;	//optional

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
			throw std::runtime_error("failed to begin recording command buffer!");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];		//attachments
		renderPassInfo.renderArea.offset = { 0,0 };
		renderPassInfo.renderArea.extent = swapChainExtent;

		std::array<VkClearValue, 2> clearValues{};	//��������attachments��color��depth
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();		//VK_ATTACHMENT_LOAD_OP_CLEARʹ�õ���ɫ

		//¼�Ʋ����ĺ�������vkcmd��ͷ������voidֵ�����¼�Ʋ���û�м�����Ĳ���
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		//basic drawing commands
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		
		//��vertex buffer
		VkBuffer vertexBuffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		//��index buffer
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		VkViewport viewport{};		//dynamic����
		viewport.x = .0f;
		viewport.y = .0f;
		viewport.width = static_cast<float>(swapChainExtent.width);
		viewport.height = static_cast<float>(swapChainExtent.height);
		viewport.minDepth = .0f;
		viewport.maxDepth = 1.f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		VkRect2D scissor{};
		scissor.offset = { 0,0 };
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		//bind descriptor set
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
			&descriptorSets[currentFrame], 0, nullptr);

		//��3����������ʵ����Ⱦ��д1��ʾ������������4����������gl_VertexIndex����Сֵ����5����������ʵ����Ⱦ������gl_InstanceIndex����Сֵ
		//vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);		//ʹ����������
		vkCmdEndRenderPass(commandBuffer);
		
		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
			throw std::runtime_error("failed to end recording command buffer!");
	}

	void createSyncObjects() {
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;		//��ʼ����Ϊsignaled

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS
				|| vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS
				|| vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
		}
	}

	void cleanupSwapChain() {
		vkDestroyImageView(device, colorImageView, nullptr);
		vkDestroyImage(device, colorImage, nullptr);
		vkFreeMemory(device, colorImageMemory, nullptr);

		vkDestroyImageView(device, depthImageView, nullptr);
		vkDestroyImage(device, depthImage, nullptr);
		vkFreeMemory(device, depthImageMemory, nullptr);

		for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
			vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
		}

		for (size_t i = 0; i < swapChainImageViews.size(); i++) {
			vkDestroyImageView(device, swapChainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	void recreateSwapChain() {
		//��������С��(��ʱframebuffer�Ĵ�СΪ0)
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();		//һֱ�ȴ�����
		}

		vkDeviceWaitIdle(device);

		cleanupSwapChain();	//����ɵ�swap chain / image view / framebuffer

		createSwapChain();
		createImageViews();
		createColorResources();
		createDepthResources();		//���Ĵ��ڴ�СҲ��Ҫ����depth buffer��resolution��Ҳ��image��
		createFramebuffers();

		//�����µ�camera��perspective����
		camera.updatePerspectiveMatrix(camera.fov_y, static_cast<float>(width) / height, camera.zNear, camera.zFar);
	}

	void createVertexBuffer() {
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		//�������������ʾ���Ա�ӳ��(CPU����д������) �Լ� ��֤����map���ڴ�һֱƥ������ڴ��е�����(��driver������ʶ����buffer��д��)
		//��ֱ����cpuд��vertex buffer(MMIO)������cpuд��һ����ʱ���ڴ���(staging buffer)��Ȼ������ת�Ƶ�gpu��ȡ�����device local�ڴ���
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);		//ֻ�ô���һ�Σ���˴��ú�ֱ��unmap��

		//gpu���Զ�ȡ�ĸ����device local�ڴ�����(����ʹ��vkMapMemory�ˣ�cpu������������򣬵�����ͨ�����ݿ���/ת����ʵ��)
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

		//���staging buffer
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);

		//���ˣ�vertex data is now being loaded from high performance memory
	}

	//�ҵ����ʵ�memoryType����graphics card���з���
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		//����available types of memory
		VkPhysicalDeviceMemoryProperties memProperties;		//��ʱֻ�������е�memoryType��ݲ�����memory heap��
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
			if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				return i;	//suitable for the buffer & has all of the properties we need
		}
		throw std::runtime_error("failed to find suitable memory type!");
	}

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
					  VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;		
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		//�Ƿ�ᱻ���queue���ã�����ֱ��exclusive����

		if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
			throw std::runtime_error("failed to create vertex buffer!");

		//��ѯbuffer���ڴ�����
		VkMemoryRequirements memRequirements;		//query memory requirements
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		//������ʵ��ڴ浽buffer memory
		VkMemoryAllocateInfo allocInfo{};			//allocate memory
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate vertex buffer memory!");

		//��buffer���������(buffer memory)
		vkBindBufferMemory(device, buffer, bufferMemory, 0);	//���ĸ�������ʾ������ڴ�����ʼλ�õ�ƫ����
	}

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		//�ڴ�ת�Ʋ���ҲҪʹ��command buffer��ʵ��
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);	//buffer���copy����

		//�ύbuffer copy������command buffer������transfer���ܵĶ�����(graphicsQueue�о���)   ȥ�����ڲ���
		endSingleTimeCommands(commandBuffer);
	}

	void createIndexBuffer() {
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void createUniformBuffers() {
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		
		uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
		uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i], uniformBuffersMemory[i]);
			//ע�����ﲻҪ���mapping����Ϊ��������������ubo�е�uniform��������ÿ֡�仯����Ҫһֱ����
			//���ּ�������Ϊ"persistent mapping"�����mapping������free��
			vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
		}
	}

	void updateUniformBuffer(uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();	//ֻ�г�ʼ����һ��

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		//�����������z�ᳯ��
		//ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(.0f, .0f, 1.f));//����ת
		//ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, 1.f));
		//ubo.projection = glm::perspective(glm::radians(45.f), swapChainExtent.width / 
			//(float)swapChainExtent.height, .1f, 10.f);
		
		ubo.model = glm::mat4(1.f);
		ubo.view = camera.matrices.view;
		ubo.projection = camera.matrices.perspective;
		
		//openGL��Vulkan�Ĳü������y�����෴�ģ�ͨ����������proj������scale factor��yȡ�����ﵽ��ͬ��Ч��
		//ubo.projection[1][1] *= -1;

		//ֱ�Ӹ���buffer�е����ݣ�����Ҫ����mapping
		memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void createDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;	//��Ӧvertex shader��ubo�������binding
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;	//���Խ�ͬһ�������ϵĲ�ͬ�˶�������д��һ��descriptor layout���飬������Ϊ����Ԫ�ظ���
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	//����vertex shader������
		uboLayoutBinding.pImmutableSamplers = nullptr;		//����ͼ�������أ���������

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;		//����fragment shader��ʹ��

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
			throw  std::runtime_error("failed to create descriptor set layout!");
	}

	void createDescriptorPool() {
		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);		//�������descriptor set����
		poolInfo.flags = 0;		//optional

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
			throw std::runtime_error("failed to create descriptor pool!");
	}

	void createDescriptorSet() {
		std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		allocInfo.pSetLayouts = layouts.data();

		descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate descriptor sets!");

		//����ڲ�����
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			VkDescriptorBufferInfo bufferInfo{};	//ʹ��buffer�洢��desciptor����ub��Ҫʹ������ṹ��
			bufferInfo.buffer = uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureImageView;
			imageInfo.sampler = textureSampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};		//�������庬��ؿ��̳�pool��һ��
			
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;	//descriptorsΪ����ʱʹ�ã���������󶨵ȣ�
			descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			descriptorWrites[0].descriptorCount = 1;
			descriptorWrites[0].pBufferInfo = &bufferInfo;	

			descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[1].dstSet = descriptorSets[i];
			descriptorWrites[1].dstBinding = 1;
			descriptorWrites[1].dstArrayElement = 0;
			descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			descriptorWrites[1].descriptorCount = 1;
			descriptorWrites[1].pImageInfo = &imageInfo;

			vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), 
				descriptorWrites.data(), 0, nullptr);
		}
	}

	void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
		//����image object
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;	//1dͼ��洢������ݶȣ�2d����3d voxel volumes
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = numSamples;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
			throw std::runtime_error("failed to create image!");
		}

		//�����ڴ棨�Ȳ�ѯҪ�󣬺�����ڴ棩
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		//�󶨷�����ڴ浽image object��
		vkBindImageMemory(device, image, imageMemory, 0);
	}

	void createTextureImage() {
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * 4;	//ÿ������4byte
		if (!pixels)
			throw std::runtime_error("failed to load texture image!");

		//ȷ�������Ӧ�����mipmap�ȼ�����ʼͼ���Ӧ1
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

		//����staging buffer���������ݽ�ȥ
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(device, stagingBufferMemory);

		stbi_image_free(pixels);
		
		// VK_IMAGE_USAGE_SAMPLED_BIT ��ʾϣ��shader�ɻ�ȡ��Щ����
		createImage(texWidth, texHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, 
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

		//copying staging buffer to texture image
		//��texture image transitionΪת��Ŀ���layout
		transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
		copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		
		//��texture image transitionΪshader���Զ�ȡ��״̬
		//transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, 
		//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		//����mipmap����transitionΪshader�ɶ���״̬
		generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	//���䲢��ʼ
	VkCommandBuffer beginSingleTimeCommands() {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = commandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	//�������ύ���ͷţ���Ϊ�ǵ��Σ�
	void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	//perform image layout transitions������Imageʱ��Ҫ���⿼��layout�Ĺ������⣩
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		//����ķ�������image memory barrier
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;	//��������transition image layout
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	//��������transfer queue family ownership
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	//ע�ⲻת�ƵĻ����ﲻ������ΪĬ��ֵ��
		barrier.image = image;

		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (hasStencilComponent(format)) {
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = mipLevels;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
			barrier.srcAccessMask = 0;	//ָ��barrier֮ǰ��Ҫ����ʲô����һ��û�����ƣ�
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;	//ָ��barrier֮����Ҫ����ʲô

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;	//û�����ƣ����ֱ��ѡ������Ľ׶�
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;	//transfer write����������׶η���
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;	//����fragment�Ƿ�ɼ�ʱ��ȡdepth image buffer�е�����
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		//������Ľ��ͼ���https://vulkan-tutorial.com/Texture_mapping/Images
		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);	//��������ǿ��ܵ�������;����ѡһ

		endSingleTimeCommands(commandBuffer);
	}

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferImageCopy region{};		//ָ��buffer����Щ���ֻᱻcopy��image����Щ����
		region.bufferOffset = 0;
		region.bufferRowLength = 0;		//0��ʾ���ص�����������
		region.bufferImageHeight = 0;	//ͬ��

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(commandBuffer, buffer, image, 
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		endSingleTimeCommands(commandBuffer);
	}

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
			throw std::runtime_error("failed to create texture image view!");
		}

		return imageView;
	}

	void createTextureImageView() {
		textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
	}

	void createTextureSampler() {
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		
		samplerInfo.magFilter = VK_FILTER_LINEAR;	//����oversampling
		samplerInfo.minFilter = VK_FILTER_LINEAR;	//����undersampling
		
		//uvw��Ӧxyz�����ڳ���ͼƬ��Χ������Ĵ���ʽ
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		//anisotropic filtring  �������Թ���
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;	//����texel samples���������

		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;	//clamp to borderģʽ�г����߽����ɫ
		samplerInfo.unnormalizedCoordinates = VK_FALSE;		//false����[0,1)�� ture��[0, texWidth) & [0, texHeight)

		samplerInfo.compareEnable = VK_FALSE;	//true��texels����ĳ��ֵ�Ƚϣ��������filtering������PCF
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0; // Optional
		//samplerInfo.minLod = static_cast<float>(mipLevels / 2); // Optional
		samplerInfo.maxLod = static_cast<float>(mipLevels);
		//samplerInfo.maxLod = 0;
		samplerInfo.mipLodBias = 0.0f; // Optional

		//ע��sampler�������ض���image object�ϣ�����ͬ�õĶ�����object
		if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
			throw std::runtime_error("failed to create texture sampler!");
	}

	void createDepthResources() {
		VkFormat depthFormat = findDepthFormat();
		createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, 
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
		depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

		//optional��
		transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
	}

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
		for (VkFormat format : candidates) {
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	VkFormat findDepthFormat() {
		return findSupportedFormat(
			{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	bool hasStencilComponent(VkFormat format) {
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	void loadModel() {
		tinyobj::attrib_t attrib;	//����vertices, normals, texcoords
		std::vector<tinyobj::shape_t> shapes;	//����separate objects & their faces
		//һ��face������Ӧ�Ķ��㣬�����а�����attrib�е�indices
		std::vector<tinyobj::material_t> materials;	
		std::string warn, err;
		//Ĭ�ϻὫ����faces��������ι��ɵ�faces
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
			throw std::runtime_error(warn + err);

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		//��ӵ�vertex�����indices������
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {	//ʹ����Щ��������ȡʵ�ʵ�vertex attributes
				Vertex vertex{};
				vertex.position = {
					attrib.vertices[3 * index.vertex_index],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index],
					1.f - attrib.texcoords[2 * index.texcoord_index + 1]	
				};	//obj�Ĳ���ͼƬĬ��������0���������ӣ�������������top to bottom��������0
				vertex.color = { 1.f,1.f,1.f };
				
				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());	//��¼���������Ӧ�����
					vertices.push_back(vertex);
				}

				indices.push_back(uniqueVertices[vertex]);
				//indices.push_back(indices.size());		//����������0��ʼ��
			}
		}
	}

	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
		// Check if image format supports linear blitting
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

		if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
			throw std::runtime_error("texture image format does not support linear blitting!");
		}

		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = texWidth;
		int32_t mipHeight = texHeight;

		for (uint32_t i = 1; i < mipLevels; i++) {
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			vkCmdBlitImage(commandBuffer,
				image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blit,
				VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		endSingleTimeCommands(commandBuffer);
	}

	VkSampleCountFlagBits getMaxUsableSampleCount() {
		VkPhysicalDeviceProperties physicalDeviceProperties;
		vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

		VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
		if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
		if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
		if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
		if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
		if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
		if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

		return VK_SAMPLE_COUNT_1_BIT;
	}

	//�������ڴ��multisample��color image
	void createColorResources() {
		VkFormat colorFormat = swapChainImageFormat;
		createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, colorFormat, 
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT 
			| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			colorImage, colorImageMemory);
		colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

	void drawFrame() {
		//�ȴ��ϴθø�framebuffer��Ӧ��gpu��������(���framebufferһ��ִ��)
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);	//true����������fences���ԣ����ĸ�������time out

		// per-frame time logic
		currentTime = static_cast<float>(glfwGetTime());
		deltaTime = currentTime - lastTime;
		lastTime = currentTime;
		
		//��ȡswap chain�е���һ��image��imageIndex�������õ�image�󼤻�semaphore��������ڸı䣬�ؽ�������
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		
		//�����ȡimageʱswap chain�Ѿ�out of date����ô������Present�ɹ�����������ִ�У�ֱ���ؽ�������
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {	//swap chain out of date
			//recreate swap chain
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		
		// Only reset the fence if we are submitting work(д�������ֹ���Ĵ��ں�ֱ�ӷ��ص��µ�fence��������)
		vkResetFences(device, 1, &inFlightFences[currentFrame]);	//fence��Ҫ�ֶ�reset

		//recording the command buffer
		vkResetCommandBuffer(commandBuffers[currentFrame], 0);	//0��ʾ������������
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		camera.Tick(deltaTime);

		//update ubo
		updateUniformBuffer(currentFrame);

		//submit command buffer
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		//����ĺ��壺ִ�е�color attachment outputʱ���������ȴ��ź������swapchain���õ�ͼ��󣩲ż���
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;		//ָ����Ⱦ��ʼǰ�ȴ���Щsemaphore
		submitInfo.pWaitDstStageMask = waitStages;			//�ȴ�дcolor attchment�Ľ׶�
		
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame]};
		submitInfo.signalSemaphoreCount = 1;	
		submitInfo.pSignalSemaphores = signalSemaphores;	//ָ����Ⱦ����������Щsemaphore

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)	//���������ֱ�ӷ��أ������Ҫ�����ź�����ʹcpu�ȴ�gpu��ɹ���
			throw std::runtime_error("failed to submit draw command buffer!");

		//Presentation
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;		//�ȴ���Ⱦ�������ǵ�semaphore��Ǻ�Ž���

		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;
		
		result = vkQueuePresentKHR(presentQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			recreateSwapChain();
			framebufferResized = false;
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		//�л�����һ֡(���Ĳ���)
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
};

int main() {
	HelloTriangleApplication app;

	try {
		app.run();
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

void processInput(GLFWwindow* window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		camera.enterCam = false;
	}
	
	//��f������Ļ
	if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		camera.enterCam = true;
		camera.firstEnter = true;
	}

	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (camera.enterCam) {
		//��������ͷ�ƶ�
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
			camera.keys.forward = true;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
			camera.keys.backward = true;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
			camera.keys.left = true;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
			camera.keys.right = true;
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
			camera.keys.up = true;
		if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
			camera.keys.down = true;

		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_RELEASE)
			camera.keys.forward = false;
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_RELEASE)
			camera.keys.backward = false;
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE)
			camera.keys.left = false;
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE)
			camera.keys.right = false;
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE)
			camera.keys.up = false;
		if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE)
			camera.keys.down = false;
	}

}