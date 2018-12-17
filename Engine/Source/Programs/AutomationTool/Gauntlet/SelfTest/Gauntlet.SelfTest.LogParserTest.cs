// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Gauntlet.SelfTest
{
	/// <summary>
	/// Tests the various queries of the log parser
	/// </summary>
	class LogParserTest : BaseNode
	{
		public override void OnTick()
		{
			MarkComplete(TestResult.Passed);
		}

		public override ITestNode[] GetSubTests()
		{
			return new ITestNode[]
			{
				 new LogParserTestWarnings()
				,new LogParserTestErrors()
				,new LogParserTestFatalError()
				,new LogParserTestChannels()
				,new LogParserTestRequestExit()
				,new LogParserTestAssertPS4()
			};
		}
	}


	class LogParserTestBase : BaseNode
	{
		protected string BaseDataPath = Path.Combine(Environment.CurrentDirectory, @"Engine\Source\Programs\AutomationTool\NotForLicensees\Gauntlet\SelfTest\TestData");

		protected string WindowsServerLogContent;
		protected string WindowsClientLogContent;
		protected string WindowsClientFailureLogContent;
		protected string PS4ClientLogWithAssertContent;
		protected string PS4ClientLogContent;

		public override bool LaunchTest()
		{
			string ServerFilePath = Path.Combine(BaseDataPath, "OrionWindowsServerLog.txt");
			string ClientFilePath = Path.Combine(BaseDataPath, "OrionWindowsClientLog.txt");
			string ClientFailureLog = Path.Combine(BaseDataPath, "OrionWindowsClientFailureLog.txt");
			string PS4ClientLogWithAssert = Path.Combine(BaseDataPath, "PS4ClientLogWithAssert.txt");
			string PS4ClientLog = Path.Combine(BaseDataPath, "OrionPS4ClientLog.txt");

			if (File.Exists(ServerFilePath) == false)
			{
				throw new TestException("Missing data file {0}", ServerFilePath);
			}

			if (File.Exists(ClientFilePath) == false)
			{
				throw new TestException("Missing data file {0}", ClientFilePath);
			}

			WindowsServerLogContent = File.ReadAllText(ServerFilePath);
			WindowsClientLogContent = File.ReadAllText(ClientFilePath);
			WindowsClientFailureLogContent = File.ReadAllText(ClientFailureLog);
			PS4ClientLogWithAssertContent = File.ReadAllText(PS4ClientLogWithAssert);
			PS4ClientLogContent = File.ReadAllText(PS4ClientLog);

			return true;
		}

		public override void OnTick()
		{
			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestWarnings : LogParserTestBase
	{
		public override void OnTick()
		{
			const int ExpectedWarnings = 262;

			UnrealLogParser Parser = new UnrealLogParser(WindowsServerLogContent);

			// Get warnings
			IEnumerable<string> WarningLines = Parser.GetWarnings();

			if (WarningLines.Count() != ExpectedWarnings)
			{
				throw new TestException("LogParser returned incorrect warning count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestGauntletExit : LogParserTestBase
	{
		public override void OnTick()
		{
			const int ExpectedMessages = 7;
			const int ExpectedErrors = 1;

			UnrealLogParser Parser = new UnrealLogParser(WindowsClientFailureLogContent);

			// Get warnings
			int ExitCode = 2;
			Parser.GetTestExitCode(out ExitCode);

			if (ExitCode != -1)
			{
				throw new TestException("LogParser returned incorrect exit code");
			}

			IEnumerable<string> GauntletMessages = Parser.GetLogChannel("Gauntlet");

			if (GauntletMessages.Count() != ExpectedMessages)
			{
				throw new TestException("LogParser returned incorrect channel message count");
			}

			IEnumerable<string> GauntletErrors = Parser.GetErrors("Gauntlet");

			if (GauntletErrors.Count() != ExpectedErrors)
			{
				throw new TestException("LogParser returned incorrect channel error count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestErrors : LogParserTestBase
	{
		public override void OnTick()
		{
			const int ExpectedErrors = 81;

			UnrealLogParser Parser = new UnrealLogParser(WindowsServerLogContent);

			// Get warnings
			IEnumerable<string> ErrorLines = Parser.GetErrors();

			if (ErrorLines.Count() != ExpectedErrors)
			{
				throw new TestException("LogParser returned incorrect error count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestFatalError : LogParserTestBase
	{
		public override void OnTick()
		{
			UnrealLogParser Parser = new UnrealLogParser(WindowsServerLogContent);

			UnrealLogParser.FatalError FatalError = Parser.GetFatalError();

			if (FatalError == null || FatalError.Callstack.Length == 0 || string.IsNullOrEmpty(FatalError.Message))
			{
				throw new TestException("LogParser returned incorrect assert info");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestEnsures : LogParserTestBase
	{
		public override void OnTick()
		{
			UnrealLogParser Parser = new UnrealLogParser(PS4ClientLogContent);

			var Ensures = Parser.GetEnsures();

			if (Ensures.Count() != 2)
			{
				throw new TestException("LogParser returned incorrect assert info");
			}

			foreach (var E in Ensures)
			{
				if (string.IsNullOrEmpty(E.Message))
				{
					throw new TestException("LogParser returned incorrect assert info");
				}

				if (E.Callstack.Count() < 10)
				{
					throw new TestException("LogParser returned incorrect assert info");
				}
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestChannels : LogParserTestBase
	{
		public override void OnTick()
		{
			const int ExpectedLines = 19;

			UnrealLogParser Parser = new UnrealLogParser(WindowsClientLogContent);

			// Get warnings
			IEnumerable<string> Lines = Parser.GetLogChannel("OrionMemory");

			if (Lines.Count() != ExpectedLines)
			{
				throw new TestException("LogParser returned incorrect channel count");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestRequestExit : LogParserTestBase
	{
		public override void OnTick()
		{
			UnrealLogParser Parser = new UnrealLogParser(WindowsClientLogContent);

			// Get warnings
			bool HadExit = Parser.HasRequestExit();

			if (HadExit == false)
			{
				throw new TestException("LogParser returned incorrect RequestExit");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestAssertPS4 : LogParserTestBase
	{
		public override void OnTick()
		{
			string FilePath = Path.Combine(BaseDataPath, "OrionPS4ClientLogWithPerf.txt");

			if (File.Exists(FilePath) == false)
			{
				throw new TestException("Missing data file {0}", FilePath);
			}

			UnrealLogParser Parser = new UnrealLogParser(File.ReadAllText(FilePath));

			UnrealLogParser.FatalError FatalError = Parser.GetFatalError();

			if (FatalError == null || FatalError.Callstack.Length == 0 || string.IsNullOrEmpty(FatalError.Message))
			{
				throw new TestException("LogParser returned incorrect assert info");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserTestHealthReport : LogParserTestBase
	{

		public override void OnTick()
		{

			OrionTest.OrionHealthReport Report = new OrionTest.OrionHealthReport(PS4ClientLogContent);

			if (Report.LogCount == 0 || Report.EnsureCount == 0)
			{
				throw new TestException("LogParser returned incorrect assert info");
			}

			MarkComplete(TestResult.Passed);
		}
	}

	class LogParserPerfSummary : LogParserTestBase
	{

		public override void OnTick()
		{
			string FilePath = Path.Combine(BaseDataPath, "OrionWindowsClientLog.txt");

			OrionTest.PerformanceSummary Summary = new OrionTest.PerformanceSummary(File.ReadAllText(FilePath));

			MarkComplete(TestResult.Passed);
		}

	}


}

