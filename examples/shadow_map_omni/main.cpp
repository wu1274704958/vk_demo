//
// Created by A on 2023/4/30.
//
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#define INSTANCE_COUNT 2048
#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1
#define RADIUS 25.0f
#define ORTHO_SIZE 20.0f
#define Light_Move_Speed 0.1f
#define DEPTH_FORMAT VK_FORMAT_R32_SFLOAT
// Depth bias (and slope) are used to avoid shadowing artifacts
// Constant depth bias factor (always applied)
float depthBiasConstant = 1.25f;
// Slope depth bias factor, applied depending on polygon's slope
float depthBiasSlope = 1.75f;
struct DepthUBO{
	glm::mat4 model;
	glm::vec4 lightPos;
};
class VulkanExample : public VulkanExampleBase{
 public:
	struct SpotLightData {
		glm::vec3 direct;
		float phi = glm::radians(18.0f);
		float theta = glm::radians(10.0f);
		float range = 150.0f;
	};
	VulkanExample() : VulkanExampleBase(true)
	{
		title = "Shadow Mapping Point Light";
		camera.type = Camera::CameraType::firstperson;
		camera.setPerspective(60.f,(float )width / (float) height,0.01,1024.0f);
		camera.setPosition(glm::vec3(0.6702f, 12.356249f, -23.246f));
		camera.setRotation(glm::vec3(-20.0f, 0.0f, 0.0f));
		camera.movementSpeed = 50.0f;

		sceneData.plane = glm::scale(sceneData.plane, glm::vec3(20.0f, 20.0f, 20.0f));
//		sceneData.plane = glm::scale(sceneData.plane,glm::vec3(20.0f,1.0f,20.0f));
	}
	~VulkanExample()
	{
		//destroy spot light
		spotLight.buffer.destroy();
		//destroy depth resource
		vkDestroySampler(device,depth_pass.sampler,nullptr);
		vkDestroyImageView(device,depth_pass.attachment.view,nullptr);
		for(auto& v : depth_pass.views)
			vkDestroyImageView(device,v,nullptr);
		vkDestroyImage(device,depth_pass.attachment.image,nullptr);
		vkFreeMemory(device,depth_pass.attachment.memory,nullptr);

		vkDestroyImageView(device,depth_pass.depthAttachment.view,nullptr);
		vkDestroyImage(device,depth_pass.depthAttachment.image,nullptr);
		vkFreeMemory(device,depth_pass.depthAttachment.memory,nullptr);

		for(auto& frameBuffer : depth_pass.frameBuffers)
			vkDestroyFramebuffer(device,frameBuffer,nullptr);
		vkDestroyRenderPass(device,depth_pass.renderPass,nullptr);


		uniformData.plane.destroy();
		uniformData.cube.destroy();
		uniformData.depth.destroy();
		//clear buffers
		vkDestroyPipeline(device,pipelines.base, nullptr);
		vkDestroyPipeline(device,pipelines.depth, nullptr);
		vkDestroyPipeline(device, pipelines.cube, nullptr);
		//clear pipeline layout
		vkDestroyPipelineLayout(device,pipelineLayouts.base,nullptr);
		vkDestroyPipelineLayout(device,pipelineLayouts.depth,nullptr);
		//clear descriptor pool
		vkDestroyDescriptorSetLayout(device,descriptorSetLayouts.base, nullptr);
		vkDestroyDescriptorSetLayout(device,descriptorSetLayouts.depth, nullptr);
	}
	void getEnabledFeatures() override {
		// Example uses multi draw indirect if available
		if(deviceFeatures.geometryShader)
			enabledFeatures.geometryShader = VK_TRUE;
		else
			vks::tools::exitFatal("Geometry shader not supported",-1);
		if(deviceFeatures.wideLines)
			enabledFeatures.wideLines = VK_TRUE;
		if(deviceFeatures.fillModeNonSolid)
			enabledFeatures.fillModeNonSolid = VK_TRUE;
	}

	void loadAssets()
	{
		scene.plane.loadFromFile(getAssetPath() + "models/sphere.gltf", vulkanDevice, queue,vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors);
		scene.cube.loadFromFile(getAssetPath() + "models/shadowscene_fire.gltf", vulkanDevice, queue, vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors);
	}
	void updateLight()
	{
		// Animate the light source
		uboVP.lightPos.x = glm::sin(time * 4.0f) * 0.3f;
		uboVP.lightPos.z = glm::cos(time * 3.0f) * 0.4f;
		uboVP.lightPos.y = -2 + glm::sin(time * 3.0f) * 0.5f;
	}

	float CalcPerspectiveFov()
	{
//		float halfSideLen = glm::sin(spotLight.data.phi) * 1;
//		float hypotenuseLen = glm::sqrt(glm::pow(halfSideLen,2.0f) + glm::pow(halfSideLen,2.0f));
//		float angle = glm::asin(hypotenuseLen / 1.0f) * 2.0f;
		return spotLight.data.phi * 2.0f;
	}

	void updateDepthUniformBuffer(bool changed)
	{
		uboDepth._1 = { sceneData.plane , uboVP.lightPos } ;
		uboDepth._2 = { sceneData.cube , uboVP.lightPos };
		memcpy(uniformData.depth.mapped,&uboDepth,sizeof(uboDepth));
	}
	void updateUniformBuffer(bool viewChanged) {
		updateLight();
		updateDepthUniformBuffer(viewChanged);
		if(viewChanged)
		{
			uboVP.projection = camera.matrices.perspective;
			uboVP.view = camera.matrices.view;
			uboVP.viewPos = glm::vec4 (camera.position,1.0f);
		}
		uboVP.model = sceneData.plane;
		memcpy(uniformData.plane.mapped,&uboVP,sizeof(uboVP));
		uboVP.model = sceneData.cube;
		memcpy(uniformData.cube.mapped,&uboVP,sizeof(uboVP));


	}

	void prepareUniformBuffers()
	{
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&uniformData.plane, sizeof(uboVP)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&uniformData.cube, sizeof(uboVP)));
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&uniformData.depth, sizeof(uboDepth)));
		VK_CHECK_RESULT(uniformData.plane.map());
		VK_CHECK_RESULT(uniformData.cube.map());
		VK_CHECK_RESULT(uniformData.depth.map());
		updateUniformBuffer(true);
		// spot light
		VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
			&spotLight.buffer, sizeof(SpotLightData)));
		VK_CHECK_RESULT(spotLight.buffer.map());
		updateSpotLightUniformBuffer(true);
	}

	void setupDescriptorPool()
	{
		std::array<VkDescriptorPoolSize, 2> poolSizes = {
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,6},
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 10);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device,&descriptorPoolInfo,nullptr,&descriptorPool));
	}

	void setupDescriptorSetLayouts() {
		std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
		};
		std::array<VkDescriptorSetLayoutBinding, 1> depthBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		};
		VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(bindings.data(), bindings.size());
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayouts.base));

		VkDescriptorSetLayoutCreateInfo depthDescriptorSetLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(depthBindings.data(), depthBindings.size());
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &depthDescriptorSetLayoutInfo, nullptr, &descriptorSetLayouts.depth));

		std::array<VkPushConstantRange, 1> pushConstantRanges = {
			vks::initializers::pushConstantRange(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float),0)
		};
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.base, 1);
		pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
		pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts.base));

		pushConstantRanges[0] = vks::initializers::pushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4),0);
		VkPipelineLayoutCreateInfo depthPipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.depth, 1);
		depthPipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
		depthPipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &depthPipelineLayoutInfo, nullptr, &pipelineLayouts.depth));
	}

	void setupDescriptorSet()
	{
		VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.base, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.plane));
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.cube));
		allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.depth, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.planeForDepth));
		allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayouts.depth, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSets.cubeForDepth));

		std::array<VkWriteDescriptorSet, 3> descriptorWrites = {
			vks::initializers::writeDescriptorSet(descriptorSets.plane, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformData.plane.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.plane, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &depth_pass.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.plane, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &spotLight.buffer.descriptor),
		};

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		descriptorWrites[0].dstSet = descriptorSets.cube;
		descriptorWrites[0].pBufferInfo = &uniformData.cube.descriptor;
		descriptorWrites[1].dstSet = descriptorSets.cube;
		descriptorWrites[2].dstSet = descriptorSets.cube;
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		// mat1 plane mat2 cube
		VkDescriptorBufferInfo depthDescriptorBufferInfo = { .buffer = uniformData.depth.buffer, .offset = 0, .range = sizeof(DepthUBO) };
		std::array<VkWriteDescriptorSet, 1> depthDescriptorWrites = {
			vks::initializers::writeDescriptorSet(descriptorSets.planeForDepth, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &depthDescriptorBufferInfo)
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(depthDescriptorWrites.size()), depthDescriptorWrites.data(), 0, nullptr);
		depthDescriptorBufferInfo = { .buffer = uniformData.depth.buffer, .offset = sizeof(DepthUBO) + sizeof(uboDepth.space), .range = sizeof(DepthUBO) };
		depthDescriptorWrites[0].dstSet = descriptorSets.cubeForDepth;
		vkUpdateDescriptorSets(device,static_cast<uint32_t>(depthDescriptorWrites.size()), depthDescriptorWrites.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineColorBlendAttachmentState colorBlendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf,VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = vks::initializers::pipelineColorBlendStateCreateInfo(1,&colorBlendAttachmentState);
		VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
		VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL,VK_CULL_MODE_BACK_BIT,VK_FRONT_FACE_COUNTER_CLOCKWISE);
		VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE,VK_TRUE,VK_COMPARE_OP_LESS_OR_EQUAL);

		std::array<VkDynamicState,4> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_LINE_WIDTH,VK_DYNAMIC_STATE_DEPTH_BIAS};
		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStates.data(),dynamicStates.size() - 1);
		VkPipelineViewportStateCreateInfo viewportStateCreateInfo = vks::initializers::pipelineViewportStateCreateInfo(1,1);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(pipelineLayouts.base, renderPass);
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
		pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
		pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
		pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
		pipelineCreateInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::UV });
		pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
		pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
		pipelineCreateInfo.pStages = shaderStages.data();
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		// pipeline for indirect(and instance) draw
		shaderStages[0] = loadShader(getShadersPath() + "shadow_mapping_omni/base.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "shadow_mapping_omni/base.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);


		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.base));
		rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.cube));
		//depth pipeline
		shaderStages[0] = loadShader(getShadersPath() + "shadow_mapping_omni/shadowMap.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "shadow_mapping_omni/shadowMap.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		pipelineCreateInfo.layout = pipelineLayouts.depth;
		pipelineCreateInfo.renderPass = depth_pass.renderPass;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.depth));
	}

	void drawDepthPass(int j, VkCommandBuffer& cmd)
	{
		glm::mat4 viewMatrix = glm::mat4(1.0f);
		switch (j)
		{
		case 0: // POSITIVE_X
			viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 1:	// NEGATIVE_X
			viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 2:	// POSITIVE_Y
			viewMatrix = glm::rotate(viewMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 3:	// NEGATIVE_Y
			viewMatrix = glm::rotate(viewMatrix, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 4:	// POSITIVE_Z
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			break;
		case 5:	// NEGATIVE_Z
			viewMatrix = glm::rotate(viewMatrix, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
			break;
		}
		auto pos = glm::vec3 (-uboVP.lightPos.x,-uboVP.lightPos.y,-uboVP.lightPos.z);
		viewMatrix = glm::translate(viewMatrix,pos);

		glm::mat vp = depth_pass.perspective * viewMatrix;
		VkViewport viewport = vks::initializers::viewport(depth_pass.size, depth_pass.size, 0.0f, 1.0f);
		vkCmdSetViewport(cmd,0,1,&viewport);

		VkRect2D scissor = vks::initializers::rect2D(depth_pass.size, depth_pass.size, 0, 0);
		vkCmdSetScissor(cmd,0,1,&scissor);

		vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines.depth);

		vkCmdPushConstants(cmd,pipelineLayouts.depth,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(glm::mat4),&vp);

//		vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayouts.depth,0,1,&descriptorSets.planeForDepth,0,nullptr);
//		scene.plane.draw(cmd);

		vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayouts.depth,0,1,&descriptorSets.cubeForDepth,0,nullptr);
		scene.cube.draw(cmd);
	}

	void buildCommandBuffers() override {
		VulkanExampleBase::buildCommandBuffers();
		VkCommandBufferBeginInfo beginInfo = vks::initializers::commandBufferBeginInfo();
		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
		clearValues[1].depthStencil = {1.0f, 0};
		VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();

		std::array<VkClearValue, 2> depthClearValue = {};
		depthClearValue[0].color = {99999.0f, 0.0f, 0.0f, 1.0f};
		depthClearValue[1].depthStencil = {1.0f, 0};

		VkRenderPassBeginInfo depthPassBeginInfo = vks::initializers::renderPassBeginInfo();
		depthPassBeginInfo.renderPass = depth_pass.renderPass;
		depthPassBeginInfo.renderArea.extent.width = depth_pass.size;
		depthPassBeginInfo.renderArea.extent.height = depth_pass.size;

		depthPassBeginInfo.pClearValues = depthClearValue.data();
		depthPassBeginInfo.clearValueCount = depthClearValue.size();


		VkDeviceSize offset = 0;
		for(int i = 0;i < this->drawCmdBuffers.size();++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(this->drawCmdBuffers[i], &beginInfo));
			//depth pass
			for(int j = 0;j < 6;++j)
			{
				depthPassBeginInfo.framebuffer = depth_pass.frameBuffers[j];
				vkCmdBeginRenderPass(this->drawCmdBuffers[i],&depthPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);
				drawDepthPass(j,drawCmdBuffers[i]);
				vkCmdEndRenderPass(drawCmdBuffers[i]);
			}
			renderPassBeginInfo.framebuffer = frameBuffers[i];
			//draw pass --------------------------------------------------------------------
			vkCmdBeginRenderPass(this->drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport(width, height, 0.0f, 1.0f);
			vkCmdSetViewport(this->drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(this->drawCmdBuffers[i], 0, 1, &scissor);

			float isCustomNormal = 1.0f;

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.cube);

			vkCmdBindDescriptorSets(drawCmdBuffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayouts.base,
				0,
				1,
				&descriptorSets.plane,
				0,
				nullptr);
			vkCmdPushConstants(drawCmdBuffers[i],pipelineLayouts.base,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(isCustomNormal),&isCustomNormal);
			scene.plane.draw(drawCmdBuffers[i]);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.base);

			vkCmdBindDescriptorSets(drawCmdBuffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayouts.base,
				0,
				1,
				&descriptorSets.cube,
				0,
				nullptr);
			isCustomNormal = 0.0f;
			vkCmdPushConstants(drawCmdBuffers[i],pipelineLayouts.base,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(isCustomNormal),&isCustomNormal);
			scene.cube.draw(drawCmdBuffers[i]);

			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}
	void prepareDepthRenderPass()
	{
		//create depth Render pass
		std::array<VkAttachmentDescription,2> attachments = {};
		attachments[0].format = DEPTH_FORMAT;
		attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		attachments[1].format = VK_FORMAT_D24_UNORM_S8_UINT;
		attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//sub pass description
		std::array<VkAttachmentReference,2> attachmentRefs = {};
		attachmentRefs[0] = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		attachmentRefs[1] = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription pass{};
		pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		pass.colorAttachmentCount = 1;
		pass.pColorAttachments = &attachmentRefs[0];
		pass.pDepthStencilAttachment = &attachmentRefs[1];

		auto renderPassInfo = vks::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = attachments.size();
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &pass;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &depth_pass.renderPass));
	}
	void prepareDepthPassAttachment()
	{
		//create Image
		auto imageInfo = vks::initializers::imageCreateInfo();
		imageInfo.arrayLayers = 6;
		imageInfo.extent.width = depth_pass.size;
		imageInfo.extent.height = depth_pass.size;
		imageInfo.extent.depth = 1;
		imageInfo.format = DEPTH_FORMAT;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.mipLevels = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &depth_pass.attachment.image));
		//allocation Image Memory

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, depth_pass.attachment.image, &memoryRequirements);
		auto memInfo = vks::initializers::memoryAllocateInfo();
		memInfo.allocationSize = memoryRequirements.size;
		memInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memInfo, nullptr, &depth_pass.attachment.memory));
		VK_CHECK_RESULT(vkBindImageMemory(device, depth_pass.attachment.image, depth_pass.attachment.memory, 0));

		VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkImageSubresourceRange subresourceLayout = { };
		subresourceLayout.layerCount=6;
		subresourceLayout.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceLayout.levelCount=1;
		vks::tools::setImageLayout(layoutCmd,
			depth_pass.attachment.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceLayout);

		vulkanDevice->flushCommandBuffer(layoutCmd,queue);

		//create view
		VkImageViewCreateInfo viewInfo = vks::initializers::imageViewCreateInfo();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewInfo.format = imageInfo.format;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 6;
		viewInfo.image = depth_pass.attachment.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &depth_pass.attachment.view));

		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.subresourceRange.layerCount = 1;

		for(int i = 0;i < depth_pass.views.size();++i)
		{
			viewInfo.subresourceRange.baseArrayLayer = i;
			VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &depth_pass.views[i]));
		}
		//create depth sampler
		auto filterMode = vks::tools::formatIsFilterable(physicalDevice,imageInfo.format,imageInfo.tiling) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
		VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
		samplerInfo.magFilter = filterMode;
		samplerInfo.minFilter = filterMode;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 1.0f;
		samplerInfo.maxAnisotropy = 1.0f;
		VK_CHECK_RESULT(vkCreateSampler(device, &samplerInfo, nullptr, &depth_pass.sampler));

		depth_pass.descriptor.sampler = depth_pass.sampler;
		depth_pass.descriptor.imageView = depth_pass.attachment.view;
		depth_pass.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		auto depthImageInfo = vks::initializers::imageCreateInfo();
		depthImageInfo.format = VK_FORMAT_D24_UNORM_S8_UINT;
		depthImageInfo.extent.width = depth_pass.size;
		depthImageInfo.extent.height = depth_pass.size;
		depthImageInfo.extent.depth = 1;
		depthImageInfo.mipLevels = 1;
		depthImageInfo.arrayLayers = 1;
		depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depthImageInfo.flags = 0;
		depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
		VK_CHECK_RESULT(vkCreateImage(device, &depthImageInfo, nullptr, &depth_pass.depthAttachment.image));

		vkGetImageMemoryRequirements(device, depth_pass.depthAttachment.image, &memoryRequirements);
		memInfo.allocationSize = memoryRequirements.size;
		memInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memInfo, nullptr, &depth_pass.depthAttachment.memory));
		VK_CHECK_RESULT(vkBindImageMemory(device, depth_pass.depthAttachment.image, depth_pass.depthAttachment.memory, 0));

		viewInfo = vks::initializers::imageViewCreateInfo();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = depthImageInfo.format;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.image = depth_pass.depthAttachment.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &depth_pass.depthAttachment.view));

		prepareDepthRenderPass();

		std::array<VkImageView ,2> views = {depth_pass.attachment.view,depth_pass.depthAttachment.view};
		//create frameBuffer
		VkFramebufferCreateInfo framebufferInfo = vks::initializers::framebufferCreateInfo();
		framebufferInfo.renderPass = depth_pass.renderPass;
		framebufferInfo.attachmentCount = views.size();
		framebufferInfo.pAttachments = views.data();
		framebufferInfo.width = depth_pass.size;
		framebufferInfo.height = depth_pass.size;
		framebufferInfo.layers = 1;
		for(int i = 0;i < depth_pass.views.size();++i)
		{
			views[0] = depth_pass.views[i];
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &depth_pass.frameBuffers[i]));
		}

	}

	void prepare() override
	{
		VulkanExampleBase::prepare();

		prepareDepthPassAttachment();
		loadAssets();
		prepareUniformBuffers();
		setupDescriptorPool();
		setupDescriptorSetLayouts();
		setupDescriptorSet();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	void updateSpotLightUniformBuffer(bool b)
	{
		if(b)
		{
			spotLight.data.direct = glm::normalize(-uboVP.lightPos);
		}
		memcpy(spotLight.buffer.mapped,&spotLight.data,sizeof(SpotLightData));
	}
	void render() override {
		if(prepared)
		{
			renderFrame();
			OnUpdateLightPos();
			time += timerSpeed * frameTimer;
		}
	}
	void OnUpdateLightPos()
	{
		updateUniformBuffer(true);
		updateSpotLightUniformBuffer(true);
		buildCommandBuffers();
	}
	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
	{
		if(overlay->button("+x"))
		{
			uboVP.lightPos.x += 0.1f;
			OnUpdateLightPos();
		}
		if(overlay->button("-x"))
		{
			uboVP.lightPos.x -= 0.1f;
			OnUpdateLightPos();
		}
		if(overlay->button("+y"))
		{
			uboVP.lightPos.y += 0.1f;
			OnUpdateLightPos();
		}
		if(overlay->button("-y"))
		{
			uboVP.lightPos.y -= 0.1f;
			OnUpdateLightPos();
		}
		if(overlay->button("+z"))
		{
			uboVP.lightPos.z += 0.1f;
			OnUpdateLightPos();
		}
		if(overlay->button("-z"))
		{
			uboVP.lightPos.z -= 0.1f;
			OnUpdateLightPos();
		}
		if (overlay->header("Statistics")) {
			char buf[100] = {0};
			sprintf(buf, "lightPos: %f, %f, %f",uboVP.lightPos.x,uboVP.lightPos.y,uboVP.lightPos.z);
			overlay->text(buf);
		}
	}


 private:
	float time = 0.0f;
	struct {
		glm::mat4 projection;
		glm::mat4 view;
		glm::mat4 model;
		glm::mat4 lightSpace;
		glm::vec4 viewPos;
		glm::vec4 lightPos = glm::vec4(0.1f, -3.7f, 0.0f, 1.0f);
	} uboVP;
	struct {
		DepthUBO _1;
		char space[48];
		DepthUBO _2;
	} uboDepth;
	struct {
		glm::mat4 plane = glm::mat4(1.0f);
		glm::mat4 cube = glm::mat4(1.f);
	} sceneData;
	struct {
		vks::Buffer plane;
		vks::Buffer cube;
		vks::Buffer depth;
	} uniformData;
	struct {
		VkPipeline base;
		VkPipeline depth;
		VkPipeline cube;
	} pipelines;
	struct {
		VkDescriptorSet plane;
		VkDescriptorSet cube;
		VkDescriptorSet planeForDepth;
		VkDescriptorSet cubeForDepth;
	} descriptorSets;
	struct {

	} textures;
	struct {
		vkglTF::Model plane;
		vkglTF::Model cube;
	}scene;
	struct FrameBufferAttachment{
		VkImage image;
		VkDeviceMemory memory;
		VkImageView view;
	};
	struct {
		int size = 2048;
		std::array<VkFramebuffer,6> frameBuffers;
		FrameBufferAttachment attachment;
		FrameBufferAttachment depthAttachment;
		VkRenderPass renderPass;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
		std::array<VkImageView,6> views;
		glm::mat4 perspective = glm::perspective(glm::pi<float>() * 0.5f,1.0f, 0.1f, 220.0f);
	} depth_pass;
	struct {
		VkDescriptorSetLayout base;
		VkDescriptorSetLayout depth;
	}  descriptorSetLayouts;
	struct
	{
		VkPipelineLayout base;
		VkPipelineLayout depth;
	} pipelineLayouts;
	struct {
		SpotLightData data;
		bool updated;
		vks::Buffer buffer;
	} spotLight;
};

VULKAN_EXAMPLE_MAIN()