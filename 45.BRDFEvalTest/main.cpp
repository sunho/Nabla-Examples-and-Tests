// Copyright (C) 2018-2022 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#define _NBL_STATIC_LIB_
#include <cstdio>
#include <iostream>
#include <nabla.h>

#include "../common/Camera.hpp"
#include "../common/CommonAPI.h"

using namespace nbl;
using namespace asset;
using namespace system;
using namespace ui;
using namespace video;
using namespace core;

/*
        Uncomment for more detailed logging
*/

// #define NBL_MORE_LOGS

class BRDFEvalTestApp : public ApplicationBase
{
        constexpr static uint32_t WIN_W = 1280;
        constexpr static uint32_t WIN_H = 720;
        constexpr static uint32_t SC_IMG_COUNT = 3u;
        constexpr static uint32_t FRAMES_IN_FLIGHT = 5u;

        static_assert(FRAMES_IN_FLIGHT > SC_IMG_COUNT);

    public:
        // move to common API ? 
        smart_refctd_ptr<IWindowManager> windowManager;
        smart_refctd_ptr<IWindow> window;
        smart_refctd_ptr<CommonAPI::CommonAPIEventCallback> windowCb;

        smart_refctd_ptr<IAPIConnection> apiConnection;
        smart_refctd_ptr<ISurface> surface;
        smart_refctd_ptr<IUtilities> utilities;
        smart_refctd_ptr<ILogicalDevice> logicalDevice;
        IPhysicalDevice *physicalDevice;
        std::array<IGPUQueue*,CommonAPI::InitOutput::MaxQueuesCount> queues = {nullptr, nullptr, nullptr, nullptr};

        smart_refctd_ptr<ISwapchain> swapchain;
        smart_refctd_ptr<IGPURenderpass> renderpass;
        smart_refctd_dynamic_array<smart_refctd_ptr<IGPUFramebuffer>> fbo;
        std::array<
            std::array<smart_refctd_ptr<IGPUCommandPool>,
                        CommonAPI::InitOutput::MaxFramesInFlight>,
            CommonAPI::InitOutput::MaxQueuesCount>
            commandPools; // TODO: Multibuffer and reset the commandpools
        smart_refctd_ptr<ISystem> system;
        smart_refctd_ptr<IAssetManager> assetManager;
        IGPUObjectFromAssetConverter::SParams cpu2gpuParams;
        smart_refctd_ptr<ILogger> logger;
        smart_refctd_ptr<CommonAPI::InputSystem> inputSystem;

        IGPUObjectFromAssetConverter cpu2gpu;

        core::smart_refctd_ptr<video::IGPUMeshBuffer> gpuMeshBuffer;
        core::smart_refctd_ptr<IGPURenderpassIndependentPipeline>
            gpuRenderpassIndependentPipeline;
        core::smart_refctd_ptr<IGPUBuffer> gpuubo;
        core::smart_refctd_ptr<IGPUDescriptorSet> gpuDescriptorSet1;
        core::smart_refctd_ptr<IGPUDescriptorSet> gpuDescriptorSet3;
        core::smart_refctd_ptr<IGPUGraphicsPipeline> gpuGraphicsPipeline;

        core::smart_refctd_ptr<video::IGPUFence> frameComplete[FRAMES_IN_FLIGHT] = {
            nullptr};
        core::smart_refctd_ptr<video::IGPUSemaphore> imageAcquire[FRAMES_IN_FLIGHT] =
            {nullptr};
        core::smart_refctd_ptr<video::IGPUSemaphore>
            renderFinished[FRAMES_IN_FLIGHT] = {nullptr};
        core::smart_refctd_ptr<video::IGPUCommandBuffer>
            commandBuffers[FRAMES_IN_FLIGHT];

        ISwapchain::SCreationParams m_swapchainCreationParams;

        CommonAPI::InputSystem::ChannelReader<ui::IMouseEventChannel> mouse;
        CommonAPI::InputSystem::ChannelReader<ui::IKeyboardEventChannel> keyboard;
        Camera camera;

        int resourceIx;
        uint32_t acquiredNextFBO = {};


        // new stuff
        enum BRDFTestNumber : uint32_t {
            TEST_GGX = 1,
            TEST_BECKMANN,
            TEST_PHONG,
            TEST_AS,
            TEST_OREN_NAYAR,
            TEST_LAMBERT,
        };

        BRDFTestNumber currentTestNum = TEST_GGX;

        struct SPushConsts {
        struct VertStage {
            core::matrix4SIMD VP;
        } vertStage;
        struct FragStage {
            core::vectorSIMDf campos;
            BRDFTestNumber testNum;
            uint32_t pad[3];
        } fragStage;
        };

        auto createDescriptorPool(const uint32_t textureCount) {
            constexpr uint32_t maxItemCount = 256u;
            {
                IDescriptorPool::SDescriptorPoolSize poolSize;
                poolSize.count = textureCount;
                poolSize.type = EDT_COMBINED_IMAGE_SAMPLER;
                return logicalDevice->createDescriptorPool(
                    static_cast<IDescriptorPool::E_CREATE_FLAGS>(0),
                    maxItemCount, 1u, &poolSize);
            }
        }

        void setSystem(core::smart_refctd_ptr<ISystem> &&s) override {
            system = std::move(s);
        }

        APP_CONSTRUCTOR(BRDFEvalTestApp)

        void onAppInitialized_impl() override
        {
            const auto swapchainImageUsage = static_cast<asset::IImage::E_USAGE_FLAGS>(asset::IImage::EUF_COLOR_ATTACHMENT_BIT);
            CommonAPI::InitParams initParams;
            initParams.window = core::smart_refctd_ptr(window);
            initParams.apiType = video::EAT_VULKAN;
            initParams.appName = {"45.BRDFEvalTest"};
            initParams.framesInFlight = FRAMES_IN_FLIGHT;
            initParams.windowWidth = WIN_W;
            initParams.windowHeight = WIN_H;
            initParams.swapchainImageCount = SC_IMG_COUNT;
            initParams.swapchainImageUsage = swapchainImageUsage;
            initParams.depthFormat = EF_D32_SFLOAT;
            auto initOutput = CommonAPI::InitWithDefaultExt(std::move(initParams));

            window = std::move(initParams.window);
            windowCb = std::move(initParams.windowCb);
            apiConnection = std::move(initOutput.apiConnection);
            surface = std::move(initOutput.surface);
            utilities = std::move(initOutput.utilities);
            logicalDevice = std::move(initOutput.logicalDevice);
            physicalDevice = initOutput.physicalDevice;
            queues = std::move(initOutput.queues);
            renderpass = std::move(initOutput.renderToSwapchainRenderpass);
            commandPools = std::move(initOutput.commandPools);
            system = std::move(initOutput.system);
            assetManager = std::move(initOutput.assetManager);
            cpu2gpuParams = std::move(initOutput.cpu2gpuParams);
            logger = std::move(initOutput.logger);
            inputSystem = std::move(initOutput.inputSystem);
            m_swapchainCreationParams = std::move(initOutput.swapchainCreationParams);

            CommonAPI::createSwapchain(std::move(logicalDevice),
                                        m_swapchainCreationParams, WIN_W, WIN_H,
                                        swapchain);
            assert(swapchain);

            fbo = CommonAPI::createFBOWithSwapchainImages(
                swapchain->getImageCount(), WIN_W, WIN_H, logicalDevice, swapchain,
                renderpass, EF_D32_SFLOAT);

            auto geometryCreator = assetManager->getGeometryCreator();
            constexpr uint32_t INSTANCE_COUNT = 10u;

            auto geometryObject = geometryCreator->createSphereMesh(0.5f, 50u, 50u);

            // TODO: util function => move CommonAPI
            auto assertAssetLoaded = [&](const SAssetBundle& bundle) -> void
            {
                if (bundle.getContents().empty())
                {
                    logger->log("Asset with key %s and type %u failed to load! Expect program termination!",ILogger::ELL_ERROR,bundle.getCacheKey().c_str(),bundle.getAssetType());
                    assert(false);
                }
            };
            auto convertAssetsToGPUObjects = [&]<typename AssetType>(const auto* ppAssets, const size_t count) -> created_gpu_object_array<AssetType>
            {
                auto gpu_array = cpu2gpu.getGPUObjectsFromAssets(ppAssets,ppAssets+count,cpu2gpuParams);
                if (!gpu_array || gpu_array->size()<1u)
                {
                    logger->log("Failed to convert CPU Assets to GPU Objects!",ILogger::ELL_ERROR);
                    return nullptr;
                }
                return gpu_array;
            };
            auto convertAssetToGPUObject = [&]<typename AssetType>(const auto& asset) -> core::smart_refctd_ptr<typename video::asset_traits<AssetType>::GPUObjectType>
            {
                auto gpu_array = convertAssetsToGPUObjects.operator()<AssetType>(&asset,&asset+1);
                if (!gpu_array)
                    return nullptr;
                auto gpuObject = gpu_array->operator[](0);
                if (!gpuObject)
                {
                    logger->log("Asset of type %u failed to load! Expect program termination!",ILogger::ELL_ERROR,asset->getAssetType()());
                    assert(false);
                    return nullptr;
                }
                return gpuObject;
            };
            auto firstBundleItemToGPUSpecShader = [&](const SAssetBundle& bundle) -> core::smart_refctd_ptr<IGPUSpecializedShader>
            {
                assertAssetLoaded(bundle);
                if (bundle.getAssetType()!=IAsset::ET_SPECIALIZED_SHADER)
                {
                    logger->log("Expected asset with key %s to be IAsset::ET_SHADER got type %u instead! Expect program termination!",ILogger::ELL_ERROR,bundle.getCacheKey().c_str(),bundle.getAssetType());
                    assert(false);
                    return nullptr;
                }
                else
                {
                    auto cpuSpec = core::smart_refctd_ptr_static_cast<ICPUSpecializedShader>(bundle.getContents().begin()[0]);
                    return convertAssetToGPUObject.operator()<ICPUSpecializedShader>(cpuSpec);
                }
            };

            // create pipeline and shaders
            {
                auto vertexShaderBundle = assetManager->getAsset("../shader.vert.hlsl", {});
                assertAssetLoaded(vertexShaderBundle);

                auto fragmentShaderBundle = assetManager->getAsset("../shader.frag.hlsl", {});
                assertAssetLoaded(fragmentShaderBundle);
            }


            auto gpuVertexShader = firstBundleItemToGPUSpecShader(assetManager->getAsset("../shader.vert", {}));
            auto gpuFragmentShader = firstBundleItemToGPUSpecShader(assetManager->getAsset("../shader.frag", {}));

            asset::SPushConstantRange rng[2];
            rng[0].offset = 0u;
            rng[0].size = sizeof(SPushConsts::vertStage);
            rng[0].stageFlags = asset::IShader::ESS_VERTEX;
            rng[1].offset = offsetof(SPushConsts, fragStage);
            rng[1].size = sizeof(SPushConsts::fragStage);
            rng[1].stageFlags = asset::IShader::ESS_FRAGMENT;

            auto gpuPipelineLayout = logicalDevice->createPipelineLayout(rng, rng + 2, nullptr, nullptr, nullptr, nullptr);


            core::smart_refctd_ptr<video::IGPUSpecializedShader> gpuGShaders[] = {
                gpuVertexShader, gpuFragmentShader};
            auto gpuGShadersPointer =
                reinterpret_cast<video::IGPUSpecializedShader **>(gpuGShaders);

            asset::SBlendParams blendParams;
            asset::SRasterizationParams rasterParams;
            rasterParams.faceCullingMode = asset::EFCM_NONE;

            auto gpuPipeline = logicalDevice->createRenderpassIndependentPipeline(
                nullptr, std::move(gpuPipelineLayout), gpuGShadersPointer,
                gpuGShadersPointer + 2, geometryObject.inputParams, blendParams,
                geometryObject.assemblyParams, rasterParams);

            constexpr auto MAX_ATTR_BUF_BINDING_COUNT =
                video::IGPUMeshBuffer::MAX_ATTR_BUF_BINDING_COUNT;
            constexpr auto MAX_DATA_BUFFERS = MAX_ATTR_BUF_BINDING_COUNT + 1;
            core::vector<asset::ICPUBuffer *> cpubuffers;
            cpubuffers.reserve(MAX_DATA_BUFFERS);
            for (auto i = 0; i < MAX_ATTR_BUF_BINDING_COUNT; i++) {
                auto buf = geometryObject.bindings[i].buffer.get();
                if (buf)
                cpubuffers.push_back(buf);
            }
            auto cpuindexbuffer = geometryObject.indexBuffer.buffer.get();
            if (cpuindexbuffer)
                cpubuffers.push_back(cpuindexbuffer);

            cpu2gpuParams.beginCommandBuffers();
            auto gpubuffers = cpu2gpu.getGPUObjectsFromAssets(
                cpubuffers.data(), cpubuffers.data() + cpubuffers.size(),
                cpu2gpuParams);
            cpu2gpuParams.waitForCreationToComplete();

            asset::SBufferBinding<video::IGPUBuffer> bindings[MAX_DATA_BUFFERS];
            for (auto i = 0, j = 0; i < MAX_ATTR_BUF_BINDING_COUNT; i++) {
                if (!geometryObject.bindings[i].buffer)
                continue;
                auto buffPair = gpubuffers->operator[](j++);
                bindings[i].offset = buffPair->getOffset();
                bindings[i].buffer =
                    core::smart_refctd_ptr<video::IGPUBuffer>(buffPair->getBuffer());
            }
            if (cpuindexbuffer) {
                auto buffPair = gpubuffers->back();
                bindings[MAX_ATTR_BUF_BINDING_COUNT].offset = buffPair->getOffset();
                bindings[MAX_ATTR_BUF_BINDING_COUNT].buffer =
                    core::smart_refctd_ptr<video::IGPUBuffer>(buffPair->getBuffer());
            }

            gpuMeshBuffer = core::make_smart_refctd_ptr<video::IGPUMeshBuffer>(
                core::smart_refctd_ptr(gpuPipeline), nullptr, bindings,
                std::move(bindings[MAX_ATTR_BUF_BINDING_COUNT]));
            {
                gpuMeshBuffer->setIndexType(geometryObject.indexType);
                gpuMeshBuffer->setIndexCount(geometryObject.indexCount);
                gpuMeshBuffer->setBoundingBox(geometryObject.bbox);
                gpuMeshBuffer->setInstanceCount(INSTANCE_COUNT);
            }

            {
                IGPUGraphicsPipeline::SCreationParams graphicsPipelineParams;
                graphicsPipelineParams.renderpassIndependent =
                    core::smart_refctd_ptr<IGPURenderpassIndependentPipeline>(
                        const_cast<video::IGPURenderpassIndependentPipeline *>(
                            gpuMeshBuffer->getPipeline()));
                graphicsPipelineParams.renderpass = core::smart_refctd_ptr(renderpass);
                gpuGraphicsPipeline = logicalDevice->createGraphicsPipeline(
                    nullptr, std::move(graphicsPipelineParams));
            }

            const auto &graphicsCommandPools =
                commandPools[CommonAPI::InitOutput::EQT_GRAPHICS];
            for (uint32_t i = 0u; i < FRAMES_IN_FLIGHT; i++) {
                logicalDevice->createCommandBuffers(graphicsCommandPools[i].get(),
                                                    video::IGPUCommandBuffer::EL_PRIMARY,
                                                    1, commandBuffers + i);
                imageAcquire[i] = logicalDevice->createSemaphore();
                renderFinished[i] = logicalDevice->createSemaphore();
            }

            matrix4SIMD projectionMatrix =
                matrix4SIMD::buildProjectionMatrixPerspectiveFovLH(
                    core::radians(60.0f), float(WIN_W) / WIN_H, 0.01f, 5000.0f);
            camera = Camera(core::vectorSIMDf(6.75f, 2.f, 6.f),
                            core::vectorSIMDf(6.75f, 0.f, -1.f), projectionMatrix, 10.f,
                            1.f);
            }

            void workLoopBody() override {
            ++resourceIx;
            if (resourceIx >= FRAMES_IN_FLIGHT)
                resourceIx = 0;

            auto &commandBuffer = commandBuffers[resourceIx];
            auto &fence = frameComplete[resourceIx];

            if (fence) {
                logicalDevice->blockForFences(1u, &fence.get());
                logicalDevice->resetFences(1u, &fence.get());
            } else
                fence = logicalDevice->createFence(
                    static_cast<video::IGPUFence::E_CREATE_FLAGS>(0));

            inputSystem->getDefaultKeyboard(&keyboard);
            keyboard.consumeEvents(
                [&](const ui::IKeyboardEventChannel::range_t &events) -> void {
                    for (auto &event : events) {
                    if (event.action == ui::SKeyboardEvent::ECA_PRESSED) {
                        switch (event.keyCode) {
                        case ui::EKC_1:
                        currentTestNum = TEST_GGX;
                        break;
                        case ui::EKC_2:
                        currentTestNum = TEST_BECKMANN;
                        break;
                        case ui::EKC_3:
                        currentTestNum = TEST_PHONG;
                        break;
                        case ui::EKC_4:
                        currentTestNum = TEST_AS;
                        break;
                        case ui::EKC_5:
                        currentTestNum = TEST_OREN_NAYAR;
                        break;
                        case ui::EKC_6:
                        currentTestNum = TEST_LAMBERT;
                        break;
                        }
                    }
                    }
                },
                logger.get());

            commandBuffer->reset(
                IGPUCommandBuffer::ERF_RELEASE_RESOURCES_BIT);
            commandBuffer->begin(IGPUCommandBuffer::EU_NONE);

            asset::SViewport viewport;
            viewport.minDepth = 1.f;
            viewport.maxDepth = 0.f;
            viewport.x = 0u;
            viewport.y = 0u;
            viewport.width = WIN_W;
            viewport.height = WIN_H;
            commandBuffer->setViewport(0u, 1u, &viewport);
            VkRect2D scissor;
            scissor.offset = {0u, 0u};
            scissor.extent = {WIN_W, WIN_H};
            commandBuffer->setScissor(0u, 1u, &scissor);

            swapchain->acquireNextImage(MAX_TIMEOUT, imageAcquire[resourceIx].get(),
                                        nullptr, &acquiredNextFBO);

            IGPUCommandBuffer::SRenderpassBeginInfo beginInfo;
            {
                VkRect2D area;
                area.offset = {0, 0};
                area.extent = {WIN_W, WIN_H};
                asset::SClearValue clear[2] = {};
                clear[0].color.float32[0] = 0.f;
                clear[0].color.float32[1] = 0.f;
                clear[0].color.float32[2] = 0.f;
                clear[0].color.float32[3] = 1.f;
                clear[1].depthStencil.depth = 0.f;

                beginInfo.clearValueCount = 2u;
                beginInfo.framebuffer = fbo->begin()[acquiredNextFBO];
                beginInfo.renderpass = renderpass;
                beginInfo.renderArea = area;
                beginInfo.clearValues = clear;
            }
            commandBuffer->beginRenderPass(&beginInfo, ESC_INLINE);
            commandBuffer->bindGraphicsPipeline(gpuGraphicsPipeline.get());

            SPushConsts pc;
            pc.vertStage.VP = camera.getConcatenatedMatrix();
            pc.fragStage.campos = core::vectorSIMDf(&camera.getPosition().X);
            pc.fragStage.testNum = currentTestNum;
            commandBuffer->pushConstants(
                gpuGraphicsPipeline->getRenderpassIndependentPipeline()->getLayout(),
                asset::IShader::ESS_VERTEX, 0u, sizeof(SPushConsts::vertStage),
                &pc.vertStage);

            commandBuffer->pushConstants(
                gpuGraphicsPipeline->getRenderpassIndependentPipeline()->getLayout(),
                asset::IShader::ESS_FRAGMENT, offsetof(SPushConsts, fragStage),
                sizeof(SPushConsts::fragStage), &pc.fragStage);

            commandBuffer->drawMeshBuffer(gpuMeshBuffer.get());

            commandBuffer->endRenderPass();
            commandBuffer->end();

            CommonAPI::Submit(logicalDevice.get(), commandBuffer.get(),
                                queues[CommonAPI::InitOutput::EQT_GRAPHICS],
                                imageAcquire[resourceIx].get(),
                                renderFinished[resourceIx].get(), fence.get());
            CommonAPI::Present(logicalDevice.get(), swapchain.get(),
                                queues[CommonAPI::InitOutput::EQT_GRAPHICS],
                                renderFinished[resourceIx].get(), acquiredNextFBO);
        }

        bool keepRunning() override { return windowCb->isWindowOpen(); }
};

NBL_COMMON_API_MAIN(BRDFEvalTestApp)