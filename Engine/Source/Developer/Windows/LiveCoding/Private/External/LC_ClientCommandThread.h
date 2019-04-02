// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Thread.h"
#include <string>

class DuplexPipeClient;
class Event;
class CriticalSection;


// handles incoming commands from the Live++ server
class ClientCommandThread
{
public:
	explicit ClientCommandThread(DuplexPipeClient* pipeClient);
	~ClientCommandThread(void);

	// Starts the thread that takes care of handling incoming commands on the pipe.
	// Returns the thread ID.
	unsigned int Start(const std::wstring& processGroupName, Event* compilationEvent, Event* waitForStartEvent, CriticalSection* pipeAccessCS);

	// Joins this thread.
	void Join(void);

private:
	struct ThreadContext
	{
		ClientCommandThread* thisInstance;
		std::wstring processGroupName;
		Event* compilationEvent;
		Event* waitForStartEvent;
		CriticalSection* pipeAccessCS;
	};

	static unsigned int __stdcall ThreadProxy(void* context);
	unsigned int ThreadFunction(const std::wstring& processGroupName, Event* compilationEvent, Event* waitForStartEvent, CriticalSection* pipeAccessCS);

	thread::Handle m_thread;
	DuplexPipeClient* m_pipe;
};
