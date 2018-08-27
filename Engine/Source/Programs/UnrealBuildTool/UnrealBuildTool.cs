// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Reflection;
using System.Linq;
using System.Runtime.Serialization.Formatters.Binary;
using System.Runtime.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	enum EHotReload
	{
		Disabled,
		FromIDE,
		FromEditor
	}

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
		/// Cached array of all the supported target platforms
		/// </summary>
		static public UnrealTargetPlatform[] AllPlatforms = (UnrealTargetPlatform[])Enum.GetValues(typeof(UnrealTargetPlatform));

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
		/// Sets the global flag indicating whether the engine is installed
		/// </summary>
		/// <param name="bInIsEngineInstalled">Whether the engine is installed or not</param>
		public static void SetIsEngineInstalled(bool bInIsEngineInstalled)
		{
			bIsEngineInstalled = bInIsEngineInstalled;
		}

		/// <summary>
		/// Returns true if UnrealBuildTool is running using installed Engine components
		/// </summary>
		/// <returns>True if running using installed Engine components</returns>
		static public bool IsEngineInstalled()
		{
			if (!bIsEngineInstalled.HasValue)
			{
				throw new BuildException("IsEngineInstalled() called before being initialized.");
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
				bIsProjectInstalled = FileReference.Exists(FileReference.Combine(RootDirectory, "Engine", "Build", "InstalledProjectBuild.txt"));
			}
			return bIsProjectInstalled.Value;
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

		public static void RegisterAllUBTClasses(SDKOutputLevel OutputLevel, bool bValidatingPlatforms)
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
							TempInst.TryRegisterBuildPlatforms(OutputLevel, bValidatingPlatforms);
						}
					}
				}
				// register all the other classes
				foreach (Type CheckType in AllTypes)
				{
					if (CheckType.IsClass && !CheckType.IsAbstract)
					{
						if (CheckType.IsSubclassOf(typeof(UEPlatformProjectGenerator)))
						{
							ProjectGeneratorList.Add(CheckType);
						}
					}
				}

				// We need to process the ProjectGenerators AFTER all BuildPlatforms have been 
				// registered as they use the BuildPlatform to verify they are legitimate.
				foreach (Type ProjType in ProjectGeneratorList)
				{
					Log.TraceVerbose("    Registering project generator: {0}", ProjType.ToString());
					UEPlatformProjectGenerator TempInst = (UEPlatformProjectGenerator)(UBTAssembly.CreateInstance(ProjType.FullName, true));
					TempInst.RegisterPlatformProjectGenerator();
				}
			}
		}

		private static bool ParseRocketCommandlineArg(string InArg, ref string OutGameName, ref FileReference ProjectFile)
		{
			string LowercaseArg = InArg.ToLowerInvariant();

			string ProjectArg = null;
			if (LowercaseArg.StartsWith("-project="))
			{
				ProjectArg = InArg.Substring(9).Trim(new Char[] { ' ', '"', '\'' });
			}
			else if (LowercaseArg.EndsWith(".uproject"))
			{
				ProjectArg = InArg;
			}
			else if (LowercaseArg.StartsWith("-remoteini="))
			{
				RemoteIniPath = InArg.Substring(11);
			}
			else
			{
				return false;
			}

			if (ProjectArg != null && ProjectArg.Length > 0)
			{
				string ProjectPath = Path.GetDirectoryName(ProjectArg);
				OutGameName = Path.GetFileNameWithoutExtension(ProjectArg);

				if (Path.IsPathRooted(ProjectPath) == false)
				{
					if (Directory.Exists(ProjectPath) == false)
					{
						// If it is *not* rooted, then we potentially received a path that is 
						// relative to Engine/Binaries/[PLATFORM] rather than Engine/Source
						if (ProjectPath.StartsWith("../") || ProjectPath.StartsWith("..\\"))
						{
							string AlternativeProjectpath = ProjectPath.Substring(3);
							if (Directory.Exists(AlternativeProjectpath))
							{
								ProjectPath = AlternativeProjectpath;
								ProjectArg = ProjectArg.Substring(3);
								Debug.Assert(ProjectArg.Length > 0);
							}
						}
					}
				}

				ProjectFile = new FileReference(ProjectArg);
			}

			return true;
		}

		/// <summary>
		/// UBT startup order is fairly fragile, and relies on globals that may or may not be safe to use yet.
		/// This function is for super early startup stuff that should not access Configuration classes (anything loaded by XmlConfg).
		/// This should be very minimal startup code.
		/// </summary>
		/// <param name="Arguments">Cmdline arguments</param>
		private static int GuardedMain(string[] Arguments)
		{
			DateTime StartTime = DateTime.UtcNow;
			ECompilationResult Result = ECompilationResult.Succeeded;

			// Parse the log level argument
			LogEventType LogLevel = LogEventType.Log;
			if (Arguments.Any(x => x.Equals("-Verbose", StringComparison.InvariantCultureIgnoreCase)))
			{
				LogLevel = LogEventType.Verbose;
			}
			else if (Arguments.Any(x => x.Equals("-VeryVerbose", StringComparison.InvariantCultureIgnoreCase)))
			{
				LogLevel = LogEventType.VeryVerbose;
			}

			// Initialize the log system, buffering the output until we can create the log file
			StartupTraceListener StartupListener = new StartupTraceListener();
			bool bLogProgramNameWithSeverity = Arguments.Any(x => x.Equals("-FromMsBuild", StringComparison.InvariantCultureIgnoreCase));
			Log.InitLogging(
				bLogTimestamps: Arguments.Any(x => x.Equals("-Timestamps", StringComparison.InvariantCultureIgnoreCase)),
				InLogLevel: LogLevel,
				bLogSeverity: true,
				bLogProgramNameWithSeverity: bLogProgramNameWithSeverity,
				bLogSources: true,
				bLogSourcesToConsole: false,
				bColorConsoleOutput: true,
				TraceListeners: new TraceListener[]
				{
					StartupListener
				}
			);

			// Write the command line
			Log.TraceLog("Command line: {0}", Environment.CommandLine);

			// ensure we can resolve any external assemblies that are not in the same folder as our assembly.
			AssemblyUtils.InstallAssemblyResolver(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()));

			// Grab the environment.
			InitialEnvironment = Environment.GetEnvironmentVariables();
			if (InitialEnvironment.Count < 1)
			{
				throw new BuildException("Environment could not be read");
			}

			// @todo: Ideally we never need to Mutex unless we are invoked with the same target project,
			// in the same branch/path!  This would allow two clientspecs to build at the same time (even though we need
			// to make sure that XGE is only used once at a time)
			bool bUseMutex = true;
			{
				int NoMutexArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-NoMutex", out NoMutexArgumentIndex))
				{
					bUseMutex = false;
				}
			}
			bool bWaitMutex = false;
			{
				int WaitMutexArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-WaitMutex", out WaitMutexArgumentIndex))
				{
					bWaitMutex = true;
				}
			}

			bool bAutoSDKOnly = false;
			{
				int AutoSDKOnlyArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-autosdkonly", out AutoSDKOnlyArgumentIndex))
				{
					bAutoSDKOnly = true;
				}
			}
			bool bValidatePlatforms = false;
			{
				int ValidatePlatformsArgumentIndex;
				if (Utils.ParseCommandLineFlag(Arguments, "-validateplatform", out ValidatePlatformsArgumentIndex))
				{
					bValidatePlatforms = true;
				}
			}


			// Don't allow simultaneous execution of Unreal Built Tool. Multi-selection in the UI e.g. causes this and you want serial
			// execution in this case to avoid contention issues with shared produced items.
			bool bCreatedMutex = false;
			{
				Mutex SingleInstanceMutex = null;
				if (bUseMutex)
				{
					int LocationHash = Assembly.GetEntryAssembly().GetOriginalLocation().GetHashCode();

					String MutexName;
					if (bAutoSDKOnly || bValidatePlatforms)
					{
						// this mutex has to be truly global because AutoSDK installs may change global state (like a console copying files into VS directories,
						// or installing Target Management services
						MutexName = "Global\\UnrealBuildTool_Mutex_AutoSDKS";
					}
					else
					{
						MutexName = "Global\\UnrealBuildTool_Mutex_" + LocationHash.ToString();
					}
					SingleInstanceMutex = new Mutex(true, MutexName, out bCreatedMutex);
					if (!bCreatedMutex)
					{
						if (bWaitMutex)
						{
							try
							{
								SingleInstanceMutex.WaitOne();
							}
							catch (AbandonedMutexException)
							{
							}
						}
						else if (bAutoSDKOnly || bValidatePlatforms)
						{
							throw new BuildException("A conflicting instance of UnrealBuildTool is already running. Either -autosdkonly or -validateplatform was passed, which allows only a single instance to be run globally. Therefore, the conflicting instance may be in another location or the current location: {0}. A process manager may be used to determine the conflicting process and what tool may have launched it.", Assembly.GetEntryAssembly().GetOriginalLocation());
						}
						else
						{
							throw new BuildException("A conflicting instance of UnrealBuildTool is already running at this location: {0}. A process manager may be used to determine the conflicting process and what tool may have launched it.", Assembly.GetEntryAssembly().GetOriginalLocation());
						}
					}
				}

				try
				{
					// Change the working directory to be the Engine/Source folder. We are likely running from Engine/Binaries/DotNET
					// This is critical to be done early so any code that relies on the current directory being Engine/Source will work.
					// UEBuildConfiguration.PostReset is susceptible to this, so we must do this before configs are loaded.
					string EngineSourceDirectory = Path.Combine(Path.GetDirectoryName(Assembly.GetEntryAssembly().GetOriginalLocation()), "..", "..", "..", "Engine", "Source");

					//@todo.Rocket: This is a workaround for recompiling game code in editor
					// The working directory when launching is *not* what we would expect
					if (Directory.Exists(EngineSourceDirectory) == false)
					{
						// We are assuming UBT always runs from <>/Engine/Binaries/DotNET/...
						EngineSourceDirectory = Assembly.GetExecutingAssembly().GetOriginalLocation();
						EngineSourceDirectory = EngineSourceDirectory.Replace("\\", "/");
						Int32 EngineIdx = EngineSourceDirectory.IndexOf("/Engine/Binaries/DotNET/", StringComparison.InvariantCultureIgnoreCase);
						if (EngineIdx > 0)
						{
							EngineSourceDirectory = Path.Combine(EngineSourceDirectory.Substring(0, EngineIdx), "Engine", "Source");
						}
					}
					if (Directory.Exists(EngineSourceDirectory)) // only set the directory if it exists, this should only happen if we are launching the editor from an artist sync
					{
						Directory.SetCurrentDirectory(EngineSourceDirectory);
					}

					// Figure out if the engine is installed or not
					foreach (string Argument in Arguments)
					{
						string LowercaseArg = Argument.ToLowerInvariant();
						if (LowercaseArg == "-installed" || LowercaseArg == "-installedengine")
						{
							bIsEngineInstalled = true;
						}
						else if (LowercaseArg == "-notinstalledengine")
						{
							bIsEngineInstalled = false;
						}
					}
					if (!bIsEngineInstalled.HasValue)
					{
						bIsEngineInstalled = FileReference.Exists(FileReference.Combine(RootDirectory, "Engine", "Build", "InstalledBuild.txt"));
					}

					// Read the XML configuration files
					bool bForceXmlConfigCache = Arguments.Any(x => x.Equals("-ForceXmlConfigCache", StringComparison.InvariantCultureIgnoreCase));
					if (!XmlConfig.ReadConfigFiles(bForceXmlConfigCache))
					{
						return 1;
					}

					// Create the build configuration object, and read the settings
					BuildConfiguration BuildConfiguration = new BuildConfiguration();
					XmlConfig.ApplyTo(BuildConfiguration);
					CommandLine.ParseArguments(Arguments, BuildConfiguration);

					// Copy some of the static settings that are being deprecated from BuildConfiguration
					bPrintDebugInfo = BuildConfiguration.bPrintDebugInfo || LogLevel == LogEventType.Verbose || LogLevel == LogEventType.VeryVerbose;
					bPrintPerformanceInfo = BuildConfiguration.bPrintPerformanceInfo;

					// Don't run junk deleter if we're just setting up autosdks
					if (bAutoSDKOnly || bValidatePlatforms)
					{
						BuildConfiguration.bIgnoreJunk = true;
					}

					// Then let the command lines override any configs necessary.
					foreach (string Argument in Arguments)
					{
						string LowercaseArg = Argument.ToLowerInvariant();
						if (LowercaseArg.StartsWith("-log="))
						{
							BuildConfiguration.LogFileName = Argument.Substring("-log=".Length);
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
						else if (LowercaseArg.StartsWith("-buildconfigurationdoc="))
						{
							XmlConfig.WriteDocumentation(new FileReference(Argument.Substring("-buildconfigurationdoc=".Length)));
							return 0;
						}
						else if (LowercaseArg.StartsWith("-modulerulesdoc="))
						{
							RulesDocumentation.WriteDocumentation(typeof(ModuleRules), new FileReference(Argument.Substring("-modulerulesdoc=".Length)));
							return 0;
						}
						else if (LowercaseArg.StartsWith("-targetrulesdoc="))
						{
							RulesDocumentation.WriteDocumentation(typeof(TargetRules), new FileReference(Argument.Substring("-targetrulesdoc=".Length)));
							return 0;
						}
					}

					// Create the log file, and flush the startup listener to it
					if (!String.IsNullOrEmpty(BuildConfiguration.LogFileName))
					{
						FileReference LogLocation = new FileReference(BuildConfiguration.LogFileName);
						try
						{
							DirectoryReference.CreateDirectory(LogLocation.Directory);
							TextWriterTraceListener LogTraceListener = new TextWriterTraceListener(new StreamWriter(LogLocation.FullName), "LogTraceListener");
							StartupListener.CopyTo(LogTraceListener);
							Trace.Listeners.Add(LogTraceListener);
						}
						catch (Exception Ex)
						{
							throw new BuildException(Ex, "Unable to open log file for writing ({0})", LogLocation);
						}
					}
					Trace.Listeners.Remove(StartupListener);

					// Parse rocket-specific arguments.
					FileReference ProjectFile = null;
					foreach (string Arg in Arguments)
					{
						string TempGameName = null;
						if (ParseRocketCommandlineArg(Arg, ref TempGameName, ref ProjectFile) == true)
						{
							// This is to allow relative paths for the project file
							Log.TraceVerbose("UBT Running for Rocket: " + ProjectFile);
							break;
						}
					}

					// Read the project file from the installed project text file
					if (ProjectFile == null)
					{
						FileReference InstalledProjectFile = FileReference.Combine(RootDirectory, "Engine", "Build", "InstalledProjectBuild.txt");
						if (FileReference.Exists(InstalledProjectFile))
						{
							ProjectFile = FileReference.Combine(UnrealBuildTool.RootDirectory, File.ReadAllText(InstalledProjectFile.FullName).Trim());
						}
					}

					// Build the list of game projects that we know about. When building from the editor (for hot-reload) or for projects from installed builds, we require the 
					// project file to be passed in. Otherwise we scan for projects in directories named in UE4Games.uprojectdirs.
					if (ProjectFile != null)
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

					Log.TraceVerbose("UnrealBuildTool (DEBUG OUTPUT MODE)");
					Log.TraceVerbose("Command-line: {0}", String.Join(" ", Arguments));

					string GameName = null;
					bool bSpecificModulesOnly = false;

					// We need to be able to identify if one of the arguments is the platform...
					UnrealTargetPlatform CheckPlatform = UnrealTargetPlatform.Unknown;
					foreach (string Argument in Arguments)
					{
						UnrealTargetPlatform ParsedPlatform = UEBuildPlatform.ConvertStringToPlatform(Argument);
						if (ParsedPlatform != UnrealTargetPlatform.Unknown)
						{
							CheckPlatform = ParsedPlatform;
						}
					}

					// @todo ubtmake: remove this when building with RPCUtility works
					// @todo tvos merge: Check the change to this line, not clear why. Is TVOS needed here?
					if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac && (CheckPlatform == UnrealTargetPlatform.Mac || CheckPlatform == UnrealTargetPlatform.IOS || CheckPlatform == UnrealTargetPlatform.TVOS))
					{
						BuildConfiguration.bUseUBTMakefiles = false;
					}

					bool bGenerateProjectFiles = false;
					WindowsCompiler OverrideWindowsCompiler = WindowsCompiler.Default;
					List<ProjectFileFormat> ProjectFileFormats = new List<ProjectFileFormat>();
					foreach (string Arg in Arguments)
					{
						string LowercaseArg = Arg.ToLowerInvariant();
						if (ProjectFile == null && ParseRocketCommandlineArg(Arg, ref GameName, ref ProjectFile))
						{
							// Already handled at startup. Calling now just to properly set the game name
							continue;
						}
						else if (LowercaseArg == "-projectfiles")
						{
							bGenerateProjectFiles = true;
						}
						else if (LowercaseArg.StartsWith("-projectfileformat="))
						{
							ProjectFileFormats.AddRange(ProjectFileGeneratorSettings.ParseFormatList(Arg.Substring("-projectfileformat=".Length)));
							bGenerateProjectFiles = true;
						}
						else if (LowercaseArg == "-2012unsupported")
						{
							// May be for compiling; don't set bGenerateProjectFiles by default 
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudio2012);
						}
						else if (LowercaseArg == "-2013unsupported")
						{
							// May be for compiling; don't set bGenerateProjectFiles by default 
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudio2013);
						}
						else if (LowercaseArg == "-2015")
						{
							// May be for compiling; don't set bGenerateProjectFiles by default, but do override the compiler if it is.
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudio2015);
							OverrideWindowsCompiler = WindowsCompiler.VisualStudio2015;
						}
						else if (LowercaseArg == "-2017")
						{
							// May be for compiling; don't set bGenerateProjectFiles by default, but do override the compiler if it is. 
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudio2017);
							OverrideWindowsCompiler = WindowsCompiler.VisualStudio2017;
						}
						else if (LowercaseArg.StartsWith("-makefile"))
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.Make);
						}
						else if (LowercaseArg.StartsWith("-cmakefile"))
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.CMake);
						}
						else if (LowercaseArg.StartsWith("-qmakefile"))
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.QMake);
						}
						else if (LowercaseArg.StartsWith("-kdevelopfile"))
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.KDevelop);
						}
						else if (LowercaseArg == "-codelitefiles")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.CodeLite);
						}
						else if (LowercaseArg == "-xcodeprojectfiles")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.XCode);
						}
						else if (LowercaseArg == "-eddieprojectfiles")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.Eddie);
						}
						else if (LowercaseArg == "-vscode")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudioCode);
						}
						else if (LowercaseArg == "-vsmac")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.VisualStudioMac);
						}
						else if (LowercaseArg == "-clion")
						{
							bGenerateProjectFiles = true;
							ProjectFileFormats.Add(ProjectFileFormat.CLion);
						}
						else if (LowercaseArg == "development" || LowercaseArg == "debug" || LowercaseArg == "shipping" || LowercaseArg == "test" || LowercaseArg == "debuggame")
						{
							//ConfigName = Arg;
						}
						else if (LowercaseArg.StartsWith("-modulewithsuffix="))
						{
							bSpecificModulesOnly = true;
							continue;
						}
						else if (LowercaseArg == "-ignorejunk")
						{
							BuildConfiguration.bIgnoreJunk = true;
						}
						else if (LowercaseArg == "-define")
						{
							// Skip -define
						}
						else if (LowercaseArg == "-progress")
						{
							ProgressWriter.bWriteMarkup = true;
						}
						else if (CheckPlatform.ToString().ToLowerInvariant() == LowercaseArg)
						{
							// It's the platform set...
							//PlatformName = Arg;
						}
						else if (LowercaseArg == "-vsdebugandroid")
						{
							AndroidProjectGenerator.VSDebugCommandLineOptionPresent = true;
						}
						else
						{
							// This arg may be a game name. Check for the existence of a game folder with this name.
							// "Engine" is not a valid game name.
							if (LowercaseArg != "engine" && Arg.IndexOfAny(Path.GetInvalidPathChars()) == -1 && Arg.IndexOfAny(new char[] { ':', Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar }) == -1 &&
								DirectoryReference.Exists(DirectoryReference.Combine(RootDirectory, Arg, "Config")))
							{
								GameName = Arg;
								Log.TraceVerbose("CommandLine: Found game name '{0}'", GameName);
							}
						}
					}

					// Find and register all tool chains, build platforms, etc. that are present
					SDKOutputLevel OutputLevel;
					if (UnrealBuildTool.bPrintDebugInfo)
					{
						OutputLevel = SDKOutputLevel.Verbose;
					}
					else if (Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
					{
						OutputLevel = SDKOutputLevel.Minimal;
					}
					else
					{
						OutputLevel = SDKOutputLevel.Quiet;
					}
					RegisterAllUBTClasses(OutputLevel, bValidatePlatforms);

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

					if (bGenerateProjectFiles)
					{
						// If there aren't any formats set, read the default project file format from the config file
						if (ProjectFileFormats.Count == 0)
						{
							// Read from the XML config
							if (!String.IsNullOrEmpty(ProjectFileGeneratorSettings.Format))
							{
								ProjectFileFormats.AddRange(ProjectFileGeneratorSettings.ParseFormatList(ProjectFileGeneratorSettings.Format));
							}

							// Read from the editor config
							ProjectFileFormat PreferredSourceCodeAccessor;
							if (ProjectFileGenerator.GetPreferredSourceCodeAccessor(ProjectFile, out PreferredSourceCodeAccessor))
							{
								ProjectFileFormats.Add(PreferredSourceCodeAccessor);
							}

							// If there's still nothing set, get the default project file format for this platform
							if (ProjectFileFormats.Count == 0)
							{
								BuildHostPlatform.Current.GetDefaultProjectFileFormats(ProjectFileFormats);
							}
						}

						// Create each project generator and run it
						ProjectFileGenerator.bGenerateProjectFiles = true;
						foreach (ProjectFileFormat ProjectFileFormat in ProjectFileFormats.Distinct())
						{
							ProjectFileGenerator Generator;
							switch (ProjectFileFormat)
							{
								case ProjectFileFormat.Make:
									Generator = new MakefileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.CMake:
									Generator = new CMakefileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.QMake:
									Generator = new QMakefileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.KDevelop:
									Generator = new KDevelopGenerator(ProjectFile);
									break;
								case ProjectFileFormat.CodeLite:
									Generator = new CodeLiteGenerator(ProjectFile, Arguments);
									break;
								case ProjectFileFormat.VisualStudio:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.Default, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.VisualStudio2012:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2012, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.VisualStudio2013:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2013, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.VisualStudio2015:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2015, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.VisualStudio2017:
									Generator = new VCProjectFileGenerator(ProjectFile, VCProjectFileFormat.VisualStudio2017, OverrideWindowsCompiler);
									break;
								case ProjectFileFormat.XCode:
									Generator = new XcodeProjectFileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.Eddie:
									Generator = new EddieProjectFileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.VisualStudioCode:
									Generator = new VSCodeProjectFileGenerator(ProjectFile);
									break;
								case ProjectFileFormat.CLion:
									Generator = new CLionGenerator(ProjectFile);
									break;
								default:
									throw new BuildException("Unhandled project file type '{0}", ProjectFileFormat);
							}
							if (!Generator.GenerateProjectFiles(Arguments))
							{
								Result = ECompilationResult.OtherCompilationError;
							}
						}
					}
					else if (bAutoSDKOnly)
					{
						// RegisterAllUBTClasses has already done all the SDK setup.
						Result = ECompilationResult.Succeeded;
					}
					else if (bValidatePlatforms)
					{
						ValidatePlatforms(Arguments);
					}
					else if (BuildConfiguration.DeployTargetFile != null)
					{
						UEBuildDeployTarget DeployTarget = new UEBuildDeployTarget(BuildConfiguration.DeployTargetFile);
						Log.WriteLine(LogEventType.Console, "Deploying {0} {1} {2}...", DeployTarget.TargetName, DeployTarget.Platform, DeployTarget.Configuration);
						UEBuildPlatform.GetBuildPlatform(DeployTarget.Platform).Deploy(DeployTarget);
						Result = ECompilationResult.Succeeded;
					}
					else
					{
						// Build our project
						if (Result == ECompilationResult.Succeeded)
						{
							Result = RunUBT(BuildConfiguration, Arguments, ProjectFile, true);
						}
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

				if (bUseMutex)
				{
					// Release the mutex to avoid the abandoned mutex timeout.
					SingleInstanceMutex.ReleaseMutex();
					SingleInstanceMutex.Dispose();
					SingleInstanceMutex = null;
				}
			}

			// Print some performance info
			Log.TraceVerbose("Execution time: {0}", (DateTime.UtcNow - StartTime).TotalSeconds);

			if (ExtendedErrorCode != 0)
			{
				return ExtendedErrorCode;
			}
			return (int)Result;
		}

		public static int ExtendedErrorCode = 0;
		private static int Main(string[] Arguments)
		{
			// make sure we catch any exceptions and return an appropriate error code.
			// Some inner code already does this (to ensure the Mutex is released),
			// but we need something to cover all outer code as well.
			try
			{
				return GuardedMain(Arguments);
			}
			catch (Exception Exception)
			{
				if (Log.IsInitialized())
				{
					Log.TraceError("UnrealBuildTool Exception: " + Exception.ToString());
				}
				if (ExtendedErrorCode != 0)
				{
					return ExtendedErrorCode;
				}
				return (int)ECompilationResult.OtherCompilationError;
			}
		}

		/// <summary>
		/// Generates project files.  Can also be used to generate projects "in memory", to allow for building
		/// targets without even having project files at all.
		/// </summary>
		/// <param name="Generator">Project generator to use</param>
		/// <param name="Arguments">Command-line arguments</param>
		/// <returns>True if successful</returns>
		internal static bool GenerateProjectFiles(ProjectFileGenerator Generator, string[] Arguments)
		{
			ProjectFileGenerator.bGenerateProjectFiles = true;
			bool bSuccess = Generator.GenerateProjectFiles(Arguments);
			ProjectFileGenerator.bGenerateProjectFiles = false;
			return bSuccess;
		}



		/// <summary>
		/// Validates the various platforms to determine if they are ready for building
		/// </summary>
		public static void ValidatePlatforms(string[] Arguments)
		{
			List<UnrealTargetPlatform> Platforms = new List<UnrealTargetPlatform>();
			foreach (string CurArgument in Arguments)
			{
				if (CurArgument.StartsWith("-"))
				{
					if (CurArgument.StartsWith("-Platforms=", StringComparison.InvariantCultureIgnoreCase))
					{
						// Parse the list... will be in Foo+Bar+New format
						string PlatformList = CurArgument.Substring(11);
						while (PlatformList.Length > 0)
						{
							string PlatformString = PlatformList;
							Int32 PlusIdx = PlatformList.IndexOf("+");
							if (PlusIdx != -1)
							{
								PlatformString = PlatformList.Substring(0, PlusIdx);
								PlatformList = PlatformList.Substring(PlusIdx + 1);
							}
							else
							{
								// We are on the last platform... clear the list to exit the loop
								PlatformList = "";
							}

							// Is the string a valid platform? If so, add it to the list
							UnrealTargetPlatform SpecifiedPlatform = UnrealTargetPlatform.Unknown;
							foreach (UnrealTargetPlatform PlatformParam in Enum.GetValues(typeof(UnrealTargetPlatform)))
							{
								if (PlatformString.Equals(PlatformParam.ToString(), StringComparison.InvariantCultureIgnoreCase))
								{
									SpecifiedPlatform = PlatformParam;
									break;
								}
							}

							if (SpecifiedPlatform != UnrealTargetPlatform.Unknown)
							{
								if (Platforms.Contains(SpecifiedPlatform) == false)
								{
									Platforms.Add(SpecifiedPlatform);
								}
							}
							else
							{
								Log.TraceWarning("ValidatePlatforms invalid platform specified: {0}", PlatformString);
							}
						}
					}
					else switch (CurArgument.ToUpperInvariant())
						{
							case "-ALLPLATFORMS":
								foreach (UnrealTargetPlatform platform in Enum.GetValues(typeof(UnrealTargetPlatform)))
								{
									if (platform != UnrealTargetPlatform.Unknown)
									{
										if (Platforms.Contains(platform) == false)
										{
											Platforms.Add(platform);
										}
									}
								}
								break;
						}
				}
			}

			foreach (UnrealTargetPlatform platform in Platforms)
			{
				UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(platform, true);
				if (BuildPlatform != null && BuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid)
				{
					Log.TraceInformation("##PlatformValidate: {0} VALID", BuildPlatform.GetPlatformValidationName());
				}
				else
				{
					Log.TraceInformation("##PlatformValidate: {0} INVALID", BuildPlatform != null ? BuildPlatform.GetPlatformValidationName() : platform.ToString());
				}
			}
		}

		internal static ECompilationResult RunUBT(BuildConfiguration BuildConfiguration, string[] Arguments, FileReference ProjectFile, bool bCatchExceptions)
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

				EHotReload HotReload = EHotReload.Disabled;
				if (TargetDescs.Count == 1 && !Arguments.Any(x => x.Equals("-NoHotReload", StringComparison.InvariantCultureIgnoreCase)))
				{
					if (BuildConfiguration.bAllowHotReloadFromIDE && ShouldDoHotReloadFromIDE(BuildConfiguration, Arguments, TargetDescs[0]))
					{
						HotReload = EHotReload.FromIDE;
					}
					else if (TargetDescs[0].OnlyModules.Count > 0 && TargetDescs[0].ForeignPlugin == null)
					{
						HotReload = EHotReload.FromEditor;
					}

					if (HotReload != EHotReload.Disabled && BuildConfiguration.bCleanProject)
					{
						throw new BuildException("Unable to clean target while hot-reloading. Close the editor and try again.");
					}
				}
				TargetDescriptor HotReloadTargetDesc = (HotReload != EHotReload.Disabled) ? TargetDescs[0] : null;

				if (ProjectFileGenerator.bGenerateProjectFiles)
				{
					// Create empty timestamp file to record when was the last time we regenerated projects.
					Directory.CreateDirectory(Path.GetDirectoryName(ProjectFileGenerator.ProjectTimestampFile));
					File.Create(ProjectFileGenerator.ProjectTimestampFile).Dispose();
				}


				// Used when BuildConfiguration.bUseUBTMakefiles is enabled.  If true, it means that our cached includes may not longer be
				// valid (or never were), and we need to recover by forcibly scanning included headers for all build prerequisites to make sure that our
				// cached set of includes is actually correct, before determining which files are outdated.
				bool bNeedsFullCPPIncludeRescan = false;

				if (!ProjectFileGenerator.bGenerateProjectFiles)
				{
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

							string LastBuiltTargetsFileName = (HotReload != EHotReload.Disabled) ? "HotReloadLastBuiltTargets.txt" : "LastBuiltTargets.txt";
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
								Log.TraceInformation("Performing full C++ include scan ({0} a new target)", (HotReload != EHotReload.Disabled) ? "hot reloading" : "building");
							}
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
						// If we're generating project files, then go ahead and wipe out the existing UBTMakefile for every target, to make sure that
						// it gets a full dependency scan next time.
						// NOTE: This is just a safeguard and doesn't have to be perfect.  We also check for newer project file timestamps in LoadUBTMakefile()
						FileReference UBTMakefilePath = UBTMakefile.GetUBTMakefilePath(TargetDescs, HotReload);
						if (ProjectFileGenerator.bGenerateProjectFiles) // @todo ubtmake: This is only hit when generating IntelliSense for project files.  Probably should be done right inside ProjectFileGenerator.bat
						{                                                   // @todo ubtmake: Won't catch multi-target cases as GPF always builds one target at a time for Intellisense
																			// Delete the UBTMakefile
							if (FileReference.Exists(UBTMakefilePath))
							{
								FileReference.Delete(UBTMakefilePath);
							}
						}

						// Make sure the gather phase is executed if we're not actually building anything
						if (ProjectFileGenerator.bGenerateProjectFiles || BuildConfiguration.bGenerateManifest || BuildConfiguration.bCleanProject || BuildConfiguration.bXGEExport || GeneratingActionGraph)
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

							// Invalid makefile if only modules have changed
							if (UBTMakefile != null && !TargetDescs.SelectMany(x => x.OnlyModules)
									.Select(x => new Tuple<string, bool>(x.OnlyModuleName.ToLower(), string.IsNullOrWhiteSpace(x.OnlyModuleSuffix)))
									.SequenceEqual(
										UBTMakefile.Targets.SelectMany(x => x.OnlyModules)
										.Select(x => new Tuple<string, bool>(x.OnlyModuleName.ToLower(), string.IsNullOrWhiteSpace(x.OnlyModuleSuffix)))
									)
								)
							{
								UBTMakefile = null;
								ReasonNotLoaded = "modules to compile have changed";
							}

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

								Log.TraceInformation("Creating makefile for {0}{1}{2} ({3})",
									(HotReload != EHotReload.Disabled) ? "hot reloading " : "",
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

						ReadOnlyBuildVersion Version = new ReadOnlyBuildVersion(BuildVersion.ReadDefault());

						Targets = new List<UEBuildTarget>();
						foreach (TargetDescriptor TargetDesc in TargetDescs)
						{
							UEBuildTarget Target = UEBuildTarget.CreateTarget(TargetDesc, Arguments, bSkipRulesCompile, BuildConfiguration.SingleFileToCompile != null, BuildConfiguration.bUsePrecompiled, Version);
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
					Dictionary<string, List<UHTModuleInfo>> TargetNameToUObjectModules = new Dictionary<string, List<UHTModuleInfo>>(StringComparer.InvariantCultureIgnoreCase);
					foreach (UEBuildTarget Target in Targets)
					{
						if (HotReload != EHotReload.Disabled)
						{
							// Don't produce new DLLs if there's been no code changes
							BuildConfiguration.bSkipLinkingWhenNothingToCompile = true;
							Log.TraceInformation("Compiling game modules for hot reload");
						}

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
							FileReference CacheFile = FlatCPPIncludeDependencyCache.GetDependencyCachePathForTarget(Target);
							if (!FlatCPPIncludeDependencyCache.TryRead(CacheFile, out Headers.FlatCPPIncludeDependencyCache))
							{
								if (!bNeedsFullCPPIncludeRescan)
								{
									if (!ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bXGEExport && !BuildConfiguration.bGenerateManifest && !BuildConfiguration.bCleanProject)
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
							if (BuildConfiguration.bCleanProject)
							{
								BuildResult = Target.Clean(!BuildConfiguration.bDoNotBuildUHT && HotReload != EHotReload.FromIDE);
							}
							else
							{
								BuildResult = Target.Build(BuildConfiguration, TargetToHeaders[Target], TargetOutputItems, TargetUObjectModules, WorkingSet, ActionGraph, HotReload, bIsAssemblingBuild);
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
						}
					}

					if (BuildResult == ECompilationResult.Succeeded && !BuildConfiguration.bSkipBuild &&
						(
							(BuildConfiguration.bXGEExport && BuildConfiguration.bGenerateManifest) ||
							(!GeneratingActionGraph && !ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bGenerateManifest && !BuildConfiguration.bCleanProject)
						))
					{
						if (bIsGatheringBuild)
						{
							ActionGraph.FinalizeActionGraph();

							UBTMakefile = new UBTMakefile();
							UBTMakefile.AllActions = ActionGraph.AllActions;

							// For now simply treat all object files as the root target.
							{
								HashSet<Action> PrerequisiteActionsSet = new HashSet<Action>();
								foreach (FileItem OutputItem in OutputItemsForAllTargets)
								{
									ActionGraph.GatherPrerequisiteActions(OutputItem, ref PrerequisiteActionsSet);
								}
								UBTMakefile.PrerequisiteActions = PrerequisiteActionsSet.ToArray();
							}

							foreach (System.Collections.DictionaryEntry EnvironmentVariable in Environment.GetEnvironmentVariables())
							{
								UBTMakefile.EnvironmentVariables.Add(Tuple.Create((string)EnvironmentVariable.Key, (string)EnvironmentVariable.Value));
							}
							UBTMakefile.TargetNameToUObjectModules = TargetNameToUObjectModules;
							UBTMakefile.Targets = Targets;
							UBTMakefile.bUseAdaptiveUnityBuild = Targets.Any(x => x.Rules.bUseAdaptiveUnityBuild);
							UBTMakefile.SourceFileWorkingSet = Unity.SourceFileWorkingSet;
							UBTMakefile.CandidateSourceFilesForWorkingSet = Unity.CandidateSourceFilesForWorkingSet;

							if (BuildConfiguration.bUseUBTMakefiles && !UBTMakefile.PrerequisiteActions.Any(x => x.ActionHandler != null))
							{
								// We've been told to prepare to build, so let's go ahead and save out our action graph so that we can use in a later invocation 
								// to assemble the build.  Even if we are configured to assemble the build in this same invocation, we want to save out the
								// Makefile so that it can be used on subsequent 'assemble only' runs, for the fastest possible iteration times
								// @todo ubtmake: Optimization: We could make 'gather + assemble' mode slightly faster by saving this while busy compiling (on our worker thread)
								UBTMakefile.SaveUBTMakefile(TargetDescs, HotReload, UBTMakefile);
							}
						}

						if (bIsAssemblingBuild)
						{
							// If we didn't build the graph in this session, then we'll need to load a cached one
							if (!bIsGatheringBuild && BuildResult.Succeeded())
							{
								ActionGraph.AllActions = UBTMakefile.AllActions;

								// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
								// the same for every invocation on that makefile, but we need a new suffix each time.
								if (HotReload != EHotReload.Disabled)
								{
									PatchActionHistoryForHotReloadAssembling(ActionGraph, HotReloadTargetDesc.OnlyModules, Targets);
								}


								foreach (Tuple<string, string> EnvironmentVariable in UBTMakefile.EnvironmentVariables)
								{
									// @todo ubtmake: There may be some variables we do NOT want to clobber.
									Environment.SetEnvironmentVariable(EnvironmentVariable.Item1, EnvironmentVariable.Item2);
								}

								// Execute all the pre-build steps
								if (!ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bXGEExport)
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
												ECompilationResult UHTResult = ECompilationResult.OtherCompilationError;
												if (!ExternalExecution.ExecuteHeaderToolIfNecessary(BuildConfiguration, Target, GlobalCompileEnvironment: null, UObjectModules: TargetUObjectModules, ModuleInfoFileName: ModuleInfoFileName, UHTResult: ref UHTResult, HotReload: HotReload, bIsGatheringBuild: bIsGatheringBuild, bIsAssemblingBuild: bIsAssemblingBuild))
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
								// Make sure any old DLL files from in-engine recompiles aren't lying around.  Must be called after the action graph is finalized.
								ActionGraph.DeleteStaleHotReloadDLLs();

								foreach (UEBuildTarget Target in Targets)
								{
									UEBuildPlatform.GetBuildPlatform(Target.Platform).PreBuildSync();
								}

								// Plan the actions to execute for the build.
								Dictionary<UEBuildTarget, List<FileItem>> TargetToOutdatedPrerequisitesMap;
								List<Action> ActionsToExecute = ActionGraph.GetActionsToExecute(BuildConfiguration, UBTMakefile.PrerequisiteActions, Targets, TargetToHeaders, bIsAssemblingBuild, bNeedsFullCPPIncludeRescan, out TargetToOutdatedPrerequisitesMap);

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

								// If we're not touching any shared files (ie. anything under Engine), allow the build ids to be recycled between applications.
								HashSet<FileReference> OutputFiles = new HashSet<FileReference>(ActionsToExecute.SelectMany(x => x.ProducedItems.Select(y => y.Location)));
								foreach (UEBuildTarget Target in Targets)
								{
									if (!Target.TryRecycleVersionManifests(OutputFiles))
									{
										Target.InvalidateVersionManifests();
									}
								}

								// Execute the actions.
								string TargetInfoForTelemetry = String.Join("|", Targets.Select(x => String.Format("{0} {1} {2}{3}", x.TargetName, x.Platform, x.Configuration, "")));
								DateTime ExecutorStartTime = DateTime.UtcNow;
								if (BuildConfiguration.bXGEExport)
								{
									XGE.ExportActions(ActionsToExecute);
									bSuccess = true;
								}
								else
								{
									bSuccess = ActionGraph.ExecuteActions(BuildConfiguration, ActionsToExecute, out ExecutorName, TargetInfoForTelemetry, HotReload);
								}
								TotalExecutorTime += (DateTime.UtcNow - ExecutorStartTime).TotalSeconds;

								// if the build succeeded, write the receipts and do any needed syncing
								if (bSuccess)
								{
									foreach (UEBuildTarget Target in Targets)
									{
										Target.WriteReceipts();
										UEBuildPlatform.GetBuildPlatform(Target.Platform).PostBuildSync(Target);
									}
									if (ActionsToExecute.Count == 0 && BuildConfiguration.bSkipLinkingWhenNothingToCompile)
									{
										BuildResult = ECompilationResult.UpToDate;
									}
								}
								else
								{
									BuildResult = ECompilationResult.OtherCompilationError;
								}
							}

							// Execute all the post-build steps
						if (BuildResult.Succeeded() && !ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bXGEExport)
							foreach (UEBuildTarget Target in Targets)
							{
								if (!Target.ExecuteCustomPostBuildSteps())
								{
									{
										BuildResult = ECompilationResult.OtherCompilationError;
										break;
									}
								}
							}

							// Run the deployment steps
							if (BuildResult.Succeeded()
								&& String.IsNullOrEmpty(BuildConfiguration.SingleFileToCompile)
								&& !ProjectFileGenerator.bGenerateProjectFiles
								&& !BuildConfiguration.bGenerateManifest
								&& !BuildConfiguration.bCleanProject
							&& !BuildConfiguration.bXGEExport)
							{
								foreach (UEBuildTarget Target in Targets)
								{
									if (Target.DeployTargetFile != null && Target.OnlyModules.Count == 0)
									{
										Log.WriteLine(LogEventType.Console, "Deploying {0} {1} {2}...", Target.TargetName, Target.Platform, Target.Configuration);
										UEBuildDeployTarget DeployTarget = new UEBuildDeployTarget(Target.DeployTargetFile);
										UEBuildPlatform.GetBuildPlatform(Target.Platform).Deploy(DeployTarget);
									}
								}
							}
						}
					}
				}
			}
			catch (Exception Ex)
			{
				if (!bCatchExceptions)
				{
					throw;
				}
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
		/// Checks if the editor is currently running and this is a hot-reload
		/// </summary>
		private static bool ShouldDoHotReloadFromIDE(BuildConfiguration BuildConfiguration, string[] Arguments, TargetDescriptor TargetDesc)
		{
			// Check if Hot-reload is disabled globally for this project
			ConfigHierarchy Hierarchy = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(TargetDesc.ProjectFile), TargetDesc.Platform);
			bool bAllowHotReloadFromIDE;
			if (Hierarchy.TryGetValue("BuildConfiguration", "bAllowHotReloadFromIDE", out bAllowHotReloadFromIDE) && !bAllowHotReloadFromIDE)
			{
				return false;
			}

			if (Arguments.Any(x => x.Equals("-NoHotReloadFromIDE", StringComparison.InvariantCultureIgnoreCase)))
			{
				// Hot reload disabled through command line, possibly running from UAT
				return false;
			}

			bool bIsRunning = false;

			// @todo ubtmake: Kind of cheating here to figure out if an editor target.  At this point we don't have access to the actual target description, and
			// this code must be able to execute before we create or load module rules DLLs so that hot reload can work with bUseUBTMakefiles
			bool bIsEditorTarget = TargetDesc.Name.EndsWith("Editor", StringComparison.InvariantCultureIgnoreCase);

			if (!ProjectFileGenerator.bGenerateProjectFiles && !BuildConfiguration.bGenerateManifest && bIsEditorTarget)
			{
				string EditorBaseFileName = "UE4Editor";
				if (TargetDesc.Configuration != UnrealTargetConfiguration.Development && TargetDesc.Configuration != UnrealTargetConfiguration.DebugGame)
				{
					EditorBaseFileName = String.Format("{0}-{1}-{2}", EditorBaseFileName, TargetDesc.Platform, TargetDesc.Configuration);
				}

				FileReference EditorLocation;
				if (TargetDesc.Platform == UnrealTargetPlatform.Win64)
				{
					EditorLocation = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Binaries", "Win64", String.Format("{0}.exe", EditorBaseFileName));
				}
				else if (TargetDesc.Platform == UnrealTargetPlatform.Mac)
				{
					EditorLocation = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Binaries", "Mac", String.Format("{0}.app/Contents/MacOS/{0}", EditorBaseFileName));
				}
				else if (TargetDesc.Platform == UnrealTargetPlatform.Linux)
				{
					EditorLocation = FileReference.Combine(UnrealBuildTool.EngineDirectory, "Binaries", "Linux", EditorBaseFileName);
				}
				else
				{
					throw new BuildException("Unknown editor filename for this platform");
				}

				BuildHostPlatform.ProcessInfo[] Processes = BuildHostPlatform.Current.GetProcesses();
				string EditorRunsDir = Path.Combine(UnrealBuildTool.EngineDirectory.FullName, "Intermediate", "EditorRuns");

				if (!Directory.Exists(EditorRunsDir))
				{
					return false;
				}

				FileInfo[] EditorRunsFiles = new DirectoryInfo(EditorRunsDir).GetFiles();

				foreach (FileInfo File in EditorRunsFiles)
				{
					int PID;
					BuildHostPlatform.ProcessInfo Proc = null;
					if (!Int32.TryParse(File.Name, out PID) || (Proc = Processes.FirstOrDefault(P => P.PID == PID)) == default(BuildHostPlatform.ProcessInfo))
					{
						// Delete stale files (it may happen if editor crashes).
						File.Delete();
						continue;
					}

					// Don't break here to allow clean-up of other stale files.
					if (!bIsRunning)
					{
						// Otherwise check if the path matches.
						bIsRunning = new FileReference(Proc.Filename) == EditorLocation;
					}
				}
			}
			return bIsRunning;
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

		/// <summary>
		/// Replaces a hot reload suffix in a filename.
		/// </summary>
		/// <param name="InOutFilename">The filename to replace the suffix in.</param>
		/// <param name="ModuleNameToSuffix">A function which returns a replacement suffix for a given module name.</param>
		/// <returns>true is a suffix replacement was made, false otherwise.</returns>
		public static bool ReplaceHotReloadFilenameSuffix(ref string InOutFilename, Func<string, string> ModuleNameToSuffix)
		{
			// Remove the extension
			string FilenameWithoutExtension = Path.GetFileNameWithoutExtension(InOutFilename);
			string Extension = Path.GetExtension(InOutFilename);

			// Remove the numbered suffix from the end of the filename
			int IndexOfLastHyphen = FilenameWithoutExtension.LastIndexOf('-');
			string OriginalNumberSuffix = FilenameWithoutExtension.Substring(IndexOfLastHyphen);
			int NumberSuffix;
			bool bHasNumberSuffix = int.TryParse(OriginalNumberSuffix, out NumberSuffix);

			string OriginalFileNameWithoutNumberSuffix = null;
			string PlatformConfigSuffix = "";
			if (bHasNumberSuffix)
			{
				// Remove "-####" suffix in Development configuration
				OriginalFileNameWithoutNumberSuffix = FilenameWithoutExtension.Substring(0, IndexOfLastHyphen);
			}
			else
			{
				OriginalFileNameWithoutNumberSuffix = FilenameWithoutExtension;

				// Remove "-####-Platform-Configuration" suffix in Debug configuration
				int IndexOfSecondLastHyphen = FilenameWithoutExtension.LastIndexOf('-', IndexOfLastHyphen - 1, IndexOfLastHyphen);
				if (IndexOfSecondLastHyphen > 0)
				{
					int IndexOfThirdLastHyphen = FilenameWithoutExtension.LastIndexOf('-', IndexOfSecondLastHyphen - 1, IndexOfSecondLastHyphen);
					if (IndexOfThirdLastHyphen > 0)
					{
						if (int.TryParse(FilenameWithoutExtension.Substring(IndexOfThirdLastHyphen + 1, IndexOfSecondLastHyphen - IndexOfThirdLastHyphen - 1), out NumberSuffix))
						{
							OriginalFileNameWithoutNumberSuffix = FilenameWithoutExtension.Substring(0, IndexOfThirdLastHyphen);
							PlatformConfigSuffix = FilenameWithoutExtension.Substring(IndexOfSecondLastHyphen);
							bHasNumberSuffix = true;
						}
					}
				}
			}

			if (!bHasNumberSuffix)
			{
				return false;
			}

			// Figure out which suffix to use
			int FirstHyphenIndex = OriginalFileNameWithoutNumberSuffix.IndexOf('-');
			if (FirstHyphenIndex == -1)
			{
				throw new BuildException("Expected produced item file name '{0}' to start with a prefix (such as 'UE4Editor-') when hot reloading", FilenameWithoutExtension);
			}
			string OutputFileNamePrefix = OriginalFileNameWithoutNumberSuffix.Substring(0, FirstHyphenIndex + 1);

			// Get the module name from the file name
			string ModuleName = OriginalFileNameWithoutNumberSuffix.Substring(OutputFileNamePrefix.Length);

			// Add a new suffix
			string UniqueSuffix = "-" + ModuleNameToSuffix(ModuleName);

			InOutFilename = OriginalFileNameWithoutNumberSuffix + UniqueSuffix + PlatformConfigSuffix + Extension;
			return true;
		}

		/// <summary>
		/// Returns a module suffix from a list of OnlyModules if one exists, otherwise generates a new one and adds it to the list.
		/// </summary>
		/// <param name="OnlyModules">A list of modules with suffixes to search and add to if necessary.</param>
		/// <param name="ModuleName">The module name to find in the list.</param>
		/// <returns>The existing or new suffix for the module name.</returns>
		public static string GetReplacementModuleSuffix(List<OnlyModule> OnlyModules, string ModuleName)
		{
			// Check the OnlyModules list for a prefix
			if (OnlyModules != null)
			{
				OnlyModule FoundModule = OnlyModules.Find((OnlyModule) => OnlyModule.OnlyModuleName.Equals(ModuleName, StringComparison.InvariantCultureIgnoreCase));
				if (FoundModule != null)
				{
					return FoundModule.OnlyModuleSuffix;
				}
			}

			// Generate a new random suffix
			string Result = (new Random((int)(DateTime.Now.Ticks % Int32.MaxValue)).Next(10000)).ToString();

			// Add it to the OnlyModules list
			OnlyModules.Add(new OnlyModule(ModuleName, Result));

			return Result;
		}

		/// <summary>
		/// Patch action history for hot reload when running in assembler mode.  In assembler mode, the suffix on the output file will be
		/// the same for every invocation on that makefile, but we need a new suffix each time.
		/// </summary>
		static void PatchActionHistoryForHotReloadAssembling(ActionGraph ActionGraph, List<OnlyModule> OnlyModules, List<UEBuildTarget> Targets)
		{
			// Take a copy of the array so that we don't modify the original in GetReplacementModuleSuffix
			List<OnlyModule> OnlyModulesLocal = new List<OnlyModule>(OnlyModules);

			// Gather all of the response files for link actions.  We're going to need to patch 'em up after we figure out new
			// names for all of the output files and import libraries
			List<string> ResponseFilePaths = new List<string>();

			// Same as Response files but for all of the link.sh files for link actions.
			// Only used on BuildHostPlatform Linux
			List<string> LinkScriptFilePaths = new List<string>();

			// Keep a map of the original file names and their new file names, so we can fix up response files after
			List<Tuple<string, string>> OriginalFileNameAndNewFileNameList_NoExtensions = new List<Tuple<string, string>>();

			// Finally, we'll keep track of any file items that we had to create counterparts for change file names, so we can fix those up too
			Dictionary<FileItem, FileItem> AffectedOriginalFileItemAndNewFileItemMap = new Dictionary<FileItem, FileItem>();

			foreach (Action Action in ActionGraph.AllActions.Where((Action) => Action.ActionType == ActionType.Link))
			{
				// Assume that the first produced item (with no extension) is our output file name
				string OriginalFileNameWithoutExtension = Utils.GetFilenameWithoutAnyExtensions(Action.ProducedItems[0].AbsolutePath);

				string NewFileNameWithoutExtension = OriginalFileNameWithoutExtension;
				if (!ReplaceHotReloadFilenameSuffix(ref NewFileNameWithoutExtension, (ModuleName) => GetReplacementModuleSuffix(OnlyModulesLocal, ModuleName)))
				{
					continue;
				}

				// Find the response file in the command line.  We'll need to make a copy of it with our new file name.
				string ResponseFileExtension = ".response";
				int ResponseExtensionIndex = Action.CommandArguments.IndexOf(ResponseFileExtension, StringComparison.InvariantCultureIgnoreCase);
				if (ResponseExtensionIndex != -1)
				{
					int ResponseFilePathIndex = Action.CommandArguments.LastIndexOf("@\"", ResponseExtensionIndex);
					if (ResponseFilePathIndex == -1)
					{
						throw new BuildException("Couldn't find response file path in action's command arguments when hot reloading");
					}

					string OriginalResponseFilePathWithoutExtension = Action.CommandArguments.Substring(ResponseFilePathIndex + 2, (ResponseExtensionIndex - ResponseFilePathIndex) - 2);
					string OriginalResponseFilePath = OriginalResponseFilePathWithoutExtension + ResponseFileExtension;

					string NewResponseFilePath = OriginalResponseFilePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);

					// Copy the old response file to the new path
					File.Copy(OriginalResponseFilePath, NewResponseFilePath, overwrite: true);

					// Keep track of the new response file name.  We'll have to do some edits afterwards.
					ResponseFilePaths.Add(NewResponseFilePath);
				}

				// Find the *.link.sh file in the command line.  We'll need to make a copy of it with our new file name.
				// Only currently used on Linux
				if (UEBuildPlatform.IsPlatformInGroup(BuildHostPlatform.Current.Platform, UnrealPlatformGroup.Unix))
				{
					string LinkScriptFileExtension = ".link.sh";
					int LinkScriptExtensionIndex = Action.CommandArguments.IndexOf(LinkScriptFileExtension, StringComparison.InvariantCultureIgnoreCase);
					if (LinkScriptExtensionIndex != -1)
					{
						// We expect the script invocation to be quoted
						int LinkScriptFilePathIndex = Action.CommandArguments.LastIndexOf("\"", LinkScriptExtensionIndex);
						if (LinkScriptFilePathIndex == -1)
						{
							throw new BuildException("Couldn't find link script file path in action's command arguments when hot reloading. Is the path quoted?");
						}

						string OriginalLinkScriptFilePathWithoutExtension = Action.CommandArguments.Substring(LinkScriptFilePathIndex + 1, (LinkScriptExtensionIndex - LinkScriptFilePathIndex) - 1);
						string OriginalLinkScriptFilePath = OriginalLinkScriptFilePathWithoutExtension + LinkScriptFileExtension;

						string NewLinkScriptFilePath = OriginalLinkScriptFilePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);

						// Copy the old response file to the new path
						File.Copy(OriginalLinkScriptFilePath, NewLinkScriptFilePath, overwrite: true);

						// Keep track of the new response file name.  We'll have to do some edits afterwards.
						LinkScriptFilePaths.Add(NewLinkScriptFilePath);
					}

					// Update this action's list of prerequisite items too
					for (int ItemIndex = 0; ItemIndex < Action.PrerequisiteItems.Count; ++ItemIndex)
					{
						FileItem OriginalPrerequisiteItem = Action.PrerequisiteItems[ItemIndex];
						string NewPrerequisiteItemFilePath = OriginalPrerequisiteItem.AbsolutePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);

						if (OriginalPrerequisiteItem.AbsolutePath != NewPrerequisiteItemFilePath)
						{
							// OK, the prerequisite item's file name changed so we'll update it to point to our new file
							FileItem NewPrerequisiteItem = FileItem.GetItemByPath(NewPrerequisiteItemFilePath);
							Action.PrerequisiteItems[ItemIndex] = NewPrerequisiteItem;

							// Copy the other important settings from the original file item
							NewPrerequisiteItem.bNeedsHotReloadNumbersDLLCleanUp = OriginalPrerequisiteItem.bNeedsHotReloadNumbersDLLCleanUp;
							NewPrerequisiteItem.ProducingAction = OriginalPrerequisiteItem.ProducingAction;

							// Keep track of it so we can fix up dependencies in a second pass afterwards
							AffectedOriginalFileItemAndNewFileItemMap.Add(OriginalPrerequisiteItem, NewPrerequisiteItem);

							ResponseExtensionIndex = OriginalPrerequisiteItem.AbsolutePath.IndexOf(ResponseFileExtension, StringComparison.InvariantCultureIgnoreCase);
							if (ResponseExtensionIndex != -1)
							{
								string OriginalResponseFilePathWithoutExtension = OriginalPrerequisiteItem.AbsolutePath.Substring(0, ResponseExtensionIndex);
								string OriginalResponseFilePath = OriginalResponseFilePathWithoutExtension + ResponseFileExtension;

								string NewResponseFilePath = OriginalResponseFilePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);

								// Copy the old response file to the new path
								File.Copy(OriginalResponseFilePath, NewResponseFilePath, overwrite: true);

								// Keep track of the new response file name.  We'll have to do some edits afterwards.
								ResponseFilePaths.Add(NewResponseFilePath);
							}
						}
					}
				}

				// Go ahead and replace all occurrences of our file name in the command-line (ignoring extensions)
				Action.CommandArguments = Action.CommandArguments.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);


				// Update this action's list of produced items too
				for (int ItemIndex = 0; ItemIndex < Action.ProducedItems.Count; ++ItemIndex)
				{
					FileItem OriginalProducedItem = Action.ProducedItems[ItemIndex];

					string NewProducedItemFilePath = OriginalProducedItem.AbsolutePath.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);
					if (OriginalProducedItem.AbsolutePath != NewProducedItemFilePath)
					{
						// OK, the produced item's file name changed so we'll update it to point to our new file
						FileItem NewProducedItem = FileItem.GetItemByPath(NewProducedItemFilePath);
						Action.ProducedItems[ItemIndex] = NewProducedItem;

						// Copy the other important settings from the original file item
						NewProducedItem.bNeedsHotReloadNumbersDLLCleanUp = OriginalProducedItem.bNeedsHotReloadNumbersDLLCleanUp;
						NewProducedItem.ProducingAction = OriginalProducedItem.ProducingAction;

						// Keep track of it so we can fix up dependencies in a second pass afterwards
						AffectedOriginalFileItemAndNewFileItemMap.Add(OriginalProducedItem, NewProducedItem);
					}
				}

				// The status description of the item has the file name, so we'll update it too
				Action.StatusDescription = Action.StatusDescription.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);


				// Keep track of the file names, so we can fix up response files afterwards.
				OriginalFileNameAndNewFileNameList_NoExtensions.Add(Tuple.Create(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension));
			}


			// Do another pass and update any actions that depended on the original file names that we changed
			foreach (Action Action in ActionGraph.AllActions)
			{
				for (int ItemIndex = 0; ItemIndex < Action.PrerequisiteItems.Count; ++ItemIndex)
				{
					FileItem OriginalFileItem = Action.PrerequisiteItems[ItemIndex];

					FileItem NewFileItem;
					if (AffectedOriginalFileItemAndNewFileItemMap.TryGetValue(OriginalFileItem, out NewFileItem))
					{
						// OK, looks like we need to replace this file item because we've renamed the file
						Action.PrerequisiteItems[ItemIndex] = NewFileItem;
					}
				}
			}


			if (OriginalFileNameAndNewFileNameList_NoExtensions.Count > 0)
			{
				foreach (string ResponseFilePath in ResponseFilePaths)
				{
					// Load the file up
					StringBuilder FileContents = new StringBuilder(Utils.ReadAllText(ResponseFilePath));

					// Replace all of the old file names with new ones
					foreach (Tuple<string, string> FileNameTuple in OriginalFileNameAndNewFileNameList_NoExtensions)
					{
						string OriginalFileNameWithoutExtension = FileNameTuple.Item1;
						string NewFileNameWithoutExtension = FileNameTuple.Item2;

						FileContents.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);
					}

					// Overwrite the original file
					string FileContentsString = FileContents.ToString();
					File.WriteAllText(ResponseFilePath, FileContentsString, new System.Text.UTF8Encoding(false));
				}

				if (UEBuildPlatform.IsPlatformInGroup(BuildHostPlatform.Current.Platform, UnrealPlatformGroup.Unix))
				{
					foreach (string LinkScriptFilePath in LinkScriptFilePaths)
					{
						// Load the file up
						StringBuilder FileContents = new StringBuilder(Utils.ReadAllText(LinkScriptFilePath));

						// Replace all of the old file names with new ones
						foreach (Tuple<string, string> FileNameTuple in OriginalFileNameAndNewFileNameList_NoExtensions)
						{
							string OriginalFileNameWithoutExtension = FileNameTuple.Item1;
							string NewFileNameWithoutExtension = FileNameTuple.Item2;

							FileContents.Replace(OriginalFileNameWithoutExtension, NewFileNameWithoutExtension);
						}

						// Overwrite the original file
						string FileContentsString = FileContents.ToString();
						File.WriteAllText(LinkScriptFilePath, FileContentsString, new System.Text.UTF8Encoding(false));
					}
				}
			}

			foreach (UEBuildTarget Target in Targets)
			{
				Target.PatchModuleManifestsForHotReloadAssembling(OnlyModulesLocal);
				Target.WriteReceipts();
			}
		}
	}
}
