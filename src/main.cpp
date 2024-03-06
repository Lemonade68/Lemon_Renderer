#define GLFW_INCLUDE_VULKAN
#include<GLFW/glfw3.h>							//定义上面的宏后，会自动include vulkan.h
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

#define GLM_FORCE_RADIANS						//确保glm中类似rotate的函数使用的都是radians而不是角度
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES		//强制glm使用内存对齐要求，解决如vec2等类型带来的问题（解决不了自定义类型的对齐，因此最好还是显式对齐）
#define GLM_FORCE_DEPTH_ZERO_TO_ONE				//vulkan中depth范围为0到1，而不是opengl中的-1到1
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

const int MAX_FRAMES_IN_FLIGHT = 2;	//最大同时处理的frameBuffer数量
uint32_t currentFrame = 0;			//当前处理的frame

float currentTime = 0, lastTime = 0, deltaTime = 0;

class Camera {
private:
	glm::vec3 position;
	glm::vec3 worldUp{ 0.f, 1.f, 0.f };

public:
	struct {
		glm::vec3 right;	//x，cam的右方
		glm::vec3 up;		//y，摄像机的上方
		glm::vec3 back;		//z，对应摄像机朝向的反方向
	} cameraWorldCoords;

	bool enterCam = true;
	bool firstEnter = true;

	float fov_y;
	float zNear, zFar;
	float aspect_ratio;
	float pitch = 0.f, yaw = -90.f;				//俯仰角与偏航角(初始指向z轴)

	enum class CameraType {
		firstperson,
		lookat
	};
	CameraType type;		

	struct {
		glm::mat4 view;				//view矩阵
		glm::mat4 perspective;		//perspective矩阵
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
		glm::vec3 target(0.f);						//初始时看向原点
		cameraWorldCoords.back = glm::normalize(position - target);
		cameraWorldCoords.right = glm::normalize(glm::cross(worldUp, cameraWorldCoords.back));
		cameraWorldCoords.up = glm::normalize(glm::cross(cameraWorldCoords.back, cameraWorldCoords.right));
		updateViewMatrix();
		//updatePerspectiveMatrix(fov_y, SCR_WIDTH / SCR_HEIGHT, zNear, zFar);		//创建swapchain时初始化
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

		//默认使用vulkan，需要flipY
		matrices.perspective[1][1] *= -1.0f;
	}

	void Tick(float deltaTime) {
		if (enterCam) {
			if (type == CameraType::firstperson) {
				//修改朝向信息
				cameraWorldCoords.right = glm::normalize(glm::cross(worldUp, cameraWorldCoords.back));
				cameraWorldCoords.up = glm::normalize(glm::cross(cameraWorldCoords.back, cameraWorldCoords.right));

				//修改位置信息
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

				updateViewMatrix();		//修改camera的信息会导致view matrix的重建，但是只有当fov和aspect_ratio改变时才会影响perspectiveMatrix
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

//检测需要的校验层是否可用
bool checkValidationLayerSupport() {
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	//检查
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

//检验要求的设备拓展是否可用（与上面是两种方法，都可行）
bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	//检查需要的拓展是否被可用拓展全部包含
	std::set<std::string> unconfirmedRequiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
	for (const auto& extension : availableExtensions)
		unconfirmedRequiredExtensions.erase(extension.extensionName);

	return unconfirmedRequiredExtensions.empty();
}

//因为是拓展，所以vkCreateDebugUtilsMessengerEXT没有自动加载，需要我们自己写好代理函数后将其装载进vulkan
//PFN: pointer to function
//vkGetInstanceProcAddr从实例中获取后面名字的函数的地址，并装载到func上
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
	const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		//装载成功
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

}

//destroy debug messenger的函数同上
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
	const VkAllocationCallbacks* pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
		func(instance, debugMessenger, pAllocator);
}

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
	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{};
		bindingDescription.binding = 0;		//对应的绑定数组在所有数组中的下标（只有一个数组，因此为0）
		bindingDescription.stride = sizeof(Vertex);		//步长
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;		//Move to the next data entry after each vertex
		return bindingDescription;
	}

	//顶点数据怎么分成具体哪些部分(VertexAttributePointer)
	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {	//2代表位置和颜色两个属性
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;	//同上
		attributeDescriptions[0].location = 0;	//同vertex shader中的location对应
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;	//对应vec3
		attributeDescriptions[0].offset = offsetof(Vertex, position);	//offsetof返回结构体成员在结构体中的偏移量

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;	//对应vec3
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}

	//unordered map使用的equality test函数
	bool operator==(const Vertex& other) const {
		return position == other.position && color == other.color && texCoord == other.texCoord;
	}
};

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
//const std::vector<uint16_t> indices = {		//uint15和uint32都可以
//	0, 1, 2, 2, 3, 0,
//	4, 5, 6, 6, 7, 4
//};

//注意对齐要求(mat4的大小为 4byte*(4行*4列) = 64byte)，使用c++11标准的alignas来保证对齐
//比较好的做法：全都显示对齐内存
struct UniformBufferObject {
	//glm::vec2 foo;		//vec2大小为8byte，导致后面的mat的offset都不为16的整数倍
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};

class HelloTriangleApplication {
public:
	void run() {
		initWindow();
		compileShader();	//程序运行时自动compile shader
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;	//用于调用回调函数
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device;			//logical device
	VkQueue graphicsQueue;		//to handle the graphics queue
	VkSurfaceKHR surface;		//窗口对象
	VkQueue presentQueue;		//to handle the presentation queue
	VkSwapchainKHR swapChain;	//交换链
	std::vector<VkImage> swapChainImages;	//交换链中的图像
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;		//画面长宽
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
	uint32_t mipLevels;		//mipmap等级
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

	void initWindow() {
		glfwInit();		//激活glfw lib
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	//防止生成openGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);		//允许更改窗口大小
		window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Vulkan", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);			//该函数可以存出任意一个指针，解决this问题
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);	//Resize了就会调用framebufferResizeCallback函数

		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		glfwSetCursorPosCallback(window, mouseCallback);
		glfwSetScrollCallback(window, scrollCallback);

	}

	//GLFW并不会使用this指针来指代这个类，因此设置为static函数(不能调用this)
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
		auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;		//显式设置为true
		//添加这俩个可以让窗口在变化时也能跟着即使变化，glfw在resize时会暂停event loop，
		//但是鼠标每移动一点都会调用这个call back，在这里添加从而达到实时改变的效果
		app->recreateSwapChain();	//第一个用于recreate swap chain
		app->drawFrame();			//第二个用于实际的渲染
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

	//compile new shader file  (注意使用绝对路径！)
	void compileShader() {
		const char* batchFilePath = "C:/Users/Lemonade/Desktop/Lemon_Render/shaders/compile.bat";
		if (system(batchFilePath) != 0)
			throw std::runtime_error("failed to execute shader batch file!");
	}

	void initVulkan() {
		createInstance();		//创建实例（初始化vk library）
		setupDebugMessenger();	//初始化debug messenger
		createSurface();
		pickPhysicalCard();		//选择physical device
		createLogicalDevice();	//创建device来与上面进行interface
		createSwapChain();		//创建swap chain
		createImageViews();		//不直接使用image，而是需要image view
		createRenderPass();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool();
		createColorResources();	//for multisample
		createDepthResources();
		createFramebuffers();
		createTextureImage();	//load image into vulkan image object
		createTextureImageView();
		createTextureSampler();		//负责filtering & transformations to compute final color 
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
			processInput(window);		//加上就有问题，为啥？
			glfwPollEvents();
			drawFrame();
		}

		vkDeviceWaitIdle(device);	//等待logical device结束运行所有操作
	}

	//手动创建的都需要destroy，这里的顺序有待理解
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

		vkDestroySurfaceKHR(instance, surface, nullptr);	//清除实例前清除surface对象
		vkDestroyInstance(instance, nullptr);	//清除实例

		glfwDestroyWindow(window);

		glfwTerminate();
	}

	//作用：指定application info，extension info
	void createInstance() {
		//validation layer检查
		if (enableValidationLayers && !checkValidationLayerSupport())
			throw std::runtime_error("validation layers requested, but not available!");

		VkApplicationInfo appInfo{};		//非必须，但之后如果用到特定引擎，可有助于优化
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};		//必须的结构体，告诉driver需要使用的全局拓展和校验层
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		//获取可支持的拓展（可选）
		//uint32_t extensionCount = 0;
		//vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);	//获取可支持的拓展的个数
		//std::vector<VkExtensionProperties> extensions_ava(extensionCount);
		//vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions_ava.data());
		//std::cout << "available extensions:\n";
		//for (const auto& extension : extensions_ava) {
		//	std::cout << '\t' << extension.extensionName << '\n';
		//}

		//指定需要的全局拓展(global extensions)  改进版
		auto extensions = getRequiredExtensions();		//目前是 glfw + validation layer
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		//指定全局校验层(global validation layers)
		// + vkCreateInstance/vkDestroyInstance 的message打印及debug（完全没看懂）
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			//告诉vulkan，在创建实例的同时配置一个debug回调，pNext允许传递额外的信息，以便于扩展或配置特定功能
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else {
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		//以上是所有必要信息，调用下面的vkCreateInstance来创建vk实例到instance中，
		//并通过返回值(类型为VkResult)来确定是否成功执行
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			throw std::runtime_error("failed to create instance!");
		}
	}

	std::vector<const char*> getRequiredExtensions() {
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		//glfw指定的拓展必须使用
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		//将需要的extensions转换为需要的extension的vector，方便后续的添加
		//这里使用的是起始迭代器和结束迭代器的构造方法
		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		//校验层的拓展可以选择是否添加
		if (enableValidationLayers)
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);		//这个宏代表一个字符串

		return extensions;
	}

	//EXT表示extension（非vk核心部分，而是官方拓展，messenger这种的都要加），utils表示utilities（工具）
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,		//信息重要性
		VkDebugUtilsMessageTypeFlagsEXT messageType,				//信息类型
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,	//包含message的结构体指针
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
		//指定回调函数处理的信息级别
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		//指定回调函数处理的信息类型
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		//指定指向回调函数的指针
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
		//找这个physical device有没有对应的支持图形操作的队列族并记录
		QueueFamilyIndices indices = findQueueFamilies(device);

		//找这个physical device支不支持对应的拓展
		bool extensionsSupported = checkDeviceExtensionSupport(device);

		//检查extension中的swap chain是否adequate(至少支持一种图片格式与一种呈现形式)
		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		//检查是否支持anisotropic filtering
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

		//在所有队列族中找到支持queueFamily中要求操作的多个队列族，分别记录其index
		int i = 0;
		for (const auto& queueFamily : queueFamilies) {
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)	//通过位与来判断
				indices.graphicsFamily = i;

			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
			if (presentSupport)
				indices.presentFamily = i;

			if (indices.isComplete())		//找到了就直接退出即可
				break;
			++i;
		}

		return indices;
	}

	void createLogicalDevice() {
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		//多个队列族，直接使用vector来进行操作
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		//使用set的好处：如果两个的下标相同，则只会产生含有一个下标的set
		std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		float queuePriority = 1.f;  //队列优先级
		for (auto queueFamily : uniqueQueueFamilies) {
			//Specifying the queues to be created
			VkDeviceQueueCreateInfo queueCreateInfo{};		//设置希望一个队列族中的队列的数量
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;		//p表示pointer
			queueCreateInfos.push_back(queueCreateInfo);
		}

		//Specifying used device features
		VkPhysicalDeviceFeatures deviceFeatures{};
		deviceFeatures.samplerAnisotropy = VK_TRUE;		//允许使用anisotropic filtering
		deviceFeatures.sampleRateShading = VK_TRUE; // enable sample shading feature for the device

		//creating logical device
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();		//更改为vector的内容
		createInfo.pEnabledFeatures = &deviceFeatures;

		//下面部分与实例化中设置拓展和校验层的部分相似
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());			//例如swapchain这种技术，待后续加上
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		//现在不区分实例和设备间的校验层，可以不用，但为了与旧版本兼容，还是加上
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else {
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
			throw std::runtime_error("failed to create logical device!");

		//第二个参数是队列族的下标，第三个参数是该队列族中的对应队列下标
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
	}

	void createSurface() {
		//直接由glfw来实现，如果要自己实现的话需要用到windows中的相关参数（见教程）
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
			throw std::runtime_error("failed to create window surface!");
	}

	//Querying details of swap chain support
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;

		//所有swap chain查询函数都包含device和surface这两个关键参数
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
			//找到支持SRGB的就返回（一般来说第一个就好）
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				return availableFormat;
		}
		return availableFormats[0];		//找不到想要的格式，直接返回第一个就挺好
	}

	//conditions for "swapping" images to the screen（如何阻止画面撕裂感）
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

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;	//确定swap chain中有多少图片
		//限制范围，防止超出最大处理图片数目（=0表示没有最大数目限制）
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;	//说明swapchain应该绑定到哪个surface
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;		//指定图像的层数
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;	//直接渲染到图片上（不进行后期处理）

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

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;	//图像在swap chain中不需要变化
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;		//忽略alpha通道
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;		//不关注遮蔽像素？
		createInfo.oldSwapchain = VK_NULL_HANDLE;	//例如窗口变换大小问题带来的swap chain失效，后续完善

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
			throw std::runtime_error("failed to create swap chain!");

		//Retrieving the swap chain images（检索交换链中的vkImage）
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		//设置新的camera的perspective矩阵
		camera.updatePerspectiveMatrix(camera.fov_y, static_cast<float>(extent.width) / extent.height, camera.zNear, camera.zFar);
	}

	//为了使用vkImage，必须创建ImageView对象（描述了怎么获取图像，处理图像的什么部分，mipmap等）
	void createImageViews() {
		swapChainImageViews.resize(swapChainImages.size());
		for (size_t i = 0; i < swapChainImages.size(); ++i) {
			swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
		}
	}

	//读入binary文件的helper function
	static std::vector<char> readFile(const std::string& filename) {
		//ate：文件末尾开始读（方便用位置来决定文件大小，从而指定buffer）；binary：作为2进制文件读入
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
			throw std::runtime_error("failed to open file!");

		size_t fileSize = file.tellg();
		std::vector<char> buffer(fileSize);		//根据大小设置缓冲

		file.seekg(0);	//seek back to the beginning of the file
		file.read(buffer.data(), fileSize);		//读到buffer中

		file.close();
		return buffer;
	}

	//创建shader module的helper function
	VkShaderModule createShaderModule(const std::vector<char>& code) {
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());	//要转换成uint32_t的指针

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
		vertShaderStageInfo.pName = "main";		//入口函数(实际invoke(调用)的函数)
		//vertShaderStageInfo.pSpecializationInfo = nullptr;		//optional 但重要(看教程)

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo,fragShaderStageInfo };

		//* fixed-function stages
		//dynamic state(可以在绘制时动态改变的量，不用重新设置pipeline来改变)
		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		//vertex input(后续真正传入vertex数据时更改  --已修改)
		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescription = Vertex::getAttributeDescriptions();
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;	//optional
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();		//optional
		
		//input assembly（针对vertex shader中传入的顶点）
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;	//三角形且不复用
		inputAssembly.primitiveRestartEnable = VK_FALSE;	//针对_STRIP模式，用于打断复用

		//viewports and scissors
		VkViewport viewport{};
		viewport.x = .0f;
		viewport.y = .0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = .0f;
		viewport.maxDepth = 1.f;

		VkRect2D scissor{};		//相当于在viewport的基础上再进行一次裁切，这里选择满画幅
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		//剩下的可以交给drawing time时设置（动态），也可以这里设置好（静态）
		viewportState.pViewports = &viewport;
		viewportState.pScissors = &scissor;

		//rasterizer
		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;		//true时在近平面和远平面外的片段会被截断在平面上而不是丢弃它们，可能对shadowmap有用
		rasterizer.rasterizerDiscardEnable = VK_FALSE;	//阻止输出到framebuffer上（不会显示任何东西）
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;	//面模式/线模式/点模式
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;	//cull the back face，与openGL相同，与顶点顺序相关
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;		//顺时针定义为front face
		rasterizer.depthBiasEnable = VK_FALSE;		//自遮挡现象那个？加上bias来防止条纹
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
		depthStencil.depthTestEnable = VK_TRUE;		//是否开启深度测试来丢弃被挡住的fragments
		depthStencil.depthWriteEnable = VK_TRUE;	//通过测试后是否写入为新的深度
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;	//lower depth = closer
		depthStencil.depthBoundsTestEnable = VK_FALSE;		//只关心特定深度内的fragments
		depthStencil.minDepthBounds = 0.0f; // Optional
		depthStencil.maxDepthBounds = 1.0f; // Optional
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front = {}; // Optional
		depthStencil.back = {}; // Optional

		//color blending
		//当前两个全部设置为vk_false，表示fragment出来的颜色会直接写到framebuffer上
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};		//针对单个framebuffer使用
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;		//掩码，最后和它相与来确定哪些通道可以通过
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional（blend使用的operator）
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional（a通道使用的operator）
		//下面是常规的考虑透明度的实现：
		//colorBlendAttachment.blendEnable = VK_TRUE;
		//colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		//colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		//colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		//colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		//colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		//colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};	//对所有framebuffer使用（全局），与上面实现方法二选一
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;		//设置成true后将会使上面perframebuffer的全部无效化
		colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f; // Optional
		colorBlending.blendConstants[1] = 0.0f; // Optional
		colorBlending.blendConstants[2] = 0.0f; // Optional
		colorBlending.blendConstants[3] = 0.0f; // Optional

		//* pipeline layout(指定uniform变量需要，后续完善   --已修改)   管线布局
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;		
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;	//descriptor set layout
		pipelineLayoutInfo.pushConstantRangeCount = 0;	//optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr;	//optional
		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}

		//* render pass（在该函数前已经构造完成）

		//创建graphics pipeline
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		//shader module
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;		//vertex + fragment
		pipelineInfo.pStages = shaderStages;	//数组名做指针
		//fixed function
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;		//后续添加
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		//pipeline layout
		pipelineInfo.layout = pipelineLayout;
		//depth test attachment
		pipelineInfo.pDepthStencilState = &depthStencil;
		//render pass
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;	//index
		//从已有的Pipeline中脱离创建新的pipeline
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;	//optional
		pipelineInfo.basePipelineIndex = -1;	//optional

		//可以同时创建多个pipelines
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
			throw std::runtime_error("failed to create graphics pipeline!");

		vkDestroyShaderModule(device, vertShaderModule, nullptr);
		vkDestroyShaderModule(device, fragShaderModule, nullptr);
	}

	//是pipeline的一个参数，定义了用到的attachments的数量和格式
	//specify how many color and depth buffers there will be, 
	// how many samples to use for each of them, 
	// and how their contents should be handled throughout the rendering operations
	void createRenderPass() {
		//Attachment description
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = swapChainImageFormat;
		colorAttachment.samples = msaaSamples;	//未使用multisampling
		//对color & depth data而言
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;	//clear the fb before drawing a new frame
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;	//渲染内容存储在内存中，且可读
		//对stencil data而言
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	//不涉及stencil test
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		//vkImage设置(textures & framebuffers等在内存中的layout布局)
		//注：image需要提前变为下个操作所需求的Layout布局
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	//指定渲染开始前图像在内存中的布局方式
		//colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	//指定渲染结束后图像过渡到的布局方式(要被present)
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	//指定渲染结束后图像过渡到的布局方式(要作为color attachment, ms的image不能直接被present)

		//Subpasses and attachment references（可以有多个子流程，每个子流程依赖于上个子流程的内容，可以用来后期处理）
		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;				//subpass引用的vkAttachmentDescription的下标(从renderpass中获取)
		//整个subpass过程中指定的attachment的layout(subpass开始时自动转换为此layout)
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;


		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = findDepthFormat();		//与depth image相同
		depthAttachment.samples = msaaSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		//画完就丢，不用存储
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;		//不关心之前的depth content
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{};
		depthAttachmentRef.attachment = 1;		//第二个（上），下标为1
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//呈现msaa用
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
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;	//指定是graphics subpass
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;		//直接被layout(locatoin = 0)使用   待理解
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		subpass.pResolveAttachments = &colorAttachmentResolveRef;

		//新增：subpass dependencies(更改render pass开始transition的时间)
		VkSubpassDependency dependency{};
		//EXTERNAL对srcSubpass使用表示渲染流程开始前的子流程，对dstSubpass使用表示渲染流程结束后的子流程
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;	//指定被依赖的subpass索引，external表示隐含的子流程
		dependency.dstSubpass = 0;						//指定依赖上面subpass的subpass索引，0表示自己的subpass
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
			| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;	//指定src发生的时机，等待交换链中的图片读取结束
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT 
			| VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT 
			| VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;	//只有要写颜色的时候才会开始这个subpass?

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
	
	//为swapchain中的所有image对应的imageview(attachment)创建framebuffer
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
			framebufferInfo.renderPass = renderPass;	//attachments的数量和类型要对应
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
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();	//说明是用于绘制图像的command buffer

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
			throw std::runtime_error("failed to create command pool!");
	}

	void createCommandBuffers() {
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;	//分配他的command pool
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;	//PRIMARY：Can be submitted to a queue for execution, but cannot be called from other command buffers.
		allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate command buffers!");
	}

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {	//index表示当前交换链中想写入的图片下标
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

		std::array<VkClearValue, 2> clearValues{};	//包括两个attachments：color和depth
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();		//VK_ATTACHMENT_LOAD_OP_CLEAR使用的颜色

		//录制操作的函数都以vkcmd开头，返回void值，因此录制操作没有检测错误的步骤
		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		//basic drawing commands
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
		
		//绑定vertex buffer
		VkBuffer vertexBuffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

		//绑定index buffer
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		VkViewport viewport{};		//dynamic部分
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

		//第3个参数用于实例渲染，写1表示不这样做；第4个参数定义gl_VertexIndex的最小值；第5个参数用于实例渲染，定义gl_InstanceIndex的最小值
		//vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);		//使用索引绘制
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
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;		//初始设置为signaled

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
		//处理窗口最小化(此时framebuffer的大小为0)
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();		//一直等待即可
		}

		vkDeviceWaitIdle(device);

		cleanupSwapChain();	//清除旧的swap chain / image view / framebuffer

		createSwapChain();
		createImageViews();
		createColorResources();
		createDepthResources();		//更改窗口大小也需要更改depth buffer的resolution（也是image）
		createFramebuffers();

		//设置新的camera的perspective矩阵
		camera.updatePerspectiveMatrix(camera.fov_y, static_cast<float>(width) / height, camera.zNear, camera.zFar);
	}

	void createVertexBuffer() {
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		//倒数第三个宏表示可以被映射(CPU可以写入数据) 以及 保证下面map的内存一直匹配分配内存中的内容(让driver可以意识到对buffer的写入)
		//不直接用cpu写入vertex buffer(MMIO)，而是cpu写到一个暂时的内存中(staging buffer)，然后将数据转移到gpu读取更快的device local内存中
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);		//只用传递一次，因此传好后直接unmap掉

		//gpu可以读取的更快的device local内存区域(不能使用vkMapMemory了，cpu读不到这个区域，但可以通过数据拷贝/转移来实现)
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

		//清除staging buffer
		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);

		//至此，vertex data is now being loaded from high performance memory
	}

	//找到合适的memoryType来让graphics card进行分配
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
		//查找available types of memory
		VkPhysicalDeviceMemoryProperties memProperties;		//暂时只关心其中的memoryType项，暂不关心memory heap项
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
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		//是否会被多个queue共用，不是直接exclusive即可

		if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
			throw std::runtime_error("failed to create vertex buffer!");

		//查询buffer的内存需求
		VkMemoryRequirements memRequirements;		//query memory requirements
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		//分配合适的内存到buffer memory
		VkMemoryAllocateInfo allocInfo{};			//allocate memory
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate vertex buffer memory!");

		//绑定buffer到这块区域(buffer memory)
		vkBindBufferMemory(device, buffer, bufferMemory, 0);	//第四个参数表示在这块内存中起始位置的偏移量
	}

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
		//内存转移操作也要使用command buffer来实现
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.srcOffset = 0;
		copyRegion.dstOffset = 0;
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);	//buffer间的copy操作

		//提交buffer copy操作的command buffer到含有transfer功能的队列中(graphicsQueue中就有)   去函数内部看
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
			//注意这里不要解除mapping，因为整个生命周期中ubo中的uniform变量都在每帧变化，需要一直更改
			//这种技术被称为"persistent mapping"，解除mapping并不是free的
			vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
		}
	}

	void updateUniformBuffer(uint32_t currentImage) {
		static auto startTime = std::chrono::high_resolution_clock::now();	//只有初始调用一次

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		//坐标轴好像是z轴朝上
		//ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(.0f, .0f, 1.f));//仅旋转
		//ubo.view = glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(.0f, .0f, .0f), glm::vec3(.0f, .0f, 1.f));
		//ubo.projection = glm::perspective(glm::radians(45.f), swapChainExtent.width / 
			//(float)swapChainExtent.height, .1f, 10.f);
		
		ubo.model = glm::mat4(1.f);
		ubo.view = camera.matrices.view;
		ubo.projection = camera.matrices.perspective;
		
		//openGL与Vulkan的裁剪坐标的y轴是相反的，通过设置下面proj矩阵中scale factor的y取反来达到相同的效果
		//ubo.projection[1][1] *= -1;

		//直接更新buffer中的数据，不需要重新mapping
		memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
	}

	void createDescriptorSetLayout() {
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;	//对应vertex shader中ubo括号里的binding
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;	//可以将同一个物体上的不同运动的描述写成一个descriptor layout数组，个数即为数组元素个数
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;	//仅在vertex shader中引用
		uboLayoutBinding.pImmutableSamplers = nullptr;		//仅与图像采样相关，后续完善

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;		//仅在fragment shader中使用

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
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);		//最多分配的descriptor set个数
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

		//填充内部数据
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			VkDescriptorBufferInfo bufferInfo{};	//使用buffer存储的desciptor（如ub）要使用这个结构体
			bufferInfo.buffer = uniformBuffers[i];
			bufferInfo.offset = 0;
			bufferInfo.range = sizeof(UniformBufferObject);

			VkDescriptorImageInfo imageInfo{};
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfo.imageView = textureImageView;
			imageInfo.sampler = textureSampler;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites{};		//参数具体含义回看教程pool那一章
			
			descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrites[0].dstSet = descriptorSets[i];
			descriptorWrites[0].dstBinding = 0;
			descriptorWrites[0].dstArrayElement = 0;	//descriptors为数组时使用（身体骨骼绑定等）
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
		//创建image object
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;	//1d图像存储数组或梯度；2d纹理；3d voxel volumes
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

		//分配内存（先查询要求，后分配内存）
		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}

		//绑定分配的内存到image object上
		vkBindImageMemory(device, image, imageMemory, 0);
	}

	void createTextureImage() {
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * 4;	//每个像素4byte
		if (!pixels)
			throw std::runtime_error("failed to load texture image!");

		//确定纹理对应的最大mipmap等级，初始图像对应1
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

		//创建staging buffer并拷贝数据进去
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
			| VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(device, stagingBufferMemory);

		stbi_image_free(pixels);
		
		// VK_IMAGE_USAGE_SAMPLED_BIT 表示希望shader可获取这些数据
		createImage(texWidth, texHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, 
			VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

		//copying staging buffer to texture image
		//将texture image transition为转移目标的layout
		transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
		copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		
		//将texture image transition为shader可以读取的状态
		//transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, 
		//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		//生成mipmap后再transition为shader可读的状态
		generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	//分配并开始
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

	//结束并提交与释放（因为是单次）
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

	//perform image layout transitions（处理Image时需要额外考虑layout的过渡问题）
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		//最常见的方法：用image memory barrier
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;	//这俩用于transition image layout
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	//这俩用于transfer queue family ownership
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;	//注意不转移的话这里不能设置为默认值！
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
			barrier.srcAccessMask = 0;	//指明barrier之前需要发生什么（这一步没有限制）
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;	//指明barrier之后需要发生什么

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;	//没有限制，因此直接选择最早的阶段
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;	//transfer write必须在这个阶段发生
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
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;	//测试fragment是否可见时读取depth image buffer中的内容
		}
		else {
			throw std::invalid_argument("unsupported layout transition!");
		}

		//对下面的解释见：https://vulkan-tutorial.com/Texture_mapping/Images
		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);	//最后三对是可能的三种用途，三选一

		endSingleTimeCommands(commandBuffer);
	}

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferImageCopy region{};		//指定buffer的哪些部分会被copy到image的哪些部分
		region.bufferOffset = 0;
		region.bufferRowLength = 0;		//0表示像素单纯紧密连接
		region.bufferImageHeight = 0;	//同上

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
		
		samplerInfo.magFilter = VK_FILTER_LINEAR;	//关于oversampling
		samplerInfo.minFilter = VK_FILTER_LINEAR;	//关于undersampling
		
		//uvw对应xyz，关于超出图片范围的纹理的处理方式
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		//anisotropic filtring  各向异性过滤
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;	//限制texel samples的最大数量

		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;	//clamp to border模式中超出边界的颜色
		samplerInfo.unnormalizedCoordinates = VK_FALSE;		//false：用[0,1)； ture：[0, texWidth) & [0, texHeight)

		samplerInfo.compareEnable = VK_FALSE;	//true：texels先与某个值比较，结果用于filtering，例如PCF
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0; // Optional
		//samplerInfo.minLod = static_cast<float>(mipLevels / 2); // Optional
		samplerInfo.maxLod = static_cast<float>(mipLevels);
		//samplerInfo.maxLod = 0;
		samplerInfo.mipLodBias = 0.0f; // Optional

		//注意sampler不绑定在特定的image object上，而是同用的独立的object
		if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
			throw std::runtime_error("failed to create texture sampler!");
	}

	void createDepthResources() {
		VkFormat depthFormat = findDepthFormat();
		createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, 
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
		depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

		//optional？
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
		tinyobj::attrib_t attrib;	//包含vertices, normals, texcoords
		std::vector<tinyobj::shape_t> shapes;	//包含separate objects & their faces
		//一个face包含对应的顶点，顶点中包含在attrib中的indices
		std::vector<tinyobj::material_t> materials;	
		std::string warn, err;
		//默认会将所有faces变成三角形构成的faces
		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
			throw std::runtime_error(warn + err);

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};

		//添加到vertex数组和indices数组中
		for (const auto& shape : shapes) {
			for (const auto& index : shape.mesh.indices) {	//使用这些索引来获取实际的vertex attributes
				Vertex vertex{};
				vertex.position = {
					attrib.vertices[3 * index.vertex_index],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2]
				};
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index],
					1.f - attrib.texcoords[2 * index.texcoord_index + 1]	
				};	//obj的材质图片默认下面是0，往上增加；而程序中则是top to bottom，上面是0
				vertex.color = { 1.f,1.f,1.f };
				
				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());	//记录独立顶点对应的序号
					vertices.push_back(vertex);
				}

				indices.push_back(uniqueVertices[vertex]);
				//indices.push_back(indices.size());		//简单自增（从0开始）
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

	//创建用于存放multisample的color image
	void createColorResources() {
		VkFormat colorFormat = swapChainImageFormat;
		createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, colorFormat, 
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT 
			| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			colorImage, colorImageMemory);
		colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}

	void drawFrame() {
		//等待上次该个framebuffer对应的gpu操作结束(多个framebuffer一起执行)
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);	//true：对于所有fences而言；第四个参数：time out

		// per-frame time logic
		currentTime = static_cast<float>(glfwGetTime());
		deltaTime = currentTime - lastTime;
		lastTime = currentTime;
		
		//获取swap chain中的下一个image的imageIndex，并在拿到image后激活semaphore，如果窗口改变，重建交换链
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
		
		//如果获取image时swap chain已经out of date，那么不可能Present成功，不用往下执行，直接重建并返回
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {	//swap chain out of date
			//recreate swap chain
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		
		// Only reset the fence if we are submitting work(写在下面防止更改窗口后直接返回导致的fence产生死锁)
		vkResetFences(device, 1, &inFlightFences[currentFrame]);	//fence需要手动reset

		//recording the command buffer
		vkResetCommandBuffer(commandBuffers[currentFrame], 0);	//0表示不做其他操作
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		camera.Tick(deltaTime);

		//update ubo
		updateUniformBuffer(currentFrame);

		//submit command buffer
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		//这里的含义：执行到color attachment output时会阻塞，等待信号量激活（swapchain中拿到图像后）才继续
		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;		//指定渲染开始前等待哪些semaphore
		submitInfo.pWaitDstStageMask = waitStages;			//等待写color attchment的阶段
		
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame]};
		submitInfo.signalSemaphoreCount = 1;	
		submitInfo.pSignalSemaphores = signalSemaphores;	//指定渲染结束后标记哪些semaphore

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)	//这个函数会直接返回，因此需要设置信号量来使cpu等待gpu完成工作
			throw std::runtime_error("failed to submit draw command buffer!");

		//Presentation
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;		//等待渲染结束后标记的semaphore标记后才进行

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

		//切换到下一帧(最后的操作)
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
	
	//按f进入屏幕
	if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		camera.enterCam = true;
		camera.firstEnter = true;
	}

	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (camera.enterCam) {
		//控制摄像头移动
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