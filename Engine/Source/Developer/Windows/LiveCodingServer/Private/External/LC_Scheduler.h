// Copyright 2011-2019 Molecular Matters GmbH, all rights reserved.

#pragma once

#include "CoreTypes.h"
#include "LC_SchedulerTask.h"


namespace scheduler
{
	void Startup(void);
	void Shutdown(void);

	// creates a new task from any function
	template <typename F>
	Task<typename std::result_of<F()>::type>* CreateTask(F&& function)
	{
		return new Task<typename std::result_of<F()>::type>(function);
	}

	// creates a new task from any function as child of a parent task
	template <typename F>
	Task<typename std::result_of<F()>::type>* CreateTask(TaskBase* parent, F&& function)
	{
		return new Task<typename std::result_of<F()>::type>(parent, function);
	}

	// creates an empty task
	TaskBase* CreateEmptyTask(void);

	// destroys a task
	void DestroyTask(TaskBase* task);

	// destroys a container of tasks
	template <typename T>
	void DestroyTasks(const T& container)
	{
		const size_t count = container.size();
		for (size_t i = 0u; i < count; ++i)
		{
			DestroyTask(container[i]);
		}
	}


	void RunTask(TaskBase* task);

	void WaitForTask(TaskBase* task);
}
