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
	[ToolMode("Build")]
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

			DateTime StartTime = DateTime.UtcNow;

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

			// Get a mutex for building in this branch
			using(SingleInstanceMutex.Acquire(SingleInstanceMutexType.PerBranch, Arguments))
			{
				// Change the working directory to be the Engine/Source folder. We are likely running from Engine/Binaries/DotNET
				// This is critical to be done early so any code that relies on the current directory being Engine/Source will work.
				DirectoryReference.SetCurrentDirectory(UnrealBuildTool.EngineSourceDirectory);

				// Read the XML configuration files
				FileReference XmlConfigCache = Arguments.GetFileReferenceOrDefault("-XmlConfigCache=", null);
				XmlConfig.ReadConfigFiles(XmlConfigCache);
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

				// Copy some of the static settings that are being deprecated from BuildConfiguration
				UnrealBuildTool.bPrintPerformanceInfo = BuildConfiguration.bPrintPerformanceInfo;

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

				// Build the list of game projects that we know about. When building from the editor (for hot-reload) or for projects from installed builds, we require the 
				// project file to be passed in. Otherwise we scan for projects in directories named in UE4Games.uprojectdirs.
				FileReference ProjectFile;
				UnrealBuildTool.TryParseProjectFileArgument(Arguments, out ProjectFile);
				
				DateTime BasicInitStartTime = DateTime.UtcNow;

				// Find and register all tool chains, build platforms, etc. that are present
				UnrealBuildTool.RegisterAllUBTClasses(false);

				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					double BasicInitTime = (DateTime.UtcNow - BasicInitStartTime).TotalSeconds;
					Log.TraceInformation("Basic UBT initialization took " + BasicInitTime + "s");
				}

				// now that we know the available platforms, we can delete other platforms' junk. if we're only building specific modules from the editor, don't touch anything else (it may be in use).
				if (!BuildConfiguration.bIgnoreJunk)
				{
					JunkDeleter.DeleteJunk();
				}

				// Build our project
				ECompilationResult Result = UnrealBuildTool.RunUBT(BuildConfiguration, Arguments.GetRawArray(), ProjectFile, LogFile);

				// Print some performance info
				double BuildDuration = (DateTime.UtcNow - StartTime).TotalSeconds;
				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					Log.TraceInformation("GetIncludes time: " + CPPHeaders.TotalTimeSpentGettingIncludes + "s (" + CPPHeaders.TotalIncludesRequested + " includes)");
					Log.TraceInformation("DirectIncludes cache miss time: " + CPPHeaders.DirectIncludeCacheMissesTotalTime + "s (" + CPPHeaders.TotalDirectIncludeCacheMisses + " misses)");
					Log.TraceInformation("FindIncludePaths calls: " + CPPHeaders.TotalFindIncludedFileCalls + " (" + CPPHeaders.IncludePathSearchAttempts + " searches)");
					Log.TraceInformation("Deep C++ include scan time: " + UnrealBuildTool.TotalDeepIncludeScanTime + "s");
					Log.TraceInformation("Include Resolves: {0} ({1} misses, {2:0.00}%)", CPPHeaders.TotalDirectIncludeResolves, CPPHeaders.TotalDirectIncludeResolveCacheMisses, (float)CPPHeaders.TotalDirectIncludeResolveCacheMisses / (float)CPPHeaders.TotalDirectIncludeResolves * 100);
					Log.TraceInformation("Total FileItems: {0} ({1} missing)", FileItem.TotalFileItemCount, FileItem.MissingFileItemCount);

					Log.TraceInformation("Execution time: {0}s", BuildDuration);
				}

				// Print some performance info
				Log.TraceVerbose("Execution time: {0}", (DateTime.UtcNow - StartTime).TotalSeconds);

				return (int)Result;
			}
		}
	}
}

