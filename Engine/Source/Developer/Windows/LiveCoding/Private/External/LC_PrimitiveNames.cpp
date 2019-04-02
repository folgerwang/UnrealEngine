// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_PrimitiveNames.h"


// main suffix for all live coding primitives except pipes
#define LPP						L"_UE_LC"
#define LPP_JOB					LPP L"_JOB"
#define LPP_MUTEX				LPP L"_IPM"
#define LPP_MEMORY				LPP L"_NSM"
#define LPP_SERVER_READY		LPP L"_SR"
#define LPP_COMPILE				LPP L"_CMP"
#define LPP_PIPE				L"\\\\.\\pipe\\UE_LC"
#define LPP_EXCEPTION_PIPE		L"\\\\.\\pipe\\UE_LC_EXC"
#define LPP_HEARTBEAT_MUTEX		LPP_MUTEX L"_HB"
#define LPP_HEARTBEAT_MEMORY	LPP_MEMORY L"_HB"


std::wstring primitiveNames::JobGroup(const std::wstring& processGroupName)
{
	std::wstring name;
	name.reserve(128u);

	name += processGroupName;
	name += LPP_JOB;

	return name;
}


std::wstring primitiveNames::StartupMutex(const std::wstring& processGroupName)
{
	std::wstring name;
	name.reserve(128u);

	name += processGroupName;
	name += LPP_MUTEX;

	return name;
}


std::wstring primitiveNames::StartupNamedSharedMemory(const std::wstring& processGroupName)
{
	std::wstring name;
	name.reserve(128u);

	name += processGroupName;
	name += LPP_MEMORY;

	return name;
}


std::wstring primitiveNames::ServerReadyEvent(const std::wstring& processGroupName)
{
	std::wstring name;
	name.reserve(128u);

	name += processGroupName;
	name += LPP_SERVER_READY;

	return name;
}


std::wstring primitiveNames::CompilationEvent(const std::wstring& processGroupName)
{
	std::wstring name;
	name.reserve(128u);

	name += processGroupName;
	name += LPP_COMPILE;

	return name;
}


std::wstring primitiveNames::Pipe(const std::wstring& processGroupName)
{
	std::wstring name;
	name.reserve(128u);

	name += LPP_PIPE;
	name += processGroupName;

	return name;
}


std::wstring primitiveNames::ExceptionPipe(const std::wstring& processGroupName)
{
	std::wstring name;
	name.reserve(128u);

	name += LPP_EXCEPTION_PIPE;
	name += processGroupName;

	return name;
}


std::wstring primitiveNames::HeartBeatMutex(const std::wstring& processGroupName, unsigned int processId)
{
	std::wstring name;
	name.reserve(128u);

	name += processGroupName;
	name += LPP_HEARTBEAT_MUTEX;
	name += std::to_wstring(processId);

	return name;
}


std::wstring primitiveNames::HeartBeatNamedSharedMemory(const std::wstring& processGroupName, unsigned int processId)
{
	std::wstring name;
	name.reserve(128u);

	name += processGroupName;
	name += LPP_HEARTBEAT_MEMORY;
	name += std::to_wstring(processId);

	return name;
}


#undef LPP
#undef LPP_JOB
#undef LPP_MUTEX
#undef LPP_MEMORY
#undef LPP_SERVER_READY
#undef LPP_COMPILE
#undef LPP_PIPE
#undef LPP_EXCEPTION_PIPE
#undef LPP_HEARTBEAT_MUTEX
#undef LPP_HEARTBEAT_MEMORY
