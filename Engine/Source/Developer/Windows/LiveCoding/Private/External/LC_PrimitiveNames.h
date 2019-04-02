// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <string>

namespace primitiveNames
{
	// client
	std::wstring JobGroup(const std::wstring& processGroupName);
	std::wstring StartupMutex(const std::wstring& processGroupName);
	std::wstring StartupNamedSharedMemory(const std::wstring& processGroupName);

	// server
	std::wstring ServerReadyEvent(const std::wstring& processGroupName);
	std::wstring CompilationEvent(const std::wstring& processGroupName);

	// pipes
	std::wstring Pipe(const std::wstring& processGroupName);
	std::wstring ExceptionPipe(const std::wstring& processGroupName);

	// heart beat
	std::wstring HeartBeatMutex(const std::wstring& processGroupName, unsigned int processId);
	std::wstring HeartBeatNamedSharedMemory(const std::wstring& processGroupName, unsigned int processId);
}
