// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using UnrealBuildTool;
using System.Diagnostics;
using Tools.DotNETCommon;
using System.Reflection;

namespace AutomationTool
{
	public struct SingleTargetProperties
	{
		public string TargetName;
		public TargetRules Rules;
	}

	/// <summary>
	/// Autodetected project properties.
	/// </summary>
	public class ProjectProperties
	{
		/// <summary>
		/// Full Project path. Must be a .uproject file
		/// </summary>
		public FileReference RawProjectPath;

		/// <summary>
		/// True if the uproject contains source code.
		/// </summary>
		public bool bIsCodeBasedProject;

		/// <summary>
		/// List of all targets detected for this project.
		/// </summary>
		public List<SingleTargetProperties> Targets = new List<SingleTargetProperties>();

		/// <summary>
		/// List of all Engine ini files for this project
		/// </summary>
		public Dictionary<UnrealTargetPlatform, ConfigHierarchy> EngineConfigs = new Dictionary<UnrealTargetPlatform,ConfigHierarchy>();

		/// <summary>
		/// List of all Game ini files for this project
		/// </summary>
		public Dictionary<UnrealTargetPlatform, ConfigHierarchy> GameConfigs = new Dictionary<UnrealTargetPlatform, ConfigHierarchy>();

		/// <summary>
		/// List of all programs detected for this project.
		/// </summary>
		public List<SingleTargetProperties> Programs = new List<SingleTargetProperties>();

		/// <summary>
		/// Specifies if the target files were generated
		/// </summary>
		public bool bWasGenerated = false;

		internal ProjectProperties()
		{
		}
	}

	/// <summary>
	/// Project related utility functions.
	/// </summary>
	public class ProjectUtils
	{
		private static Dictionary<string, ProjectProperties> PropertiesCache = new Dictionary<string, ProjectProperties>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Gets a short project name (QAGame, Elemental, etc)
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="bIsUProjectFile">True if a uproject.</param>
		/// <returns>Short project name</returns>
		public static string GetShortProjectName(FileReference RawProjectPath)
		{
			return CommandUtils.GetFilenameWithoutAnyExtensions(RawProjectPath.FullName);
		}

		/// <summary>
		/// Gets project properties.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>Properties of the project.</returns>
		public static ProjectProperties GetProjectProperties(FileReference RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms = null, List<UnrealTargetConfiguration> ClientTargetConfigurations = null, bool AssetNativizationRequested = false)
		{
			string ProjectKey = "UE4";
			if (RawProjectPath != null)
			{
				ProjectKey = CommandUtils.ConvertSeparators(PathSeparator.Slash, RawProjectPath.FullName);
			}
			ProjectProperties Properties;
			if (PropertiesCache.TryGetValue(ProjectKey, out Properties) == false)
			{
                Properties = DetectProjectProperties(RawProjectPath, ClientTargetPlatforms, ClientTargetConfigurations, AssetNativizationRequested);
				PropertiesCache.Add(ProjectKey, Properties);
			}
			return Properties;
		}

		/// <summary>
		/// Checks if the project is a UProject file with source code.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>True if the project is a UProject file with source code.</returns>
		public static bool IsCodeBasedUProjectFile(FileReference RawProjectPath, List<UnrealTargetConfiguration> ClientTargetConfigurations = null)
		{
			return GetProjectProperties(RawProjectPath, null, ClientTargetConfigurations).bIsCodeBasedProject;
		}

		/// <summary>
		/// Returns a path to the client binaries folder.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="Platform">Platform type.</param>
		/// <returns>Path to the binaries folder.</returns>
		public static DirectoryReference GetProjectClientBinariesFolder(DirectoryReference ProjectClientBinariesPath, UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown)
		{
			if (Platform != UnrealTargetPlatform.Unknown)
			{
				ProjectClientBinariesPath = DirectoryReference.Combine(ProjectClientBinariesPath, Platform.ToString());
			}
			return ProjectClientBinariesPath;
		}

		private static bool RequiresTempTarget(FileReference RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms, List<UnrealTargetConfiguration> ClientTargetConfigurations, bool AssetNativizationRequested)
		{
			string Reason;
			if(RequiresTempTarget(RawProjectPath, ClientTargetPlatforms, ClientTargetConfigurations, AssetNativizationRequested, out Reason))
			{
				Log.TraceInformation("{0} requires a temporary target.cs to be generated ({1})", RawProjectPath.GetFileName(), Reason);
				return true;
			}
			return false;
		}

		private static bool RequiresTempTarget(FileReference RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms, List<UnrealTargetConfiguration> ClientTargetConfigurations, bool AssetNativizationRequested, out string Reason)
		{
			// check to see if we already have a Target.cs file
			if (File.Exists (Path.Combine (Path.GetDirectoryName (RawProjectPath.FullName), "Source", RawProjectPath.GetFileNameWithoutExtension() + ".Target.cs")))
			{
				Reason = null;
				return false;
			}
			else if (Directory.Exists(Path.Combine(Path.GetDirectoryName(RawProjectPath.FullName), "Source")))
			{
				// wasn't one in the main Source directory, let's check all sub-directories
				//@todo: may want to read each target.cs to see if it has a target corresponding to the project name as a final check
				FileInfo[] Files = (new DirectoryInfo( Path.Combine (Path.GetDirectoryName (RawProjectPath.FullName), "Source")).GetFiles ("*.Target.cs", SearchOption.AllDirectories));
				if (Files.Length > 0)
				{
					Reason = null;
					return false;
				}
			}

            //
            // once we reach this point, we can surmise that this is an asset-
            // only (code free) project

            if (AssetNativizationRequested)
            {
                // we're going to be converting some of the project's assets 
                // into native code, so we require a distinct target (executable) 
                // be generated for this project
				Reason = "asset nativization is enabled";
                return true;
            }

			if (ClientTargetPlatforms != null)
			{
				foreach (UnrealTargetPlatform ClientPlatform in ClientTargetPlatforms)
				{
					EncryptionAndSigning.CryptoSettings Settings = EncryptionAndSigning.ParseCryptoSettings(RawProjectPath.Directory, ClientPlatform);
					if (Settings.IsAnyEncryptionEnabled() || Settings.IsPakSigningEnabled())
					{
						Reason = "encryption/signing is enabled";
						return true;
					}
				}
			}

			// no Target file, now check to see if build settings have changed
			List<UnrealTargetPlatform> TargetPlatforms = ClientTargetPlatforms;
			if (ClientTargetPlatforms == null || ClientTargetPlatforms.Count < 1)
			{
				// No client target platforms, add all in
				TargetPlatforms = new List<UnrealTargetPlatform>();
				foreach (UnrealTargetPlatform TargetPlatformType in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (TargetPlatformType != UnrealTargetPlatform.Unknown)
					{
						TargetPlatforms.Add(TargetPlatformType);
					}
				}
			}

			List<UnrealTargetConfiguration> TargetConfigurations = ClientTargetConfigurations;
			if (TargetConfigurations == null || TargetConfigurations.Count < 1)
			{
				// No client target configurations, add all in
				TargetConfigurations = new List<UnrealTargetConfiguration>();
				foreach (UnrealTargetConfiguration TargetConfigurationType in Enum.GetValues(typeof(UnrealTargetConfiguration)))
				{
					if (TargetConfigurationType != UnrealTargetConfiguration.Unknown)
					{
						TargetConfigurations.Add(TargetConfigurationType);
					}
				}
			}

			// Read the project descriptor, and find all the plugins available to this project
			ProjectDescriptor Project = ProjectDescriptor.FromFile(RawProjectPath);
			List<PluginInfo> AvailablePlugins = Plugins.ReadAvailablePlugins(CommandUtils.EngineDirectory, RawProjectPath, Project.AdditionalPluginDirectories);

			// check the target platforms for any differences in build settings or additional plugins
			foreach (UnrealTargetPlatform TargetPlatformType in TargetPlatforms)
			{
				if(!CommandUtils.IsEngineInstalled() && !PlatformExports.HasDefaultBuildConfig(RawProjectPath, TargetPlatformType))
				{
					Reason = "project has non-default build configuration";
					return true;
				}
				if(PlatformExports.RequiresBuild(RawProjectPath, TargetPlatformType))
				{
					Reason = "overriden by target platform";
					return true;
				}

				// find if there are any plugins enabled or disabled which differ from the default
				foreach(PluginInfo Plugin in AvailablePlugins)
				{
					bool bPluginEnabledForTarget = false;
					foreach (UnrealTargetConfiguration TargetConfigType in TargetConfigurations)
					{
						bPluginEnabledForTarget |= Plugins.IsPluginEnabledForProject(Plugin, Project, TargetPlatformType, TargetConfigType, TargetRules.TargetType.Game);
					}

					bool bPluginEnabledForBaseTarget = false;
					if(!Plugin.Descriptor.bInstalled)
					{
						foreach (UnrealTargetConfiguration TargetConfigType in TargetConfigurations)
						{
							bPluginEnabledForBaseTarget |= Plugins.IsPluginEnabledForProject(Plugin, null, TargetPlatformType, TargetConfigType, TargetRules.TargetType.Game);
						}
					}

					if (bPluginEnabledForTarget != bPluginEnabledForBaseTarget)
					{
						if(bPluginEnabledForTarget)
						{
							Reason = String.Format("{0} plugin is enabled", Plugin.Name);
							return true;
						}
						else
						{
							Reason = String.Format("{0} plugin is disabled", Plugin.Name);
							return true;
						}
					}
				}
			}

			Reason = null;
			return false;
		}

		private static void GenerateTempTarget(FileReference RawProjectPath)
		{
			DirectoryReference TempDir = DirectoryReference.Combine(RawProjectPath.Directory, "Intermediate", "Source");
			DirectoryReference.CreateDirectory(TempDir);

			// Get the project name for use in temporary files
			string ProjectName = RawProjectPath.GetFileNameWithoutExtension();

			// Create a target.cs file
			MemoryStream TargetStream = new MemoryStream();
			using (StreamWriter Writer = new StreamWriter(TargetStream))
			{
				Writer.WriteLine("using UnrealBuildTool;");
				Writer.WriteLine();
				Writer.WriteLine("public class {0}Target : TargetRules", ProjectName);
				Writer.WriteLine("{");
				Writer.WriteLine("\tpublic {0}Target(TargetInfo Target) : base(Target)", ProjectName);
				Writer.WriteLine("\t{");
				Writer.WriteLine("\t\tType = TargetType.Game;");
				Writer.WriteLine("\t\tExtraModuleNames.Add(\"{0}\");", ProjectName);
				Writer.WriteLine("\t}");
				Writer.WriteLine("}");
			}
			FileReference TargetLocation = FileReference.Combine(TempDir, ProjectName + ".Target.cs");
			FileReference.WriteAllBytesIfDifferent(TargetLocation, TargetStream.ToArray());

			// Create a build.cs file
			MemoryStream ModuleStream = new MemoryStream();
			using (StreamWriter Writer = new StreamWriter(ModuleStream))
			{
				Writer.WriteLine("using UnrealBuildTool;");
				Writer.WriteLine();
				Writer.WriteLine("public class {0} : ModuleRules", ProjectName);
				Writer.WriteLine("{");
				Writer.WriteLine("\tpublic {0}(ReadOnlyTargetRules Target) : base(Target)", ProjectName);
				Writer.WriteLine("\t{");
				Writer.WriteLine("\t\tPCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;");
				Writer.WriteLine();
				Writer.WriteLine("\t\tPrivateDependencyModuleNames.Add(\"Core\");");
				Writer.WriteLine("\t\tPrivateDependencyModuleNames.Add(\"Core\");");
				Writer.WriteLine("\t}");
				Writer.WriteLine("}");
			}
			FileReference ModuleLocation = FileReference.Combine(TempDir, ProjectName + ".Build.cs");
			FileReference.WriteAllBytesIfDifferent(ModuleLocation, ModuleStream.ToArray());

			// Create a main module cpp file
			MemoryStream SourceFileStream = new MemoryStream();
			using (StreamWriter Writer = new StreamWriter(SourceFileStream))
			{
				Writer.WriteLine("#include \"CoreTypes.h\"");
				Writer.WriteLine("#include \"Modules/ModuleManager.h\"");
				Writer.WriteLine();
				Writer.WriteLine("IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultModuleImpl, {0}, \"{0}\");", ProjectName);
			}
			FileReference SourceFileLocation = FileReference.Combine(TempDir, ProjectName + ".cpp");
			FileReference.WriteAllBytesIfDifferent(SourceFileLocation, SourceFileStream.ToArray());
		}

		/// <summary>
		/// Attempts to autodetect project properties.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>Project properties.</returns>
        private static ProjectProperties DetectProjectProperties(FileReference RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms, List<UnrealTargetConfiguration> ClientTargetConfigurations, bool AssetNativizationRequested)
		{
			ProjectProperties Properties = new ProjectProperties();
			Properties.RawProjectPath = RawProjectPath;

			// detect if the project is content only, but has non-default build settings
			List<string> ExtraSearchPaths = null;
			if (RawProjectPath != null)
			{
                string TempTargetDir = CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath.FullName), "Intermediate", "Source");
                if (RequiresTempTarget(RawProjectPath, ClientTargetPlatforms, ClientTargetConfigurations, AssetNativizationRequested))
				{
					GenerateTempTarget(RawProjectPath);
					Properties.bWasGenerated = true;
					ExtraSearchPaths = new List<string>();
                    ExtraSearchPaths.Add(TempTargetDir);
				}
				else if (File.Exists(Path.Combine(Path.GetDirectoryName(RawProjectPath.FullName), "Intermediate", "Source", Path.GetFileNameWithoutExtension(RawProjectPath.FullName) + ".Target.cs")))
				{
					File.Delete(Path.Combine(Path.GetDirectoryName(RawProjectPath.FullName), "Intermediate", "Source", Path.GetFileNameWithoutExtension(RawProjectPath.FullName) + ".Target.cs"));
				}

                // in case the RulesCompiler (what we use to find all the 
                // Target.cs files) has already cached the contents of this 
                // directory, then we need to invalidate that cache (so 
                // it'll find/use the new Target.cs file)
                RulesCompiler.InvalidateRulesFileCache(TempTargetDir);
            }

			if (CommandUtils.CmdEnv.HasCapabilityToCompile)
			{
				DetectTargetsForProject(Properties, ExtraSearchPaths);
				Properties.bIsCodeBasedProject = !CommandUtils.IsNullOrEmpty(Properties.Targets) || !CommandUtils.IsNullOrEmpty(Properties.Programs);
			}
			else
			{
				// should never ask for engine targets if we can't compile
				if (RawProjectPath == null)
				{
					throw new AutomationException("Cannot determine engine targets if we can't compile.");
				}

				Properties.bIsCodeBasedProject = Properties.bWasGenerated;
				// if there's a Source directory with source code in it, then mark us as having source code
				string SourceDir = CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath.FullName), "Source");
				if (Directory.Exists(SourceDir))
				{
					string[] CppFiles = Directory.GetFiles(SourceDir, "*.cpp", SearchOption.AllDirectories);
					string[] HFiles = Directory.GetFiles(SourceDir, "*.h", SearchOption.AllDirectories);
					Properties.bIsCodeBasedProject |= (CppFiles.Length > 0 || HFiles.Length > 0);
				}
			}

			// check to see if the uproject loads modules, only if we haven't already determined it is a code based project
			if (!Properties.bIsCodeBasedProject && RawProjectPath != null)
			{
				string uprojectStr = File.ReadAllText(RawProjectPath.FullName);
				Properties.bIsCodeBasedProject = uprojectStr.Contains("\"Modules\"");
			}

			// Get all ini files
			if (RawProjectPath != null)
			{
				CommandUtils.LogVerbose("Loading ini files for {0}", RawProjectPath);

				foreach (UnrealTargetPlatform TargetPlatformType in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (TargetPlatformType != UnrealTargetPlatform.Unknown)
					{
						ConfigHierarchy Config = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, RawProjectPath.Directory, TargetPlatformType);
						Properties.EngineConfigs.Add(TargetPlatformType, Config);
					}
				}

				foreach (UnrealTargetPlatform TargetPlatformType in Enum.GetValues(typeof(UnrealTargetPlatform)))
				{
					if (TargetPlatformType != UnrealTargetPlatform.Unknown)
					{
						ConfigHierarchy Config = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, RawProjectPath.Directory, TargetPlatformType);
						Properties.GameConfigs.Add(TargetPlatformType, Config);
					}
				}
			}

			return Properties;
		}


		/// <summary>
		/// Gets the project's root binaries folder.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="TargetType">Target type.</param>
		/// <param name="bIsUProjectFile">True if uproject file.</param>
		/// <returns>Binaries path.</returns>
		public static DirectoryReference GetClientProjectBinariesRootPath(FileReference RawProjectPath, TargetType TargetType, bool bIsCodeBasedProject)
		{
			DirectoryReference BinPath = null;
			switch (TargetType)
			{
				case TargetType.Program:
					BinPath = DirectoryReference.Combine(CommandUtils.RootDirectory, "Engine", "Binaries");
					break;
				case TargetType.Client:
				case TargetType.Game:
					if (!bIsCodeBasedProject)
					{
						BinPath = DirectoryReference.Combine(CommandUtils.RootDirectory, "Engine", "Binaries");
					}
					else
					{
						BinPath = DirectoryReference.Combine(RawProjectPath.Directory, "Binaries");
					}
					break;
			}
			return BinPath;
		}

		/// <summary>
		/// Gets the location where all rules assemblies should go
		/// </summary>
		private static string GetRulesAssemblyFolder()
		{
			string RulesFolder;
			if (CommandUtils.IsEngineInstalled())
			{
				RulesFolder = CommandUtils.CombinePaths(Path.GetTempPath(), "UAT", CommandUtils.EscapePath(CommandUtils.CmdEnv.LocalRoot), "Rules"); 
			}
			else
			{
				RulesFolder = CommandUtils.CombinePaths(CommandUtils.CmdEnv.EngineSavedFolder, "Rules");
			}
			return RulesFolder;
		}

		/// <summary>
		/// Finds all targets for the project.
		/// </summary>
		/// <param name="Properties">Project properties.</param>
		/// <param name="ExtraSearchPaths">Additional search paths.</param>
		private static void DetectTargetsForProject(ProjectProperties Properties, List<string> ExtraSearchPaths = null)
		{
			Properties.Targets = new List<SingleTargetProperties>();
			FileReference TargetsDllFilename;
			string FullProjectPath = null;

			List<DirectoryReference> GameFolders = new List<DirectoryReference>();
			DirectoryReference RulesFolder = new DirectoryReference(GetRulesAssemblyFolder());
			if (Properties.RawProjectPath != null)
			{
				CommandUtils.LogVerbose("Looking for targets for project {0}", Properties.RawProjectPath);

				TargetsDllFilename = FileReference.Combine(RulesFolder, String.Format("UATRules{0}.dll", Properties.RawProjectPath.GetHashCode()));

				FullProjectPath = CommandUtils.GetDirectoryName(Properties.RawProjectPath.FullName);
				GameFolders.Add(new DirectoryReference(FullProjectPath));
				CommandUtils.LogVerbose("Searching for target rule files in {0}", FullProjectPath);
			}
			else
			{
				TargetsDllFilename = FileReference.Combine(RulesFolder, String.Format("UATRules{0}.dll", "_BaseEngine_"));
			}

			// the UBT code assumes a certain CWD, but artists don't have this CWD.
			string SourceDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source");
			bool DirPushed = false;
			if (CommandUtils.DirectoryExists_NoExceptions(SourceDir))
			{
				CommandUtils.PushDir(SourceDir);
				DirPushed = true;
			}
			List<DirectoryReference> ExtraSearchDirectories = (ExtraSearchPaths == null)? null : ExtraSearchPaths.Select(x => new DirectoryReference(x)).ToList();
			List<FileReference> TargetScripts = RulesCompiler.FindAllRulesSourceFiles(RulesCompiler.RulesFileType.Target, GameFolders: GameFolders, ForeignPlugins: null, AdditionalSearchPaths: ExtraSearchDirectories, bIncludeEnterprise: false);
			if (DirPushed)
			{
				CommandUtils.PopDir();
			}

			if (!CommandUtils.IsNullOrEmpty(TargetScripts))
			{
				// We only care about project target script so filter out any scripts not in the project folder, or take them all if we are just doing engine stuff
				List<FileReference> ProjectTargetScripts = new List<FileReference>();
				foreach (FileReference TargetScript in TargetScripts)
				{
					if (FullProjectPath == null || TargetScript.IsUnderDirectory(new DirectoryReference(FullProjectPath)))
					{
						ProjectTargetScripts.Add(TargetScript);
					}
				}
				TargetScripts = ProjectTargetScripts;
			}

			if (!CommandUtils.IsNullOrEmpty(TargetScripts))
			{
				CommandUtils.LogVerbose("Found {0} target rule files:", TargetScripts.Count);
				foreach (FileReference Filename in TargetScripts)
				{
					CommandUtils.LogVerbose("  {0}", Filename);
				}

				// Check if the scripts require compilation
				bool DoNotCompile = false;
				if (!CommandUtils.IsBuildMachine && !CheckIfScriptAssemblyIsOutOfDate(TargetsDllFilename, TargetScripts))
				{
					Log.TraceVerbose("Targets DLL {0} is up to date.", TargetsDllFilename);
					DoNotCompile = true;
				}
				if (!DoNotCompile && CommandUtils.FileExists_NoExceptions(TargetsDllFilename.FullName))
				{
					if (!CommandUtils.DeleteFile_NoExceptions(TargetsDllFilename.FullName, true))
					{
						DoNotCompile = true;
						CommandUtils.LogVerbose("Could not delete {0} assuming it is up to date and reusable for a recursive UAT call.", TargetsDllFilename);
					}
				}

				CompileAndLoadTargetsAssembly(Properties, TargetsDllFilename, DoNotCompile, TargetScripts);
			}
		}

		/// <summary>
		/// Optionally compiles and loads target rules assembly.
		/// </summary>
		/// <param name="Properties"></param>
		/// <param name="TargetsDllFilename"></param>
		/// <param name="DoNotCompile"></param>
		/// <param name="TargetScripts"></param>
		private static void CompileAndLoadTargetsAssembly(ProjectProperties Properties, FileReference TargetsDllFilename, bool DoNotCompile, List<FileReference> TargetScripts)
		{
			CommandUtils.LogVerbose("Compiling targets DLL: {0}", TargetsDllFilename);

			List<string> ReferencedAssemblies = new List<string>() 
					{ 
						"System.dll", 
						"System.Core.dll", 
						"System.Xml.dll", 
						typeof(UnrealBuildTool.PlatformExports).Assembly.Location
					};
			List<string> PreprocessorDefinitions = RulesAssembly.GetPreprocessorDefinitions();
			Assembly TargetsDLL = DynamicCompilation.CompileAndLoadAssembly(TargetsDllFilename, new HashSet<FileReference>(TargetScripts), ReferencedAssemblies, PreprocessorDefinitions, DoNotCompile);
			Type[] AllCompiledTypes = TargetsDLL.GetTypes();
			foreach (Type TargetType in AllCompiledTypes)
			{
				// Find TargetRules but skip all "UE4Editor", "UE4Game" targets.
				if (typeof(TargetRules).IsAssignableFrom(TargetType) && !TargetType.IsAbstract)
				{
					string TargetName = GetTargetName(TargetType);

					TargetInfo DummyTargetInfo = new TargetInfo(TargetName, BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development, "", Properties.RawProjectPath);

					// Create an instance of this type
					CommandUtils.LogVerbose("Creating target rules object: {0}", TargetType.Name);
					TargetRules Rules = Activator.CreateInstance(TargetType, DummyTargetInfo) as TargetRules;
					CommandUtils.LogVerbose("Adding target: {0} ({1})", TargetType.Name, Rules.Type);

					SingleTargetProperties TargetData;
					TargetData.TargetName = GetTargetName(TargetType);
					TargetData.Rules = Rules;
					if (Rules.Type == global::UnrealBuildTool.TargetType.Program)
					{
						Properties.Programs.Add(TargetData);
					}
					else
					{
						Properties.Targets.Add(TargetData);
					}
				}
			}
		}

		/// <summary>
		/// Checks if any of the script files in newer than the generated assembly.
		/// </summary>
		/// <param name="TargetsDllFilename"></param>
		/// <param name="TargetScripts"></param>
		/// <returns>True if the generated assembly is out of date.</returns>
		private static bool CheckIfScriptAssemblyIsOutOfDate(FileReference TargetsDllFilename, List<FileReference> TargetScripts)
		{
			bool bOutOfDate = false;
			FileInfo AssemblyInfo = new FileInfo(TargetsDllFilename.FullName);
			if (AssemblyInfo.Exists)
			{
				foreach (FileReference ScriptFilename in TargetScripts)
				{
					FileInfo ScriptInfo = new FileInfo(ScriptFilename.FullName);
					if (ScriptInfo.Exists && ScriptInfo.LastWriteTimeUtc > AssemblyInfo.LastWriteTimeUtc)
					{
						bOutOfDate = true;
						break;
					}
				}
			}
			else
			{
				bOutOfDate = true;
			}
			return bOutOfDate;
		}

		/// <summary>
		/// Converts class type name (usually ends with Target) to a target name (without the postfix).
		/// </summary>
		/// <param name="TargetRulesType">Tagert class.</param>
		/// <returns>Target name</returns>
		private static string GetTargetName(Type TargetRulesType)
		{
			const string TargetPostfix = "Target";
			string Name = TargetRulesType.Name;
			if (Name.EndsWith(TargetPostfix, StringComparison.InvariantCultureIgnoreCase))
			{
				Name = Name.Substring(0, Name.Length - TargetPostfix.Length);
			}
			return Name;
		}

		/// <summary>
		/// Performs initial cleanup of target rules folder
		/// </summary>
		public static void CleanupFolders()
		{
			CommandUtils.LogVerbose("Cleaning up project rules folder");
			string RulesFolder = GetRulesAssemblyFolder();
			if (CommandUtils.DirectoryExists(RulesFolder))
			{
				CommandUtils.DeleteDirectoryContents(RulesFolder);
			}
		}
	}

    public class BranchInfo
    {

        public static List<TargetType> MonolithicKinds = new List<TargetType>
        {
            TargetType.Game,
            TargetType.Client,
            TargetType.Server,
        };

		[DebuggerDisplay("{GameName}")]
        public class BranchUProject
        {
            public string GameName;
            public FileReference FilePath;
            public ProjectProperties Properties;

            public BranchUProject(FileReference ProjectFile)
            {
                GameName = ProjectFile.GetFileNameWithoutExtension();

                //not sure what the heck this path is relative to
                FilePath = ProjectFile;

                if (!CommandUtils.FileExists_NoExceptions(FilePath.FullName))
                {
                    throw new AutomationException("Could not resolve relative path corrctly {0} -> {1} which doesn't exist.", ProjectFile, FilePath);
                }

                Properties = ProjectUtils.GetProjectProperties(FilePath);



            }
            public BranchUProject()
            {
                GameName = "UE4";
                Properties = ProjectUtils.GetProjectProperties(null);
                if (!Properties.Targets.Exists(Target => Target.Rules.Type == TargetType.Editor))
                {
                    throw new AutomationException("Base UE4 project did not contain an editor target.");
                }
            }
            public void Dump(List<UnrealTargetPlatform> InHostPlatforms)
            {
                CommandUtils.LogVerbose("    ShortName:    " + GameName);
				CommandUtils.LogVerbose("      FilePath          : " + FilePath);
				CommandUtils.LogVerbose("      bIsCodeBasedProject  : " + (Properties.bIsCodeBasedProject ? "YES" : "NO"));
                foreach (UnrealTargetPlatform HostPlatform in InHostPlatforms)
                {
					CommandUtils.LogVerbose("      For Host : " + HostPlatform.ToString());
					CommandUtils.LogVerbose("          Targets {0}:", Properties.Targets.Count);
                    foreach (SingleTargetProperties ThisTarget in Properties.Targets)
                    {
						CommandUtils.LogVerbose("            TargetName          : " + ThisTarget.TargetName);
						CommandUtils.LogVerbose("              Type          : " + ThisTarget.Rules.Type);
						CommandUtils.LogVerbose("              bUsesSteam  : " + (ThisTarget.Rules.bUsesSteam ? "YES" : "NO"));
						CommandUtils.LogVerbose("              bUsesCEF3   : " + (ThisTarget.Rules.bUsesCEF3 ? "YES" : "NO"));
						CommandUtils.LogVerbose("              bUsesSlate  : " + (ThisTarget.Rules.bUsesSlate ? "YES" : "NO"));
                    }
					CommandUtils.LogVerbose("      Programs {0}:", Properties.Programs.Count);
                    foreach (SingleTargetProperties ThisTarget in Properties.Programs)
                    {
						CommandUtils.LogVerbose("            TargetName                    : " + ThisTarget.TargetName);
                    }
                }
            }
        };
        public BranchUProject BaseEngineProject = null;
        public List<BranchUProject> CodeProjects = new List<BranchUProject>();
        public List<BranchUProject> NonCodeProjects = new List<BranchUProject>();

		public IEnumerable<BranchUProject> AllProjects
		{
			get { return CodeProjects.Union(NonCodeProjects); }
		}

        public BranchInfo(List<UnrealTargetPlatform> InHostPlatforms)
        {
            BaseEngineProject = new BranchUProject();

            IEnumerable<FileReference> AllProjects = UnrealBuildTool.NativeProjects.EnumerateProjectFiles();
			using(TelemetryStopwatch SortProjectsStopwatch = new TelemetryStopwatch("SortProjects"))
			{
				foreach (FileReference InfoEntry in AllProjects)
				{
					BranchUProject UProject = new BranchUProject(InfoEntry);
					if (UProject.Properties.bIsCodeBasedProject)
					{
						CodeProjects.Add(UProject);
					}
					else
					{
						NonCodeProjects.Add(UProject);
						// the base project uses BlankProject if it really needs a .uproject file
						if (BaseEngineProject.FilePath == null && UProject.GameName == "BlankProject")
						{
							BaseEngineProject.FilePath = UProject.FilePath;
						}
					}
				}
			}
 /*           if (String.IsNullOrEmpty(BaseEngineProject.FilePath))
            {
                throw new AutomationException("All branches must have the blank project /Samples/Sandbox/BlankProject");
            }*/

			using(TelemetryStopwatch ProjectDumpStopwatch = new TelemetryStopwatch("Project Dump"))
			{
				CommandUtils.LogVerbose("  Base Engine:");
				BaseEngineProject.Dump(InHostPlatforms);

				CommandUtils.LogVerbose("  {0} Code projects:", CodeProjects.Count);
				foreach (BranchUProject Proj in CodeProjects)
				{
					Proj.Dump(InHostPlatforms);
				}
				CommandUtils.LogVerbose("  {0} Non-Code projects:", NonCodeProjects.Count);
				foreach (BranchUProject Proj in NonCodeProjects)
				{
					Proj.Dump(InHostPlatforms);
				}
			}
        }

        public BranchUProject FindGame(string GameName)
        {
            foreach (BranchUProject Proj in CodeProjects)
            {
                if (Proj.GameName.Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
                {
                    return Proj;
                }
            }
            foreach (BranchUProject Proj in NonCodeProjects)
            {
                if (Proj.GameName.Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
                {
                    return Proj;
                }
            }
            return null;
        }
		public BranchUProject FindGameChecked(string GameName)
		{
			BranchUProject Project = FindGame(GameName);
			if(Project == null)
			{
				throw new AutomationException("Cannot find project '{0}' in branch", GameName);
			}
			return Project;
		}
        public SingleTargetProperties FindProgram(string ProgramName)
        {
            foreach (SingleTargetProperties Proj in BaseEngineProject.Properties.Programs)
            {
                if (Proj.TargetName.Equals(ProgramName, StringComparison.InvariantCultureIgnoreCase))
                {
                    return Proj;
                }
            }

			foreach (BranchUProject CodeProj in CodeProjects)
			{
				foreach (SingleTargetProperties Proj in CodeProj.Properties.Programs)
				{
					if (Proj.TargetName.Equals(ProgramName, StringComparison.InvariantCultureIgnoreCase))
					{
						return Proj;
					}
				}
			}

            SingleTargetProperties Result;
            Result.TargetName = ProgramName;
            Result.Rules = null;
            return Result;
        }
    };

}
