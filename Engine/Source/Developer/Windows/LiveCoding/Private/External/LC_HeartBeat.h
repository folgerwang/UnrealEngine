// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include <inttypes.h>

class InterprocessMutex;
class NamedSharedMemory;

// used for communicating heart beats between client and server.
// can be used by several processes.
class HeartBeat
{
public:
	HeartBeat(const wchar_t* const processGroupName, unsigned int processId);
	~HeartBeat(void);

	// stores the current UTC time as heart beat
	void Store(void);

	// reads the last stored heart beat and compares it against the current UTC time
	uint64_t ReadBeatDelta(void) const;

private:
	uint64_t ReadBeat(void) const;

	InterprocessMutex* m_mutex;
	NamedSharedMemory* m_memory;
};
