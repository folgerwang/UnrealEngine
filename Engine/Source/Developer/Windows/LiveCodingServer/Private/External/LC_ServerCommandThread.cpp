// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_ServerCommandThread.h"
#include "LC_Commands.h"
#include "LC_Telemetry.h"
#include "LC_Symbols.h"
#include "LC_FileUtil.h"
#include "LC_Process.h"
#include "LC_Compiler.h"
#include "LC_StringUtil.h"
#include "LC_CommandMap.h"
#include "LC_FileAttributeCache.h"
#include "LiveCodingServer.h"
#include "LC_Shortcut.h"
#include "LC_Key.h"
#include "LC_ChangeNotification.h"
#include "LC_DirectoryCache.h"
#include "LC_VirtualDrive.h"
#include "LC_LiveModule.h"
#include "LC_LiveProcess.h"
#include "LC_CodeCave.h"
#include "LC_PrimitiveNames.h"
#include "LC_AppSettings.h"
#include "LC_Allocators.h"
#include "LC_DuplexPipeClient.h"
#include <mmsystem.h>

// unreachable code
#pragma warning (disable : 4702)

// BEGIN EPIC MODS
#pragma warning(push)
#pragma warning(disable:6031) // warning C6031: Return value ignored: 'CoInitialize'.
// END EPIC MODS

namespace
{
	static telemetry::Accumulator g_loadedModuleSize("Module size");

	struct InitializeCOM
	{
		InitializeCOM(void)
		{
			::CoInitialize(NULL);
		}

		~InitializeCOM(void)
		{
			::CoUninitialize();
		}
	};


	static void AddVirtualDrive(void)
	{
		const std::wstring virtualDriveLetter = appSettings::g_virtualDriveLetter->GetValue();
		const std::wstring virtualDrivePath = appSettings::g_virtualDrivePath->GetValue();
		if ((virtualDriveLetter.size() != 0) && (virtualDrivePath.size() != 0))
		{
			virtualDrive::Add(virtualDriveLetter.c_str(), virtualDrivePath.c_str());
		}
	}

	static void RemoveVirtualDrive(void)
	{
		const std::wstring virtualDriveLetter = appSettings::g_virtualDriveLetter->GetValue();
		const std::wstring virtualDrivePath = appSettings::g_virtualDrivePath->GetValue();
		if ((virtualDriveLetter.size() != 0) && (virtualDrivePath.size() != 0))
		{
			virtualDrive::Remove(virtualDriveLetter.c_str(), virtualDrivePath.c_str());
		}
	}

	static executable::Header GetImageHeader(const wchar_t* path)
	{
		executable::Image* image = executable::OpenImage(path, file::OpenMode::READ_ONLY);
		if (!image)
		{
			return executable::Header {};
		}

		const executable::Header& imageHeader = executable::GetHeader(image);
		executable::CloseImage(image);

		return imageHeader;
	}
}


ServerCommandThread::ServerCommandThread(MainFrame* mainFrame, const wchar_t* const processGroupName, RunMode::Enum runMode)
	: m_processGroupName(processGroupName)
	, m_runMode(runMode)
	, m_mainFrame(mainFrame)
	, m_serverThread()
	, m_compileThread()
	, m_liveModules()
	, m_liveProcesses()
	, m_imageHeaderToLiveModule()
	, m_actionCS()
	, m_exceptionCS()
	, m_inExceptionHandlerEvent(nullptr, Event::Type::MANUAL_RESET)
	, m_handleCommandsEvent(nullptr, Event::Type::MANUAL_RESET)
	, m_directoryCache(new DirectoryCache(2048u))
	, m_connectionCS()
	, m_commandThreads()
	, m_moduleBatchScope("Module loading")
	, m_loadedCompilandCountInBatchScope(0u)
	, m_manualRecompileTriggered(false)
	, m_liveModuleToModifiedOrNewObjFiles()
{
	m_serverThread = thread::Create(64u * 1024u, &ServerThreadProxy, this);
	m_compileThread = thread::Create(64u * 1024u, &CompileThreadProxy, this);

	m_liveModules.reserve(256u);
	m_liveProcesses.reserve(8u);
	m_imageHeaderToLiveModule.reserve(256u);

	m_commandThreads.reserve(8u);
}


ServerCommandThread::~ServerCommandThread(void)
{
	// note that we deliberately do *nothing* here.
	// this is only called when Live++ is being torn down anyway, so we leave cleanup to the OS.
	// otherwise we could run into races when trying to terminate the thread that might currently be doing
	// some intensive work.
}


std::wstring ServerCommandThread::GetProcessImagePath(void) const
{
	// there must be at least one registered process.
	// in case the EXE was erroneously started directly, no process will be registered.
	// handle this case gracefully.
	if (m_liveProcesses.size() == 0u)
	{
		return L"Unknown";
	}

	return process::GetImagePath(m_liveProcesses[0]->GetProcessHandle());
}


void ServerCommandThread::LoadModule(const wchar_t* givenModulePath, const DuplexPipe* pipe, TaskContext* tasks, unsigned int processId)
{
	// EPIC REMOVED: g_theApp.GetMainFrame()->ChangeStatusBarText(L"Loading modules...");

	const std::wstring& modulePath = file::NormalizePath(givenModulePath);
	const executable::Header imageHeader = GetImageHeader(modulePath.c_str());
	if (!executable::IsValidHeader(imageHeader))
	{
		return;
	}

	LiveProcess* liveProcess = FindProcessById(processId);
	LC_ASSERT(liveProcess, "Invalid process ID.");

	if (liveProcess->TriedToLoadImage(imageHeader))
	{
		// tried loading this module into this process already
		return;
	}

	{
		CommandMap commandMap;
		commandMap.RegisterAction<GetModuleInfoAction>();

		// defer loading of the module to make sure that we get the correct module base address,
		// no matter if .exe or .dll.
		{
			commands::GetModule cmd = {};
			cmd.loadImports = false;
			cmd.taskContext = tasks;
			wcscpy_s(cmd.path, modulePath.c_str());
			pipe->SendCommandAndWaitForAck(cmd);
		}

		// handle commands that return module info
		commandMap.HandleCommands(pipe, this);
	}

	liveProcess->AddLoadedImage(imageHeader);
}


void ServerCommandThread::LoadAllModules(const wchar_t* givenModulePath, const DuplexPipe* pipe, TaskContext* tasks, unsigned int processId)
{
	// EPIC REMOVED: g_theApp.GetMainFrame()->ChangeStatusBarText(L"Loading modules...");

	const std::wstring& modulePath = file::NormalizePath(givenModulePath);
	const executable::Header imageHeader = GetImageHeader(modulePath.c_str());
	if (!executable::IsValidHeader(imageHeader))
	{
		return;
	}

	LiveProcess* liveProcess = FindProcessById(processId);
	LC_ASSERT(liveProcess, "Invalid process ID.");

	if (liveProcess->TriedToLoadImage(imageHeader))
	{
		// tried loading this module into this process already
		return;
	}

	symbols::Provider* provider = symbols::OpenEXE(modulePath.c_str(), symbols::OpenOptions::ACCUMULATE_SIZE);
	if (!provider)
	{
		liveProcess->AddLoadedImage(imageHeader);
		return;
	}

	// grab DIA compilands first. this is very fast, and needed in order to gather modules next
	symbols::DiaCompilandDB* diaCompilandDb = symbols::GatherDiaCompilands(provider);
	symbols::ModuleDB* moduleDB = symbols::GatherModules(diaCompilandDb);

	// now that we have a list of modules, load them all concurrently, starting with the main executable, followed
	// by all DLLs.
	{
		CommandMap commandMap;
		commandMap.RegisterAction<GetModuleInfoAction>();
		{
			commands::GetModule cmd = {};
			cmd.loadImports = false;
			cmd.taskContext = tasks;
			wcscpy_s(cmd.path, modulePath.c_str());
			pipe->SendCommandAndWaitForAck(cmd);
		}

		commandMap.HandleCommands(pipe, this);

		const size_t count = moduleDB->modules.size();
		for (size_t i = 0u; i < count; ++i)
		{
			const std::wstring& path = moduleDB->modules[i];

			// all we have is a relative path to the DLL. get the full path from the modules loaded into the main process
			{
				// because DLLs might also have import DLLs, load all those as well
				commands::GetModule cmd = {};
				cmd.loadImports = true;
				cmd.taskContext = tasks;
				wcscpy_s(cmd.path, path.c_str());
				pipe->SendCommandAndWaitForAck(cmd);
			}

			// handle commands that return module info
			commandMap.HandleCommands(pipe, this);
		}
	}

	symbols::DestroyDiaCompilandDB(diaCompilandDb);
	symbols::DestroyModuleDB(moduleDB);

	symbols::Close(provider);

	liveProcess->AddLoadedImage(imageHeader);
}


void ServerCommandThread::UnloadModule(const wchar_t* givenModulePath, const DuplexPipe* pipe, unsigned int processId)
{
	// EPIC REMOVED: g_theApp.GetMainFrame()->ChangeStatusBarText(L"Unloading modules...");

	const std::wstring& modulePath = file::NormalizePath(givenModulePath);
	const executable::Header imageHeader = GetImageHeader(modulePath.c_str());
	if (!executable::IsValidHeader(imageHeader))
	{
		return;
	}

	LiveProcess* liveProcess = FindProcessById(processId);
	LC_ASSERT(liveProcess, "Invalid process ID.");

	if (!liveProcess->TriedToLoadImage(imageHeader))
	{
		// this module was never loaded
		return;
	}

	{
		CommandMap commandMap;
		commandMap.RegisterAction<GetModuleInfoAction>();

		// defer unloading of the module to make sure that we get the correct module base address,
		// no matter if .exe or .dll.
		{
			commands::GetModule cmd = {};
			cmd.loadImports = false;
			cmd.taskContext = nullptr;
			wcscpy_s(cmd.path, modulePath.c_str());
			pipe->SendCommandAndWaitForAck(cmd);
		}

		// handle commands that return module info
		commandMap.HandleCommands(pipe, this);
	}

	liveProcess->RemoveLoadedImage(imageHeader);
}


void ServerCommandThread::UnloadAllModules(const wchar_t* givenModulePath, const DuplexPipe* pipe, unsigned int processId)
{
	// EPIC REMOVED: g_theApp.GetMainFrame()->ChangeStatusBarText(L"Unloading modules...");

	const std::wstring& modulePath = file::NormalizePath(givenModulePath);
	const executable::Header imageHeader = GetImageHeader(modulePath.c_str());
	if (!executable::IsValidHeader(imageHeader))
	{
		return;
	}

	LiveProcess* liveProcess = FindProcessById(processId);
	LC_ASSERT(liveProcess, "Invalid process ID.");

	if (!liveProcess->TriedToLoadImage(imageHeader))
	{
		// this module was never loaded
		return;
	}

	symbols::Provider* provider = symbols::OpenEXE(modulePath.c_str(), symbols::OpenOptions::ACCUMULATE_SIZE);
	if (!provider)
	{
		liveProcess->RemoveLoadedImage(imageHeader);
		return;
	}

	// grab DIA compilands first. this is very fast, and needed in order to gather modules next
	symbols::DiaCompilandDB* diaCompilandDb = symbols::GatherDiaCompilands(provider);
	symbols::ModuleDB* moduleDB = symbols::GatherModules(diaCompilandDb);

	// now that we have a list of modules, load them all concurrently, starting with the main executable, followed
	// by all DLLs.
	{
		CommandMap commandMap;
		commandMap.RegisterAction<GetModuleInfoAction>();
		{
			commands::GetModule cmd = {};
			cmd.loadImports = false;
			cmd.taskContext = nullptr;
			wcscpy_s(cmd.path, modulePath.c_str());
			pipe->SendCommandAndWaitForAck(cmd);
		}

		commandMap.HandleCommands(pipe, this);

		const size_t count = moduleDB->modules.size();
		for (size_t i = 0u; i < count; ++i)
		{
			const std::wstring& path = moduleDB->modules[i];

			// all we have is a relative path to the DLL. get the full path from the modules loaded into the main process
			{
				// because DLLs might also have import DLLs, load all those as well
				commands::GetModule cmd = {};
				cmd.loadImports = true;
				cmd.taskContext = nullptr;
				wcscpy_s(cmd.path, path.c_str());
				pipe->SendCommandAndWaitForAck(cmd);
			}

			// handle commands that return module info
			commandMap.HandleCommands(pipe, this);
		}
	}

	symbols::DestroyDiaCompilandDB(diaCompilandDb);
	symbols::DestroyModuleDB(moduleDB);

	symbols::Close(provider);

	liveProcess->RemoveLoadedImage(imageHeader);
}


void ServerCommandThread::PrewarmCompilerEnvironmentCache(void)
{
	// EPIC REMOVED: g_theApp.GetMainFrame()->ChangeStatusBarText(L"Prewarming compiler/linker environment cache...");

	telemetry::Scope scope("Prewarming compiler/linker environment cache");

	// fetch unique compiler and linker paths from all modules
	types::StringSet uniquePaths;

	// compiler and linker paths can be overridden, so we need to make sure that we pre-warm the
	// cache for all compilers and linkers involved, depending on the UI settings.
	// there are 3 options:
	// - the path is not overridden: fetch only the paths from the compilands
	// - the paths are overridden, but only used as fallback: fetch the paths from the compilands
	//   as well as the overridden ones. we might need both, depending on which file we compile
	// - the paths are overridden, and always used: fetch only the overridden paths, we're only using those

	// fetch all compiler paths involved.
	// the compiler is only used in default mode, NOT when using an external build system.
	const bool useCompilerEnvironment = appSettings::g_useCompilerEnvironment->GetValue();
	if (useCompilerEnvironment && (m_runMode == RunMode::DEFAULT))
	{
		const std::wstring overriddenPath = appSettings::GetCompilerPath();
		const bool useOverriddenPathAsFallback = appSettings::g_useCompilerOverrideAsFallback->GetValue();

		// always prewarm for overridden compiler path if it is available
		const bool prewarmOverridenPath = (overriddenPath.length() != 0u);

		const bool prewarmCompilandCompilerPath = prewarmOverridenPath
			? useOverriddenPathAsFallback			// overridden path is set. only prewarm compiland compiler paths if the override is only used as fallback
			: true;									// no override is set, always prewarm

		if (prewarmCompilandCompilerPath)
		{
			const size_t count = m_liveModules.size();
			for (size_t i = 0u; i < count; ++i)
			{
				const LiveModule* liveModule = m_liveModules[i];

				const symbols::CompilandDB* compilandDB = liveModule->GetCompilandDatabase();
				for (auto it = compilandDB->compilands.begin(); it != compilandDB->compilands.end(); ++it)
				{
					const symbols::Compiland* compiland = it->second;
					LC_ASSERT(compiland->compilerPath.c_str(), "Invalid compiler path.");

					if (compiland->compilerPath.GetLength() != 0u)
					{
						uniquePaths.insert(compiland->compilerPath);
					}
					else
					{
						LC_WARNING_USER("Not prewarming environment cache for empty compiler in module %S", liveModule->GetModuleName().c_str());
					}
				}
			}
		}

		if (prewarmOverridenPath)
		{
			uniquePaths.insert(string::ToUtf8String(overriddenPath));
		}
	}

	// fetch all linker paths involved
	const bool useLinkerEnvironment = appSettings::g_useLinkerEnvironment->GetValue();
	if (useLinkerEnvironment)
	{
		const std::wstring overriddenPath = appSettings::GetLinkerPath();
		const bool useOverriddenPathAsFallback = appSettings::g_useLinkerOverrideAsFallback->GetValue();

		// always prewarm for overridden linker path if it is available
		const bool prewarmOverridenPath = (overriddenPath.length() != 0u);

		const bool prewarmLinkerPath = prewarmOverridenPath
			? useOverriddenPathAsFallback			// overridden path is set. only prewarm linker paths if the override is only used as fallback
			: true;									// no override is set, always prewarm

		if (prewarmLinkerPath)
		{
			const size_t count = m_liveModules.size();
			for (size_t i = 0u; i < count; ++i)
			{
				const LiveModule* liveModule = m_liveModules[i];

				const symbols::LinkerDB* linkerDB = liveModule->GetLinkerDatabase();
				if (linkerDB->linkerPath.GetLength() != 0u)
				{
					uniquePaths.insert(linkerDB->linkerPath);
				}
				else
				{
					LC_WARNING_USER("Not prewarming environment cache for empty linker in module %S", liveModule->GetModuleName().c_str());
				}
			}
		}

		if (prewarmOverridenPath)
		{
			uniquePaths.insert(string::ToUtf8String(overriddenPath));
		}
	}

	// grab environment blocks for all unique compilers/linkers concurrently
	auto taskRoot = scheduler::CreateEmptyTask();

	types::vector<scheduler::TaskBase*> tasks;
	tasks.reserve(uniquePaths.size());

	for (auto it = uniquePaths.begin(); it != uniquePaths.end(); ++it)
	{
		auto task = scheduler::CreateTask(taskRoot, [it]()
		{
			const ImmutableString& path = *it;
			compiler::UpdateEnvironmentCache(string::ToWideString(path).c_str());

			return true;
		});
		scheduler::RunTask(task);

		tasks.emplace_back(task);
	}

	// wait for all tasks to end
	scheduler::RunTask(taskRoot);
	scheduler::WaitForTask(taskRoot);

	// destroy all tasks
	scheduler::DestroyTasks(tasks);
	scheduler::DestroyTask(taskRoot);

	LC_SUCCESS_USER("Prewarmed compiler/linker environment cache (%.3fs, %zu)", scope.ReadSeconds(), uniquePaths.size());
}


unsigned int __stdcall ServerCommandThread::ServerThreadProxy(void* context)
{
	thread::SetName("Live coding server");

	ServerCommandThread* instance = static_cast<ServerCommandThread*>(context);
	return instance->ServerThread();
}


unsigned int ServerCommandThread::ServerThread(void)
{
	InitializeCOM initCOM;

	// inter process event for telling client that server is ready
	Event serverReadyEvent(primitiveNames::ServerReadyEvent(m_processGroupName).c_str(), Event::Type::AUTO_RESET);

	// run separate pipe servers for all incoming connections
	for (;;)
	{
		CommandThreadContext* context = new CommandThreadContext;
		context->instance = this;

		context->pipe.Create(primitiveNames::Pipe(m_processGroupName).c_str());
		context->exceptionPipe.Create(primitiveNames::ExceptionPipe(m_processGroupName).c_str());

		context->readyEvent = new Event(nullptr, Event::Type::AUTO_RESET);

		// tell other processes that a new server is ready
		serverReadyEvent.Signal();

		// wait until any client connects, blocking
		context->pipe.WaitForClient();
		context->exceptionPipe.WaitForClient();

		// a new client has connected, open a new thread for communication
		context->commandThread = thread::Create(64u * 1024u, &CommandThreadProxy, context);
		context->exceptionCommandThread = thread::Create(64u * 1024u, &ExceptionCommandThreadProxy, context);

		// register this connection
		{
			CriticalSection::ScopedLock lock(&m_connectionCS);
			m_commandThreads.push_back(context);
		}
	}

	return 0u;
}


unsigned int __stdcall ServerCommandThread::CompileThreadProxy(void* context)
{
	thread::SetName("Live coding compilation");

	ServerCommandThread* instance = static_cast<ServerCommandThread*>(context);
	return instance->CompileThread();
}

// BEGIN EPIC MOD - Focus application windows on patch complete
BOOL CALLBACK FocusApplicationWindows(HWND WindowHandle, LPARAM Lparam)
{
	DWORD WindowProcessId;
    GetWindowThreadProcessId(WindowHandle, &WindowProcessId);

	const types::vector<LiveProcess*>& Processes = *(const types::vector<LiveProcess*>*)Lparam;
	for (LiveProcess* Process : Processes)
	{
		if (Process->GetProcessId() == WindowProcessId && IsWindowVisible(WindowHandle))
		{
			SetForegroundWindow(WindowHandle);
		}
	}
    return Windows::TRUE;
}
// END EPIC MOD

// BEGIN EPIC MOD - Support for lazy-loading modules
bool ServerCommandThread::FinishedLazyLoadingModulesAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{ 
	pipe->SendAck(); 
	return false; 
}

struct ClientProxyThread
{
	struct ProxyGetModuleAction
	{
		typedef commands::GetModule CommandType;

		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context)
		{
			pipe->SendAck();

			LiveProcess* process = static_cast<LiveProcess*>(context);

			commands::GetModuleInfo cmd;
			cmd.moduleBase = process->GetLazyLoadedModuleBase(command->path);
			cmd.processId = process->GetProcessId();
			cmd.loadImports = command->loadImports;
			cmd.taskContext = command->taskContext;
			wcscpy_s(cmd.path, command->path);
			pipe->SendCommandAndWaitForAck(cmd);

			return true;
		}
	};

	struct ProxyEnableModuleFinishedAction
	{
		typedef commands::EnableModuleFinished CommandType;

		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context)
		{
			pipe->SendAck();
			return false;
		}
	};

	LiveProcess* m_process;
	DuplexPipeClient* m_pipe;
	std::vector<std::wstring> m_enableModules;
	thread::Handle m_threadHandle;

	ClientProxyThread(LiveProcess* process, DuplexPipeClient* pipe, const std::vector<std::wstring> enableModules)
		: m_process(process)
		, m_pipe(pipe)
		, m_enableModules(enableModules)
	{
		m_threadHandle = thread::Create(64u * 1024u, &StaticEntryPoint, this);
		thread::SetName("Live coding client proxy");
	}

	~ClientProxyThread()
	{
		thread::Join(m_threadHandle);
		thread::Close(m_threadHandle);
	}

	static unsigned int __stdcall StaticEntryPoint(void* context)
	{
		static_cast<ClientProxyThread*>(context)->EntryPoint();
		return 0;
	}

	void EntryPoint()
	{
		m_pipe->SendCommandAndWaitForAck(commands::EnableModuleBatchBegin());
		for(const std::wstring& enableModule : m_enableModules)
		{
			commands::EnableModule enableModuleCommand;
			enableModuleCommand.processId = m_process->GetProcessId();
			wcscpy_s(enableModuleCommand.path, enableModule.c_str());
			enableModuleCommand.token = nullptr;
			m_pipe->SendCommandAndWaitForAck(enableModuleCommand);

			CommandMap commandMap;
			commandMap.RegisterAction<ProxyGetModuleAction>();
			commandMap.RegisterAction<ProxyEnableModuleFinishedAction>();
			commandMap.HandleCommands(m_pipe, m_process);
		}
		m_pipe->SendCommandAndWaitForAck(commands::EnableModuleBatchEnd());
		m_pipe->SendCommandAndWaitForAck(commands::FinishedLazyLoadingModules());
	}
};
// END EPIC MOD

void ServerCommandThread::CompileChanges(bool didAllProcessesMakeProgress)
{
	// recompile files, if any
	// EPIC REMOVED: g_theApp.GetMainFrame()->SetBusy(true);
	// EPIC REMOVED: g_theApp.GetMainFrame()->ChangeStatusBarText(L"Creating patch...");

	telemetry::Scope scope("Creating patch");

	// EPIC REMOVED: g_theApp.GetMainFrame()->OnCompilationStart();

	LC_LOG_USER("---------- Creating patch ----------");

	// BEGIN EPIC MOD - Hook for the compiler
	GLiveCodingServer->GetCompileStartedDelegate().ExecuteIfBound();

	const ILiveCodingServer::FCompileDelegate& CompileDelegate = GLiveCodingServer->GetCompileDelegate();
	if (CompileDelegate.IsBound())
	{
		// Get the list of arguments for building each target, and use the delegate to pass them to UBT
		TArray<FString> Targets;
		for (LiveProcess* liveProcess : m_liveProcesses)
		{
			Targets.Add(liveProcess->GetBuildArguments());
		}

		GLiveCodingServer->GetStatusChangeDelegate().ExecuteIfBound(L"Compiling changes for live coding...");

		TMap<FString, TArray<FString>> ModuleToObjectFiles;
		if (!CompileDelegate.Execute(Targets, ModuleToObjectFiles))
		{
			GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Error, L"Compilation error.");
			return;
		}

		// Enable any lazy-loaded modules that we need
		for (LiveProcess* liveProcess : m_liveProcesses)
		{
			types::vector<std::wstring> LoadModuleFileNames;
			for(const TPair<FString, TArray<FString>>& Pair : ModuleToObjectFiles)
			{
				std::wstring ModuleFileName = file::NormalizePath(*Pair.Key);
				if (liveProcess->IsPendingLazyLoadedModule(ModuleFileName))
				{
					LoadModuleFileNames.push_back(ModuleFileName);
				}
			}
			if (LoadModuleFileNames.size() > 0)
			{
				const std::wstring PipeName = primitiveNames::Pipe(m_processGroupName + L"_ClientProxy");

				DuplexPipeServer ServerPipe;
				ServerPipe.Create(PipeName.c_str());

				DuplexPipeClient ClientPipe;
				ClientPipe.Connect(PipeName.c_str());

				ClientProxyThread ClientThread(liveProcess, &ClientPipe, LoadModuleFileNames);

				CommandMap commandMap;
				commandMap.RegisterAction<EnableModuleBatchBeginAction>();
				commandMap.RegisterAction<EnableModuleBatchEndAction>();
				commandMap.RegisterAction<EnableModuleAction>();
				commandMap.RegisterAction<FinishedLazyLoadingModulesAction>();
				commandMap.HandleCommands(&ServerPipe, this);

				for (const std::wstring& loadModuleFileName : LoadModuleFileNames)
				{
					liveProcess->SetLazyLoadedModuleAsLoaded(loadModuleFileName);
				}
			}
		}

		// Build up a list of all the modified object files in each module
		types::unordered_set<std::wstring> ValidModuleFileNames;
		for (const LiveModule* liveModule : m_liveModules)
		{
			ValidModuleFileNames.insert(liveModule->GetModuleName());
		}

		for(const TPair<FString, TArray<FString>>& Pair : ModuleToObjectFiles)
		{
			std::wstring ModuleFileName = file::NormalizePath(*Pair.Key);
			if(ValidModuleFileNames.find(ModuleFileName) == ValidModuleFileNames.end())
			{
				std::wstring ModuleName = file::GetFilename(ModuleFileName);
				LC_ERROR_USER("Live coding is not enabled for %S.", ModuleName.c_str());
				LC_ERROR_USER("Configure the list of enabled modules from the Live Coding section of the editor preferences window.");
				GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Error, *FString::Printf(TEXT("Live coding not enabled for %s"), ModuleName.c_str()));
				return;
			}

			types::vector<std::wstring> ObjectFiles;
			for(const FString& ObjectFile : Pair.Value)
			{
				std::wstring NormalizedObjectFile = file::NormalizePath(*ObjectFile);
				ObjectFiles.push_back(std::move(NormalizedObjectFile));
			}

			m_liveModuleToModifiedOrNewObjFiles.insert(std::make_pair(ModuleFileName, std::move(ObjectFiles)));
		}
	}

	GLiveCodingServer->GetStatusChangeDelegate().ExecuteIfBound(L"Creating patch...");
	// END EPIC MOD

	// recompile files, if any
	const size_t count = m_liveModules.size();
	if (count == 0u)
	{
		LC_LOG_USER("No live modules enabled");
	}

	LiveModule::ErrorType::Enum updateError = LiveModule::ErrorType::NO_CHANGE;

	// check directory notifications first to prune file changes based on directories
	m_directoryCache->PrimeNotifications();

	FileAttributeCache fileCache;

	// when all processes made progress, none of them is being held in the debugger which means it is safe to
	// communicate with the client, call hooks, use synchronization points, etc.
	// however, when a process was held in the debugger and now spins inside the code cave, we are not allowed
	// to call any of these functions, because that might lead to a deadlock.
	// similarly, if we're currently handling an exception, calling any of the client-provided functions could be fatal.
	const bool inExceptionHandler = m_inExceptionHandlerEvent.WaitTimeout(0u);
	const LiveModule::UpdateType::Enum updateType = (didAllProcessesMakeProgress && !inExceptionHandler)
		? LiveModule::UpdateType::DEFAULT
		: LiveModule::UpdateType::NO_CLIENT_COMMUNICATION;

	// has the user given us at least one modified or new .obj file for at least one of the modules?
	const bool hasAtLeastOneOptionalObj = (m_liveModuleToModifiedOrNewObjFiles.size() != 0u);
	for (size_t i = 0u; i < count; ++i)
	{
		LiveModule* liveModule = m_liveModules[i];

		LiveModule::ErrorType::Enum moduleUpdateError = LiveModule::ErrorType::SUCCESS;
		if (hasAtLeastOneOptionalObj)
		{
			// try to find the list of modified or new .objs for this module
			const auto objFilesIt = m_liveModuleToModifiedOrNewObjFiles.find(liveModule->GetModuleName());
			if (objFilesIt == m_liveModuleToModifiedOrNewObjFiles.end())
			{
				// no .objs for this module, ignore
				continue;
			}
			else
			{
				// build a patch with the given list of .objs for this module
				const std::vector<std::wstring>& objFiles = objFilesIt->second;
				moduleUpdateError = liveModule->Update(&fileCache, m_directoryCache, updateType, objFiles);
			}
		}
		else
		{
			// no optional .objs were given, update all live modules regularly
			std::vector<std::wstring> emptyObjs;
			moduleUpdateError = liveModule->Update(&fileCache, m_directoryCache, updateType, emptyObjs);
		}

		// only accept new error conditions for this module if there haven't been any updates until now.
		// this ensures that error conditions are kept and can be shown when updating several modules at once.
		if (updateError == LiveModule::ErrorType::NO_CHANGE)
		{
			updateError = moduleUpdateError;
		}
	}

	// restart directory notifications for next compilation
	m_directoryCache->RestartNotifications();

	//EPIC REMOVED: g_theApp.GetMainFrame()->OnCompilationEnd();

	if (updateError == LiveModule::ErrorType::SUCCESS)
	{
		// bring Live++ to front on success
		if (appSettings::g_receiveFocusOnRecompile->GetValue() == appSettings::FocusOnRecompile::ON_SUCCESS)
		{
			GLiveCodingServer->GetBringToFrontDelegate().ExecuteIfBound();
		}

		// play sound on success
		const std::wstring soundOnSuccess = appSettings::g_playSoundOnSuccess->GetValue();
		if (soundOnSuccess.size() != 0u)
		{
			// first finish any sound that might still be playing, then play the real sound
			::PlaySoundW(NULL, NULL, 0u);
			::PlaySoundW(soundOnSuccess.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
		}
	}

	if ((updateError == LiveModule::ErrorType::COMPILE_ERROR) ||
		(updateError == LiveModule::ErrorType::LINK_ERROR) ||
		(updateError == LiveModule::ErrorType::LOAD_PATCH_ERROR) ||
		(updateError == LiveModule::ErrorType::ACTIVATE_PATCH_ERROR))
	{
		// bring Live++ to front on failure
		if (appSettings::g_receiveFocusOnRecompile->GetValue() == appSettings::FocusOnRecompile::ON_ERROR)
		{
			GLiveCodingServer->GetBringToFrontDelegate().ExecuteIfBound();
		}

		// play sound on error
		const std::wstring soundOnError = appSettings::g_playSoundOnError->GetValue();
		if (soundOnError.size() != 0u)
		{
			// first finish any sound that might still be playing, then play the real sound
			::PlaySoundW(NULL, NULL, 0u);
			::PlaySoundW(soundOnError.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
		}
	}

	// BEGIN EPIC MOD - Custom hooks for finishing compile
	switch (updateError)
	{
		case LiveModule::ErrorType::NO_CHANGE:
			GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Success, L"No changes detected.");
			break;

		case LiveModule::ErrorType::COMPILE_ERROR:
			GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Error, L"Compilation error.");
			break;

		case LiveModule::ErrorType::LINK_ERROR:
			GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Error, L"Linker error.");
			break;

		case LiveModule::ErrorType::LOAD_PATCH_ERROR:
			GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Error, L"Could not load patch image.");
			break;

		case LiveModule::ErrorType::ACTIVATE_PATCH_ERROR:
			GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Error, L"Could not activate patch.");
			break;

		case LiveModule::ErrorType::SUCCESS:
			GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Success, L"Patch creation successful.");
			EnumWindows(FocusApplicationWindows, (LPARAM)&m_liveProcesses);
			break;

		default:
			GLiveCodingServer->GetCompileFinishedDelegate().ExecuteIfBound(ELiveCodingResult::Success, L"Finished.");
			break;
	}
	// END EPIC MOD

	LC_LOG_USER("---------- Finished (%.3fs) ----------", scope.ReadSeconds());

	// EPIC REMOVED: g_theApp.GetMainFrame()->ResetStatusBarText();
	// EPIC REMOVED: g_theApp.GetMainFrame()->SetBusy(false);
}


unsigned int ServerCommandThread::CompileThread(void)
{
	input::Key keyControl(VK_CONTROL);
	input::Key keyAlt(VK_MENU);
	input::Key keyShift(VK_SHIFT);
	input::Key keyShortcut(VK_F11);

	Event compilationEvent(primitiveNames::CompilationEvent(m_processGroupName).c_str(), Event::Type::MANUAL_RESET);

	ChangeNotification changeNotification;

	if (appSettings::g_continuousCompilationEnabled->GetValue())
	{
		changeNotification.Create(appSettings::g_continuousCompilationPath->GetValue());
	}

	for (;;)
	{
		const int shortcutValue = appSettings::g_compileShortcut->GetValue();
		keyShortcut.AssignCode(shortcut::GetVirtualKeyCode(shortcutValue));

		keyControl.Clear();
		keyAlt.Clear();
		keyShift.Clear();
		keyShortcut.Clear();

		keyControl.Update();
		keyAlt.Update();
		keyShift.Update();
		keyShortcut.Update();

		// BEGIN EPIC MOD - Adding SetActive command
		if(!m_active)
		{
			keyShortcut.Clear();
		}
		// END EPIC MOD

		const bool control = shortcut::ContainsControl(shortcutValue) ? keyControl.IsPressed() : !keyControl.IsPressed();
		const bool alt = shortcut::ContainsAlt(shortcutValue) ? keyAlt.IsPressed() : !keyAlt.IsPressed();
		const bool shift = shortcut::ContainsShift(shortcutValue) ? keyShift.IsPressed() : !keyShift.IsPressed();
		const bool isShortcutPressed = (control && alt && shift && keyShortcut.WentDown());

		// did anything change in the watched directory?
		const unsigned int changeNotificationTimeout = static_cast<unsigned int>(appSettings::g_continuousCompilationTimeout->GetValue());

		const bool foundAnyModification = changeNotification.CheckOnce();
		if (foundAnyModification)
		{
			// clear the log if desired by the user
			if (appSettings::g_clearLogOnRecompile->GetValue())
			{
				GLiveCodingServer->GetClearOutputDelegate().ExecuteIfBound();
			}

			LC_SUCCESS_USER("Detected file modification, re-checking until timeout (%d ms)", changeNotificationTimeout);
			changeNotification.CheckNext(changeNotificationTimeout);
		}

		if (isShortcutPressed || foundAnyModification || m_manualRecompileTriggered)
		{
			// forbid command thread to handle commands through the pipe
			m_handleCommandsEvent.Reset();

			// tell clients that we're about to compile.
			// clients will send a command to say that they're ready. this command will let the command thread
			// rest until we signal the event again.
			compilationEvent.Signal();

			// remove inactive/disconnected processes
			{
				for (auto processIt = m_liveProcesses.begin(); processIt != m_liveProcesses.end(); /* nothing */)
				{
					LiveProcess* liveProcess = *processIt;
					process::Handle processHandle = liveProcess->GetProcessHandle();
					if (!process::IsActive(processHandle))
					{
						LC_WARNING_USER("Process %d is no longer valid, disconnecting", liveProcess->GetProcessId());

						process::Close(processHandle);

						// tell live modules to remove this process
						const size_t moduleCount = m_liveModules.size();
						for (size_t j = 0u; j < moduleCount; ++j)
						{
							LiveModule* liveModule = m_liveModules[j];
							liveModule->UnregisterProcess(liveProcess);
						}

						delete liveProcess;

						processIt = m_liveProcesses.erase(processIt);
					}
					else
					{
						// update process heart beats to know whether it made some progress
						liveProcess->ReadHeartBeatDelta(m_processGroupName.c_str());

						++processIt;
					}
				}
			}

			bool didAllProcessesMakeProgress = true;
			{
				const size_t processCount = m_liveProcesses.size();
				for (size_t i = 0u; i < processCount; ++i)
				{
					LiveProcess* liveProcess = m_liveProcesses[i];
					didAllProcessesMakeProgress &= liveProcess->MadeProgress();
				}
			}

			if (!didAllProcessesMakeProgress)
			{
				// install a code cave for all processes.
				// this ensures that if a process is currently being held in the debugger, the process will
				// not make progress in terms of new instructions being executed after continuing it in the debugger.
				const size_t processCount = m_liveProcesses.size();
				for (size_t i = 0u; i < processCount; ++i)
				{
					LiveProcess* liveProcess = m_liveProcesses[i];
					liveProcess->InstallCodeCave();
				}

				// don't allow the exception handler dialog to be shown when continuing in the debugger with F5
				m_exceptionCS.Enter();
			}

			// wait until all command threads/clients are ready to go. we might not be getting commands
			// from a client because it is being held in the debugger.
			{
				if (didAllProcessesMakeProgress)
				{
					LC_SUCCESS_USER("Waiting for client(s)");
				}
				else
				{
					LC_SUCCESS_USER("Waiting for client(s), hit 'Continue' (F5) if being held in the debugger");
				}

				CriticalSection::ScopedLock lock(&m_connectionCS);

				const size_t count = m_commandThreads.size();
				for (size_t i = 0u; i < count; ++i)
				{
					CommandThreadContext* threadContext = m_commandThreads[i];
					threadContext->readyEvent->Wait();
				}
			}

			// do not let other processes register new modules during compilation
			CriticalSection::ScopedLock actionLock(&m_actionCS);

			// setup the same virtual drive we had when loading the project
			AddVirtualDrive();

			if (isShortcutPressed || m_manualRecompileTriggered)
			{
				// clear the log if desired by the user
				if (appSettings::g_clearLogOnRecompile->GetValue())
				{
					GLiveCodingServer->GetClearOutputDelegate().ExecuteIfBound();
				}

				if (isShortcutPressed)
				{
					LC_SUCCESS_USER("Accepted live coding shortcut");
				}
				else if (m_manualRecompileTriggered)
				{
					LC_SUCCESS_USER("Manual recompile triggered");
				}
			}

			// bring Live++ to front on shortcut trigger
			if (appSettings::g_receiveFocusOnRecompile->GetValue() == appSettings::FocusOnRecompile::ON_SHORTCUT)
			{
				GLiveCodingServer->GetBringToFrontDelegate().ExecuteIfBound();
			}

			CompileChanges(didAllProcessesMakeProgress);

			RemoveVirtualDrive();

			if (!didAllProcessesMakeProgress)
			{
				// remove all code caves
				const size_t processCount = m_liveProcesses.size();
				for (size_t i = 0u; i < processCount; ++i)
				{
					LiveProcess* liveProcess = m_liveProcesses[i];
					liveProcess->UninstallCodeCave();
				}

				// remove the lock on the exception handler dialog
				m_exceptionCS.Leave();
			}

			compilationEvent.Reset();

			m_handleCommandsEvent.Signal();

			// clear change notifications that might have happened while compiling
			changeNotification.Check(0u);

			// clear API recompiles
			m_manualRecompileTriggered = false;
			m_liveModuleToModifiedOrNewObjFiles.clear();
		}
		else
		{
			// nothing to do for now, go to sleep a bit
			thread::Sleep(10u);
		}
	}

	return 0u;
}


unsigned int __stdcall ServerCommandThread::CommandThreadProxy(void* context)
{
	thread::SetName("Live coding client command communication");

	CommandThreadContext* realContext = static_cast<CommandThreadContext*>(context);
	return realContext->instance->CommandThread(&realContext->pipe, realContext->readyEvent);
}


unsigned int ServerCommandThread::CommandThread(DuplexPipeServer* pipe, Event* readyEvent)
{
	// handle incoming commands
	CommandMap commandMap;
	commandMap.RegisterAction<TriggerRecompileAction>();
	commandMap.RegisterAction<BuildPatchAction>();
	commandMap.RegisterAction<ReadyForCompilationAction>();
	commandMap.RegisterAction<DisconnectClientAction>();
	// BEGIN EPIC MOD - Adding ShowConsole command
	commandMap.RegisterAction<ShowConsoleAction>();
	// END EPIC MOD
	// BEGIN EPIC MOD - Adding SetVisible command
	commandMap.RegisterAction<SetVisibleAction>();
	// END EPIC MOD
	// BEGIN EPIC MOD - Adding SetActive command
	commandMap.RegisterAction<SetActiveAction>();
	// END EPIC MOD
	// BEGIN EPIC MOD - Adding SetBuildArguments command
	commandMap.RegisterAction<SetBuildArgumentsAction>();
	// END EPIC MOD
	// BEGIN EPIC MOD - Support for lazy-loading modules
	commandMap.RegisterAction<EnableLazyLoadedModuleAction>();
	// END EPIC MOD
	commandMap.RegisterAction<RegisterProcessAction>();
	commandMap.RegisterAction<EnableModuleBatchBeginAction>();
	commandMap.RegisterAction<EnableModuleBatchEndAction>();
	commandMap.RegisterAction<DisableModuleBatchBeginAction>();
	commandMap.RegisterAction<DisableModuleBatchEndAction>();
	commandMap.RegisterAction<EnableModuleAction>();
	commandMap.RegisterAction<EnableAllModulesAction>();
	commandMap.RegisterAction<DisableModuleAction>();
	commandMap.RegisterAction<DisableAllModulesAction>();
	commandMap.RegisterAction<ApplySettingBoolAction>();
	commandMap.RegisterAction<ApplySettingIntAction>();
	commandMap.RegisterAction<ApplySettingStringAction>();

	for (;;)
	{
		const bool success = commandMap.HandleCommands(pipe, this);

		// we must have received a ReadyForCompilation command to get here, or the pipe is broken.
		// in any case, let the main server thread responsible for compilation know that this client is ready.
		// this is needed to always let the compilation thread advance, even when a client might have disconnected.
		readyEvent->Signal();

		if ((!success) || (!pipe->IsValid()))
		{
			// pipe was closed or is broken, bail out.
			// remove ourselves from the array of threads first.
			RemoveCommandThread(pipe);
			return 1u;
		}

		// wait until we're allowed to handle commands again
		m_handleCommandsEvent.Wait();

		// tell client that compilation has finished
		pipe->SendCommandAndWaitForAck(commands::CompilationFinished {});
	}

	RemoveCommandThread(pipe);
	return 0u;
}


unsigned int __stdcall ServerCommandThread::ExceptionCommandThreadProxy(void* context)
{
	thread::SetName("Live coding client exception command communication");

	CommandThreadContext* realContext = static_cast<CommandThreadContext*>(context);
	return realContext->instance->ExceptionCommandThread(&realContext->exceptionPipe);
}


unsigned int ServerCommandThread::ExceptionCommandThread(DuplexPipeServer* exceptionPipe)
{
	// handle incoming exception commands
	CommandMap commandMap;
	commandMap.RegisterAction<HandleExceptionAction>();

	for (;;)
	{
		const bool success = commandMap.HandleCommands(exceptionPipe, this);
		if ((!success) || (!exceptionPipe->IsValid()))
		{
			// pipe was closed or is broken, bail out
			return 1u;
		}
	}

	return 0u;
}


void ServerCommandThread::RemoveCommandThread(const DuplexPipe* pipe)
{
	CriticalSection::ScopedLock lock(&m_connectionCS);

	const size_t count = m_commandThreads.size();
	for (size_t i = 0u; i < count; ++i)
	{
		CommandThreadContext* threadContext = m_commandThreads[i];
		if (&threadContext->pipe == pipe)
		{
			// don't bother cleaning up the context, just remove it
			auto it = m_commandThreads.begin();
			std::advance(it, i);
			m_commandThreads.erase(it);

			return;
		}
	}
}


LiveProcess* ServerCommandThread::FindProcessById(unsigned int processId)
{
	const size_t count = m_liveProcesses.size();
	for (size_t i = 0u; i < count; ++i)
	{
		LiveProcess* process = m_liveProcesses[i];
		if (process->GetProcessId() == processId)
		{
			return process;
		}
	}

	return nullptr;
}


bool ServerCommandThread::TriggerRecompileAction::Execute(CommandType*, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against accepting this command while compilation is already in progress
	CriticalSection::ScopedLock lock(&commandThread->m_actionCS);

	pipe->SendAck();

	commandThread->m_manualRecompileTriggered = true;

	return true;
}


bool ServerCommandThread::BuildPatchAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against accepting this command while compilation is already in progress
	CriticalSection::ScopedLock lock(&commandThread->m_actionCS);

	pipe->SendAck();

	// receive module names and .obj paths
	for (unsigned int i = 0u; i < command->count; ++i)
	{
		uint32_t id = 0u;
		pipe->ReceiveCommandId(&id);
		
		commands::BuildPatchPacket packetCommand = {};
		pipe->ReceiveCommand(&packetCommand);

		pipe->SendAck();

		commandThread->m_liveModuleToModifiedOrNewObjFiles[packetCommand.moduleName].push_back(packetCommand.objPath);
	}

	commandThread->m_manualRecompileTriggered = true;

	return true;
}


bool ServerCommandThread::HandleExceptionAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	// BEGIN EPIC MOD - Using internal CrashReporter
#if 0
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against several processes showing a dialog at the same time.
	// protect against showing the exception handler dialog while compilation is already in progress.
	CriticalSection::ScopedLock lock(&commandThread->m_exceptionCS);

	LiveProcess* liveProcess = commandThread->FindProcessById(command->processId);
	if (!liveProcess)
	{
		// signal client we did not handle the exception
		pipe->SendCommandAndWaitForAck(commands::HandleExceptionFinished { nullptr, nullptr, nullptr, false });
		return true;
	}

	// let the compile thread know that we're currently handling an exception.
	// this is needed to ensure that no hooks or synchronization points are called during compilation.
	commandThread->m_inExceptionHandlerEvent.Signal();

	// hold all processes in place
	const size_t processCount = commandThread->m_liveProcesses.size();
	for (size_t i = 0u; i < processCount; ++i)
	{
		commandThread->m_liveProcesses[i]->InstallCodeCave();
	}

	ExceptionHandlerDialog dialog(commandThread->m_processGroupName, liveProcess, command->threadId, command->exception, command->context, command->clientContextPtr);
	const INT_PTR result = dialog.DoModal();

	// release processes from the cave
	for (size_t i = 0u; i < processCount; ++i)
	{
		commandThread->m_liveProcesses[i]->UninstallCodeCave();
	}

	// remove our signal saying that we're handling an exception
	commandThread->m_inExceptionHandlerEvent.Reset();

	if (result == IDC_EXCEPTION_HANDLER_LEAVE)
	{
		// tell the client that it needs to unwind its stack and continue at the return address
		const ExceptionHandlerDialog::ParentFrameData& frameData = dialog.GetParentFrameData();
		pipe->SendCommandAndWaitForAck(commands::HandleExceptionFinished { frameData.returnAddress, frameData.framePointer, frameData.stackPointer, true });
		return true;
	}
	else if (result == IDC_EXCEPTION_HANDLER_IGNORE)
	{
		// tell the client that we ignored the exception
		pipe->SendCommandAndWaitForAck(commands::HandleExceptionFinished { nullptr, nullptr, nullptr, false });
		return true;
	}

	// signal client that we handled the exception and there's nothing left to do
	pipe->SendCommandAndWaitForAck(commands::HandleExceptionFinished { nullptr, nullptr, nullptr, true });
#endif
	// END EPIC MOD

	return true;
}


bool ServerCommandThread::ReadyForCompilationAction::Execute(CommandType*, const DuplexPipe* pipe, void*)
{
	pipe->SendAck();

	// don't continue execution
	return false;
}


bool ServerCommandThread::DisconnectClientAction::Execute(CommandType*, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* instance = static_cast<ServerCommandThread*>(context);

	// unregister this connection
	{
		instance->RemoveCommandThread(pipe);

		CriticalSection::ScopedLock lock(&instance->m_connectionCS);
		if (instance->m_commandThreads.size() == 0u)
		{
			// BEGIN EPIC MOD - No built-in UI
			// // this was the last client to disconnect, remove the system tray
			// g_theApp.GetMainFrame()->GetSystemTray()->Destroy();
			// END EPIC MOD
		}
	}

	pipe->SendAck();

	return true;
}

// BEGIN EPIC MOD - Adding ShowConsole command
bool ServerCommandThread::ShowConsoleAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	GLiveCodingServer->GetShowConsoleDelegate().ExecuteIfBound();

	return true;
}
// END EPIC MOD

// BEGIN EPIC MOD - Adding SetVisible command
bool ServerCommandThread::SetVisibleAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	GLiveCodingServer->GetSetVisibleDelegate().ExecuteIfBound(command->visible);

	return true;
}
// END EPIC MOD

// BEGIN EPIC MOD - Adding SetActive command
bool ServerCommandThread::SetActiveAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against accepting this command while compilation is already in progress
	CriticalSection::ScopedLock lock(&commandThread->m_actionCS);

	pipe->SendAck();

	commandThread->m_active = command->active;

	return true;
}
// END EPIC MOD

// BEGIN EPIC MOD - Adding SetBuildArguments command
bool ServerCommandThread::SetBuildArgumentsAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against accepting this command while compilation is already in progress
	CriticalSection::ScopedLock lock(&commandThread->m_actionCS);

	for (LiveProcess* process : commandThread->m_liveProcesses)
	{
		if (process->GetProcessId() == command->processId)
		{
			process->SetBuildArguments(command->arguments);
		}
	}

	pipe->SendAck();

	return true;
}
// END EPIC MOD

// BEGIN EPIC MOD - Support for lazy-loading modules
bool ServerCommandThread::EnableLazyLoadedModuleAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against accepting this command while compilation is already in progress
	CriticalSection::ScopedLock lock(&commandThread->m_actionCS);

	for (LiveProcess* process : commandThread->m_liveProcesses)
	{
		if (process->GetProcessId() == command->processId)
		{
			const std::wstring modulePath = file::NormalizePath(command->fileName);
			process->AddLazyLoadedModule(modulePath, command->moduleBase);
			LC_LOG_DEV("Registered module %S for lazy-loading", modulePath.c_str());
		}
	}

	pipe->SendAck();

	return true;
}
// END EPIC MOD

bool ServerCommandThread::RegisterProcessAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against several client DLLs calling into this action at the same time
	CriticalSection::ScopedLock lock(&commandThread->m_actionCS);

	process::Handle processHandle = process::Open(command->processId);

	// check if any live module in this process group has patches installed already
	{
		const std::wstring& processPath = process::GetImagePath(processHandle);

		bool registeredSuccessfully = true;
		if (!appSettings::g_installCompiledPatchesMultiProcess->GetValue())
		{
			// we are not allowed to install any compiled patches when a new executable is spawned
			bool processGroupHasPatches = false;
			const size_t count = commandThread->m_liveModules.size();
			for (size_t i = 0u; i < count; ++i)
			{
				LiveModule* liveModule = commandThread->m_liveModules[i];
				if (liveModule->HasInstalledPatches())
				{
					std::wstring caption(L"Live coding - Registering process ");
					caption += file::GetFilename(processPath);

					processGroupHasPatches = true;
					// BEGIN EPIC MOD - Using non-modal error dialog
					GLiveCodingServer->GetLogOutputDelegate().ExecuteIfBound(ELiveCodingLogVerbosity::Failure, L"This process cannot be added to the existing process group, because at least one module already has installed patches. Live coding is disabled for this process.");
					// END EPIC MD
					break;
				}
			}

			registeredSuccessfully = !processGroupHasPatches;
		}

		if (registeredSuccessfully)
		{
			LiveProcess* liveProcess = new LiveProcess(processHandle, command->processId, command->threadId, pipe);
			commandThread->m_liveProcesses.push_back(liveProcess);
			// BEGIN EPIC MOD - No built-in UI
			// commandThread->m_mainFrame->UpdateWindowTitle();
			// END EPIC MOD

			LC_SUCCESS_USER("Registered process %S (PID: %d)", processPath.c_str(), command->processId);
		}

		// tell client we are finished
		pipe->SendCommandAndWaitForAck(commands::RegisterProcessFinished { registeredSuccessfully });
	}

	return true;
}


bool ServerCommandThread::EnableModuleBatchBeginAction::Execute(CommandType*, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against several client DLLs calling into this action at the same time.
	// we hold this critical section until we get the BatchEnd signal.
	// this ensures that all modules are loaded serialized per process.
	commandThread->m_actionCS.Enter();

	commandThread->m_moduleBatchScope.Restart();
	commandThread->m_loadedCompilandCountInBatchScope = 0u;

	// set up virtual drives before loading anything, otherwise files won't be detected and therefore discarded
	const std::wstring virtualDriveLetter = appSettings::g_virtualDriveLetter->GetValue();
	const std::wstring virtualDrivePath = appSettings::g_virtualDrivePath->GetValue();
	if ((virtualDriveLetter.size() != 0) && (virtualDrivePath.size() != 0))
	{
		virtualDrive::Add(virtualDriveLetter.c_str(), virtualDrivePath.c_str());
	}

	pipe->SendAck();

	return true;
}


bool ServerCommandThread::EnableModuleBatchEndAction::Execute(CommandType*, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	commandThread->m_moduleBatchScope.End();
	LC_SUCCESS_USER("Successfully loaded modules (%.3fs, %zu translation units)", commandThread->m_moduleBatchScope.ReadSeconds(), commandThread->m_loadedCompilandCountInBatchScope);

	// EPIC REMOVED: commandThread->PrewarmCompilerEnvironmentCache();

	// EPIC REMOVED: g_theApp.GetMainFrame()->ResetStatusBarText();

	// tell user we are ready
	// BEGIN EPIC MOD - Support for lazy-loading modules
	if (thread::GetId() != thread::GetId(commandThread->m_compileThread))
	// END EPIC MOD
	{
		const int shortcut = appSettings::g_compileShortcut->GetValue();
		const std::wstring& shortcutText = shortcut::ConvertShortcutToText(shortcut);
		LC_SUCCESS_USER("Live coding ready - Save changes and press %S to re-compile code", shortcutText.c_str());
	}

	// remove virtual drives once we're finished
	const std::wstring virtualDriveLetter = appSettings::g_virtualDriveLetter->GetValue();
	const std::wstring virtualDrivePath = appSettings::g_virtualDrivePath->GetValue();
	if ((virtualDriveLetter.size() != 0) && (virtualDrivePath.size() != 0))
	{
		virtualDrive::Remove(virtualDriveLetter.c_str(), virtualDrivePath.c_str());
	}

	pipe->SendAck();

	// protect against several client DLLs calling into this action at the same time
	commandThread->m_actionCS.Leave();

	return true;
}


bool ServerCommandThread::DisableModuleBatchBeginAction::Execute(CommandType*, const DuplexPipe* pipe, void* context)
{
	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	// protect against several client DLLs calling into this action at the same time.
	// we hold this critical section until we get the BatchEnd signal.
	// this ensures that all modules are unloaded serialized per process.
	commandThread->m_actionCS.Enter();

	pipe->SendAck();

	return true;
}


bool ServerCommandThread::DisableModuleBatchEndAction::Execute(CommandType*, const DuplexPipe* pipe, void* context)
{
	// EPIC REMOVED: g_theApp.GetMainFrame()->ResetStatusBarText();

	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	pipe->SendAck();

	// protect against several client DLLs calling into this action at the same time
	commandThread->m_actionCS.Leave();

	return true;
}


bool ServerCommandThread::EnableModuleAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	TaskContext taskContext = {};
	taskContext.taskRoot = scheduler::CreateEmptyTask();
	commandThread->LoadModule(command->path, pipe, &taskContext, command->processId);

	// wait for all tasks to finish
	scheduler::RunTask(taskContext.taskRoot);
	scheduler::WaitForTask(taskContext.taskRoot);

	// add all live modules loaded by the tasks
	for (size_t i = 0u; i < taskContext.tasks.size(); ++i)
	{
		LiveModule* liveModule = taskContext.tasks[i]->GetResult();
		commandThread->m_liveModules.push_back(liveModule);

		// update directory cache for this live module
		liveModule->UpdateDirectoryCache(commandThread->m_directoryCache);

		// update the number of loaded translation units during this batch
		commandThread->m_loadedCompilandCountInBatchScope += liveModule->GetCompilandDatabase()->compilands.size();
	}

	scheduler::DestroyTasks(taskContext.tasks);
	scheduler::DestroyTask(taskContext.taskRoot);

	// tell client we are finished
	pipe->SendCommandAndWaitForAck(commands::EnableModuleFinished { command->token });

	// dump memory statistics
	{
		LC_LOG_INDENT_TELEMETRY;
		g_symbolAllocator.PrintStats();
		g_immutableStringAllocator.PrintStats();
		g_contributionAllocator.PrintStats();
		g_compilandAllocator.PrintStats();
		g_dependencyAllocator.PrintStats();
	}

	return true;
}


bool ServerCommandThread::EnableAllModulesAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);

	TaskContext taskContext = {};
	taskContext.taskRoot = scheduler::CreateEmptyTask();
	commandThread->LoadAllModules(command->path, pipe, &taskContext, command->processId);

	// wait for all tasks to finish
	scheduler::RunTask(taskContext.taskRoot);
	scheduler::WaitForTask(taskContext.taskRoot);

	// add all live modules loaded by the tasks
	for (size_t i = 0u; i < taskContext.tasks.size(); ++i)
	{
		LiveModule* liveModule = taskContext.tasks[i]->GetResult();
		commandThread->m_liveModules.push_back(liveModule);

		// update directory cache for this live module
		liveModule->UpdateDirectoryCache(commandThread->m_directoryCache);

		// update the number of loaded translation units during this batch
		commandThread->m_loadedCompilandCountInBatchScope += liveModule->GetCompilandDatabase()->compilands.size();
	}

	scheduler::DestroyTasks(taskContext.tasks);
	scheduler::DestroyTask(taskContext.taskRoot);

	// tell server we are finished
	pipe->SendCommandAndWaitForAck(commands::EnableAllModulesFinished { command->token });

	// dump memory statistics
	{
		LC_LOG_INDENT_TELEMETRY;
		g_symbolAllocator.PrintStats();
		g_immutableStringAllocator.PrintStats();
		g_contributionAllocator.PrintStats();
		g_compilandAllocator.PrintStats();
		g_dependencyAllocator.PrintStats();
	}

	return true;
}


bool ServerCommandThread::DisableModuleAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);
	commandThread->UnloadModule(command->path, pipe, command->processId);

	// tell server we are finished
	pipe->SendCommandAndWaitForAck(commands::DisableModuleFinished { command->token });

	return true;
}


bool ServerCommandThread::DisableAllModulesAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);
	commandThread->UnloadAllModules(command->path, pipe, command->processId);

	// tell server we are finished
	pipe->SendCommandAndWaitForAck(commands::DisableAllModulesFinished { command->token });

	return true;
}


bool ServerCommandThread::GetModuleInfoAction::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	if (!command->moduleBase)
	{
		return false;
	}

	// note that the path we get back from the DLL might not be normalized, depending on how the executable was launched.
	// it is crucial to normalize the path again, otherwise we could load already loaded modules into the same
	// Live++ instance, which would wreak havoc
	const std::wstring modulePath = file::NormalizePath(file::RelativeToAbsolutePath(command->path).c_str());

	ServerCommandThread* commandThread = static_cast<ServerCommandThread*>(context);
	TaskContext* taskContext = static_cast<TaskContext*>(command->taskContext);

	// a task context is provided for loading modules
	const bool shouldLoad = (taskContext != nullptr);

	if (command->loadImports)
	{
		if (shouldLoad)
		{
			// load this module and all its import DLLs as well
			commandThread->LoadAllModules(modulePath.c_str(), pipe, taskContext, command->processId);
		}
		else
		{
			// unload this module and all its import DLLs as well
			commandThread->UnloadAllModules(modulePath.c_str(), pipe, command->processId);
		}

		return false;
	}

	LiveProcess* liveProcess = commandThread->FindProcessById(command->processId);
	LC_ASSERT(liveProcess, "Invalid process ID.");

	const executable::Header imageHeader = GetImageHeader(modulePath.c_str());
	if (shouldLoad)
	{
		if (liveProcess->TriedToLoadImage(imageHeader))
		{
			// tried loading this module into this process already
			return false;
		}

		// find any other process ID that tried to load this module already (if any)
		{
			const size_t count = commandThread->m_liveProcesses.size();
			for (size_t i = 0u; i < count; ++i)
			{
				LiveProcess* otherLiveProcess = commandThread->m_liveProcesses[i];
				if (otherLiveProcess->TriedToLoadImage(imageHeader))
				{
					// some *other* process loaded this module already
					LC_LOG_USER("Registering module %S (PID: %d)", modulePath.c_str(), command->processId);

					LiveModule* liveModule = commandThread->m_imageHeaderToLiveModule[imageHeader];
					if (liveModule)
					{
						const unsigned int processId = command->processId;
						void* moduleBase = command->moduleBase;

						liveModule->RegisterProcess(liveProcess, moduleBase, modulePath);
						liveModule->DisableControlFlowGuard(liveProcess, moduleBase);
					
						const bool installedPatchesSuccessfully = liveModule->InstallCompiledPatches(liveProcess, moduleBase);
						if (!installedPatchesSuccessfully)
						{
							LC_ERROR_USER("Compiled patches could not be installed (PID: %d)", processId);
							liveModule->UnregisterProcess(liveProcess);
						}

						liveProcess->AddLoadedImage(imageHeader);
					}

					return false;
				}
			}
		}

		symbols::Provider* moduleProvider = symbols::OpenEXE(modulePath.c_str(), symbols::OpenOptions::ACCUMULATE_SIZE);
		if (!moduleProvider)
		{
			liveProcess->AddLoadedImage(imageHeader);
			return false;
		}

		// this live module hasn't been loaded yet by any process
		void* moduleBase = command->moduleBase;

		// accumulate module info
		const file::Attributes attributes = file::GetAttributes(modulePath.c_str());
		const uint64_t size = file::GetSize(attributes);
		g_loadedModuleSize.Accumulate(size);

		{
			// create a task to load the module of this batch concurrently
			LC_LOG_USER("Loading module %S (%.3f MB)", modulePath.c_str(), size / 1048576.0f);

			LiveModule* liveModule = new LiveModule(modulePath.c_str(), imageHeader, commandThread->m_runMode);
			commandThread->m_imageHeaderToLiveModule.emplace(imageHeader, liveModule);

			auto task = scheduler::CreateTask(taskContext->taskRoot, [liveModule, liveProcess, modulePath, moduleBase, moduleProvider]()
			{
				telemetry::Scope scope("Loading module");

				symbols::DiaCompilandDB* moduleDiaCompilandDb = symbols::GatherDiaCompilands(moduleProvider);

				liveModule->Load(moduleProvider, moduleDiaCompilandDb);
				liveModule->RegisterProcess(liveProcess, moduleBase, modulePath);
				liveModule->DisableControlFlowGuard(liveProcess, moduleBase);

				symbols::DestroyDiaCompilandDB(moduleDiaCompilandDb);
				symbols::Close(moduleProvider);

				return liveModule;
			});
			scheduler::RunTask(task);

			taskContext->tasks.emplace_back(task);
		}

		g_loadedModuleSize.Print();
		g_loadedModuleSize.ResetCurrent();

		liveProcess->AddLoadedImage(imageHeader);

		return false;
	}
	else
	{
		LC_LOG_USER("Unloading module %S", modulePath.c_str());

		liveProcess->RemoveLoadedImage(imageHeader);
		commandThread->m_imageHeaderToLiveModule.erase(imageHeader);

		for (auto it = commandThread->m_liveModules.begin(); it != commandThread->m_liveModules.end(); /* nothing */)
		{
			LiveModule* liveModule = *it;
			if (std::equal_to<executable::Header>()(liveModule->GetImageHeader(), imageHeader))
			{
				liveModule->Unload();
				delete liveModule;

				it = commandThread->m_liveModules.erase(it);
			}
			else
			{
				++it;
			}
		}

		return false;
	}
}


bool ServerCommandThread::ApplySettingBoolAction::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	pipe->SendAck();

	appSettings::ApplySettingBool(command->settingName, (command->settingValue == 0) ? false : true);

	return true;
}


bool ServerCommandThread::ApplySettingIntAction::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	pipe->SendAck();

	appSettings::ApplySettingInt(command->settingName, command->settingValue);

	return true;
}


bool ServerCommandThread::ApplySettingStringAction::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	pipe->SendAck();

	appSettings::ApplySettingString(command->settingName, command->settingValue);

	return true;
}

// BEGIN EPIC MODS
#pragma warning(pop)
// END EPIC MODS
