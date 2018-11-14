// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Drawing;
using System.Linq;

namespace Gauntlet
{
	/// <summary>
	/// Helper class for parsing logs
	/// </summary>
	public class UnrealLogParser
	{
		/// <summary>
		/// Compound object that represents a fatal entry 
		/// </summary>
		public class CallstackMessage
		{
			public int				Position;
			public string			Message;
			public string[]			Callstack;
			public bool				IsEnsure;
		};

		public class PlatformInfo
		{
			public string OSName;
			public string OSVersion;
			public string CPUName;
			public string GPUName;
		}

		public class BuildInfo
		{
			public string BranchName;
			public int Changelist;
		}

		public class LogSummary
		{
			public BuildInfo							BuildInfo;
			public PlatformInfo							PlatformInfo;
			public IEnumerable<string>					Warnings;
			public IEnumerable<string>					Errors;
			public CallstackMessage						FatalError;
			public IEnumerable<CallstackMessage>		Ensures;
			public int									LineCount;
			public bool									RequestedExit;
			public bool									HasTestExitCode;
			public int									TestExitCode;
		}

		/// <summary>
		/// Our current content
		/// </summary>
		public string Content { get; protected set; }

		private LogSummary Summary;

		/// <summary>
		/// Constructor that takes the content to parse
		/// </summary>
		/// <param name="InContent"></param>
		/// <returns></returns>
		public UnrealLogParser(string InContent)
		{
			// convert linefeed to remove \r which is captured in regex's :(
			Content = InContent.Replace(Environment.NewLine, "\n");
		}

		public LogSummary GetSummary()
		{
			if (Summary == null)
			{
				Summary = CreateSummary();
			}

			return Summary;
		}

		protected LogSummary CreateSummary()
		{
			LogSummary NewSummary = new LogSummary();

			NewSummary.BuildInfo = GetBuildInfo();
			NewSummary.PlatformInfo = GetPlatformInfo();
			NewSummary.Warnings = GetWarnings();
			NewSummary.Errors = GetErrors();
			NewSummary.FatalError = GetFatalError();
			NewSummary.Ensures = GetEnsures();
			NewSummary.LineCount = Content.Split('\n').Count();
			NewSummary.RequestedExit = HasRequestExit();
			NewSummary.HasTestExitCode = GetTestExitCode(out NewSummary.TestExitCode);

			return NewSummary;
		}


		/// <summary>
		/// Returns all lines from the specified content match the specified regex
		/// </summary>
		/// <param name="InContent"></param>
		/// <param name="InPattern"></param>
		/// <returns></returns>
		protected string[] GetAllMatchingLines(string InContent, string InPattern)
		{
			Regex regex = new Regex(InPattern);

			return regex.Matches(InContent).Cast<Match>().Select(M => M.Value).ToArray();
		}

		/// <summary>
		/// Returns all lines that match the specified regex
		/// </summary>
		/// <param name="InPattern"></param>
		/// <returns></returns>
		public string[] GetAllMatchingLines(string InPattern)
		{
			return GetAllMatchingLines(Content, InPattern);
		}

		/// <summary>
		/// Returns a structure containing platform information extracted from the log
		/// </summary>
		/// <returns></returns>
		public PlatformInfo GetPlatformInfo()
		{
			var Info = new PlatformInfo();

			var InfoRegEx = @"LogInit.+OS:\s*(.+?)\s*(\((.+)\))?,\s*CPU:\s*(.+)\s*,\s*GPU:\s*(.+)";

			RegexUtil.MatchAndApplyGroups(Content, InfoRegEx, (Groups) =>
			{
				Info.OSName = Groups[1];
				Info.OSVersion = Groups[3];
				Info.CPUName = Groups[4];
				Info.GPUName = Groups[5];
			});

			return Info;
		}

		/// <summary>
		/// Returns a structure containing build information extracted from the log
		/// </summary>
		/// <returns></returns>
		public BuildInfo GetBuildInfo()
		{
			var Info = new BuildInfo();

			// pull from Branch Name: UE4
			Match M = Regex.Match(Content, @"LogInit.+Name:\s*(.*)", RegexOptions.IgnoreCase);

			if (M.Success)
			{
				Info.BranchName = M.Groups[1].ToString();
				Info.BranchName = Info.BranchName.Replace("+", "/");
			}

			M = Regex.Match(Content, @"LogInit.+CL-(\d+)", RegexOptions.IgnoreCase);

			if (M.Success)
			{
				Info.Changelist = Convert.ToInt32(M.Groups[1].ToString());
			}

			return Info;
		}


		/// <summary>
		/// Return all entries for the specified channel. E.g. "OrionGame" will
		/// return all entries starting with LogOrionGame
		/// </summary>
		/// <param name="Channel"></param>
		/// <returns></returns>
		public IEnumerable<string> GetLogChannel(string Channel)
		{
			string Pattern = string.Format(@"(Log{0}:\s{{0,1}}.+)", Channel);

			return Regex.Matches(Content, Pattern).Cast<Match>().Select(M => M.Groups[1].ToString()).ToArray();
		}

		/// <summary>
		/// Returns all warnings from the log
		/// </summary>
		/// <param name="InChannel">Optional channel to restrict search to</param>
		/// <returns></returns>
		public IEnumerable<string> GetWarnings(string InChannel = null)
		{
			string WarningRegEx = @"(Log.+:\s{0,1}Warning:.+)";

			string SearchContent = Content;

			if (string.IsNullOrEmpty(InChannel) == false)
			{
				SearchContent = string.Join(Environment.NewLine, GetLogChannel(InChannel));
			}

			return GetAllMatchingLines(SearchContent, WarningRegEx);
		}

		/// <summary>
		/// Returns all errors from the log
		/// </summary>
		/// <param name="InChannel">Optional channel to restrict search to</param>
		/// <returns></returns>
		public IEnumerable<string> GetErrors(string InChannel = null)
		{
			string ErrorRegEx = @"(Log.+:\s{0,1}Error:.+)";

			string SearchContent = Content;

			if (string.IsNullOrEmpty(InChannel) == false)
			{
				var ChannelLines = GetLogChannel(InChannel);
				SearchContent = string.Join("\n", ChannelLines);
			}

			return GetAllMatchingLines(SearchContent, ErrorRegEx);
		}

		/// <summary>
		/// Returns all ensures from the log
		/// </summary>
		/// <returns></returns>
		public IEnumerable<CallstackMessage> GetEnsures()
		{
			IEnumerable<CallstackMessage> Ensures = ParseTracedErrors(new[] { @"Log.+:\s{0,1}Error:\s{0,1}(Ensure condition failed:.+)" }, false);

			foreach (CallstackMessage Error in Ensures)
			{
				Error.IsEnsure = true;
			}

			return Ensures;
		}

		/// <summary>
		/// If the log contains a fatal error return that information
		/// </summary>
		/// <param name="ErrorInfo"></param>
		/// <returns></returns>
		public CallstackMessage GetFatalError()
		{
			string[] ErrorMsgMatches = new string[] { "(Fatal Error:.+)", "(Assertion Failed:.+)", "(Unhandled Exception:.+)", "(LowLevelFatalError.+)" };

			var Traces = ParseTracedErrors(ErrorMsgMatches);

			return Traces.Count() > 0 ? Traces.First() : null;
		}

		/// <summary>
		/// Returns true if the log contains a test complete marker
		/// </summary>
		/// <returns></returns>
		public bool HasTestCompleteMarker()
		{
			string[] Completion = GetAllMatchingLines(@"\*\*\* TEST COMPLETE.+");

			return Completion.Length > 0;
		}

		/// <summary>
		/// Returns true if the log contains a request to exit that was not due to an error
		/// </summary>
		/// <returns></returns>
		public bool HasRequestExit()
		{
			string[] Completion = GetAllMatchingLines("F.+::RequestExit");
			string[] ErrorCompletion = GetAllMatchingLines("StaticShutdownAfterError");

			return Completion.Length > 0 && ErrorCompletion.Length == 0;
		}

		/// <summary>
		/// Returns a block of lines that start with the specified regex
		/// </summary>
		/// <param name="Pattern"></param>
		/// <param name="LineCount"></param>
		/// <returns></returns>
		public string[] GetGroupsOfLinesStartingWith(string Pattern, int LineCount)
		{
			Regex regex = new Regex(Pattern, RegexOptions.IgnoreCase);

			List<string> Blocks = new List<string>();

			foreach (Match match in regex.Matches(Content))
			{
				int Location = match.Index;

				int Start = Content.LastIndexOf('\n', Location) + 1;
				int End = Location;

				for (int i = 0; i < LineCount; i++)
				{
					End = Content.IndexOf('\n', End) + 1;
				}

				if (End > Start)
				{
					string Block = Content.Substring(Start, End - Start);

					Blocks.Add(Block);
				}
			}

			return Blocks.ToArray();
		}



		/// <summary>
		/// Finds all callstack-based errors with the specified pattern
		/// </summary>
		/// <param name="Patterns"></param>
		/// <returns></returns>
		protected IEnumerable<CallstackMessage> ParseTracedErrors(string[] Patterns, bool IncludePostmortem = true)
		{
			List<CallstackMessage> Traces = new List<CallstackMessage>();

			// As well as what was requested, search for a postmortem stack...
			IEnumerable<string> AllPatterns = IncludePostmortem ? Patterns.Concat(new[] { "(Postmortem Cause:.*)" }) : Patterns;

			// Try and find an error message
			foreach (string Pattern in AllPatterns)
			{
				MatchCollection Matches = Regex.Matches(Content, Pattern, RegexOptions.IgnoreCase);

				foreach (Match TraceMatch in Matches)
				{ 
					CallstackMessage NewTrace = new CallstackMessage();

					NewTrace.Position = TraceMatch.Index;
					NewTrace.Message = TraceMatch.Groups[1].Value;

					// Next there will be a message (in some cases), and a callstack..
					string ErrorContent = Content.Substring(TraceMatch.Index + TraceMatch.Length + 1);

					Match MsgMatch = Regex.Match(ErrorContent, @".+:\s*Error:\s*(.+)");

					if (MsgMatch.Success)
					{
						string MsgString = MsgMatch.Groups[1].ToString();
						if (MsgString.Contains("[Callstack]") == false)
						{
							// Put the message first. e,g "foo != false: Assert [File:d:\UE\foomanager.cpp:10]
							NewTrace.Message = MsgMatch.Groups[1].ToString() + ": " + NewTrace.Message;
						}
					}

					//
					// Handing callstacks-
					//
					// Unreal now uses a canonical format for printing callstacks during errors which is 
					//
					//0xaddress module!func [file]
					// 
					// E.g. 0x045C8D01 OrionClient.self!UEngine::PerformError() [D:\Epic\Orion\Engine\Source\Runtime\Engine\Private\UnrealEngine.cpp:6481]
					//
					// Module may be omitted, everything else should be present, or substituted with a string that conforms to the expected type
					//
					// E.g 0x00000000 UnknownFunction []
					//
					// A calstack as part of an ensure, check, or exception will look something like this -
					// 
					//
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: Assertion failed: false [File:D:\Epic\Orion\Release-Next\Engine\Plugins\NotForLicensees\Gauntlet\Source\Gauntlet\Private\GauntletTestControllerErrorTest.cpp] [Line: 29] 
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: Asserting as requested
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: 
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: 
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: [Callstack] 0x00000000FDC2A06D KERNELBASE.dll!UnknownFunction []
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: [Callstack] 0x00000000418C0119 OrionClient.exe!FOutputDeviceWindowsError::Serialize() [d:\epic\orion\release-next\engine\source\runtime\core\private\windows\windowsplatformoutputdevices.cpp:120]
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: [Callstack] 0x00000000416AC12B OrionClient.exe!FOutputDevice::Logf__VA() [d:\epic\orion\release-next\engine\source\runtime\core\private\misc\outputdevice.cpp:70]
					//[2017.08.21-03.28.40:667][313]LogWindows:Error: [Callstack] 0x00000000418BD124 OrionClient.exe!FDebug::AssertFailed() [d:\epic\orion\release-next\engine\source\runtime\core\private\misc\assertionmacros.cpp:373]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x000000004604A879 OrionClient.exe!UGauntletTestControllerErrorTest::OnTick() [d:\epic\orion\release-next\engine\plugins\notforlicensees\gauntlet\source\gauntlet\private\gauntlettestcontrollererrortest.cpp:29]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x0000000046049166 OrionClient.exe!FGauntletModuleImpl::InnerTick() [d:\epic\orion\release-next\engine\plugins\notforlicensees\gauntlet\source\gauntlet\private\gauntletmodule.cpp:315]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x0000000046048472 OrionClient.exe!TBaseFunctorDelegateInstance<bool __cdecl(float),<lambda_b2e6da8e95d7ed933c391f0ec034aa11> >::Execute() [d:\epic\orion\release-next\engine\source\runtime\core\public\delegates\delegateinstancesimpl.h:1132]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x00000000415101BE OrionClient.exe!FTicker::Tick() [d:\epic\orion\release-next\engine\source\runtime\core\private\containers\ticker.cpp:82]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x00000000402887DD OrionClient.exe!FEngineLoop::Tick() [d:\epic\orion\release-next\engine\source\runtime\launch\private\launchengineloop.cpp:3295]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x00000000402961FC OrionClient.exe!GuardedMain() [d:\epic\orion\release-next\engine\source\runtime\launch\private\launch.cpp:166]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x000000004029625A OrionClient.exe!GuardedMainWrapper() [d:\epic\orion\release-next\engine\source\runtime\launch\private\windows\launchwindows.cpp:134]
					//[2017.08.21-03.28.40:668][313]LogWindows:Error: [Callstack] 0x00000000402A2D68 OrionClient.exe!WinMain() [d:\epic\orion\release-next\engine\source\runtime\launch\private\windows\launchwindows.cpp:210]
					//[2017.08.21-03.28.40:669][313]LogWindows:Error: [Callstack] 0x0000000046EEC0CB OrionClient.exe!__scrt_common_main_seh() [f:\dd\vctools\crt\vcstartup\src\startup\exe_common.inl:253]
					//[2017.08.21-03.28.40:669][313]LogWindows:Error: [Callstack] 0x0000000077A759CD kernel32.dll!UnknownFunction []
					//[2017.08.21-03.28.40:669][313]LogWindows:Error: [Callstack] 0x0000000077CAA561 ntdll.dll!UnknownFunction []
					//[2017.08.21-03.28.40:669][313]LogWindows:Error: [Callstack] 0x0000000077CAA561 ntdll.dll!UnknownFunction []
					//
					// So the code below starts at the point of the error message, and searches subsequent lines for things that look like a callstack. If we go too many lines without 
					// finding one then we break. Note that it's possible that log messages from another thread may be intermixed, so we can't just break on a change of verbosity or 
					// channel
					// 

					string SearchContent = ErrorContent;

					int LinesWithoutBacktrace = 0;

					List<string> Backtrace = new List<string>();

					do
					{
						int EOL = SearchContent.IndexOf("\n");

						if (EOL == -1)
						{
							break;
						}

						string Line = SearchContent.Substring(0, EOL);

						// Must have [Callstack] 0x00123456
						// The module name is optional, must start with whitespace, and continues until next whites[ace
						// filename is optional, must be in [file]
						// Note - Ubreal callstacks are always meant to omit all three with placeholders for missing values, but
						// we'll assume that may not happen...
						Match CSMatch = Regex.Match(Line, @"\[Callstack\]\s+(0[xX][0-9A-f]{8,16})(?:\s+([^\s]+))?(?:\s+\[(.*?)\])?", RegexOptions.IgnoreCase);

						if (CSMatch.Success)
						{
							string Address = CSMatch.Groups[1].Value;
							string Func = CSMatch.Groups[2].Value;
							string File = CSMatch.Groups[3].Value;

							if (string.IsNullOrEmpty(File))
							{
								 File = "[Unknown File]";
							}

							// Remove any exe
							const string StripFrom = ".exe!";

							if (Func.IndexOf(StripFrom) > 0)
							{
								Func = Func.Substring(Func.IndexOf(StripFrom) + StripFrom.Length);
							}

							string NewLine = string.Format("{0} {1} {2}", Address, Func, File);

							Backtrace.Add(NewLine);

							LinesWithoutBacktrace = 0;
						}
						else
						{
							LinesWithoutBacktrace++;
						}

						SearchContent = SearchContent.Substring(EOL + 1);

					} while (LinesWithoutBacktrace < 5);

					if (Backtrace.Count > 0)
					{
						NewTrace.Callstack = Backtrace.ToArray();
						Traces.Add(NewTrace);
					}
				}
			}

			// Now, because platforms sometimes dump asserts to the log and low-level logging, and we might have a post-mortem stack, we
			// need to prune out redundancies. Basic approach - find errors with the same assert message and keep the one with the
			// longest callstack. If we have a post-mortem error, overwrite the previous trace with its info (on PS4 the post-mortem
			// info is way more informative).

			List<CallstackMessage> FilteredTraces = new List<CallstackMessage>();

			
			for (int i = 0; i < Traces.Count ; i++)
			{
				var Trace = Traces[i];

				// check the next trace to see if it's a dupe of us
				if (i + 1 < Traces.Count)
				{
					var NextTrace = Traces[i + 1];

					if (Trace.Message.Equals(NextTrace.Message, StringComparison.OrdinalIgnoreCase))
					{
						if (Trace.Callstack.Length < NextTrace.Callstack.Length)
						{
							Trace.Callstack = NextTrace.Callstack;
							// skip the next error as we stole its callstack already
							i++;
						}
					}
				}

				// check this trace to see if it's postmortem
				if (Trace.Message.IndexOf("Postmortem Cause:", StringComparison.OrdinalIgnoreCase) != -1)
				{
					// we have post-mortem info, which should be much better than the game-generated stuff so 
					// update any previous trace
					var PrevTrace = FilteredTraces.LastOrDefault();

					if (PrevTrace != null)
					{
						PrevTrace.Callstack = Trace.Callstack;
					}
					else
					{
						// no in-log callstack? Strange... but just go with this
						FilteredTraces.Add(Trace);
					}
				}
				else
				{
					FilteredTraces.Add(Trace);
				}
			}
			

			return FilteredTraces;
		}

			


		/// <summary>
		/// Attempts to find an exit code for a test
		/// </summary>
		/// <param name="ExitCode"></param>
		/// <returns></returns>
		public bool GetTestExitCode(out int ExitCode)
		{
			Regex Reg = new Regex(@"TEST COMPLETE. EXIT CODE:\s*(\d+)\s*");

			Match M = Reg.Match(Content);

			if (M.Groups.Count > 1)
			{
				ExitCode = Convert.ToInt32(M.Groups[1].Value);
				return true;
			}

			Reg = new Regex(@"RequestExitWithStatus\(\d+,\s*(\d+)\)");

			M = Reg.Match(Content);

			if (M.Groups.Count > 1)
			{
				ExitCode = Convert.ToInt32(M.Groups[1].Value);
				return true;
			}

			if (Content.Contains("EnvironmentalPerfTest summary"))
			{
				Log.Warning("Found - 'EnvironmentalPerfTest summary', using temp workaround and assuming success (!)");
				ExitCode = 0;
				return true;
			}

			ExitCode = -1;
			return false; 
		}
	}
}