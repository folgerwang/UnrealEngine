// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_Semaphore.h"
#include "LC_CriticalSection.h"
#include <stdint.h>


namespace scheduler
{
	class TaskBase;


	// simple multi-producer, multi-consumer queue
	class TaskQueue
	{
		static const unsigned int TASK_COUNT = 1024u;
		static const unsigned int ACCESS_MASK = TASK_COUNT - 1u;

	public:
		TaskQueue(void);

		// blocks when there is no more room for a task
		void PushTask(TaskBase* task);

		// blocks when there is no task in the queue
		TaskBase* PopTask(void);

		// does not block
		TaskBase* TryPopTask(void);

	private:
		TaskBase* m_tasks[TASK_COUNT];
		uint64_t m_readIndex;
		uint64_t m_writeIndex;
		Semaphore m_producerSema;
		Semaphore m_consumerSema;
		CriticalSection m_cs;
	};
}
