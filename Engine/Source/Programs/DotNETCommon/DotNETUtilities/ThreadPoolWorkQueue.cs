// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Allows queuing a large number of tasks to a thread pool and waiting for them all to complete.
	/// </summary>
	public class ThreadPoolWorkQueue : IDisposable
	{
		/// <summary>
		/// Object used for controlling access to NumOutstandingJobs and updating EmptyEvent
		/// </summary>
		object LockObject = new object();

		/// <summary>
		/// Number of jobs remaining in the queue. This is updated in an atomic way.
		/// </summary>
		int NumOutstandingJobs;

		/// <summary>
		/// Event which indicates whether the queue is empty.
		/// </summary>
		ManualResetEvent EmptyEvent = new ManualResetEvent(true);

		/// <summary>
		/// Exceptions which occurred while executing tasks
		/// </summary>
		List<Exception> Exceptions = new List<Exception>();

		/// <summary>
		/// Default constructor
		/// </summary>
		public ThreadPoolWorkQueue()
		{
		}

		/// <summary>
		/// Waits for the queue to drain, and disposes of it
		/// </summary>
		public void Dispose()
		{
			if(EmptyEvent != null)
			{
				Wait();

				EmptyEvent.Dispose();
				EmptyEvent = null;
			}
		}

		/// <summary>
		/// Returns the number of items remaining in the queue
		/// </summary>
		public int NumRemaining
		{
			get { return NumOutstandingJobs; }
		}

		/// <summary>
		/// Adds an item to the queue
		/// </summary>
		/// <param name="ActionToExecute">The action to add</param>
		public void Enqueue(Action ActionToExecute)
		{
			lock(LockObject)
			{
				if(NumOutstandingJobs == 0)
				{
					EmptyEvent.Reset();
				}
				NumOutstandingJobs++;
			}

#if SINGLE_THREAD
			Execute(ActionToExecute);
#else
			ThreadPool.QueueUserWorkItem(Execute, ActionToExecute);
#endif
		}

		/// <summary>
		/// Internal method to execute an action
		/// </summary>
		/// <param name="ActionToExecute">The action to execute</param>
		void Execute(object ActionToExecute)
		{
			try
			{
				((Action)ActionToExecute)();
			}
			catch(Exception Ex)
			{
				lock(LockObject)
				{
					Exceptions.Add(Ex);
				}
				throw;
			}
			finally
			{
				lock(LockObject)
				{
					NumOutstandingJobs--;
					if(NumOutstandingJobs == 0)
					{
						EmptyEvent.Set();
					}
				}
			}
		}

		/// <summary>
		/// Waits for all queued tasks to finish
		/// </summary>
		public void Wait()
		{
			EmptyEvent.WaitOne();
			RethrowExceptions();
		}

		/// <summary>
		/// Waits for all queued tasks to finish, or the timeout to elapse
		/// </summary>
		/// <param name="MillisecondsTimeout">Maximum time to wait</param>
		/// <returns>True if the queue completed, false if the timeout elapsed</returns>
		public bool Wait(int MillisecondsTimeout)
		{
			bool bResult = EmptyEvent.WaitOne(MillisecondsTimeout);
			if(bResult)
			{
				RethrowExceptions();
			}
			return bResult;
		}

		/// <summary>
		/// Checks for any exceptions which ocurred in queued tasks, and re-throws them on the current thread
		/// </summary>
		public void RethrowExceptions()
		{
			lock(LockObject)
			{
				if(Exceptions.Count > 0)
				{
					throw new AggregateException(Exceptions.ToArray());
				}
			}
		}
	}
}
