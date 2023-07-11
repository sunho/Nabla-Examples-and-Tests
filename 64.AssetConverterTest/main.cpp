
#include <nabla.h>

#include "../common/CommonAPI.h"

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

		//test nullptrs in visitor methods
		{
			auto asset = core::make_smart_refctd_ptr<nbl::asset::ICPUBuffer>(1024u);
			auto different_asset = core::make_smart_refctd_ptr<nbl::asset::ICPUBuffer>(1025u);
			make_asset_asserts(asset, different_asset);
		}
		{
			/*nbl::asset::ICPUAccelerationStructure::SCreationParams params;
			params.flags = nbl::asset::IAccelerationStructure::E_CREATE_FLAGS::ECF_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT;
			params.type = nbl::asset::IAccelerationStructure::E_TYPE::ET_TOP_LEVEL;
			auto asset = core::make_smart_refctd_ptr<nbl::asset::ICPUAccelerationStructure>(params);*/

			//ommited due to NBL_TODO in compatible
			/*auto different_asset = ...;
			make_asset_asserts(asset, different_asset);*/

		}
		{
			/*auto asset = core::make_smart_refctd_ptr<nbl::asset::ICPUAnimationLibrary>();
			auto different_asset = core::make_smart_refctd_ptr<nbl::asset::ICPUAnimationLibrary>();
			make_asset_asserts(asset, different_asset);*/
		}
		{
			auto buffer = core::make_smart_refctd_ptr<nbl::asset::ICPUBuffer>(64);
			auto asset = core::make_smart_refctd_ptr<nbl::asset::ICPUBufferView>(buffer, nbl::asset::E_FORMAT::EF_S8_UINT);
			auto buffer2 = core::make_smart_refctd_ptr<nbl::asset::ICPUBuffer>(64);
			auto different_asset = core::make_smart_refctd_ptr<nbl::asset::ICPUBufferView>(buffer2, nbl::asset::E_FORMAT::EF_D16_UNORM);
			make_asset_asserts(asset, different_asset);
		}
				//test hash
	}

	void onAppTerminated_impl() override
	{
		logger->log("==========Passed==========", system::ILogger::ELL_INFO);
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

	void make_asset_asserts(core::smart_refctd_ptr<asset::IAsset> assetA, core::smart_refctd_ptr<asset::IAsset> assetB)
	{
		std::cout << assetA->getAssetType() << std::endl;
		assert(assetA->hash() != assetB->hash());
		std::unordered_map<asset::IAsset*, size_t> temporary_hash_cache = {};
		assert(assetA->hash() == assetA->hash(&temporary_hash_cache));
		assert(temporary_hash_cache.size() >= 1);
		temporary_hash_cache.insert({assetB.get(), 2137});
		assert(assetB->hash(&temporary_hash_cache) == 2137);

		assert(!assetA->canBeRestoredFrom(nullptr));
		assert(assetA->canBeRestoredFrom(assetA.get()));
		assert(!assetA->canBeRestoredFrom(assetB.get()));

		assert(!assetA->equals(nullptr));
		assert(assetA->equals(assetA.get()));
		assert(!assetA->equals(assetB.get()));

		//make sure these do not cause exceptions but discard return values
		assetA->willBeRestoredFrom(nullptr);
		assetA->willBeRestoredFrom(assetB.get());
		assetA->canBeConvertedToDummy();
		assetA->restoreFromDummy(nullptr);
		assetA->restoreFromDummy(assetB.get());
		auto clone = assetA->clone();
		assetA->convertToDummyObject();
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
