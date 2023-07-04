
#include <nabla.h>

#include "../common/CommonAPI.h"
#include "nbl/video/EApiType.h"
#include "nbl/video/IApiConnection.h"

using namespace nbl;
using namespace core;
using namespace video;

class AssetConverterUnitTestApp : public NonGraphicalApplicationBase
{

public:
	void setSystem(nbl::core::smart_refctd_ptr<nbl::system::ISystem>&& s) override
	{
		system = std::move(s);
	}

	NON_GRAPHICAL_APP_CONSTRUCTOR(AssetConverterUnitTestApp)

	void onAppInitialized_impl() override
	{
		CommonAPI::InitParams initParams;
		initParams.apiType = nbl::video::EAT_VULKAN;
		initParams.appName = { "Asset Converter Test" };
		auto initOutput = CommonAPI::Init(std::move(initParams));

		apiConnection = std::move(initOutput.apiConnection);
		gpuPhysicalDevice = std::move(initOutput.physicalDevice);
		logicalDevice = std::move(initOutput.logicalDevice);
		queues = std::move(initOutput.queues);
		commandPools = std::move(initOutput.commandPools);
		assetManager = std::move(initOutput.assetManager);
		logger = std::move(initOutput.logger);
		inputSystem = std::move(initOutput.inputSystem);
		system = std::move(initOutput.system);
		cpu2gpuParams = std::move(initOutput.cpu2gpuParams);
		utilities = std::move(initOutput.utilities);

		
	}

	void onAppTerminated_impl() override
	{
		logger->log("==========Result==========", system::ILogger::ELL_INFO);
		//logger->log("Fail Count: %u", system::ILogger::ELL_INFO, totalFailCount);
	}

	void workLoopBody() override
	{
		//unit test is ran from onAppInitialized_impl func
	}

	bool keepRunning() override
	{
		return false;
	}

private:

	nbl::core::smart_refctd_ptr<nbl::video::IAPIConnection> apiConnection;
	nbl::core::smart_refctd_ptr<nbl::video::IUtilities> utilities;
	nbl::core::smart_refctd_ptr<nbl::video::ILogicalDevice> logicalDevice;
	nbl::video::IPhysicalDevice* gpuPhysicalDevice;
	std::array<video::IGPUQueue*, CommonAPI::InitOutput::MaxQueuesCount> queues;
	nbl::core::smart_refctd_ptr<nbl::video::IGPURenderpass> renderpass;
	std::array<std::array<nbl::core::smart_refctd_ptr<nbl::video::IGPUCommandPool>, CommonAPI::InitOutput::MaxFramesInFlight>, CommonAPI::InitOutput::MaxQueuesCount> commandPools;
	nbl::core::smart_refctd_ptr<nbl::system::ISystem> system;
	nbl::core::smart_refctd_ptr<nbl::asset::IAssetManager> assetManager;
	nbl::video::IGPUObjectFromAssetConverter::SParams cpu2gpuParams;
	nbl::core::smart_refctd_ptr<nbl::system::ILogger> logger;
	nbl::core::smart_refctd_ptr<CommonAPI::InputSystem> inputSystem;

	uint32_t* inputData = nullptr;
	uint32_t totalFailCount = 0;
};

NBL_COMMON_API_MAIN(AssetConverterUnitTestApp)
