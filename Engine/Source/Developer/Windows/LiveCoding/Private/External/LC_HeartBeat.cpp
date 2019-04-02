// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_HeartBeat.h"
#include "LC_InterprocessMutex.h"
#include "LC_NamedSharedMemory.h"
#include "LC_UtcTime.h"
#include "LC_PrimitiveNames.h"


HeartBeat::HeartBeat(const wchar_t* const processGroupName, unsigned int processId)
	: m_mutex(nullptr)
	, m_memory(nullptr)
{
	m_mutex = new InterprocessMutex(primitiveNames::HeartBeatMutex(processGroupName, processId).c_str());
	m_memory = new NamedSharedMemory(primitiveNames::HeartBeatNamedSharedMemory(processGroupName, processId).c_str());
}


HeartBeat::~HeartBeat(void)
{
	delete m_memory;
	delete m_mutex;
}


void HeartBeat::Store(void)
{
	const uint64_t currentTime = utcTime::GetCurrent();

	InterprocessMutex::ScopedLock lock(m_mutex);
	m_memory->Write(currentTime);
}


uint64_t HeartBeat::ReadBeatDelta(void) const
{
	const uint64_t currentTime = utcTime::GetCurrent();
	const uint64_t heartBeat = ReadBeat();

	if (currentTime >= heartBeat)
	{
		return currentTime - heartBeat;
	}
	else
	{
		return heartBeat - currentTime;
	}
}


uint64_t HeartBeat::ReadBeat(void) const
{
	InterprocessMutex::ScopedLock lock(m_mutex);
	return m_memory->Read<uint64_t>();
}
