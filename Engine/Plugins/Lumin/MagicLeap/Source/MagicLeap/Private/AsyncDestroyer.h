// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeCounter.h"
#include "Containers/Queue.h"

namespace MagicLeap
{
	/**
	* Destroys objects that have blocking destructors on a worker thread.
	*/
	class FAsyncDestroyer : public FRunnable
	{
	public:
		/** Creates a worker thread to handle the delayed destruction of IEventHandle objects. */
		FAsyncDestroyer();

		/** Stops and destroys the worker thread. */
		virtual ~FAsyncDestroyer();

		/**
			Enqueues the IAppEventHandler instance into a thread safe structure for later deletion on the worker thread.
			@note There should be no references to this object before this function is called.
			@param InEventHandler The event IAppEventHandler instance to be destroyed on the worker thread.
		*/
		virtual void AddRaw(class IAppEventHandler* InEventHandler);

		/** Contains the while loop which is continuously checking for objects to destroy. */
		virtual uint32 Run() override;
		
	private:
		FRunnableThread* Thread;
		FThreadSafeCounter StopTaskCounter;
		TQueue<IAppEventHandler*, EQueueMode::Mpsc> IncomingEventHandlers;
		FEvent* Semaphore;
	};
} // MagicLeap
