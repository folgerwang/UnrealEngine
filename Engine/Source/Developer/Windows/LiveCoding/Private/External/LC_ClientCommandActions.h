// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Commands.h"


class DuplexPipe;

namespace actions
{
	#define DECLARE_ACTION(_name)																	\
		struct _name																				\
		{																							\
			typedef ::commands::_name CommandType;													\
			static bool Execute(CommandType* command, const DuplexPipe* pipe, void* context);		\
		}

	DECLARE_ACTION(RegisterProcessFinished);
	DECLARE_ACTION(EnableModuleFinished);
	DECLARE_ACTION(DisableModuleFinished);
	DECLARE_ACTION(EnableAllModulesFinished);
	DECLARE_ACTION(DisableAllModulesFinished);
	DECLARE_ACTION(EnterSyncPoint);
	DECLARE_ACTION(LeaveSyncPoint);
	DECLARE_ACTION(CallHooks);
	DECLARE_ACTION(GetModule);
	DECLARE_ACTION(LoadPatch);
	DECLARE_ACTION(UnloadPatch);
	DECLARE_ACTION(CallEntryPoint);
	DECLARE_ACTION(LogOutput);
	DECLARE_ACTION(CompilationFinished);
	DECLARE_ACTION(HandleExceptionFinished);

	#undef DECLARE_ACTION
}
