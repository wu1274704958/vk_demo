//
// Created by A on 2023/4/30.
//
#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#define INSTANCE_COUNT 2048
#define VERTEX_BUFFER_BIND_ID 0
#define INSTANCE_BUFFER_BIND_ID 1
#define RADIUS 25.0f
class VulkanExample : public VulkanExampleBase{
public:
    VulkanExample() : VulkanExampleBase(true)
    {
        title = "Geometry Shader";
        camera.type = Camera::CameraType::lookat;
        camera.setPerspective(60.f,(float )width / (float) height,0.01,1024.0f);
        camera.setRotation(glm::vec3(-90.0f, 0.0f, 0.0f));
        camera.setTranslation(glm::vec3(0.0f, 0.0f, -5.0f));
        camera.movementSpeed = 5.0f;
    }
    ~VulkanExample()
    {
		textures.diffuse_map.destroy();
		textures.normal_map.destroy();
		uniformData.scene.destroy();
        //clear buffers
		vkDestroyPipeline(device,pipelines.normal_map, nullptr);
		vkDestroyPipeline(device,pipelines.wireframe, nullptr);
        //clear pipeline layout
        vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
        //clear descriptor pool
        vkDestroyDescriptorSetLayout(device,descriptorSetLayout, nullptr);
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
		scene.model.loadFromFile(getAssetPath() + "models/plane.gltf", vulkanDevice, queue,vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors);
    	textures.normal_map.loadFromFile(getAssetPath() + "textures/stonefloor01_normal_rgba.ktx",VK_FORMAT_R8G8B8A8_UNORM,vulkanDevice,queue);
		textures.diffuse_map.loadFromFile(getAssetPath() + "textures/stonefloor01_color_rgba.ktx",VK_FORMAT_R8G8B8A8_UNORM,vulkanDevice,queue);
	}

    void updateUniformBuffer(bool viewChanged) {
        if(viewChanged)
        {
            uboVP.projection = camera.matrices.perspective;
            uboVP.view = camera.matrices.view;
        }
        memcpy(uniformData.scene.mapped,&uboVP,sizeof(uboVP));
    }

    void prepareUniformBuffers()
    {
        VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                   &uniformData.scene,sizeof(uboVP)));
        VK_CHECK_RESULT(uniformData.scene.map());
        updateUniformBuffer(true);
    }

    void setupDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes = {
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
			VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 1);
        VK_CHECK_RESULT(vkCreateDescriptorPool(device,&descriptorPoolInfo,nullptr,&descriptorPool));
    }

    void setupDescriptorSetLayouts() {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),
        };
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo = vks::initializers::descriptorSetLayoutCreateInfo(bindings.data(), bindings.size());
        VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutInfo, nullptr, &descriptorSetLayout));

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
        VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout));
    }

    void setupDescriptorSet()
    {
        VkDescriptorSetAllocateInfo allocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
        VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

        std::array<VkWriteDescriptorSet, 3> descriptorWrites = {
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformData.scene.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.normal_map.descriptor),
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.diffuse_map.descriptor),
        };

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    void preparePipelines()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineColorBlendAttachmentState colorBlendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf,VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = vks::initializers::pipelineColorBlendStateCreateInfo(1,&colorBlendAttachmentState);
        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_LINE,VK_CULL_MODE_NONE,VK_FRONT_FACE_COUNTER_CLOCKWISE);
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE,VK_TRUE,VK_COMPARE_OP_LESS_OR_EQUAL);

        std::array<VkDynamicState,3> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR,VK_DYNAMIC_STATE_LINE_WIDTH};
        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStates.data(),dynamicStates.size());
        VkPipelineViewportStateCreateInfo viewportStateCreateInfo = vks::initializers::pipelineViewportStateCreateInfo(1,1);
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {};

        VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo(pipelineLayout, renderPass);
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
        pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
        pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
        pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
        pipelineCreateInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::Normal, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Tangent });
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pStages = shaderStages.data();
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        // pipeline for indirect(and instance) draw
        shaderStages[0] = loadShader(getShadersPath() + "normal_texture/base.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "normal_texture/base.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);


        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.wireframe));
		shaderStages[0] = loadShader(getShadersPath() + "normal_texture/mesh.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "normal_texture/mesh.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        // pipeline for normal_map
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.normal_map));
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
        VkDeviceSize offset = 0;
        for(int i = 0;i < this->drawCmdBuffers.size();++i)
        {
			renderPassBeginInfo.framebuffer = frameBuffers[i];
			VK_CHECK_RESULT(vkBeginCommandBuffer(this->drawCmdBuffers[i], &beginInfo));
			vkCmdBeginRenderPass(this->drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport(width, height, 0.0f, 1.0f);
			vkCmdSetViewport(this->drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
			vkCmdSetScissor(this->drawCmdBuffers[i], 0, 1, &scissor);

			vkCmdBindDescriptorSets(drawCmdBuffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				pipelineLayout,
				0,
				1,
				&descriptorSet,
				0,
				nullptr);

			vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &scene.model.vertices.buffer, &offset);
			vkCmdBindIndexBuffer(drawCmdBuffers[i], scene.model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.normal_map);
			scene.model.draw(drawCmdBuffers[i]);
			if (wireframe)
			{
				vkCmdSetLineWidth(drawCmdBuffers[i], lineWidth);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireframe);
				scene.model.draw(drawCmdBuffers[i]);
			}
			drawUI(drawCmdBuffers[i]);
			vkCmdEndRenderPass(drawCmdBuffers[i]);
			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
        }
    }

    void prepare() override
    {
        VulkanExampleBase::prepare();
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
        if (overlay->header("Statistics")) {
			auto old = wireframe;
			overlay->checkBox("Wireframe", &wireframe);
			if(old != wireframe && wireframe)
				buildCommandBuffers();
			if(wireframe && enabledFeatures.wideLines)
			{
				auto old = lineWidth;
				if(overlay->sliderFloat("LineWidth",&lineWidth,1.0f,8.0f))
				{
					if(glm::abs( old - lineWidth) > 0.01f)
						buildCommandBuffers();
				}
			}
        }
    }


private:

	bool wireframe = false;
    struct {
        glm::mat4 projection;
        glm::mat4 view;
    } uboVP;
    struct {
        vks::Buffer scene;
    } uniformData;
    struct {
        VkPipeline normal_map;
        VkPipeline wireframe;
    } pipelines;
	struct {
		vks::Texture2D normal_map;
		vks::Texture2D diffuse_map;
	} textures;
	struct {
		vkglTF::Model model;
	}scene;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;
    float lineWidth = 1.0f;
};

VULKAN_EXAMPLE_MAIN()