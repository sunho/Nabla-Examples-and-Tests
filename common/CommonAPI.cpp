#include "CommonAPI.h"

using namespace nbl;
using namespace video;

IPhysicalDevice* CommonAPI::CDefaultPhysicalDeviceSelector::selectPhysicalDevice(const core::set<IPhysicalDevice*>& suitablePhysicalDevices)
{

	if (suitablePhysicalDevices.empty())
		return nullptr;

	for (auto itr = suitablePhysicalDevices.begin(); itr != suitablePhysicalDevices.end(); ++itr)
	{
		IPhysicalDevice* physdev = *itr;
		if (physdev->getProperties().driverID == preferredDriver)
			return physdev;
	}

	return *suitablePhysicalDevices.begin();
}

ISwapchain::SCreationParams CommonAPI::computeSwapchainCreationParams(
	uint32_t& imageCount,
	const core::smart_refctd_ptr<ILogicalDevice>& device,
	const core::smart_refctd_ptr<ISurface>& surface,
	asset::IImage::E_USAGE_FLAGS imageUsage,
	// Acceptable settings, ordered by preference.
	const asset::E_FORMAT* acceptableSurfaceFormats, uint32_t acceptableSurfaceFormatCount,
	const asset::E_COLOR_PRIMARIES* acceptableColorPrimaries, uint32_t acceptableColorPrimaryCount,
	const asset::ELECTRO_OPTICAL_TRANSFER_FUNCTION* acceptableEotfs, uint32_t acceptableEotfCount,
	const ISurface::E_PRESENT_MODE* acceptablePresentModes, uint32_t acceptablePresentModeCount,
	const ISurface::E_SURFACE_TRANSFORM_FLAGS* acceptableSurfaceTransforms, uint32_t acceptableSurfaceTransformCount
)
{
	using namespace nbl;

	ISurface::SFormat surfaceFormat;
	ISurface::E_PRESENT_MODE presentMode = ISurface::EPM_UNKNOWN;
	ISurface::E_SURFACE_TRANSFORM_FLAGS surfaceTransform = ISurface::EST_FLAG_BITS_MAX_ENUM;

	if (device->getAPIType() == EAT_VULKAN)
	{
		ISurface::SCapabilities surfaceCapabilities;
		surface->getSurfaceCapabilitiesForPhysicalDevice(device->getPhysicalDevice(), surfaceCapabilities);

		for (uint32_t i = 0; i < acceptableSurfaceTransformCount; i++)
		{
			auto testSurfaceTransform = acceptableSurfaceTransforms[i];
			if (surfaceCapabilities.currentTransform == testSurfaceTransform)
			{
				surfaceTransform = testSurfaceTransform;
				break;
			}
		}
		assert(surfaceTransform != ISurface::EST_FLAG_BITS_MAX_ENUM); // currentTransform must be supported in acceptableSurfaceTransforms

		auto availablePresentModes = surface->getAvailablePresentModesForPhysicalDevice(device->getPhysicalDevice());
		for (uint32_t i = 0; i < acceptablePresentModeCount; i++)
		{
			auto testPresentMode = acceptablePresentModes[i];
			if ((availablePresentModes & testPresentMode) == testPresentMode)
			{
				presentMode = testPresentMode;
				break;
			}
		}
		assert(presentMode != ISurface::EST_FLAG_BITS_MAX_ENUM);

		constexpr uint32_t MAX_SURFACE_FORMAT_COUNT = 1000u;
		uint32_t availableFormatCount;
		ISurface::SFormat availableFormats[MAX_SURFACE_FORMAT_COUNT];
		surface->getAvailableFormatsForPhysicalDevice(device->getPhysicalDevice(), availableFormatCount, availableFormats);

		for (uint32_t i = 0; i < availableFormatCount; ++i)
		{
			auto testsformat = availableFormats[i];
			bool supportsFormat = false;
			bool supportsEotf = false;
			bool supportsPrimary = false;

			for (uint32_t i = 0; i < acceptableSurfaceFormatCount; i++)
			{
				if (testsformat.format == acceptableSurfaceFormats[i])
				{
					supportsFormat = true;
					break;
				}
			}
			for (uint32_t i = 0; i < acceptableEotfCount; i++)
			{
				if (testsformat.colorSpace.eotf == acceptableEotfs[i])
				{
					supportsEotf = true;
					break;
				}
			}
			for (uint32_t i = 0; i < acceptableColorPrimaryCount; i++)
			{
				if (testsformat.colorSpace.primary == acceptableColorPrimaries[i])
				{
					supportsPrimary = true;
					break;
				}
			}

			if (supportsFormat && supportsEotf && supportsPrimary)
			{
				surfaceFormat = testsformat;
				break;
			}
		}
		// Require at least one of the acceptable options to be present
		assert(surfaceFormat.format != asset::EF_UNKNOWN &&
			surfaceFormat.colorSpace.primary != asset::ECP_COUNT &&
			surfaceFormat.colorSpace.eotf != asset::EOTF_UNKNOWN);

		imageCount = std::max(surfaceCapabilities.minImageCount, std::min(surfaceCapabilities.maxImageCount, imageCount));
	}
	else
	{
		// Temporary path until OpenGL reports properly!
		surfaceFormat = ISurface::SFormat(acceptableSurfaceFormats[0], acceptableColorPrimaries[0], acceptableEotfs[0]);
		presentMode = ISurface::EPM_IMMEDIATE;
		surfaceTransform = ISurface::EST_HORIZONTAL_MIRROR_ROTATE_180_BIT;
	}
	ISwapchain::SCreationParams sc_params = {};
	sc_params.arrayLayers = 1u;
	sc_params.minImageCount = imageCount;
	sc_params.presentMode = presentMode;
	sc_params.imageUsage = imageUsage;
	sc_params.surface = surface;
	sc_params.preTransform = surfaceTransform;
	sc_params.compositeAlpha = ISurface::ECA_OPAQUE_BIT;
	sc_params.surfaceFormat = surfaceFormat;

	return sc_params;
}

void CommonAPI::dropRetiredSwapchainResources(core::deque<IRetiredSwapchainResources*>& qRetiredSwapchainResources, const uint64_t completedFrameId)
{
	while (!qRetiredSwapchainResources.empty() && qRetiredSwapchainResources.front()->retiredFrameId < completedFrameId)
	{
		std::cout << "Dropping resource scheduled at " << qRetiredSwapchainResources.front()->retiredFrameId << " with completedFrameId " << completedFrameId << "\n";
		delete(qRetiredSwapchainResources.front());
		qRetiredSwapchainResources.pop_front();
	}
}

void CommonAPI::retireSwapchainResources(core::deque<IRetiredSwapchainResources*>& qRetiredSwapchainResources, IRetiredSwapchainResources* retired)
{
	qRetiredSwapchainResources.push_back(retired);
}

core::smart_refctd_ptr<IGPURenderpass> CommonAPI::createRenderpass(const core::smart_refctd_ptr<ILogicalDevice>& device, asset::E_FORMAT colorAttachmentFormat, asset::E_FORMAT baseDepthFormat)
{
	using namespace nbl;

	bool useDepth = baseDepthFormat != asset::EF_UNKNOWN;
	asset::E_FORMAT depthFormat = asset::EF_UNKNOWN;
	if (useDepth)
	{
		depthFormat = device->getPhysicalDevice()->promoteImageFormat(
			{ baseDepthFormat, IPhysicalDevice::SFormatImageUsages::SUsage(asset::IImage::EUF_DEPTH_STENCIL_ATTACHMENT_BIT) },
			IGPUImage::ET_OPTIMAL
		);
		assert(depthFormat != asset::EF_UNKNOWN);
	}

	IGPURenderpass::SCreationParams::SAttachmentDescription attachments[2];
	attachments[0].initialLayout = asset::IImage::EL_UNDEFINED;
	attachments[0].finalLayout = asset::IImage::EL_PRESENT_SRC;
	attachments[0].format = colorAttachmentFormat;
	attachments[0].samples = asset::IImage::ESCF_1_BIT;
	attachments[0].loadOp = IGPURenderpass::ELO_CLEAR;
	attachments[0].storeOp = IGPURenderpass::ESO_STORE;

	attachments[1].initialLayout = asset::IImage::EL_UNDEFINED;
	attachments[1].finalLayout = asset::IImage::EL_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].format = depthFormat;
	attachments[1].samples = asset::IImage::ESCF_1_BIT;
	attachments[1].loadOp = IGPURenderpass::ELO_CLEAR;
	attachments[1].storeOp = IGPURenderpass::ESO_STORE;

	IGPURenderpass::SCreationParams::SSubpassDescription::SAttachmentRef colorAttRef;
	colorAttRef.attachment = 0u;
	colorAttRef.layout = asset::IImage::EL_COLOR_ATTACHMENT_OPTIMAL;

	IGPURenderpass::SCreationParams::SSubpassDescription::SAttachmentRef depthStencilAttRef;
	depthStencilAttRef.attachment = 1u;
	depthStencilAttRef.layout = asset::IImage::EL_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	IGPURenderpass::SCreationParams::SSubpassDescription sp;
	sp.pipelineBindPoint = asset::EPBP_GRAPHICS;
	sp.colorAttachmentCount = 1u;
	sp.colorAttachments = &colorAttRef;
	if (useDepth) {
		sp.depthStencilAttachment = &depthStencilAttRef;
	}
	else {
		sp.depthStencilAttachment = nullptr;
	}
	sp.flags = IGPURenderpass::ESDF_NONE;
	sp.inputAttachmentCount = 0u;
	sp.inputAttachments = nullptr;
	sp.preserveAttachmentCount = 0u;
	sp.preserveAttachments = nullptr;
	sp.resolveAttachments = nullptr;

	IGPURenderpass::SCreationParams rp_params;
	rp_params.attachmentCount = (useDepth) ? 2u : 1u;
	rp_params.attachments = attachments;
	rp_params.dependencies = nullptr;
	rp_params.dependencyCount = 0u;
	rp_params.subpasses = &sp;
	rp_params.subpassCount = 1u;

	return device->createRenderpass(rp_params);
}

core::smart_refctd_dynamic_array<core::smart_refctd_ptr<IGPUFramebuffer>> CommonAPI::createFBOWithSwapchainImages(
	size_t imageCount, uint32_t width, uint32_t height,
	const core::smart_refctd_ptr<ILogicalDevice>& device,
	core::smart_refctd_ptr<ISwapchain> swapchain,
	core::smart_refctd_ptr<IGPURenderpass> renderpass,
	asset::E_FORMAT baseDepthFormat
) {
	using namespace nbl;

	bool useDepth = baseDepthFormat != asset::EF_UNKNOWN;
	asset::E_FORMAT depthFormat = asset::EF_UNKNOWN;
	if (useDepth)
	{
		depthFormat = baseDepthFormat;
		//depthFormat = device->getPhysicalDevice()->promoteImageFormat(
		//	{ baseDepthFormat, IPhysicalDevice::SFormatImageUsages::SUsage(asset::IImage::EUF_DEPTH_STENCIL_ATTACHMENT_BIT) },
		//	asset::IImage::ET_OPTIMAL
		//);
		// TODO error reporting
		assert(depthFormat != asset::EF_UNKNOWN);
	}

	auto fbo = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<core::smart_refctd_ptr<IGPUFramebuffer>>>(imageCount);
	for (uint32_t i = 0u; i < imageCount; ++i)
	{
		core::smart_refctd_ptr<IGPUImageView> view[2] = {};

		auto img = swapchain->createImage(i);
		{
			IGPUImageView::SCreationParams view_params;
			view_params.format = img->getCreationParameters().format;
			view_params.viewType = asset::IImageView<IGPUImage>::ET_2D;
			view_params.subresourceRange.aspectMask = asset::IImage::EAF_COLOR_BIT;
			view_params.subresourceRange.baseMipLevel = 0u;
			view_params.subresourceRange.levelCount = 1u;
			view_params.subresourceRange.baseArrayLayer = 0u;
			view_params.subresourceRange.layerCount = 1u;
			view_params.image = std::move(img);

			view[0] = device->createImageView(std::move(view_params));
			assert(view[0]);
		}

		if (useDepth) {
			IGPUImage::SCreationParams imgParams;
			imgParams.flags = static_cast<asset::IImage::E_CREATE_FLAGS>(0u);
			imgParams.type = asset::IImage::ET_2D;
			imgParams.format = depthFormat;
			imgParams.extent = { width, height, 1 };
			imgParams.usage = asset::IImage::E_USAGE_FLAGS::EUF_DEPTH_STENCIL_ATTACHMENT_BIT;
			imgParams.mipLevels = 1u;
			imgParams.arrayLayers = 1u;
			imgParams.samples = asset::IImage::ESCF_1_BIT;

			auto depthImg = device->createImage(std::move(imgParams));
			auto depthImgMemReqs = depthImg->getMemoryReqs();
			depthImgMemReqs.memoryTypeBits &= device->getPhysicalDevice()->getDeviceLocalMemoryTypeBits();
			auto depthImgMem = device->allocate(depthImgMemReqs, depthImg.get());

			IGPUImageView::SCreationParams view_params;
			view_params.format = depthFormat;
			view_params.viewType = asset::IImageView<IGPUImage>::ET_2D;
			view_params.subresourceRange.aspectMask = asset::IImage::EAF_DEPTH_BIT;
			view_params.subresourceRange.baseMipLevel = 0u;
			view_params.subresourceRange.levelCount = 1u;
			view_params.subresourceRange.baseArrayLayer = 0u;
			view_params.subresourceRange.layerCount = 1u;
			view_params.image = std::move(depthImg);

			view[1] = device->createImageView(std::move(view_params));
			assert(view[1]);
		}

		IGPUFramebuffer::SCreationParams fb_params;
		fb_params.width = width;
		fb_params.height = height;
		fb_params.layers = 1u;
		fb_params.renderpass = renderpass;
		fb_params.flags = static_cast<IGPUFramebuffer::E_CREATE_FLAGS>(0);
		fb_params.attachmentCount = (useDepth) ? 2u : 1u;
		fb_params.attachments = view;

		fbo->begin()[i] = device->createFramebuffer(std::move(fb_params));
		assert(fbo->begin()[i]);
	}
	return fbo;
}

bool CommonAPI::createSwapchain(
	const core::smart_refctd_ptr<ILogicalDevice>&& device,
	ISwapchain::SCreationParams& params,
	uint32_t width, uint32_t height,
	// nullptr for initial creation, old swapchain for eventual resizes
	core::smart_refctd_ptr<ISwapchain>& swapchain
)
{
	auto oldSwapchain = swapchain;

	ISwapchain::SCreationParams paramsCp = params;
	paramsCp.width = width;
	paramsCp.height = height;
	paramsCp.oldSwapchain = oldSwapchain;

	assert(device->getAPIType() == EAT_VULKAN);
	swapchain = CVulkanSwapchain::create(std::move(device), std::move(paramsCp));
	assert(swapchain);
	assert(swapchain != oldSwapchain);

	return true;
}


//! Graphical Application
#ifndef _NBL_PLATFORM_ANDROID_

// TODO: probably rewrite
void GraphicalApplication::immediateImagePresent(nbl::video::IGPUQueue* queue, nbl::video::ISwapchain* swapchain, nbl::core::smart_refctd_ptr<nbl::video::IGPUImage>* swapchainImages, uint32_t frameIx, uint32_t lastRenderW, uint32_t lastRenderH)
{
	using namespace nbl;

	uint32_t bufferIx = frameIx % 2;
	auto image = m_tripleBufferRenderTargets.begin()[bufferIx];
	auto logicalDevice = getLogicalDevice();

	auto imageAcqToSubmit = logicalDevice->createSemaphore();
	auto submitToPresent = logicalDevice->createSemaphore();

	// acquires image, allocates one shot fences, commandpool and commandbuffer to do a blit, submits and presents
	uint32_t imgnum = 0;
	bool acquireResult = tryAcquireImage(swapchain, imageAcqToSubmit.get(), &imgnum);
	assert(acquireResult);

	auto& swapchainImage = swapchainImages[imgnum]; // tryAcquireImage will have this image be recreated
	auto fence = logicalDevice->createFence(static_cast<nbl::video::IGPUFence::E_CREATE_FLAGS>(0));;
	auto commandPool = logicalDevice->createCommandPool(queue->getFamilyIndex(), nbl::video::IGPUCommandPool::ECF_NONE);
	nbl::core::smart_refctd_ptr<nbl::video::IGPUCommandBuffer> commandBuffer;
	logicalDevice->createCommandBuffers(commandPool.get(), nbl::video::IGPUCommandBuffer::EL_PRIMARY, 1u, &commandBuffer);

	commandBuffer->begin(nbl::video::IGPUCommandBuffer::EU_ONE_TIME_SUBMIT_BIT);

	const uint32_t numBarriers = 2;
	video::IGPUCommandBuffer::SImageMemoryBarrier layoutTransBarrier[numBarriers] = {};
	for (uint32_t i = 0; i < numBarriers; i++) {
		layoutTransBarrier[i].srcQueueFamilyIndex = ~0u;
		layoutTransBarrier[i].dstQueueFamilyIndex = ~0u;
		layoutTransBarrier[i].subresourceRange.aspectMask = asset::IImage::EAF_COLOR_BIT;
		layoutTransBarrier[i].subresourceRange.baseMipLevel = 0u;
		layoutTransBarrier[i].subresourceRange.levelCount = 1u;
		layoutTransBarrier[i].subresourceRange.baseArrayLayer = 0u;
		layoutTransBarrier[i].subresourceRange.layerCount = 1u;
	}

	layoutTransBarrier[0].barrier.srcAccessMask = asset::EAF_NONE;
	layoutTransBarrier[0].barrier.dstAccessMask = asset::EAF_TRANSFER_WRITE_BIT;
	layoutTransBarrier[0].oldLayout = asset::IImage::EL_UNDEFINED;
	layoutTransBarrier[0].newLayout = asset::IImage::EL_TRANSFER_DST_OPTIMAL;
	layoutTransBarrier[0].image = swapchainImage;

	layoutTransBarrier[1].barrier.srcAccessMask = asset::EAF_NONE;
	layoutTransBarrier[1].barrier.dstAccessMask = asset::EAF_TRANSFER_READ_BIT;
	layoutTransBarrier[1].oldLayout = asset::IImage::EL_GENERAL;
	layoutTransBarrier[1].newLayout = asset::IImage::EL_TRANSFER_SRC_OPTIMAL;
	layoutTransBarrier[1].image = image;

	commandBuffer->pipelineBarrier(
		asset::EPSF_TOP_OF_PIPE_BIT,
		asset::EPSF_TRANSFER_BIT,
		static_cast<asset::E_DEPENDENCY_FLAGS>(0u),
		0u, nullptr,
		0u, nullptr,
		numBarriers, &layoutTransBarrier[0]);

	nbl::asset::SImageBlit blit;
	blit.srcSubresource.aspectMask = nbl::video::IGPUImage::EAF_COLOR_BIT;
	blit.srcSubresource.layerCount = 1;
	blit.srcOffsets[0] = { 0, 0, 0 };
	blit.srcOffsets[1] = { lastRenderW, lastRenderH, 1 };
	blit.dstSubresource.aspectMask = nbl::video::IGPUImage::EAF_COLOR_BIT;
	blit.dstSubresource.layerCount = 1;
	blit.dstOffsets[0] = { 0, 0, 0 };
	blit.dstOffsets[1] = { swapchain->getCreationParameters().width, swapchain->getCreationParameters().height, 1 };

	printf(
		"Blitting from frame %i buffer %i with last render dimensions %ix%i and output %ix%i\n",
		frameIx, bufferIx,
		lastRenderW, lastRenderH,
		image->getCreationParameters().extent.width, image->getCreationParameters().extent.height
	);
	commandBuffer->blitImage(
		image.get(), nbl::asset::IImage::EL_TRANSFER_SRC_OPTIMAL,
		swapchainImage.get(), nbl::asset::IImage::EL_TRANSFER_DST_OPTIMAL,
		1, &blit, nbl::asset::ISampler::ETF_LINEAR
	);

	layoutTransBarrier[0].barrier.srcAccessMask = asset::EAF_TRANSFER_WRITE_BIT;
	layoutTransBarrier[0].barrier.dstAccessMask = asset::EAF_NONE;
	layoutTransBarrier[0].oldLayout = asset::IImage::EL_TRANSFER_DST_OPTIMAL;
	layoutTransBarrier[0].newLayout = asset::IImage::EL_PRESENT_SRC;

	layoutTransBarrier[1].barrier.srcAccessMask = asset::EAF_TRANSFER_READ_BIT;
	layoutTransBarrier[1].barrier.dstAccessMask = asset::EAF_NONE;
	layoutTransBarrier[1].oldLayout = asset::IImage::EL_TRANSFER_SRC_OPTIMAL;
	layoutTransBarrier[1].newLayout = asset::IImage::EL_GENERAL;

	commandBuffer->pipelineBarrier(
		asset::EPSF_TRANSFER_BIT,
		asset::EPSF_BOTTOM_OF_PIPE_BIT,
		static_cast<asset::E_DEPENDENCY_FLAGS>(0u),
		0u, nullptr,
		0u, nullptr,
		numBarriers, &layoutTransBarrier[0]);

	commandBuffer->end();

	CommonAPI::Submit(
		logicalDevice, commandBuffer.get(), queue,
		imageAcqToSubmit.get(),
		submitToPresent.get(),
		fence.get());
	CommonAPI::Present(
		logicalDevice,
		swapchain,
		queue,
		submitToPresent.get(),
		imgnum);

	logicalDevice->blockForFences(1u, &fence.get());
}


bool GraphicalApplication::tryAcquireImage(nbl::video::ISwapchain* swapchain, nbl::video::IGPUSemaphore* waitSemaphore, uint32_t* imgnum)
{
	constexpr uint64_t MAX_TIMEOUT = 99999999999999ull;
	if (swapchain->acquireNextImage(MAX_TIMEOUT, waitSemaphore, nullptr, imgnum) == nbl::video::ISwapchain::EAIR_SUCCESS)
	{
		if (m_swapchainIteration > m_imageSwapchainIterations[*imgnum])
		{
			auto retiredResources = onCreateResourcesWithSwapchain(*imgnum).release();
			m_imageSwapchainIterations[*imgnum] = m_swapchainIteration;
			if (retiredResources)
				CommonAPI::retireSwapchainResources(m_qRetiredSwapchainResources, retiredResources);
		}

		return true;
	}
	return false;
}
#else
#error "rewrite needed"
#endif