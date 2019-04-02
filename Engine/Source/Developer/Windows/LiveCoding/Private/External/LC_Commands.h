// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "Windows/WindowsHWrapper.h"

namespace hook
{
	typedef void (*Function)(void);
}

namespace commands
{
	// acknowledge that a command has been received
	struct Acknowledge
	{
		static const uint32_t ID = 100;
	};

	// register a process with Live++
	struct RegisterProcess
	{
		static const uint32_t ID = Acknowledge::ID + 1u;

		unsigned int processId;
		void* processBase;
		unsigned int threadId;		// thread ID of Live++ thread running in host
	};

	// tell the DLL that registration has finished
	struct RegisterProcessFinished
	{
		static const uint32_t ID = RegisterProcess::ID + 1u;

		bool success;
	};

	// tell Live++ that a new batch of modules is going to be loaded
	struct EnableModuleBatchBegin
	{
		static const uint32_t ID = RegisterProcessFinished::ID + 1u;
	};
	struct EnableModuleBatchEnd
	{
		static const uint32_t ID = EnableModuleBatchBegin::ID + 1u;
	};


	// tell Live++ that a new batch of modules is going to be unloaded
	struct DisableModuleBatchBegin
	{
		static const uint32_t ID = EnableModuleBatchEnd::ID + 1u;
	};
	struct DisableModuleBatchEnd
	{
		static const uint32_t ID = DisableModuleBatchBegin::ID + 1u;
	};


	// tell Live++ to enable a module for live coding
	struct EnableModule
	{
		static const uint32_t ID = DisableModuleBatchEnd::ID + 1u;

		unsigned int processId;
		wchar_t path[WINDOWS_MAX_PATH];
		void* token;
	};

	// tell the DLL that enabling a module has finished
	struct EnableModuleFinished
	{
		static const uint32_t ID = EnableModule::ID + 1u;

		void* token;
	};

	// tell Live++ to disable a module for live coding
	struct DisableModule
	{
		static const uint32_t ID = EnableModuleFinished::ID + 1u;

		unsigned int processId;
		wchar_t path[WINDOWS_MAX_PATH];
		void* token;
	};

	// tell the DLL that disabling a module has finished
	struct DisableModuleFinished
	{
		static const uint32_t ID = DisableModule::ID + 1u;

		void* token;
	};

	// tell Live++ to enable a module and all its imports for live coding
	struct EnableAllModules
	{
		static const uint32_t ID = DisableModuleFinished::ID + 1u;

		unsigned int processId;
		wchar_t path[WINDOWS_MAX_PATH];
		void* token;
	};

	// tell the DLL that enabling all modules has finished
	struct EnableAllModulesFinished
	{
		static const uint32_t ID = EnableAllModules::ID + 1u;

		void* token;
	};

	// tell Live++ to disable a module and all its imports for live coding
	struct DisableAllModules
	{
		static const uint32_t ID = EnableAllModulesFinished::ID + 1u;

		unsigned int processId;
		wchar_t path[WINDOWS_MAX_PATH];
		void* token;
	};

	// tell the DLL that disabling all modules has finished
	struct DisableAllModulesFinished
	{
		static const uint32_t ID = DisableAllModules::ID + 1u;

		void* token;
	};

	// tell the DLL to enter the synchronization point
	struct EnterSyncPoint
	{
		static const uint32_t ID = DisableAllModulesFinished::ID + 1u;
	};

	// tell the DLL to leave the synchronization point
	struct LeaveSyncPoint
	{
		static const uint32_t ID = EnterSyncPoint::ID + 1u;
	};

	// tell the DLL to call hooks
	struct CallHooks
	{
		static const uint32_t ID = LeaveSyncPoint::ID + 1u;

		const hook::Function* first;
		const hook::Function* last;
	};

	// tell the DLL to give us information about a module
	struct GetModule
	{
		static const uint32_t ID = CallHooks::ID + 1u;

		bool loadImports;
		void* taskContext;
		wchar_t path[WINDOWS_MAX_PATH];
	};

	// returns module info to Live++
	struct GetModuleInfo
	{
		static const uint32_t ID = GetModule::ID + 1u;

		Windows::HMODULE moduleBase;
		unsigned int processId;
		bool loadImports;
		void* taskContext;
		wchar_t path[WINDOWS_MAX_PATH];
	};

	// tell the DLL to load a DLL
	struct LoadPatch
	{
		static const uint32_t ID = GetModuleInfo::ID + 1u;

		wchar_t path[WINDOWS_MAX_PATH];
	};

	// returns info about a loaded DLL to Live++
	struct LoadPatchInfo
	{
		static const uint32_t ID = LoadPatch::ID + 1u;

		Windows::HMODULE module;
	};

	// tell the DLL to unload a DLL
	struct UnloadPatch
	{
		static const uint32_t ID = LoadPatchInfo::ID + 1u;

		Windows::HMODULE module;
	};

	// tell the DLL to call the entry point of a DLL
	struct CallEntryPoint
	{
		static const uint32_t ID = UnloadPatch::ID + 1u;

		void* moduleBase;
		uint32_t entryPointRva;
	};

	// tell the DLL to log output
	struct LogOutput
	{
		static const uint32_t ID = CallEntryPoint::ID + 1u;

		static const size_t BUFFER_SIZE = 1024u;

		size_t size;
		wchar_t buffer[BUFFER_SIZE];
	};

	// tell Live++ server we're ready for compilation
	struct ReadyForCompilation
	{
		static const uint32_t ID = LogOutput::ID + 1u;
	};

	// tell the DLL that compilation has finished
	struct CompilationFinished
	{
		static const uint32_t ID = ReadyForCompilation::ID + 1u;
	};

	// tell Live++ server that a client is about to disconnect
	struct DisconnectClient
	{
		static const uint32_t ID = CompilationFinished::ID + 1u;
	};

	// tell Live++ to trigger a recompile
	struct TriggerRecompile
	{
		static const uint32_t ID = DisconnectClient::ID + 1u;
	};

	// tell Live++ to build a patch using an array of object files
	struct BuildPatch
	{
		static const uint32_t ID = TriggerRecompile::ID + 1u;

		// number of BuildPatchPackets to follow this command
		unsigned int count;
	};

	struct BuildPatchPacket
	{
		static const uint32_t ID = BuildPatch::ID + 1u;

		wchar_t moduleName[MAX_PATH];
		wchar_t objPath[MAX_PATH];
	};

	// tell Live++ to handle an exception
	struct HandleException
	{
		static const uint32_t ID = BuildPatchPacket::ID + 1u;

		unsigned int processId;
		unsigned int threadId;
		EXCEPTION_RECORD exception;
		CONTEXT context;
		CONTEXT* clientContextPtr;
	};

	// tell the DLL that handling an exception has finished
	struct HandleExceptionFinished
	{
		static const uint32_t ID = HandleException::ID + 1u;

		const void* returnAddress;
		const void* framePointer;
		const void* stackPointer;
		bool continueExecution;
	};

	// BEGIN EPIC MOD - Adding ShowConsole command
	struct ShowConsole
	{
		static const uint32_t ID = HandleExceptionFinished::ID + 1u;
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetVisible command
	struct SetVisible
	{
		static const uint32_t ID = ShowConsole::ID + 1u;

		bool visible;
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetActive command
	struct SetActive
	{
		static const uint32_t ID = SetVisible::ID + 1u;

		bool active;
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Adding SetBuildArguments command
	struct SetBuildArguments
	{
		static const uint32_t ID = SetActive::ID + 1u;

		unsigned int processId;
		wchar_t arguments[1024];
	};
	// END EPIC MOD

	// BEGIN EPIC MOD - Support for lazy-loading modules
	struct EnableLazyLoadedModule
	{
		static const uint32_t ID = SetBuildArguments::ID + 1u;

		unsigned int processId;
		wchar_t fileName[260];
		Windows::HMODULE moduleBase;
	};

	struct FinishedLazyLoadingModules
	{
		static const uint32_t ID = EnableLazyLoadedModule::ID + 1u;
	};
	// END EPIC MOD

	// tell the EXE that a bool setting needs to be changed
	struct ApplySettingBool
	{
		// BEGIN EPIC MOD - Support for lazy-loading modules
		static const uint32_t ID = FinishedLazyLoadingModules::ID + 1u;
		// END EPIC MOD

		char settingName[256];
		int settingValue;
	};

	// tell the EXE that a int setting needs to be changed
	struct ApplySettingInt
	{
		static const uint32_t ID = ApplySettingBool::ID + 1u;

		char settingName[256];
		int settingValue;
	};

	// tell the EXE that a bool setting needs to be changed
	struct ApplySettingString
	{
		static const uint32_t ID = ApplySettingInt::ID + 1u;

		char settingName[256];
		wchar_t settingValue[256];
	};

	static const uint32_t COUNT = ApplySettingString::ID + 1u;
}
