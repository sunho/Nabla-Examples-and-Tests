// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h
#include "nabla.h"


#include <iostream>
#include <cstdio>


using namespace nbl;
using namespace core;
using namespace asset;
using namespace system;


int main(int argc, char * argv[])
{
	// need to call this to Delay-Load DLLs properly
	IApplicationFramework::GlobalsInit();

	smart_refctd_ptr<ISystem> system = nullptr;
	{
	#ifdef _NBL_PLATFORM_WINDOWS_
		system = make_smart_refctd_ptr<nbl::system::CSystemWin32>();
	#elif defined(_NBL_PLATFORM_LINUX_)
		system = make_smart_refctd_ptr<nbl::system::CSystemLinux>();
	#else
	#error "Unsupported Platform"
	#endif
	}

	#if defined(_NBL_PLATFORM_WINDOWS_)
	auto logger = core::make_smart_refctd_ptr<system::CColoredStdoutLoggerWin32>();
	#else
	auto logger = core::make_smart_refctd_ptr<system::CColoredStdoutLoggerANSI>();
	#endif
	auto assetManager = core::make_smart_refctd_ptr<nbl::asset::IAssetManager>(nbl::core::smart_refctd_ptr(system));

	const bool isItDefaultImage = argc == 1;
	if (isItDefaultImage)
		logger->log("No image specified - loading a default OpenEXR image placed in media/OpenEXR!", ILogger::ELL_INFO);
	else if (argc == 2)
		logger->log((argv[1] + std::string(" specified!")).c_str(), ILogger::ELL_INFO);
	else
	{
		logger->log("To many arguments - pass a single filename without .exr extension of OpenEXR image placed in media/OpenEXR!", ILogger::ELL_ERROR);
		return 0;
	}

	IAssetLoader::SAssetLoadParams loadParams;
	constexpr std::string_view defaultImagePath = "../../media/noises/spp_benchmark_4k_512.exr";
	const auto filePath = std::string(isItDefaultImage ? defaultImagePath.data() : argv[1]);

	const asset::COpenEXRMetadata* meta;
	auto image_bundle = assetManager->getAsset(filePath, loadParams);
	auto contents = image_bundle.getContents();
	{
		bool status = !contents.empty();
		assert(status);
		status = meta = image_bundle.getMetadata()->selfCast<const COpenEXRMetadata>();
		assert(status);
	}

	uint32_t i = 0u;
	for (auto asset : contents)
	{
		auto image = IAsset::castDown<ICPUImage>(asset);
		const auto* metadata = static_cast<const COpenEXRMetadata::CImage*>(meta->getAssetSpecificMetadata(image.get()));

		ICPUImageView::SCreationParams imgViewParams;
		imgViewParams.flags = static_cast<ICPUImageView::E_CREATE_FLAGS>(0u);
		imgViewParams.image = std::move(image);
		imgViewParams.format = imgViewParams.image->getCreationParameters().format;
		imgViewParams.viewType = ICPUImageView::ET_2D;
		imgViewParams.subresourceRange = { static_cast<IImage::E_ASPECT_FLAGS>(0u),0u,1u,0u,1u };
		auto imageView = ICPUImageView::create(std::move(imgViewParams));

		auto channelsName = metadata->m_name;

		std::filesystem::path filename, extension;
		core::splitFilename(filePath.c_str(), nullptr, &filename, &extension);
		const std::string finalFileNameWithExtension = filename.string() + extension.string();
		const std::string finalOutputPath = channelsName.empty() ? (filename.string() + "_" + std::to_string(i++) + extension.string()) : (filename.string() + "_" + channelsName + extension.string());

		const auto writeParams = IAssetWriter::SAssetWriteParams(imageView.get(), EWF_BINARY);
		{
			bool status = assetManager->writeAsset(finalOutputPath, writeParams);
			assert(status);
		}
	}
		
	return 0;
}
