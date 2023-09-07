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
#define DEPTH_FORMAT VK_FORMAT_D16_UNORM
// Depth bias (and slope) are used to avoid shadowing artifacts
// Constant depth bias factor (always applied)
float depthBiasConstant = 1.25f;
// Slope depth bias factor, applied depending on polygon's slope
float depthBiasSlope = 1.75f;
class VulkanExample : public VulkanExampleBase{
public:
    VulkanExample() : VulkanExampleBase(true)
    {
        title = "Shadow Mapping";
        camera.type = Camera::CameraType::firstperson;
        camera.setPerspective(60.f,(float )width / (float) height,0.01,1024.0f);
		camera.setPosition(glm::vec3(0.0f, 1.25f, -1.5f));
		camera.setRotation(glm::vec3(-45.0f, 0.0f, 0.0f));
		camera.movementSpeed = 50.0f;

		sceneData.plane = glm::translate(sceneData.cube, glm::vec3(0.0f, 2.0f, 0.0f));
		sceneData.plane = glm::scale(sceneData.plane,glm::vec3(20.0f,1.0f,20.0f));
    }
    ~VulkanExample()
    {
		//destroy depth resource
		vkDestroySampler(device,depth_pass.sampler,nullptr);
		vkDestroyImageView(device,depth_pass.attachment.view,nullptr);
		vkDestroyImage(device,depth_pass.attachment.image,nullptr);
		vkFreeMemory(device,depth_pass.attachment.memory,nullptr);
		vkDestroyFramebuffer(device,depth_pass.frameBuffer,nullptr);
		vkDestroyRenderPass(device,depth_pass.renderPass,nullptr);


		uniformData.plane.destroy();
		uniformData.cube.destroy();
		uniformData.depth.destroy();
        //clear buffers
		vkDestroyPipeline(device,pipelines.base, nullptr);
		vkDestroyPipeline(device,pipelines.depth, nullptr);
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
		scene.plane.loadFromFile(getAssetPath() + "models/plane.gltf", vulkanDevice, queue,vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors);
		scene.cube.loadFromFile(getAssetPath() + "models/vulkanscene_shadow.gltf", vulkanDevice, queue, vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors);
	}
	void updateLight()
	{
		// Animate the light source
		uboVP.lightPos.x = cos(glm::radians(timer * 360.0f)) * 40.0f;
		uboVP.lightPos.y = -50.0f + sin(glm::radians(timer * 360.0f)) * 20.0f ;
		uboVP.lightPos.z = 25.0f + sin(glm::radians(timer * 360.0f )) * 5.0f ;
	}

	void updateDepthUniformBuffer(bool changed)
	{

		glm::mat4 ortho = glm::ortho( -1.f * ORTHO_SIZE, 1.f * ORTHO_SIZE, 1.f * ORTHO_SIZE, -1.f * ORTHO_SIZE, 1.f, 100.0f);
		glm::mat4 view = glm::lookAt(glm::vec3(uboVP.lightPos), glm::vec3(0.0f,0.0f,0.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		uboDepth.mat1 = ortho * view * sceneData.plane;
		uboDepth.mat2 = ortho * view * sceneData.cube;
		memcpy(uniformData.depth.mapped,&uboDepth,sizeof(uboDepth));

		uboVP.lightSpace = ortho * view;
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
    }

    void setupDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes = {
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4},
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 8);
        VK_CHECK_RESULT(vkCreateDescriptorPool(device,&descriptorPoolInfo,nullptr,&descriptorPool));
    }

    void setupDescriptorSetLayouts() {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
        };
		std::array<VkDescriptorSetLayoutBinding, 1> depthBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
		};
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(bindings.data(), bindings.size());
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayouts.base));

		VkDescriptorSetLayoutCreateInfo depthDescriptorSetLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(depthBindings.data(), depthBindings.size());
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &depthDescriptorSetLayoutInfo, nullptr, &descriptorSetLayouts.depth));

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.base, 1);
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayouts.base));

		VkPipelineLayoutCreateInfo depthPipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayouts.depth, 1);
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

        std::array<VkWriteDescriptorSet, 2> descriptorWrites = {
            vks::initializers::writeDescriptorSet(descriptorSets.plane, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformData.plane.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSets.plane, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &depth_pass.descriptor)
        };

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		descriptorWrites[0].dstSet = descriptorSets.cube;
		descriptorWrites[0].pBufferInfo = &uniformData.cube.descriptor;
		descriptorWrites[1].dstSet = descriptorSets.cube;
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
		// mat1 plane mat2 cube
		VkDescriptorBufferInfo depthDescriptorBufferInfo = { .buffer = uniformData.depth.buffer, .offset = 0, .range = sizeof(glm::mat4) };
		std::array<VkWriteDescriptorSet, 1> depthDescriptorWrites = {
			vks::initializers::writeDescriptorSet(descriptorSets.planeForDepth, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &depthDescriptorBufferInfo)
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(depthDescriptorWrites.size()), depthDescriptorWrites.data(), 0, nullptr);
		depthDescriptorBufferInfo = { .buffer = uniformData.depth.buffer, .offset = sizeof(glm::mat4), .range = sizeof(glm::mat4) };
		depthDescriptorWrites[0].dstSet = descriptorSets.cubeForDepth;
		vkUpdateDescriptorSets(device,static_cast<uint32_t>(depthDescriptorWrites.size()), depthDescriptorWrites.data(), 0, nullptr);
    }

    void preparePipelines()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineColorBlendAttachmentState colorBlendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf,VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = vks::initializers::pipelineColorBlendStateCreateInfo(1,&colorBlendAttachmentState);
        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL,VK_CULL_MODE_NONE,VK_FRONT_FACE_COUNTER_CLOCKWISE);
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
        shaderStages[0] = loadShader(getShadersPath() + "shadow_mapping_direct/base.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "shadow_mapping_direct/base.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);


        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.base));
		//depth pipeline
		shaderStages[0] = loadShader(getShadersPath() + "shadow_mapping_direct/shadowMap.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
		pipelineCreateInfo.layout = pipelineLayouts.depth;
		pipelineCreateInfo.stageCount = 1;
		colorBlendCreateInfo.attachmentCount = 0;
		dynamicStateCreateInfo.dynamicStateCount = dynamicStates.size();
		rasterizationStateCreateInfo.depthBiasEnable = true;
		pipelineCreateInfo.renderPass = depth_pass.renderPass;
		rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.depth));
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

		VkRenderPassBeginInfo depthPassBeginInfo = vks::initializers::renderPassBeginInfo();
		depthPassBeginInfo.renderPass = depth_pass.renderPass;
		depthPassBeginInfo.renderArea.extent.width = depth_pass.size;
		depthPassBeginInfo.renderArea.extent.height = depth_pass.size;
		depthPassBeginInfo.pClearValues = &clearValues[1];
		depthPassBeginInfo.clearValueCount = 1;
		depthPassBeginInfo.framebuffer = depth_pass.frameBuffer;

        VkDeviceSize offset = 0;
        for(int i = 0;i < this->drawCmdBuffers.size();++i)
        {
			renderPassBeginInfo.framebuffer = frameBuffers[i];
			VK_CHECK_RESULT(vkBeginCommandBuffer(this->drawCmdBuffers[i], &beginInfo));
			//depth pass
			vkCmdBeginRenderPass(this->drawCmdBuffers[i],&depthPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport(depth_pass.size, depth_pass.size, 0.0f, 1.0f);
			vkCmdSetViewport(drawCmdBuffers[i],0,1,&viewport);

			VkRect2D scissor = vks::initializers::rect2D(depth_pass.size, depth_pass.size, 0, 0);
			vkCmdSetScissor(drawCmdBuffers[i],0,1,&scissor);

			vkCmdSetDepthBias(drawCmdBuffers[i],depthBiasConstant,0.0f,depthBiasSlope);

			vkCmdBindPipeline(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines.depth);

			vkCmdBindDescriptorSets(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayouts.depth,0,1,&descriptorSets.planeForDepth,0,nullptr);
			scene.plane.draw(drawCmdBuffers[i]);

			vkCmdBindDescriptorSets(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayouts.depth,0,1,&descriptorSets.cubeForDepth,0,nullptr);
			scene.cube.draw(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);

			//draw pass --------------------------------------------------------------------
			vkCmdBeginRenderPass(this->drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			viewport = vks::initializers::viewport(width, height, 0.0f, 1.0f);
			vkCmdSetViewport(this->drawCmdBuffers[i], 0, 1, &viewport);

			scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(this->drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.base);

			vkCmdBindDescriptorSets(drawCmdBuffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayouts.base,
				0,
				1,
				&descriptorSets.plane,
				0,
				nullptr);

			scene.plane.draw(drawCmdBuffers[i]);

			vkCmdBindDescriptorSets(drawCmdBuffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayouts.base,
				0,
				1,
				&descriptorSets.cube,
				0,
				nullptr);

			scene.cube.draw(drawCmdBuffers[i]);

			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
        }
    }
	void prepareDepthRenderPass()
	{
		//create depth Render pass
		auto attachmentDesctiptor = VkAttachmentDescription{};
		attachmentDesctiptor.format = DEPTH_FORMAT;
		attachmentDesctiptor.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDesctiptor.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDesctiptor.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDesctiptor.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDesctiptor.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDesctiptor.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDesctiptor.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		//sub pass description
		VkAttachmentReference depth_attachment_ref = { 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		VkSubpassDescription pass{};
		pass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		pass.colorAttachmentCount = 0;
		pass.pColorAttachments = nullptr;
		pass.pDepthStencilAttachment = &depth_attachment_ref;

		VkSubpassDependency depth_pass_dependency[2] = { {},{}};
		depth_pass_dependency[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		depth_pass_dependency[0].dstSubpass = 0;
		depth_pass_dependency[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		depth_pass_dependency[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		depth_pass_dependency[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		depth_pass_dependency[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		depth_pass_dependency[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		depth_pass_dependency[1].srcSubpass = 0;
		depth_pass_dependency[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		depth_pass_dependency[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		depth_pass_dependency[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		depth_pass_dependency[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		depth_pass_dependency[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		depth_pass_dependency[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


		auto renderPassInfo = vks::initializers::renderPassCreateInfo();
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &attachmentDesctiptor;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &pass;
		renderPassInfo.dependencyCount = sizeof(depth_pass_dependency) / sizeof(depth_pass_dependency[0]);
		renderPassInfo.pDependencies = depth_pass_dependency;

		VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &depth_pass.renderPass));
	}
	void prepareDepthPassAttachment()
	{
		//create Image
		auto imageInfo = vks::initializers::imageCreateInfo();
		imageInfo.arrayLayers = 1;
		imageInfo.extent.width = depth_pass.size;
		imageInfo.extent.height = depth_pass.size;
		imageInfo.extent.depth = 1;
		imageInfo.format = DEPTH_FORMAT;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.mipLevels = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		VK_CHECK_RESULT(vkCreateImage(device, &imageInfo, nullptr, &depth_pass.attachment.image));
		//allocation Image Memory

		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(device, depth_pass.attachment.image, &memoryRequirements);
		auto memInfo = vks::initializers::memoryAllocateInfo();
		memInfo.allocationSize = memoryRequirements.size;
		memInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memoryRequirements.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memInfo, nullptr, &depth_pass.attachment.memory));
		VK_CHECK_RESULT(vkBindImageMemory(device, depth_pass.attachment.image, depth_pass.attachment.memory, 0));

		//create view
		VkImageViewCreateInfo viewInfo = vks::initializers::imageViewCreateInfo();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = imageInfo.format;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.image = depth_pass.attachment.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &depth_pass.attachment.view));
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
		depth_pass.descriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		prepareDepthRenderPass();
		//create frameBuffer
		VkFramebufferCreateInfo framebufferInfo = vks::initializers::framebufferCreateInfo();
		framebufferInfo.renderPass = depth_pass.renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &depth_pass.attachment.view;
		framebufferInfo.width = depth_pass.size;
		framebufferInfo.height = depth_pass.size;
		framebufferInfo.layers = 1;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &depth_pass.frameBuffer));


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

    void render() override {
        if(prepared)
        {
            renderFrame();
            if(camera.updated)
            {
                updateUniformBuffer(true);
            }
        }
    }
    virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay) override
    {
		if(overlay->button("+x"))
		{
			uboVP.lightPos.x += 0.1f;
			updateDepthUniformBuffer(true);
		}
		if(overlay->button("-x"))
		{
			uboVP.lightPos.x -= 0.1f;
			updateDepthUniformBuffer(true);
		}
		if(overlay->button("+y"))
		{
			uboVP.lightPos.y += 0.1f;
			updateDepthUniformBuffer(true);
		}
		if(overlay->button("-y"))
		{
			uboVP.lightPos.y -= 0.1f;
			updateDepthUniformBuffer(true);
		}
		if(overlay->button("+z"))
		{
			uboVP.lightPos.z += 0.1f;
			updateDepthUniformBuffer(true);
		}
		if(overlay->button("-z"))
		{
			uboVP.lightPos.z -= 0.1f;
			updateDepthUniformBuffer(true);
		}
        if (overlay->header("Statistics")) {
			char buf[100] = {0};
			sprintf(buf, "lightPos: %f, %f, %f",uboVP.lightPos.x,uboVP.lightPos.y,uboVP.lightPos.z);
			overlay->text(buf);
        }
    }


private:

    struct {
        glm::mat4 projection;
        glm::mat4 view;
		glm::mat4 model;
		glm::mat4 lightSpace;
		glm::vec4 viewPos;
		glm::vec4 lightPos = glm::vec4(0.0f, -10.0f, 0.0f, 1.0f);
    } uboVP;
	struct {
		glm::mat4 mat1;
		glm::mat4 mat2;
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
		VkFramebuffer frameBuffer;
		FrameBufferAttachment attachment;
		VkRenderPass renderPass;
		VkSampler sampler;
		VkDescriptorImageInfo descriptor;
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

};

VULKAN_EXAMPLE_MAIN()