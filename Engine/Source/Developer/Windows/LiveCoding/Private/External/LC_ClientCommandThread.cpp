// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_ClientCommandThread.h"
#include "LC_Event.h"
#include "LC_CommandMap.h"
#include "LC_CriticalSection.h"
#include "LC_ClientCommandActions.h"
#include "LC_DuplexPipeClient.h"
#include "LC_Process.h"
#include "LC_HeartBeat.h"


ClientCommandThread::ClientCommandThread(DuplexPipeClient* pipeClient)
	: m_thread(INVALID_HANDLE_VALUE)
	, m_pipe(pipeClient)
{
}


ClientCommandThread::~ClientCommandThread(void)
{
}


unsigned int ClientCommandThread::Start(const std::wstring& processGroupName, Event* compilationEvent, Event* waitForStartEvent, CriticalSection* pipeAccessCS)
{
	// spawn a thread that communicates with the server
	ThreadContext* context = new ThreadContext;
	context->thisInstance = this;
	context->processGroupName = processGroupName;
	context->compilationEvent = compilationEvent;
	context->waitForStartEvent = waitForStartEvent;
	context->pipeAccessCS = pipeAccessCS;

	m_thread = thread::Create(128u * 1024u, &ThreadProxy, context);

	return thread::GetId(m_thread);
}


void ClientCommandThread::Join(void)
{
	if (m_thread != INVALID_HANDLE_VALUE)
	{
		thread::Join(m_thread);
		thread::Close(m_thread);
	}
}


unsigned int __stdcall ClientCommandThread::ThreadProxy(void* context)
{
	thread::SetName("Live coding commands");

	ThreadContext* realContext = static_cast<ThreadContext*>(context);
	const unsigned int exitCode = realContext->thisInstance->ThreadFunction(realContext->processGroupName, realContext->compilationEvent, realContext->waitForStartEvent, realContext->pipeAccessCS);

	delete realContext;

	return exitCode;
}


unsigned int ClientCommandThread::ThreadFunction(const std::wstring& processGroupName, Event* compilationEvent, Event* waitForStartEvent, CriticalSection* pipeAccessCS)
{
	waitForStartEvent->Wait();

	CommandMap commandMap;
	commandMap.RegisterAction<actions::LoadPatch>();
	commandMap.RegisterAction<actions::UnloadPatch>();
	commandMap.RegisterAction<actions::EnterSyncPoint>();
	commandMap.RegisterAction<actions::LeaveSyncPoint>();
	commandMap.RegisterAction<actions::CallEntryPoint>();
	commandMap.RegisterAction<actions::CallHooks>();
	commandMap.RegisterAction<actions::LogOutput>();
	commandMap.RegisterAction<actions::CompilationFinished>();

	HeartBeat heartBeat(processGroupName.c_str(), process::GetId());

	for (;;)
	{
		// wait for compilation to start
		while (!compilationEvent->WaitTimeout(10))
		{
			if (!m_pipe->IsValid())
			{
				// pipe was closed or is broken, bail out
				return 1u;
			}

			heartBeat.Store();
		}

		if (!m_pipe->IsValid())
		{
			// pipe was closed or is broken, bail out
			return 1u;
		}

		// lock critical section for accessing the pipe.
		// we need to make sure that other threads talking through the pipe don't use it at the same time.
		CriticalSection::ScopedLock lock(pipeAccessCS);

		m_pipe->SendCommandAndWaitForAck(commands::ReadyForCompilation {});

		commandMap.HandleCommands(m_pipe, nullptr);
	}

	return 0u;
}
