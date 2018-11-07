// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet.SelfTest
{
	public abstract class BaseTestNode : ITestNode
	{
		public virtual float MaxDuration
		{
			get
			{
				return 300;
			}
		}
	
		public TestPriority Priority { get { return TestPriority.Normal; } }

		public string Name
		{
			get
			{
				return ToString();
			}
		}

		public bool HasWarnings { get { return false; } }

		public TestResult GetTestResult()
		{
			return InnerResult;
		}

		public string GetTestSummary()
		{
			return string.Format("{0} {1}", Name, GetTestResult());
		}

		public TestStatus GetTestStatus()
		{
			return InnerStatus;
		}

		public virtual ITestNode[] GetSubTests()
		{
			return new ITestNode[0];
		}
		
		public bool RestartTest()
		{
			return false;
		}

		public void SetContext(ITestContext InContext)
		{
		}

		protected TestResult InnerResult;
		protected TestStatus InnerStatus;

		public virtual bool IsReadyToStart()
		{
			return true;
		}
	
		public virtual bool StartTest(int Pass, int NumPasses)
		{
			InnerStatus = TestStatus.InProgress;
			return true;
		}
		public virtual void CleanupTest()
		{
		}

		public virtual void StopTest(bool WasCancelled)
		{
		}

		public BaseTestNode()
		{
			TestFailures = new List<string>();
		}

		public bool CheckResult(bool Result, string Format, params object[] Args)
		{

			if (Result == false)
			{
				StackTrace Stack = new StackTrace();
				StackFrame StackFrame = Stack.GetFrame(1);
				string TestClass = GetType().Name;

				string UserMessage = string.Format(Format, Args);

				string Msg = string.Format("Test Failure: {0}::{1} {2}", TestClass, StackFrame.GetMethod().Name, UserMessage);
				TestFailures.Add(Msg);
				Log.Error(Msg);
			}
			Trace.Assert(Result);

			return Result;
		}

		protected void MarkComplete(TestResult InResult)
		{
			InnerResult = InResult;
			InnerStatus = TestStatus.Complete;
		}

		protected void MarkComplete()
		{
			MarkComplete(TestFailures.Count == 0 ? TestResult.Passed : TestResult.Failed);
		}

		// Self test interface
		public abstract void TickTest();

		protected List<string> TestFailures;

	}
}
