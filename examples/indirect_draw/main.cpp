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
        title = "Indirect Draw";
        camera.type = Camera::CameraType::firstperson;
        camera.setPerspective(60.f,(float )width / (float) height,0.01,1024.0f);
        camera.setRotation(glm::vec3(-12.0f, 159.0f, 0.0f));
        camera.setTranslation(glm::vec3(0.4f, 1.25f, 0.0f));
        camera.movementSpeed = 5.0f;
    }
    ~VulkanExample()
    {
        //clear buffers
        indirectDrawCmdBuffer.destroy();
        instanceDataBuffer.destroy();
        uniformData.scene.destroy();
        textures.ground.destroy();
        textures.plants.destroy();
        if(pipelines.plantsWireframe != VK_NULL_HANDLE)
            vkDestroyPipeline(device,pipelines.plantsWireframe,nullptr);
        //clear pipelines
        vkDestroyPipeline(device,pipelines.ground,nullptr);
        vkDestroyPipeline(device,pipelines.skysphere,nullptr);
        vkDestroyPipeline(device,pipelines.plants,nullptr);
        //clear pipeline layout
        vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
        //clear descriptor pool
        vkDestroyDescriptorSetLayout(device,descriptorSetLayout, nullptr);
    }
    void getEnabledFeatures() override {
        // Example uses multi draw indirect if available
        if (deviceFeatures.multiDrawIndirect) {
            enabledFeatures.multiDrawIndirect = VK_TRUE;
        }
        // Enable anisotropic filtering if supported
        if (deviceFeatures.samplerAnisotropy) {
            enabledFeatures.samplerAnisotropy = VK_TRUE;
        }
        if(deviceFeatures.wideLines)
            enabledFeatures.wideLines = VK_TRUE;
        if(deviceFeatures.fillModeNonSolid)
            enabledFeatures.fillModeNonSolid = VK_TRUE;
    }

    void loadAssets()
    {
        const uint32_t glTFLoadingFlags = vkglTF::FileLoadingFlags::PreTransformVertices | vkglTF::FileLoadingFlags::PreMultiplyVertexColors | vkglTF::FileLoadingFlags::FlipY;
        models.plants.loadFromFile(getAssetPath() + "models/plants.gltf", vulkanDevice, queue, glTFLoadingFlags);
        models.ground.loadFromFile(getAssetPath() + "models/plane_circle.gltf", vulkanDevice, queue, glTFLoadingFlags);
        models.skysphere.loadFromFile(getAssetPath() + "models/sphere.gltf", vulkanDevice, queue, glTFLoadingFlags);
        textures.plants.loadFromFile(getAssetPath() + "textures/texturearray_plants_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);
        textures.ground.loadFromFile(getAssetPath() + "textures/ground_dry_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, queue);

    }

    void prepareIndirectDrawData() {
        int m = 0;
        for(const auto& node : models.plants.nodes) {
            if(node->mesh) {
                VkDrawIndexedIndirectCommand drawIndirectCmd = {};
                drawIndirectCmd.indexCount = node->mesh->primitives[0]->indexCount;
                drawIndirectCmd.instanceCount = INSTANCE_COUNT;
                drawIndirectCmd.firstIndex = node->mesh->primitives[0]->firstIndex;
                drawIndirectCmd.firstInstance = m * INSTANCE_COUNT;
                indirectDrawCommands.push_back(drawIndirectCmd);
                ++m;
            }
        }
        vks::Buffer stagingBuffer;
        VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                   &stagingBuffer, sizeof(VkDrawIndexedIndirectCommand) * indirectDrawCommands.size(), indirectDrawCommands.data()));

        VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &indirectDrawCmdBuffer,
                                                   stagingBuffer.size));
        vulkanDevice->copyBuffer(&stagingBuffer,&indirectDrawCmdBuffer,queue);
        stagingBuffer.destroy();

    }

    void prepareInstanceData() {
        std::vector<InstanceData> instanceData(indirectDrawCommands.size() * INSTANCE_COUNT);
        std::default_random_engine rndEngine(benchmark.active ? 0 : time(nullptr));
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        int i = 0;
        for(auto& d : instanceData){
            float theta = 2.0f * glm::pi<float>() * dist(rndEngine);//1~2pi
            float phi = acos(1.f - 2.f * dist(rndEngine)); // acos(-1 ~ 1)
            d.rot = glm::vec3(0.f, dist(rndEngine) * glm::pi<float>(), 0.f);
            d.pos = glm::vec3 (glm::sin(phi) * glm::cos(theta),0.0f, glm::cos(phi)) * RADIUS;
            d.scale = 1.0f + dist(rndEngine) * 2.0f;
            d.texIndex = i / INSTANCE_COUNT;
            ++i;
        }
        vks::Buffer stagingBuffer;
        VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                                   &stagingBuffer, sizeof(InstanceData) * instanceData.size(), instanceData.data()));
        VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &instanceDataBuffer,stagingBuffer.size));
        vulkanDevice->copyBuffer(&stagingBuffer,&instanceDataBuffer,queue);
        stagingBuffer.destroy();
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
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2}
        };
        VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 2);
        VK_CHECK_RESULT(vkCreateDescriptorPool(device,&descriptorPoolInfo,nullptr,&descriptorPool));
    }

    void setupDescriptorSetLayouts() {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),
            vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2)
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
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &textures.plants.descriptor),
            vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &textures.ground.descriptor)
        };

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    void preparePipelines()
    {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
        VkPipelineColorBlendAttachmentState colorBlendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf,VK_FALSE);
        VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = vks::initializers::pipelineColorBlendStateCreateInfo(1,&colorBlendAttachmentState);
        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);
        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL,VK_CULL_MODE_NONE,VK_FRONT_FACE_COUNTER_CLOCKWISE);
        VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE,VK_TRUE,VK_COMPARE_OP_LESS_OR_EQUAL);
        const std::vector<VkVertexInputAttributeDescription > vertexInputAttributeDescriptions = {
            vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID,0,VK_FORMAT_R32G32B32_SFLOAT,0),
            vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID,1,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 3),
            vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID,2,VK_FORMAT_R32G32_SFLOAT,sizeof(float) * 6),
            vks::initializers::vertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID,3,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float) * 8),

            vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID,4,VK_FORMAT_R32G32B32_SFLOAT,offsetof(InstanceData,pos)),
            vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID,5,VK_FORMAT_R32G32B32_SFLOAT,offsetof(InstanceData,rot)),
            vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID,6,VK_FORMAT_R32_SFLOAT,offsetof(InstanceData,scale)),
            vks::initializers::vertexInputAttributeDescription(INSTANCE_BUFFER_BIND_ID,7,VK_FORMAT_R32_SINT,offsetof(InstanceData,texIndex)),
        };
        const std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            vks::initializers::vertexInputBindingDescription(VERTEX_BUFFER_BIND_ID,sizeof(vkglTF::Vertex),VK_VERTEX_INPUT_RATE_VERTEX),
            vks::initializers::vertexInputBindingDescription(INSTANCE_BUFFER_BIND_ID,sizeof(InstanceData),VK_VERTEX_INPUT_RATE_INSTANCE)
        };
        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = vks::initializers::pipelineVertexInputStateCreateInfo(vertexInputBindings,vertexInputAttributeDescriptions);
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
        pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
        pipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pStages = shaderStages.data();
        pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        // pipeline for indirect(and instance) draw
        shaderStages[0] = loadShader(getShadersPath() + "indirectdraw/indirectdraw.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "indirectdraw/indirectdraw.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.plants));
        if(enabledFeatures.fillModeNonSolid)
        {
            shaderStages[1] = loadShader(getShadersPath() + "indirectdraw/indirectdraw_white.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
            rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_LINE;
            VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.plantsWireframe));
            rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
        }
        vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 4;
        // pipeline for ground
        shaderStages[0] = loadShader(getShadersPath() + "indirectdraw/ground.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "indirectdraw/ground.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT);
        rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.ground));
        // pipeline for skybox
        rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
        depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
        shaderStages[0] = loadShader(getShadersPath() + "indirectdraw/skysphere.vert.spv",VK_SHADER_STAGE_VERTEX_BIT);
        shaderStages[1] = loadShader(getShadersPath() + "indirectdraw/skysphere.frag.spv",VK_SHADER_STAGE_FRAGMENT_BIT);
        VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache,1,&pipelineCreateInfo,nullptr,&pipelines.skysphere));
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
            VK_CHECK_RESULT(vkBeginCommandBuffer(this->drawCmdBuffers[i],&beginInfo));
            vkCmdBeginRenderPass(this->drawCmdBuffers[i],&renderPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport = vks::initializers::viewport(width, height,0.0f,1.0f);
            vkCmdSetViewport(this->drawCmdBuffers[i],0,1,&viewport);

            VkRect2D scissor = vks::initializers::rect2D(width,height,0,0);
            vkCmdSetScissor(this->drawCmdBuffers[i],0,1,&scissor);

            vkCmdBindDescriptorSets(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,1,&descriptorSet,0,nullptr);
            vkCmdBindPipeline(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines.skysphere);
            models.skysphere.draw(drawCmdBuffers[i]);

            if(!wireframe)vkCmdBindPipeline(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines.ground);
            models.ground.draw(drawCmdBuffers[i]);
            if(pipelines.plantsWireframe != VK_NULL_HANDLE)
                vkCmdBindPipeline(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,wireframe ? pipelines.plantsWireframe : pipelines.plants);
            else
                vkCmdBindPipeline(drawCmdBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines.plants);
            if(wireframe)
                vkCmdSetLineWidth(drawCmdBuffers[i],lineWidth);
            vkCmdBindVertexBuffers(drawCmdBuffers[i],VERTEX_BUFFER_BIND_ID,1,&models.plants.vertices.buffer,&offset);
            vkCmdBindVertexBuffers(drawCmdBuffers[i],INSTANCE_BUFFER_BIND_ID,1,&instanceDataBuffer.buffer,&offset);
            vkCmdBindIndexBuffer(drawCmdBuffers[i],models.plants.indices.buffer,0,VK_INDEX_TYPE_UINT32);
            if(vulkanDevice->features.multiDrawIndirect)
            {
                vkCmdDrawIndexedIndirect(drawCmdBuffers[i],indirectDrawCmdBuffer.buffer,0,static_cast<uint32_t>(indirectDrawCommands.size()),sizeof(VkDrawIndexedIndirectCommand));
            }else{
                for(int j = 0; j < indirectDrawCommands.size(); ++j)
                {
                    vkCmdDrawIndexedIndirect(drawCmdBuffers[i],indirectDrawCmdBuffer.buffer,j * sizeof(VkDrawIndexedIndirectCommand),1,sizeof(VkDrawIndexedIndirectCommand));
                }
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
        prepareIndirectDrawData();
        prepareInstanceData();
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
        if (!vulkanDevice->features.multiDrawIndirect) {
            if (overlay->header("Info")) {
                overlay->text("multiDrawIndirect not supported");
            }
        }
        if (overlay->header("Statistics")) {
            overlay->text("Objects: %d", indirectDrawCommands.size() * INSTANCE_COUNT);
            if(enabledFeatures.fillModeNonSolid)
            {
                auto old = wireframe;
                if(overlay->checkBox("Wireframe",&wireframe))
                {
                    if(old != wireframe)
                        buildCommandBuffers();
                }
            }
            if(enabledFeatures.wideLines)
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
    struct {
        vks::Texture2DArray plants;
        vks::Texture2D ground;
    } textures;

    struct {
        vkglTF::Model plants;
        vkglTF::Model ground;
        vkglTF::Model skysphere;
    } models;
    struct InstanceData {
        glm::vec3 pos;
        glm::vec3 rot;
        float scale;
        uint32_t texIndex;
    };
    struct {
        glm::mat4 projection;
        glm::mat4 view;
    } uboVP;
    struct {
        vks::Buffer scene;
    } uniformData;
    struct {
        VkPipeline plants,plantsWireframe = VK_NULL_HANDLE;
        VkPipeline ground;
        VkPipeline skysphere;
    } pipelines;
    std::vector<VkDrawIndexedIndirectCommand> indirectDrawCommands;
    vks::Buffer indirectDrawCmdBuffer;
    vks::Buffer instanceDataBuffer;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet descriptorSet;
    bool wireframe = false;
    float lineWidth = 1.0f;
};

VULKAN_EXAMPLE_MAIN()