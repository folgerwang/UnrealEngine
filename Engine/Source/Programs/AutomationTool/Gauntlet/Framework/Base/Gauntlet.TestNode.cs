// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;


namespace Gauntlet
{

	/// <summary>
	/// Describe the end result of a test. Until a test is complete
	/// TestResult.Invalid should be returned
	/// </summary>
	public enum TestResult
	{
		Invalid,
		Passed,
		Failed,
		WantRetry
	};

	/// <summary>
	/// Describes the current state of a test. 
	/// </summary>
	public enum TestStatus
	{
		NotStarted,
		Pending,			// Not currently used
		InProgress,
		Complete,
	};

	/// <summary>
	/// Describes the priority of test. 
	/// </summary>
	public enum TestPriority
	{
		Critical,
		High,
		Normal,
		Low,
		Idle
	};

	/// <summary>
	/// The interface that all Gauntlet rests are required to implement. How these are
	/// implemented and whether responsibilities are handed off to other systems (e.g. Launch)
	/// is left to the implementor. It's expected that tests for major systems (e.g. Unreal)
	/// will likely implement their own base node.
	/// </summary>
	public interface ITestNode
	{
		/// <summary>
		/// Name of the test - used for logging / reporting
		/// </summary>
		string Name { get; }

		/// <summary>
		/// Maximum duration that the test is expected to run for. Tests longer than this will be halted.
		/// </summary>
		float MaxDuration { get; }

		/// <summary>
		/// Priority of this test in relation to any others that are running
		/// </summary>
		TestPriority Priority { get;  }

		/// <summary>
		/// Sets the context that this test will run under. TODO - make this more of a contract that happens before CheckRequirements / LaunchTest
		/// </summary>
		/// <param name="InContext"></param>
		/// <returns></returns>
		void SetContext(ITestContext InContext);
		
		/// <summary>
		/// Checks if the test is ready to be started. If the test is not ready to run (e.g. resources not available) then it should return false.
		/// If it determines that it will never be ready it should throw an exception.
		/// </summary>
		/// <returns></returns>
		bool IsReadyToStart();

		/// <summary>
		/// Begin executing the provided test. At this point .Status should return InProgress and the test will eventually receive
		/// OnComplete and ShutdownTest calls
		/// </summary>
		/// <param name="Node"></param>
		/// <returns>true/false based on whether the test successfully launched</returns>
		bool StartTest(int Pass, int NumPasses);

		/// <summary>
		/// Called regularly to allow the test to check and report status
		/// </summary>
		void TickTest();

		/// <summary>
		/// Called to request that the test stop, either because it's reported that its complete or due to external factors.
		/// Tests should consider whether they passed or succeeded (even a terminated test may have gotten all the data it needs) 
		/// and set their result appropriately.
		/// </summary>
		void StopTest(bool WasCancelled);

		/// <summary>
		/// Allows the node to restart with the same assigned devices. Only called if the expresses 
		/// a .Result of TestResult.WantRetry while running.
		/// </summary>
		/// <param name="Node"></param>
		/// <returns></returns>
		bool RestartTest();

		/// <summary>
		/// Return an enum, that describes the state of the test
		/// </summary>
		TestStatus GetTestStatus();

		/// <summary>
		/// The result of the test. Only called once GetTestStatus() returns complete, but may be called multiple
		/// times.
		/// </summary>
		TestResult GetTestResult();

		/// <summary>
		/// Summary of the test. Only called once GetTestStatus() returns complete, but may be called multiple
		/// times.
		/// </summary>
		string GetTestSummary();

		/// <summary>
		/// Returns true if the test has encountered warnings. Test is expected to list any warnings it considers appropriate in the summary
		/// </summary>
		bool HasWarnings { get;  }


		/// <summary>
		/// Called to request any that any necessary cleanup be performed. After CleanupTest is called no further calls will be
		/// made to this test and thus all resources should be released.
		/// </summary>
		/// <param name="Node"></param>
		/// <returns></returns>
		void CleanupTest();	
		
	}
}