// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_ClientCommandActions.h"
#include "LC_ClientUserCommandThread.h"
#include "LC_DuplexPipe.h"
#include "LC_SyncPoint.h"
#include "LC_Executable.h"
#include "LC_Event.h"
#include "LC_Process.h"
#include "LC_Logging.h"


bool actions::RegisterProcessFinished::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	pipe->SendAck();

	bool* successfullyRegisteredProcess = static_cast<bool*>(context);
	*successfullyRegisteredProcess = command->success;

	// don't continue execution
	return false;
}


bool actions::EnableModuleFinished::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	Event* event = static_cast<Event*>(command->token);
	event->Signal();
	pipe->SendAck();

	return false;
}


bool actions::DisableModuleFinished::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	Event* event = static_cast<Event*>(command->token);
	event->Signal();
	pipe->SendAck();

	return false;
}


bool actions::EnableAllModulesFinished::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	Event* event = static_cast<Event*>(command->token);
	event->Signal();
	pipe->SendAck();

	return false;
}


bool actions::DisableAllModulesFinished::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	Event* event = static_cast<Event*>(command->token);
	event->Signal();
	pipe->SendAck();

	return false;
}


bool actions::EnterSyncPoint::Execute(CommandType*, const DuplexPipe* pipe, void*)
{
	syncPoint::Enter();
	pipe->SendAck();

	return true;
}


bool actions::LeaveSyncPoint::Execute(CommandType*, const DuplexPipe* pipe, void*)
{
	syncPoint::Leave();
	pipe->SendAck();

	return true;
}


bool actions::CallHooks::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	for (const hook::Function* hook = command->first; hook < command->last; ++hook)
	{
		// note that sections are often padded with zeroes, so skip everything that's zero
		hook::Function function = *hook;
		if (function)
		{
			function();
		}
	}

	pipe->SendAck();

	return true;
}


bool actions::GetModule::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	HMODULE module = ::GetModuleHandleW(command->path);
	if (module)
	{
		wchar_t fullPath[MAX_PATH];
		::GetModuleFileNameW(module, fullPath, MAX_PATH);

		// send back command with module info
		{
			commands::GetModuleInfo cmd = { module, process::GetId(), command->loadImports, command->taskContext };
			wcscpy_s(cmd.path, fullPath);

			pipe->SendAck();

			pipe->SendCommandAndWaitForAck(cmd);
		}
	}
	else
	{
		pipe->SendAck();

		pipe->SendCommandAndWaitForAck(commands::GetModuleInfo { nullptr, process::GetId(), command->loadImports, command->taskContext });
	}

	return true;
}


// BEGIN EPIC MOD - Support for UE4 debug visualizers
struct FNameEntry;
extern FNameEntry*** GFNameTableForDebuggerVisualizers_MT;

class FChunkedFixedUObjectArray;
extern FChunkedFixedUObjectArray*& GObjectArrayForDebugVisualizers;
// END EPIC MOD


bool actions::LoadPatch::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	// load library into this process
	HMODULE module = ::LoadLibraryW(command->path);

	// BEGIN EPIC MOD - Support for UE4 debug visualizers
	if (module != nullptr)
	{
		typedef void InitNatvisHelpersFunc(FNameEntry*** NameTable, FChunkedFixedUObjectArray* ObjectArray);

		InitNatvisHelpersFunc* InitNatvisHelpers = (InitNatvisHelpersFunc*)(void*)GetProcAddress(module, "InitNatvisHelpers");
		if (InitNatvisHelpers != nullptr)
		{
			(*InitNatvisHelpers)(GFNameTableForDebuggerVisualizers_MT, GObjectArrayForDebugVisualizers);
		}
	}
	// END EPIC MOD

	pipe->SendAck();

	// send back command with module info
	pipe->SendCommandAndWaitForAck(commands::LoadPatchInfo { module });

	return true;
}


bool actions::UnloadPatch::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	// unload library from this process
	::FreeLibrary(command->module);
	pipe->SendAck();

	return true;
}


bool actions::CallEntryPoint::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	executable::CallDllEntryPoint(command->moduleBase, command->entryPointRva);
	pipe->SendAck();

	return true;
}


bool actions::LogOutput::Execute(CommandType* command, const DuplexPipe* pipe, void*)
{
	logging::LogNoFormat<logging::Channel::USER>(command->buffer);
	pipe->SendAck();

	return true;
}

// BEGIN EPIC MOD - Notification that compilation has finished
extern bool GIsCompileActive;
// END EPIC MOD

bool actions::CompilationFinished::Execute(CommandType*, const DuplexPipe* pipe, void*)
{
	pipe->SendAck();

	// BEGIN EPIC MOD - Notification that compilation has finished
	GIsCompileActive = false;
	// END EPIC MOD

	// don't continue execution
	return false;
}


bool actions::HandleExceptionFinished::Execute(CommandType* command, const DuplexPipe* pipe, void* context)
{
	ClientUserCommandThread::ExceptionResult* resultContext = static_cast<ClientUserCommandThread::ExceptionResult*>(context);
	resultContext->returnAddress = command->returnAddress;
	resultContext->framePointer = command->framePointer;
	resultContext->stackPointer = command->stackPointer;
	resultContext->continueExecution = command->continueExecution;

	pipe->SendAck();

	// don't continue execution
	return false;
}
