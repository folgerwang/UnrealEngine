// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading;
using System.Text.RegularExpressions;
using System.Drawing;

namespace Gauntlet
{
	public class TestExecutorOptions
	{
		[AutoParamWithNames(1, "repeat")]
		public int TestLoops;

		[AutoParam(false)]
		public bool StopOnError;

		[AutoParam(false)]
		public bool NoTimeout;

		[AutoParam(300)]
		public int Wait;

		[AutoParam(1)]
		public int Parallel;
	}

	/// <summary>
	/// Class that is manages the creation and execution of one or more tests
	/// </summary>
	public class TextExecutor
	{
		class TestExecutionInfo
		{
			public enum ExecutionResult
			{
				NotStarted,
				TimedOut,
				Passed,
				Failed
			}

			public ITestNode		TestNode;
			public DateTime			FirstReadyCheckTime;
			public DateTime			PreStartTime;
			public DateTime			PostStartTime;
			public DateTime			EndTime;
			public ExecutionResult	Result;

			public TestExecutionInfo(ITestNode InNode)
			{
				FirstReadyCheckTime = PreStartTime = PostStartTime = EndTime = DateTime.MinValue;
				TestNode = InNode;
			}

			public override string ToString()
			{
				return TestNode.ToString();
			}
		};

		private List<TestExecutionInfo> RunningTests;

		private DateTime StartTime;

		private int CurrentTestPass;

		TestExecutorOptions Options;

		public bool IsRunning { get; private set; }
		public bool IsCancelled { get; private set; }
		protected bool HaveReceivedPostAbort { get; private set; }

		/// <summary>
		/// Constructor that fills in some member variables
		/// </summary>
		public TextExecutor()
		{
			RunningTests = new List<TestExecutionInfo>();
		}

		public void Dispose()
		{
		}

		/// <summary>
		/// Executes the provided tests. Currently tests are executed synchronously
		/// </summary>
		/// <param name="Context"></param>
		public bool ExecuteTests(TestExecutorOptions InOptions, IEnumerable<ITestNode> RequiredTests)
		{
			Options = InOptions;

			Log.Info("Preparing to start {0} automation test(s)", RequiredTests.Count());

			// install a cancel handler so we can stop parallel-for gracefully
			Action CancelHandler = delegate ()
			{
				Log.Info("Cancelling Tests");
				IsCancelled = true;
			};

			Action PostCancelHandler = delegate ()
			{
				HaveReceivedPostAbort = true;
			};

			Globals.AbortHandlers.Add(CancelHandler);
			Globals.PostAbortHandlers.Add(PostCancelHandler);

			StartTime = DateTime.Now;
			CurrentTestPass = 0;

			IsRunning = true;

			List<int> FailedPassList = new List<int>();

			int MaxParallelTasks = 0;
			int MaxStartingTasks = 0;

			// sort by priority
			if (Options.Parallel > 1)
			{
				RequiredTests = RequiredTests.OrderBy(Node => Node.Priority);
			}

			for (CurrentTestPass = 0; CurrentTestPass < Options.TestLoops; CurrentTestPass++)
			{
				// do not start a pass if cancelled
				if (IsCancelled)
				{
					break;
				}

				if (CurrentTestPass > 0)
				{
					// if repeating tests wait a little bit. If there was a crash CR might still be
					// processing things.
					Thread.Sleep(10000);
				}

				DateTime StartPassTime = DateTime.Now;

				Log.Info("Starting test pass {0} of {1}", CurrentTestPass + 1, Options.TestLoops);

				// Tests that we want to run
				List<TestExecutionInfo> PendingTests = RequiredTests.Select(N => new TestExecutionInfo(N)).ToList();

				// Tests that are in the process of starting
				List<TestExecutionInfo> StartingTests = new List<TestExecutionInfo>();

				List<Thread> StartingTestThreads = new List<Thread>();

				// Tests that have been started and we're ticking/checking
				List<TestExecutionInfo> RunningTests = new List<TestExecutionInfo>();

				// Completed tests
				List<TestExecutionInfo> CompletedTests = new List<TestExecutionInfo>();

				DateTime LastUpdateMsg = DateTime.MinValue;
				DateTime LastReadyCheck = DateTime.MinValue;
				DateTime LastStatusUpdateTime = DateTime.MinValue;

				const double ReadyCheckPeriod = 30.0;
				const double StatusUpdatePeriod = 60.0;

				while (CompletedTests.Count() < RequiredTests.Count() && IsCancelled == false)
				{
					Monitor.Enter(Globals.MainLock);

					int SecondsRunning = (int)(DateTime.Now - StartPassTime).TotalSeconds;

					int InProgressCount = RunningTests.Count() + StartingTests.Count();

					double TimeSinceLastReadyCheck = (DateTime.Now - LastReadyCheck).TotalSeconds;

					// Are any tests ready to run?
					if (InProgressCount < Options.Parallel 
						&& PendingTests.Count() > 0 
						&& TimeSinceLastReadyCheck >= ReadyCheckPeriod)
					{
						TestExecutionInfo TestToStart = null;

						List<ITestNode> TestsFailingToStart = new List<ITestNode>();

						// find a node that can run, and
						// find the first test that can run....
						for (int i = 0; i < PendingTests.Count(); i++)
						{
							TestExecutionInfo NodeInfo = PendingTests[i];
							ITestNode Node = NodeInfo.TestNode;

							bool IsTestReady = false;

							try
							{
								IsTestReady = Node.IsReadyToStart();
							}
							catch (System.Exception ex)
							{
								Log.Error("Test {0} threw an exception during ready check. Ex: {1}", Node, ex);

								PendingTests[i] = null;
								NodeInfo.PreStartTime = NodeInfo.PostStartTime = NodeInfo.EndTime = DateTime.Now;
								CompletedTests.Add(NodeInfo);
							}

							if (IsTestReady)
							{
								// if ready then take it and stop looking
								TestToStart = NodeInfo;

								if (NodeInfo.FirstReadyCheckTime == DateTime.MinValue)
								{
									NodeInfo.FirstReadyCheckTime = DateTime.Now;
								}
								break;
							}
							else
							{
								// track the time that this test should have been able to run due to no other tests
								// consuming resources (at least locally...)
								// TODO - how can tests express resource requirements in a generic way?
								// TODO - what about the situation where no tests can run so all FirstCheck times are set, but 
								// then a test starts and consumes all resources?
								if (RunningTests.Count() == 0 && StartingTests.Count() == 0)
								{
									if (NodeInfo.FirstReadyCheckTime == DateTime.MinValue)
									{
										NodeInfo.FirstReadyCheckTime = DateTime.Now;
									}

									double TimeWaiting = (DateTime.Now - NodeInfo.FirstReadyCheckTime).TotalSeconds;
									if (TimeWaiting >= Options.Wait)
									{
										Log.Warning("Test {0} has been waiting to run resource-free for {1:00} seconds. Removing from wait list", Node, TimeWaiting);
										PendingTests[i] = null;
										NodeInfo.PreStartTime = NodeInfo.PostStartTime = NodeInfo.EndTime = DateTime.Now;
										NodeInfo.Result = TestExecutionInfo.ExecutionResult.TimedOut;
										CompletedTests.Add(NodeInfo);
									}
								}
							}
						}

						// remove anything we nulled
						PendingTests = PendingTests.Where(T => T != null).ToList();

						if (TestToStart != null)
						{
							Log.Info("Test {0} is ready to run", TestToStart);

							PendingTests.Remove(TestToStart);
							StartingTests.Add(TestToStart);

							// StartTest is the only thing we do on a thread because it's likely to be the most time consuming
							// as build are copied so will get the most benefit from happening in parallel
							Thread StartThread = new Thread(() =>
							{
								Thread.CurrentThread.IsBackground = true;

								// start the test, this also fills in the pre/post start times
								bool Started = StartTest(TestToStart, CurrentTestPass, Options.TestLoops);

								lock (Globals.MainLock)
								{
									if (Started == false)
									{
										TestToStart.PostStartTime = TestToStart.EndTime = DateTime.Now;
										CompletedTests.Add(TestToStart);
										Log.Error("Test {0} failed to start", TestToStart);
									}
									else
									{
										RunningTests.Add(TestToStart);
									}

									StartingTests.Remove(TestToStart);
									StartingTestThreads.Remove(Thread.CurrentThread);
								}
							});

							if (StartingTests.Count > MaxStartingTasks)
							{
								MaxStartingTasks = StartingTests.Count;
							}

							// track the thread and start it
							StartingTestThreads.Add(StartThread);
							StartThread.Start();							
						}
						else
						{
							// don't check for a while as querying kits for availability can be expensive
							LastReadyCheck = DateTime.Now;
						}
					}

					// Tick all running tests
					foreach (TestExecutionInfo TestInfo in RunningTests)
					{
						TestResult Result = TickTest(TestInfo);

						// invalid = no result yet
						if (Result == TestResult.Invalid)
						{
							TimeSpan RunningTime = DateTime.Now - TestInfo.PostStartTime;

							if ((SecondsRunning % 60) == 0)
							{
								Log.Verbose("Test {0} is still running. {1:00} seconds elapsed, will timeout in {2:00} seconds",
									TestInfo,
									RunningTime.TotalSeconds,
									TestInfo.TestNode.MaxDuration - RunningTime.TotalSeconds);

								LastUpdateMsg = DateTime.Now;
							}
						}
						else
						{
							TestInfo.EndTime = DateTime.Now;
							TestInfo.Result = Result == TestResult.Passed ? TestExecutionInfo.ExecutionResult.Passed : TestExecutionInfo.ExecutionResult.Failed;
							CompletedTests.Add(TestInfo);
						}
					}

					// remove any tests that were completed
					RunningTests = RunningTests.Where(R => CompletedTests.Contains(R) == false).ToList();		
					
					if ((DateTime.Now - LastStatusUpdateTime).TotalSeconds >= StatusUpdatePeriod)
					{
						LastStatusUpdateTime = DateTime.Now;
						Log.Info("Status: Completed:{0}, Running:{1}, Starting: {2}, Waiting:{3}",
							CompletedTests.Count(), RunningTests.Count(), StartingTests.Count(), PendingTests.Count());
					}

					if (InProgressCount > MaxParallelTasks)
					{
						MaxParallelTasks = RunningTests.Count();
					}

					// Release our global lock before we loop
					Monitor.Exit(Globals.MainLock);

					// sleep a while before we tick our running tasks again
					Thread.Sleep(500);
				}

				if (IsCancelled)
				{
					DateTime StartTime = DateTime.Now;
					Log.Info("Cleaning up pending and running tests.");
					while (HaveReceivedPostAbort == false)
					{
						Thread.Sleep(500);
						double Elapsed = (DateTime.Now - StartTime).TotalSeconds;

						if (Elapsed >= 5)
						{
							Log.Error("Giving up waiting for tests after {0:00} seconds", Elapsed);
							break;
						}
					}

					// tick anything running, this will also check IsCancelled and stop them
					// forcibly kill anything waiting
					if (StartingTestThreads.Count > 0)
					{
						foreach (Thread T in StartingTestThreads)
						{
							Log.Info("Aborting startup thread");
							T.Abort();
						}
						Thread.Sleep(1000);
					}

					foreach (TestExecutionInfo TestInfo in StartingTests)
					{
						Log.Info("Forcing pending test {0} to run CleanupTest", TestInfo.TestNode.Name);
						TestInfo.TestNode.CleanupTest();
						CompletedTests.Add(TestInfo);
					}

					foreach (TestExecutionInfo TestInfo in RunningTests)
					{
						Log.Info("Ticking test {0} to cancel", TestInfo.TestNode.Name);
						TestResult Res = TickTest(TestInfo);
						CompletedTests.Add(TestInfo);

						if (Res != TestResult.Failed)
						{
							Log.Warning("Ticking of cancelled test {0} returnd {1}", TestInfo.TestNode.Name, Res);
						}
					}
				}
				else
				{
					TimeSpan PassDuration = DateTime.Now - StartPassTime;

					int FailedCount = 0;
					int TestCount = CompletedTests.Count;

					CompletedTests.ForEach(T =>
					{
						TimeSpan TimeWaiting = T.FirstReadyCheckTime - T.PreStartTime;
						TimeSpan SetupTime = T.PostStartTime - T.PreStartTime;
						TimeSpan TestDuration = T.EndTime - T.PostStartTime;

						// status msg, kept uniform to avoid spam on notifiers (ie. don't include timestamps, etc) 
						string Msg = string.Format("Test {0} {1}", T.TestNode, T.Result);

						if (T.Result != TestExecutionInfo.ExecutionResult.Passed)
						{
							FailedCount++;
						}
	
						Log.Info(Msg);

						// log test timing to info
						Log.Info(string.Format("Test Time: {0:mm\\:ss} (Waited:{1:mm\\:ss}, Setup:{2:mm\\:ss})", TestDuration, TimeWaiting, SetupTime));

					});

					if (Options.Parallel > 1)
					{
						Log.Info("MaxParallelTasks: {0}", MaxParallelTasks);
						Log.Info("MaxStartingTasks: {0}", MaxStartingTasks);
					}

					Log.Info("Completed test pass {0} of {1} in {2:mm\\:ss}. {3}/{4} tests passed.", CurrentTestPass + 1, Options.TestLoops, PassDuration, TestCount - FailedCount, TestCount);

					if (FailedCount > 0)
					{
						FailedPassList.Add(CurrentTestPass);

						if (Options.StopOnError)
						{
							break;
						}
					}
				}

				// show details for multi passes				
				if (Options.TestLoops > 1)
				{
					Log.Info("Completed all passes. {0} of {1} completed without error", CurrentTestPass+1 - FailedPassList.Count(), Options.TestLoops);

					if (FailedPassList.Count > 0)
					{
						string FailedList = string.Join(",", FailedPassList);
						Log.Warning("Failed passes: " + FailedList);
					}
				}
			}			

			IsRunning = false;

			Globals.AbortHandlers.Remove(CancelHandler);
			Globals.PostAbortHandlers.Remove(PostCancelHandler);

			return FailedPassList.Count == 0 && !IsCancelled;
		}
		
		/// <summary>
		/// Executes a single test
		/// </summary>
		/// <param name="Test">Test to execute</param>
		/// <param name="Context">The context to execute this test under</param>
		/// <returns></returns>
		private bool StartTest(TestExecutionInfo TestInfo, int Pass, int NumPasses)
		{
			string Name = TestInfo.TestNode.Name;

			Log.Info("Starting Test {0}", TestInfo);

			try
			{
				TestInfo.PreStartTime = DateTime.Now;
				if (TestInfo.TestNode.StartTest(Pass, NumPasses))
				{
					TestInfo.PostStartTime = DateTime.Now;
					Log.Info("Launched test {0} at {1}", Name, TestInfo.PostStartTime.ToString("h:mm:ss"));
				}
			}
			catch (Exception Ex)
			{
				Log.Error("Test {0} threw an exception during launch. Skipping test. Ex: {1}\n{2}", Name, Ex.Message, Ex.StackTrace);
				return false;
			}			

			return true;			
		}

		TestResult TickTest(TestExecutionInfo TestInfo)
		{
			// invalid = no result yet..
			TestResult ReturnResult = TestResult.Invalid;
			bool TestCancelled = false;

			string CancellationReason = "";

			// Give the test a chance to update itself
			try
			{
				TestInfo.TestNode.TickTest();					
			}
			catch (Exception Ex)
			{
				CancellationReason = string.Format("Test {0} threw an exception. Cancelling. Ex: {1}\n{2}", TestInfo.TestNode.Name, Ex.Message, Ex.StackTrace);
			}
		
			// Does the test still say it's running?
			bool TestIsRunning = TestInfo.TestNode.GetTestStatus() == TestStatus.InProgress;

			TimeSpan RunningTime = DateTime.Now - TestInfo.PostStartTime;
			
			if (TestIsRunning && RunningTime.TotalSeconds > TestInfo.TestNode.MaxDuration && !Options.NoTimeout)
			{
				CancellationReason = string.Format("Terminating Test {0} due to maximum duration of {1} seconds. ", TestInfo.TestNode, TestInfo.TestNode.MaxDuration);
			}

			if (IsCancelled)
			{
				CancellationReason = string.Format("Cancelling Test {0} on request", TestInfo.TestNode);
			}

			if (TestIsRunning == false || CancellationReason.Length > 0)
			{
				// Request the test stop
				try
				{
					// Note - we log in this order to try and make it easy to grep the log and find the
					// artifcat links
					Log.Info("*");
					Log.Info("****************************************************************");
					Log.Info("Finished Test: {0} in {1:mm\\:ss}", TestInfo, DateTime.Now - TestInfo.PostStartTime);

					// Tell the test it's done
					TestInfo.TestNode.StopTest(TestCancelled);

					TestResult NodeResult = TestInfo.TestNode.GetTestResult();
					TestResult ActualResult = (CancellationReason.Length > 0) ? TestResult.Failed : NodeResult;

					if (ActualResult == TestResult.WantRetry)
					{
						Log.Warning("{0} requested retry. Cleaning up old test and relaunching", TestInfo);

						DateTime OriginalStartTime = TestInfo.PostStartTime;

						TestInfo.TestNode.RestartTest();

						// Mark us as still running
						ReturnResult = TestResult.Invalid;
					}
					else
					{
						ReturnResult = ActualResult;

						string Summary = TestInfo.TestNode.GetTestSummary();
			
						if (ActualResult == TestResult.Failed)
						{
							string Cause = ActualResult.ToString();

							if (TestCancelled)
							{
								Cause = CancellationReason;
							}

							Log.Error("{0} result={1}", TestInfo, Cause);
						}
						else
						{
							if (TestInfo.TestNode.HasWarnings)
							{
								Log.Warning("{0} result={1}", TestInfo, ActualResult);
							}
							else
							{
								Log.Info("{0} result={1}", TestInfo, ActualResult);
							}
						}
						Summary.Split('\n').ToList().ForEach(L => Log.Info("  " + L));
					}

					// now cleanup
					try
					{
						TestInfo.TestNode.CleanupTest();
					}
					catch (System.Exception ex)
					{
						Log.Error("Test {0} threw an exception while cleaning up. Ex: {1}", TestInfo.TestNode.Name, ex.Message);
					}

					Log.Info("****************************************************************");
					Log.Info("*");
				}
				catch (System.Exception ex)
				{
					if (TestCancelled)
					{
						Log.Warning("Cancelled Test {0} threw an exception while stopping. Ex: {1}\n{2}", 
							TestInfo.TestNode.Name, ex.Message, ex.StackTrace);
					}
					else
					{
						Log.Error("Test {0} threw an exception while stopping. Ex: {1}\n{2}",
							TestInfo.TestNode.Name, ex.Message, ex.StackTrace);
					}

					ReturnResult = TestResult.Failed;
				}				
			}

			return ReturnResult;
		}

		/// <summary>
		/// Waits for all pending tests to complete. Returns true/false based on whether
		/// all tests completed successfully.
		/// </summary>
		bool WaitForTests()
		{
			Log.Info("Waiting for {0} tests to complete", RunningTests.Count);

			DateTime LastUpdateMsg = DateTime.Now;

			bool AllTestsPassed = true;

			while (RunningTests.Count > 0)
			{
				List<TestExecutionInfo > RemainingTests = new List<TestExecutionInfo>();

				foreach (TestExecutionInfo Process in RunningTests)
				{
					
					TestResult Result = TickTest(Process);

					// invalid = no
					if (Result == TestResult.Invalid)
					{ 
						RemainingTests.Add(Process);

						TimeSpan RunningTime = DateTime.Now - Process.PostStartTime;

						if ((DateTime.Now - LastUpdateMsg).TotalSeconds > 60.0f)
						{
							Log.Verbose("Test {0} is still running. {1:00} seconds elapsed, will timeout in {2:00} seconds",
								Process.TestNode.Name,
								RunningTime.TotalSeconds,
								Process.TestNode.MaxDuration - RunningTime.TotalSeconds);

							LastUpdateMsg = DateTime.Now;
						}
					}
					else
					{
						if (Result != TestResult.Passed)
						{
							AllTestsPassed = false;
						}

						Log.Info("Test {0} Result: {1}", Process.TestNode.Name, Result);
					}
				}

				RunningTests = RemainingTests;

				Thread.Sleep(1000);
			}

			return AllTestsPassed;
		}
	}
}
