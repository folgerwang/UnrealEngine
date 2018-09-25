// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet.SelfTest
{
	
	/// <summary>
	/// Base class for log parser tests
	/// </summary>
	abstract class TestUnrealLogParserBase : BaseTestNode
	{
		protected string BaseDataPath = Path.Combine(Environment.CurrentDirectory, @"Engine\Source\Programs\AutomationTool\Gauntlet\SelfTest\TestData\LogParser");

		protected string GetFileContents(string FileName)
		{
			string FilePath = Path.Combine(BaseDataPath, FileName);

			if (File.Exists(FilePath) == false)
			{
				throw new TestException("Missing data file {0}", FilePath);
			}

			return File.ReadAllText(FilePath);
		}

		public override bool StartTest(int Pass, int NumPasses)
		{
			return true;
		}

		public override void TickTest()
		{
			MarkComplete(TestResult.Passed);
		}
	}

	
	[TestGroup("Framework")]
	class LogParserTestGauntletExitSuccess : TestUnrealLogParserBase
	{
		public override void TickTest()
		{

			foreach (var Platform in new[] { "Win64Client", "PS4Client" })
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithTestSuccess" + Platform + ".txt"));

				int ExitCode = 2;
				Parser.GetTestExitCode(out ExitCode);

				if (ExitCode != 0)
				{
					throw new TestException("LogParser did not find succesful exit for {0}", Platform);
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}


	[TestGroup("Framework")]
	class LogParserTestEnsure : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			foreach (var Platform in new[] { "Win64Client", "PS4Client" })
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithEnsure" + Platform + ".txt"));

				var Ensures = Parser.GetEnsures();

				if (Ensures.Count() !=1)
				{
					throw new TestException("LogParser failed to find ensure for {0}", Platform);
				}

				var Ensure = Ensures.First();

				if (string.IsNullOrEmpty(Ensure.Message) || Ensure.Callstack.Length < 8)
				{
					throw new TestException("LogParser failed to find ensure details for {0}", Platform);
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}

	[TestGroup("Framework")]
	class LogParserTestAssert : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			foreach (var Platform in new[] { "Win64Client", "PS4Client" })
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithCheck" + Platform + ".txt"));

				UnrealLogParser.CallstackMessage FatalError = Parser.GetFatalError();

				if (FatalError == null || FatalError.Callstack.Length < 8 || string.IsNullOrEmpty(FatalError.Message))
				{
					throw new TestException("LogParser returned incorrect assert info for {0}", Platform);
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// <summary>
	/// Tests that the log parser correctly finds a fatal error in a logfile
	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestFatalError : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			foreach (var Platform in new[] { "Win64Client", "PS4Client" })
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithFatalError" + Platform + ".txt"));

				UnrealLogParser.CallstackMessage FatalError = Parser.GetFatalError();

				if (FatalError == null || FatalError.Callstack.Length == 0 || string.IsNullOrEmpty(FatalError.Message))
				{
					throw new TestException("LogParser returned incorrect assert info for {0}", Platform);
				}
			}			

			MarkComplete(TestResult.Passed);
		}
	}

	[TestGroup("Framework")]
	class LogParserTestException : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			foreach (var Platform in new[] { "Win64Client", "PS4Client" })
			{
				UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionLogWithException" + Platform + ".txt"));

				UnrealLogParser.CallstackMessage FatalError = Parser.GetFatalError();

				if (FatalError == null || FatalError.Callstack.Length == 0 || string.IsNullOrEmpty(FatalError.Message))
				{
					throw new TestException("LogParser returned incorrect assert info for {0}", Platform);
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// <summary>
	/// Tests that the logfile correcly finds a RequestExit line in a log file
	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestRequestExit : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionPS4ClientLogWithPerf.txt"));

			// Get warnings
			bool HadExit = Parser.HasRequestExit();

			if (HadExit == false)
			{
				throw new TestException("LogParser returned incorrect RequestExit");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// <summary>
	/// Tests that the log parser correctly extracts channel-lines from a logfile
	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestChannels: TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			const int ExpectedLines = 761;

			UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionPS4ClientLogWithPerf.txt"));

			// Get warnings
			IEnumerable<string> Lines = Parser.GetLogChannel("OrionMemory");

			if (Lines.Count() != ExpectedLines)
			{
				throw new TestException("LogParser returned incorrect channel count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// <summary>
	/// Tests that the log parser correctly pulls warnings from a log file
	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestWarnings : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			const int ExpectedWarnings = 21146;

			UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionPS4ClientLogWithPerf.txt"));

			// Get warnings
			IEnumerable<string> WarningLines = Parser.GetWarnings();

			if (WarningLines.Count() != ExpectedWarnings)
			{
				throw new TestException("LogParser returned incorrect warning count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	/// </summary>
	[TestGroup("Framework")]
	class LogParserTestErrors : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			const int ExpectedErrors = 20;

			UnrealLogParser Parser = new UnrealLogParser(GetFileContents("OrionPS4ClientLogWithPerf.txt"));

			// Get warnings
			IEnumerable<string> ErrorLines = Parser.GetErrors();

			if (ErrorLines.Count() != ExpectedErrors)
			{
				throw new TestException("LogParser returned incorrect error count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	

	/// <summary>
	/// Tests that the logfile correctly finds an assert statement and callstack in a PS4 log
	/// </summary>
	/*[TestGroup("LogParser")]
	class LogParserTestHealthReport : TestUnrealLogParserBase
	{

		public override void OnTick()
		{

			OrionTest.OrionHealthReport Report = new OrionTest.OrionHealthReport("OrionPS4ClientLogWithPerf.txt");

			if (Report.LogCount == 0 || Report.EnsureCount == 0)
			{
				throw new TestException("LogParser returned incorrect assert info");
			}

			MarkComplete(TestResult.Passed);
		}
	}*/

	[TestGroup("Framework")]
	class LogParserPerfSummary : TestUnrealLogParserBase
	{
		public override void TickTest()
		{
			//string FilePath = Path.Combine(BaseDataPath, "OrionPS4ClientLogWithPerf.txt");

			//OrionTest.PerformanceSummary Summary = new OrionTest.PerformanceSummary(File.ReadAllText(FilePath));

			MarkComplete(TestResult.Passed);
		}

	}
}

