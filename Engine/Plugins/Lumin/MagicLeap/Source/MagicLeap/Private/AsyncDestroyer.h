// %BANNER_BEGIN%
// ---------------------------------------------------------------------
// %COPYRIGHT_BEGIN%
//
// Copyright (c) 201x Magic Leap, Inc. (COMPANY) All Rights Reserved.
// Magic Leap, Inc. Confidential and Proprietary
//
// NOTICE: All information contained herein is, and remains the property
// of COMPANY. The intellectual and technical concepts contained herein
// are proprietary to COMPANY and may be covered by U.S. and Foreign
// Patents, patents in process, and are protected by trade secret or
// copyright law. Dissemination of this information or reproduction of
// this material is strictly forbidden unless prior written permission is
// obtained from COMPANY. Access to the source code contained herein is
// hereby forbidden to anyone except current COMPANY employees, managers
// or contractors who have executed Confidentiality and Non-disclosure
// agreements explicitly covering such access.
//
// The copyright notice above does not evidence any actual or intended
// publication or disclosure of this source code, which includes
// information that is confidential and/or proprietary, and is a trade
// secret, of COMPANY. ANY REPRODUCTION, MODIFICATION, DISTRIBUTION,
// PUBLIC PERFORMANCE, OR PUBLIC DISPLAY OF OR THROUGH USE OF THIS
// SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT OF COMPANY IS
// STRICTLY PROHIBITED, AND IN VIOLATION OF APPLICABLE LAWS AND
// INTERNATIONAL TREATIES. THE RECEIPT OR POSSESSION OF THIS SOURCE
// CODE AND/OR RELATED INFORMATION DOES NOT CONVEY OR IMPLY ANY RIGHTS
// TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE,
// USE, OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
//
// %COPYRIGHT_END%
// --------------------------------------------------------------------
// %BANNER_END%

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
