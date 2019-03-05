// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#include "LC_SchedulerWorkerThread.h"
#include "LC_SchedulerQueue.h"
#include "LC_SchedulerTask.h"


namespace
{
	struct ThreadContext
	{
		scheduler::WorkerThread* instance;
		scheduler::TaskQueue* queue;
	};
}


scheduler::WorkerThread::WorkerThread(TaskQueue* queue)
	: m_thread()
{
	ThreadContext* context = new ThreadContext;
	context->instance = this;
	context->queue = queue;
	
	m_thread = thread::Create(128u * 1024u, &ThreadProxy, context);
}


scheduler::WorkerThread::~WorkerThread(void)
{
	thread::Join(m_thread);
}


unsigned int __stdcall scheduler::WorkerThread::ThreadProxy(void* context)
{
	thread::SetName("Live coding worker");

	ThreadContext* realContext = static_cast<ThreadContext*>(context);
	const unsigned int exitCode = realContext->instance->ThreadFunction(realContext->queue);

	delete realContext;

	return exitCode;
}


unsigned int scheduler::WorkerThread::ThreadFunction(TaskQueue* queue)
{
	for (;;)
	{
		// get a task from the queue and execute it
		TaskBase* task = queue->PopTask();
		if (task == nullptr)
		{
			break;
		}

		task->Execute();
	}

	return 0u;
}
