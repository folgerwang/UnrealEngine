// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Thread.h"
#include <string>

class DuplexPipeClient;
class Event;
class CriticalSection;


// handles incoming commands from the host (the executable that loaded the Live++ DLL)
class ClientUserCommandThread
{
public:
	struct ExceptionResult
	{
		const void* returnAddress;
		const void* framePointer;
		const void* stackPointer;
		bool continueExecution;
	};

	ClientUserCommandThread(DuplexPipeClient* pipeClient, DuplexPipeClient* exceptionPipeClient);
	~ClientUserCommandThread(void);

	// Starts the thread that takes care of handling incoming commands on the pipe.
	// Returns the thread ID.
	unsigned int Start(const std::wstring& processGroupName, Event* waitForStartEvent, CriticalSection* pipeAccessCS);

	// Joins this thread.
	void Join(void);

	void* EnableModule(const wchar_t* const nameOfExeOrDll);
	void* EnableAllModules(const wchar_t* const nameOfExeOrDll);

	void* DisableModule(const wchar_t* const nameOfExeOrDll);
	void* DisableAllModules(const wchar_t* const nameOfExeOrDll);

	void WaitForToken(void* token);
	void TriggerRecompile(void);
	void BuildPatch(const wchar_t* moduleNames[], const wchar_t* objPaths[], unsigned int count);

	void InstallExceptionHandler(void);
	ExceptionResult HandleException(EXCEPTION_RECORD* exception, CONTEXT* context, unsigned int threadId);
	void End(void);

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

	void ApplySettingBool(const char* const settingName, int value);
	void ApplySettingInt(const char* const settingName, int value);
	void ApplySettingString(const char* const settingName, const wchar_t* const value);

private:
	struct ThreadContext
	{
		ClientUserCommandThread* thisInstance;
		Event* waitForStartEvent;
		CriticalSection* pipeAccessCS;
	};

	static unsigned int __stdcall ThreadProxy(void* context);
	unsigned int ThreadFunction(Event* waitForStartEvent, CriticalSection* pipeAccessCS);

	thread::Handle m_thread;
	std::wstring m_processGroupName;
	DuplexPipeClient* m_pipe;
	DuplexPipeClient* m_exceptionPipe;
	Event* m_itemInQueueEvent;
};
