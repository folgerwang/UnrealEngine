// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	static class UnrealBuildTool
	{
		/// <summary>
		/// How much time was spent scanning for include dependencies for outdated C++ files
		/// </summary>
		public static double TotalDeepIncludeScanTime = 0.0;

		/// <summary>
		/// The environment at boot time.
		/// </summary>
		static public System.Collections.IDictionary InitialEnvironment;

		/// <summary>
		/// Whether we're running with engine installed
		/// </summary>
		static private bool? bIsEngineInstalled;

		/// <summary>
		/// Whether we're running with enterprise installed
		/// </summary>
		static private bool? bIsEnterpriseInstalled;

		/// <summary>
		/// Whether we're running with an installed project
		/// </summary>
		static private bool? bIsProjectInstalled;
		
		/// <summary>
		/// If we are running with an installed project, specifies the path to it
		/// </summary>
		static FileReference InstalledProjectFile;

		/// <summary>
		/// The full name of the Root UE4 directory
		/// </summary>
		public static readonly DirectoryReference RootDirectory = new DirectoryReference(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().GetOriginalLocation()), "..", "..", ".."));

		/// <summary>
		/// The full name of the Engine directory
		/// </summary>
		public static readonly DirectoryReference EngineDirectory = DirectoryReference.Combine(RootDirectory, "Engine");

		/// <summary>
		/// The full name of the Engine/Source directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceDirectory = DirectoryReference.Combine(EngineDirectory, "Source");

		/// <summary>
		/// Full path to the Engine/Source/Runtime directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceRuntimeDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Runtime");

		/// <summary>
		/// Full path to the Engine/Source/Developer directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceDeveloperDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Developer");

		/// <summary>
		/// Full path to the Engine/Source/Editor directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceEditorDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Editor");

		/// <summary>
		/// Full path to the Engine/Source/Programs directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceProgramsDirectory = DirectoryReference.Combine(EngineSourceDirectory, "Programs");

		/// <summary>
		/// Full path to the Engine/Source/ThirdParty directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceThirdPartyDirectory = DirectoryReference.Combine(EngineSourceDirectory, "ThirdParty");

		/// <summary>
		/// The full name of the Enterprise directory
		/// </summary>
		public static readonly DirectoryReference EnterpriseDirectory = DirectoryReference.Combine(RootDirectory, "Enterprise");

		/// <summary>
		/// The full name of the Enterprise/Source directory
		/// </summary>
		public static readonly DirectoryReference EnterpriseSourceDirectory = DirectoryReference.Combine(EnterpriseDirectory, "Source");

		/// <summary>
		/// The full name of the Enterprise/Plugins directory
		/// </summary>
		public static readonly DirectoryReference EnterprisePluginsDirectory = DirectoryReference.Combine(EnterpriseDirectory, "Plugins");

		/// <summary>
		/// The full name of the Enterprise/Intermediate directory
		/// </summary>
		public static readonly DirectoryReference EnterpriseIntermediateDirectory = DirectoryReference.Combine(EnterpriseDirectory, "Intermediate");

		/// <summary>
		/// The Remote Ini directory.  This should always be valid when compiling using a remote server.
		/// </summary>
		static string RemoteIniPath = null;

		/// <summary>
		/// Whether to print debug information out to the log
		/// </summary>
		static public bool bPrintDebugInfo
		{
			get;
			private set;
		}

		/// <summary>
		/// Whether to print performance information to the log
		/// </summary>
		static public bool bPrintPerformanceInfo
		{
			get;
			private set;
		}

		/// <summary>
		/// Returns true if UnrealBuildTool is running using installed Engine components
		/// </summary>
		/// <returns>True if running using installed Engine components</returns>
		static public bool IsEngineInstalled()
		{
			if (!bIsEngineInstalled.HasValue)
			{
				bIsEngineInstalled = FileReference.Exists(FileReference.Combine(EngineDirectory, "Build", "InstalledBuild.txt"));
			}
			return bIsEngineInstalled.Value;
		}

		/// <summary>
		/// Returns true if UnrealBuildTool is running using installed Enterprise components
		/// </summary>
		/// <returns>True if running using installed Enterprise components</returns>
		static public bool IsEnterpriseInstalled()
		{
			if (!bIsEnterpriseInstalled.HasValue)
			{
				bIsEnterpriseInstalled = FileReference.Exists(FileReference.Combine(EnterpriseDirectory, "Build", "InstalledBuild.txt"));
			}
			return bIsEnterpriseInstalled.Value;
		}

		/// <summary>
		/// Returns true if UnrealBuildTool is running using an installed project (ie. a mod kit)
		/// </summary>
		/// <returns>True if running using an installed project</returns>
		static public bool IsProjectInstalled()
		{
			if (!bIsProjectInstalled.HasValue)
			{
				FileReference InstalledProjectLocationFile = FileReference.Combine(UnrealBuildTool.RootDirectory, "Engine", "Build", "InstalledProjectBuild.txt");
				if (FileReference.Exists(InstalledProjectLocationFile))
				{
					InstalledProjectFile = FileReference.Combine(UnrealBuildTool.RootDirectory, File.ReadAllText(InstalledProjectLocationFile.FullName).Trim());
					bIsProjectInstalled = true;
				}
				else
				{
					InstalledProjectFile = null;
					bIsProjectInstalled = false;
				}
			}
			return bIsProjectInstalled.Value;
		}

		/// <summary>
		/// Checks whether the given file is under an installed directory, and should not be overridden
		/// </summary>
		/// <param name="File">File to test</param>
		/// <returns>True if the file is part of the installed distribution, false otherwise</returns>
		static public bool IsFileInstalled(FileReference File)
		{
			if(IsEngineInstalled() && File.IsUnderDirectory(EngineDirectory))
			{
				return true;
			}
			if(IsEnterpriseInstalled() && File.IsUnderDirectory(EnterpriseDirectory))
			{
				return true;
			}
			if(IsProjectInstalled() && File.IsUnderDirectory(InstalledProjectFile.Directory))
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Gets the absolute path to the UBT assembly.
		/// </summary>
		/// <returns>A string containing the path to the UBT assembly.</returns>
		static public string GetUBTPath()
		{
			string UnrealBuildToolPath = Assembly.GetExecutingAssembly().GetOriginalLocation();
			return UnrealBuildToolPath;
		}

		/// <summary>
		/// The Unreal remote tool ini directory.  This should be valid if compiling using a remote server
		/// </summary>
		/// <returns>The directory path</returns>
		static public string GetRemoteIniPath()
		{
			return RemoteIniPath;
		}

		static public void SetRemoteIniPath(string Path)
		{
			RemoteIniPath = Path;
		}

		/// <summary>
		/// Determines whether a directory is part of the engine
		/// </summary>
		/// <param name="InDirectory"></param>
		/// <returns>true if the directory is under of the engine directories, false if not</returns>
		static public bool IsUnderAnEngineDirectory(DirectoryReference InDirectory)
		{
			// Enterprise modules are considered as engine modules
			return InDirectory.IsUnderDirectory(UnrealBuildTool.EngineDirectory) || InDirectory.IsUnderDirectory(UnrealBuildTool.EnterpriseSourceDirectory) ||
				InDirectory.IsUnderDirectory(UnrealBuildTool.EnterprisePluginsDirectory) || InDirectory.IsUnderDirectory(UnrealBuildTool.EnterpriseIntermediateDirectory);
		}

		public static void RegisterAllUBTClasses(bool bValidatingPlatforms)
		{
			// Find and register all tool chains and build platforms that are present
			Assembly UBTAssembly = Assembly.GetExecutingAssembly();
			if (UBTAssembly != null)
			{
				Log.TraceVerbose("Searching for ToolChains, BuildPlatforms, BuildDeploys and ProjectGenerators...");

				List<System.Type> ProjectGeneratorList = new List<System.Type>();
				Type[] AllTypes = UBTAssembly.GetTypes();

				// register all build platforms first, since they implement SDK-switching logic that can set environment variables
				foreach (Type CheckType in AllTypes)
				{
					if (CheckType.IsClass && !CheckType.IsAbstract)
					{
						if (CheckType.IsSubclassOf(typeof(UEBuildPlatformFactory)))
						{
							Log.TraceVerbose("    Registering build platform: {0}", CheckType.ToString());
							UEBuildPlatformFactory TempInst = (UEBuildPlatformFactory)(UBTAssembly.CreateInstance(CheckType.FullName, true));

							// We need all platforms to be registered when we run -validateplatform command to check SDK status of each
							if (bValidatingPlatforms || InstalledPlatformInfo.IsValidPlatform(TempInst.TargetPlatform))
							{
								TempInst.RegisterBuildPlatforms();
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Try to parse the project file from the command line
		/// </summary>
		/// <param name="Arguments">The command line arguments</param>
		/// <param name="ProjectFile">The project file that was parsed</param>
		/// <returns>True if the project file was parsed, false otherwise</returns>
		public static bool TryParseProjectFileArgument(CommandLineArguments Arguments, out FileReference ProjectFile)
		{
			FileReference ExplicitProjectFile;
			if(Arguments.TryGetValue("-Project=", out ExplicitProjectFile))
			{
				ProjectFile = ExplicitProjectFile;
				return true;
			}

			for(int Idx = 0; Idx < Arguments.Count; Idx++)
			{
				if(Arguments[Idx][0] != '-' && Arguments[Idx].EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
				{
					Arguments.MarkAsUsed(Idx);
					ProjectFile = new FileReference(Arguments[Idx]);
					return true;
				}
			}

			if(IsProjectInstalled())
			{
				ProjectFile = InstalledProjectFile;
				return true;
			}

			ProjectFile = null;
			return false;
		}

		/// <summary>
		/// UBT startup order is fairly fragile, and relies on globals that may or may not be safe to use yet.
		/// This function is for super early startup stuff that should not access Configuration classes (anything loaded by XmlConfg).
		/// This should be very minimal startup code.
		/// </summary>
		/// <param name="CmdLine">Cmdline arguments</param>
		internal static int GuardedMain(CommandLineArguments CmdLine)
		{
			DateTime StartTime = DateTime.UtcNow;
			ECompilationResult Result = ECompilationResult.Succeeded;

			// Initialize the log system, buffering the output until we can create the log file
			StartupTraceListener StartupListener = new StartupTraceListener();
			Trace.Listeners.Add(StartupListener);

			// Write the command line
			Log.TraceLog("Command line: {0}", Environment.CommandLine);

			// Grab the environment.
			InitialEnvironment = Environment.GetEnvironmentVariables();
			if (InitialEnvironment.Count < 1)
			{
				throw new BuildException("Environment could not be read");
			}

			using(SingleInstanceMutex.Acquire(SingleInstanceMutexType.PerBranch, CmdLine))
			{
				string[] Arguments = CmdLine.GetRawArray();
				try
				{

					// Change the working directory to be the Engine/Source folder. We are likely running from Engine/Binaries/DotNET
					// This is critical to be done early so any code that relies on the current directory being Engine/Source will work.
					DirectoryReference.SetCurrentDirectory(EngineSourceDirectory);

					// Read the XML configuration files
					FileReference XmlConfigCache = CmdLine.GetFileReferenceOrDefault("-XmlConfigCache=", null);
					XmlConfig.ReadConfigFiles(XmlConfigCache);

					// Create the build configuration object, and read the settings
					BuildConfiguration BuildConfiguration = new BuildConfiguration();
					XmlConfig.ApplyTo(BuildConfiguration);
					CommandLine.ParseArguments(Arguments, BuildConfiguration);

					// Copy some of the static settings that are being deprecated from BuildConfiguration
					bPrintDebugInfo = BuildConfiguration.bPrintDebugInfo || Log.OutputLevel == LogEventType.Verbose || Log.OutputLevel == LogEventType.VeryVerbose;
					bPrintPerformanceInfo = BuildConfiguration.bPrintPerformanceInfo;

					// Then let the command lines override any configs necessary.
					string LogSuffix = "";
					foreach (string Argument in Arguments)
					{
						string LowercaseArg = Argument.ToLowerInvariant();
						if (LowercaseArg.StartsWith("-LogSuffix=", StringComparison.OrdinalIgnoreCase))
						{
							LogSuffix += "_" + Argument.Substring("-LogSuffix=".Length);
						}
						else if (LowercaseArg == "-nolog")
						{
							BuildConfiguration.LogFileName = null;
						}
						else if (LowercaseArg == "-xgeexport")
						{
							BuildConfiguration.bXGEExport = true;
							BuildConfiguration.bAllowXGE = true;
						}
						else if (LowercaseArg.StartsWith("-singlefile="))
						{
							BuildConfiguration.bUseUBTMakefiles = false;
							BuildConfiguration.SingleFileToCompile = LowercaseArg.Replace("-singlefile=", "");
						}
					}

					// Parse the remote INI setting
					CmdLine.TryGetValue("-RemoteIni=", out RemoteIniPath);

					// Create the log file, and flush the startup listener to it
					if (!String.IsNullOrEmpty(BuildConfiguration.LogFileName) && !Log.HasFileWriter())
					{
						FileReference LogLocation = new FileReference(BuildConfiguration.LogFileName);
						if(LogSuffix.Length > 0)
						{
							LogLocation = LogLocation.ChangeExtension(null) + LogSuffix + LogLocation.GetExtension();
						}

						TextWriterTraceListener LogTraceListener = Log.AddFileWriter("DefaultLogTraceListener", LogLocation);
						StartupListener.CopyTo(LogTraceListener);
					}
					Trace.Listeners.Remove(StartupListener);

					// Build the list of game projects that we know about. When building from the editor (for hot-reload) or for projects from installed builds, we require the 
					// project file to be passed in. Otherwise we scan for projects in directories named in UE4Games.uprojectdirs.
					FileReference ProjectFile;
					if (TryParseProjectFileArgument(CmdLine, out ProjectFile))
					{
						UProjectInfo.AddProject(ProjectFile);
					}
					else
					{
						UProjectInfo.FillProjectInfo();
					}

					// Read the project-specific build configuration settings
					ConfigCache.ReadSettings(DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Unknown, BuildConfiguration);

					DateTime BasicInitStartTime = DateTime.UtcNow;

					bool bSpecificModulesOnly = false;
					foreach (string Arg in Arguments)
					{
						string LowercaseArg = Arg.ToLowerInvariant();
						if (LowercaseArg.StartsWith("-modulewithsuffix="))
						{
							bSpecificModulesOnly = true;
							continue;
						}
						else if (LowercaseArg == "-vsdebugandroid")
						{
							AndroidProjectGenerator.VSDebugCommandLineOptionPresent = true;
						}
					}

					// Find and register all tool chains, build platforms, etc. that are present
					RegisterAllUBTClasses(false);

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						double BasicInitTime = (DateTime.UtcNow - BasicInitStartTime).TotalSeconds;
						Log.TraceInformation("Basic UBT initialization took " + BasicInitTime + "s");
					}

					// now that we know the available platforms, we can delete other platforms' junk. if we're only building specific modules from the editor, don't touch anything else (it may be in use).
					if (!bSpecificModulesOnly && !BuildConfiguration.bIgnoreJunk)
					{
						JunkDeleter.DeleteJunk();
					}

					// Build our project
					if (Result == ECompilationResult.Succeeded)
					{
						Result = RunUBT(BuildConfiguration, Arguments, ProjectFile);
					}

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
				}
				catch (Exception Exception)
				{
					Log.TraceError("UnrealBuildTool Exception: " + Exception);
					Result = ECompilationResult.OtherCompilationError;
				}
			}

			// Print some performance info
			Log.TraceVerbose("Execution time: {0}", (DateTime.UtcNow - StartTime).TotalSeconds);

			return (int)Result;
		}

		/// <summary>
		/// Global options for UBT (any modes)
		/// </summary>
		class GlobalOptions
		{
			/// <summary>
			/// The amount of detail to write to the log
			/// </summary>
			[CommandLine(Prefix = "-Verbose", Value ="Verbose")]
			[CommandLine(Prefix = "-VeryVerbose", Value ="VeryVerbose")]
			public LogEventType LogOutputLevel = LogEventType.Log;

			/// <summary>
			/// Specifies the path to a log file to write. Note that the default mode (eg. building, generating project files) will create a log file by default if this not specified.
			/// </summary>
			[CommandLine(Prefix = "-Log")]
			public FileReference LogFileName = null;

			/// <summary>
			/// Whether to include timestamps in the log
			/// </summary>
			[CommandLine(Prefix = "-Timestamps")]
			public bool bLogTimestamps = false;

			/// <summary>
			/// Whether to format messages in MsBuild format
			/// </summary>
			[CommandLine(Prefix = "-FromMsBuild")]
			public bool bLogFromMsBuild = false;

			/// <summary>
			/// Whether to write progress markup in a format that can be parsed by other programs
			/// </summary>
			[CommandLine(Prefix = "-Progress")]
			public bool bWriteProgressMarkup = false;

			/// <summary>
			/// The mode to execute
			/// </summary>
			[CommandLine]
			[CommandLine("-ProjectFiles", Value="GenerateProjectFiles")]
			[CommandLine("-ProjectFileFormat=", Value="GenerateProjectFiles")]
			[CommandLine("-Makefile", Value="GenerateProjectFiles")]
			[CommandLine("-CMakefile", Value="GenerateProjectFiles")]
			[CommandLine("-QMakefile", Value="GenerateProjectFiles")]
			[CommandLine("-KDevelopfile", Value="GenerateProjectFiles")]
			[CommandLine("-CodeliteFiles", Value="GenerateProjectFiles")]
			[CommandLine("-XCodeProjectFiles", Value="GenerateProjectFiles")]
			[CommandLine("-EdditProjectFiles", Value="GenerateProjectFiles")]
			[CommandLine("-VSCode", Value="GenerateProjectFiles")]
			[CommandLine("-VSMac", Value="GenerateProjectFiles")]
			[CommandLine("-CLion", Value="GenerateProjectFiles")]
			public string Mode = "Build";

			/// <summary>
			/// Initialize the options with the given commnad line arguments
			/// </summary>
			/// <param name="Arguments"></param>
			public GlobalOptions(CommandLineArguments Arguments)
			{
				Arguments.ApplyTo(this);
			}
		}

		/// <summary>
		/// Main entry point. Parses any global options and initializes the logging system, then invokes the appropriate command.
		/// </summary>
		/// <param name="ArgumentsArray">Command line arguments</param>
		/// <returns>Zero on success, non-zero on error</returns>
		private static int Main(string[] ArgumentsArray)
		{
			try
			{
				// Parse the command line arguments
				CommandLineArguments Arguments = new CommandLineArguments(ArgumentsArray);

				// Parse the global options
				GlobalOptions Options = new GlobalOptions(Arguments);

				// Configure the log system
				Log.OutputLevel = Options.LogOutputLevel;
				Log.IncludeTimestamps = Options.bLogTimestamps;
				Log.IncludeProgramNameWithSeverityPrefix = Options.bLogFromMsBuild;
				
				// Configure the progress writer
				ProgressWriter.bWriteMarkup = Options.bWriteProgressMarkup;

				// Add the log writer if requested. When building a target, we'll create the writer for the default log file later.
				if(Options.LogFileName != null)
				{
					Log.AddFileWriter("LogTraceListener", Options.LogFileName);
				}

				// Find all the valid modes
				Dictionary<string, Type> ModeNameToType = new Dictionary<string, Type>(StringComparer.OrdinalIgnoreCase);
				foreach(Type Type in Assembly.GetExecutingAssembly().GetTypes())
				{
					if(Type.IsClass && !Type.IsAbstract && Type.IsSubclassOf(typeof(ToolMode)))
					{
						ToolModeAttribute Attribute = Type.GetCustomAttribute<ToolModeAttribute>();
						if(Attribute == null)
						{
							throw new BuildException("Class '{0}' should have a ToolModeAttribute", Type.Name);
						}
						ModeNameToType.Add(Attribute.Name, Type);
					}
				}

				// Ensure we can resolve any external assemblies that are not in the same folder as our assembly.
				AssemblyUtils.InstallAssemblyResolver(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()));

				// Try to get the correct mode
				Type ModeType;
				if(!ModeNameToType.TryGetValue(Options.Mode, out ModeType))
				{
					Log.TraceError("No mode named '{0}'. Available modes are:\n  {1}", Options.Mode, String.Join("\n  ", ModeNameToType.Keys));
					return 1;
				}

				// Create the appropriate handler
				ToolMode Mode = (ToolMode)Activator.CreateInstance(ModeType);
				return Mode.Execute(Arguments);
			}
			catch (BuildException Ex)
			{
				// BuildExceptions should have nicely formatted messages. We can log these directly.
				Log.TraceError(Ex.Message.ToString());
				Log.TraceLog(ExceptionUtils.FormatExceptionDetails(Ex));
				return (int)ECompilationResult.OtherCompilationError;
			}
			catch (Exception Ex)
			{
				// Unhandled exception. 
				Log.TraceError("Unhandled exception: {0}", Ex);
				Log.TraceLog(ExceptionUtils.FormatExceptionDetails(Ex));
				return (int)ECompilationResult.OtherCompilationError;
			}
			finally
			{
				// Make sure we flush the logs however we exit
				Trace.Close();
			}
		}

		internal static ECompilationResult RunUBT(BuildConfiguration BuildConfiguration, string[] Arguments, FileReference ProjectFile)
		{
			bool bSuccess = true;

			DateTime RunUBTInitStartTime = DateTime.UtcNow;


			// Reset global configurations
			ActionGraph ActionGraph = new ActionGraph();

			string ExecutorName = "Unknown";
			ECompilationResult BuildResult = ECompilationResult.Succeeded;

			CppIncludeBackgroundThread CppIncludeThread = null;

			List<UEBuildTarget> Targets = null;
			Dictionary<UEBuildTarget, CPPHeaders> TargetToHeaders = new Dictionary<UEBuildTarget, CPPHeaders>();

			double TotalExecutorTime = 0.0;

			try
			{
				List<string[]> TargetSettings = ParseCommandLineFlags(Arguments);

				int ArgumentIndex;
				// action graph implies using the dependency resolve cache
				bool GeneratingActionGraph = Utils.ParseCommandLineFlag(Arguments, "-graph", out ArgumentIndex);
				if (GeneratingActionGraph)
				{
					BuildConfiguration.bUseIncludeDependencyResolveCache = true;
				}

				if (UnrealBuildTool.bPrintPerformanceInfo)
				{
					double RunUBTInitTime = (DateTime.UtcNow - RunUBTInitStartTime).TotalSeconds;
					Log.TraceInformation("RunUBT initialization took " + RunUBTInitTime + "s");
				}

				bool bSkipRulesCompile = Arguments.Any(x => x.Equals("-skiprulescompile", StringComparison.InvariantCultureIgnoreCase));

				List<TargetDescriptor> TargetDescs = new List<TargetDescriptor>();
				{
					DateTime TargetDescstStartTime = DateTime.UtcNow;

					foreach (string[] TargetSetting in TargetSettings)
					{
						TargetDescs.AddRange(TargetDescriptor.ParseCommandLine(TargetSetting, BuildConfiguration.bUsePrecompiled, bSkipRulesCompile, ref ProjectFile));
					}

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						double TargetDescsTime = (DateTime.UtcNow - TargetDescstStartTime).TotalSeconds;
						Log.TraceInformation("Target descriptors took " + TargetDescsTime + "s");
					}
				}

				// Handle remote builds
				if(BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
				{
					for(int Idx = 0; Idx < TargetDescs.Count; Idx++)
					{
						TargetDescriptor TargetDesc = TargetDescs[Idx];
						if(TargetDesc.Platform == UnrealTargetPlatform.Mac || TargetDesc.Platform == UnrealTargetPlatform.IOS || TargetDesc.Platform == UnrealTargetPlatform.TVOS)
						{
							FileReference LogFile = null;
							if(!String.IsNullOrEmpty(BuildConfiguration.LogFileName))
							{
								LogFile = new FileReference(Path.Combine(Path.GetDirectoryName(BuildConfiguration.LogFileName), Path.GetFileNameWithoutExtension(BuildConfiguration.LogFileName) + "_Remote.txt"));
							}

							RemoteMac RemoteMac = new RemoteMac(TargetDesc.ProjectFile);
							if(!RemoteMac.Build(TargetDesc, LogFile))
							{
								return ECompilationResult.Unknown;
							}

							TargetDescs.RemoveAt(Idx--);
						}
					}
					if(TargetDescs.Count == 0)
					{
						return ECompilationResult.Succeeded;
					}
				}

				if (Arguments.Any(x => x.Equals("-InvalidateMakefilesOnly", StringComparison.InvariantCultureIgnoreCase)))
				{
					Log.TraceInformation("Invalidating makefiles only in this run.");
					if (TargetDescs.Count != 1)
					{
						Log.TraceError("You have to provide one target name for makefile invalidation.");
						return ECompilationResult.OtherCompilationError;
					}

					InvalidateMakefiles(TargetDescs[0]);
					return ECompilationResult.Succeeded;
				}

				// Used when BuildConfiguration.bUseUBTMakefiles is enabled.  If true, it means that our cached includes may not longer be
				// valid (or never were), and we need to recover by forcibly scanning included headers for all build prerequisites to make sure that our
				// cached set of includes is actually correct, before determining which files are outdated.
				bool bNeedsFullCPPIncludeRescan = false;

				if (BuildConfiguration.bUseUBTMakefiles)
				{
					// Only the modular editor and game targets will share build products.  Unfortunately, we can't determine at
					// at this point whether we're dealing with modular or monolithic game binaries, so we opt to always invalidate
					// cached includes if the target we're switching to is either a game target (has project file) or "UE4Editor".
					bool bMightHaveSharedBuildProducts =
						ProjectFile != null ||  // Is this a game? (has a .uproject file for the target)
						TargetDescs[0].Name.Equals("UE4Editor", StringComparison.InvariantCultureIgnoreCase); // Is the engine?
					if (bMightHaveSharedBuildProducts)
					{
						bool bIsBuildingSameTargetsAsLastTime = false;

						string TargetCollectionName = UBTMakefile.MakeTargetCollectionName(TargetDescs);

						string LastBuiltTargetsFileName = "LastBuiltTargets.txt";
						string LastBuiltTargetsFilePath = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "Build", LastBuiltTargetsFileName).FullName;
						if (File.Exists(LastBuiltTargetsFilePath) && Utils.ReadAllText(LastBuiltTargetsFilePath) == TargetCollectionName)
						{
							// @todo ubtmake: Because we're using separate files for hot reload vs. full compiles, it's actually possible that includes will
							// become out of date without us knowing if the developer ping-pongs between hot reloading target A and building target B normally.
							// To fix this we can not use a different file name for last built targets, but the downside is slower performance when
							// performing the first hot reload after compiling normally (forces full include dependency scan)
							bIsBuildingSameTargetsAsLastTime = true;
						}

						// Save out the name of the targets we're building
						if (!bIsBuildingSameTargetsAsLastTime)
						{
							Directory.CreateDirectory(Path.GetDirectoryName(LastBuiltTargetsFilePath));
							File.WriteAllText(LastBuiltTargetsFilePath, TargetCollectionName, Encoding.UTF8);

							// Can't use super fast include checking unless we're building the same set of targets as last time, because
							// we might not know about all of the C++ include dependencies for already-up-to-date shared build products
							// between the targets
							bNeedsFullCPPIncludeRescan = true;
							Log.TraceInformation("Performing full C++ include scan (building a new target)");
						}
					}
				}

				// True if we should gather module dependencies for building in this run.  If this is false, then we'll expect to be loading information from disk about
				// the target's modules before we'll be able to build anything.  One or both of IsGatheringBuild or IsAssemblingBuild must be true.
				bool bIsGatheringBuild = true;

				// True if we should go ahead and actually compile and link in this run.  If this is false, no actions will be executed and we'll stop after gathering
				// and saving information about dependencies.  One or both of IsGatheringBuild or IsAssemblingBuild must be true.
				bool bIsAssemblingBuild = true;

				// If we were asked to enable fast build iteration, we want the 'gather' phase to default to off (unless it is overridden below
				// using a command-line option.)
				if (BuildConfiguration.bUseUBTMakefiles)
				{
					bIsGatheringBuild = false;
				}

				foreach (string Argument in Arguments)
				{
					string LowercaseArg = Argument.ToLowerInvariant();
					if (LowercaseArg == "-gather")
					{
						bIsGatheringBuild = true;
					}
					else if (LowercaseArg == "-nogather")
					{
						bIsGatheringBuild = false;
					}
					else if (LowercaseArg == "-gatheronly")
					{
						bIsGatheringBuild = true;
						bIsAssemblingBuild = false;
					}
					else if (LowercaseArg == "-assemble")
					{
						bIsAssemblingBuild = true;
					}
					else if (LowercaseArg == "-noassemble")
					{
						bIsAssemblingBuild = false;
					}
					else if (LowercaseArg == "-assembleonly")
					{
						bIsGatheringBuild = false;
						bIsAssemblingBuild = true;
					}
				}

				if (!bIsGatheringBuild && !bIsAssemblingBuild)
				{
					throw new BuildException("UnrealBuildTool: At least one of either IsGatheringBuild or IsAssemblingBuild must be true.  Did you pass '-NoGather' with '-NoAssemble'?");
				}

				using (ISourceFileWorkingSet WorkingSet = SourceFileWorkingSet.Create(UnrealBuildTool.RootDirectory, DirectoryReference.FromFile(ProjectFile)))
				{
					UBTMakefile UBTMakefile = null;
					{
						FileReference UBTMakefilePath = UBTMakefile.GetUBTMakefilePath(TargetDescs);

						// Make sure the gather phase is executed if we're not actually building anything
						if (BuildConfiguration.bGenerateManifest || BuildConfiguration.bCleanProject || BuildConfiguration.bXGEExport || GeneratingActionGraph)
						{
							bIsGatheringBuild = true;
						}

						// Were we asked to run in 'assembler only' mode?  If so, let's check to see if that's even possible by seeing if
						// we have a valid UBTMakefile already saved to disk, ready for us to load.
						if (bIsAssemblingBuild && !bIsGatheringBuild)
						{
							// @todo ubtmake: Mildly terrified of BuildConfiguration/UEBuildConfiguration globals that were set during the Prepare phase but are not set now.  We may need to save/load all of these, otherwise
							//		we'll need to call SetupGlobalEnvironment on all of the targets (maybe other stuff, too.  See PreBuildStep())

							// Try to load the UBTMakefile.  It will only be loaded if it has valid content and is not determined to be out of date.    
							string ReasonNotLoaded;
							UBTMakefile = UBTMakefile.LoadUBTMakefile(UBTMakefilePath, ProjectFile, WorkingSet, out ReasonNotLoaded);

							if (UBTMakefile != null)
							{
								// Check if ini files are newer. Ini files contain build settings too.
								FileInfo UBTMakefileInfo = new FileInfo(UBTMakefilePath.FullName);
								foreach (TargetDescriptor Desc in TargetDescs)
								{
									DirectoryReference ProjectDirectory = DirectoryReference.FromFile(ProjectFile);
									foreach (ConfigHierarchyType IniType in (ConfigHierarchyType[])Enum.GetValues(typeof(ConfigHierarchyType)))
									{
										foreach (FileReference IniFilename in ConfigHierarchy.EnumerateConfigFileLocations(IniType, ProjectDirectory, Desc.Platform))
										{
											FileInfo IniFileInfo = new FileInfo(IniFilename.FullName);
											if (UBTMakefileInfo.LastWriteTime.CompareTo(IniFileInfo.LastWriteTime) < 0)
											{
												// Ini files are newer than UBTMakefile
												UBTMakefile = null;
												ReasonNotLoaded = "ini files are newer than UBTMakefile";
												break;
											}
										}

										if (UBTMakefile == null)
										{
											break;
										}
									}
									if (UBTMakefile == null)
									{
										break;
									}
								}
							}

							if (UBTMakefile == null)
							{
								// If the Makefile couldn't be loaded, then we're not going to be able to continue in "assembler only" mode.  We'll do both
								// a 'gather' and 'assemble' in the same run.  This will take a while longer, but subsequent runs will be fast!
								bIsGatheringBuild = true;

								FileItem.ClearCaches();

								Log.TraceInformation("Creating makefile for {0}{1} ({2})",
									TargetDescs[0].Name,
									TargetDescs.Count > 1 ? (" (and " + (TargetDescs.Count - 1).ToString() + " more)") : "",
									ReasonNotLoaded);
							}
						}
					}


					if (UBTMakefile != null && !bIsGatheringBuild && bIsAssemblingBuild)
					{
						// If we've loaded a makefile, then we can fill target information from this file!
						Targets = UBTMakefile.Targets;
					}
					else
					{
						DateTime TargetInitStartTime = DateTime.UtcNow;

						Targets = new List<UEBuildTarget>();
						foreach (TargetDescriptor TargetDesc in TargetDescs)
						{
							UEBuildTarget Target = UEBuildTarget.CreateTarget(TargetDesc, Arguments, bSkipRulesCompile, BuildConfiguration.SingleFileToCompile != null, BuildConfiguration.bUsePrecompiled);
							if ((Target == null) && (BuildConfiguration.bCleanProject))
							{
								continue;
							}
							Targets.Add(Target);
						}

						if (UnrealBuildTool.bPrintPerformanceInfo)
						{
							double TargetInitTime = (DateTime.UtcNow - TargetInitStartTime).TotalSeconds;
							Log.TraceInformation("Target init took " + TargetInitTime + "s");
						}
					}

					// Build action lists for all passed in targets.
					List<FileItem> OutputItemsForAllTargets = new List<FileItem>();
					Dictionary<string, FileItem[]> ModuleNameToOutputItems = new Dictionary<string, FileItem[]>(StringComparer.OrdinalIgnoreCase);
					Dictionary<string, List<UHTModuleInfo>> TargetNameToUObjectModules = new Dictionary<string, List<UHTModuleInfo>>(StringComparer.InvariantCultureIgnoreCase);
					HashSet<string> HotReloadModuleNamesForAllTargets = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
					foreach (UEBuildTarget Target in Targets)
					{
						// Create the header cache for this target
						FileReference DependencyCacheFile = DependencyCache.GetDependencyCachePathForTarget(ProjectFile, Target.Platform, Target.TargetName);
						bool bUseFlatCPPIncludeDependencyCache = BuildConfiguration.bUseUBTMakefiles && bIsAssemblingBuild;
						CPPHeaders Headers = new CPPHeaders(ProjectFile, DependencyCacheFile, bUseFlatCPPIncludeDependencyCache, BuildConfiguration.bUseUBTMakefiles, BuildConfiguration.bUseIncludeDependencyResolveCache, BuildConfiguration.bTestIncludeDependencyResolveCache);
						TargetToHeaders[Target] = Headers;

						// Make sure the appropriate executor is selected
						UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Target.Platform);
						BuildConfiguration.bAllowXGE &= BuildPlatform.CanUseXGE();
						BuildConfiguration.bAllowDistcc &= BuildPlatform.CanUseDistcc();
						BuildConfiguration.bAllowSNDBS &= BuildPlatform.CanUseSNDBS();

						// When in 'assembler only' mode, we'll load this cache later on a worker thread.  It takes a long time to load!
						if (!(!bIsGatheringBuild && bIsAssemblingBuild))
						{
							// Load the direct include dependency cache.
							Headers.IncludeDependencyCache = DependencyCache.Create(DependencyCache.GetDependencyCachePathForTarget(Target.ProjectFile, Target.Platform, Target.GetTargetName()));
						}

						// We don't need this dependency cache in 'gather only' mode
						if (BuildConfiguration.bUseUBTMakefiles &&
								!(bIsGatheringBuild && !bIsAssemblingBuild))
						{
							// Load the cache that contains the list of flattened resolved includes for resolved source files
							// @todo ubtmake: Ideally load this asynchronously at startup and only block when it is first needed and not finished loading
							FileReference CacheFile = FlatCPPIncludeDependencyCache.GetDependencyCachePathForTarget(Target.ProjectFile, Target.TargetName, Target.Platform, Target.Architecture);
							if (!FlatCPPIncludeDependencyCache.TryRead(CacheFile, out Headers.FlatCPPIncludeDependencyCache))
							{
								if (!bNeedsFullCPPIncludeRescan)
								{
									if (!BuildConfiguration.bXGEExport && !BuildConfiguration.bGenerateManifest && !BuildConfiguration.bCleanProject)
									{
										bNeedsFullCPPIncludeRescan = true;
										Log.TraceInformation("Performing full C++ include scan (no include cache file)");
									}
								}
								Headers.FlatCPPIncludeDependencyCache = new FlatCPPIncludeDependencyCache(CacheFile);
							}
						}

						if (bIsGatheringBuild)
						{
							List<FileItem> TargetOutputItems = new List<FileItem>();
							List<UHTModuleInfo> TargetUObjectModules = new List<UHTModuleInfo>();
							Dictionary<string, FileItem[]> TargetModuleNameToOutputItems = new Dictionary<string, FileItem[]>(StringComparer.OrdinalIgnoreCase);
							if (BuildConfiguration.bCleanProject)
							{
								BuildResult = Target.Clean(!BuildConfiguration.bDoNotBuildUHT);
							}
							else
							{
								BuildResult = Target.Build(BuildConfiguration, TargetToHeaders[Target], TargetOutputItems, TargetModuleNameToOutputItems, TargetUObjectModules, WorkingSet, ActionGraph, bIsAssemblingBuild);
							}
							if (BuildResult != ECompilationResult.Succeeded)
							{
								break;
							}

							// Export a JSON dump of this target
							if (BuildConfiguration.JsonExportFile != null)
							{
								Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(BuildConfiguration.JsonExportFile)));
								Target.ExportJson(BuildConfiguration.JsonExportFile);
							}

							OutputItemsForAllTargets.AddRange(TargetOutputItems);

							// Update mapping of the target name to the list of UObject modules in this target
							TargetNameToUObjectModules[Target.GetTargetName()] = TargetUObjectModules;

							// Merge the module name to output items map
							foreach(KeyValuePair<string, FileItem[]> Pair in TargetModuleNameToOutputItems)
							{
								FileItem[] Items;
								if(ModuleNameToOutputItems.TryGetValue(Pair.Key, out Items))
								{
									ModuleNameToOutputItems[Pair.Key] = Items.Concat(Pair.Value).ToArray();
								}
								else
								{
									ModuleNameToOutputItems[Pair.Key] = Pair.Value;
								}
							}

							// Get the game module names
							HotReloadModuleNamesForAllTargets.UnionWith(Target.GetHotReloadModuleNames());
						}
					}

					if (BuildResult == ECompilationResult.Succeeded && !BuildConfiguration.bSkipBuild &&
						(
							(BuildConfiguration.bXGEExport && BuildConfiguration.bGenerateManifest) ||
							(!GeneratingActionGraph && !BuildConfiguration.bGenerateManifest && !BuildConfiguration.bCleanProject)
						))
					{
						if (bIsGatheringBuild)
						{
							ActionGraph.FinalizeActionGraph();

							UBTMakefile = new UBTMakefile();
							UBTMakefile.AllActions = ActionGraph.AllActions;
							UBTMakefile.OutputItemsForAllTargets = OutputItemsForAllTargets;
							foreach (System.Collections.DictionaryEntry EnvironmentVariable in Environment.GetEnvironmentVariables())
							{
								UBTMakefile.EnvironmentVariables.Add(Tuple.Create((string)EnvironmentVariable.Key, (string)EnvironmentVariable.Value));
							}
							UBTMakefile.TargetNameToUObjectModules = TargetNameToUObjectModules;
							UBTMakefile.ModuleNameToOutputItems = ModuleNameToOutputItems;
							UBTMakefile.HotReloadModuleNamesForAllTargets = HotReloadModuleNamesForAllTargets;
							UBTMakefile.Targets = Targets;
							UBTMakefile.bUseAdaptiveUnityBuild = Targets.Any(x => x.Rules.bUseAdaptiveUnityBuild);
							UBTMakefile.SourceFileWorkingSet = Unity.SourceFileWorkingSet;
							UBTMakefile.CandidateSourceFilesForWorkingSet = Unity.CandidateSourceFilesForWorkingSet;

							if (BuildConfiguration.bUseUBTMakefiles)
							{
								// We've been told to prepare to build, so let's go ahead and save out our action graph so that we can use in a later invocation 
								// to assemble the build.  Even if we are configured to assemble the build in this same invocation, we want to save out the
								// Makefile so that it can be used on subsequent 'assemble only' runs, for the fastest possible iteration times
								// @todo ubtmake: Optimization: We could make 'gather + assemble' mode slightly faster by saving this while busy compiling (on our worker thread)
								UBTMakefile.SaveUBTMakefile(TargetDescs, UBTMakefile);
							}
						}

						HashSet<string> OnlyModuleNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
						foreach(string Argument in Arguments)
						{
							const string ModuleArgument = "-Module=";
							if(Argument.StartsWith(ModuleArgument, StringComparison.OrdinalIgnoreCase))
							{
								OnlyModuleNames.Add(Argument.Substring(ModuleArgument.Length));
							}
						}

						Dictionary<string, int> HotReloadModuleNameToSuffix = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);
						foreach(string Argument in Arguments)
						{
							const string ModuleWithSuffixArgument = "-ModuleWithSuffix=";
							if(Argument.StartsWith(ModuleWithSuffixArgument, StringComparison.OrdinalIgnoreCase))
							{
								string Value = Argument.Substring(ModuleWithSuffixArgument.Length);

								int SuffixIdx = Value.LastIndexOf(',');
								if(SuffixIdx == -1)
								{
									throw new BuildException("Missing suffix argument from -ModuleWithSuffix=Name,Suffix");
								}

								string ModuleName = Value.Substring(0, SuffixIdx);

								int Suffix;
								if(!Int32.TryParse(Value.Substring(SuffixIdx + 1), out Suffix))
								{
									throw new BuildException("Suffix for modules must be an integer");
								}

								HotReloadModuleNameToSuffix[ModuleName] = Suffix;
							}
						}

						HotReloadMode HotReloadMode = HotReloadMode.Disabled;
						if(Arguments.Any(x => x.Equals("-ForceHotReload", StringComparison.InvariantCultureIgnoreCase)))
						{
							HotReloadMode = HotReloadMode.FromIDE;
						}
						else if (TargetDescs.Count == 1 && !Arguments.Any(x => x.Equals("-NoHotReload", StringComparison.InvariantCultureIgnoreCase)))
						{
							if (BuildConfiguration.bAllowHotReloadFromIDE && HotReload.ShouldDoHotReloadFromIDE(BuildConfiguration, Arguments, TargetDescs[0]))
							{
								HotReloadMode = HotReloadMode.FromIDE;
							}
							else if (HotReloadModuleNameToSuffix.Count > 0 && TargetDescs[0].ForeignPlugin == null)
							{
								HotReloadMode = HotReloadMode.FromEditor;
							}

							if (HotReloadMode != HotReloadMode.Disabled && BuildConfiguration.bCleanProject)
							{
								throw new BuildException("Unable to clean target while hot-reloading. Close the editor and try again.");
							}
						}
						TargetDescriptor HotReloadTargetDesc = (HotReloadMode != HotReloadMode.Disabled) ? TargetDescs[0] : null;

						if (bIsAssemblingBuild)
						{
							// If we didn't build the graph in this session, then we'll need to load a cached one
							if (!bIsGatheringBuild && BuildResult.Succeeded())
							{
								ActionGraph.AllActions = UBTMakefile.AllActions;

								OutputItemsForAllTargets = UBTMakefile.OutputItemsForAllTargets;

								ModuleNameToOutputItems = UBTMakefile.ModuleNameToOutputItems;

								HotReloadModuleNamesForAllTargets = UBTMakefile.HotReloadModuleNamesForAllTargets;

								foreach (Tuple<string, string> EnvironmentVariable in UBTMakefile.EnvironmentVariables)
								{
									// @todo ubtmake: There may be some variables we do NOT want to clobber.
									Environment.SetEnvironmentVariable(EnvironmentVariable.Item1, EnvironmentVariable.Item2);
								}

								// Execute all the pre-build steps
								if (!BuildConfiguration.bXGEExport)
								{
									foreach (UEBuildTarget Target in Targets)
									{
										if (!Target.ExecuteCustomPreBuildSteps())
										{
											BuildResult = ECompilationResult.OtherCompilationError;
											break;
										}
									}
								}

								// If any of the targets need UHT to be run, we'll go ahead and do that now
								if (BuildResult.Succeeded())
								{
									foreach (UEBuildTarget Target in Targets)
									{
										List<UHTModuleInfo> TargetUObjectModules;
										if (UBTMakefile.TargetNameToUObjectModules.TryGetValue(Target.GetTargetName(), out TargetUObjectModules))
										{
											if (TargetUObjectModules.Count > 0)
											{
												// Execute the header tool
												FileReference ModuleInfoFileName = FileReference.Combine(Target.ProjectIntermediateDirectory, Target.GetTargetName() + ".uhtmanifest");
												ECompilationResult UHTResult = ExternalExecution.ExecuteHeaderToolIfNecessary(BuildConfiguration, Target.ProjectFile, Target.TargetName, Target.TargetType, Target.bHasProjectScriptPlugin,  UObjectModules: TargetUObjectModules, ModuleInfoFileName: ModuleInfoFileName, bIsGatheringBuild: bIsGatheringBuild, bIsAssemblingBuild: bIsAssemblingBuild);
												if(!UHTResult.Succeeded())
												{
													Log.TraceInformation("UnrealHeaderTool failed for target '" + Target.GetTargetName() + "' (platform: " + Target.Platform.ToString() + ", module info: " + ModuleInfoFileName + ").");
													BuildResult = UHTResult;
													break;
												}
											}
										}
									}
								}
							}

							if (BuildResult.Succeeded())
							{
								// Get the output items we're trying to build right now
								List<FileItem> OutputItems = OutputItemsForAllTargets;

								// Filter them if we have any specific modules specified
								if (OnlyModuleNames.Count > 0)
								{
									OutputItems = new List<FileItem>();
									foreach(string OnlyModuleName in OnlyModuleNames)
									{
										FileItem[] OutputItemsForModule;
										if(!ModuleNameToOutputItems.TryGetValue(OnlyModuleName, out OutputItemsForModule))
										{
											throw new BuildException("Unable to find output items for module '{0}'", OnlyModuleName);
										}
										OutputItems.AddRange(OutputItemsForModule);
									}
								}

								// For now simply treat all object files as the root target.
								Action[] PrerequisiteActions;
								{
									HashSet<Action> PrerequisiteActionsSet = new HashSet<Action>();
									foreach (FileItem OutputItem in OutputItems)
									{
										ActionGraph.GatherPrerequisiteActions(OutputItem, ref PrerequisiteActionsSet);
									}
									PrerequisiteActions = PrerequisiteActionsSet.ToArray();
								}

								// Apply the previous hot reload state
								HotReloadState HotReloadState = null;
								if(HotReloadMode == HotReloadMode.Disabled)
								{
									// Delete any previous state files; they are no longer valid
									foreach(TargetDescriptor TargetDesc in TargetDescs)
									{
										FileReference HotReloadStateFile = HotReloadState.GetLocation(TargetDesc.ProjectFile, TargetDesc.Name, TargetDesc.Platform, TargetDesc.Configuration, TargetDesc.Architecture);
										HotReload.DeleteTemporaryFiles(HotReloadStateFile);
									}
								}
								else
								{
									// Read the previous state file and apply it to the action graph
									FileReference HotReloadStateFile = HotReloadState.GetLocation(TargetDescs[0].ProjectFile, TargetDescs[0].Name, TargetDescs[0].Platform, TargetDescs[0].Configuration, TargetDescs[0].Architecture);
									if(FileReference.Exists(HotReloadStateFile))
									{
										HotReloadState = HotReloadState.Load(HotReloadStateFile);
									}
									else
									{
										HotReloadState = new HotReloadState();
									}

									// Update the action graph to produce these new files
									HotReload.PatchActionGraph(PrerequisiteActions, HotReloadState.OriginalFileToHotReloadFile);

									// Update the module to output file mapping
									foreach(string HotReloadModuleName in HotReloadModuleNamesForAllTargets)
									{
										FileItem[] ModuleOutputItems = ModuleNameToOutputItems[HotReloadModuleName];
										for(int Idx = 0; Idx < ModuleOutputItems.Length; Idx++)
										{
											FileReference NewLocation;
											if(HotReloadState.OriginalFileToHotReloadFile.TryGetValue(ModuleOutputItems[Idx].Location, out NewLocation))
											{
												ModuleOutputItems[Idx] = FileItem.GetItemByFileReference(NewLocation);
											}
										}
									}

									// If we want a specific suffix on any modules, apply that now. We'll track the outputs later, but the suffix has to be forced  (and is always out of date if it doesn't exist).
									if(HotReloadModuleNameToSuffix.Count > 0)
									{
										Dictionary<FileReference, FileReference> OldLocationToNewLocation = new Dictionary<FileReference, FileReference>();
										foreach(string HotReloadModuleName in HotReloadModuleNamesForAllTargets)
										{
											int ModuleSuffix;
											if(HotReloadModuleNameToSuffix.TryGetValue(HotReloadModuleName, out ModuleSuffix))
											{
												FileItem[] ModuleOutputItems = ModuleNameToOutputItems[HotReloadModuleName];
												foreach(FileItem ModuleOutputItem in ModuleOutputItems)
												{
													FileReference OldLocation = ModuleOutputItem.Location;
													FileReference NewLocation = HotReload.ReplaceSuffix(OldLocation, ModuleSuffix);
													OldLocationToNewLocation[OldLocation] = NewLocation;
												}
											}
										}
										HotReload.PatchActionGraph(PrerequisiteActions, OldLocationToNewLocation);
									}
								}

								// Plan the actions to execute for the build.
								Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap;
								HashSet<Action> ActionsToExecute = ActionGraph.GetActionsToExecute(BuildConfiguration, PrerequisiteActions, Targets, TargetToHeaders, bIsAssemblingBuild, bNeedsFullCPPIncludeRescan, out TargetToOutdatedPrerequisitesMap);

								// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
								// the same for every invocation on that makefile, but we need a new suffix each time.
								if (HotReloadMode != HotReloadMode.Disabled)
								{
									// For all the hot-reloadable modules that may need a unique suffix appended, build a mapping from output item to all the output items in that module. We can't 
									// apply a suffix to one without applying a suffix to all of them.
									Dictionary<FileItem, FileItem[]> HotReloadItemToDependentItems = new Dictionary<FileItem, FileItem[]>();
									foreach(string HotReloadModuleName in HotReloadModuleNamesForAllTargets)
									{
										int ModuleSuffix;
										if(!HotReloadModuleNameToSuffix.TryGetValue(HotReloadModuleName, out ModuleSuffix) || ModuleSuffix == -1)
										{
											FileItem[] ModuleOutputItems;
											if(ModuleNameToOutputItems.TryGetValue(HotReloadModuleName, out ModuleOutputItems))
											{
												foreach(FileItem ModuleOutputItem in ModuleOutputItems)
												{
													HotReloadItemToDependentItems[ModuleOutputItem] = ModuleOutputItems;
												}
											}
										} 
									}

									// Expand the list of actions to execute to include everything that references any files with a new suffix. Unlike a regular build, we can't ignore
									// dependencies on import libraries under the assumption that a header would change if the API changes, because the dependency will be on a different DLL.
									HashSet<FileItem> FilesRequiringSuffix = new HashSet<FileItem>(ActionsToExecute.SelectMany(x => x.ProducedItems).Where(x => HotReloadItemToDependentItems.ContainsKey(x)));
									for(int LastNumFilesWithNewSuffix = 0; FilesRequiringSuffix.Count > LastNumFilesWithNewSuffix;)
									{
										LastNumFilesWithNewSuffix = FilesRequiringSuffix.Count;
										foreach(Action PrerequisiteAction in PrerequisiteActions)
										{
											if(!ActionsToExecute.Contains(PrerequisiteAction))
											{
												foreach(FileItem ProducedItem in PrerequisiteAction.ProducedItems)
												{
													FileItem[] DependentItems;
													if(HotReloadItemToDependentItems.TryGetValue(ProducedItem, out DependentItems))
													{
														ActionsToExecute.Add(PrerequisiteAction);
														FilesRequiringSuffix.UnionWith(DependentItems);
													}
												}
											}
										}
									}

									// Build a list of file mappings
									Dictionary<FileReference, FileReference> OldLocationToNewLocation = new Dictionary<FileReference, FileReference>();
									foreach(FileItem FileRequiringSuffix in FilesRequiringSuffix)
									{
										FileReference OldLocation = FileRequiringSuffix.Location;
										FileReference NewLocation = HotReload.ReplaceSuffix(OldLocation, HotReloadState.NextSuffix);
										OldLocationToNewLocation[OldLocation] = NewLocation;
									}

									// Update the action graph with these new paths
									HotReload.PatchActionGraph(PrerequisiteActions, OldLocationToNewLocation);

									// Get a new list of actions to execute now that the graph has been modified
									ActionsToExecute = ActionGraph.GetActionsToExecute(BuildConfiguration, PrerequisiteActions, Targets, TargetToHeaders, bIsAssemblingBuild, bNeedsFullCPPIncludeRescan, out TargetToOutdatedPrerequisitesMap);

									// Build a mapping of all file items to their original
									Dictionary<FileReference, FileReference> HotReloadFileToOriginalFile = new Dictionary<FileReference, FileReference>();
									foreach(KeyValuePair<FileReference, FileReference> Pair in HotReloadState.OriginalFileToHotReloadFile)
									{
										HotReloadFileToOriginalFile[Pair.Value] = Pair.Key;
									}
									foreach(KeyValuePair<FileReference, FileReference> Pair in OldLocationToNewLocation)
									{
										FileReference OriginalLocation;
										if(!HotReloadFileToOriginalFile.TryGetValue(Pair.Key, out OriginalLocation))
										{
											OriginalLocation = Pair.Key;
										}
										HotReloadFileToOriginalFile[Pair.Value] = OriginalLocation;
									}

									// Now filter out all the hot reload files and update the state
									foreach(Action Action in ActionsToExecute)
									{
										foreach(FileItem ProducedItem in Action.ProducedItems)
										{
											FileReference OriginalLocation;
											if(HotReloadFileToOriginalFile.TryGetValue(ProducedItem.Location, out OriginalLocation))
											{
												HotReloadState.OriginalFileToHotReloadFile[OriginalLocation] = ProducedItem.Location;
												HotReloadState.TemporaryFiles.Add(ProducedItem.Location);
											}
										}
									}

									// Increment the suffix for the next iteration
									if(ActionsToExecute.Count > 0)
									{
										HotReloadState.NextSuffix++;
									}

									// Save the new state
									FileReference HotReloadStateFile = HotReloadState.GetLocation(TargetDescs[0].ProjectFile, TargetDescs[0].Name, TargetDescs[0].Platform, TargetDescs[0].Configuration, TargetDescs[0].Architecture);
									HotReloadState.Save(HotReloadStateFile);
								}

								// Display some stats to the user.
								Log.TraceVerbose(
										"{0} actions, {1} outdated and requested actions",
										ActionGraph.AllActions.Count,
										ActionsToExecute.Count
										);

								// Cache indirect includes for all outdated C++ files.  We kick this off as a background thread so that it can
								// perform the scan while we're compiling.  It usually only takes up to a few seconds, but we don't want to hurt
								// our best case UBT iteration times for this task which can easily be performed asynchronously
								if (BuildConfiguration.bUseUBTMakefiles && TargetToOutdatedPrerequisitesMap.Count > 0)
								{
									CppIncludeThread = new CppIncludeBackgroundThread(TargetToOutdatedPrerequisitesMap, TargetToHeaders);
								}

								// Execute the actions.
								DateTime ExecutorStartTime = DateTime.UtcNow;
								if (BuildConfiguration.bXGEExport)
								{
									XGE.ExportActions(ActionsToExecute.ToList());
									bSuccess = true;
								}
								else
								{
									bSuccess = ActionGraph.ExecuteActions(BuildConfiguration, ActionsToExecute.ToList(), out ExecutorName);
								}
								TotalExecutorTime += (DateTime.UtcNow - ExecutorStartTime).TotalSeconds;

								// if the build succeeded, write the receipts and do any needed syncing
								if (!bSuccess)
								{
									BuildResult = ECompilationResult.OtherCompilationError;
								}
							}

							// Run the deployment steps
							if (BuildResult.Succeeded()
								&& String.IsNullOrEmpty(BuildConfiguration.SingleFileToCompile)
								&& !BuildConfiguration.bGenerateManifest
								&& !BuildConfiguration.bCleanProject
							&& !BuildConfiguration.bXGEExport)
							{
								foreach (UEBuildTarget Target in Targets)
								{
									if (Target.bDeployAfterCompile && HotReloadModuleNameToSuffix.Count == 0)
									{
										Log.WriteLine(LogEventType.Console, "Deploying {0} {1} {2}...", Target.TargetName, Target.Platform, Target.Configuration);
										TargetReceipt Receipt = TargetReceipt.Read(Target.ReceiptFileName);
										UEBuildPlatform.GetBuildPlatform(Target.Platform).Deploy(Receipt);
									}
								}
							}
						}
					}
				}
			}
			catch (Exception Ex)
			{
				Log.WriteException(Ex, String.IsNullOrEmpty(BuildConfiguration.LogFileName) ? null : BuildConfiguration.LogFileName);
				BuildResult = ECompilationResult.OtherCompilationError;
			}

			// Wait until our CPPIncludes dependency scanner thread has finished
			if (CppIncludeThread != null)
			{
				CppIncludeThread.Join();
			}

			// Save the include dependency cache.
			foreach (CPPHeaders Headers in TargetToHeaders.Values)
			{
				// NOTE: It's very important that we save the include cache, even if a build exception was thrown (compile error, etc), because we need to make sure that
				//    any C++ include dependencies that we computed for out of date source files are saved.  Remember, the build may fail *after* some build products
				//    are successfully built.  If we didn't save our dependency cache after build failures, source files for those build products that were successfully
				//    built before the failure would not be considered out of date on the next run, so this is our only chance to cache C++ includes for those files!

				if (Headers.IncludeDependencyCache != null)
				{
					Headers.IncludeDependencyCache.Save();
				}

				if (Headers.FlatCPPIncludeDependencyCache != null)
				{
					Headers.FlatCPPIncludeDependencyCache.Save();
				}
			}

			// Figure out how long we took to execute.
			if (ExecutorName != "Unknown")
			{
				double BuildDuration = (DateTime.UtcNow - RunUBTInitStartTime).TotalSeconds;
				Log.TraceInformation("Total build time: {0:0.00} seconds ({1} executor: {2:0.00} seconds)", BuildDuration, ExecutorName, TotalExecutorTime);
			}

			return BuildResult;
		}

		/// <summary>
		/// Invalidates makefiles for given target.
		/// </summary>
		/// <param name="Target">Target</param>
		private static void InvalidateMakefiles(TargetDescriptor Target)
		{
			string[] MakefileNames = new string[] { "HotReloadMakefile.ubt", "Makefile.ubt" };
			DirectoryReference BaseDir = UBTMakefile.GetUBTMakefileDirectoryPathForSingleTarget(Target);

			foreach (string MakefileName in MakefileNames)
			{
				FileReference MakefileRef = FileReference.Combine(BaseDir, MakefileName);
				if (FileReference.Exists(MakefileRef))
				{
					FileReference.Delete(MakefileRef);
				}
			}
		}

		/// <summary>
		/// Parses the passed in command line for build configuration overrides.
		/// </summary>
		/// <param name="Arguments">List of arguments to parse</param>
		/// <returns>List of build target settings</returns>
		private static List<string[]> ParseCommandLineFlags(string[] Arguments)
		{
			List<string[]> TargetSettings = new List<string[]>();
			int ArgumentIndex = 0;

			if (Utils.ParseCommandLineFlag(Arguments, "-targets", out ArgumentIndex))
			{
				if (ArgumentIndex + 1 >= Arguments.Length)
				{
					throw new BuildException("Expected filename after -targets argument, but found nothing.");
				}
				// Parse lines from the referenced file into target settings.
				string[] Lines = File.ReadAllLines(Arguments[ArgumentIndex + 1]);
				foreach (string Line in Lines)
				{
					if (Line != "" && Line[0] != ';')
					{
						TargetSettings.Add(Line.Split(' '));
					}
				}
			}
			// Simply use full command line arguments as target setting if not otherwise overridden.
			else
			{
				TargetSettings.Add(Arguments);
			}

			return TargetSettings;
		}

		/// <summary>
		/// Helper class to update the C++ dependency cache on a background thread. Captures exceptions and re-throws on the main thread when joined.
		/// </summary>
		class CppIncludeBackgroundThread
		{
			Thread BackgroundThread;
			Exception CaughtException;

			public CppIncludeBackgroundThread(Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap, Dictionary<UEBuildTarget, CPPHeaders> TargetToHeaders)
			{
				BackgroundThread = new Thread(() => Run(TargetToOutdatedPrerequisitesMap, TargetToHeaders));
				BackgroundThread.Start();
			}

			public void Join()
			{
				BackgroundThread.Join();
				if (CaughtException != null)
				{
					throw CaughtException;
				}
			}

			private void Run(Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap, Dictionary<UEBuildTarget, CPPHeaders> TargetToHeaders)
			{
				try
				{
					// @todo ubtmake: This thread will access data structures that are also used on the main UBT thread, but during this time UBT
					// is only invoking the build executor, so should not be touching this stuff.  However, we need to at some guards to make sure.
					foreach (KeyValuePair<UEBuildTarget, List<FileItem>> TargetAndOutdatedPrerequisites in TargetToOutdatedPrerequisitesMap)
					{
						UEBuildTarget Target = TargetAndOutdatedPrerequisites.Key;
						List<FileItem> OutdatedPrerequisites = TargetAndOutdatedPrerequisites.Value;
						CPPHeaders Headers = TargetToHeaders[Target];

						foreach (FileItem PrerequisiteItem in OutdatedPrerequisites)
						{
							// Invoke our deep include scanner to figure out whether any of the files included by this source file have
							// changed since the build product was built
							Headers.FindAndCacheAllIncludedFiles(PrerequisiteItem, PrerequisiteItem.CachedIncludePaths, bOnlyCachedDependencies: false);
						}
					}
				}
				catch (Exception Ex)
				{
					CaughtException = Ex;
				}
			}
		}
	}
}
