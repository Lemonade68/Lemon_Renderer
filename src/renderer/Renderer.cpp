#include "Renderer.h"
#include "base/UI.h"

// for header-only libraries, we need to have define XXX_IMPLEMENTATION to avoid "Already defined" problems��������.cpp��ֻ��Ҫдһ�Σ�
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

uint32_t currentFrame = 0;//��ǰ�����frame��ע��ͷ�ļ���ֻ��������������.cpp�ļ�ͬʱinclude��ͷ�ļ�ʱ���ᷢ���ظ����壩
float    currentTime = 0, lastTime = 0, deltaTime = 0;

Camera& camera = Camera::GetInstance();

// ��������Ļ�ĸı���ı�
uint32_t SCR_WIDTH  = 1280;
uint32_t SCR_HEIGHT = 800;

const std::string MODEL_PATH   = "../../../assets/models/viking_room/viking_room.obj";
const std::string TEXTURE_PATH = "../../../assets/models/viking_room/viking_room.png";

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        //װ�سɹ�
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
        func(instance, debugMessenger, pAllocator);
}

// Vertex struct funcs
std::vector<VkVertexInputBindingDescription> Vertex::getBindingDescriptions() {
    std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
    bindingDescriptions.resize(1);
    bindingDescriptions[0].binding   = 0;                          //��Ӧ�İ����������������е��±ֻ꣨��һ�����飬���Ϊ0��
    bindingDescriptions[0].stride    = sizeof(Vertex);             //����
    bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;//Move to the next data entry after each vertex
    return bindingDescriptions;
}

std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions() {//2����λ�ú���ɫ��������
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    attributeDescriptions.resize(3);
    attributeDescriptions[0].binding  = 0;                         //ͬ��
    attributeDescriptions[0].location = 0;                         //ͬvertex shader�е�location��Ӧ
    attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;//��Ӧvec3
    attributeDescriptions[0].offset   = offsetof(Vertex, position);//offsetof���ؽṹ���Ա�ڽṹ���е�ƫ����

    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;//��Ӧvec3
    attributeDescriptions[1].offset   = offsetof(Vertex, color);

    attributeDescriptions[2].binding  = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset   = offsetof(Vertex, texCoord);

    return attributeDescriptions;
}

// Renderer class funcs
void Renderer::mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    static float lastX;
    static float lastY;
    float        xPos = static_cast<float>(xpos);
    float        yPos = static_cast<float>(ypos);

    if (camera.enterCam) {
        if (camera.firstEnter) {
            lastX             = xPos;
            lastY             = yPos;
            camera.firstEnter = false;
        }

        float xoffset = xPos - lastX;
        float yoffset = lastY - yPos;
        lastX         = xPos;
        lastY         = yPos;

        xoffset *= camera.rotationSpeed;
        yoffset *= camera.rotationSpeed;

        camera.yaw += xoffset;
        camera.pitch += yoffset;

        if (camera.pitch > 89.f)
            camera.pitch = 89.f;
        if (camera.pitch < -89.f)
            camera.pitch = -89.f;

        if (camera.yaw > 180.f)
            camera.yaw -= 360.f;
        if (camera.yaw < -180.f)
            camera.yaw += 360.f;

        camera.changedView = true;

        // glm::vec3 front;
        // front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
        // front.y = sin(glm::radians(camera.pitch));
        // front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));

        // camera.cameraWorldCoords.back = glm::normalize(-front);
    }
}

void Renderer::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (camera.enterCam) {
        if (camera.fov_y >= 20.0f && camera.fov_y <= 120.0f)
            camera.fov_y -= static_cast<float>(yoffset) * 1.5f;
        if (camera.fov_y <= 20.0f)
            camera.fov_y = 20.0f;
        if (camera.fov_y >= 120.0f)
            camera.fov_y = 120.0f;

        camera.changedPerspect = true;
        // camera.updatePerspectiveMatrix(camera.fov_y, camera.aspect_ratio, camera.zNear, camera.zFar);
    }
}

//GLFW������ʹ��thisָ����ָ������࣬�������Ϊstatic����(���ܵ���this)
void Renderer::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    // app->framebufferResized = true;//��ʽ����Ϊtrue(recreate swap chain����ҲҪ����)
    //��������������ô����ڱ仯ʱҲ�ܸ��ż�ʹ�仯��glfw��resizeʱ����ͣevent loop���������ÿ�ƶ�һ�㶼��������call back����������ӴӶ��ﵽʵʱ�ı��Ч��
    app->recreateSwapChain();//��һ������recreate swap chain
    app->drawFrame();        //�ڶ�������ʵ�ʵ���Ⱦ
}

// void Renderer::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
//     ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
// }

void Renderer::initWindow() {
    glfwInit();                                  //����glfw lib
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//��ֹ����openGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);   //������Ĵ��ڴ�С
    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Lemon_Renderer", nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);                           //�ú������Դ������һ��ָ�룬���this����
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);//Resize�˾ͻ����framebufferResizeCallback����

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);

    // key callback for imgui
    // glfwSetKeyCallback(window, keyCallback);
}

//compile new shader file  (ע��ʹ�þ���·����)
void Renderer::compileShader() {
    const char* batchFilePath = "C:/Users/Lemonade/Desktop/Lemon_Render/shaders/compile.bat";
    if (system(batchFilePath) != 0)
        throw std::runtime_error("failed to execute shader batch file!");
}

//���ã�ָ��application info��extension info
void Renderer::createInstance() {
    //validation layer���
    if (enableValidationLayers && !checkValidationLayerSupport())
        throw std::runtime_error("validation layers requested, but not available!");

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};//�Ǳ��룬��֮������õ��ض����棬���������Ż�
    appInfo.pApplicationName   = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};//����Ľṹ�壬����driver��Ҫʹ�õ�ȫ����չ��У���
    createInfo.pApplicationInfo = &appInfo;

    //ָ����Ҫ��ȫ����չ(global extensions)
    auto extensions                    = getInstanceExtensions();//Ŀǰ�� glfw + validation layer
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    //ָ��ȫ��У���(global validation layers)
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        //����vulkan���ڴ���ʵ����ͬʱ����һ��debug�ص���pNext�����ݶ������Ϣ���Ա�����չ�������ض�����
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext             = nullptr;
    }

    //���������б�Ҫ��Ϣ�����������vkCreateInstance������vkʵ����instance�У�
    //��ͨ������ֵ(����ΪVkResult)��ȷ���Ƿ�ɹ�ִ��
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("failed to create instance!");
    }
}

std::vector<const char*> Renderer::getInstanceExtensions() {
    uint32_t     glfwExtensionCount = 0;
    const char** glfwExtensions;
    //glfwָ������չ����ʹ��
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    //����Ҫ��extensionsת��Ϊ��Ҫ��extension��vector��������������
    //����ʹ�õ�����ʼ�������ͽ����������Ĺ��췽��
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    //У������չ����ѡ���Ƿ����
    if (enableValidationLayers)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);//��������һ���ַ���

    return extensions;
}

void Renderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo       = {};
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

void Renderer::setupDebugMessenger() {
    if (!enableValidationLayers) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
        throw std::runtime_error("failed to set up debug messenger!");
}

void Renderer::createSurface() {
    VkWin32SurfaceCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
    createInfo.hwnd      = glfwGetWin32Window(window);
    createInfo.hinstance = GetModuleHandle(nullptr);

    VK_CHECK(vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface));

    if (surface == VK_NULL_HANDLE)
        throw std::runtime_error("fail in surface creation!\n");

    //ֱ����glfw��ʵ�����£�����ȫ����Ҫ�ˣ����Լ�ʵ�ֵĻ���Ҫ�õ�windows�е���ز��������ϣ�
    // if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    // throw std::runtime_error("failed to create window surface!");
}

void Renderer::pickPhysicalCard() {
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
            // msaaSamples    = getMaxUsableSampleCount();
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool Renderer::isDeviceSuitable(VkPhysicalDevice device) {
    //�����physical device��û�ж�Ӧ��֧��ͼ�β����Ķ����岢��¼
    QueueFamilyIndices indices = findQueueFamilies(device);

    //�����physical device֧��֧�ֶ�Ӧ����չ
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    //���extension�е�swap chain�Ƿ�adequate(����֧��һ��ͼƬ��ʽ��һ�ֳ�����ʽ)
    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate                        = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    //����Ƿ�֧��anisotropic filtering
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

QueueFamilyIndices Renderer::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t           queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    VkBool32 presentSupport = false;

    //�����ж��������ҵ�֧��queueFamily��Ҫ������Ķ�������壬�ֱ��¼��index
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)//ͨ��λ�����ж�
            indices.graphicsFamily = i;

        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
            indices.presentFamily = i;

        if (indices.isComplete())//�ҵ��˾�ֱ���˳�����
            break;
        ++i;
    }

    return indices;
}

//Querying details of swap chain support
SwapChainSupportDetails Renderer::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    //����swap chain��ѯ����������device��surface�������ؼ�����
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);//get capabilities

    uint32_t formatCount;//get format
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;//get presentMode
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void Renderer::createLogicalDevice() {
    vulkanDevice = new LR::VulkanDevice(physicalDevice);
    setDeviceEnabledFeatures();
    setDeviceEnabledExtensions();
    VK_CHECK(vulkanDevice->createLogicalDevice(enabledDeviceFeatures, enabledDeviceExtensions, nullptr));
    device = vulkanDevice->logicalDevice;

    //�ڶ��������Ƕ�������±꣬�����������Ǹö������еĶ�Ӧ�����±꣬������ʱ������Ϊһ����ֵ
    vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &graphicsQueue);
    vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &presentQueue);

    //��ʼ��clear color
    clearValues[0].color        = {{.1f, .1f, .1f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
}

void Renderer::setDeviceEnabledFeatures() {
    if (vulkanDevice->physicalFeatures.samplerAnisotropy)
        enabledDeviceFeatures.samplerAnisotropy = VK_TRUE;//����ʹ��anisotropic filtering
    // enabledDeviceFeatures.sampleRateShading = VK_TRUE;// enable sample shading feature for the device
}

void Renderer::setDeviceEnabledExtensions() {
    // none, SwapchainKHR is set in vulkanDevice->createLogicalDevice
}

// ��ʼ��swapchain���ݣ�����instance,device�ȣ���ȡqueue��format
void Renderer::initSwapChain() {
    swapchain.connect(instance, physicalDevice, device);
    swapchain.initSurfaceAndQueue(surface, window);// �����ж�format��ѡ��Ŀǰѡ�����RGBA_SRGB(����)��RGBA_NORM�Ļ���䰵
}

// �������ؽ�swapChain����
void Renderer::setupSwapChain() {
    swapchain.create(&SCR_WIDTH, &SCR_HEIGHT);// ���Ա��ı�
    // ��ʼ����camera��PerspectiveMatrix
    camera.updatePerspectiveMatrix(camera.fov_y, static_cast<float>(swapchain.extent.width) / swapchain.extent.height, camera.zNear, camera.zFar);
}

void Renderer::createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = LR::initializers::semaphoreCreateInfo();
    VkFenceCreateInfo     fenceInfo     = LR::initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

    // clang-format off
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS 
            || vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS 
            || vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
    // clang-format on
}

void Renderer::createCommandPool() {
    // QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.graphics;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool!");
}

void Renderer::createCommandBuffers() {
    // Create one command buffer for each swap chain image and reuse for rendering
    // drawCmdBuffers.resize(swapchain.imageCount);

    // �ĳ�ÿ֡recordһ�κ�ֻ��Ҫ���֡����cmdBuffer
    drawCmdBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = commandPool;                    //��������command pool
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;//PRIMARY��Can be submitted to a queue for execution, but cannot be called from other command buffers.
    allocInfo.commandBufferCount = static_cast<uint32_t>(drawCmdBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, drawCmdBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers!");
}

void Renderer::destoryCommandBuffers() {
    // ֱ���������е�cmd buffer��Ȼ����������
    vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

/**
* @brief ��beginRenderPass, pipeline, vertexBuffer/indexBuffer, dynamic parts, descriptorsets, Ȼ��draw, ���endRenderPass
*/
// void Renderer::recordCommandBuffers() {
//     std::array<VkClearValue, 2> clearValues{};//��������attachments��color��depth
//     clearValues[0].color        = {{.1f, .1f, .1f, 1.0f}};
//     clearValues[1].depthStencil = {1.0f, 0};

//     VkRenderPassBeginInfo renderPassBeginInfo = LR::initializers::renderPassBeginInfo();
//     renderPassBeginInfo.renderPass            = renderPass;
//     renderPassBeginInfo.renderArea.offset     = {0, 0};
//     renderPassBeginInfo.renderArea.extent     = swapchain.extent;
//     renderPassBeginInfo.clearValueCount       = static_cast<uint32_t>(clearValues.size());
//     renderPassBeginInfo.pClearValues          = clearValues.data();//VK_ATTACHMENT_LOAD_OP_CLEARʹ�õ���ɫ

//     // imGui->newFrame();
//     // imGui->updateBuffers();

//     // �ȴ�ǰ���commandBuffer����(to be optimized)
//     vkDeviceWaitIdle(device);

//     for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
//         VkCommandBufferBeginInfo beginInfo = LR::initializers::commandBufferBeginInfo();
//         if (vkBeginCommandBuffer(drawCmdBuffers[i], &beginInfo) != VK_SUCCESS)
//             throw std::runtime_error("failed to begin recording command buffer!");

//         renderPassBeginInfo.framebuffer = swapChainFramebuffers[i];//attachments

//         //¼�Ʋ����ĺ�������vkcmd��ͷ������voidֵ�����¼�Ʋ���û�м�����Ĳ���
//         vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
//         vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

//         VkBuffer     vertexBuffers[] = {vertexBuffer.buffer};
//         VkDeviceSize offsets[]       = {0};
//         vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, vertexBuffers, offsets);

//         vkCmdBindIndexBuffer(drawCmdBuffers[i], indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

//         //dynamic����
//         VkViewport viewport = LR::initializers::viewport(0.f, 0.f, static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height), 0.0f, 1.0f);
//         vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
//         VkRect2D scissor = LR::initializers::rect2D(swapchain.extent.width, swapchain.extent.height, 0, 0);
//         vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

//         //bind descriptor set
//         vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

//         //��3����������ʵ����Ⱦ��д1��ʾ������������4����������gl_VertexIndex����Сֵ����5����������ʵ����Ⱦ������gl_InstanceIndex����Сֵ
//         //vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
//         vkCmdDrawIndexed(drawCmdBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);//ʹ����������

//         // ����ui
//         imGui->drawFrame(drawCmdBuffers[i]);

//         vkCmdEndRenderPass(drawCmdBuffers[i]);

//         if (vkEndCommandBuffer(drawCmdBuffers[i]) != VK_SUCCESS)
//             throw std::runtime_error("failed to end recording command buffer!");
//     }
// }

/**
* @brief ��beginRenderPass, pipeline, vertexBuffer/indexBuffer, dynamic parts, descriptorsets, Ȼ��draw, ���endRenderPass
*/
void Renderer::recordCommandBuffer(VkCommandBuffer& cmdBuffer, uint32_t imageIndex) {
    VkRenderPassBeginInfo renderPassBeginInfo = LR::initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass            = renderPass;
    renderPassBeginInfo.renderArea.offset     = {0, 0};
    renderPassBeginInfo.renderArea.extent     = swapchain.extent;
    renderPassBeginInfo.clearValueCount       = static_cast<uint32_t>(clearValues.size());
    renderPassBeginInfo.pClearValues          = clearValues.data();//VK_ATTACHMENT_LOAD_OP_CLEARʹ�õ���ɫ

    VkViewport viewport = LR::initializers::viewport(0.f, 0.f, static_cast<float>(swapchain.extent.width), static_cast<float>(swapchain.extent.height), 0.0f, 1.0f);
    VkRect2D   scissor  = LR::initializers::rect2D(swapchain.extent.width, swapchain.extent.height, 0, 0);

    VkCommandBufferBeginInfo beginInfo = LR::initializers::commandBufferBeginInfo();
    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("failed to begin recording command buffer!");

    renderPassBeginInfo.framebuffer = swapChainFramebuffers[imageIndex];//attachments

    //¼�Ʋ����ĺ�������vkcmd��ͷ������voidֵ�����¼�Ʋ���û�м�����Ĳ���
    vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ȫ����gltfScene������
    // vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    // VkBuffer     vertexBuffers[] = {vertexBuffer.buffer};
    // VkDeviceSize offsets[]       = {0};
    // vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
    // vkCmdBindIndexBuffer(cmdBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    //dynamic����
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    // Bind scene matrices descriptor to set 0 (set 1��scene��draw call����)
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[currentFrame], 0, nullptr);

    //��3����������ʵ����Ⱦ��д1��ʾ������������4����������gl_VertexIndex����Сֵ����5����������ʵ����Ⱦ������gl_InstanceIndex����Сֵ
    //vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);
    // vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);//ʹ����������

    // draw scene
    glTFScene->draw(cmdBuffer, pipelineLayout);

    // ����ui
    imGui->drawFrame(cmdBuffer, currentFrame);

    vkCmdEndRenderPass(cmdBuffer);

    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed to end recording command buffer!");
}

void Renderer::createDepthResources() {
    VkFormat depthFormat = findDepthFormat();
    // createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    createImage(swapchain.extent.width, swapchain.extent.height, 1, VK_SAMPLE_COUNT_1_BIT, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

    //optional��
    transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

VkFormat Renderer::findDepthFormat() {
    return findSupportedFormat(
        // {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat Renderer::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
    }
    throw std::runtime_error("failed to find supported format!");
}

//perform image layout transitions������Imageʱ��Ҫ���⿼��layout�Ĺ������⣩
void Renderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    //����ķ�������image memory barrier
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;//��������transition image layout
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//��������transfer queue family ownership
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;//ע�ⲻת�ƵĻ����ﲻ������ΪĬ��ֵ��
    barrier.image               = image;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    } else
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;                           //ָ��barrier֮ǰ��Ҫ����ʲô����һ��û�����ƣ�
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;//ָ��barrier֮����Ҫ����ʲô

        sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;//û�����ƣ����ֱ��ѡ������Ľ׶�
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;   //transfer write����������׶η���
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;//����fragment�Ƿ�ɼ�ʱ��ȡdepth image buffer�е�����
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    //������Ľ��ͼ���https://vulkan-tutorial.com/Texture_mapping/Images
    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);//��������ǿ��ܵ�������;����ѡһ

    endSingleTimeCommands(commandBuffer);
}

bool Renderer::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

//���䲢��ʼ����cmdBuffer(��VulkanDevice���湦��һ��)
VkCommandBuffer Renderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = commandPool;
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
void Renderer::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// specify how many color and depth buffers there will be,
// how many samples to use for each of them,
// and how their contents should be handled throughout the rendering operations(���ǳ�˵��һ��pass��������pass����)
void Renderer::createRenderPass() {
    //Attachment description
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.colorFormat;
    // colorAttachment.samples = msaaSamples;//δʹ��multisampling
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;//δʹ��multisampling
    //��color & depth data����
    colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR; //clear the fb before drawing a new frame
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;//��Ⱦ���ݴ洢���ڴ��У��ҿɶ�
    //��stencil data����
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;//���漰stencil test
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    //vkImage����(textures & framebuffers�����ڴ��е�layout����)
    //ע��image��Ҫ��ǰ��Ϊ�¸������������Layout����
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;      //ָ����Ⱦ��ʼǰͼ�����ڴ��еĲ��ַ�ʽ
    colorAttachment.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;//ָ����Ⱦ������ͼ����ɵ��Ĳ��ַ�ʽ(Ҫ��present)
    // colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;//ָ����Ⱦ������ͼ����ɵ��Ĳ��ַ�ʽ(Ҫ��Ϊcolor attachment, ms��image����ֱ�ӱ�present)

    //Subpasses and attachment references�������ж�������̣�ÿ���������������ϸ������̵����ݣ������������ڴ���
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;//subpass���õ�vkAttachmentDescription���±�(��renderpass�л�ȡ)
    //����subpass������ָ����attachment��layout(subpass��ʼʱ�Զ�ת��Ϊ��layout)
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();//��depth image��ͬ
    // depthAttachment.samples        = msaaSamples;
    depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;//����Ͷ������ô洢
    depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;//������֮ǰ��depth content
    depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;//�ڶ������ϣ����±�Ϊ1
    depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    //����msaa��
    // VkAttachmentDescription colorAttachmentResolve{};
    // colorAttachmentResolve.format         = swapChainImageFormat;
    // colorAttachmentResolve.samples        = VK_SAMPLE_COUNT_1_BIT;
    // colorAttachmentResolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    // colorAttachmentResolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    // colorAttachmentResolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    // colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // colorAttachmentResolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    // colorAttachmentResolve.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // VkAttachmentReference colorAttachmentResolveRef{};
    // colorAttachmentResolveRef.attachment = 2;
    // colorAttachmentResolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;//ָ����graphics subpass
    subpass.colorAttachmentCount    = 1;
    subpass.pColorAttachments       = &colorAttachmentRef;//ֱ�ӱ�layout(locatoin = 0)ʹ��   �����
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.inputAttachmentCount    = 0;
    subpass.pInputAttachments       = nullptr;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments    = nullptr;
    subpass.pResolveAttachments     = nullptr;
    // subpass.pResolveAttachments     = &colorAttachmentResolveRef;

    //������subpass dependencies(����render pass��ʼtransition��ʱ��)
    // VkSubpassDependency dependency{};
    // //EXTERNAL��srcSubpassʹ�ñ�ʾ��Ⱦ���̿�ʼǰ�������̣���dstSubpassʹ�ñ�ʾ��Ⱦ���̽������������
    // dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;                                                                       //ָ����������subpass������external��ʾ������������
    // dependency.dstSubpass    = 0;                                                                                         //ָ����������subpass��subpass������0��ʾ�Լ���subpass
    // dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;//ָ��src������ʱ�����ȴ��������е�ͼƬ��ȡ����
    // dependency.srcAccessMask = 0;
    // dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;//ֻ��Ҫд��ɫ��ʱ��ŻῪʼ���subpass?

    std::array<VkSubpassDependency, 2> dependencies;

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    dependencies[0].dependencyFlags = 0;

    dependencies[1].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass      = 0;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].srcAccessMask   = 0;
    dependencies[1].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = 0;

    // std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies   = dependencies.data();

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");
}

void Renderer::setupImGUI() {
    imGui = new LR::ImGUI(this);
    imGui->init(static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT));
    imGui->initResources(renderPass, graphicsQueue, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT));

    // // ��װimgui glfw call backs
    // ImGui_ImplGlfw_InitForVulkan(window, true);
}

//Ϊswapchain�е�����image��Ӧ��imageview(attachment)����framebuffer
void Renderer::createFramebuffers() {
    swapChainFramebuffers.resize(swapchain.imageCount);
    for (size_t i = 0; i < swapchain.imageCount; ++i) {
        // std::array<VkImageView, 3> attachments = {
        //     colorImageView,
        //     depthImageView,
        //     swapChainImageViews[i]};
        std::array<VkImageView, 2> attachments = {
            swapchain.buffers[i].view,
            depthImageView};

        VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass      = renderPass;//attachments������������Ҫ��Ӧ
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments    = attachments.data();
        framebufferInfo.width           = swapchain.extent.width;
        framebufferInfo.height          = swapchain.extent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }
}

void Renderer::loadAssets() {
    // loadOBJModel();
    loadGLTFFile("../../../assets/models/sponza/sponza.gltf");
}

void Renderer::loadOBJModel() {
    tinyobj::attrib_t             attrib;//����vertices, normals, texcoords
    std::vector<tinyobj::shape_t> shapes;//����separate objects & their faces
    //һ��face������Ӧ�Ķ��㣬�����а�����attrib�е�indices
    std::vector<tinyobj::material_t> materials;
    std::string                      warn, err;
    //Ĭ�ϻὫ����faces��������ι��ɵ�faces
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
        throw std::runtime_error(warn + err);

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    //��ӵ�vertex�����indices������
    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {//ʹ����Щ��������ȡʵ�ʵ�vertex attributes
            Vertex vertex{};
            vertex.position = {
                attrib.vertices[3 * index.vertex_index],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]};
            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index],
                1.f - attrib.texcoords[2 * index.texcoord_index + 1]};//obj�Ĳ���ͼƬĬ��������0���������ӣ�������������top to bottom��������0
            vertex.color = {1.f, 1.f, 1.f};

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());//��¼���������Ӧ�����
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
            //indices.push_back(indices.size());		//����������0��ʼ��
        }
    }
}

// needs to be customized
void Renderer::loadGLTFFile(std::string filename) {
    glTFScene = new LR::VulkanglTFScene();

    tinygltf::Model glTFInput;
	tinygltf::TinyGLTF gltfContext;
	std::string error, warning;

	this->device = device;

	bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, filename);

	// Pass some Vulkan resources required for setup and rendering to the glTF model loading class
	glTFScene->vulkanDevice = vulkanDevice;
	glTFScene->copyQueue    = graphicsQueue;

	size_t pos = filename.find_last_of('/');
	glTFScene->path = filename.substr(0, pos);

	std::vector<uint32_t> indexBuffer;
	std::vector<LR::VulkanglTFScene::Vertex> vertexBuffer;

	if (fileLoaded) {
		glTFScene->loadImages(glTFInput);
		glTFScene->loadMaterials(glTFInput);
		glTFScene->loadTextures(glTFInput);
		const tinygltf::Scene& scene = glTFInput.scenes[0];
		for (size_t i = 0; i < scene.nodes.size(); i++) {
			const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
			glTFScene->loadNode(node, glTFInput, nullptr, indexBuffer, vertexBuffer);
		}
	}
	else {
		throw std::runtime_error("Could not open the glTF file.\n\nMake sure the assets submodule has been checked out and is up-to-date.");
		return;
	}

	// Create and upload vertex and index buffer
	// We will be using one single vertex buffer and one single index buffer for the whole glTF scene
	// Primitives (of the glTF model) will then index into these using index offsets

	size_t vertexBufferSize = vertexBuffer.size() * sizeof(LR::VulkanglTFScene::Vertex);
	size_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
	glTFScene->indices.count = static_cast<uint32_t>(indexBuffer.size());

	struct StagingBuffer {
		VkBuffer buffer;
		VkDeviceMemory memory;
	} vertexStaging, indexStaging;

	// Create host visible staging buffers (source)
	VK_CHECK(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		vertexBufferSize,
		&vertexStaging.buffer,
		&vertexStaging.memory,
		vertexBuffer.data()));
	// Index data
	VK_CHECK(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		indexBufferSize,
		&indexStaging.buffer,
		&indexStaging.memory,
		indexBuffer.data()));

	// Create device local buffers (target)
	VK_CHECK(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		vertexBufferSize,
		&glTFScene->vertices.buffer,
		&glTFScene->vertices.memory));
	VK_CHECK(vulkanDevice->createBuffer(
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		indexBufferSize,
		&glTFScene->indices.buffer,
		&glTFScene->indices.memory));

	// Copy data from staging buffers (host) do device local buffer (gpu)
	VkCommandBuffer copyCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkBufferCopy copyRegion = {};

	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		vertexStaging.buffer,
		glTFScene->vertices.buffer,
		1,
		&copyRegion);

	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(
		copyCmd,
		indexStaging.buffer,
		glTFScene->indices.buffer,
		1,
		&copyRegion);

	vulkanDevice->flushCommandBuffer(copyCmd, graphicsQueue, true);

	// Free staging resources
	vkDestroyBuffer(device, vertexStaging.buffer, nullptr);
	vkFreeMemory(device, vertexStaging.memory, nullptr);
	vkDestroyBuffer(device, indexStaging.buffer, nullptr);
	vkFreeMemory(device, indexStaging.memory, nullptr);
}

void Renderer::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    // create staging buffer
    LR::VulkanBuffer stagingBuffer;
    vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, bufferSize, vertices.data());
    // create vertex buffer
    vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &vertexBuffer, bufferSize);
    vulkanDevice->copyBuffer(&stagingBuffer, &vertexBuffer, graphicsQueue);
    //���staging buffer
    stagingBuffer.destroy();
}

void Renderer::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    // create staging buffer
    LR::VulkanBuffer stagingBuffer;
    vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, bufferSize, indices.data());
    // create index buffer
    vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indexBuffer, bufferSize);
    vulkanDevice->copyBuffer(&stagingBuffer, &indexBuffer, graphicsQueue);
    //���staging buffer
    stagingBuffer.destroy();
}

void Renderer::createUniformBuffers() {
    VkDeviceSize bufferSize = sizeof(ShaderData);
    uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                            &uniformBuffers[i],
                                            bufferSize));
        VK_CHECK(uniformBuffers[i].map());//uniform buffer�ǵñ���map״̬
    }
}

void Renderer::updateUniformBuffer(uint32_t currentImage) {
    static auto startTime   = std::chrono::high_resolution_clock::now();//ֻ�г�ʼ����һ��
    auto        currentTime = std::chrono::high_resolution_clock::now();
    float       time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    ShaderData ubo{};
    // ubo.model      = glm::mat4(1.f);     //scene��������model�õ���push_constant�������
    ubo.view       = camera.matrices.view;
    ubo.projection = camera.matrices.perspective;
    ubo.viewPos    = glm::vec4(camera.position, 1.0f);
    uniformBuffers[currentImage].copyFrom(&ubo, sizeof(ShaderData));
}

void Renderer::updateImGUI(float deltaTime) {
    ImGuiIO& io    = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(SCR_WIDTH), static_cast<float>(SCR_HEIGHT));
    io.DeltaTime   = deltaTime;

    // handle window input(ע����걻�����ʱ�򲻱�imguiʹ��)
    if (!camera.enterCam) {
        // mouse
        io.WantCaptureMouse = true;
        io.MousePos         = ImVec2(static_cast<float>(camera.mouse.x), static_cast<float>(camera.mouse.y));
        io.MouseDown[0]     = camera.mouse.left;
        io.MouseDown[1]     = camera.mouse.right;
        io.MouseDown[2]     = camera.mouse.middle;
        // keyboard
        io.WantCaptureKeyboard = true;
        // todo

    } else {
        // mouse
        io.WantCaptureMouse = false;
        // keyboard
        io.WantCaptureKeyboard = false;
    }

    imGui->newFrame();
    imGui->updateBuffers(currentFrame);

    //-----------------------------------------------------------------------------(���pre record����ʱ����)
    // �жϴ����Ƿ��ڱ��϶��������С(��ͣ+��ק)
    // ImGuiContext& g        = *GImGui;
    // bool          resize   = (g.MovingWindow != nullptr);
    // bool          dragging = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && ImGui::IsMouseDragging(0);// 0����left

    // ��ʼ�µ�һ֡(ͬʱ�������״̬�б仯������recordCmdBuffer����Ϊ�����draw��¼����֮ǰ��״̬)
    // imGui->newFrame();
    // if (imGui->updateBuffers(fence) || dragging || resize) {
    // if (imGui->updateBuffers(fence) || dragging) {
    //     recordCommandBuffers();
    // }
}

void Renderer::createTextureImage() {
    // 1. load texture
    int          texWidth, texHeight, texChannels;
    stbi_uc*     pixels    = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;//ÿ������4byte
    if (!pixels)
        throw std::runtime_error("failed to load texture image!");

    //ȷ�������Ӧ�����mipmap�ȼ�����ʼͼ���Ӧ1
    mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

    //����staging buffer���������ݽ�ȥ
    LR::VulkanBuffer stagingBuffer;
    vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               &stagingBuffer,
                               imageSize,
                               pixels);
    stbi_image_free(pixels);

    // VK_IMAGE_USAGE_SAMPLED_BIT ��ʾϣ��shader�ɻ�ȡ��Щ����
    createImage(texWidth, texHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
    copyBufferToImage(stagingBuffer.buffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    //��texture image transitionΪshader���Զ�ȡ��״̬
    // transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
    // 	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    //����mipmap����transitionΪshader�ɶ���״̬��������������һ��ת����
    generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

    stagingBuffer.destroy();

    // 2. create texture image view
    textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);

    // 3. create texture sampler
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter     = VK_FILTER_LINEAR;//����oversampling
    samplerInfo.minFilter     = VK_FILTER_LINEAR;//����undersampling
    samplerInfo.addressModeU  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW  = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.maxAnisotropy = 1.0f;
    if (vulkanDevice->physicalFeatures.samplerAnisotropy) {
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy    = vulkanDevice->physicalProperties.limits.maxSamplerAnisotropy;
    }
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;//clamp to borderģʽ�г����߽����ɫ
    samplerInfo.unnormalizedCoordinates = VK_FALSE;                        //false����[0,1)�� ture��[0, texWidth) & [0, texHeight)
    samplerInfo.compareEnable           = VK_FALSE;                        //true��texels����ĳ��ֵ�Ƚϣ��������filtering������PCF
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod                  = 0;// ��С��mipmap�ȼ�
    samplerInfo.maxLod                  = static_cast<float>(mipLevels);
    samplerInfo.mipLodBias              = 0.0f;// Optional

    if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
        throw std::runtime_error("failed to create texture sampler!");
}

void Renderer::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
    //����image object
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;//1dͼ��洢������ݶȣ�2d����3d voxel volumes
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels     = mipLevels;
    imageInfo.arrayLayers   = 1;
    imageInfo.format        = format;
    imageInfo.tiling        = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = usage;
    imageInfo.samples       = numSamples;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    //�����ڴ棨�Ȳ�ѯҪ�󣬺�����ڴ棩
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    //�󶨷�����ڴ浽image object��
    vkBindImageMemory(device, image, imageMemory, 0);
}

VkImageView Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture image view!");
    }

    return imageView;
}

void Renderer::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
    // Check if image format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

    if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error("texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = image;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.subresourceRange.levelCount     = 1;

    int32_t mipWidth  = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);

        VkImageBlit blit{};
        blit.srcOffsets[0]                 = {0, 0, 0};
        blit.srcOffsets[1]                 = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel       = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount     = 1;
        blit.dstOffsets[0]                 = {0, 0, 0};
        blit.dstOffsets[1]                 = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
        blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel       = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount     = 1;

        vkCmdBlitImage(commandBuffer,
                       image,
                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1,
                       &blit,
                       VK_FILTER_LINEAR);

        barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);

        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    endSingleTimeCommands(commandBuffer);
}

//�ҵ����ʵ�memoryType����graphics card���з���
uint32_t Renderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    //����available types of memory
    VkPhysicalDeviceMemoryProperties memProperties;//��ʱֻ�������е�memoryType��ݲ�����memory heap��
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;//suitable for the buffer & has all of the properties we need
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

void Renderer::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};//ָ��buffer����Щ���ֻᱻcopy��image����Щ����
    region.bufferOffset      = 0;
    region.bufferRowLength   = 0;//0��ʾ���ص�����������
    region.bufferImageHeight = 0;//ͬ��

    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void Renderer::setupDescriptors() {
    // // 1. pool
    // std::vector<VkDescriptorPoolSize> poolSizes = {
    //     LR::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)),
    //     LR::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT))};
    // // maxSet = 2��ÿ��Frame_in_flight����һ��descriptorset���������������:uniform buffer �� image sampler
    // VkDescriptorPoolCreateInfo descriptorPoolInfo = LR::initializers::descriptorPoolCreateInfo(poolSizes, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT));
    // VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    // // 2. layout
    // std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
    //     LR::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
    //     LR::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)};
    // VkDescriptorSetLayoutCreateInfo descriptorLayoutCI = LR::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
    // VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorLayoutCI, nullptr, &descriptorSetLayout));

    // // 3. set
    // std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    // VkDescriptorSetAllocateInfo        allocInfo = LR::initializers::descriptorSetAllocateInfo(descriptorPool, layouts.data(), static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT));
    // descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    // VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()));

    // // update descriptorsets data
    // VkDescriptorImageInfo descriptorImageInfo{};
    // descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    // descriptorImageInfo.imageView   = textureImageView;
    // descriptorImageInfo.sampler     = textureSampler;
    // for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    //     std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
    //         LR::initializers::writeDescriptorSet(descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers[i].descriptorInfo),
    //         LR::initializers::writeDescriptorSet(descriptorSets[i], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &descriptorImageInfo)};
    //     vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    // }

    // scene �汾��
    // 1. pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        LR::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)),                   // �����ǲ��ǿ��Ըĳ�1
        LR::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(glTFScene->materials.size()) * 2)};//size��sets��ÿ��set������descriptor(color map & normal map)
    // ÿ��Frame_in_flight��matrices����һ��descriptorset��ÿ��image/texture����һ��descriptorset
    const uint32_t             maxSetCount        = static_cast<uint32_t>(glTFScene->images.size()) + 2;
    VkDescriptorPoolCreateInfo descriptorPoolInfo = LR::initializers::descriptorPoolCreateInfo(poolSizes, maxSetCount);
    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    // 2. setting layout(2 parts : matrices & material textures)
    //  matrices �� layout
    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        LR::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)};
    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = LR::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.matrices));

    // textures �� layout
    setLayoutBindings = {
        // Color map
        LR::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),
        // Normal map
        LR::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
    };
    descriptorSetLayoutCI.pBindings    = setLayoutBindings.data();
    descriptorSetLayoutCI.bindingCount = 2;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayouts.textures));

    // 3. set
    // set for matrices
    descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    // ���ﲻ֪��Ϊʲô���������д��ȥд�ᵼ�µڶ���descriptorSet������Ϊ COMBINED_IMAGE_SAMPLER
    VkDescriptorSetAllocateInfo allocInfo = LR::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets[0]));
    allocInfo = LR::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.matrices, 1);
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets[1]));
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
            LR::initializers::writeDescriptorSet(descriptorSets[i], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers[i].descriptorInfo)};
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
    }


    // sets for textures
    for (auto& material : glTFScene->materials) {
		const VkDescriptorSetAllocateInfo allocInfo = LR::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.textures, 1);
		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &material.descriptorSet));
		VkDescriptorImageInfo colorMap = glTFScene->getTextureDescriptor(material.baseColorTextureIndex);
		VkDescriptorImageInfo normalMap = glTFScene->getTextureDescriptor(material.normalTextureIndex);
		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
			LR::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &colorMap),
			LR::initializers::writeDescriptorSet(material.descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &normalMap),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}


// ֮ǰʹ�õĺ���
// void Renderer::createGraphicsPipeline() {
//      //* programmable stages(vertex/fragment shader)
//     auto vertShaderCode = readFile("../../../shaders/vert.spv");
//     auto fragShaderCode = readFile("../../../shaders/frag.spv");

//     VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
//     VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

//     VkPipelineShaderStageCreateInfo vertShaderStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
//     vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
//     vertShaderStageInfo.module = vertShaderModule;
//     vertShaderStageInfo.pName  = "main";//��ں���(ʵ��invoke(����)�ĺ���)
//     //vertShaderStageInfo.pSpecializationInfo = nullptr;		//optional ����Ҫ(���̳�)

//     VkPipelineShaderStageCreateInfo fragShaderStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
//     fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
//     fragShaderStageInfo.module = fragShaderModule;
//     fragShaderStageInfo.pName  = "main";

//     std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertShaderStageInfo, fragShaderStageInfo};

//     //* fixed-function stages
//     //dynamic state(�����ڻ���ʱ��̬�ı������������������pipeline���ı�)
//     std::vector<VkDynamicState>      dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
//     VkPipelineDynamicStateCreateInfo dynamicState        = LR::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

//     auto                                 bindingDescritions    = Vertex::getBindingDescriptions();
//     auto                                 attributeDescriptions = Vertex::getAttributeDescriptions();
//     VkPipelineVertexInputStateCreateInfo vertexInputState      = LR::initializers::pipelineVertexInputStateCreateInfo(bindingDescritions, attributeDescriptions);

//     VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = LR::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
//     VkPipelineViewportStateCreateInfo viewportState = LR::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
//     VkPipelineRasterizationStateCreateInfo rasterizationState = LR::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
//     VkPipelineMultisampleStateCreateInfo multisampleState = LR::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
//     VkPipelineDepthStencilStateCreateInfo depthStencilState = LR::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
//     VkPipelineColorBlendAttachmentState blendAttachmentState = LR::initializers::pipelineColorBlendAttachmentState(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);
//     VkPipelineColorBlendStateCreateInfo colorBlendState = LR::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
//     const VkPipelineLayoutCreateInfo pipelineLayoutCI = LR::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
//     VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

//     //����graphics pipeline
//     VkGraphicsPipelineCreateInfo pipelineCI = LR::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
//     pipelineCI.pInputAssemblyState          = &inputAssemblyState;
//     pipelineCI.pRasterizationState          = &rasterizationState;
//     pipelineCI.pColorBlendState             = &colorBlendState;
//     pipelineCI.pMultisampleState            = &multisampleState;
//     pipelineCI.pViewportState               = &viewportState;
//     pipelineCI.pDepthStencilState           = &depthStencilState;
//     pipelineCI.pDynamicState                = &dynamicState;
//     pipelineCI.stageCount                   = static_cast<uint32_t>(shaderStages.size());
//     pipelineCI.pStages                      = shaderStages.data();
//     pipelineCI.pVertexInputState            = &vertexInputState;

//     if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &graphicsPipeline) != VK_SUCCESS)
//         throw std::runtime_error("failed to create graphics pipeline!");

//     vkDestroyShaderModule(device, vertShaderModule, nullptr);
//     vkDestroyShaderModule(device, fragShaderModule, nullptr);
// }

// for scene usage
void Renderer::createGraphicsPipelines() {
    //* programmable stages(vertex/fragment shader)
    // auto vertShaderCode = readFile("../../../shaders/vert.spv");
    // auto fragShaderCode = readFile("../../../shaders/frag.spv");
    auto vertShaderCode = readFile("../../../shaders/scenevert.spv");
    auto fragShaderCode = readFile("../../../shaders/scenefrag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";//��ں���(ʵ��invoke(����)�ĺ���)
    //vertShaderStageInfo.pSpecializationInfo = nullptr;		//optional ����Ҫ(���̳�)

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{vertShaderStageInfo, fragShaderStageInfo};

    // ���ƶ�pipeline layout
    // Pipeline layout uses both descriptor sets (set 0 = matrices, set 1 = material)
    std::array<VkDescriptorSetLayout, 2> setLayouts = { descriptorSetLayouts.matrices, descriptorSetLayouts.textures };
	VkPipelineLayoutCreateInfo pipelineLayoutCI = LR::initializers::pipelineLayoutCreateInfo(setLayouts.data(), static_cast<uint32_t>(setLayouts.size()));
    // We will use push constants to push the local matrices of a primitive to the vertex shader(ʵ������record��ʱ����drawNode����)
	VkPushConstantRange pushConstantRange = LR::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), 0);
	// Push constant ranges are part of the pipeline layout
	pipelineLayoutCI.pushConstantRangeCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &pipelineLayout));

    //* fixed-function stages
    //dynamic state(�����ڻ���ʱ��̬�ı������������������pipeline���ı�)
    std::vector<VkDynamicState>      dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState        = LR::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);

    // // vertex input info(������������vertex����ʱ����  --ֱ�Ӵ��뷵�ص���ʱֵ�����δ��ֵ������)
    // auto                                 bindingDescritions    = Vertex::getBindingDescriptions();
    // auto                                 attributeDescriptions = Vertex::getAttributeDescriptions();
    // VkPipelineVertexInputStateCreateInfo vertexInputState      = LR::initializers::pipelineVertexInputStateCreateInfo(bindingDescritions, attributeDescriptions);

    const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
		LR::initializers::vertexInputBindingDescription(0, sizeof(LR::VulkanglTFScene::Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};
	const std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
		LR::initializers::vertexInputAttributeDescription(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LR::VulkanglTFScene::Vertex, pos)),
		LR::initializers::vertexInputAttributeDescription(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LR::VulkanglTFScene::Vertex, normal)),
		LR::initializers::vertexInputAttributeDescription(0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LR::VulkanglTFScene::Vertex, uv)),
		LR::initializers::vertexInputAttributeDescription(0, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LR::VulkanglTFScene::Vertex, color)),
		LR::initializers::vertexInputAttributeDescription(0, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(LR::VulkanglTFScene::Vertex, tangent)),
	};
	VkPipelineVertexInputStateCreateInfo vertexInputState = LR::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings, vertexInputAttributes);

    // input assembly(���vertex shader�д���Ķ���)  //VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: �������Ҳ�����
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = LR::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);

    // viewports and scissors(ֻ����viewport��scissor�ĸ���������Ķ�̬��)
    VkPipelineViewportStateCreateInfo viewportState = LR::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

    // rasterizer(��Ⱦģʽ��˳��ʱ�룬depthBias��)
    VkPipelineRasterizationStateCreateInfo rasterizationState = LR::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);

    // multisampling(ͨ��sampleShadingEnable������multisample)
    VkPipelineMultisampleStateCreateInfo multisampleState = LR::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);

    // depth & stencil testing(����Ĭ�ϻ�û�п���stencilTest)
    VkPipelineDepthStencilStateCreateInfo depthStencilState = LR::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    // color blend attachment state(blendEnable����Ϊfalse,��ʾfragment��������ɫ��ֱ��д��framebuffer��)
    VkPipelineColorBlendAttachmentState blendAttachmentState = LR::initializers::pipelineColorBlendAttachmentState(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, VK_FALSE);

    // color blend state(����һ��������)
    VkPipelineColorBlendStateCreateInfo colorBlendState = LR::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);

    //* render pass(�ڸú���ǰ�Ѿ��������)

    //����graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineCI = LR::initializers::pipelineCreateInfo(pipelineLayout, renderPass, 0);
    pipelineCI.pInputAssemblyState          = &inputAssemblyState;
    pipelineCI.pRasterizationState          = &rasterizationState;
    pipelineCI.pColorBlendState             = &colorBlendState;
    pipelineCI.pMultisampleState            = &multisampleState;
    pipelineCI.pViewportState               = &viewportState;
    pipelineCI.pDepthStencilState           = &depthStencilState;
    pipelineCI.pDynamicState                = &dynamicState;
    pipelineCI.stageCount                   = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages                      = shaderStages.data();
    pipelineCI.pVertexInputState            = &vertexInputState;


    // Instead of using a few fixed pipelines, we create one pipeline for each material using the properties of that material
    for (auto &material : glTFScene->materials) {

		struct MaterialSpecializationData {
			VkBool32 alphaMask;
			float alphaMaskCutoff;
		} materialSpecializationData;

		materialSpecializationData.alphaMask = material.alphaMode == "MASK";
		materialSpecializationData.alphaMaskCutoff = material.alphaCutOff;

		// POI: Constant fragment shader material parameters will be set using specialization constants
		std::vector<VkSpecializationMapEntry> specializationMapEntries = {
			LR::initializers::specializationMapEntry(0, offsetof(MaterialSpecializationData, alphaMask), sizeof(MaterialSpecializationData::alphaMask)),
			LR::initializers::specializationMapEntry(1, offsetof(MaterialSpecializationData, alphaMaskCutoff), sizeof(MaterialSpecializationData::alphaMaskCutoff)),
		};
		VkSpecializationInfo specializationInfo = LR::initializers::specializationInfo(specializationMapEntries, sizeof(materialSpecializationData), &materialSpecializationData);
		shaderStages[1].pSpecializationInfo = &specializationInfo;

		// For double sided materials, culling will be disabled
		rasterizationState.cullMode = material.doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &material.pipeline));
	}

    //����ͬʱ�������pipelines
    // if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCI, nullptr, &graphicsPipeline) != VK_SUCCESS)
        // throw std::runtime_error("failed to create graphics pipeline!");

    vkDestroyShaderModule(device, vertShaderModule, nullptr);
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
}

//����binary�ļ���helper function
std::vector<char> Renderer::readFile(const std::string& filename) {
    //ate���ļ�ĩβ��ʼ����������λ���������ļ���С���Ӷ�ָ��buffer����binary����Ϊ2�����ļ�����
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
        throw std::runtime_error("failed to open file!");

    size_t            fileSize = file.tellg();
    std::vector<char> buffer(fileSize);//���ݴ�С���û���

    file.seekg(0);                     //seek back to the beginning of the file
    file.read(buffer.data(), fileSize);//����buffer��

    file.close();
    return buffer;
}

//����shader module��helper function
VkShaderModule Renderer::createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());//Ҫת����uint32_t��ָ��

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("failed to create shader module!");

    return shaderModule;
}

void Renderer::drawFrame() {
    //�ȴ��ϴθø�framebuffer��Ӧ��gpu�����������Ӷ�ʹ����֡Ҫʹ�õ�������Դ����(���framebufferһ��ִ��)
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);//true����������fences���ԣ����ĸ�������time out

    // per-frame time logic
    currentTime = static_cast<float>(glfwGetTime());
    deltaTime   = currentTime - lastTime;
    lastTime    = currentTime;

    //��ȡswap chain�е���һ������ʹ�õ�image��imageIndex�������õ�image�󼤻�semaphore��������ڸı䣬�ؽ�������
    uint32_t imageIndex;
    VkResult result = swapchain.acquireNextImage(imageAvailableSemaphores[currentFrame], &imageIndex);

    //�����ȡimageʱswap chain�Ѿ�out of date����ô������Present�ɹ�����������ִ�У�ֱ���ؽ�������
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {//swap chain out of date
        recreateSwapChain();
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // (д��������Ϊ�˷�ֹ����out_of_date���Ĵ��ں�ֱ�ӷ��أ�Ȼ���´�ִ��ʱ���µ�fence��������)
    vkResetFences(device, 1, &inFlightFences[currentFrame]);//fence��Ҫ�ֶ�reset

    camera.Tick(deltaTime);
    updateUniformBuffer(currentFrame);
    updateImGUI(deltaTime);// ������һ��NewFrame

    //recording the command buffer per frame
    vkResetCommandBuffer(drawCmdBuffers[currentFrame], 0);//0��ʾ������������
    recordCommandBuffer(drawCmdBuffers[currentFrame], imageIndex);

    //submit command buffer
    VkSubmitInfo         submitInfo       = LR::initializers::submitInfo();
    VkSemaphore          waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount         = 1;
    submitInfo.pWaitSemaphores            = waitSemaphores;//ָ����Ⱦ��ʼǰ�ȴ���Щsemaphore
    submitInfo.pWaitDstStageMask          = waitStages;    //�ȴ�дcolor attchment�Ľ׶�

    // submitInfo.pCommandBuffers    = &drawCmdBuffers[currentFrame];
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &drawCmdBuffers[currentFrame];

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinishedSemaphores[currentFrame];//ָ����Ⱦ����������Щsemaphore

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)//���������ֱ�ӷ��أ������Ҫ�����ź�����ʹcpu�ȴ�gpu��ɹ���
        throw std::runtime_error("failed to submit draw command buffer!");

    //Presentation
    result = swapchain.queuePresent(presentQueue, imageIndex, renderFinishedSemaphores[currentFrame]);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    //�л�����һ֡(���Ĳ���)��ֻ��ִ�е�����Ż��ƽ�����һ��Frame����Ȼ��һֱdraw���ǵ�ǰ��frame
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

//�ֶ������Ķ���Ҫdestroy�������˳���д����
void Renderer::cleanup() {
    delete imGui;
    delete glTFScene;

    // clean up swapchain & images(automatically) & views & surface
    cleanupSwapChain();

    // destroy texture content
    vkDestroySampler(device, textureSampler, nullptr);
    vkDestroyImageView(device, textureImageView, nullptr);
    vkDestroyImage(device, textureImage, nullptr);
    vkFreeMemory(device, textureImageMemory, nullptr);

    // destroy descriptor contents
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    // vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.textures, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.matrices, nullptr);

    // destroy uniform buffers, vertex buffer, index buffer
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        uniformBuffers[i].destroy();
    vertexBuffer.destroy();
    indexBuffer.destroy();

    // destroy pipeline
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    // destroy renderpass
    vkDestroyRenderPass(device, renderPass, nullptr);

    // destroy sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    destoryCommandBuffers();
    vkDestroyCommandPool(device, commandPool, nullptr);

    delete vulkanDevice;

    if (enableValidationLayers)
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    // vkDestroySurfaceKHR(instance, surface, nullptr);//���ʵ��ǰ���surface����
    vkDestroyInstance(instance, nullptr);//���ʵ��

    glfwDestroyWindow(window);
    glfwTerminate();
}

void Renderer::cleanupSwapChain() {
    // clean up swapchain & images(automatically) & views & surface
    swapchain.cleanup();

    // destroy swapchain depth contents(created by ourselves)
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (size_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
    }
}

void Renderer::recreateSwapChain() {
    //��������С��(��ʱframebuffer�Ĵ�СΪ0)
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();//һֱ�ȴ�����
    }

    // Ensure all operations on the device have been finished before destroying resources
    vkDeviceWaitIdle(device);

    SCR_WIDTH  = static_cast<uint32_t>(width);
    SCR_HEIGHT = static_cast<uint32_t>(height);
    setupSwapChain();// create new swapchain, delete views and old swapchain

    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);
    createDepthResources();//���Ĵ��ڴ�СҲ��Ҫ����depth buffer��resolution��Ҳ��image��

    // Recreate the frameBuffers
    for (uint32_t i = 0; i < swapChainFramebuffers.size(); i++) {
        vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
    }
    createFramebuffers();

    // Command buffers need to be recreated as they may store references to the recreated frame buffer
    // destoryCommandBuffers();
    // createCommandBuffers();
    // recordCommandBuffers();

    // // SRS - Recreate fences in case number of swapchain images has changed on resize (?)
    // for(auto& fence : inFlightFences)
    //     vkDestroyFence(device, fence, nullptr);
    // createSyncObjects();

    vkDeviceWaitIdle(device);

    // �����µ�camera��perspective����
    camera.updatePerspectiveMatrix(camera.fov_y, static_cast<float>(width) / height, camera.zNear, camera.zFar);

    // ����ui��С
    ImGuiIO& io    = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)(width), (float)(height));
}

// VkSampleCountFlagBits Renderer::getMaxUsableSampleCount() {
//     VkPhysicalDeviceProperties physicalDeviceProperties;
//     vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

//     VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
//     if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
//     if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
//     if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
//     if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
//     if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
//     if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

//     return VK_SAMPLE_COUNT_1_BIT;
// }

//�������ڴ��multisample��color image
// void Renderer::createColorResources() {
//     VkFormat colorFormat = swapChainImageFormat;
//     // createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory);
//     createImage(swapChainExtent.width, swapChainExtent.height, 1, VK_SAMPLE_COUNT_1_BIT, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory);
//     colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
// }

// �������Ըĳ�keyinput�Ǹ�������ÿ��ֻ����Ӧһ��
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        camera.enterCam = false;
    }

    // �����������
    glfwGetCursorPos(window, &camera.mouse.x, &camera.mouse.y);

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
        camera.mouse.left = true;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
        camera.mouse.right = true;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS)
        camera.mouse.middle = true;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE)
        camera.mouse.left = false;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE)
        camera.mouse.right = false;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_RELEASE)
        camera.mouse.middle = false;

    //��f������Ļ
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        camera.enterCam   = true;
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