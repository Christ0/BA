#pragma once
#include "Vertex.h"
#include "DepthImage.h"
#include "VulkanUtils.h"
#include <vector>
#include <array>

class Pipeline {
private:
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipelineLayout depthPipelinaLayout = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipeline depthPipeline = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;

	VkPipelineShaderStageCreateInfo shaderStageCreateInfoVert;
	VkPipelineShaderStageCreateInfo shaderStageCreateInfoFrag;
	VkPipelineShaderStageCreateInfo shaderStageCreateInfoDepth;
	VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo;
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo;
	VkViewport viewport;
	VkRect2D scissor;
	VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
	VkPipelineRasterizationStateCreateInfo rasterizationCreateInfo;
	VkPipelineMultisampleStateCreateInfo multisampleCreateInfo;
	VkPipelineColorBlendAttachmentState colorBlendAttachment;
	VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo;
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
	VkPipelineDepthStencilStateCreateInfo prePassDepthStencilStateCreateInfo;
	VkPushConstantRange pushConstantRange;

	std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	std::array<VkDescriptorSetLayout, 2> depthSetLayout;

	VkVertexInputBindingDescription vertexBindingDescription = Vertex::getBindingDescription();
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions = Vertex::getAttributeDescriptions();

	bool wasInitialized = false;
	bool wasCreated = false;
public:
	Pipeline(){}

	void ini(VkShaderModule vertexShader, VkShaderModule fragmentShader, VkShaderModule depthShader, uint32_t windowWidth, uint32_t windowHeight) {
		shaderStageCreateInfoVert.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfoVert.pNext = nullptr;
		shaderStageCreateInfoVert.flags = 0;
		shaderStageCreateInfoVert.stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageCreateInfoVert.module = vertexShader;
		shaderStageCreateInfoVert.pName = "main"; //Startpunkt des Shaders
		shaderStageCreateInfoVert.pSpecializationInfo = nullptr;
		
		shaderStageCreateInfoFrag.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfoFrag.pNext = nullptr;
		shaderStageCreateInfoFrag.flags = 0;
		shaderStageCreateInfoFrag.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStageCreateInfoFrag.module = fragmentShader;
		shaderStageCreateInfoFrag.pName = "main"; //Startpunkt des Shaders
		shaderStageCreateInfoFrag.pSpecializationInfo = nullptr;

		vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputCreateInfo.pNext = nullptr;
		vertexInputCreateInfo.flags = 0;
		vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
		vertexInputCreateInfo.pVertexBindingDescriptions = &vertexBindingDescription;
		vertexInputCreateInfo.vertexAttributeDescriptionCount = vertexAttributeDescriptions.size();
		vertexInputCreateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions.data();
		
		inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyCreateInfo.pNext = nullptr;
		inputAssemblyCreateInfo.flags = 0;
		inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
		
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = windowWidth;
		viewport.height = windowHeight;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		
		scissor.offset = { 0, 0 };
		scissor.extent = { windowWidth, windowHeight };
		
		viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportStateCreateInfo.pNext = nullptr;
		viewportStateCreateInfo.flags = 0;
		viewportStateCreateInfo.viewportCount = 1;
		viewportStateCreateInfo.pViewports = &viewport;
		viewportStateCreateInfo.scissorCount = 1;
		viewportStateCreateInfo.pScissors = &scissor;
	
		rasterizationCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationCreateInfo.pNext = nullptr;
		rasterizationCreateInfo.flags = 0;
		rasterizationCreateInfo.depthClampEnable = VK_FALSE;
		rasterizationCreateInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizationCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizationCreateInfo.depthBiasEnable = VK_FALSE;
		rasterizationCreateInfo.depthBiasConstantFactor = 0.0f;
		rasterizationCreateInfo.depthBiasClamp = 0.0f;
		rasterizationCreateInfo.depthBiasSlopeFactor = 0.0f;
		rasterizationCreateInfo.lineWidth = 1.0f;
		
		multisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleCreateInfo.pNext = nullptr;
		multisampleCreateInfo.flags = 0;
		multisampleCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleCreateInfo.sampleShadingEnable = VK_FALSE;
		multisampleCreateInfo.minSampleShading = 1.0f;
		multisampleCreateInfo.pSampleMask = nullptr;
		multisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
		multisampleCreateInfo.alphaToOneEnable = VK_FALSE;
		
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		
		colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendCreateInfo.pNext = nullptr;
		colorBlendCreateInfo.flags = 0;
		colorBlendCreateInfo.logicOpEnable = VK_FALSE;
		colorBlendCreateInfo.logicOp = VK_LOGIC_OP_NO_OP;
		colorBlendCreateInfo.attachmentCount = 1;
		colorBlendCreateInfo.pAttachments = &colorBlendAttachment;
		colorBlendCreateInfo.blendConstants[0] = 0.0f;
		colorBlendCreateInfo.blendConstants[1] = 0.0f;
		colorBlendCreateInfo.blendConstants[2] = 0.0f;
		colorBlendCreateInfo.blendConstants[3] = 0.0f;
		
		dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateCreateInfo.pNext = nullptr;
		dynamicStateCreateInfo.flags = 0;
		dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
		dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();
		
		pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(PushConstantObject);


		//=============Depth PrePass Pipeline=================

		prePassDepthStencilStateCreateInfo = DepthImage::getDepthStencilStateCreateInfoOpaque();
		prePassDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
		prePassDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;

		shaderStageCreateInfoDepth.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfoDepth.pNext = nullptr;
		shaderStageCreateInfoDepth.flags = 0;
		shaderStageCreateInfoDepth.stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStageCreateInfoDepth.module = depthShader;
		shaderStageCreateInfoDepth.pName = "main";
		shaderStageCreateInfoDepth.pSpecializationInfo = nullptr;

		wasInitialized = true;
	}

	void create(VkDevice device, VkRenderPass renderPass, VkRenderPass depthPrePass, std::vector<VkDescriptorSetLayout> descriptorSetLayout) {
		if (!wasInitialized) {
			throw std::logic_error("Call ini() first!");
		}
		if (wasCreated) {
			throw std::logic_error("Pipeline was already created!");
		}

		this->device = device;

		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.push_back(shaderStageCreateInfoVert);
		shaderStages.push_back(shaderStageCreateInfoFrag);

		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pNext = nullptr;
		pipelineLayoutCreateInfo.flags = 0;
		pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayout.size();
		pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayout.data();
		pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		CHECK_FOR_CRASH(result);

		VkGraphicsPipelineCreateInfo pipelineCreateInfo;
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.pNext = nullptr;
		pipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
		pipelineCreateInfo.stageCount = shaderStages.size();
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		pipelineCreateInfo.pTessellationState = nullptr;
		pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &DepthImage::getDepthStencilStateCreateInfoOpaque();
		pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
		pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.subpass = 0;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineCreateInfo.basePipelineIndex = -1;

		result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
		CHECK_FOR_CRASH(result);



		depthSetLayout = { descriptorSetLayout.at(0), descriptorSetLayout.at(2) }; // 0 und 2 sind die Layouts für das allgemeine 
																	   // und das für die Kamera, siehe allSetLayouts in main.cpp createDescriptorSetLayout()
		
		VkPipelineLayoutCreateInfo depthPipelineLayoutInfo;
		depthPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		depthPipelineLayoutInfo.pNext = nullptr;
		depthPipelineLayoutInfo.flags = 0;
		depthPipelineLayoutInfo.setLayoutCount = depthSetLayout.size();
		depthPipelineLayoutInfo.pSetLayouts = depthSetLayout.data();
		depthPipelineLayoutInfo.pushConstantRangeCount = 0;
		depthPipelineLayoutInfo.pPushConstantRanges = nullptr;

		result = vkCreatePipelineLayout(device, &depthPipelineLayoutInfo, nullptr, &depthPipelinaLayout);
		CHECK_FOR_CRASH(result);

		VkGraphicsPipelineCreateInfo depthPipelineCreateInfo;
		depthPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		depthPipelineCreateInfo.pNext = nullptr;
		depthPipelineCreateInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
		depthPipelineCreateInfo.stageCount = 1;
		depthPipelineCreateInfo.pStages = &shaderStageCreateInfoDepth;
		depthPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
		depthPipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
		depthPipelineCreateInfo.pTessellationState = nullptr;
		depthPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		depthPipelineCreateInfo.pRasterizationState = &rasterizationCreateInfo;
		depthPipelineCreateInfo.pMultisampleState = &multisampleCreateInfo;
		depthPipelineCreateInfo.pDepthStencilState = &prePassDepthStencilStateCreateInfo;
		depthPipelineCreateInfo.pColorBlendState = nullptr;
		depthPipelineCreateInfo.pDynamicState = nullptr;
		depthPipelineCreateInfo.layout = depthPipelinaLayout;
		depthPipelineCreateInfo.renderPass = depthPrePass;
		depthPipelineCreateInfo.subpass = 0;
		depthPipelineCreateInfo.basePipelineHandle = pipeline;
		depthPipelineCreateInfo.basePipelineIndex = -1;

		vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &depthPipelineCreateInfo, nullptr, &depthPipeline);

		wasCreated = true;
	}

	void destroy() {
		if (wasCreated) {
			vkDestroyPipeline(device, pipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyPipeline(device, depthPipeline, nullptr);
			vkDestroyPipelineLayout(device, depthPipelinaLayout, nullptr);
			wasCreated = false;
		}
	}

	VkPipeline getPipeline() {
		if (!wasCreated) {
			throw std::logic_error("Pipeline was not created!");
		}
		return pipeline;
	}

	VkPipeline getDepthPipeline() {
		if (!wasCreated) {
			throw std::logic_error("Pipeline was not created!");
		}
		return depthPipeline;
	}

	VkPipelineLayout getLayout() {
		if (!wasCreated) {
			throw std::logic_error("Pipeline was not created!");
		}
		return pipelineLayout;
	}

	VkPipelineLayout getDepthLayout() {
		if (!wasCreated) {
			throw std::logic_error("Pipeline was not created!");
		}
		return depthPipelinaLayout;
	}
};