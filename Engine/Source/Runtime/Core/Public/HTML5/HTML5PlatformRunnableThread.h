// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


/*=============================================================================================
	HTML5PlatformRunnableThread.h: HTML5 platform string classes, mostly implemented with ANSI C++
==============================================================================================*/

#pragma once

#ifdef __EMSCRIPTEN_PTHREADS__

#include <pthread.h>
#include <emscripten/threading.h>
#include "Runtime/Core/Private/HAL/PThreadRunnableThread.h"

/**
 * HTML5 implementation of the Pthreads
 */
class FHTML5RunnablePThread
	: public FRunnableThreadPThread
{
protected:

	virtual ~FHTML5RunnablePThread()
	{
		// Call the parent destructor body before the parent does it - see comment on that function for explanation why.
EM_ASM_({ console.log("~FHTML5PlatformProcess DESTROY [" + $0 + "]"); }, this);
		FRunnableThreadPThread_DestructorBody();
	}

	// FRunnableThreadPThread interface

	virtual int CreateThreadWithName(pthread_t* HandlePtr, pthread_attr_t* AttrPtr, PthreadEntryPoint Proc, void* Arg, const ANSICHAR* Name) override
	{
		int rc = pthread_create(HandlePtr, AttrPtr, Proc, Arg);
		if (rc == 0)
		{
			emscripten_set_thread_name(*HandlePtr, Name);
//EM_ASM_ARGS({ var str = Pointer_stringify($0); console.log("*** CreateThreadWithName [" + str + "]"); }, Name);
		}
		return rc;
	}
};

#else // #ifdef __EMSCRIPTEN_PTHREADS__

#include "HAL/RunnableThread.h"

/**
 * @todo html5 threads: Dummy thread class
 */
class FHTML5RunnableThread : public FRunnableThread
{
public:

	virtual void SetThreadPriority (EThreadPriority NewPriority)
	{

	}

	virtual void Suspend (bool bShouldPause = 1)
	{

	}
	virtual bool Kill (bool bShouldWait = false)
	{
		return false;
	}

	virtual void WaitForCompletion ()
	{

	}

public:


	/**
	 * Virtual destructor
	 */
	virtual ~FHTML5RunnableThread ()
	{

	}


protected:

	virtual bool CreateInternal (FRunnable* InRunnable, const TCHAR* InThreadName,
		uint32 InStackSize = 0,
		EThreadPriority InThreadPri = TPri_Normal, uint64 InThreadAffinityMask = 0)
	{
		return false;
	}
};

#endif // #else // #if __EMSCRIPTEN_PTHREADS__

