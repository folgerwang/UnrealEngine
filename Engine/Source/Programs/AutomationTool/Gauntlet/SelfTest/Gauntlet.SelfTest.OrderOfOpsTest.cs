// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet.SelfTest
{
	enum TestStages
	{
		Init,
		SubTests,
		Launch,
		Tick,
		Status,
		Shutdown,
		OnComplete,
		Result,
	};

	/// <summary>
	/// Tests that all functions are called in the expected order
	/// </summary>
	class OrderOfOpsTest : ITestNode
	{
		TestStages CurrentStage = TestStages.Init;
		bool Completed;

		public float MaxDuration
		{
			get { return 0;	}
		}

		public string Name
		{
			get	{ return ToString();}
		}

		protected void SetNewStage(TestStages NewStage)
		{
			int CurrentValue = (int)CurrentStage;
			int NewValue = (int)NewStage;

			if (NewValue <= CurrentValue)
			{
				throw new TestException("New state {0} is <= current state {1}", NewStage, CurrentStage);
			}

			int Delta = NewValue - CurrentValue;
			if (Delta > 1)
			{
				throw new TestException("New state {0} is {1} steps more than the current state {2}", NewStage, Delta, CurrentStage);
			}

			CurrentStage = NewStage;
		}

		public TestResult Result
		{
			get
			{
				// this is the last stage and can hit multiple times.
				// we can just return Passed because SetNewStage will 
				// throw an exception if something is wrong here
				if (CurrentStage != TestStages.Result)
				{
					SetNewStage(TestStages.Result);
				}
				return TestResult.Passed;
			}
		}

		public TestStatus Status
		{
			get
			{
				SetNewStage(TestStages.Status);
				return Completed ? TestStatus.Complete : TestStatus.InProgress;
			}
		}

		public ITestNode[] GetSubTests()
		{
			SetNewStage(TestStages.SubTests);
			return new ITestNode[0];
		}

		public bool LaunchTest()
		{
			SetNewStage(TestStages.Launch);
			return true;
		}

		public void OnComplete(bool WasCancelled)
		{
			SetNewStage(TestStages.OnComplete);
		}

		public void OnTick()
		{
			SetNewStage(TestStages.Tick);
			Completed = true;
		}

		public bool RestartTest()
		{
			throw new NotImplementedException();
		}

		public void SetContext(ITestContext InContext)
		{
		}

		public void ShutdownTest()
		{
			SetNewStage(TestStages.Shutdown);
		}
	}
}
