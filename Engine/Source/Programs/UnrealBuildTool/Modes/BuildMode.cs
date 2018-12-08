// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Builds a target
	/// </summary>
	[ToolMode("Build", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance)]
	class BuildMode : ToolMode
	{
		/// <summary>
		/// Specifies the file to use for logging
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public string BaseLogFileName = "../Programs/UnrealBuildTool/Log.txt";

		/// <summary>
		/// Main entry point
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <returns>One of the values of ECompilationResult</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			Arguments.ApplyTo(this);

			// Initialize the log system, buffering the output until we can create the log file
			StartupTraceListener StartupListener = new StartupTraceListener();
			Trace.Listeners.Add(StartupListener);

			// Write the command line
			Log.TraceLog("Command line: {0}", Environment.CommandLine);

			// Grab the environment.
			UnrealBuildTool.InitialEnvironment = Environment.GetEnvironmentVariables();
			if (UnrealBuildTool.InitialEnvironment.Count < 1)
			{
				throw new BuildException("Environment could not be read");
			}

			// Read the XML configuration files
			XmlConfig.ApplyTo(this);

			// Create the log file, and flush the startup listener to it
			FileReference LogFile = null;
			if(!Arguments.HasOption("-NoLog") && !Log.HasFileWriter())
			{
				LogFile = new FileReference(BaseLogFileName);
				foreach(string LogSuffix in Arguments.GetValues("-LogSuffix="))
				{
					LogFile = LogFile.ChangeExtension(null) + "_" + LogSuffix + LogFile.GetExtension();
				}

				TextWriterTraceListener LogTraceListener = Log.AddFileWriter("DefaultLogTraceListener", LogFile);
				StartupListener.CopyTo(LogTraceListener);
			}
			Trace.Listeners.Remove(StartupListener);

			// Create the build configuration object, and read the settings
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			XmlConfig.ApplyTo(BuildConfiguration);
			Arguments.ApplyTo(BuildConfiguration);

			// Then let the command lines override any configs necessary.
			if(BuildConfiguration.bXGEExport)
			{
				BuildConfiguration.bAllowXGE = true;
			}
			if(BuildConfiguration.SingleFileToCompile != null)
			{
				BuildConfiguration.bUseUBTMakefiles = false;
			}

			// Parse the remote INI setting
			string RemoteIniPath;
			Arguments.TryGetValue("-RemoteIni=", out RemoteIniPath);
			UnrealBuildTool.SetRemoteIniPath(RemoteIniPath);

			Timeline.AddEvent("Basic UBT initialization");

			// now that we know the available platforms, we can delete other platforms' junk. if we're only building specific modules from the editor, don't touch anything else (it may be in use).
			if (!BuildConfiguration.bIgnoreJunk)
			{
				JunkDeleter.DeleteJunk();
			}

			// Build our project
			using(Timeline.ScopeEvent("Calling RunUBT"))
			{
				ECompilationResult Result = UnrealBuildTool.RunUBT(BuildConfiguration, Arguments.GetRawArray(), LogFile);

				// Print some performance info
				Log.TraceLog("DirectIncludes cache miss time: {0}s ({1} misses)", CPPHeaders.DirectIncludeCacheMissesTotalTime, CPPHeaders.TotalDirectIncludeCacheMisses);
				Log.TraceLog("FindIncludePaths calls: {0} ({1} searches)", CPPHeaders.TotalFindIncludedFileCalls, CPPHeaders.IncludePathSearchAttempts);
				Log.TraceLog("Deep C++ include scan time: {0}s", UnrealBuildTool.TotalDeepIncludeScanTime);
				Log.TraceLog("Include Resolves: {0} ({1} misses, {2:0.00}%)", CPPHeaders.TotalDirectIncludeResolves, CPPHeaders.TotalDirectIncludeResolveCacheMisses, (float)CPPHeaders.TotalDirectIncludeResolveCacheMisses / (float)CPPHeaders.TotalDirectIncludeResolves * 100);
				Log.TraceLog("Total FileItems: {0} ({1} missing)", FileItem.TotalFileItemCount, FileItem.MissingFileItemCount);

				return (int)Result;
			}
		}
	}
}

