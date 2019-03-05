// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Thread.h"


namespace scheduler
{
	class TaskQueue;


	class WorkerThread
	{
	public:
		explicit WorkerThread(TaskQueue* queue);
		~WorkerThread(void);

	private:
		static unsigned int __stdcall ThreadProxy(void* context);
		unsigned int ThreadFunction(TaskQueue* queue);

		thread::Handle m_thread;
	};
}
