// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Thread.h"
#include "LC_Process.h"
#include "LC_RunMode.h"

class NamedSharedMemory;
class ClientCommandThread;
class ClientUserCommandThread;
class DuplexPipeClient;
class CriticalSection;
class Event;


class ClientStartupThread
{
public:
	explicit ClientStartupThread(HINSTANCE instance);
	~ClientStartupThread(void);

	// Spawns a thread that runs client initialization
	void Start(const char* const groupName, RunMode::Enum runMode);

	// Joins the thread, waiting for initialization to finish
	void Join(void);

	void* EnableModule(const wchar_t* const nameOfExeOrDll);
	void* EnableAllModules(const wchar_t* const nameOfExeOrDll);

	void* DisableModule(const wchar_t* const nameOfExeOrDll);
	void* DisableAllModules(const wchar_t* const nameOfExeOrDll);

	void WaitForToken(void* token);
	void TriggerRecompile(void);
	void BuildPatch(const wchar_t* moduleNames[], const wchar_t* objPaths[], unsigned int count);

	void InstallExceptionHandler(void);

	void ApplySettingBool(const char* const settingName, int value);
	void ApplySettingInt(const char* const settingName, int value);
	void ApplySettingString(const char* const settingName, const wchar_t* const value);

	// BEGIN EPIC MOD - Adding ShowConsole command
	void ShowConsole();
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetVisible command
	void SetVisible(bool visible);
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetActive command
	void SetActive(bool active);
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetBuildArguments command
	void SetBuildArguments(const wchar_t* arguments);
	// END EPIC MOD

	// BEGIN EPIC MOD - Support for lazy-loading modules
	void EnableLazyLoadedModule(const wchar_t* fileName, Windows::HMODULE moduleBase);
	// END EPIC MOD

private:
	struct ThreadContext
	{
		ClientStartupThread* thisInstance;
		std::wstring processGroupName;
		RunMode::Enum runMode;
	};

	static unsigned int __stdcall ThreadProxy(void* context);
	unsigned int ThreadFunction(const std::wstring& groupName, RunMode::Enum runMode);

	HINSTANCE m_instance;

	thread::Handle m_thread;

	// job object for associating spawned processes with main process the DLL is loaded into
	HANDLE m_job;

	// named shared memory for sharing the Live++ process ID between processes
	NamedSharedMemory* m_sharedMemory;

	// main Live++ process. context may be empty in case we connected to an existing Live++ process
	process::Context* m_mainProcessContext;
	process::Handle m_processHandle;

	bool m_successfulInit;

	// pipe used for interprocess communication
	DuplexPipeClient* m_pipeClient;
	DuplexPipeClient* m_exceptionPipeClient;
	CriticalSection* m_pipeClientCS;

	// helper threads taking care of communication with the Live++ server and user code
	ClientCommandThread* m_commandThread;
	ClientUserCommandThread* m_userCommandThread;

	// manual-reset start event that signals to the helper threads that they can start talking to the pipe
	Event* m_startEvent;

	// process-wide event that is signaled by the Live++ server when compilation is about to begin
	Event* m_compilationEvent;
};
