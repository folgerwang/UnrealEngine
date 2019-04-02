// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using Gauntlet;
using System;
using System.Collections.Generic;
using System.Linq;
using EpicGame;

namespace Gauntlet.UnrealTest
{
	/// <summary>
	/// Base class for error tests. Contains all the logic but the smaller classes should be used.
	/// E.g. -test=ErrorTestEnsure -server
	/// </summary>
	public class ErrorTestBase : UE4Game.DefaultTest
	{
		protected enum ErrorTypes
		{
			Ensure,
			Check,
			Fatal,
			GPF
		}

		protected ErrorTypes ErrorType;
		protected int ErrorDelay;
		protected bool Server;

		public ErrorTestBase(UnrealTestContext InContext)
			: base(InContext)
		{
			ErrorType = ErrorTypes.Check;
			ErrorDelay = 5;
			Server = false;
		}

		public override UE4Game.UE4TestConfig GetConfiguration()
		{
			UE4Game.UE4TestConfig Config = base.GetConfiguration();		
			
			string ErrorParam = Context.TestParams.ParseValue("ErrorType", null);

			if (string.IsNullOrEmpty(ErrorParam) == false)
			{
				ErrorType = (ErrorTypes)Enum.Parse(typeof(ErrorTypes), ErrorParam);
			}

			ErrorDelay = Context.TestParams.ParseValue("ErrorDelay", ErrorDelay);
			Server = Context.TestParams.ParseParam("Server");

			string Args = string.Format(" -errortest.type={0} -errortest.delay={1}", ErrorType, ErrorDelay);
			
			if (Server)
			{
				var ServerRole = Config.RequireRole(UnrealTargetRole.Server);
				ServerRole.Controllers.Add("ErrorTest");
				ServerRole.CommandLine += Args;
			}
			else
			{
				var ClientRole = Config.RequireRole(UnrealTargetRole.Client);
				ClientRole.Controllers.Add("ErrorTest");
				ClientRole.CommandLine += Args;
			}

			return Config;
		}

		protected override int GetExitCodeAndReason(UnrealRoleArtifacts InArtifacts, out string ExitReason)
		{
			string Reason = "";
			int ExitCode = -1;

			TestResult FinalResult = TestResult.Invalid;

			UnrealLogParser.LogSummary Summary = InArtifacts.LogSummary;

			if (ErrorType == ErrorTypes.Ensure)
			{
				// for an ensure we should have an entry and a callstack
				int EnsureCount = Summary.Ensures.Count();
				int CallstackLength = EnsureCount > 0 ? Summary.Ensures.First().Callstack.Length : 0;

				if (EnsureCount == 0)
				{
					FinalResult = TestResult.Failed;
					Reason = string.Format("No ensure error found for failure of type {0}", ErrorType);
				}
				else if (EnsureCount != 1)
				{
					FinalResult = TestResult.Failed;
					Reason = string.Format("Incorrect ensure count found for failure of type {0}", ErrorType);
				}
				else if (CallstackLength == 0)
				{
					FinalResult = TestResult.Failed;
					Reason = string.Format("No callstack error found for failure of type {0}", ErrorType);
				}
				else
				{
					FinalResult = TestResult.Passed;
					Reason = string.Format("Found {0} ensures, test result = {1}", EnsureCount, FinalResult);
				}
			}
			else
			{
				if (Summary.FatalError == null)
				{
					FinalResult = TestResult.Failed;
					Log.Info("No fatal error found for failure of type {0}", ErrorType);
				}
				else if (Summary.FatalError.Callstack.Length == 0)
				{
					FinalResult = TestResult.Failed;
					Log.Info("No callstack found for failure of type {0}", ErrorType);
				}
				else
				{
					// all of these should contain a message and a result
					if (ErrorType == ErrorTypes.Check)
					{
						if (!Summary.FatalError.Message.ToLower().Contains("assertion failed"))
						{
							FinalResult = TestResult.Failed;
							Reason = string.Format("Unexpected assertion message");
						}
						else
						{
							FinalResult = TestResult.Passed;
						}
						Log.Info("Assertion message was {0}", Summary.FatalError.Message);
					}
					else if (ErrorType == ErrorTypes.Fatal)
					{
						if (!Summary.FatalError.Message.ToLower().Contains("fatal erro"))
						{
							FinalResult = TestResult.Failed;
							Reason = string.Format("Unexpected Fatal Error message");
						}
						else
						{
							FinalResult = TestResult.Passed;
						}

						Log.Info("Fatal Error message was {0}", Summary.FatalError.Message);
					}
					else if (ErrorType == ErrorTypes.GPF)
					{
						if (!Summary.FatalError.Message.ToLower().Contains("exception"))
						{
							FinalResult = TestResult.Failed;
							Reason = string.Format("Unexpected exception message");
						}
						else
						{
							FinalResult = TestResult.Passed;
						}

						Log.Info("Exception message was {0}", Summary.FatalError.Message);
					}
				}
			}

			if (FinalResult != TestResult.Invalid)
			{
				ExitCode = (FinalResult == TestResult.Passed) ? 0 : 6;
				ExitReason = Reason;
				return ExitCode;
			}
			
			return base.GetExitCodeAndReason(InArtifacts, out ExitReason);
		}
	}

	/// <summary>
	/// Tests that ensures are emitted and parsed correctly
	/// </summary>
	public class ErrorTestEnsure : ErrorTestBase
	{
		public ErrorTestEnsure(UnrealTestContext InContext)
			: base(InContext)
		{
			ErrorType = ErrorTypes.Ensure; 
		}
	}

	/// <summary>
	/// Tests that checks are emitted and parsed correctly
	/// </summary>
	public class ErrorTestCheck : ErrorTestBase
	{
		public ErrorTestCheck(UnrealTestContext InContext)
			: base(InContext)
		{
			ErrorType = ErrorTypes.Check;
		}
	}

	/// <summary>
	/// Tests that fatal logging is emitted and parsed correctly
	/// </summary>
	public class ErrorTestFatal : ErrorTestBase
	{
		public ErrorTestFatal(UnrealTestContext InContext)
			: base(InContext)
		{
			ErrorType = ErrorTypes.Fatal;
		}
	}

	/// <summary>
	/// Tests that exceptions are emitted and parsed correctly
	/// </summary>
	public class ErrorTestGPF : ErrorTestBase
	{
		public ErrorTestGPF(UnrealTestContext InContext)
			: base(InContext)
		{
			ErrorType = ErrorTypes.GPF;
		}
	}
}
