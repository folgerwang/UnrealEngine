// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Thread.h"
#include "LC_Commands.h"
#include "LC_Telemetry.h"
#include "LC_DuplexPipeServer.h"
#include "LC_CriticalSection.h"
#include "LC_Event.h"
#include "LC_Scheduler.h"
#include "LC_Executable.h"
#include "LC_RunMode.h"
#include "LC_Types.h"


class MainFrame;
class DirectoryCache;
class LiveModule;
class LiveProcess;

class ServerCommandThread
{
public:
	ServerCommandThread(MainFrame* mainFrame, const wchar_t* const processGroupName, RunMode::Enum runMode);
	~ServerCommandThread(void);

	std::wstring GetProcessImagePath(void) const;

	struct TaskContext
	{
		scheduler::TaskBase* taskRoot;
		types::vector<scheduler::Task<LiveModule*>*> tasks;
	};

private:
	void LoadModule(const wchar_t* modulePath, const DuplexPipe* pipe, TaskContext* tasks, unsigned int processId);
	void LoadAllModules(const wchar_t* modulePath, const DuplexPipe* pipe, TaskContext* tasks, unsigned int processId);

	void UnloadModule(const wchar_t* modulePath, const DuplexPipe* pipe, unsigned int processId);
	void UnloadAllModules(const wchar_t* modulePath, const DuplexPipe* pipe, unsigned int processId);

	void PrewarmCompilerEnvironmentCache(void);

	static unsigned int __stdcall ServerThreadProxy(void* context);
	unsigned int ServerThread(void);

	static unsigned int __stdcall CompileThreadProxy(void* context);
	unsigned int CompileThread(void);

	struct CommandThreadContext
	{
		ServerCommandThread* instance;

		DuplexPipeServer pipe;
		Event* readyEvent;
		thread::Handle commandThread;

		DuplexPipeServer exceptionPipe;
		thread::Handle exceptionCommandThread;
	};

	static unsigned int __stdcall CommandThreadProxy(void* context);
	unsigned int CommandThread(DuplexPipeServer* pipe, Event* readyEvent);

	static unsigned int __stdcall ExceptionCommandThreadProxy(void* context);
	unsigned int ExceptionCommandThread(DuplexPipeServer* exceptionPipe);

	void RemoveCommandThread(const DuplexPipe* pipe);

	LiveProcess* FindProcessById(unsigned int processId);

	void CompileChanges(bool didAllProcessesMakeProgress);

	// actions
	struct TriggerRecompileAction
	{
		typedef commands::TriggerRecompile CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct BuildPatchAction
	{
		typedef commands::BuildPatch CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct HandleExceptionAction
	{
		typedef commands::HandleException CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct ReadyForCompilationAction
	{
		typedef commands::ReadyForCompilation CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct DisconnectClientAction
	{
		typedef commands::DisconnectClient CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	// BEGIN EPIC MOD - Adding ShowConsole command
	struct ShowConsoleAction
	{
		typedef commands::ShowConsole CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding ShowConsole command
	struct SetVisibleAction
	{
		typedef commands::SetVisible CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetActive command
	struct SetActiveAction
	{
		typedef commands::SetActive CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetBuildArguments command
	struct SetBuildArgumentsAction
	{
		typedef commands::SetBuildArguments CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding support for lazy-loading modules
	struct EnableLazyLoadedModuleAction
	{
		typedef commands::EnableLazyLoadedModule CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct FinishedLazyLoadingModulesAction
	{
		typedef commands::FinishedLazyLoadingModules CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};
	// END EPIC MOD

	struct RegisterProcessAction
	{
		typedef commands::RegisterProcess CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct EnableModuleBatchBeginAction
	{
		typedef commands::EnableModuleBatchBegin CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct EnableModuleBatchEndAction
	{
		typedef commands::EnableModuleBatchEnd CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct DisableModuleBatchBeginAction
	{
		typedef commands::DisableModuleBatchBegin CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct DisableModuleBatchEndAction
	{
		typedef commands::DisableModuleBatchEnd CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct EnableModuleAction
	{
		typedef commands::EnableModule CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct EnableAllModulesAction
	{
		typedef commands::EnableAllModules CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct DisableModuleAction
	{
		typedef commands::DisableModule CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct DisableAllModulesAction
	{
		typedef commands::DisableAllModules CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct GetModuleInfoAction
	{
		typedef commands::GetModuleInfo CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct ApplySettingBoolAction
	{
		typedef commands::ApplySettingBool CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct ApplySettingIntAction
	{
		typedef commands::ApplySettingInt CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};

	struct ApplySettingStringAction
	{
		typedef commands::ApplySettingString CommandType;
		static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);
	};


	std::wstring m_processGroupName;
	RunMode::Enum m_runMode;

	MainFrame* m_mainFrame;
	thread::Handle m_serverThread;
	thread::Handle m_compileThread;

	types::vector<LiveModule*> m_liveModules;
	types::vector<LiveProcess*> m_liveProcesses;
	types::unordered_map<executable::Header, LiveModule*> m_imageHeaderToLiveModule;

	CriticalSection m_actionCS;
	CriticalSection m_exceptionCS;
	Event m_inExceptionHandlerEvent;
	Event m_handleCommandsEvent;

	// directory cache for all modules combined
	DirectoryCache* m_directoryCache;

	// keeping track of the client connections
	CriticalSection m_connectionCS;
	types::vector<CommandThreadContext*> m_commandThreads;

	telemetry::Scope m_moduleBatchScope;
	size_t m_loadedCompilandCountInBatchScope;

	// BEGIN EPIC MOD - Adding SetActive command
	bool m_active = true;
	// END EPIC MOD

	// for triggering recompiles using the API
	bool m_manualRecompileTriggered;
	types::unordered_map<std::wstring, types::vector<std::wstring>> m_liveModuleToModifiedOrNewObjFiles;
};
