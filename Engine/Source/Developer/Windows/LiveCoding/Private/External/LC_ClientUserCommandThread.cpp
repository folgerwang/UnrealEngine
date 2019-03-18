// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_ClientUserCommandThread.h"
#include "LC_CommandMap.h"
#include "LC_DuplexPipeClient.h"
#include "LC_ClientCommandActions.h"
#include "LC_Event.h"
#include "LC_Process.h"
#include "LC_CriticalSection.h"
#include "LC_StringUtil.h"
#include <deque>


namespace userCommands
{
	struct Scope
	{
		enum Enum
		{
			NONE,
			ENABLE_MODULES,
			DISABLE_MODULES,
		};
	};

	struct BaseCommand
	{
		explicit BaseCommand(Scope::Enum scope)
			: m_scope(scope)
		{
		}

		virtual ~BaseCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) = 0;
		
		inline Scope::Enum GetScope(void) const
		{
			return m_scope;
		}

	private:
		Scope::Enum m_scope;
	};

	struct EnableModuleCommand : public BaseCommand
	{
		EnableModuleCommand(void)
			: BaseCommand(Scope::ENABLE_MODULES)
		{
		}

		virtual ~EnableModuleCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::EnableModule serverCommand;
			serverCommand.processId = process::GetId();
			wcscpy_s(serverCommand.path, moduleName.c_str());
			serverCommand.token = token;

			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		Event* token;
		std::wstring moduleName;
	};

	struct EnableAllModulesCommand : public BaseCommand
	{
		EnableAllModulesCommand(void)
			: BaseCommand(Scope::ENABLE_MODULES)
		{
		}

		virtual ~EnableAllModulesCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::EnableAllModules serverCommand;
			serverCommand.processId = process::GetId();
			wcscpy_s(serverCommand.path, moduleName.c_str());
			serverCommand.token = token;

			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		Event* token;
		std::wstring moduleName;
	};

	struct DisableModuleCommand : public BaseCommand
	{
		DisableModuleCommand(void)
			: BaseCommand(Scope::DISABLE_MODULES)
		{
		}

		virtual ~DisableModuleCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::DisableModule serverCommand;
			serverCommand.processId = process::GetId();
			wcscpy_s(serverCommand.path, moduleName.c_str());
			serverCommand.token = token;

			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		Event* token;
		std::wstring moduleName;
	};

	struct DisableAllModulesCommand : public BaseCommand
	{
		DisableAllModulesCommand(void)
			: BaseCommand(Scope::DISABLE_MODULES)
		{
		}

		virtual ~DisableAllModulesCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::DisableAllModules serverCommand;
			serverCommand.processId = process::GetId();
			wcscpy_s(serverCommand.path, moduleName.c_str());
			serverCommand.token = token;

			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		Event* token;
		std::wstring moduleName;
	};

	struct TriggerRecompileCommand : public BaseCommand
	{
		TriggerRecompileCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~TriggerRecompileCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::TriggerRecompile serverCommand;
			pipe->SendCommandAndWaitForAck(serverCommand);
		}
	};

	// BEGIN EPIC MOD - Adding ShowConsole command
	struct ShowConsoleCommand : public BaseCommand
	{
		ShowConsoleCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~ShowConsoleCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::ShowConsole serverCommand;
			pipe->SendCommandAndWaitForAck(serverCommand);
		}
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetVisible command
	struct SetVisibleCommand : public BaseCommand
	{
		SetVisibleCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~SetVisibleCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::SetVisible serverCommand;
			serverCommand.visible = visible;
			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		bool visible;
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetActive command
	struct SetActiveCommand : public BaseCommand
	{
		SetActiveCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~SetActiveCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::SetActive serverCommand;
			serverCommand.active = active;
			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		bool active;
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetActive command
	struct SetBuildArgumentsCommand : public BaseCommand
	{
		SetBuildArgumentsCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~SetBuildArgumentsCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::SetBuildArguments serverCommand;
			serverCommand.processId = process::GetId();
			wcscpy_s(serverCommand.arguments, arguments.c_str());
			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		std::wstring arguments;
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Support for lazy-loading modules
	struct EnableLazyLoadedModuleCommand : public BaseCommand
	{
		EnableLazyLoadedModuleCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~EnableLazyLoadedModuleCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::EnableLazyLoadedModule serverCommand;
			serverCommand.processId = process::GetId();
			wcscpy_s(serverCommand.fileName, fileName.c_str());
			serverCommand.moduleBase = moduleBase;
			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		std::wstring fileName;
		Windows::HMODULE moduleBase;
	};
	// END EPIC MOD


	struct BuildPatchCommand : public BaseCommand
	{
		BuildPatchCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~BuildPatchCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::BuildPatch serverCommand;
			serverCommand.count = count;
			pipe->SendCommandAndWaitForAck(serverCommand);

			for (unsigned int i = 0u; i < count; ++i)
			{
				commands::BuildPatchPacket packet;
				wcscpy_s(packet.moduleName, moduleNames[i].c_str());
				wcscpy_s(packet.objPath, objPaths[i].c_str());

				pipe->SendCommandAndWaitForAck(packet);
			}
		}

		unsigned int count;
		std::vector<std::wstring> moduleNames;
		std::vector<std::wstring> objPaths;
	};

	struct ApplySettingBoolCommand : public BaseCommand
	{
		ApplySettingBoolCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~ApplySettingBoolCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::ApplySettingBool serverCommand;
			strcpy_s(serverCommand.settingName, settingName.c_str());
			serverCommand.settingValue = value;
			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		std::string settingName;
		int value;
	};

	struct ApplySettingIntCommand : public BaseCommand
	{
		ApplySettingIntCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~ApplySettingIntCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::ApplySettingInt serverCommand;
			strcpy_s(serverCommand.settingName, settingName.c_str());
			serverCommand.settingValue = value;
			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		std::string settingName;
		int value;
	};

	struct ApplySettingStringCommand : public BaseCommand
	{
		ApplySettingStringCommand(void)
			: BaseCommand(Scope::NONE)
		{
		}

		virtual ~ApplySettingStringCommand(void) {}

		virtual void Execute(DuplexPipe* pipe) override
		{
			commands::ApplySettingString serverCommand;
			strcpy_s(serverCommand.settingName, settingName.c_str());
			wcscpy_s(serverCommand.settingValue, value.c_str());
			pipe->SendCommandAndWaitForAck(serverCommand);
		}

		std::string settingName;
		std::wstring value;
	};
}


namespace
{
	// queue for working on commands received by user code
	static std::deque<userCommands::BaseCommand*> g_userCommandQueue;
	static CriticalSection g_userCommandQueueCS;
}

// BEGIN EPIC MOD - Allow manually batching commands
void BeginCommandBatch()
{
	g_userCommandQueueCS.Enter();
}

void EndCommandBatch()
{
	g_userCommandQueueCS.Leave();
}
// END EPIC MOD


ClientUserCommandThread::ClientUserCommandThread(DuplexPipeClient* pipeClient, DuplexPipeClient* exceptionPipeClient)
	: m_thread(INVALID_HANDLE_VALUE)
	, m_processGroupName()
	, m_pipe(pipeClient)
	, m_exceptionPipe(exceptionPipeClient)
	, m_itemInQueueEvent(new Event(nullptr, Event::Type::MANUAL_RESET))
{
}


ClientUserCommandThread::~ClientUserCommandThread(void)
{
	delete m_itemInQueueEvent;
}


unsigned int ClientUserCommandThread::Start(const std::wstring& processGroupName, Event* waitForStartEvent, CriticalSection* pipeAccessCS)
{
	m_processGroupName = processGroupName;

	// spawn a thread that does the work
	ThreadContext* context = new ThreadContext;
	context->thisInstance = this;
	context->waitForStartEvent = waitForStartEvent;
	context->pipeAccessCS = pipeAccessCS;

	m_thread = thread::Create(128u * 1024u, &ThreadProxy, context);

	return thread::GetId(m_thread);
}


void ClientUserCommandThread::Join(void)
{
	if (m_thread != INVALID_HANDLE_VALUE)
	{
		thread::Join(m_thread);
		thread::Close(m_thread);
	}
}


void* ClientUserCommandThread::EnableModule(const wchar_t* const nameOfExeOrDll)
{
	userCommands::EnableModuleCommand* command = new userCommands::EnableModuleCommand;
	command->token = new Event(nullptr, Event::Type::AUTO_RESET);
	command->moduleName = nameOfExeOrDll;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();

	return command->token;
}


void* ClientUserCommandThread::EnableAllModules(const wchar_t* const nameOfExeOrDll)
{
	userCommands::EnableAllModulesCommand* command = new userCommands::EnableAllModulesCommand;
	command->token = new Event(nullptr, Event::Type::AUTO_RESET);
	command->moduleName = nameOfExeOrDll;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();

	return command->token;
}


void* ClientUserCommandThread::DisableModule(const wchar_t* const nameOfExeOrDll)
{
	userCommands::DisableModuleCommand* command = new userCommands::DisableModuleCommand;
	command->token = new Event(nullptr, Event::Type::AUTO_RESET);
	command->moduleName = nameOfExeOrDll;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();

	return command->token;
}


void* ClientUserCommandThread::DisableAllModules(const wchar_t* const nameOfExeOrDll)
{
	userCommands::DisableAllModulesCommand* command = new userCommands::DisableAllModulesCommand;
	command->token = new Event(nullptr, Event::Type::AUTO_RESET);
	command->moduleName = nameOfExeOrDll;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();

	return command->token;
}


void ClientUserCommandThread::WaitForToken(void* token)
{
	Event* event = static_cast<Event*>(token);

	if (m_thread != INVALID_HANDLE_VALUE)
	{
		// thread was successfully initialized, wait until the command has been executed in the queue
		event->Wait();
	}

	delete event;
}


void ClientUserCommandThread::TriggerRecompile(void)
{
	userCommands::TriggerRecompileCommand* command = new userCommands::TriggerRecompileCommand;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}


void ClientUserCommandThread::BuildPatch(const wchar_t* moduleNames[], const wchar_t* objPaths[], unsigned int count)
{
	userCommands::BuildPatchCommand* command = new userCommands::BuildPatchCommand;
	command->count = count;
	command->moduleNames.reserve(count);
	command->objPaths.reserve(count);

	for (unsigned int i = 0u; i < count; ++i)
	{
		command->moduleNames.push_back(moduleNames[i]);
		command->objPaths.push_back(objPaths[i]);
	}
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}


void ClientUserCommandThread::InstallExceptionHandler(void)
{
	// BEGIN EPIC MOD - Using internal CrashReporter
	// exceptionHandler::Register(this);
	// END EPIC MOD
}


ClientUserCommandThread::ExceptionResult ClientUserCommandThread::HandleException(EXCEPTION_RECORD* exception, CONTEXT* context, unsigned int threadId)
{
	commands::HandleException serverCommand;
	serverCommand.processId = process::GetId();
	serverCommand.threadId = threadId;
	serverCommand.exception = *exception;
	serverCommand.context = *context;
	serverCommand.clientContextPtr = context;

	m_exceptionPipe->SendCommandAndWaitForAck(serverCommand);

	ExceptionResult result = {};

	CommandMap commandMap;
	commandMap.RegisterAction<actions::HandleExceptionFinished>();
	commandMap.HandleCommands(m_exceptionPipe, &result);

	return result;
}


void ClientUserCommandThread::End(void)
{
	// signal to the thread that a new item is in the queue to make it break out of its main loop
	m_itemInQueueEvent->Reset();
	m_itemInQueueEvent->Signal();
}


// BEGIN EPIC MOD - Adding ShowConsole command
void ClientUserCommandThread::ShowConsole()
{
	userCommands::ShowConsoleCommand* command = new userCommands::ShowConsoleCommand;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}
// END EPIC MOD

// BEGIN EPIC MOD - Adding SetVisible command
void ClientUserCommandThread::SetVisible(bool visible)
{
	userCommands::SetVisibleCommand* command = new userCommands::SetVisibleCommand;
	command->visible = visible;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}
// END EPIC MOD

// BEGIN EPIC MOD - Adding SetActive command
void ClientUserCommandThread::SetActive(bool active)
{
	userCommands::SetActiveCommand* command = new userCommands::SetActiveCommand;
	command->active = active;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}
// END EPIC MOD

// BEGIN EPIC MOD - Adding SetBuildArguments command
void ClientUserCommandThread::SetBuildArguments(const wchar_t* arguments)
{
	userCommands::SetBuildArgumentsCommand* command = new userCommands::SetBuildArgumentsCommand;
	command->arguments = arguments;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}
// END EPIC MOD

// BEGIN EPIC MOD - Adding support for lazy-loading modules
void ClientUserCommandThread::EnableLazyLoadedModule(const wchar_t* fileName, Windows::HMODULE moduleBase)
{
	userCommands::EnableLazyLoadedModuleCommand* command = new userCommands::EnableLazyLoadedModuleCommand;
	command->fileName = fileName;
	command->moduleBase = moduleBase;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}
// END EPIC MOD


void ClientUserCommandThread::ApplySettingBool(const char* const settingName, int value)
{
	userCommands::ApplySettingBoolCommand* command = new userCommands::ApplySettingBoolCommand;
	command->settingName = settingName;
	command->value = value;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}


void ClientUserCommandThread::ApplySettingInt(const char* const settingName, int value)
{
	userCommands::ApplySettingIntCommand* command = new userCommands::ApplySettingIntCommand;
	command->settingName = settingName;
	command->value = value;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}


void ClientUserCommandThread::ApplySettingString(const char* const settingName, const wchar_t* const value)
{
	userCommands::ApplySettingStringCommand* command = new userCommands::ApplySettingStringCommand;
	command->settingName = settingName;
	command->value = value;
	{
		CriticalSection::ScopedLock lock(&g_userCommandQueueCS);
		g_userCommandQueue.push_front(command);
	}

	// signal to the thread that a new item is in the queue
	m_itemInQueueEvent->Signal();
}


unsigned int __stdcall ClientUserCommandThread::ThreadProxy(void* context)
{
	thread::SetName("Live coding user commands");

	ThreadContext* realContext = static_cast<ThreadContext*>(context);
	const unsigned int exitCode = realContext->thisInstance->ThreadFunction(realContext->waitForStartEvent, realContext->pipeAccessCS);

	delete realContext;

	return exitCode;
}


// BEGIN EPIC MOD - Temporarily release lock to prevent hangs
class LeaveableScopedLock
{
public:
	explicit LeaveableScopedLock(CriticalSection* cs)
		: m_cs(cs)
		, m_hasLock(true)
	{
		cs->Enter();
	}

	void Enter()
	{
		if (!m_hasLock)
		{
			m_cs->Enter();
			m_hasLock = true;
		}
	}

	void Leave()
	{
		if (m_hasLock)
		{
			m_cs->Leave();
			m_hasLock = false;
		}
	}

	~LeaveableScopedLock(void)
	{
		Leave();
	}

private:
	CriticalSection* m_cs;
	bool m_hasLock;
};
// END EPIC MOD



unsigned int ClientUserCommandThread::ThreadFunction(Event* waitForStartEvent, CriticalSection* pipeAccessCS)
{
	// wait until we get the signal that the thread can start
	waitForStartEvent->Wait();

	CommandMap moduleCommandMap;
	moduleCommandMap.RegisterAction<actions::GetModule>();
	moduleCommandMap.RegisterAction<actions::EnableModuleFinished>();
	moduleCommandMap.RegisterAction<actions::EnableAllModulesFinished>();
	moduleCommandMap.RegisterAction<actions::DisableModuleFinished>();
	moduleCommandMap.RegisterAction<actions::DisableAllModulesFinished>();

	// those commands are needed when loading compiled patches into spawned executables
	moduleCommandMap.RegisterAction<actions::LoadPatch>();
	moduleCommandMap.RegisterAction<actions::UnloadPatch>();
	moduleCommandMap.RegisterAction<actions::EnterSyncPoint>();
	moduleCommandMap.RegisterAction<actions::LeaveSyncPoint>();
	moduleCommandMap.RegisterAction<actions::CallEntryPoint>();
	moduleCommandMap.RegisterAction<actions::CallHooks>();

	for (;;)
	{
		// wait for event that signals that something is in the queue
		m_itemInQueueEvent->Wait();

		if (!m_pipe->IsValid())
		{
			// BEGIN EPIC MOD - Using internal CrashReporter
			// // pipe was closed or is broken, bail out
			// exceptionHandler::Unregister();
			// END EPIC MOD - Using internal CrashReporter
			return 1u;
		}

		// lock critical section for accessing the pipe.
		// we need to make sure that other threads talking through the pipe don't use it at the same time.
		CriticalSection::ScopedLock pipeLock(pipeAccessCS);

		// lock critical section for accessing the queue.
		// user code might be calling other exported functions in the mean time.
		// BEGIN EPIC MOD - Rearrange lock scope to prevent hangs
		LeaveableScopedLock queueLock(&g_userCommandQueueCS);
		// END EPIC MOD

		if (g_userCommandQueue.size() == 0u)
		{
			// BEGIN EPIC MOD - Using internal CrashReporter
			// // no new item available, bail out
			// exceptionHandler::Unregister();
			// END EPIC MOD - Using internal CrashReporter
			return 2u;
		}

		// separate commands into three groups: ones that need to be scoped for enabling modules, the ones that
		// need to be scoped for disabling modules, and the others that don't need to be scoped at all
		std::vector<userCommands::BaseCommand*> enabledScopedCommands;
		enabledScopedCommands.reserve(g_userCommandQueue.size());

		std::vector<userCommands::BaseCommand*> disableScopedCommands;
		disableScopedCommands.reserve(g_userCommandQueue.size());

		std::vector<userCommands::BaseCommand*> commands;
		commands.reserve(g_userCommandQueue.size());

		while (g_userCommandQueue.size() > 0u)
		{
			userCommands::BaseCommand* command = g_userCommandQueue.back();
			g_userCommandQueue.pop_back();

			if (command->GetScope() == userCommands::Scope::NONE)
			{
				commands.push_back(command);
			}
			else if (command->GetScope() == userCommands::Scope::ENABLE_MODULES)
			{
				enabledScopedCommands.push_back(command);
			}
			else if (command->GetScope() == userCommands::Scope::DISABLE_MODULES)
			{
				disableScopedCommands.push_back(command);
			}
		}

		// BEGIN EPIC MOD - Temporarily release lock to prevent hangs
		queueLock.Leave();
		// BEGIN EPIC MOD - Pre

		// send out scoped commands first
		{
			const size_t count = enabledScopedCommands.size();
			if (count != 0u)
			{
				m_pipe->SendCommandAndWaitForAck(commands::EnableModuleBatchBegin {});

				for (size_t i=0u; i < count; ++i)
				{
					userCommands::BaseCommand* command = enabledScopedCommands[i];
					command->Execute(m_pipe);

					moduleCommandMap.HandleCommands(m_pipe, nullptr);

					delete command;
				}

				m_pipe->SendCommandAndWaitForAck(commands::EnableModuleBatchEnd {});
			}
		}
		{
			const size_t count = disableScopedCommands.size();
			if (count != 0u)
			{
				m_pipe->SendCommandAndWaitForAck(commands::DisableModuleBatchBegin{});

				for (size_t i = 0u; i < count; ++i)
				{
					userCommands::BaseCommand* command = disableScopedCommands[i];
					command->Execute(m_pipe);

					moduleCommandMap.HandleCommands(m_pipe, nullptr);

					delete command;
				}

				m_pipe->SendCommandAndWaitForAck(commands::DisableModuleBatchEnd{});
			}
		}

		// send out non-scoped commands second
		{
			const size_t count = commands.size();
			for (size_t i = 0u; i < count; ++i)
			{
				userCommands::BaseCommand* command = commands[i];
				command->Execute(m_pipe);

				delete command;
			}
		}

		// BEGIN EPIC MOD - Temporarily release lock to prevent hangs
		queueLock.Enter();
		if(g_userCommandQueue.size() > 0)
		{
			continue;
		}
		// BEGIN EPIC MOD

		m_itemInQueueEvent->Reset();
	}

	return 0u;
}
