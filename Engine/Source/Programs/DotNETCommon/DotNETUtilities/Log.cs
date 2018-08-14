// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Log Event Type
	/// </summary>
	public enum LogEventType
	{
		/// <summary>
		/// The log event is a fatal error
		/// </summary>
		Fatal = 1,

		/// <summary>
		/// The log event is an error
		/// </summary>
		Error = 2,

		/// <summary>
		/// The log event is a warning
		/// </summary>
		Warning = 4,

		/// <summary>
		/// Output the log event to the console
		/// </summary>
		Console = 8,

		/// <summary>
		/// Output the event to the on-disk log
		/// </summary>
		Log = 16,

		/// <summary>
		/// The log event should only be displayed if verbose logging is enabled
		/// </summary>
		Verbose = 32,

		/// <summary>
		/// The log event should only be displayed if very verbose logging is enabled
		/// </summary>
		VeryVerbose = 64
	}

	/// <summary>
	/// Options for formatting messages
	/// </summary>
	[Flags]
	public enum LogFormatOptions
	{
		/// <summary>
		/// Format normally
		/// </summary>
		None = 0,

		/// <summary>
		/// Never write a severity prefix. Useful for pre-formatted messages that need to be in a particular format for, eg. the Visual Studio output window
		/// </summary>
		NoSeverityPrefix = 1,

		/// <summary>
		/// Do not output text to the console
		/// </summary>
		NoConsoleOutput = 2,
	}

	/// <summary>
	/// UAT/UBT Custom log system.
	/// 
	/// This lets you use any TraceListeners you want, but you should only call the static 
	/// methods below, not call Trace.XXX directly, as the static methods
	/// This allows the system to enforce the formatting and filtering conventions we desire.
	///
	/// For posterity, we cannot use the Trace or TraceSource class directly because of our special log requirements:
	///   1. We possibly capture the method name of the logging event. This cannot be done as a macro, so must be done at the top level so we know how many layers of the stack to peel off to get the real function.
	///   2. We have a verbose filter we would like to apply to all logs without having to have each listener filter individually, which would require our string formatting code to run every time.
	///   3. We possibly want to ensure severity prefixes are logged, but Trace.WriteXXX does not allow any severity info to be passed down.
	/// </summary>
	static public class Log
	{
		/// <summary>
		/// Temporary status message displayed on the console.
		/// </summary>
		[DebuggerDisplay("{HeadingText}")]
		class StatusMessage
		{
			/// <summary>
			/// The heading for this status message.
			/// </summary>
			public string HeadingText;

			/// <summary>
			/// The current status text.
			/// </summary>
			public string CurrentText;

			/// <summary>
			/// Whether the heading has been written to the console. Before the first time that lines are output to the log in the midst of a status scope, the heading will be written on a line of its own first.
			/// </summary>
			public bool bHasFlushedHeadingText;
		}

		/// <summary>
		/// Guard our initialization. Mainly used by top level exception handlers to ensure its safe to call a logging function.
		/// In general user code should not concern itself checking for this.
		/// </summary>
		private static bool bIsInitialized = false;

		/// <summary>
		/// Object used for synchronization
		/// </summary>
		private static object SyncObject = new object();

		/// <summary>
		/// When true, verbose logging is enabled.
		/// </summary>
		private static LogEventType LogLevel = LogEventType.VeryVerbose;

		/// <summary>
		/// When true, warnings and errors will have a WARNING: or ERROR: prexifx, respectively.
		/// </summary>
		private static bool bLogSeverity = false;
		
		/// <summary>
		/// When true, warnings and errors will have a prefix suitable for display by MSBuild (avoiding error messages showing as (EXEC : Error : ")
		/// </summary>
		private static bool bLogProgramNameWithSeverity = false;

		/// <summary>
		/// When true, logs will have the calling mehod prepended to the output as MethodName:
		/// </summary>
		private static bool bLogSources = false;
		
		/// <summary>
		/// When true, console output will have the calling mehod prepended to the output as MethodName:
		/// </summary>
		private static bool bLogSourcesToConsole = false;
		
		/// <summary>
		/// When true, will detect warnings and errors and set the console output color to yellow and red.
		/// </summary>
		private static bool bColorConsoleOutput = false;

		/// <summary>
		/// Whether console output is redirected. This prevents writing status updates that rely on moving the cursor.
		/// </summary>
		private static bool bAllowStatusUpdates = true;

		/// <summary>
		/// When configured, this tracks time since initialization to prepend a timestamp to each log.
		/// </summary>
		private static Stopwatch Timer;

		/// <summary>
		/// Expose the log level. This is a hack for ProcessResult.LogOutput, which wants to bypass our normal formatting scheme.
		/// </summary>
		public static bool bIsVerbose { get { return LogLevel >= LogEventType.Verbose; } }

		/// <summary>
		/// A collection of strings that have been already written once
		/// </summary>
		private static List<string> WriteOnceSet = new List<string>();

		/// <summary>
		/// Stack of status scope information.
		/// </summary>
		private static Stack<StatusMessage> StatusMessageStack = new Stack<StatusMessage>();

		/// <summary>
		/// The currently visible status text
		/// </summary>
		private static string StatusText = "";

		/// <summary>
		/// Last time a status message was pushed to the stack
		/// </summary>
		private static Stopwatch StatusTimer = new Stopwatch();

		/// <summary>
		/// Indent added to every output line
		/// </summary>
		public static string Indent
		{
			get;
			set;
		}

		/// <summary>
		/// Static initializer
		/// </summary>
		static Log()
		{
			Indent = "";
		}

		/// <summary>
		/// Allows code to check if the log system is ready yet.
		/// End users should NOT need to use this. It pretty much exists
		/// to work around startup issues since this is a global singleton.
		/// </summary>
		/// <returns></returns>
		public static bool IsInitialized()
		{
			return bIsInitialized;
		}

		/// <summary>
		/// Allows code to check if the log system is using console output color.
		/// </summary>
		/// <returns></returns>
		public static bool ColorConsoleOutput()
		{
			return bColorConsoleOutput;
		}

		/// <summary>
		/// Allows us to change verbosity after initializing. This can happen since we initialize logging early, 
		/// but then read the config and command line later, which could change this value.
		/// </summary>
		public static void SetLoggingLevel(LogEventType InLogLevel)
		{
			Log.LogLevel = InLogLevel;
		}

		/// <summary>
		/// This class allows InitLogging to be called more than once to work around chicken and eggs issues with logging and parsing command lines (see UBT startup code).
		/// </summary>
		/// <param name="bLogTimestamps">If true, the timestamp from Log init time will be prepended to all logs.</param>
		/// <param name="InLogLevel"></param>
		/// <param name="bLogSeverity">If true, warnings and errors will have a WARNING: and ERROR: prefix to them. </param>
		/// <param name="bLogProgramNameWithSeverity">If true, includes the program name with any severity prefix</param>
		/// <param name="bLogSources">If true, logs will have the originating method name prepended to them.</param>
		/// <param name="bLogSourcesToConsole">If true, console output will have the originating method name appended to it.</param>
		/// <param name="bColorConsoleOutput"></param>
		/// <param name="TraceListeners">Collection of trace listeners to attach to the Trace.Listeners, in addition to the Default listener. The existing listeners (except the Default listener) are cleared first.</param>
		public static void InitLogging(bool bLogTimestamps, LogEventType InLogLevel, bool bLogSeverity, bool bLogProgramNameWithSeverity, bool bLogSources, bool bLogSourcesToConsole, bool bColorConsoleOutput, IEnumerable<TraceListener> TraceListeners)
		{
			bIsInitialized = true;
			Timer = (bLogTimestamps && Timer == null) ? Stopwatch.StartNew() : null;
			Log.LogLevel = InLogLevel;
			Log.bLogSeverity = bLogSeverity;
			Log.bLogProgramNameWithSeverity = bLogProgramNameWithSeverity;
			Log.bLogSources = bLogSources;
			Log.bLogSourcesToConsole = bLogSourcesToConsole;
			Log.bColorConsoleOutput = bColorConsoleOutput;
			Log.bAllowStatusUpdates = !Console.IsOutputRedirected;

			// ensure that if InitLogging is called more than once we don't stack listeners.
			// but always leave the default listener around.
			for (int ListenerNdx = 0; ListenerNdx < Trace.Listeners.Count;)
			{
				if (Trace.Listeners[ListenerNdx].GetType() != typeof(DefaultTraceListener))
				{
					Trace.Listeners.RemoveAt(ListenerNdx);
				}
				else
				{
					++ListenerNdx;
				}
			}
			// don't add any null listeners
			Trace.Listeners.AddRange(TraceListeners.Where(l => l != null).ToArray());
			Trace.AutoFlush = true;
		}

		/// <summary>
		/// Gets the name of the Method N levels deep in the stack frame. Used to trap what method actually made the logging call.
		/// Only used when bLogSources is true.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <returns>ClassName.MethodName</returns>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static string GetSource(int StackFramesToSkip)
		{
			StackFrame Frame = new StackFrame(2 + StackFramesToSkip);
			System.Reflection.MethodBase Method = Frame.GetMethod();
			return String.Format("{0}.{1}", Method.DeclaringType.Name, Method.Name);
		}

		/// <summary>
		/// Converts a LogEventType into a log prefix. Only used when bLogSeverity is true.
		/// </summary>
		/// <param name="Severity"></param>
		/// <returns></returns>
		private static string GetSeverityPrefix(LogEventType Severity)
		{
			switch (Severity)
			{
				case LogEventType.Fatal:
					return "FATAL ERROR: ";
				case LogEventType.Error:
					return "ERROR: ";
				case LogEventType.Warning:
					return "WARNING: ";
				case LogEventType.Console:
					return "";
				case LogEventType.Verbose:
					return "VERBOSE: ";
				case LogEventType.VeryVerbose:
					return "VVERBOSE: ";
				default:
					return "";
			}
		}

		/// <summary>
		/// Converts a LogEventType into a message code
		/// </summary>
		/// <param name="Severity"></param>
		/// <returns></returns>
		private static int GetMessageCode(LogEventType Severity)
		{
			return (int)Severity;
		}

		/// <summary>
		/// Formats message for logging. Enforces the configured options.
		/// </summary>
		/// <param name="StackFramesToSkip">Number of frames to skip to get to the originator of the log request.</param>
		/// <param name="Verbosity">Message verbosity level</param>
		/// <param name="Options">Options for formatting this string</param>
		/// <param name="bForConsole">Whether the message is intended for console output</param>
		/// <param name="Format">Message text format string</param>
		/// <param name="Args">Message text parameters</param>
		/// <returns>Formatted message</returns>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static List<string> FormatMessage(int StackFramesToSkip, LogEventType Verbosity, LogFormatOptions Options, bool bForConsole, string Format, params object[] Args)
		{
			string TimePrefix = (Timer != null) ? String.Format("[{0:hh\\:mm\\:ss\\.fff}] ", Timer.Elapsed) : "";
			string SourcePrefix = (bForConsole ? bLogSourcesToConsole : bLogSources) ? string.Format("{0}: ", GetSource(StackFramesToSkip)) : "";
			string SeverityPrefix = (bLogSeverity && ((Options & LogFormatOptions.NoSeverityPrefix) == 0)) ? GetSeverityPrefix(Verbosity) : "";

			// Include the executable name when running inside MSBuild. If unspecified, MSBuild re-formats them with an "EXEC :" prefix.
			if(SeverityPrefix.Length > 0 && bLogProgramNameWithSeverity)
			{
				SeverityPrefix = String.Format("{0}: {1}", Path.GetFileNameWithoutExtension(Assembly.GetEntryAssembly().Location), SeverityPrefix);
			}

			// If there are no extra args, don't try to format the string, in case it has any format control characters in it (our LOCTEXT strings tend to).
			string[] Lines = ((Args.Length > 0) ? String.Format(Format, Args) : Format).TrimEnd(' ', '\t', '\r', '\n').Split('\n');

			List<string> FormattedLines = new List<string>();
			FormattedLines.Add(String.Format("{0}{1}{2}{3}{4}", TimePrefix, SourcePrefix, Indent, SeverityPrefix, Lines[0].TrimEnd('\r')));

			if (Lines.Length > 1)
			{
				int PaddingLength = 0;
				while(PaddingLength < Lines[0].Length && Char.IsWhiteSpace(Lines[0][PaddingLength]))
				{
					PaddingLength++;
				}

				string Padding = new string(' ', SeverityPrefix.Length) + Lines[0].Substring(0, PaddingLength);
				for (int Idx = 1; Idx < Lines.Length; Idx++)
				{
					FormattedLines.Add(String.Format("{0}{1}{2}{3}{4}", TimePrefix, SourcePrefix, Indent, Padding, Lines[Idx].TrimEnd('\r')));
				}
			}

			return FormattedLines;
		}

		/// <summary>
		/// Writes a formatted message to the console. All other functions should boil down to calling this method.
		/// </summary>
		/// <param name="StackFramesToSkip">Number of frames to skip to get to the originator of the log request.</param>
		/// <param name="bWriteOnce">If true, this message will be written only once</param>
		/// <param name="Verbosity">Message verbosity level. We only meaningfully use values up to Verbose</param>
		/// <param name="FormatOptions">Options for formatting messages</param>
		/// <param name="Format">Message format string.</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		private static void WriteLinePrivate(int StackFramesToSkip, bool bWriteOnce, LogEventType Verbosity, LogFormatOptions FormatOptions, string Format, params object[] Args)
		{
			if (!bIsInitialized)
			{
				throw new InvalidOperationException("Tried to using Logging system before it was ready");
			}

			// if we want this message only written one time, check if it was already written out
			if (bWriteOnce)
			{
				string Formatted = string.Format(Format, Args);
				if (WriteOnceSet.Contains(Formatted))
				{
					return;
				}

				WriteOnceSet.Add(Formatted);
			}

			if (Verbosity <= LogLevel)
			{
				lock (SyncObject)
				{
					// Output to all the other trace listeners
					List<string> Lines = FormatMessage(StackFramesToSkip + 1, Verbosity, FormatOptions, false, Format, Args);
					foreach (TraceListener Listener in Trace.Listeners)
					{
						foreach(string Line in Lines)
						{
							Listener.WriteLine(Line);
						}
						Listener.Flush();
					}

					// Handle the console output separately; we format things differently
					if ((Verbosity != LogEventType.Log || LogLevel >= LogEventType.Verbose) && (FormatOptions & LogFormatOptions.NoConsoleOutput) == 0)
					{
						FlushStatusHeading();

						bool bResetConsoleColor = false;
						if (bColorConsoleOutput)
						{
							if(Verbosity == LogEventType.Warning)
							{
								Console.ForegroundColor = ConsoleColor.Yellow;
								bResetConsoleColor = true;
							}
							if(Verbosity <= LogEventType.Error)
							{
								Console.ForegroundColor = ConsoleColor.Red;
								bResetConsoleColor = true;
							}
						}
						try
						{
							List<string> ConsoleLines = FormatMessage(StackFramesToSkip + 1, Verbosity, FormatOptions, true, Format, Args);
							foreach (string ConsoleLine in ConsoleLines)
							{
								Console.WriteLine(ConsoleLine);
							}
						}
						finally
						{
							// make sure we always put the console color back.
							if(bResetConsoleColor)
							{
								Console.ResetColor();
							}
						}

						if(StatusMessageStack.Count > 0 && bAllowStatusUpdates)
						{
							SetStatusText(StatusMessageStack.Peek().CurrentText);
						}
					}
				}
			}
		}

		/// <summary>
		/// Similar to Trace.WriteLineIf
		/// </summary>
		/// <param name="Condition"></param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineIf(bool Condition, LogEventType Verbosity, string Format, params object[] Args)
		{
			if (Condition)
			{
				WriteLinePrivate(1, false, Verbosity, LogFormatOptions.None, Format, Args);
			}
		}

		/// <summary>
		/// Mostly an internal function, but expose StackFramesToSkip to allow UAT to use existing wrapper functions and still get proper formatting.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(int StackFramesToSkip, LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(StackFramesToSkip + 1, false, Verbosity, LogFormatOptions.None, Format, Args);
		}
		/// <summary>
		/// Mostly an internal function, but expose StackFramesToSkip to allow UAT to use existing wrapper functions and still get proper formatting.
		/// </summary>
		/// <param name="StackFramesToSkip"></param>
		/// <param name="Verbosity"></param>
		/// <param name="FormatOptions"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(int StackFramesToSkip, LogEventType Verbosity, LogFormatOptions FormatOptions, string Format, params object[] Args)
		{
			WriteLinePrivate(StackFramesToSkip + 1, false, Verbosity, FormatOptions, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLine
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, Verbosity, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLine
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="FormatOptions"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLine(LogEventType Verbosity, LogFormatOptions FormatOptions, string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, Verbosity, FormatOptions, Format, Args);
		}

		/// <summary>
		/// Formats an exception for display in the log. The exception message is shown as an error, and the stack trace is included in the log.
		/// </summary>
		/// <param name="Ex">The exception to display</param>
		/// <param name="LogFileName">The log filename to display, if any</param>
		public static void WriteException(Exception Ex, string LogFileName)
		{
			string LogSuffix = (LogFileName == null)? "" : String.Format("\n(see {0} for full exception trace)", LogFileName);
			TraceLog("==============================================================================");
			TraceError("{0}{1}", ExceptionUtils.FormatException(Ex), LogSuffix);
			TraceLog("\n{0}", ExceptionUtils.FormatExceptionDetails(Ex));
			TraceLog("==============================================================================");
		}

		/// <summary>
		/// Writes an error message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceError(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Error, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes an error message to the console, in a format suitable for Visual Studio to parse.
		/// </summary>
		/// <param name="File">The file containing the error</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceError(FileReference File, string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Error, LogFormatOptions.NoSeverityPrefix, "{0}: error: {1}", File, String.Format(Format, Args));
		}

		/// <summary>
		/// Writes an error message to the console, in a format suitable for Visual Studio to parse.
		/// </summary>
		/// <param name="File">The file containing the error</param>
		/// <param name="Line">Line number of the error</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceError(FileReference File, int Line, string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Error, LogFormatOptions.NoSeverityPrefix, "{0}({1}): error: {2}", File, Line, String.Format(Format, Args));
		}

		/// <summary>
		/// Writes a verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVerbose(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Verbose, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceInformation(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Console, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarning(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Warning, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="File">The file containing the error</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarning(FileReference File, string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Warning, LogFormatOptions.NoSeverityPrefix, "{0}: warning: {1}", File, String.Format(Format, Args));
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="File">The file containing the error</param>
		/// <param name="Line">Line number of the error</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarning(FileReference File, int Line, string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Warning, LogFormatOptions.NoSeverityPrefix, "{0}({1}): warning: {2}", File, Line, String.Format(Format, Args));
		}

		/// <summary>
		/// Writes a very verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVeryVerbose(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.VeryVerbose, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a message to the log only.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceLog(string Format, params object[] Args)
		{
			WriteLinePrivate(1, false, LogEventType.Log, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLine
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineOnce(LogEventType Verbosity, string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, Verbosity, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Similar to Trace.WriteLine
		/// </summary>
		/// <param name="Verbosity"></param>
		/// <param name="Options"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void WriteLineOnce(LogEventType Verbosity, LogFormatOptions Options, string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, Verbosity, Options, Format, Args);
		}

		/// <summary>
		/// Writes an error message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceErrorOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Error, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVerboseOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Verbose, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceInformationOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Console, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarningOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Warning, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="File">The file containing the error</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarningOnce(FileReference File, string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Warning, LogFormatOptions.NoSeverityPrefix, "{0}: warning: {1}", File, String.Format(Format, Args));
		}

		/// <summary>
		/// Writes a warning message to the console.
		/// </summary>
		/// <param name="File">The file containing the error</param>
		/// <param name="Line">Line number of the error</param>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceWarningOnce(FileReference File, int Line, string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Warning, LogFormatOptions.NoSeverityPrefix, "{0}({1}): warning: {2}", File, Line, String.Format(Format, Args));
		}

		/// <summary>
		/// Writes a very verbose message to the console.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceVeryVerboseOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.VeryVerbose, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Writes a message to the log only.
		/// </summary>
		/// <param name="Format">Message format string</param>
		/// <param name="Args">Optional arguments</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void TraceLogOnce(string Format, params object[] Args)
		{
			WriteLinePrivate(1, true, LogEventType.Log, LogFormatOptions.None, Format, Args);
		}

		/// <summary>
		/// Flushes the current status text before writing out a new log line or status message
		/// </summary>
		static void FlushStatusHeading()
		{
			if(StatusMessageStack.Count > 0)
			{
				StatusMessage CurrentStatus = StatusMessageStack.Peek();
				if(CurrentStatus.HeadingText.Length > 0 && !CurrentStatus.bHasFlushedHeadingText && bAllowStatusUpdates)
				{
					SetStatusText(CurrentStatus.HeadingText);
					Console.WriteLine();
					StatusText = "";
					CurrentStatus.bHasFlushedHeadingText = true;
				}
				else
				{
					SetStatusText("");
				}
			}
		}

		/// <summary>
		/// Enter a scope with the given status message. The message will be written to the console without a newline, allowing it to be updated through subsequent calls to UpdateStatus().
		/// The message will be written to the log immediately. If another line is written while in a status scope, the initial status message is flushed to the console first.
		/// </summary>
		/// <param name="Message">The status message</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void PushStatus(string Message)
		{
			lock(SyncObject)
			{
				FlushStatusHeading();

				StatusMessage NewStatusMessage = new StatusMessage();
				NewStatusMessage.HeadingText = Message;
				NewStatusMessage.CurrentText = Message;
				StatusMessageStack.Push(NewStatusMessage);

				StatusTimer.Restart();

				if(Message.Length > 0)
				{
					WriteLine(LogEventType.Log, LogFormatOptions.NoConsoleOutput, "{0}", Message);
					SetStatusText(Message);
				}
			}
		}

		/// <summary>
		/// Updates the current status message. This will overwrite the previous status line.
		/// </summary>
		/// <param name="Message">The status message</param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void UpdateStatus(string Message)
		{
			lock(SyncObject)
			{
				StatusMessage CurrentStatusMessage = StatusMessageStack.Peek();
				CurrentStatusMessage.CurrentText = Message;

				if(bAllowStatusUpdates || StatusTimer.Elapsed.TotalSeconds > 10.0)
				{
					SetStatusText(Message);
					StatusTimer.Restart();
				}
			}
		}

		/// <summary>
		/// Updates the Pops the top status message from the stack. The mess
		/// </summary>
		/// <param name="Message"></param>
		[Conditional("TRACE")]
		[MethodImplAttribute(MethodImplOptions.NoInlining)]
		public static void PopStatus()
		{
			lock(SyncObject)
			{
				StatusMessage CurrentStatusMessage = StatusMessageStack.Peek();
				SetStatusText(CurrentStatusMessage.CurrentText);

				if(StatusText.Length > 0)
				{
					Console.WriteLine();
					StatusText = "";
				}

				StatusMessageStack.Pop();
			}
		}

		/// <summary>
		/// Update the status text. For internal use only; does not modify the StatusMessageStack objects.
		/// </summary>
		/// <param name="NewStatusText">New status text to display</param>
		private static void SetStatusText(string NewStatusText)
		{
			if(NewStatusText.Length > 0)
			{
				NewStatusText = Indent + NewStatusText;
			}

			if(StatusText != NewStatusText)
			{
				int NumCommonChars = 0;
				while(NumCommonChars < StatusText.Length && NumCommonChars < NewStatusText.Length && StatusText[NumCommonChars] == NewStatusText[NumCommonChars])
				{
					NumCommonChars++;
				}

				if(!bAllowStatusUpdates && NumCommonChars < StatusText.Length)
				{
					// Prevent writing backspace characters if the console doesn't support it
					Console.WriteLine();
					StatusText = "";
					NumCommonChars = 0;
				}

				StringBuilder Text = new StringBuilder();
				Text.Append('\b', StatusText.Length - NumCommonChars);
				Text.Append(NewStatusText, NumCommonChars, NewStatusText.Length - NumCommonChars);
				if(NewStatusText.Length < StatusText.Length)
				{
					int NumChars = StatusText.Length - NewStatusText.Length;
					Text.Append(' ', NumChars);
					Text.Append('\b', NumChars);
				}
				Console.Write(Text.ToString());

				StatusText = NewStatusText;
				StatusTimer.Restart();
			}
		}
	}
}
