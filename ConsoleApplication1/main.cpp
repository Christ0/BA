// ConsoleApplication1.cpp : Diese Datei enthält die Funktion "main". Hier beginnt und endet die Ausführung des Programms.
//

#include "pch.h"
#include "VulkanUtils.h"
#include "EasyImage.h"
#include "DepthImage.h"
#include "Vertex.h"
#include "Mesh.h"
#include "Pipeline.h"
#include <iostream>
#include <vector>
#include <array>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Bei OpenGL ist Z-Buffer zwischen -1 und 1, in Vulkan zwischen 0 und 1. Makro um GLMs Projectionmatrix auf Vulkan zu stellen
#include <glm.hpp>
#include <../glm/gtc/matrix_transform.hpp>
#include <../glm/gtc/random.hpp>
#include <chrono>

const int MAX_POINT_LIGHT_COUNT = 1024 * 8;
const uint32_t MAX_POINT_LIGHT_PER_TILE = 1023;
const int TILE_SIZE = 16;
int tileCountPerRow = 0;
int tileCountPerCol = 0;
int graphicsQueueIndex = 0;
int computeQueueIndex = 0;
short debugViewForFragment = 0; //0 = normales Rendering, 1 2 3 4 =  debugview

bool wDown = false;
bool aDown = false;
bool sDown = false;
bool dDown = false;

VkInstance instance;
std::vector<VkPhysicalDevice> allPhysicalDevices;
VkPhysicalDevice physicalDevice = nullptr;
VkSurfaceKHR surface;
VkDevice device;
VkSwapchainKHR swapchain = VK_NULL_HANDLE;
VkImageView *imageViews;

VkFramebuffer *framebuffers;
VkFramebuffer depthFramebuffer;

VkShaderModule shaderModuleVert;
VkShaderModule shaderModuleFrag;
VkShaderModule shaderModuleDepth;
VkShaderModule shaderModuleComp;

VkRenderPass depthPrePass;
VkRenderPass renderPass;

Pipeline pipeline;

VkCommandPool commandPool;
VkCommandPool commandPoolCompute;

VkCommandBuffer * commandBuffers;
VkCommandBuffer depthPrepassCommandBuffer;
VkCommandBuffer lightCullingCommandBuffer;

VkSemaphore semaphoreImageAvailable;
VkSemaphore semaphoreRenderingDone;
VkSemaphore semaphoreLightcullingDone;
VkSemaphore semaphoreDepthPrepassDone;

VkQueue queue;
VkQueue computeQueue;

VkBuffer uniformBuffer;
VkBuffer stagingBuffer;
VkBuffer cameraStagingBuffer;
VkBuffer cameraUniformBuffer;
VkBuffer lightVisibilityBuffer;
VkBuffer pointlightBuffer;
VkBuffer lightStagingBuffer;

VkDeviceMemory uniformBufferMemory;
VkDeviceMemory stagingBufferMemory;
VkDeviceMemory cameraStagingBufferMemory;
VkDeviceMemory cameraUniformBufferMemory;
VkDeviceMemory lightVisibilityBufferMemory;
VkDeviceMemory pointlightBufferMemory;
VkDeviceMemory lightStagingBufferMemory;

VkDeviceSize lightVisibilityBufferSize = 0;
VkDeviceSize pointlightBufferSize = 0;

std::vector<char> shaderCodeVert;
std::vector<char> shaderCodeFrag;
std::vector<char> shaderCodeDepth;


uint32_t amountOfImagesInSwapchain = 0;
GLFWwindow* window;

// This storage buffer stores visible lights for each tile
	// which is output from the light culling compute shader
	// max MAX_POINT_LIGHT_PER_TILE point lights per tile




struct PointLight
{
public:
	glm::vec3 pos;
	float radius = { 5.0f };
	glm::vec3 intensity = { 1.0f, 1.0f, 1.0f };
	float padding;

	PointLight() {}
	PointLight(glm::vec3 pos, float radius, glm::vec3 intensity)
		: pos(pos), radius(radius), intensity(intensity)
	{};
};

std::vector<PointLight> pointLights;

uint32_t windowWidth = 1280;
uint32_t windowHeight = 720;
const VkFormat imageFormat = VK_FORMAT_B8G8R8A8_UNORM; //TODO CIV

struct UniformBufferObject {
	glm::mat4 model;
	glm::vec3 lightPosition;
};

struct CameraUbo {
	glm::mat4 view;
	glm::mat4 proj;
	glm::mat4 projview;
	glm::vec3 cameraPos;
};

struct SceneConfiguration {
	float scale;
	glm::vec3 minLightPos;
	glm::vec3 maxLightPos;
	float lightRadius;
	int lightNum;
	glm::vec3 cameraPosition;
	glm::quat cameraRotation;
};

UniformBufferObject ubo;
CameraUbo cameraUbo;
glm::mat4 modelMatrix;
glm::mat4 viewMatrix;
glm::mat4 projMatrix;
glm::mat4 projViewMatrix;

VkDescriptorSetLayout descriptorSetLayout; 
VkDescriptorSetLayout descriptorSetLayoutLightCulling;
VkDescriptorSetLayout descriptorSetLayoutCamera;
VkDescriptorSetLayout descriptorSetLayoutDepth;
std::vector<VkDescriptorSetLayout> allSetLayouts;
std::vector<VkDescriptorSetLayout> allSetLayoutsForCompute;

VkDescriptorSet lightCullingDescriptorSet;
VkDescriptorSet descriptorSet;
VkDescriptorSet cameraDescriptorSet;
VkDescriptorSet depthDescriptorSet;
VkDescriptorPool descriptorPool;


EasyImage ezImage;
DepthImage depthImage;
SceneConfiguration sceneConfiguration;

Mesh mesh;

void printStats(VkPhysicalDevice &device) {
	//Gibt Eigenschaften der GPU aus
	VkPhysicalDeviceProperties properties = { };
	vkGetPhysicalDeviceProperties(device, &properties);
	std::cout << std::endl;
	std::cout << "Name:                    " << properties.deviceName << std::endl;
	uint32_t apiVer = properties.apiVersion;
	std::cout << "API Version:             " << VK_VERSION_MAJOR(apiVer) <<"."<<VK_VERSION_MINOR(apiVer)<<"."<<VK_VERSION_PATCH(apiVer) << std::endl;
	std::cout << "Driver Version:          " << properties.driverVersion << std::endl;
	std::cout << "Vendor ID:               " << properties.vendorID << std::endl;
	std::cout << "Device ID:               " << properties.deviceID << std::endl;
	std::cout << "Device Type:             " << properties.deviceType << std::endl; 
	std::cout << "DiscreteQueuePriorities: " << properties.limits.discreteQueuePriorities << std::endl;
	std::cout << "Max Clip Distances: "		 << properties.limits.maxClipDistances << std::endl;
	//Was kann die GPU alles
	VkPhysicalDeviceFeatures features = { };
	vkGetPhysicalDeviceFeatures(device, &features);
	std::cout << "GeoShader:               " << features.geometryShader << std::endl;//...

	//Was kann der Speicher der GPU
	VkPhysicalDeviceMemoryProperties memProp = { };
	vkGetPhysicalDeviceMemoryProperties(device, &memProp);

	uint32_t amountOfQueueFamilies = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &amountOfQueueFamilies, nullptr);
	VkQueueFamilyProperties * familyProperties = new VkQueueFamilyProperties[amountOfQueueFamilies];
	vkGetPhysicalDeviceQueueFamilyProperties(device, &amountOfQueueFamilies, familyProperties);

	std::cout << "Amount of Queue Families: " << amountOfQueueFamilies << std::endl;

	for (uint32_t i = 0; i < amountOfQueueFamilies; i++) {
		std::cout << std::endl;
		std::cout << "Queue Family #" << i << std::endl;
		std::cout << "VK_QUEUE_GRAPHICS_BIT       " << ((familyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) << std::endl;
		std::cout << "VK_QUEUE_COMPUTE_BIT        " << ((familyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) << std::endl;
		std::cout << "VK_QUEUE_TRANSFER_BIT       " << ((familyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0) << std::endl;
		std::cout << "VK_QUEUE_SPARSE_BINDING_BIT " << ((familyProperties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0) << std::endl;
		std::cout << "Queue Count: " << familyProperties[i].queueCount << std::endl;
		std::cout << "Timestamp Valid Bits: " << familyProperties[i].timestampValidBits << std::endl;
		uint32_t width = familyProperties[i].minImageTransferGranularity.width;
		uint32_t depth = familyProperties[i].minImageTransferGranularity.depth;
		uint32_t height = familyProperties[i].minImageTransferGranularity.height;
		
		std::cout << "Min Image Timestamp Granularity: " << width << ", " << height << ", " << depth << std::endl << std::endl;
	}
	std::cout << std::endl << std::endl;

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &surfaceCapabilities);
	std::cout << "Surface Capabilities: " << std::endl;
	std::cout << "\tminImageCount:           " << surfaceCapabilities.minImageCount << std::endl;
	std::cout << "\tmaxImageCount:           " << surfaceCapabilities.maxImageCount << std::endl;
	std::cout << "\tcurrentExtent:           " << surfaceCapabilities.currentExtent.width << " / " << surfaceCapabilities.currentExtent.height << std::endl;
	std::cout << "\tminImageExtent:          " << surfaceCapabilities.minImageExtent.width << " / " << surfaceCapabilities.minImageExtent.height << std::endl;
	std::cout << "\tmaxImageExtent:          " << surfaceCapabilities.maxImageExtent.width << " / " << surfaceCapabilities.maxImageExtent.height << std::endl;
	std::cout << "\tmaxImageArrayLayers:     " << surfaceCapabilities.maxImageArrayLayers << std::endl;
	std::cout << "\tsupportedTransforms:     " << surfaceCapabilities.supportedTransforms << std::endl;
	std::cout << "\tcurrentTransform:        " << surfaceCapabilities.currentTransform << std::endl;
	std::cout << "\tsupportedCompositeAlpha: " << surfaceCapabilities.supportedCompositeAlpha << std::endl;
	std::cout << "\tsupportedUsageFlags:     " << surfaceCapabilities.supportedUsageFlags << std::endl;

	uint32_t amountOfFormats = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &amountOfFormats, nullptr);
	VkSurfaceFormatKHR* surfaceFormats = new VkSurfaceFormatKHR[amountOfFormats];
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &amountOfFormats, surfaceFormats);
	std::cout << std::endl;
	std::cout << "Amount of Formats: " << amountOfFormats << std::endl;
	for (uint32_t i = 0; i < amountOfFormats; i++) {
		std::cout << "Format: " << surfaceFormats[i].format << std::endl;
	}

	uint32_t amountOfPresentationModes = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &amountOfPresentationModes, nullptr);
	VkPresentModeKHR* presentModes = new VkPresentModeKHR[amountOfPresentationModes];
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &amountOfPresentationModes, presentModes);
	std::cout << std::endl;
	std::cout << "Amount of Presentation Modes: " << amountOfPresentationModes << std::endl;
	for (uint32_t i = 0; i < amountOfPresentationModes; i++) {
		std::cout << "Supported Presentation Mode: " << presentModes[i] << std::endl;
	}


	std::cout << std::endl;
	delete[] familyProperties;
	delete[] surfaceFormats;
	delete[] presentModes;
}

void recreateSwapchain();

void onWindowResized(GLFWwindow *window, int width, int height) {
	if (width == 0) width == 1;
	if (height == 0) height == 1;

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
	windowWidth = width;
	windowHeight = height;
	recreateSwapchain();
}

void startGLFW() {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
	//glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

	window = glfwCreateWindow(windowWidth, windowHeight, "VulkanRenderer", 0, nullptr);
	glfwSetWindowSizeCallback(window, onWindowResized);
}

void setSceneConfiguration() {
	sceneConfiguration = {
		//0.08f,								//scale
		0.5f,									//scale
		glm::vec3{-25, -25, -25}, 				//minLightPos
		glm::vec3{25, 25, 25},					//maxLightPos
		3.0f,									//radius
		2000,									//lightNum
		glm::vec3{0.7f, 7.0f, 5.0f},			//camera Position
		glm::quat{0.8f, -2.9f, 0.34f, 0.11f}	//camera Rotation
	};
}



void createInstance() {
	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "VulkanRenderer";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.pEngineName = "Vulkanalles";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

#ifdef _DEBUG
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_LUNARG_standard_validation"
	};
#else
	const std::vector<const char*> validationLayers = {};
#endif

	uint32_t amountOfGlfwExtensions = 0;
	auto glfwExtensions = glfwGetRequiredInstanceExtensions(&amountOfGlfwExtensions);

	VkInstanceCreateInfo instanceInfo = { };
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pNext = nullptr;
	instanceInfo.flags = 0;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledLayerCount = validationLayers.size();
	instanceInfo.ppEnabledLayerNames = validationLayers.data();
	instanceInfo.enabledExtensionCount = amountOfGlfwExtensions;
	instanceInfo.ppEnabledExtensionNames = glfwExtensions;

	VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
	CHECK_FOR_CRASH(result);
}

void printInstanceLayers() {
	uint32_t amountOfLayers = 0;
	vkEnumerateInstanceLayerProperties(&amountOfLayers, nullptr);
	VkLayerProperties* layers = new VkLayerProperties[amountOfLayers];
	vkEnumerateInstanceLayerProperties(&amountOfLayers, layers);

	std::cout << "Amount of Instance Layers: " << amountOfLayers << std::endl;
for (uint32_t i = 0; i < amountOfLayers; i++) {
	std::cout << std::endl;
	std::cout << "Name:         " << layers[i].layerName << std::endl;
	std::cout << "Spec Version: " << layers[i].specVersion << std::endl;
	std::cout << "Impl Version: " << layers[i].implementationVersion << std::endl;
	std::cout << "Description:  " << layers[i].description << std::endl;
}
std::cout << std::endl;
delete[] layers;
}

void printInstanceExtensions() {
	uint32_t amountOfExtensions = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &amountOfExtensions, nullptr);
	VkExtensionProperties* extensions = new VkExtensionProperties[amountOfExtensions];
	vkEnumerateInstanceExtensionProperties(nullptr, &amountOfExtensions, extensions);

	std::cout << std::endl;
	std::cout << "Amount of Extensions: " << amountOfExtensions << std::endl;
	for (uint32_t i = 0; i < amountOfExtensions; i++) {
		std::cout << std::endl;
		std::cout << "Name:         " << extensions[i].extensionName << std::endl;
		std::cout << "Spec Version: " << extensions[i].specVersion << std::endl;
	}
	std::cout << std::endl;
	std::cout << std::endl;
	delete[] extensions;
}

void createGlfwWindowSurface() {
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	CHECK_FOR_CRASH(result);
}

void pickPhysicalDevice() {
	uint32_t amountOfPhysicalDevices = 0;
	VkResult result = vkEnumeratePhysicalDevices(instance, &amountOfPhysicalDevices, nullptr); //Gibt in amountOfPhysicalDevices die Anzahl an verfügbaren Grafikkarten ein
	CHECK_FOR_CRASH(result);

	allPhysicalDevices.resize(amountOfPhysicalDevices);
	if (allPhysicalDevices.size() == 0) {
		throw std::runtime_error("Could not find a device with vulkan support!");
	}

	result = vkEnumeratePhysicalDevices(instance, &amountOfPhysicalDevices, allPhysicalDevices.data()); 
	//^ Selbe Funktion wie oben, hat aber unterschiedliche Funktion. Diesmal statt nullptr mit dem Inhalt von allPhysicalDevices
	// Gibt Infos zu allen Grafikkarten in allPhysicalDevices ein																									
	CHECK_FOR_CRASH(result);

	for (int i = 0; i < amountOfPhysicalDevices; i++) {
		uint32_t amountOfQueueFamilies = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(allPhysicalDevices[i], &amountOfQueueFamilies, nullptr);
		VkQueueFamilyProperties * familyProperties = new VkQueueFamilyProperties[amountOfQueueFamilies];
		vkGetPhysicalDeviceQueueFamilyProperties(allPhysicalDevices[i], &amountOfQueueFamilies, familyProperties);

		if (((familyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0) &&
			((familyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0) &&
			((familyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0)) {
			physicalDevice = allPhysicalDevices[i];
			break;
		}
	}
	if (physicalDevice == nullptr) {
		throw std::runtime_error("Could not find a device with suiting Queue Support!");
	}
}

void printallPhysicalDevicesStats() {
	for (uint32_t i = 0; i < allPhysicalDevices.size(); i++) {
		printStats(allPhysicalDevices[i]);
	}
#ifndef _DEBUG
	system("cls");
#endif
}

void createLogicalDevice() {	
	float queuePrios[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	VkDeviceQueueCreateInfo deviceQueueCreateInfo;
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = 0; //TODO bester family index?
	deviceQueueCreateInfo.queueCount = 1;		//TODO zulässig?
	deviceQueueCreateInfo.pQueuePriorities = queuePrios;

	VkDeviceQueueCreateInfo deviceQueueCreateInfoCompute;
	deviceQueueCreateInfoCompute.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfoCompute.pNext = nullptr;
	deviceQueueCreateInfoCompute.flags = 0;
	deviceQueueCreateInfoCompute.queueFamilyIndex = 2; //TODO bester family index?
	deviceQueueCreateInfoCompute.queueCount = 1;		//TODO zulässig?
	deviceQueueCreateInfoCompute.pQueuePriorities = queuePrios;

	queueCreateInfos.push_back(deviceQueueCreateInfo);
	queueCreateInfos.push_back(deviceQueueCreateInfoCompute);

	VkPhysicalDeviceFeatures usedFeatures = {};
	usedFeatures.samplerAnisotropy = VK_TRUE;
	usedFeatures.vertexPipelineStoresAndAtomics = VK_TRUE; // Wichtig für Shader
	usedFeatures.fragmentStoresAndAtomics = VK_TRUE;	   // Wichtig für Shader

	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME //= "VK_KHR_swapchain"
	};

	VkDeviceCreateInfo deviceCreateInfo = { };
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
	deviceCreateInfo.flags = 0;
	deviceCreateInfo.queueCreateInfoCount = queueCreateInfos.size();
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	deviceCreateInfo.enabledExtensionCount = deviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &usedFeatures;


	VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device); 
	CHECK_FOR_CRASH(result);
}

void createQueue() {
	vkGetDeviceQueue(device, graphicsQueueIndex, 0, &queue); 
	vkGetDeviceQueue(device, computeQueueIndex, 0, &computeQueue); 
}

void checkSurfaceSupport() {
	VkBool32 surfaceSupport = false;
	VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, 0, surface, &surfaceSupport); 
	CHECK_FOR_CRASH(result);

	if (!surfaceSupport) {
		std::cerr << "Surface not supported!" << std::endl;
		__debugbreak();
	}
}

void createSwapchain() {
	VkSwapchainCreateInfoKHR swapChainCreateInfo;
	swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainCreateInfo.pNext = nullptr;
	swapChainCreateInfo.flags = 0;
	swapChainCreateInfo.surface = surface;
	swapChainCreateInfo.minImageCount = 2; //TODO Check if valid
	swapChainCreateInfo.imageFormat = imageFormat;
	swapChainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; //TODO CIV
	swapChainCreateInfo.imageExtent = VkExtent2D{ windowWidth, windowHeight };
	swapChainCreateInfo.imageArrayLayers = 1;
	swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainCreateInfo.queueFamilyIndexCount = 0;
	swapChainCreateInfo.pQueueFamilyIndices = nullptr;
	swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; 
	swapChainCreateInfo.clipped = VK_TRUE;
	swapChainCreateInfo.oldSwapchain = swapchain;

	VkResult result = vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapchain);
	CHECK_FOR_CRASH(result);
}

void createImageViews() {
	vkGetSwapchainImagesKHR(device, swapchain, &amountOfImagesInSwapchain, nullptr);
	VkImage* swapchainImages = new VkImage[amountOfImagesInSwapchain];
	VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &amountOfImagesInSwapchain, swapchainImages);
	CHECK_FOR_CRASH(result);

	imageViews = new VkImageView[amountOfImagesInSwapchain];
	for (uint32_t i = 0; i < amountOfImagesInSwapchain; i++) {
		createImageView(device, swapchainImages[i], imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, imageViews[i]);
	}
	delete[] swapchainImages;
}

void createRenderPasses() {
	//Depth PrePass
	{
		VkAttachmentDescription depthAttachment = DepthImage::getDepthAttachment(physicalDevice); 

		VkAttachmentReference depthAttachmentReference;
		depthAttachmentReference.attachment = 0;
		depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDescription;
		subpassDescription.flags = 0;
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = nullptr;
		subpassDescription.colorAttachmentCount = 0;
		subpassDescription.pColorAttachments = nullptr;
		subpassDescription.pResolveAttachments = nullptr;
		subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = nullptr;

		VkSubpassDependency subpassDependency;
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependency.dstSubpass = 0;
		/*subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = 0;*/
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		subpassDependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpassDependency.dependencyFlags = 0;

		VkRenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.pNext = nullptr;
		renderPassCreateInfo.flags = 0;
		renderPassCreateInfo.attachmentCount = 1;
		renderPassCreateInfo.pAttachments = &depthAttachment;
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDescription;
		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies = &subpassDependency;

		VkResult result = vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &depthPrePass);
		CHECK_FOR_CRASH(result);
	}

	//Render Pass
	{
		VkAttachmentDescription colorAttachmentDescription;
		colorAttachmentDescription.flags = 0;
		colorAttachmentDescription.format = imageFormat;
		colorAttachmentDescription.format = imageFormat;
		colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachmentDescription;
		depthAttachmentDescription.flags = 0;
		depthAttachmentDescription.format = DepthImage::findDepthFormat(physicalDevice); 
		depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; //lade depth prepass info
		depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorAttachmentReference;
		colorAttachmentReference.attachment = 0;
		colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentReference;
		depthAttachmentReference.attachment = 1;
		depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		VkSubpassDescription subpassDescription;
		subpassDescription.flags = 0;
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.inputAttachmentCount = 0;
		subpassDescription.pInputAttachments = nullptr;
		subpassDescription.colorAttachmentCount = 1;
		subpassDescription.pColorAttachments = &colorAttachmentReference;
		subpassDescription.pResolveAttachments = nullptr;
		subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
		subpassDescription.preserveAttachmentCount = 0;
		subpassDescription.pPreserveAttachments = nullptr;

		VkSubpassDependency subpassDependency;
		subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpassDependency.dstSubpass = 0;
		subpassDependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpassDependency.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		subpassDependency.dependencyFlags = 0;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachmentDescription, depthAttachmentDescription };

		VkRenderPassCreateInfo renderPassCreateInfo;
		renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassCreateInfo.pNext = nullptr;
		renderPassCreateInfo.flags = 0;
		renderPassCreateInfo.attachmentCount = attachments.size();
		renderPassCreateInfo.pAttachments = attachments.data();
		renderPassCreateInfo.subpassCount = 1;
		renderPassCreateInfo.pSubpasses = &subpassDescription;
		renderPassCreateInfo.dependencyCount = 1;
		renderPassCreateInfo.pDependencies = &subpassDependency;

		VkResult result = vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass);
		CHECK_FOR_CRASH(result);
	}
}

void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule) {
	VkShaderModuleCreateInfo shaderCreateInfo;
	shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderCreateInfo.pNext = nullptr;
	shaderCreateInfo.flags = 0;
	shaderCreateInfo.codeSize = code.size();
	shaderCreateInfo.pCode = (uint32_t*)code.data(); //Casten geht hier NUR, weil SPIR V garantiert, dass Files immer ein Vielfaches von 4 sind, ansonsten unsicheres Verhalten

	VkResult result = vkCreateShaderModule(device, &shaderCreateInfo, nullptr, shaderModule);
	CHECK_FOR_CRASH(result);
}

void createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
	descriptorSetLayoutBinding.binding = 0;
	descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding.descriptorCount = 1;
	descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding;
	samplerDescriptorSetLayoutBinding.binding = 1;
	samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerDescriptorSetLayoutBinding.descriptorCount = 1;
	samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> descriptorSets;
	descriptorSets.push_back(descriptorSetLayoutBinding);
	descriptorSets.push_back(samplerDescriptorSetLayoutBinding);

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = descriptorSets.size();
	descriptorSetLayoutCreateInfo.pBindings = descriptorSets.data();

	VkResult result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);
	CHECK_FOR_CRASH(result);


	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindingsLight = {};

	//descriptor for storage buffer to read light culling results
	VkDescriptorSetLayoutBinding storageBufferLightResults;
	storageBufferLightResults.binding = 0;
	storageBufferLightResults.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	storageBufferLightResults.descriptorCount = 1;
	storageBufferLightResults.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	storageBufferLightResults.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding uniformBufferForPointLights;
	uniformBufferForPointLights.binding = 1;
	uniformBufferForPointLights.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	uniformBufferForPointLights.descriptorCount = 1;
	uniformBufferForPointLights.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	uniformBufferForPointLights.pImmutableSamplers;

	setLayoutBindingsLight.push_back(storageBufferLightResults);
	setLayoutBindingsLight.push_back(uniformBufferForPointLights);

	VkDescriptorSetLayoutCreateInfo LightSetLayoutInfo;
	LightSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	LightSetLayoutInfo.pNext = nullptr;
	LightSetLayoutInfo.flags = 0;
	LightSetLayoutInfo.bindingCount = setLayoutBindingsLight.size();
	LightSetLayoutInfo.pBindings = setLayoutBindingsLight.data();

	result = vkCreateDescriptorSetLayout(device, &LightSetLayoutInfo, nullptr, &descriptorSetLayoutLightCulling);
	CHECK_FOR_CRASH(result);


	VkDescriptorSetLayoutBinding cameraLayoutBinding;
	cameraLayoutBinding.binding = 0;
	cameraLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cameraLayoutBinding.descriptorCount = 1;
	cameraLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
	cameraLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCameraCreateInfo;
	descriptorSetLayoutCameraCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCameraCreateInfo.pNext = nullptr;
	descriptorSetLayoutCameraCreateInfo.flags = 0; 
	descriptorSetLayoutCameraCreateInfo.bindingCount = 1;
	descriptorSetLayoutCameraCreateInfo.pBindings = &cameraLayoutBinding;

	result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCameraCreateInfo, nullptr, &descriptorSetLayoutCamera);
	CHECK_FOR_CRASH(result);

	VkDescriptorSetLayoutBinding samplerLayoutBinding;
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutDepthCreateInfo;
	descriptorSetLayoutDepthCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutDepthCreateInfo.pNext = nullptr;
	descriptorSetLayoutDepthCreateInfo.flags = 0; 
	descriptorSetLayoutDepthCreateInfo.bindingCount = 1;
	descriptorSetLayoutDepthCreateInfo.pBindings = &samplerLayoutBinding;

	result = vkCreateDescriptorSetLayout(device, &descriptorSetLayoutDepthCreateInfo, nullptr, &descriptorSetLayoutDepth);
	CHECK_FOR_CRASH(result);

	
	allSetLayouts.push_back(descriptorSetLayout);
	allSetLayouts.push_back(descriptorSetLayoutLightCulling);
	allSetLayouts.push_back(descriptorSetLayoutCamera);
	allSetLayouts.push_back(descriptorSetLayoutDepth);

	allSetLayoutsForCompute.push_back(descriptorSetLayoutLightCulling);
	allSetLayoutsForCompute.push_back(descriptorSetLayoutCamera);
	allSetLayoutsForCompute.push_back(descriptorSetLayoutDepth);
}

void createPipelineGraphics() {
	shaderCodeVert = readFile("vert.spv");
	shaderCodeFrag = readFile("frag.spv");
	shaderCodeDepth = readFile("depth_vert.spv");
	
	createShaderModule(shaderCodeVert, &shaderModuleVert);
	createShaderModule(shaderCodeFrag, &shaderModuleFrag);
	createShaderModule(shaderCodeDepth, &shaderModuleDepth);

	pipeline.iniGraphics(shaderModuleVert, shaderModuleFrag, shaderModuleDepth, windowWidth, windowHeight);
	pipeline.createGraphics(device, renderPass, depthPrePass, allSetLayouts);
}

void createPipelineCompute() {
	auto shaderCodeCompute = readFile("comp.spv");
	createShaderModule(shaderCodeCompute, &shaderModuleComp);

	pipeline.iniCompute(allSetLayoutsForCompute, shaderModuleComp);
	pipeline.createCompute();
}

void createCommandPool() {
	VkCommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;
	commandPoolCreateInfo.queueFamilyIndex = graphicsQueueIndex; //nur Queue mit VK_QUEUE_GRAPHICS_BIT

	VkResult result = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
	CHECK_FOR_CRASH(result);

	VkCommandPoolCreateInfo commandPoolCreateInfoCompute;
	commandPoolCreateInfoCompute.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfoCompute.pNext = nullptr;
	commandPoolCreateInfoCompute.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfoCompute.queueFamilyIndex = computeQueueIndex; //nur Queue mit VK_QUEUE_COMPUTE_BIT

	result = vkCreateCommandPool(device, &commandPoolCreateInfoCompute, nullptr, &commandPoolCompute);
	CHECK_FOR_CRASH(result);
}

void createCommandBuffers() {
	//CommandBuffer für Rendering
	VkCommandBufferAllocateInfo commandBufferRenderAllocateInfo;
	commandBufferRenderAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferRenderAllocateInfo.pNext = nullptr;
	commandBufferRenderAllocateInfo.commandPool = commandPool;
	commandBufferRenderAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferRenderAllocateInfo.commandBufferCount = amountOfImagesInSwapchain;

	commandBuffers = new VkCommandBuffer[amountOfImagesInSwapchain];
	VkResult result = vkAllocateCommandBuffers(device, &commandBufferRenderAllocateInfo, commandBuffers);
	CHECK_FOR_CRASH(result);

	//CommandBuffer für Light Culling
	VkCommandBufferAllocateInfo commandBufferLightCullingAllocateInfo;
	commandBufferLightCullingAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferLightCullingAllocateInfo.pNext = nullptr;
	commandBufferLightCullingAllocateInfo.commandPool = commandPoolCompute;
	commandBufferLightCullingAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferLightCullingAllocateInfo.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(device, &commandBufferLightCullingAllocateInfo, &lightCullingCommandBuffer);
	CHECK_FOR_CRASH(result);

	//CommandBuffer für Depth Prepass
	VkCommandBufferAllocateInfo commandBufferDepthAllocateInfo;
	commandBufferDepthAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferDepthAllocateInfo.pNext = nullptr;
	commandBufferDepthAllocateInfo.commandPool = commandPool;
	commandBufferDepthAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferDepthAllocateInfo.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(device, &commandBufferDepthAllocateInfo, &depthPrepassCommandBuffer);
	CHECK_FOR_CRASH(result);
}

void createDepthImage() { //aka Z-Buffer
	depthImage.create(device, physicalDevice, commandPool, queue, windowWidth, windowHeight);
}

void createFramebuffers() {
	framebuffers = new VkFramebuffer[amountOfImagesInSwapchain];
	for (size_t i = 0; i < amountOfImagesInSwapchain; i++) {
		std::vector<VkImageView> attachmentViews;
		attachmentViews.push_back(imageViews[i]);
		attachmentViews.push_back(depthImage.getImageView());

		VkFramebufferCreateInfo framebufferCreateInfo;
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.pNext = nullptr;
		framebufferCreateInfo.flags = 0;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = attachmentViews.size();
		framebufferCreateInfo.pAttachments = attachmentViews.data();
		framebufferCreateInfo.width = windowWidth;
		framebufferCreateInfo.height = windowHeight;
		framebufferCreateInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &(framebuffers[i]));
		CHECK_FOR_CRASH(result);
	}

	//depth PrePass frame buffer
	std::array<VkImageView, 1> attachments = { depthImage.getImageView() };
	VkFramebufferCreateInfo depthFramebufferCreateInfo;
	depthFramebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	depthFramebufferCreateInfo.pNext = nullptr;
	depthFramebufferCreateInfo.flags = 0;
	depthFramebufferCreateInfo.renderPass = depthPrePass;
	depthFramebufferCreateInfo.attachmentCount = attachments.size();
	depthFramebufferCreateInfo.pAttachments = attachments.data();
	depthFramebufferCreateInfo.width = windowWidth;
	depthFramebufferCreateInfo.height = windowHeight;
	depthFramebufferCreateInfo.layers = 1;

	VkResult result = vkCreateFramebuffer(device, &depthFramebufferCreateInfo, nullptr, &depthFramebuffer);
	CHECK_FOR_CRASH(result);
}

void createUniformBuffer() {
	VkDeviceSize bufferSize = sizeof(ubo);
	createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferMemory);

	createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, uniformBuffer,
		/*VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT*/ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, uniformBufferMemory);


	ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(sceneConfiguration.scale));
	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(device, stagingBufferMemory);
	copyBuffer(device, queue, commandPool, stagingBuffer, uniformBuffer, sizeof(ubo), 0, 0);

	//Camera buffer
	bufferSize = sizeof(CameraUbo);
	createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, cameraStagingBuffer,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cameraStagingBufferMemory);

	createBuffer(device, physicalDevice, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, cameraUniformBuffer,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, cameraUniformBufferMemory);
}

void createLights() {
	for (int i = 0; i < MAX_POINT_LIGHT_COUNT; i++) {
		glm::vec3 color;
		do { color = { glm::linearRand(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1)) }; } while (color.length() < 0.8f);
		//emplace_back(minLightPos, maxLightPos, lightRadius, color)
		pointLights.emplace_back(glm::linearRand(sceneConfiguration.minLightPos, sceneConfiguration.maxLightPos), sceneConfiguration.lightRadius, color);
	}

	pointlightBufferSize = sizeof(PointLight) * MAX_POINT_LIGHT_COUNT + sizeof(glm::vec4);
	createBuffer(device, physicalDevice, pointlightBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, lightStagingBuffer,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightStagingBufferMemory);

	createBuffer(device, physicalDevice, pointlightBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, pointlightBuffer,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pointlightBufferMemory);
}

void loadTexture() {
	ezImage.load("../blank.jpg");
	ezImage.upload(device, physicalDevice, commandPool, queue);
}

void loadMesh() {
	mesh.loadModelFromFile(device, physicalDevice, queue, commandPool, "meshes/dragon.obj", descriptorPool); //dragon, city, plane, cube
	sceneConfiguration.scale = 0.5f; //andere Modelle, anderer scale. dragon = 0.5, city = 0.08
}

void createDescriptorPool() {
	std::array<VkDescriptorPoolSize, 3> poolSizes = {};

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 100;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[1].descriptorCount = 3;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[2].descriptorCount = 100;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCreateInfo.maxSets = 200;
	descriptorPoolCreateInfo.poolSizeCount = poolSizes.size();
	descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

	VkResult result = vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool);
	CHECK_FOR_CRASH(result);
}

void createDescriptorSet() {
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

	VkResult result = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
	CHECK_FOR_CRASH(result);

	VkDescriptorBufferInfo descriptorBufferInfo;
	descriptorBufferInfo.buffer = uniformBuffer;
	descriptorBufferInfo.offset = 0;
	descriptorBufferInfo.range = sizeof(ubo);

	VkWriteDescriptorSet descriptorWrite;
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.pNext = nullptr;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrite.pImageInfo = nullptr;
	descriptorWrite.pBufferInfo = &descriptorBufferInfo;
	descriptorWrite.pTexelBufferView = nullptr;

	/*VkDescriptorImageInfo descriptorImageInfo;
	descriptorImageInfo.sampler = ezImage.getSampler();
	descriptorImageInfo.imageView = ezImage.getImageView();
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet descriptorSampler;
	descriptorSampler.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorSampler.pNext = nullptr;
	descriptorSampler.dstSet = descriptorSet;
	descriptorSampler.dstBinding = 1;
	descriptorSampler.dstArrayElement = 0;
	descriptorSampler.descriptorCount = 1;
	descriptorSampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorSampler.pImageInfo = &descriptorImageInfo;
	descriptorSampler.pBufferInfo = nullptr;
	descriptorSampler.pTexelBufferView = nullptr;*/

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;
	writeDescriptorSets.push_back(descriptorWrite);
	//writeDescriptorSets.push_back(descriptorSampler);

	vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
}

void createLightCullingDescriptorSet() {
	//light visiblity descriptor sets für beide Passes
	//erstelle set welches sowohl compute und rendering pipeline verwenden
	VkDescriptorSetAllocateInfo allocateInfo;
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = &descriptorSetLayoutLightCulling;

	vkAllocateDescriptorSets(device, &allocateInfo, &lightCullingDescriptorSet);
}

void createCameraDescriptorSet() {
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayoutCamera;

	VkResult result = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &cameraDescriptorSet);
	CHECK_FOR_CRASH(result);

	VkDescriptorBufferInfo descriptorBufferInfo;
	descriptorBufferInfo.buffer = cameraUniformBuffer;
	descriptorBufferInfo.offset = 0;
	descriptorBufferInfo.range = sizeof(CameraUbo);

	VkWriteDescriptorSet descriptorWrite;
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.pNext = nullptr;
	descriptorWrite.dstSet = cameraDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	descriptorWrite.pImageInfo = nullptr;
	descriptorWrite.pBufferInfo = &descriptorBufferInfo;
	descriptorWrite.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

void createDepthDescriptorSet() {
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayoutDepth;

	VkResult result = vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &depthDescriptorSet);
	CHECK_FOR_CRASH(result);
}

//eigene Methode, braucht update wenn Swapchain outdated ist
void updateDepthDescriptorSet() {
	VkDescriptorImageInfo depthImageInfo;
	depthImageInfo.sampler = ezImage.getSampler();
	depthImageInfo.imageView = depthImage.getImageView();
	depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet descriptorWrite;
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.pNext = nullptr;
	descriptorWrite.dstSet = depthDescriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.pImageInfo = &depthImageInfo;
	descriptorWrite.pBufferInfo = nullptr;
	descriptorWrite.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

//für Größeninfos
struct VisibleLightsForTile {
	uint32_t count; //Anzahl aktueller Lichter pro tile
	std::array<uint32_t, MAX_POINT_LIGHT_PER_TILE> lightIndices; //Array der Lichtindices
};

void createLightVisibilityBuffer() {
	assert(sizeof(VisibleLightsForTile) == sizeof(int)*(MAX_POINT_LIGHT_PER_TILE + 1));

	//tileCountPerRow = (windowWidth - 1) / TILE_SIZE + 1;
	//tileCountPerCol = (windowHeight - 1) / TILE_SIZE + 1;
	tileCountPerRow = windowWidth / TILE_SIZE;
	tileCountPerCol = windowHeight / TILE_SIZE;

	lightVisibilityBufferSize = sizeof(VisibleLightsForTile) * tileCountPerRow * tileCountPerCol;

	createBuffer(device, physicalDevice, lightVisibilityBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		lightVisibilityBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, lightVisibilityBufferMemory); 

	//Schreibe Set in Computeshader
	VkDescriptorBufferInfo lightVisibilityBufferInfo;
	lightVisibilityBufferInfo.buffer = lightVisibilityBuffer;
	lightVisibilityBufferInfo.offset = 0;
	lightVisibilityBufferInfo.range = lightVisibilityBufferSize;

	VkDescriptorBufferInfo pointLightBufferInfo;
	pointLightBufferInfo.buffer = pointlightBuffer;
	pointLightBufferInfo.offset = 0;
	pointLightBufferInfo.range = pointlightBufferSize;

	std::vector<VkWriteDescriptorSet> descriptorWrites;
	VkWriteDescriptorSet writeLightVisibilityDescriptorSet;
	writeLightVisibilityDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeLightVisibilityDescriptorSet.pNext = nullptr;
	writeLightVisibilityDescriptorSet.dstSet = lightCullingDescriptorSet;
	writeLightVisibilityDescriptorSet.dstBinding = 0;
	writeLightVisibilityDescriptorSet.dstArrayElement = 0;
	writeLightVisibilityDescriptorSet.descriptorCount = 1;
	writeLightVisibilityDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writeLightVisibilityDescriptorSet.pImageInfo = nullptr;
	writeLightVisibilityDescriptorSet.pBufferInfo = &lightVisibilityBufferInfo;
	writeLightVisibilityDescriptorSet.pTexelBufferView = nullptr;

	VkWriteDescriptorSet writePointlightDescriptorSet;
	writePointlightDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writePointlightDescriptorSet.pNext = nullptr;
	writePointlightDescriptorSet.dstSet = lightCullingDescriptorSet;
	writePointlightDescriptorSet.dstBinding = 1;
	writePointlightDescriptorSet.dstArrayElement = 0;
	writePointlightDescriptorSet.descriptorCount = 1;
	writePointlightDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; 
	writePointlightDescriptorSet.pImageInfo = nullptr;
	writePointlightDescriptorSet.pBufferInfo = &pointLightBufferInfo;
	writePointlightDescriptorSet.pTexelBufferView = nullptr;

	descriptorWrites.emplace_back(writeLightVisibilityDescriptorSet);
	descriptorWrites.emplace_back(writePointlightDescriptorSet);

	vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, nullptr);
}

void recordLightCullingCommandBuffer() {
	VkCommandBufferBeginInfo beginInfo;
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	VkResult result = vkBeginCommandBuffer(lightCullingCommandBuffer, &beginInfo);
	CHECK_FOR_CRASH(result);

	//Barriers, da sharing mode beim Allokieren exklusiv ist
	//Hier nicht nötig, da nur eine Queue verwendet

	/*std::vector<VkBufferMemoryBarrier> barriersBefore;
	VkBufferMemoryBarrier lightVisibilityBarrierBefore;
	lightVisibilityBarrierBefore.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	lightVisibilityBarrierBefore.pNext = nullptr;
	lightVisibilityBarrierBefore.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	lightVisibilityBarrierBefore.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	lightVisibilityBarrierBefore.srcQueueFamilyIndex = graphicsQueueIndex; 
	lightVisibilityBarrierBefore.dstQueueFamilyIndex = computeQueueIndex; 
	lightVisibilityBarrierBefore.buffer = lightVisibilityBuffer;
	lightVisibilityBarrierBefore.offset = 0;
	lightVisibilityBarrierBefore.size = lightVisibilityBufferSize;

	VkBufferMemoryBarrier pointlightBarrierBefore;
	pointlightBarrierBefore.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	pointlightBarrierBefore.pNext = nullptr;
	pointlightBarrierBefore.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	pointlightBarrierBefore.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	pointlightBarrierBefore.srcQueueFamilyIndex = graphicsQueueIndex; //todo
	pointlightBarrierBefore.dstQueueFamilyIndex = computeQueueIndex;  //todo
	pointlightBarrierBefore.buffer = pointlightBuffer;
	pointlightBarrierBefore.offset = 0;
	pointlightBarrierBefore.size = pointlightBufferSize;

	barriersBefore.emplace_back(lightVisibilityBarrierBefore);
	barriersBefore.emplace_back(pointlightBarrierBefore);*/

	//barrier
	/*vkCmdPipelineBarrier(lightCullingCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VkDependencyFlags(), 0,
		nullptr, barriersBefore.size(), barriersBefore.data(), 0, nullptr);*/
	
	std::array<VkDescriptorSet, 3> allSetsForCompute = {lightCullingDescriptorSet, cameraDescriptorSet, depthDescriptorSet};

	vkCmdBindDescriptorSets(lightCullingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getComputeLayout(), 0, 
		allSetsForCompute.size(), allSetsForCompute.data(), 0, nullptr);

	PushConstantObject pco = { static_cast<int> (windowWidth), static_cast<int> (windowHeight), tileCountPerRow, tileCountPerCol }; 
	vkCmdPushConstants(lightCullingCommandBuffer, pipeline.getComputeLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pco), &pco);
	vkCmdBindPipeline(lightCullingCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.getComputePipeline());
	vkCmdDispatch(lightCullingCommandBuffer, tileCountPerRow, tileCountPerCol, 1);

	//Nicht nötig da nur eine Queue
	/*std::vector<VkBufferMemoryBarrier> barriersAfter;
	VkBufferMemoryBarrier lightVisibilityBarrierAfter;
	lightVisibilityBarrierAfter.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	lightVisibilityBarrierAfter.pNext = nullptr;
	lightVisibilityBarrierAfter.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	lightVisibilityBarrierAfter.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	lightVisibilityBarrierAfter.srcQueueFamilyIndex = computeQueueIndex; 
	lightVisibilityBarrierAfter.dstQueueFamilyIndex = graphicsQueueIndex; 
	lightVisibilityBarrierAfter.buffer = lightVisibilityBuffer;
	lightVisibilityBarrierAfter.offset = 0;
	lightVisibilityBarrierAfter.size = lightVisibilityBufferSize;
	

	VkBufferMemoryBarrier pointlightBarrierAfter;
	pointlightBarrierAfter.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	pointlightBarrierAfter.pNext = nullptr;
	pointlightBarrierAfter.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	pointlightBarrierAfter.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	pointlightBarrierAfter.srcQueueFamilyIndex = computeQueueIndex;  
	pointlightBarrierAfter.dstQueueFamilyIndex = graphicsQueueIndex; 
	pointlightBarrierAfter.buffer = pointlightBuffer;
	pointlightBarrierAfter.offset = 0;
	pointlightBarrierAfter.size = pointlightBufferSize;

	barriersAfter.emplace_back(lightVisibilityBarrierAfter);
	barriersAfter.emplace_back(pointlightBarrierAfter);*/

	/*vkCmdPipelineBarrier(lightCullingCommandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VkDependencyFlags(), 0,
		nullptr, barriersAfter.size(), barriersAfter.data(), 0, nullptr);*/

	vkEndCommandBuffer(lightCullingCommandBuffer);
}

//Passiert in loadMesh
/*void createVertexBuffer() {
	createAndUploadBuffer(device, physicalDevice, queue, commandPool, vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexBufferDeviceMemory);
}

void createIndexBuffer() {
	createAndUploadBuffer(device, physicalDevice, queue, commandPool, indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexBufferDeviceMemory);
}*/

void recordDepthPrePassCommandBuffer() {
	VkCommandBufferBeginInfo commandBufferBeginInfo;
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;

	VkResult result = vkBeginCommandBuffer(depthPrepassCommandBuffer, &commandBufferBeginInfo);
	CHECK_FOR_CRASH(result);

	VkClearDepthStencilValue clearDepthValue;
	clearDepthValue.depth = 1.0f; // 1.0f ist far clipping plane
	clearDepthValue.stencil = 0;

	VkClearValue clearValue;
	clearValue.depthStencil =clearDepthValue;

	VkRect2D renderArea;
	renderArea.offset = { 0, 0 };
	renderArea.extent = {windowWidth, windowHeight};

	VkRenderPassBeginInfo depthPrepassInfo;
	depthPrepassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	depthPrepassInfo.pNext = nullptr;
	depthPrepassInfo.renderPass = depthPrePass;
	depthPrepassInfo.framebuffer = depthFramebuffer;
	depthPrepassInfo.renderArea = renderArea;
	depthPrepassInfo.clearValueCount = 1;
	depthPrepassInfo.pClearValues = &clearValue; 

	vkCmdBeginRenderPass(depthPrepassCommandBuffer, &depthPrepassInfo, VK_SUBPASS_CONTENTS_INLINE);
	
	//eine Iteration, nur für einfacheren Zugriff auf Meshparts über "part" statt immer mesh.getMeshParts().
	for (const auto& part : mesh.getMeshParts() ) {
		vkCmdBindPipeline(depthPrepassCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getDepthPipeline());
		std::array<VkDescriptorSet, 2> depthDescriptorSets = { descriptorSet, cameraDescriptorSet };
		vkCmdBindDescriptorSets(depthPrepassCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getDepthLayout(), 0, depthDescriptorSets.size(), depthDescriptorSets.data(), 0, nullptr);
		
		VkBuffer depthVertexBuffer[] = { part.vertexBufferSection.buffer };
		VkDeviceSize depthVertexBufferOffset[] = {part.vertexBufferSection.offset};
		vkCmdBindVertexBuffers(depthPrepassCommandBuffer, 0, 1, depthVertexBuffer, depthVertexBufferOffset);
		vkCmdBindIndexBuffer(depthPrepassCommandBuffer, part.indexBufferSection.buffer, part.indexBufferSection.offset, VK_INDEX_TYPE_UINT32);

		vkCmdDrawIndexed(depthPrepassCommandBuffer, part.indexCount, 1, 0, 0, 0);
	}

	vkCmdEndRenderPass(depthPrepassCommandBuffer);
	result = vkEndCommandBuffer(depthPrepassCommandBuffer);

}

void recordRenderCommandBuffers() {
	VkCommandBufferBeginInfo commandBufferBeginInfo;
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;

	for (size_t i = 0; i < amountOfImagesInSwapchain; i++) {
		VkResult result = vkBeginCommandBuffer(commandBuffers[i], &commandBufferBeginInfo);
		CHECK_FOR_CRASH(result);

		VkRenderPassBeginInfo renderPassBeginInfo;
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = framebuffers[i];
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = { windowWidth, windowHeight };
		VkClearValue clearValue;
		clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f }; //standardfarbe der Szene
		//VkClearValue depthClearValue = { 1.0f, 0 }; //don't reset with depth prepass!
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValue;

		VkViewport viewport;
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = windowWidth;
		viewport.height = windowHeight;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffers[i], 0, 1, &viewport);

		VkRect2D scissor;
		scissor.offset = { 0, 0 };
		scissor.extent = { windowWidth, windowHeight };
		vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);


		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE); //nur primäre Buffers nutzen, sonst VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS

		PushConstantObject pco = {
			static_cast<int>(windowWidth),
			static_cast<int>(windowHeight),
			tileCountPerRow, tileCountPerCol
		};
		pco.debugviewIndex = debugViewForFragment; //1 2 3 4 = debugview
		vkCmdPushConstants(commandBuffers[i], pipeline.getGraphicsLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pco), &pco);

		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getGraphicsPipeline());
		
		std::array<VkDescriptorSet, 4> descriptorSets = { descriptorSet, lightCullingDescriptorSet, cameraDescriptorSet, depthDescriptorSet }; //Reihenfolge wichtig, siehe allSetLayouts in createDescriptorSetLayout()!
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getGraphicsLayout(), 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

		//eine Iteration, nur für einfacheren Zugriff auf Meshparts über "part" statt immer mesh.getMeshParts().
		for (const auto& part : mesh.getMeshParts()) {
			VkBuffer vertexBuffer[] = { part.vertexBufferSection.buffer };
			VkDeviceSize offsets[] = { part.vertexBufferSection.offset };
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffer, offsets);
			vkCmdBindIndexBuffer(commandBuffers[i], part.indexBufferSection.buffer, part.indexBufferSection.offset, VK_INDEX_TYPE_UINT32);
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getGraphicsLayout(), 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr); //todo necessary?

			vkCmdDrawIndexed(commandBuffers[i], part.indexCount, 1, 0, 0, 0);
			vkCmdEndRenderPass(commandBuffers[i]);
		}
		result = vkEndCommandBuffer(commandBuffers[i]);
		CHECK_FOR_CRASH(result);
	}
}

void createSemaphores() {
	VkSemaphoreCreateInfo semaphoreCreateInfo;
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;


	VkResult result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphoreDepthPrepassDone);
	CHECK_FOR_CRASH(result);
	result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphoreLightcullingDone);
	CHECK_FOR_CRASH(result);
	result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphoreImageAvailable);
	CHECK_FOR_CRASH(result);
	result = vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphoreRenderingDone);
	CHECK_FOR_CRASH(result);
}

void updateMVP(float deltatime) {
	//update model ubo
	ubo.lightPosition = glm::vec4(0.0f) + glm::rotate(glm::mat4(1.0f), deltatime * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::vec4(0, 3, 1, 0);
	modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::scale(modelMatrix, glm::vec3(sceneConfiguration.scale));
	modelMatrix = glm::rotate(modelMatrix, deltatime * glm::radians(30.0f), glm::vec3(0, 0, 1.0f));
	//glm::rotate(welcheMatrix, wieSchnell, welcheAchse)
	ubo.model = modelMatrix;

	//update camera ubo
	viewMatrix = glm::lookAt(sceneConfiguration.cameraPosition, glm::vec3(0.0f, -3.0f, 0.0f), glm::vec3(0, 0, 1.0f));
	//glm::lookAt(cameraPos, schauDahin, upVector)
	cameraUbo.view = viewMatrix;
	cameraUbo.proj = glm::perspective(glm::radians(60.0f), windowWidth / (float)windowHeight, 0.01f, 100.0f);
	//glm::perspective(FOV, aspectRatio, nearPlane, farPlane)
	cameraUbo.proj[1][1] *= -1; //Y Axis zeigt in Vulkan nach unten
	cameraUbo.projview = cameraUbo.proj * cameraUbo.view;
	cameraUbo.cameraPos = sceneConfiguration.cameraPosition;

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(device, stagingBufferMemory);
	copyBuffer(device, queue, commandPool, stagingBuffer, uniformBuffer, sizeof(ubo), 0, 0);

	void* cameraData;
	vkMapMemory(device, cameraStagingBufferMemory, 0, sizeof(CameraUbo), 0, &cameraData);
	memcpy(cameraData, &cameraUbo, sizeof(cameraUbo));
	vkUnmapMemory(device, cameraStagingBufferMemory);
	copyBuffer(device, queue, commandPool, cameraStagingBuffer, cameraUniformBuffer, sizeof(cameraUbo), 0, 0);
}

void updateLights() {
	//update light ubo
	auto lightNums = static_cast<int>(pointLights.size());
	VkDeviceSize bufferSize = sizeof(PointLight) * MAX_POINT_LIGHT_COUNT + sizeof(glm::vec4);

	for (int i = 0; i < lightNums; i++) {
		pointLights[i].pos += glm::vec3(0, 0.2f, 0);
		//reset Lichter wenn Y zu groß
		if (pointLights[i].pos.y > sceneConfiguration.maxLightPos.y) {
			pointLights[i].pos.y -= (sceneConfiguration.maxLightPos.y - sceneConfiguration.minLightPos.y);
		}
	}

	auto pointlightsSize = sizeof(PointLight) * pointLights.size();
	void* rawData;
	vkMapMemory(device, lightStagingBufferMemory, 0, pointlightBufferSize, 0, &rawData);
	memcpy(rawData, &lightNums, sizeof(int));
	memcpy((char*)rawData + sizeof(glm::vec4), pointLights.data(), pointlightsSize);
	vkUnmapMemory(device, lightStagingBufferMemory);
	copyBuffer(device, queue, commandPool, lightStagingBuffer, pointlightBuffer, pointlightBufferSize, 0, 0);
}

void drawFrame() {
	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(device, swapchain, (std::numeric_limits<uint64_t>::max)(), semaphoreImageAvailable, VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapchain();
		return;
	}
	//Submit depth prepass to graphics queue
	VkSubmitInfo submitInfoDepthPrepass;
	submitInfoDepthPrepass.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfoDepthPrepass.pNext = nullptr;
	submitInfoDepthPrepass.waitSemaphoreCount = 0;
	submitInfoDepthPrepass.pWaitSemaphores = nullptr;
	submitInfoDepthPrepass.pWaitDstStageMask = nullptr;
	submitInfoDepthPrepass.commandBufferCount = 1;
	submitInfoDepthPrepass.pCommandBuffers = &depthPrepassCommandBuffer;
	submitInfoDepthPrepass.signalSemaphoreCount = 1;
	submitInfoDepthPrepass.pSignalSemaphores = &semaphoreDepthPrepassDone;

	result = vkQueueSubmit(queue, 1, &submitInfoDepthPrepass, VK_NULL_HANDLE);
	CHECK_FOR_CRASH(result);


	//Submit lightculling to compute queue
	VkPipelineStageFlags waitStagesLight[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
	VkSubmitInfo submitInfoLightculling;
	submitInfoLightculling.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfoLightculling.pNext = nullptr;
	submitInfoLightculling.waitSemaphoreCount = 1;
	submitInfoLightculling.pWaitSemaphores = &semaphoreDepthPrepassDone; //warte auf diese
	submitInfoLightculling.pWaitDstStageMask = waitStagesLight; //wo in der Pipeline warten
	submitInfoLightculling.commandBufferCount = 1;
	submitInfoLightculling.pCommandBuffers = &lightCullingCommandBuffer;
	submitInfoLightculling.signalSemaphoreCount = 1;
	submitInfoLightculling.pSignalSemaphores = &semaphoreLightcullingDone; //setze diese am Ende

	result = vkQueueSubmit(computeQueue, 1, &submitInfoLightculling, VK_NULL_HANDLE);
	CHECK_FOR_CRASH(result);


	//Submit Graphics to graphics queue	
	VkPipelineStageFlags waitStagesRender[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
	VkSemaphore waitSemaphores[] = { semaphoreImageAvailable, semaphoreLightcullingDone };
	VkSubmitInfo submitInfoRender;
	submitInfoRender.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfoRender.pNext = nullptr;
	submitInfoRender.waitSemaphoreCount = 2;
	submitInfoRender.pWaitSemaphores = waitSemaphores;
	submitInfoRender.pWaitDstStageMask = waitStagesRender;
	submitInfoRender.commandBufferCount = 1;
	submitInfoRender.pCommandBuffers = &(commandBuffers[imageIndex]);
	submitInfoRender.signalSemaphoreCount = 1;
	submitInfoRender.pSignalSemaphores = &semaphoreRenderingDone;
	result = vkQueueSubmit(queue, 1, &submitInfoRender, VK_NULL_HANDLE);
	CHECK_FOR_CRASH(result);

	VkPresentInfoKHR presentInfo;
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &semaphoreRenderingDone;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(queue, &presentInfo);
	CHECK_FOR_CRASH(result);
}


void startVulkan() {
	setSceneConfiguration();
	createInstance();
	pickPhysicalDevice();
	printInstanceLayers();
	printInstanceExtensions();
	createGlfwWindowSurface();
	printallPhysicalDevicesStats();
	createLogicalDevice();
	createQueue();
	checkSurfaceSupport();
	createSwapchain();
	createImageViews();
	createRenderPasses();
	createDescriptorSetLayout();
	createPipelineGraphics();
	createPipelineCompute();
	createCommandPool();
	createCommandBuffers();
	createDepthImage();
	createFramebuffers();
	createUniformBuffer();
	createLights();
	loadTexture();
	loadMesh();
	//Passiert in loadMesh
	//createVertexBuffer();
	//createIndexBuffer();
	createDescriptorPool();
	createDescriptorSet();
	createLightCullingDescriptorSet();
	createCameraDescriptorSet();
	createDepthDescriptorSet();
	updateDepthDescriptorSet();
	createLightVisibilityBuffer();
	recordLightCullingCommandBuffer();
	recordDepthPrePassCommandBuffer();
	recordRenderCommandBuffers();
	createSemaphores();
}

void recreateSwapchain() {
	vkDeviceWaitIdle(device);

	depthImage.destroy();

	vkFreeCommandBuffers(device, commandPool, amountOfImagesInSwapchain, commandBuffers);
	delete[] commandBuffers;
	vkFreeCommandBuffers(device, commandPoolCompute, 1, &lightCullingCommandBuffer);
	
	vkFreeMemory(device, lightVisibilityBufferMemory, nullptr);
	vkDestroyBuffer(device, lightVisibilityBuffer, nullptr);

	for (size_t i = 0; i < amountOfImagesInSwapchain; i++) {
		vkDestroyFramebuffer(device, framebuffers[i], nullptr);
	}
	delete[] framebuffers;

	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroyRenderPass(device, depthPrePass, nullptr);
	for (int i = 0; i < amountOfImagesInSwapchain; i++) {
		vkDestroyImageView(device, imageViews[i], nullptr);
	}
	delete[] imageViews;

	pipeline.destroy();

	VkSwapchainKHR oldSwapchain = swapchain;

	createSwapchain();
	createImageViews();
	createRenderPasses();
	pipeline.iniGraphics(shaderModuleVert, shaderModuleFrag, shaderModuleDepth, windowWidth, windowHeight);
	pipeline.createGraphics(device, renderPass, depthPrePass, allSetLayouts);
	pipeline.iniCompute(allSetLayoutsForCompute, shaderModuleComp);
	pipeline.createCompute();
	createDepthImage();
	createFramebuffers();
	createLightVisibilityBuffer();
	updateDepthDescriptorSet();
	createCommandBuffers();
	recordLightCullingCommandBuffer();
	recordDepthPrePassCommandBuffer();
	recordRenderCommandBuffers();

	vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
}

void mainLoop() {
	auto startTime = std::chrono::high_resolution_clock::now();
	decltype(startTime) current;
	float timeSinceStart;
	std::cout << 2.0 * 16 / windowWidth << std::endl;
	std::cout << tileCountPerRow;

	while (!glfwWindowShouldClose(window)) {
		current = std::chrono::high_resolution_clock::now();
		timeSinceStart = std::chrono::duration<float>(current - startTime).count();
		glfwPollEvents();
		updateMVP(timeSinceStart);
		updateLights();
		drawFrame();
		auto after = std::chrono::high_resolution_clock::now();
		//FPS ausgeben
		double timeNeededInMicro = std::chrono::duration_cast<std::chrono::microseconds>(after - current).count();
		//std::cout << "FPS: " << 1 / (timeNeededInMicro / 1000000) << std::endl;

		//Einfache Kamerabewegungen
		if (GetAsyncKeyState('D') & 0x8000){
			sceneConfiguration.cameraPosition.x += 1;
		}
		if (GetAsyncKeyState('A') & 0x8000) {
			sceneConfiguration.cameraPosition.x -= 1;
		}
		if (GetAsyncKeyState('S') & 0x8000) {
			sceneConfiguration.cameraPosition.y += 1;
		}
		if (GetAsyncKeyState('W') & 0x8000) {
			sceneConfiguration.cameraPosition.y -= 1;
		}
		if (GetAsyncKeyState('R') & 0x8000) {
			sceneConfiguration.cameraPosition = glm::vec3{ 0.7f, 7.0f, 5.0f }; //reset cam
		}

		if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
			return;
		}
	}
}

void shutdownVulkan() {
	vkDeviceWaitIdle(device);

	depthImage.destroy();

	vkFreeDescriptorSets(device, descriptorPool, 1, &lightCullingDescriptorSet);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayoutLightCulling, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayoutCamera, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayoutDepth, nullptr);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);

	vkFreeMemory(device, uniformBufferMemory, nullptr);
	vkDestroyBuffer(device, uniformBuffer, nullptr);
	vkFreeMemory(device, lightVisibilityBufferMemory, nullptr);
	vkDestroyBuffer(device, lightVisibilityBuffer, nullptr);
	vkFreeMemory(device, pointlightBufferMemory, nullptr);
	vkDestroyBuffer(device, pointlightBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, cameraStagingBufferMemory, nullptr);
	vkDestroyBuffer(device, cameraStagingBuffer, nullptr);
	vkFreeMemory(device, cameraUniformBufferMemory, nullptr);
	vkDestroyBuffer(device, cameraUniformBuffer, nullptr);
	vkFreeMemory(device, lightStagingBufferMemory, nullptr);
	vkDestroyBuffer(device, lightStagingBuffer, nullptr);
	mesh.cleanup(device);

	ezImage.destroy();

	vkDestroySemaphore(device, semaphoreImageAvailable, nullptr);
	vkDestroySemaphore(device, semaphoreRenderingDone, nullptr);
	vkDestroySemaphore(device, semaphoreLightcullingDone, nullptr);
	vkDestroySemaphore(device, semaphoreDepthPrepassDone, nullptr);

	vkFreeCommandBuffers(device, commandPool, amountOfImagesInSwapchain, commandBuffers);
	delete[] commandBuffers;
	vkDestroyCommandPool(device, commandPoolCompute, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);

	for (size_t i = 0; i < amountOfImagesInSwapchain; i++) {
		vkDestroyFramebuffer(device, framebuffers[i], nullptr);
	}
	delete[] framebuffers;
	vkDestroyFramebuffer(device, depthFramebuffer, nullptr);

	pipeline.destroy();

	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroyRenderPass(device, depthPrePass, nullptr);
	for (int i = 0; i < amountOfImagesInSwapchain; i++) {
		vkDestroyImageView(device, imageViews[i], nullptr);
	}
	delete[] imageViews;
	
	vkDestroyShaderModule(device, shaderModuleVert, nullptr);
	vkDestroyShaderModule(device, shaderModuleFrag, nullptr);
	vkDestroyShaderModule(device, shaderModuleComp, nullptr);
	vkDestroyShaderModule(device, shaderModuleDepth, nullptr);
	vkDestroySwapchainKHR(device, swapchain, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyInstance(instance, nullptr);
}

void shutdownGLFW() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

int main(){
#ifdef _DEBUG
	system("runShaderCompiler.bat");
#endif
	startGLFW();
	startVulkan();
	mainLoop();
	shutdownVulkan();
	shutdownGLFW();
	return 0;
}