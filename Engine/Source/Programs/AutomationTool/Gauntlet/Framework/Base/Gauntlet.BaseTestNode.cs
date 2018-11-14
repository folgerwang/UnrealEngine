// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet
{
	/// <summary>
	/// Simple node that implements the ITestNode interface and provides some convenience functions for
	/// inherited tests to use
	/// </summary>
	public abstract class BaseTest : ITestNode
	{
		/// <summary>
		/// Override this to set the max running time for this test
		/// </summary>
		public abstract float MaxDuration { get; protected set; }

		/// <summary>
		/// Override this to set the priority of this test
		/// </summary>
		public virtual TestPriority Priority { get { return TestPriority.Normal; } }

		/// <summary>
		/// Return the name of this test
		/// </summary>
		public abstract string Name { get;  }

		/// <summary>
		/// Returns true if the test has encountered warnings. Test is expected to list any warnings it considers appropriate in the summary
		/// </summary>
		public virtual bool HasWarnings { get; protected set; }

		/// <summary>
		/// Internal status state
		/// </summary>
		private TestStatus InnerStatus;

		/// <summary>
		/// 
		/// </summary>
		public BaseTest()
		{
			InnerStatus = TestStatus.NotStarted;
		}

		/// <summary>
		/// OVerride this to return the result of the test
		/// </summary>
		/// <returns></returns>
		public abstract TestResult GetTestResult();

		/// <summary>
		/// Summarize the result of the test
		/// </summary>
		/// <returns></returns>
		public virtual string GetTestSummary()
		{
			return string.Format("{0}: {1}", Name, GetTestResult());
		}

		/// <summary>
		/// Mark the test as started
		/// </summary>
		/// <returns></returns>
		protected void MarkTestStarted()
		{
			InnerStatus = TestStatus.InProgress;
		}

		/// <summary>
		/// Mark the test as complete
		/// </summary>
		/// <returns></returns>
		protected void MarkTestComplete()
		{
			InnerStatus = TestStatus.Complete;
		}

		/// <summary>
		/// Return our internal status
		/// </summary>
		/// <returns></returns>
		public TestStatus GetTestStatus()
		{
			return InnerStatus;
		}

		public virtual bool IsReadyToStart()
		{
			return true;
		}

		/// <summary>
		/// Called to request the test launch. Should call MarkTestComplete
		/// </summary>
		/// <returns></returns>
		public abstract bool StartTest(int Pass, int NumPasses);

		/// <summary>
		/// Called to request the test is shutdown
		/// </summary>
		/// <returns></returns>
		public abstract void CleanupTest();

		/// <summary>
		/// Called if TestResult returns WantRetry. Can be overridden for something
		/// more elegant than a hard shutdown/launch
		/// </summary>
		/// <returns></returns>
		public virtual bool RestartTest()
		{
			CleanupTest();
			return StartTest(0,1);
		}

		/// <summary>
		/// Gives the test a chance to perform logic. Should call MarkComplete() when it detects a
		/// success or error state
		/// </summary>
		/// <returns></returns>
		public virtual void TickTest()
		{
		}

		/// <summary>
		/// Called after the test is completed and shutdown
		/// </summary>
		/// <param name="WasCancelled"></param>
		/// <returns></returns>
		public virtual void StopTest(bool WasCancelled)
		{
		}

		/// <summary>
		/// Helper to set context
		/// </summary>
		/// <param name="InContext"></param>
		/// <returns></returns>
		public virtual void SetContext(ITestContext InContext)
		{
		}	
	}
}
