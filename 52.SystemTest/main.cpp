// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#include "nabla.h"

#include <iostream>
#include <cstdio>

#include "../common/CommonAPI.h"
#include "nbl/system/CStdoutLogger.h"
#include "nbl/system/CFileLogger.h"
#include "nbl/system/CColoredStdoutLoggerWin32.h"

//! builtin resources archive test
#ifdef _BR_TEST_
#include "nbl/builtin/CArchive.h"
#include "yourNamespace/builtin/CArchive.h"
#endif

using namespace nbl;
using namespace core;
using namespace ui;
using namespace system;
using namespace asset;

class WindowEventCallback;

class InputSystem : public IReferenceCounted
{
	public:
		template <class ChannelType>
		struct Channels
		{
			core::mutex lock;
			std::condition_variable added;
			core::vector<core::smart_refctd_ptr<ChannelType>> channels;
		};
		// TODO: move to "nbl/ui/InputEventChannel.h" once the interface of this utility struct matures, also maybe rename to `Consumer` ?
		template <class ChannelType>
		struct ChannelReader
		{
			template<typename F>
			inline void consumeEvents(F&& processFunc, system::logger_opt_ptr logger=nullptr)
			{
				auto events = channel->getEvents();
				const auto frontBufferCapacity = channel->getFrontBufferCapacity();
				if (events.size()>consumedCounter+frontBufferCapacity)
				{
					logger.log(
						"Detected overflow, %d unconsumed events in channel of size %d!",
						system::ILogger::ELL_ERROR,events.size()-consumedCounter,frontBufferCapacity
					);
					consumedCounter = events.size()-frontBufferCapacity;
				}
				processFunc(ChannelType::range_t(events.begin()+consumedCounter,events.end()), channel);
				consumedCounter = events.size();
			}

			core::smart_refctd_ptr<ChannelType> channel = nullptr;
			uint64_t consumedCounter = 0ull;
		};
		
		InputSystem(system::logger_opt_smart_ptr&& logger) : m_logger(std::move(logger)) {}

		void getDefaultMouse(ChannelReader<IMouseEventChannel>* reader)
		{
			getDefault(m_mouse,reader);
		}
		void getDefaultKeyboard(ChannelReader<IKeyboardEventChannel>* reader)
		{
			getDefault(m_keyboard,reader);
		}

	private:
		friend class WindowEventCallback;
		template<class ChannelType>
		void add(Channels<ChannelType>& channels, core::smart_refctd_ptr<ChannelType>&& channel)
		{
			std::unique_lock lock(channels.lock);
			channels.channels.push_back(std::move(channel));
			channels.added.notify_all();
		}
		template<class ChannelType>
		void remove(Channels<ChannelType>& channels, const ChannelType* channel)
		{
			std::unique_lock lock(channels.lock);
			channels.channels.erase(
				std::find_if(
					channels.channels.begin(),channels.channels.end(),[channel](const auto& chan)->bool{return chan.get()==channel;}
				)
			);
		}
		template<class ChannelType>
		void getDefault(Channels<ChannelType>& channels, ChannelReader<ChannelType>* reader)
		{
			/*
			* TODO: Improve default device switching.
			* For nice results, we should actually make a multi-channel reader,
			* and then keep a consumed counter together with a last consumed event from each channel.
			* If there is considerable pause in events received by our current chosen channel or
			* we can detect some other channel of the same "compatible class" is producing more events,
			* Switch the channel choice, but prune away all events younger than the old default's consumption timestamp.
			* (Basically switch keyboards but dont try to process events older than the events you've processed from the old keyboard)
			*/
			std::unique_lock lock(channels.lock);
			while (channels.channels.empty())
			{
				m_logger.log("Waiting For Input Device to be connected...",system::ILogger::ELL_INFO);
				channels.added.wait(lock);
			}

			auto current_default = channels.channels.front();
			if (reader->channel==current_default)
				return;

			reader->channel = current_default;
			reader->consumedCounter = 0u;
		}

		system::logger_opt_smart_ptr m_logger;
		Channels<IMouseEventChannel> m_mouse;
		Channels<IKeyboardEventChannel> m_keyboard;
};

class WindowEventCallback : public IWindow::IEventCallback
{
public:
	WindowEventCallback(core::smart_refctd_ptr<InputSystem>&& inputSystem, system::logger_opt_smart_ptr&& logger) : m_inputSystem(std::move(inputSystem)), m_logger(std::move(logger)), m_gotWindowClosedMsg(false) {}

	bool isWindowOpen() const {return !m_gotWindowClosedMsg;}

private:
	bool onWindowShown_impl() override 
	{
		m_logger.log("Window Shown");
		return true;
	}
	bool onWindowHidden_impl() override
	{
		m_logger.log("Window hidden");
		return true;
	}
	bool onWindowMoved_impl(int32_t x, int32_t y) override
	{
		m_logger.log("Window window moved to { %d, %d }", system::ILogger::ELL_WARNING, x, y);
		return true;
	}
	bool onWindowResized_impl(uint32_t w, uint32_t h) override
	{
		m_logger.log("Window resized to { %u, %u }", system::ILogger::ELL_DEBUG, w, h);
		return true;
	}
	bool onWindowMinimized_impl() override
	{
		m_logger.log("Window minimized", system::ILogger::ELL_ERROR);
		return true;
	}
	bool onWindowMaximized_impl() override
	{
		m_logger.log("Window maximized", system::ILogger::ELL_PERFORMANCE);
		return true;
	}
	void onGainedMouseFocus_impl() override
	{
		m_logger.log("Window gained mouse focus", system::ILogger::ELL_INFO);
	}
	void onLostMouseFocus_impl() override
	{
		m_logger.log("Window lost mouse focus", system::ILogger::ELL_INFO);
	}
	void onGainedKeyboardFocus_impl() override
	{
		m_logger.log("Window gained keyboard focus", system::ILogger::ELL_INFO);
	}
	void onLostKeyboardFocus_impl() override
	{
		m_logger.log("Window lost keyboard focus", system::ILogger::ELL_INFO);
	}
	bool onWindowClosed_impl() override
	{
		m_logger.log("Window closed");
		m_gotWindowClosedMsg = true;
		return true;
	}

	void onMouseConnected_impl(core::smart_refctd_ptr<IMouseEventChannel>&& mch) override
	{
		m_logger.log("A mouse %p has been connected", system::ILogger::ELL_INFO, mch);
		m_inputSystem.get()->add(m_inputSystem.get()->m_mouse,std::move(mch));
	}
	void onMouseDisconnected_impl(IMouseEventChannel* mch) override
	{
		m_logger.log("A mouse %p has been disconnected", system::ILogger::ELL_INFO, mch);
		m_inputSystem.get()->remove(m_inputSystem.get()->m_mouse,mch);
	}
	void onKeyboardConnected_impl(core::smart_refctd_ptr<IKeyboardEventChannel>&& kbch) override
	{
		m_logger.log("A keyboard %p has been connected", system::ILogger::ELL_INFO, kbch);
		m_inputSystem.get()->add(m_inputSystem.get()->m_keyboard,std::move(kbch));
	}
	void onKeyboardDisconnected_impl(IKeyboardEventChannel* kbch) override
	{
		m_logger.log("A keyboard %p has been disconnected", system::ILogger::ELL_INFO, kbch);
		m_inputSystem.get()->remove(m_inputSystem.get()->m_keyboard,kbch);
	}

private:
	core::smart_refctd_ptr<InputSystem> m_inputSystem;
	system::logger_opt_smart_ptr m_logger;
	bool m_gotWindowClosedMsg;
};

int main(int argc, char** argv)
{
	IApplicationFramework::GlobalsInit();
	const path CWD = path(argv[0]).parent_path().generic_string() + "/";
	const path mediaWD = CWD.generic_string() + "../../media/";

	auto system = CommonAPI::createSystem();
	// TODO: system->deleteFile("log.txt");

	core::smart_refctd_ptr<system::ILogger> logger;
	{
		system::ISystem::future_t<smart_refctd_ptr<system::IFile>> future;
		system->createFile(future, CWD/"log.txt", nbl::system::IFile::ECF_READ_WRITE);
		if (future.wait())
			logger = core::make_smart_refctd_ptr<system::CFileLogger>(future.copy(), false);
	}

	auto assetManager = core::make_smart_refctd_ptr<IAssetManager>(smart_refctd_ptr(system));

	auto winManager = IWindowManagerWin32::create();
	

	IWindow::SCreationParams params;
	params.callback = nullptr;
	params.width = 720;
	params.height = 480;
	params.x = 500;
	params.y = 300;
	params.flags = IWindow::ECF_NONE;
	params.windowCaption = "Test Window";

	auto input = make_smart_refctd_ptr<InputSystem>(system::logger_opt_smart_ptr(smart_refctd_ptr(logger)));
	auto windowCb = make_smart_refctd_ptr<WindowEventCallback>(core::smart_refctd_ptr(input),system::logger_opt_smart_ptr(smart_refctd_ptr(logger)));
	params.callback = windowCb;
	// *********************************
	auto window = winManager->createWindow(std::move(params));
	auto* cursorControl = window->getCursorControl();

	ISystem::future_t<smart_refctd_ptr<system::IFile>> future;
	system->createFile(future, CWD/"testFile.txt", core::bitflag(nbl::system::IFile::ECF_READ_WRITE)/*Growing mappable files are a TODO |IFile::ECF_MAPPABLE*/);
	if (auto pFile = future.acquire())
	{
		auto& file = *pFile;
		const std::string fileData = "Test file data!";

		system::IFile::success_t writeSuccess;
		file->write(writeSuccess, fileData.data(), 0, fileData.length());
		{
			const bool success = bool(writeSuccess);
			assert(success);
		}

		std::string readStr(fileData.length(), '\0');
		system::IFile::success_t readSuccess;
		file->read(readSuccess, readStr.data(), 0, readStr.length());
		{
			const bool success = bool(readSuccess);
			assert(success);
		}
		assert(readStr == fileData);
	}
	else
	{
		assert(false);
	}

	//! builtin resources archive test
	#ifdef _BR_TEST_
	// Nabla case
	{
		nbl::system::ISystem::future_t<core::smart_refctd_ptr<IFile>> future;
		system->createFile(future, "nbl/builtin/glsl/utils/acceleration_structures.glsl", core::bitflag(IFileBase::ECF_READ));
		if (auto pFile = future.acquire())
		{
			auto& file = *pFile;

			const size_t fileSize = file->getSize();
			std::string readStr(fileSize, '\0');
			system::IFile::success_t readSuccess;
			file->read(readSuccess, readStr.data(), 0, readStr.length());
			{
				const bool success = bool(readSuccess);
				assert(success);
			}

			const auto* testStream = readStr.c_str();
			std::cout << testStream << "\n\n\n\n\n===================================================================\n\n\n\n\n";
		}
	}
	// Custom case
	{
		#ifdef _NBL_SHARED_BUILD_
		{
			const auto brOutputDLLAbsoluteDirectory = std::filesystem::absolute(std::filesystem::path(_BR_DLL_DIRECTORY_)).string();
			const HRESULT brLoad = nbl::system::CSystemWin32::delayLoadDLL(_BR_DLL_NAME_, { brOutputDLLAbsoluteDirectory.c_str(), "" });

			assert(SUCCEEDED(brLoad));
		};
		#endif

		nbl::core::smart_refctd_ptr<yourNamespace::builtin::CArchive> archive = core::make_smart_refctd_ptr<yourNamespace::builtin::CArchive>(core::smart_refctd_ptr(logger));
		system->mount(core::smart_refctd_ptr(archive));

		// archive path test
		{
			nbl::system::ISystem::future_t<core::smart_refctd_ptr<IFile>> future;
			system->createFile(future, "dir/data/test.txt", core::bitflag(IFileBase::ECF_READ));
			if (auto pFile = future.acquire())
			{
				auto& file = *pFile;

				const size_t fileSize = file->getSize();
				std::string readStr(fileSize, '\0');
				system::IFile::success_t readSuccess;
				file->read(readSuccess, readStr.data(), 0, readStr.length());
				{
					const bool success = bool(readSuccess);
					assert(success);
				}

				const auto* testStream = readStr.c_str();
				std::cout << testStream << "\n\n\n\n\n===================================================================\n\n\n\n\n";
			}
		}

		// archive alias test
		{
			nbl::core::smart_refctd_ptr<system::IFile> testFile = archive->getFile("aliasTest1", ""); // alias to dir/data/test.txt

			const size_t fileSize = testFile->getSize();
			std::string readStr(fileSize, '\0');
			system::IFile::success_t readSuccess;
			testFile->read(readSuccess, readStr.data(), 0, readStr.length());
			{
				const bool success = bool(readSuccess);
				assert(success);
			}
			const auto* testStream = readStr.c_str();
			std::cout << testStream << "\n\n\n\n\n===================================================================\n\n\n\n\n";
		}
	}
	#endif // _BR_TEST_

	// polling for events!
	InputSystem::ChannelReader<IMouseEventChannel> mouse;
	InputSystem::ChannelReader<IKeyboardEventChannel> keyboard;
	auto mouseProcess = [logger](const IMouseEventChannel::range_t& events, const core::smart_refctd_ptr<IMouseEventChannel>& ch) -> void
	{
		for (auto eventIt=events.begin(); eventIt!=events.end(); eventIt++)
		{
			switch ((*eventIt).type)
			{
			case SMouseEvent::EET_MOVEMENT:
			{
				logger->log("Mouse movement (%d, %d) from device %p at %u us", system::ILogger::ELL_INFO, (*eventIt).movementEvent.relativeMovementX, (*eventIt).movementEvent.relativeMovementY, ch, (*eventIt).timeStamp);
				break;
			}
			case SMouseEvent::EET_CLICK:
			{
				if((*eventIt).clickEvent.action == SMouseEvent::SClickEvent::EA_PRESSED)
					logger->log("Mouse click press at (%d, %d) from device %p at %u us", system::ILogger::ELL_INFO, (*eventIt).clickEvent.clickPosX, (*eventIt).clickEvent.clickPosY, ch, (*eventIt).timeStamp);
				else if ((*eventIt).clickEvent.action == SMouseEvent::SClickEvent::EA_RELEASED)
					logger->log("Mouse click release at (%d, %d) from device %p at %u us", system::ILogger::ELL_INFO, (*eventIt).clickEvent.clickPosX, (*eventIt).clickEvent.clickPosY, ch, (*eventIt).timeStamp);
				break;
			}
			case SMouseEvent::EET_SCROLL:
			{
				logger->log("Mouse scroll (vertical: %d, horizontal: %d) from device %p at %u us", system::ILogger::ELL_INFO, (*eventIt).scrollEvent.verticalScroll, (*eventIt).scrollEvent.horizontalScroll, ch, (*eventIt).timeStamp);
				break;
			}
			}
		}
	};
	auto keyboardProcess = [logger](const IKeyboardEventChannel::range_t& events, const core::smart_refctd_ptr<IKeyboardEventChannel>& ch) -> void
	{
		for (auto eventIt=events.begin(); eventIt!=events.end(); eventIt++)
		{
			if ((*eventIt).action == SKeyboardEvent::ECA_PRESSED)
			{
				logger->log("Keyboard key \"%c\" pressed from device % p", system::ILogger::ELL_INFO, (*eventIt).keyCode, ch);
			}
			else if ((*eventIt).action == SKeyboardEvent::ECA_RELEASED)
			{
				logger->log("Keyboard key \"%c\" released from device % p", system::ILogger::ELL_INFO, (*eventIt).keyCode, ch);
			}
		}
	};

	IAssetLoader::SAssetLoadParams lp;
	lp.workingDirectory = mediaWD;
	//PNG loader test
	{
		auto asset = assetManager->getAsset("Cerberus_by_Andrew_Maximov/Textures/Cerberus_H.png", lp);
		assert(!asset.getContents().empty());
		auto cpuImage = IAsset::castDown<ICPUImage>(asset.getContents()[0]);
		core::smart_refctd_ptr<ICPUImageView> imageView;

		ICPUImageView::SCreationParams imgViewParams;
		imgViewParams.flags = static_cast<ICPUImageView::E_CREATE_FLAGS>(0u);
		imgViewParams.format = cpuImage->getCreationParameters().format;
		imgViewParams.image = core::smart_refctd_ptr<ICPUImage>(cpuImage);
		imgViewParams.viewType = ICPUImageView::ET_2D;
		imgViewParams.subresourceRange = { static_cast<IImage::E_ASPECT_FLAGS>(0u),0u,1u,0u,1u };
		imageView = ICPUImageView::create(std::move(imgViewParams));

		IAssetWriter::SAssetWriteParams wp(imageView.get());
		wp.workingDirectory = CWD;
		assetManager->writeAsset("pngWriteSuccessful.png", wp);
	}
	//TODO OBJ loader test 
	{
		//auto bundle = assetManager->getAsset("../../media/sponza.obj", lp);
		//assert(!bundle.getContents().empty());
		//auto cpumesh = bundle.getContents().begin()[0];
		//auto cpumesh_raw = static_cast<ICPUMesh*>(cpumesh.get());
		//
		//IAssetWriter::SAssetWriteParams wp(cpumesh.get());
		//assetManager->writeAsset("objWriteSuccessful.obj", wp);
	}
	//JPEG loader test
	{
		auto asset = assetManager->getAsset("dwarf.jpg", lp);
		assert(!asset.getContents().empty());
		auto cpuImage = IAsset::castDown<ICPUImage>(asset.getContents()[0]);
		core::smart_refctd_ptr<ICPUImageView> imageView;
		ICPUImageView::SCreationParams imgViewParams;
		imgViewParams.flags = static_cast<ICPUImageView::E_CREATE_FLAGS>(0u);
		imgViewParams.format = E_FORMAT::EF_R8G8B8_SRGB;
		imgViewParams.image = core::smart_refctd_ptr<ICPUImage>(cpuImage);
		imgViewParams.viewType = ICPUImageView::ET_2D;
		imgViewParams.subresourceRange = { static_cast<IImage::E_ASPECT_FLAGS>(0u),0u,1u,0u,1u };
		imageView = ICPUImageView::create(std::move(imgViewParams));

		IAssetWriter::SAssetWriteParams wp(imageView.get());
		wp.workingDirectory = CWD;

		assetManager->writeAsset("jpgWriteSuccessful.jpg", wp);
	}
	
	
	auto bigarch = system->openFileArchive(CWD/"../../media/sponza.zip");
	system->mount(std::move(bigarch), "sponza");

	system->copy(CWD/"pngWriteSuccessful.png", CWD/"pngCopy.png");
	
	system->createDirectory(CWD/"textures1");
	system->copy(CWD/"textures", CWD/"textures1");
	system->copy("sponza/textures", CWD/"textures");

	const auto items = system->listItemsInDirectory(CWD/"textures");
	for (const auto& item : items)
		logger->log("%s",system::ILogger::ELL_DEBUG,item.c_str());

/* TODO: Tart Archive reader test
	system->moveFileOrDirectory("file.tar","movedFile.tar");
	{
		system::future<smart_refctd_ptr<IFile>> fut;
		system->createFile(fut, "tarArch/file.txt", IFile::ECF_READ);
		auto file = fut.get();
		{
			system::future<smart_refctd_ptr<IFile>> fut;
			system->createFile(fut, "tarArch/file.txt", IFile::ECF_READ);
			file = fut.get();
		}
		std::string str(5, '\0');
		system::future<size_t> readFut;
		file->read(readFut, str.data(), 0, 5);
		readFut.get();
		std::cout << str << std::endl;
	}
*/

	while (windowCb->isWindowOpen())
	{
		input->getDefaultMouse(&mouse);
		input->getDefaultKeyboard(&keyboard);

		mouse.consumeEvents(mouseProcess,logger.get());
		keyboard.consumeEvents(keyboardProcess,logger.get());
	}
}
