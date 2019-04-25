// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Runtime.Serialization;
using System.Text.RegularExpressions;
using Tools.DotNETCommon;
using System.Reflection;

namespace UnrealBuildTool
{
	/// <summary>
	/// The platform we're building for
	/// </summary>
	public enum UnrealTargetPlatform
	{
		/// <summary>
		/// Unknown target platform
		/// </summary>
		Unknown,

		/// <summary>
		/// 32-bit Windows
		/// </summary>
		Win32,

		/// <summary>
		/// 64-bit Windows
		/// </summary>
		Win64,

		/// <summary>
		/// Mac
		/// </summary>
		Mac,

		/// <summary>
		/// XboxOne
		/// </summary>
		XboxOne,

		/// <summary>
		/// Playstation 4
		/// </summary>
		PS4,

		/// <summary>
		/// iOS
		/// </summary>
		IOS,

		/// <summary>
		/// Android
		/// </summary>
		Android,

		/// <summary>
		/// HTML5
		/// </summary>
		HTML5,

		/// <summary>
		/// Linux
		/// </summary>
		Linux,

		/// <summary>
		/// All desktop platforms
		/// </summary>
		AllDesktop,

		/// <summary>
		/// TVOS
		/// </summary>
		TVOS,

		/// <summary>
		/// Nintendo Switch
		/// </summary>
		Switch,

		/// <summary>
		/// NDA'd platform Quail
		/// </summary>
		Quail,

		/// <summary>
		/// Confidential platform
		/// </summary>
		Lumin,
	}

	/// <summary>
	/// Platform groups
	/// </summary>
	public enum UnrealPlatformGroup
	{
		/// <summary>
		/// this group is just to lump Win32 and Win64 into Windows directories, removing the special Windows logic in MakeListOfUnsupportedPlatforms
		/// </summary>
		Windows,

		/// <summary>
		/// Microsoft platforms
		/// </summary>
		Microsoft,

		/// <summary>
		/// Apple platforms
		/// </summary>
		Apple,

		/// <summary>
		/// making IOS a group allows TVOS to compile IOS code
		/// </summary>
		IOS,

		/// <summary>
		/// Unix platforms
		/// </summary>
		Unix,

		/// <summary>
		/// Android platforms
		/// </summary>
		Android,

		/// <summary>
		/// Sony platforms
		/// </summary>
		Sony,

		/// <summary>
		/// Target all desktop platforms (Win64, Mac, Linux) simultaneously
		/// </summary>
		AllDesktop,
	}

	/// <summary>
	/// The class of platform. See Utils.GetPlatformsInClass().
	/// </summary>
	public enum UnrealPlatformClass
	{
		/// <summary>
		/// All platforms
		/// </summary>
		All,

		/// <summary>
		/// All desktop platforms (Win32, Win64, Mac, Linux)
		/// </summary>
		Desktop,

		/// <summary>
		/// All platforms which support the editor (Win64, Mac, Linux)
		/// </summary>
		Editor,

		/// <summary>
		/// Platforms which support running servers (Win32, Win64, Mac, Linux)
		/// </summary>
		Server,
	}

	/// <summary>
	/// The type of configuration a target can be built for
	/// </summary>
	public enum UnrealTargetConfiguration
	{
		/// <summary>
		/// Unknown
		/// </summary>
		Unknown,

		/// <summary>
		/// Debug configuration
		/// </summary>
		Debug,

		/// <summary>
		/// DebugGame configuration; equivalent to development, but with optimization disabled for game modules
		/// </summary>
		DebugGame,

		/// <summary>
		/// Development configuration
		/// </summary>
		Development,

		/// <summary>
		/// Shipping configuration
		/// </summary>
		Shipping,

		/// <summary>
		/// Test configuration
		/// </summary>
		Test,
	}

	/// <summary>
	/// A container for a binary files (dll, exe) with its associated debug info.
	/// </summary>
	public class BuildManifest
	{
		/// <summary>
		/// 
		/// </summary>
		public readonly List<string> BuildProducts = new List<string>();

		/// <summary>
		/// 
		/// </summary>
		public readonly List<string> DeployTargetFiles = new List<string>();

		/// <summary>
		/// 
		/// </summary>
		public BuildManifest()
		{
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="FileName"></param>
		public void AddBuildProduct(string FileName)
		{
			string FullFileName = Path.GetFullPath(FileName);
			if (!BuildProducts.Contains(FullFileName))
			{
				BuildProducts.Add(FullFileName);
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="FileName"></param>
		/// <param name="DebugInfoExtension"></param>
		public void AddBuildProduct(string FileName, string DebugInfoExtension)
		{
			AddBuildProduct(FileName);
			if (!String.IsNullOrEmpty(DebugInfoExtension))
			{
				AddBuildProduct(Path.ChangeExtension(FileName, DebugInfoExtension));
			}
		}
	}

	/// <summary>
	/// A target that can be built
	/// </summary>
	class UEBuildTarget
	{
		/// <summary>
		/// Creates a target object for the specified target name.
		/// </summary>
		/// <param name="Descriptor">Information about the target</param>
		/// <param name="bSkipRulesCompile">Whether to skip compiling any rules assemblies</param>
		/// <param name="bUsePrecompiled">Whether to use a precompiled engine/enterprise build</param>
		/// <returns>The build target object for the specified build rules source file</returns>
		public static UEBuildTarget Create(TargetDescriptor Descriptor, bool bSkipRulesCompile, bool bUsePrecompiled)
		{
			RulesAssembly RulesAssembly;
			using(Timeline.ScopeEvent("RulesCompiler.CreateTargetRulesAssembly()"))
			{
				RulesAssembly = RulesCompiler.CreateTargetRulesAssembly(Descriptor.ProjectFile, Descriptor.Name, bSkipRulesCompile, bUsePrecompiled, Descriptor.ForeignPlugin);
			}
	
			TargetRules RulesObject;
			using(Timeline.ScopeEvent("RulesAssembly.CreateTargetRules()"))
			{
				RulesObject = RulesAssembly.CreateTargetRules(Descriptor.Name, Descriptor.Platform, Descriptor.Configuration, Descriptor.Architecture, Descriptor.ProjectFile, Descriptor.AdditionalArguments);
			}
			if ((ProjectFileGenerator.bGenerateProjectFiles == false) && !RulesObject.GetSupportedPlatforms().Contains(Descriptor.Platform))
			{
				throw new BuildException("{0} does not support the {1} platform.", Descriptor.Name, Descriptor.Platform.ToString());
			}

			// If we're using the shared build environment, make sure all the settings are valid
			if(RulesObject.BuildEnvironment == TargetBuildEnvironment.Shared)
			{
				ValidateSharedEnvironment(RulesAssembly, Descriptor.Name, RulesObject);
			}

			// If we're precompiling, generate a list of all the files that we depend on
			if (RulesObject.bPrecompile)
			{
				DirectoryReference DependencyListDir;
				if(RulesObject.ProjectFile == null)
				{
					DependencyListDir = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Intermediate", "DependencyLists", RulesObject.Name, RulesObject.Configuration.ToString(), RulesObject.Platform.ToString());
				}
				else
				{
					DependencyListDir = DirectoryReference.Combine(RulesObject.ProjectFile.Directory, "Intermediate", "DependencyLists", RulesObject.Name, RulesObject.Configuration.ToString(), RulesObject.Platform.ToString());
				}

				FileReference DependencyListFile;
				if(RulesObject.bBuildAllModules)
				{
					DependencyListFile = FileReference.Combine(DependencyListDir, "DependencyList-AllModules.txt");
				}
				else
				{
					DependencyListFile = FileReference.Combine(DependencyListDir, "DependencyList.txt");
				}

				RulesObject.DependencyListFileNames.Add(DependencyListFile);
			}

			// If we're compiling just a single file, we need to prevent unity builds from running
			if(Descriptor.SingleFileToCompile != null)
			{
				RulesObject.bUseUnityBuild = false;
				RulesObject.bForceUnityBuild = false;
				RulesObject.bUsePCHFiles = false;
				RulesObject.bDisableLinking = true;
			}

			// If we're compiling a plugin, and this target is monolithic, just create the object files
			if(Descriptor.ForeignPlugin != null && RulesObject.LinkType == TargetLinkType.Monolithic)
			{
				// Don't actually want an executable
				RulesObject.bDisableLinking = true;

				// Don't allow using shared PCHs; they won't be distributed with the plugin
				RulesObject.bUseSharedPCHs = false;
			}

			// Include generated code plugin if not building an editor target and project is configured for nativization
			FileReference NativizedPluginFile = RulesObject.GetNativizedPlugin();
			if(NativizedPluginFile != null)
			{
				RulesAssembly = RulesCompiler.CreatePluginRulesAssembly(NativizedPluginFile, bSkipRulesCompile, RulesAssembly, false);
			}

			// Generate a build target from this rules module
			UEBuildTarget Target;
			using(Timeline.ScopeEvent("UEBuildTarget constructor"))
			{
				Target = new UEBuildTarget(Descriptor, new ReadOnlyTargetRules(RulesObject), RulesAssembly);
			}
			using(Timeline.ScopeEvent("UEBuildTarget.PreBuildSetup()"))
			{
				Target.PreBuildSetup();
			}

			// If we're just compiling a single file, filter the list of binaries to only include the file we're interested in.
			FileReference SingleFileToCompile = Descriptor.SingleFileToCompile;
			if (SingleFileToCompile != null)
			{
				Target.Binaries.RemoveAll(x => !x.Modules.Any(y => SingleFileToCompile.IsUnderDirectory(y.ModuleDirectory)));
				if(Target.Binaries.Count == 0)
				{
					throw new BuildException("Couldn't find any module containing {0} in {1}.", SingleFileToCompile, Target.TargetName);
				}
			}

			return Target;
		}

		/// <summary>
		/// Validates that the build environment matches the shared build environment, by comparing the TargetRules instance to the vanilla target rules for the current target type.
		/// </summary>
		static void ValidateSharedEnvironment(RulesAssembly RulesAssembly, string ThisTargetName, TargetRules ThisRules)
		{
			// Get the name of the target with default settings
			string BaseTargetName;
			switch(ThisRules.Type)
			{
				case TargetType.Game:
					BaseTargetName = "UE4Game";
					break;
				case TargetType.Editor:
					BaseTargetName = "UE4Editor";
					break;
				case TargetType.Client:
					BaseTargetName = "UE4Client";
					break;
				case TargetType.Server:
					BaseTargetName = "UE4Server";
					break;
				default:
					return;
			}

			// Create the target rules for it
			TargetRules BaseRules = RulesAssembly.CreateTargetRules(BaseTargetName, ThisRules.Platform, ThisRules.Configuration, ThisRules.Architecture, null, null);

			// Get all the configurable objects
			object[] BaseObjects = BaseRules.GetConfigurableObjects().ToArray();
			object[] ThisObjects = ThisRules.GetConfigurableObjects().ToArray();
			if(BaseObjects.Length != ThisObjects.Length)
			{
				throw new BuildException("Expected same number of configurable objects from base rules object.");
			}

			// Iterate through all fields with the [SharedBuildEnvironment] attribute
			for(int Idx = 0; Idx < BaseObjects.Length; Idx++)
			{
				Type ObjectType = BaseObjects[Idx].GetType();
				foreach(FieldInfo Field in ObjectType.GetFields())
				{
					if(Field.GetCustomAttribute<RequiresUniqueBuildEnvironmentAttribute>() != null)
					{
						object ThisValue = Field.GetValue(ThisObjects[Idx]);
						object BaseValue = Field.GetValue(BaseObjects[Idx]);
						CheckValuesMatch(ThisRules.GetType(), ThisTargetName, BaseTargetName, Field.Name, Field.FieldType, ThisValue, BaseValue);
					}
				}
				foreach(PropertyInfo Property in ObjectType.GetProperties())
				{
					if(Property.GetCustomAttribute<RequiresUniqueBuildEnvironmentAttribute>() != null)
					{
						object ThisValue = Property.GetValue(ThisObjects[Idx]);
						object BaseValue = Property.GetValue(BaseObjects[Idx]);
						CheckValuesMatch(ThisRules.GetType(), ThisTargetName, BaseTargetName, Property.Name, Property.PropertyType, ThisValue, BaseValue);
					}
				}
			}

			// Make sure that we don't explicitly enable or disable any plugins through the target rules. We can't do this with the shared build environment because it requires recompiling the "Projects" engine module.
			if(ThisRules.EnablePlugins.Count > 0 || ThisRules.DisablePlugins.Count > 0)
			{
				throw new BuildException("Explicitly enabling and disabling plugins for a target is only supported when using a unique build environment (eg. for monolithic game targets).");
			}
		}

		/// <summary>
		/// Check that two values match between a base and derived rules type
		/// </summary>
		static void CheckValuesMatch(Type RulesType, string ThisTargetName, string BaseTargetName, string FieldName, Type ValueType, object ThisValue, object BaseValue)
		{
			// Check if the fields match, treating lists of strings (eg. definitions) differently to value types.
			bool bFieldsMatch;
			if(ThisValue == null || BaseValue == null)
			{
				bFieldsMatch = (ThisValue == BaseValue);
			}
			else if(typeof(IEnumerable<string>).IsAssignableFrom(ValueType))
			{
				bFieldsMatch = Enumerable.SequenceEqual((IEnumerable<string>)ThisValue, (IEnumerable<string>)BaseValue);
			}
			else
			{
				bFieldsMatch = ThisValue.Equals(BaseValue);
			}

			// Throw an exception if they don't match
			if(!bFieldsMatch)
			{
				throw new BuildException("{0} modifies the value of {1}. This is not allowed, as {0} has build products in common with {2}.\nRemove the modified setting or change {0} to use a unique build environment by setting 'BuildEnvironment = TargetBuildEnvironment.Unique;' in the {3} constructor.", ThisTargetName, FieldName, BaseTargetName, RulesType.Name);
			}
		}

		/// <summary>
		/// The target rules
		/// </summary>
		public ReadOnlyTargetRules Rules;

		/// <summary>
		/// The rules assembly to use when searching for modules
		/// </summary>
		public RulesAssembly RulesAssembly;

		/// <summary>
		/// Cache of source file metadata for this target
		/// </summary>
		public SourceFileMetadataCache MetadataCache;

		/// <summary>
		/// The project file for this target
		/// </summary>
		public FileReference ProjectFile;

		/// <summary>
		/// The project descriptor for this target
		/// </summary>
		public ProjectDescriptor ProjectDescriptor;

		/// <summary>
		/// Type of target
		/// </summary>
		public TargetType TargetType;

		/// <summary>
		/// The name of the application the target is part of. For targets with bUseSharedBuildEnvironment = true, this is typically the name of the base application, eg. UE4Editor for any game editor.
		/// </summary>
		public string AppName;

		/// <summary>
		/// The name of the target
		/// </summary>
		public string TargetName;

		/// <summary>
		/// Whether the target uses the shared build environment. If false, AppName==TargetName and all binaries should be written to the project directory.
		/// </summary>
		public bool bUseSharedBuildEnvironment;

		/// <summary>
		/// Platform as defined by the VCProject and passed via the command line. Not the same as internal config names.
		/// </summary>
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// Target as defined by the VCProject and passed via the command line. Not necessarily the same as internal name.
		/// </summary>
		public UnrealTargetConfiguration Configuration;

		/// <summary>
		/// The architecture this target is being built for
		/// </summary>
		public string Architecture;

		/// <summary>
		/// Relative path for platform-specific intermediates (eg. Intermediate/Build/Win64)
		/// </summary>
		public string PlatformIntermediateFolder;

		/// <summary>
		/// Root directory for the active project. Typically contains the .uproject file, or the engine root.
		/// </summary>
		public DirectoryReference ProjectDirectory;

		/// <summary>
		/// Default directory for intermediate files. Typically underneath ProjectDirectory.
		/// </summary>
		public DirectoryReference ProjectIntermediateDirectory;

		/// <summary>
		/// Directory for engine intermediates. For an agnostic editor/game executable, this will be under the engine directory. For monolithic executables this will be the same as the project intermediate directory.
		/// </summary>
		public DirectoryReference EngineIntermediateDirectory;

		/// <summary>
		/// Identifies whether the project contains a script plugin. This will cause UHT to be rebuilt, even in installed builds.
		/// </summary>
		public bool bHasProjectScriptPlugin;

		/// <summary>
		/// All plugins which are built for this target
		/// </summary>
		public List<UEBuildPlugin> BuildPlugins;

		/// <summary>
		/// All plugin dependencies for this target. This differs from the list of plugins that is built for Launcher, where we build everything, but link in only the enabled plugins.
		/// </summary>
		public List<UEBuildPlugin> EnabledPlugins;

		/// <summary>
		/// Specifies the path to a specific plugin to compile.
		/// </summary>
		public FileReference ForeignPlugin;

		/// <summary>
		/// All application binaries; may include binaries not built by this target.
		/// </summary>
		public List<UEBuildBinary> Binaries = new List<UEBuildBinary>();

		/// <summary>
		/// Kept to determine the correct module parsing order when filtering modules.
		/// </summary>
		protected List<UEBuildBinary> NonFilteredModules = new List<UEBuildBinary>();

		/// <summary>
		/// true if target should be compiled in monolithic mode, false if not
		/// </summary>
		protected bool bCompileMonolithic = false;

		/// <summary>
		/// Used to keep track of all modules by name.
		/// </summary>
		private Dictionary<string, UEBuildModule> Modules = new Dictionary<string, UEBuildModule>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Filename for the receipt for this target.
		/// </summary>
		public FileReference ReceiptFileName
		{
			get;
			private set;
		}

		/// <summary>
		/// The name of the .Target.cs file, if the target was created with one
		/// </summary>
		readonly FileReference TargetRulesFile;

		/// <summary>
		/// Whether to deploy this target after compilation
		/// </summary>
		public bool bDeployAfterCompile;

		/// <summary>
		/// Whether this target should be compiled in monolithic mode
		/// </summary>
		/// <returns>true if it should, false if it shouldn't</returns>
		public bool ShouldCompileMonolithic()
		{
			return bCompileMonolithic;	// @todo ubtmake: We need to make sure this function and similar things aren't called in assembler mode
		}

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InDescriptor">Target descriptor</param>
		/// <param name="InRules">The target rules, as created by RulesCompiler.</param>
		/// <param name="InRulesAssembly">The chain of rules assemblies that this target was created with</param>
		private UEBuildTarget(TargetDescriptor InDescriptor, ReadOnlyTargetRules InRules, RulesAssembly InRulesAssembly)
		{
			MetadataCache = SourceFileMetadataCache.CreateHierarchy(InDescriptor.ProjectFile);
			ProjectFile = InDescriptor.ProjectFile;
			AppName = InDescriptor.Name;
			TargetName = InDescriptor.Name;
			Platform = InDescriptor.Platform;
			Configuration = InDescriptor.Configuration;
			Architecture = InDescriptor.Architecture;
			Rules = InRules;
			RulesAssembly = InRulesAssembly;
			TargetType = Rules.Type;
			ForeignPlugin = InDescriptor.ForeignPlugin;
			bDeployAfterCompile = InRules.bDeployAfterCompile && !InRules.bDisableLinking && InDescriptor.SingleFileToCompile == null;

			// now that we have the platform, we can set the intermediate path to include the platform/architecture name
			PlatformIntermediateFolder = GetPlatformIntermediateFolder(Platform, Architecture);

			TargetRulesFile = InRules.File;

			bCompileMonolithic = (Rules.LinkType == TargetLinkType.Monolithic);

			// Set the build environment
			bUseSharedBuildEnvironment = (Rules.BuildEnvironment == TargetBuildEnvironment.Shared);
			if (bUseSharedBuildEnvironment)
			{
				if(Rules.Type == TargetType.Program)
				{
					AppName = TargetName;
				}
				else
				{
					AppName = GetAppNameForTargetType(Rules.Type);
				}
			}

			// Figure out what the project directory is. If we have a uproject file, use that. Otherwise use the engine directory.
			if (ProjectFile != null)
			{
				ProjectDirectory = ProjectFile.Directory;
			}
			else if (Rules.File.IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory))
			{
				ProjectDirectory = UnrealBuildTool.EnterpriseDirectory;
			}
			else
			{
				ProjectDirectory = UnrealBuildTool.EngineDirectory;
			}

			// Build the project intermediate directory
			if(bUseSharedBuildEnvironment && TargetRulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
			{
				ProjectIntermediateDirectory = DirectoryReference.Combine(ProjectDirectory, PlatformIntermediateFolder, AppName, Configuration.ToString());
			}
			else
			{
				ProjectIntermediateDirectory = DirectoryReference.Combine(ProjectDirectory, PlatformIntermediateFolder, TargetName, Configuration.ToString());
			}

			// Build the engine intermediate directory. If we're building agnostic engine binaries, we can use the engine intermediates folder. Otherwise we need to use the project intermediates directory.
			if (!bUseSharedBuildEnvironment)
			{
				EngineIntermediateDirectory = ProjectIntermediateDirectory;
			}
			else if (Configuration == UnrealTargetConfiguration.DebugGame)
			{
				EngineIntermediateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, PlatformIntermediateFolder, AppName, UnrealTargetConfiguration.Development.ToString());
			}
			else
			{
				EngineIntermediateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, PlatformIntermediateFolder, AppName, Configuration.ToString());
			}

			// Get the receipt path for this target
			ReceiptFileName = TargetReceipt.GetDefaultPath(ProjectDirectory, TargetName, Platform, Configuration, Architecture);

			// Read the project descriptor
			if (ProjectFile != null)
			{
				ProjectDescriptor = ProjectDescriptor.FromFile(ProjectFile);
			}
		}

		/// <summary>
		/// Gets the intermediate directory for a given platform
		/// </summary>
		/// <param name="Platform">Platform to get the folder for</param>
		/// <param name="Architecture">Architecture to get the folder for</param>
		/// <returns>The output directory for intermediates</returns>
		public static string GetPlatformIntermediateFolder(UnrealTargetPlatform Platform, string Architecture)
		{
			// now that we have the platform, we can set the intermediate path to include the platform/architecture name
			return Path.Combine("Intermediate", "Build", Platform.ToString(), UEBuildPlatform.GetBuildPlatform(Platform).GetFolderNameForArchitecture(Architecture));
		}

		/// <summary>
		/// Gets the app name for a given target type
		/// </summary>
		/// <param name="Type">The target type</param>
		/// <returns>The app name for this target type</returns>
		public static string GetAppNameForTargetType(TargetType Type)
		{
			switch(Type)
			{
				case TargetType.Game:
					return "UE4";
				case TargetType.Client:
					return "UE4Client";
				case TargetType.Server:
					return "UE4Server";
				case TargetType.Editor:
					return "UE4Editor";
				default:
					throw new BuildException("Invalid target type ({0})", (int)Type);
			}
		}

		/// <summary>
		/// Writes a list of all the externally referenced files required to use the precompiled data for this target
		/// </summary>
		/// <param name="Location">Path to the dependency list</param>
		/// <param name="RuntimeDependencies">List of all the runtime dependencies for this target</param>
		/// <param name="RuntimeDependencyTargetFileToSourceFile">Map of runtime dependencies to their location in the source tree, before they are staged</param>
		void WriteDependencyList(FileReference Location, List<RuntimeDependency> RuntimeDependencies, Dictionary<FileReference, FileReference> RuntimeDependencyTargetFileToSourceFile)
		{
			HashSet<FileReference> Files = new HashSet<FileReference>();

			// Find all the runtime dependency files in their original location
			foreach(RuntimeDependency RuntimeDependency in RuntimeDependencies)
			{
				FileReference SourceFile;
				if(!RuntimeDependencyTargetFileToSourceFile.TryGetValue(RuntimeDependency.Path, out SourceFile))
				{
					SourceFile = RuntimeDependency.Path;
				}
				if(RuntimeDependency.Type != StagedFileType.DebugNonUFS || FileReference.Exists(SourceFile))
				{
					Files.Add(SourceFile);
				}
			}

			// Figure out all the modules referenced by this target. This includes all the modules that are referenced, not just the ones compiled into binaries.
			HashSet<UEBuildModule> Modules = new HashSet<UEBuildModule>();
			foreach (UEBuildBinary Binary in Binaries)
			{
				foreach(UEBuildModule Module in Binary.Modules)
				{
					Modules.Add(Module);
					Modules.UnionWith(Module.GetDependencies(true, true));
				}
			}

			// Get the platform we're building for
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			foreach (UEBuildModule Module in Modules)
			{
				// Skip artificial modules
				if(Module.RulesFile == null)
				{
					continue;
				}

				// Create the module rules
				ModuleRules Rules = CreateModuleRulesAndSetDefaults(Module.Name, "external file list option");

				// Add Additional Bundle Resources for all modules
				foreach (ModuleRules.BundleResource Resource in Rules.AdditionalBundleResources)
				{
					if (Directory.Exists(Resource.ResourcePath))
					{
						Files.UnionWith(DirectoryReference.EnumerateFiles(new DirectoryReference(Resource.ResourcePath), "*", SearchOption.AllDirectories));
					}
					else
					{
						Files.Add(new FileReference(Resource.ResourcePath));
					}
				}

				// Add any zip files from Additional Frameworks
				foreach (ModuleRules.Framework Framework in Rules.PublicAdditionalFrameworks)
				{
					if (!String.IsNullOrEmpty(Framework.ZipPath))
					{
						Files.Add(FileReference.Combine(Module.ModuleDirectory, Framework.ZipPath));
					}
				}

				// Add the rules file itself
				Files.Add(Rules.File);

				// Get a list of all the library paths
				List<string> LibraryPaths = new List<string>();
				LibraryPaths.Add(Directory.GetCurrentDirectory());
				LibraryPaths.AddRange(Rules.PublicLibraryPaths.Where(x => !x.StartsWith("$(")).Select(x => Path.GetFullPath(x.Replace('/', Path.DirectorySeparatorChar))));

				// Get all the extensions to look for
				List<string> LibraryExtensions = new List<string>();
				LibraryExtensions.Add(BuildPlatform.GetBinaryExtension(UEBuildBinaryType.StaticLibrary));
				LibraryExtensions.Add(BuildPlatform.GetBinaryExtension(UEBuildBinaryType.DynamicLinkLibrary));

				// Add all the libraries
				foreach (string LibraryExtension in LibraryExtensions)
				{
					foreach (string LibraryName in Rules.PublicAdditionalLibraries)
					{
						foreach (string LibraryPath in LibraryPaths)
						{
							string LibraryFileName = Path.Combine(LibraryPath, LibraryName);
							if (File.Exists(LibraryFileName))
							{
								Files.Add(new FileReference(LibraryFileName));
							}

							if(LibraryName.IndexOfAny(new char[] { Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar }) == -1)
							{
								string UnixLibraryFileName = Path.Combine(LibraryPath, "lib" + LibraryName + LibraryExtension);
								if (File.Exists(UnixLibraryFileName))
								{
									Files.Add(new FileReference(UnixLibraryFileName));
								}
							}
						}
					}
				}

				// Find all the include paths
				List<string> AllIncludePaths = new List<string>();
				AllIncludePaths.AddRange(Rules.PublicIncludePaths);
				AllIncludePaths.AddRange(Rules.PublicSystemIncludePaths);

				// Add all the include paths
				foreach (string IncludePath in AllIncludePaths.Where(x => !x.StartsWith("$(")))
				{
					if (Directory.Exists(IncludePath))
					{
						foreach (string IncludeFileName in Directory.EnumerateFiles(IncludePath, "*", SearchOption.AllDirectories))
						{
							string Extension = Path.GetExtension(IncludeFileName).ToLower();
							if (Extension == ".h" || Extension == ".inl" || Extension == ".hpp")
							{
								Files.Add(new FileReference(IncludeFileName));
							}
						}
					}
				}
			}

			// Write the file
			Log.TraceInformation("Writing dependency list to {0}", Location);
			DirectoryReference.CreateDirectory(Location.Directory);
			FileReference.WriteAllLines(Location, Files.Where(x => x.IsUnderDirectory(UnrealBuildTool.RootDirectory)).Select(x => x.MakeRelativeTo(UnrealBuildTool.RootDirectory).Replace(Path.DirectorySeparatorChar, '/')).OrderBy(x => x));
		}

		/// <summary>
		/// Generates a public manifest file for writing out
		/// </summary>
		public void GenerateManifest(FileReference ManifestPath, List<KeyValuePair<FileReference, BuildProductType>> BuildProducts)
		{
			BuildManifest Manifest = new BuildManifest();

			// Add the regular build products
			foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProducts)
			{
				Manifest.BuildProducts.Add(BuildProductPair.Key.FullName);
			}

			// Add all the dependency lists
			foreach(FileReference DependencyListFileName in Rules.DependencyListFileNames)
			{
				Manifest.BuildProducts.Add(DependencyListFileName.FullName);
			}

			if (!Rules.bDisableLinking)
			{
				Manifest.AddBuildProduct(ReceiptFileName.FullName);

				if (bDeployAfterCompile)
				{
					Manifest.DeployTargetFiles.Add(ReceiptFileName.FullName);
				}
			}

			// Remove anything that's not part of the plugin
			if(ForeignPlugin != null)
			{
				DirectoryReference ForeignPluginDir = ForeignPlugin.Directory;
				Manifest.BuildProducts.RemoveAll(x => !new FileReference(x).IsUnderDirectory(ForeignPluginDir));
			}

			Manifest.BuildProducts.Sort();
			Manifest.DeployTargetFiles.Sort();

			Log.TraceInformation("Writing manifest to {0}", ManifestPath);
			Utils.WriteClass<BuildManifest>(Manifest, ManifestPath.FullName, "");
		}

		/// <summary>
		/// Prepare all the module manifests for this target
		/// </summary>
		/// <returns>Dictionary mapping from filename to module manifest</returns>
		Dictionary<FileReference, ModuleManifest> PrepareModuleManifests()
		{
			Dictionary<FileReference, ModuleManifest> FileNameToModuleManifest = new Dictionary<FileReference, ModuleManifest>();
			if (!bCompileMonolithic)
			{
				// Create the receipts for each folder
				foreach (UEBuildBinary Binary in Binaries)
				{
					if(Binary.Type == UEBuildBinaryType.DynamicLinkLibrary)
					{
						DirectoryReference DirectoryName = Binary.OutputFilePath.Directory;
						bool bIsGameBinary = RulesAssembly.IsGameModule(Binary.PrimaryModule.Name);
						FileReference ManifestFileName = FileReference.Combine(DirectoryName, ModuleManifest.GetStandardFileName(AppName, Platform, Configuration, Architecture, bIsGameBinary));

						ModuleManifest Manifest;
						if (!FileNameToModuleManifest.TryGetValue(ManifestFileName, out Manifest))
						{
							Manifest = new ModuleManifest("");
							FileNameToModuleManifest.Add(ManifestFileName, Manifest);
						}

						foreach (UEBuildModuleCPP Module in Binary.Modules.OfType<UEBuildModuleCPP>())
						{
							Manifest.ModuleNameToFileName[Module.Name] = Binary.OutputFilePath.GetFileName();
						}
					}
				}
			}
			return FileNameToModuleManifest;
		}

		/// <summary>
		/// Prepare all the receipts this target (all the .target and .modules files). See the VersionManifest class for an explanation of what these files are.
		/// </summary>
		/// <param name="ToolChain">The toolchain used to build the target</param>
		/// <param name="BuildProducts">Artifacts from the build</param>
		/// <param name="RuntimeDependencies">Output runtime dependencies</param>
		TargetReceipt PrepareReceipt(UEToolChain ToolChain, List<KeyValuePair<FileReference, BuildProductType>> BuildProducts, List<RuntimeDependency> RuntimeDependencies)
		{
			// Read the version file
			BuildVersion Version;
			if (!BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				Version = new BuildVersion();
			}

			// Create a unique identifier for this build which can be used to identify modules which are compatible. It's fine to share this between runs with the same makefile.
			// By default we leave it blank when compiling a subset of modules (for hot reload, etc...), otherwise it won't match anything else. When writing to a directory
			// that already contains a manifest, we'll reuse the build id that's already in there (see below).
			if(String.IsNullOrEmpty(Version.BuildId))
			{
				if(Rules.bFormalBuild)
				{
					// If this is a formal build, we can just the compatible changelist as the unique id.
					Version.BuildId = String.Format("{0}", Version.EffectiveCompatibleChangelist);
				}
			}

			// Create the receipt
			TargetReceipt Receipt = new TargetReceipt(ProjectFile, TargetName, TargetType, Platform, Configuration, Version);

			// Set the launch executable if there is one
			foreach(KeyValuePair<FileReference, BuildProductType> Pair in BuildProducts)
			{
				if(Pair.Value == BuildProductType.Executable)
				{
					Receipt.Launch = Pair.Key;
					break;
				}
			}

			// Find all the build products and modules from this binary
			foreach (KeyValuePair<FileReference, BuildProductType> BuildProductPair in BuildProducts)
			{
				if(BuildProductPair.Value != BuildProductType.BuildResource)
				{
					Receipt.AddBuildProduct(BuildProductPair.Key, BuildProductPair.Value);
				}
			}

			// Add the project file
			if(ProjectFile != null)
			{
				Receipt.RuntimeDependencies.Add(ProjectFile, StagedFileType.UFS);
			}

			// Add the descriptors for all enabled plugins
			foreach(UEBuildPlugin EnabledPlugin in EnabledPlugins)
			{
				if(EnabledPlugin.bDescriptorNeededAtRuntime || EnabledPlugin.bDescriptorReferencedExplicitly)
				{
					Receipt.RuntimeDependencies.Add(EnabledPlugin.File, StagedFileType.UFS);
				}
			}

			// Add all the other runtime dependencies
			HashSet<FileReference> UniqueRuntimeDependencyFiles = new HashSet<FileReference>();
			foreach(RuntimeDependency RuntimeDependency in RuntimeDependencies)
			{
				if(UniqueRuntimeDependencyFiles.Add(RuntimeDependency.Path))
				{
					Receipt.RuntimeDependencies.Add(RuntimeDependency);
				}
			}

			// Find all the modules which are part of this target
			HashSet<UEBuildModule> UniqueLinkedModules = new HashSet<UEBuildModule>();
			foreach (UEBuildBinary Binary in Binaries)
			{
				foreach (UEBuildModule Module in Binary.Modules)
				{
					if (UniqueLinkedModules.Add(Module))
					{
						Receipt.AdditionalProperties.AddRange(Module.Rules.AdditionalPropertiesForReceipt.Inner);
					}
				}
			}

			// add the SDK used by the tool chain
			Receipt.AdditionalProperties.Add(new ReceiptProperty("SDK", ToolChain.GetSDKVersion()));

			return Receipt;
		}

		/// <summary>
		/// Gathers dependency modules for given binaries list.
		/// </summary>
		/// <param name="Binaries">Binaries list.</param>
		/// <returns>Dependency modules set.</returns>
		static HashSet<UEBuildModuleCPP> GatherDependencyModules(List<UEBuildBinary> Binaries)
		{
			HashSet<UEBuildModuleCPP> Output = new HashSet<UEBuildModuleCPP>();

			foreach (UEBuildBinary Binary in Binaries)
			{
				List<UEBuildModule> DependencyModules = Binary.GetAllDependencyModules(bIncludeDynamicallyLoaded: false, bForceCircular: false);
				foreach (UEBuildModuleCPP Module in DependencyModules.OfType<UEBuildModuleCPP>())
				{
					if (Module.Binary != null)
					{
						Output.Add(Module);
					}
				}
			}

			return Output;
		}

		/// <summary>
		/// Creates a global compile environment suitable for generating project files.
		/// </summary>
		/// <returns>New compile environment</returns>
		public CppCompileEnvironment CreateCompileEnvironmentForProjectFiles()
		{
			CppPlatform CppPlatform = UEBuildPlatform.GetBuildPlatform(Platform).DefaultCppPlatform;
			CppConfiguration CppConfiguration = GetCppConfiguration(Configuration);

			SourceFileMetadataCache MetadataCache = SourceFileMetadataCache.CreateHierarchy(ProjectFile);

			CppCompileEnvironment GlobalCompileEnvironment = new CppCompileEnvironment(CppPlatform, CppConfiguration, Architecture, MetadataCache);
			LinkEnvironment GlobalLinkEnvironment = new LinkEnvironment(GlobalCompileEnvironment.Platform, GlobalCompileEnvironment.Configuration, GlobalCompileEnvironment.Architecture);

			UEToolChain TargetToolChain = CreateToolchain(CppPlatform);
			SetupGlobalEnvironment(TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment);

			return GlobalCompileEnvironment;
		}

		/// <summary>
		/// Builds the target, appending list of output files and returns building result.
		/// </summary>
		public TargetMakefile Build(BuildConfiguration BuildConfiguration, ISourceFileWorkingSet WorkingSet, bool bIsAssemblingBuild)
		{
			CppPlatform CppPlatform = UEBuildPlatform.GetBuildPlatform(Platform).DefaultCppPlatform;
			CppConfiguration CppConfiguration = GetCppConfiguration(Configuration);

			SourceFileMetadataCache MetadataCache = SourceFileMetadataCache.CreateHierarchy(ProjectFile);

			CppCompileEnvironment GlobalCompileEnvironment = new CppCompileEnvironment(CppPlatform, CppConfiguration, Architecture, MetadataCache);
			LinkEnvironment GlobalLinkEnvironment = new LinkEnvironment(GlobalCompileEnvironment.Platform, GlobalCompileEnvironment.Configuration, GlobalCompileEnvironment.Architecture);

			UEToolChain TargetToolChain = CreateToolchain(CppPlatform);
			SetupGlobalEnvironment(TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment);

			// Save off the original list of binaries. We'll use this to figure out which PCHs to create later, to avoid switching PCHs when compiling single modules.
			List<UEBuildBinary> OriginalBinaries = Binaries;

			// For installed builds, filter out all the binaries that aren't in mods
			if (UnrealBuildTool.IsProjectInstalled())
			{
				List<DirectoryReference> ModDirectories = EnabledPlugins.Where(x => x.Type == PluginType.Mod).Select(x => x.Directory).ToList();

				List<UEBuildBinary> FilteredBinaries = new List<UEBuildBinary>();
				foreach (UEBuildBinary DLLBinary in Binaries)
				{
					if(ModDirectories.Any(x => DLLBinary.OutputFilePath.IsUnderDirectory(x)))
					{
						FilteredBinaries.Add(DLLBinary);
					}
				}
				Binaries = FilteredBinaries;

				if (Binaries.Count == 0)
				{
					throw new BuildException("No modules found to build. All requested binaries were already part of the installed data.");
				}
			}

			// Build a mapping from module to its plugin
			Dictionary<UEBuildModule, UEBuildPlugin> ModuleToPlugin = new Dictionary<UEBuildModule, UEBuildPlugin>();
			foreach(UEBuildPlugin Plugin in BuildPlugins)
			{
				foreach(UEBuildModule Module in Plugin.Modules)
				{
					if (!ModuleToPlugin.ContainsKey(Module))
					{
						ModuleToPlugin.Add(Module, Plugin);
					}
				}
			}

			// Check there aren't any engine binaries with dependencies on game modules. This can happen when game-specific plugins override engine plugins.
			foreach(UEBuildModule Module in Modules.Values)
			{
				if(Module.Binary != null && UnrealBuildTool.IsUnderAnEngineDirectory(Module.RulesFile.Directory))
				{
					DirectoryReference RootDirectory = UnrealBuildTool.EngineDirectory;

					if (Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory))
					{
						RootDirectory = UnrealBuildTool.EnterpriseDirectory;
					}

					HashSet<UEBuildModule> ReferencedModules = Module.GetDependencies(bWithIncludePathModules: true, bWithDynamicallyLoadedModules: true);

					// Make sure engine modules don't depend on enterprise or game modules and that enterprise modules don't depend on game modules
					foreach(UEBuildModule ReferencedModule in ReferencedModules)
					{
						if(ReferencedModule.RulesFile != null && !ReferencedModule.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) && !ReferencedModule.RulesFile.IsUnderDirectory(RootDirectory))
						{
							string EngineModuleRelativePath = Module.RulesFile.MakeRelativeTo(UnrealBuildTool.EngineDirectory.ParentDirectory);
							string ReferencedModuleRelativePath = (ProjectFile != null && ReferencedModule.RulesFile.IsUnderDirectory(ProjectFile.Directory)) ? ReferencedModule.RulesFile.MakeRelativeTo(ProjectFile.Directory.ParentDirectory) : ReferencedModule.RulesFile.FullName;
							throw new BuildException("Engine module '{0}' should not depend on game module '{1}'", EngineModuleRelativePath, ReferencedModuleRelativePath);
						}
					}

					// Make sure engine modules don't directly reference engine plugins
					if(Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineSourceDirectory) && !Module.RulesFile.IsUnderDirectory(TargetRulesFile.Directory))
					{
						foreach(UEBuildModule ReferencedModule in ReferencedModules)
						{
							if(ReferencedModule.RulesFile != null && ModuleToPlugin.ContainsKey(ReferencedModule) && !IsWhitelistedEnginePluginReference(Module.Name, ReferencedModule.Name))
							{
								string EngineModuleRelativePath = Module.RulesFile.MakeRelativeTo(UnrealBuildTool.EngineDirectory.ParentDirectory);
								string ReferencedModuleRelativePath = ReferencedModule.RulesFile.MakeRelativeTo(UnrealBuildTool.EngineDirectory.ParentDirectory);
								Log.TraceWarning("Warning: Engine module '{0}' should not depend on plugin module '{1}'", EngineModuleRelativePath, ReferencedModuleRelativePath);
							}
						}
					}
				}
			}

			// Check that each plugin declares its dependencies explicitly
			foreach(UEBuildPlugin Plugin in BuildPlugins)
			{
				foreach(UEBuildModule Module in Plugin.Modules)
				{
					HashSet<UEBuildModule> DependencyModules = Module.GetDependencies(bWithIncludePathModules: true, bWithDynamicallyLoadedModules: true);
					foreach(UEBuildModule DependencyModule in DependencyModules)
					{
						UEBuildPlugin DependencyPlugin;
						if(ModuleToPlugin.TryGetValue(DependencyModule, out DependencyPlugin) && DependencyPlugin != Plugin && !Plugin.Dependencies.Contains(DependencyPlugin))
						{
							Log.TraceWarning("Warning: Plugin '{0}' does not list plugin '{1}' as a dependency, but module '{2}' depends on '{3}'.", Plugin.Name, DependencyPlugin.Name, Module.Name, DependencyModule.Name);
						}
					}
				}
			}

			// Create the makefile
			string ExternalMetadata = UEBuildPlatform.GetBuildPlatform(Platform).GetExternalBuildMetadata(ProjectFile);
			TargetMakefile Makefile = new TargetMakefile(TargetToolChain.GetVersionInfo(), ExternalMetadata, ReceiptFileName, ProjectIntermediateDirectory, TargetType, bDeployAfterCompile, bHasProjectScriptPlugin);

			// Setup the hot reload module list
			Makefile.HotReloadModuleNames = GetHotReloadModuleNames();

			// If we're compiling monolithic, make sure the executable knows about all referenced modules
			if (ShouldCompileMonolithic())
			{
				UEBuildBinary ExecutableBinary = Binaries[0];

				// Add all the modules that the executable depends on. Plugins will be already included in this list.
				List<UEBuildModule> AllReferencedModules = ExecutableBinary.GetAllDependencyModules(bIncludeDynamicallyLoaded: true, bForceCircular: true);
				foreach (UEBuildModule CurModule in AllReferencedModules)
				{
					if (CurModule.Binary == null || CurModule.Binary == ExecutableBinary || CurModule.Binary.Type == UEBuildBinaryType.StaticLibrary)
					{
						ExecutableBinary.AddModule(CurModule);
					}
				}
			}

			// Add global definitions for project-specific binaries. HACK: Also defining for monolithic builds in binary releases. Might be better to set this via command line instead?
			if(!bUseSharedBuildEnvironment || bCompileMonolithic)
			{
				UEBuildBinary ExecutableBinary = Binaries[0];

				bool IsCurrentPlatform;
				if (Utils.IsRunningOnMono)
				{
					IsCurrentPlatform = Platform == UnrealTargetPlatform.Mac || (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Unix) && Platform == BuildHostPlatform.Current.Platform);
				}
				else
				{
					IsCurrentPlatform = Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Win32;
				}

				if (IsCurrentPlatform)
				{
					// The hardcoded engine directory needs to be a relative path to match the normal EngineDir format. Not doing so breaks the network file system (TTP#315861).
					string OutputFilePath = ExecutableBinary.OutputFilePath.FullName;
					if (Platform == UnrealTargetPlatform.Mac && OutputFilePath.Contains(".app/Contents/MacOS"))
					{
						OutputFilePath = OutputFilePath.Substring(0, OutputFilePath.LastIndexOf(".app/Contents/MacOS") + 4);
					}
					string EnginePath = Utils.CleanDirectorySeparators(UnrealBuildTool.EngineDirectory.MakeRelativeTo(ExecutableBinary.OutputFilePath.Directory), '/');
					if (EnginePath.EndsWith("/") == false)
					{
						EnginePath += "/";
					}
					GlobalCompileEnvironment.Definitions.Add(String.Format("UE_ENGINE_DIRECTORY=\"{0}\"", EnginePath));
				}
			}

			// On Mac and Linux we have actions that should be executed after all the binaries are created
			TargetToolChain.SetupBundleDependencies(Binaries, TargetName);

			// Generate headers
			HashSet<UEBuildModuleCPP> ModulesToGenerateHeadersFor = GatherDependencyModules(OriginalBinaries.ToList());
			using(Timeline.ScopeEvent("ExternalExecution.SetupUObjectModules()"))
			{
				ExternalExecution.SetupUObjectModules(ModulesToGenerateHeadersFor, Rules.Platform, ProjectDescriptor, Makefile.UObjectModules, Makefile.UObjectModuleHeaders, Rules.GeneratedCodeVersion, bIsAssemblingBuild, MetadataCache);
			}

			// NOTE: Even in Gather mode, we need to run UHT to make sure the files exist for the static action graph to be setup correctly.  This is because UHT generates .cpp
			// files that are injected as top level prerequisites.  If UHT only emitted included header files, we wouldn't need to run it during the Gather phase at all.
			if (Makefile.UObjectModules.Count > 0)
			{
				FileReference ModuleInfoFileName = FileReference.Combine(ProjectIntermediateDirectory, TargetName + ".uhtmanifest");
				ExternalExecution.ExecuteHeaderToolIfNecessary(BuildConfiguration, ProjectFile, TargetName, TargetType, bHasProjectScriptPlugin, Makefile.UObjectModules, ModuleInfoFileName, true, bIsAssemblingBuild, WorkingSet);
			}

			// Find all the shared PCHs.
			if (Rules.bUseSharedPCHs)
			{
				FindSharedPCHs(OriginalBinaries, GlobalCompileEnvironment);
			}

			// Compile the resource files common to all DLLs on Windows
			if (!ShouldCompileMonolithic())
			{
				if (Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64)
				{
					if(!Rules.bFormalBuild)
					{
						CppCompileEnvironment DefaultResourceCompileEnvironment = new CppCompileEnvironment(GlobalCompileEnvironment);

						FileItem DefaultResourceFile = FileItem.GetItemByFileReference(FileReference.Combine(UnrealBuildTool.EngineDirectory, "Build", "Windows", "Resources", "Default.rc2"));

						CPPOutput DefaultResourceOutput = TargetToolChain.CompileRCFiles(DefaultResourceCompileEnvironment, new List<FileItem> { DefaultResourceFile }, EngineIntermediateDirectory, Makefile.Actions);
						GlobalLinkEnvironment.DefaultResourceFiles.AddRange(DefaultResourceOutput.ObjectFiles);
					}
				}
			}

			// Build the target's binaries.
			DirectoryReference ExeDir = GetExecutableDir();
			using(Timeline.ScopeEvent("UEBuildBinary.Build()"))
			{
				foreach (UEBuildBinary Binary in Binaries)
				{
					List<FileItem> BinaryOutputItems = Binary.Build(Rules, TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment, WorkingSet, ExeDir, Makefile);
					Makefile.OutputItems.AddRange(BinaryOutputItems);
				}
			}

			// Prepare all the runtime dependencies, copying them from their source folders if necessary
			List<RuntimeDependency> RuntimeDependencies = new List<RuntimeDependency>();
			Dictionary<FileReference, FileReference> RuntimeDependencyTargetFileToSourceFile = new Dictionary<FileReference, FileReference>();
			foreach(UEBuildBinary Binary in Binaries)
			{
				Binary.PrepareRuntimeDependencies(RuntimeDependencies, RuntimeDependencyTargetFileToSourceFile, ExeDir);
			}
			foreach(KeyValuePair<FileReference, FileReference> Pair in RuntimeDependencyTargetFileToSourceFile)
			{
				if(!UnrealBuildTool.IsFileInstalled(Pair.Key))
				{
					Makefile.OutputItems.Add(CreateCopyAction(Pair.Value, Pair.Key, Makefile.Actions));
				}
			}

			// If we're just precompiling a plugin, only include output items which are under that directory
			if(ForeignPlugin != null)
			{
				Makefile.OutputItems.RemoveAll(x => !x.Location.IsUnderDirectory(ForeignPlugin.Directory));
			}

			// Allow the toolchain to modify the final output items
			TargetToolChain.FinalizeOutput(Rules, Makefile);

			// Get all the regular build products
			List<KeyValuePair<FileReference, BuildProductType>> BuildProducts = new List<KeyValuePair<FileReference, BuildProductType>>();
			foreach (UEBuildBinary Binary in Binaries)
			{
				Dictionary<FileReference, BuildProductType> BinaryBuildProducts = new Dictionary<FileReference, BuildProductType>();
				Binary.GetBuildProducts(Rules, TargetToolChain, BinaryBuildProducts, GlobalLinkEnvironment.bCreateDebugInfo);
				BuildProducts.AddRange(BinaryBuildProducts);
			}
			BuildProducts.AddRange(RuntimeDependencyTargetFileToSourceFile.Select(x => new KeyValuePair<FileReference, BuildProductType>(x.Key, BuildProductType.RequiredResource)));

			// Remove any installed build products that don't exist. They may be part of an optional install.
			if(UnrealBuildTool.IsEngineInstalled())
			{
				BuildProducts.RemoveAll(x => UnrealBuildTool.IsFileInstalled(x.Key) && !FileReference.Exists(x.Key));
			}

			// Make sure all the checked headers were valid
			List<string> InvalidIncludeDirectiveMessages = Modules.Values.OfType<UEBuildModuleCPP>().Where(x => x.InvalidIncludeDirectiveMessages != null).SelectMany(x => x.InvalidIncludeDirectiveMessages).ToList();
			if (InvalidIncludeDirectiveMessages.Count > 0)
			{
				foreach (string InvalidIncludeDirectiveMessage in InvalidIncludeDirectiveMessages)
				{
					Log.WriteLine(0, LogEventType.Warning, LogFormatOptions.NoSeverityPrefix, "{0}", InvalidIncludeDirectiveMessage);
				}
			}

			// Finalize and generate metadata for this target
			if(!Rules.bDisableLinking)
			{
				// Also add any explicitly specified build products
				if(Rules.AdditionalBuildProducts.Count > 0)
				{
					Dictionary<string, string> Variables = GetTargetVariables(null);
					foreach(string AdditionalBuildProduct in Rules.AdditionalBuildProducts)
					{
						FileReference BuildProductFile = new FileReference(Utils.ExpandVariables(AdditionalBuildProduct, Variables));
						BuildProducts.Add(new KeyValuePair<FileReference, BuildProductType>(BuildProductFile, BuildProductType.RequiredResource));
					}
				}

				// Get the path to the version file unless this is a formal build (where it will be compiled in)
				FileReference VersionFile = null;
				if(Rules.LinkType != TargetLinkType.Monolithic && Binaries[0].Type == UEBuildBinaryType.Executable)
				{
					UnrealTargetConfiguration VersionConfig = Configuration;
					if(VersionConfig == UnrealTargetConfiguration.DebugGame && !bCompileMonolithic && TargetType != TargetType.Program && bUseSharedBuildEnvironment)
					{
						VersionConfig = UnrealTargetConfiguration.Development;
					}
					VersionFile = BuildVersion.GetFileNameForTarget(ExeDir, bCompileMonolithic? TargetName : AppName, Platform, VersionConfig, Architecture);
				}

				// Also add the version file as a build product
				if(VersionFile != null)
				{
					BuildProducts.Add(new KeyValuePair<FileReference, BuildProductType>(VersionFile, BuildProductType.RequiredResource));
				}

				// Prepare the module manifests, and add them to the list of build products
				Dictionary<FileReference, ModuleManifest> FileNameToModuleManifest = PrepareModuleManifests();
				BuildProducts.AddRange(FileNameToModuleManifest.Select(x => new KeyValuePair<FileReference, BuildProductType>(x.Key, BuildProductType.RequiredResource)));

				// Prepare the receipt
				TargetReceipt Receipt = PrepareReceipt(TargetToolChain, BuildProducts, RuntimeDependencies);

				// Create an action which to generate the receipts
				WriteMetadataTargetInfo MetadataTargetInfo = new WriteMetadataTargetInfo(ProjectFile, VersionFile, ReceiptFileName, Receipt, FileNameToModuleManifest);
				FileReference MetadataTargetFile = FileReference.Combine(ProjectIntermediateDirectory, "Metadata.dat");
				BinaryFormatterUtils.SaveIfDifferent(MetadataTargetFile, MetadataTargetInfo);

				StringBuilder WriteMetadataArguments = new StringBuilder();
				WriteMetadataArguments.AppendFormat("-Input={0}", Utils.MakePathSafeToUseWithCommandLine(MetadataTargetFile));
				WriteMetadataArguments.AppendFormat(" -Version={0}", WriteMetadataMode.CurrentVersionNumber);
				if(Rules.bNoManifestChanges)
				{
					WriteMetadataArguments.Append(" -NoManifestChanges");
				}

				Action WriteMetadataAction = Action.CreateRecursiveAction<WriteMetadataMode>(ActionType.WriteMetadata, WriteMetadataArguments.ToString());
				WriteMetadataAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
				WriteMetadataAction.StatusDescription = ReceiptFileName.GetFileName();
				WriteMetadataAction.bCanExecuteRemotely = false;
				WriteMetadataAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(MetadataTargetFile));
				WriteMetadataAction.PrerequisiteItems.AddRange(Makefile.OutputItems);
				WriteMetadataAction.ProducedItems.Add(FileItem.GetItemByFileReference(ReceiptFileName));
				Makefile.Actions.Add(WriteMetadataAction);

				Makefile.OutputItems.AddRange(WriteMetadataAction.ProducedItems);

				// Create actions to run the post build steps
				FileReference[] PostBuildScripts = CreatePostBuildScripts();
				foreach(FileReference PostBuildScript in PostBuildScripts)
				{
					FileReference OutputFile = new FileReference(PostBuildScript.FullName + ".ran");

					Action PostBuildStepAction = new Action(ActionType.PostBuildStep);
					PostBuildStepAction.CommandPath = BuildHostPlatform.Current.Shell;
					if(BuildHostPlatform.Current.ShellType == ShellType.Cmd)
					{
						PostBuildStepAction.CommandArguments = String.Format("/C \"call \"{0}\" && type NUL >\"{1}\"\"", PostBuildScript, OutputFile);
					}
					else
					{
						PostBuildStepAction.CommandArguments = String.Format("\"{0}\" && touch \"{1}\"", PostBuildScript, OutputFile);
					}
					PostBuildStepAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
					PostBuildStepAction.StatusDescription = String.Format("Executing post build script ({0})", PostBuildScript.GetFileName());
					PostBuildStepAction.bCanExecuteRemotely = false;
					PostBuildStepAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(ReceiptFileName));
					PostBuildStepAction.ProducedItems.Add(FileItem.GetItemByFileReference(OutputFile));
					Makefile.Actions.Add(PostBuildStepAction);

					Makefile.OutputItems.AddRange(PostBuildStepAction.ProducedItems);
				}
			}

			// Build a list of all the files required to build
			foreach(FileReference DependencyListFileName in Rules.DependencyListFileNames)
			{
				WriteDependencyList(DependencyListFileName, RuntimeDependencies, RuntimeDependencyTargetFileToSourceFile);
			}

			// If we're only generating the manifest, return now
			foreach(FileReference ManifestFileName in Rules.ManifestFileNames)
			{
				GenerateManifest(ManifestFileName, BuildProducts);
			}

			// Check there are no EULA or restricted folder violations
			if(!Rules.bDisableLinking)
			{
				// Check the distribution level of all binaries based on the dependencies they have
				if(ProjectFile == null && !Rules.bOutputPubliclyDistributable)
				{
					Dictionary<UEBuildModule, Dictionary<RestrictedFolder, DirectoryReference>> ModuleRestrictedFolderCache = new Dictionary<UEBuildModule, Dictionary<RestrictedFolder, DirectoryReference>>();

					bool bResult = true;
					foreach (UEBuildBinary Binary in Binaries)
					{
						bResult &= Binary.CheckRestrictedFolders(DirectoryReference.FromFile(ProjectFile), ModuleRestrictedFolderCache);
					}
					foreach(KeyValuePair<FileReference, FileReference> Pair in RuntimeDependencyTargetFileToSourceFile)
					{
						bResult &= CheckRestrictedFolders(Pair.Key, Pair.Value);
					}

					if(!bResult)
					{
						throw new BuildException("Unable to create binaries in less restricted locations than their input files.");
					}
				}

				// Check for linking against modules prohibited by the EULA
				CheckForEULAViolation();
			}

			// Add all the plugins to be tracked
			foreach(FileReference PluginFile in global::UnrealBuildTool.Plugins.EnumeratePlugins(ProjectFile))
			{
				FileItem PluginFileItem = FileItem.GetItemByFileReference(PluginFile);
				Makefile.PluginFiles.Add(PluginFileItem);
			}

			// Add all the input files to the predicate store
			Makefile.AdditionalDependencies.Add(FileItem.GetItemByFileReference(TargetRulesFile));
			foreach(UEBuildModule Module in Modules.Values)
			{
				Makefile.AdditionalDependencies.Add(FileItem.GetItemByFileReference(Module.RulesFile));
				foreach(string ExternalDependency in Module.Rules.ExternalDependencies)
				{
					FileReference Location = FileReference.Combine(Module.RulesFile.Directory, ExternalDependency);
					Makefile.AdditionalDependencies.Add(FileItem.GetItemByFileReference(Location));
				}
			}
			Makefile.AdditionalDependencies.UnionWith(Makefile.PluginFiles);

			// Clean any stale modules which exist in multiple output directories. This can lead to the wrong DLL being loaded on Windows.
			CleanStaleModules();

			return Makefile;
		}

		/// <summary>
		/// Gets the output directory for the main executable
		/// </summary>
		/// <returns>The executable directory</returns>
		DirectoryReference GetExecutableDir()
		{
			DirectoryReference ExeDir = Binaries[0].OutputDir;
			if (Platform == UnrealTargetPlatform.Mac && ExeDir.FullName.EndsWith(".app/Contents/MacOS"))
			{
				ExeDir = ExeDir.ParentDirectory.ParentDirectory.ParentDirectory;
			}
			return ExeDir;
		}

		/// <summary>
		/// Check that copying a file from one location to another does not violate rules regarding restricted folders
		/// </summary>
		/// <param name="TargetFile">The destination location for the file</param>
		/// <param name="SourceFile">The source location of the file</param>
		/// <returns>True if the copy is permitted, false otherwise</returns>
		bool CheckRestrictedFolders(FileReference TargetFile, FileReference SourceFile)
		{
			List<RestrictedFolder> TargetRestrictedFolders = GetRestrictedFolders(TargetFile);
			List<RestrictedFolder> SourceRestrictedFolders = GetRestrictedFolders(SourceFile);
			foreach(RestrictedFolder SourceRestrictedFolder in SourceRestrictedFolders)
			{
				if(!TargetRestrictedFolders.Contains(SourceRestrictedFolder))
				{
					Log.TraceError("Runtime dependency '{0}' is copied to '{1}', which does not contain a '{2}' folder.", SourceFile, TargetFile, SourceRestrictedFolder);
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Gets the restricted folders that the given file is in
		/// </summary>
		/// <param name="File">The file to test</param>
		/// <returns>List of restricted folders for the file</returns>
		List<RestrictedFolder> GetRestrictedFolders(FileReference File)
		{
			// Find the base directory for this binary
			DirectoryReference BaseDir;
			if(File.IsUnderDirectory(UnrealBuildTool.RootDirectory))
			{
				BaseDir = UnrealBuildTool.RootDirectory;
			}
			else if(ProjectDirectory != null && File.IsUnderDirectory(ProjectDirectory))
			{
				BaseDir = ProjectDirectory;
			}
			else
			{
				return new List<RestrictedFolder>();
			}

			// Find the restricted folders under the base directory
			return RestrictedFolders.FindRestrictedFolders(BaseDir, File.Directory);
		}

		/// <summary>
		/// Creates an action which copies a file from one location to another
		/// </summary>
		/// <param name="SourceFile">The source file location</param>
		/// <param name="TargetFile">The target file location</param>
		/// <param name="Actions">List of actions to be executed. Additional actions will be added to this list.</param>
		/// <returns>File item for the output file</returns>
		static FileItem CreateCopyAction(FileReference SourceFile, FileReference TargetFile, List<Action> Actions)
		{
			FileItem SourceFileItem = FileItem.GetItemByFileReference(SourceFile);
			FileItem TargetFileItem = FileItem.GetItemByFileReference(TargetFile);

			Action CopyAction = new Action(ActionType.BuildProject);
			CopyAction.CommandDescription = "Copy";
			CopyAction.CommandPath = BuildHostPlatform.Current.Shell;
			if(BuildHostPlatform.Current.ShellType == ShellType.Cmd)
			{
				CopyAction.CommandArguments = String.Format("/C \"copy /Y \"{0}\" \"{1}\" 1>nul\"", SourceFile, TargetFile);
			}
			else
			{
				CopyAction.CommandArguments = String.Format("-c 'cp -f \"{0}\" \"{1}\"'", SourceFile.FullName, TargetFile.FullName);
			}
			CopyAction.WorkingDirectory = UnrealBuildTool.EngineSourceDirectory;
			CopyAction.PrerequisiteItems.Add(SourceFileItem);
			CopyAction.ProducedItems.Add(TargetFileItem);
			CopyAction.DeleteItems.Add(TargetFileItem);
			CopyAction.StatusDescription = TargetFileItem.Location.GetFileName();
			CopyAction.bCanExecuteRemotely = false;
			Actions.Add(CopyAction);

			return TargetFileItem;
		}

		/// <summary>
		/// Creates a toolchain for the current target. May be overridden by the target rules.
		/// </summary>
		/// <returns>New toolchain instance</returns>
		private UEToolChain CreateToolchain(CppPlatform CppPlatform)
		{
			if (Rules.ToolChainName == null)
			{
				return UEBuildPlatform.GetBuildPlatform(Platform).CreateToolChain(CppPlatform, Rules);
			}
			else
			{
				Type ToolchainType = Assembly.GetExecutingAssembly().GetType(String.Format("UnrealBuildTool.{0}", Rules.ToolChainName), false, true);
				if (ToolchainType == null)
				{
					throw new BuildException("Unable to create toolchain '{0}'. Check that the name is correct.", Rules.ToolChainName);
				}
				return (UEToolChain)Activator.CreateInstance(ToolchainType, Rules);
			}
		}

		/// <summary>
		/// Cleans any stale modules that have changed moved output folder.
		/// 
		/// On Windows, the loader reads imported DLLs from the first location it finds them. If modules are moved from one place to another, we have to be sure to clean up the old versions 
		/// so that they're not loaded accidentally causing unintuitive import errors.
		/// </summary>
		void CleanStaleModules()
		{
			// Find all the output files
			HashSet<FileReference> OutputFiles = new HashSet<FileReference>();
			foreach(UEBuildBinary Binary in Binaries)
			{
				OutputFiles.UnionWith(Binary.OutputFilePaths);
			}

			// Build a map of base filenames to their full path
			Dictionary<string, FileReference> OutputNameToLocation = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);
			foreach(FileReference OutputFile in OutputFiles)
			{
				OutputNameToLocation[OutputFile.GetFileName()] = OutputFile;
			}

			// Search all the output directories for files with a name matching one of our output files
			foreach(DirectoryReference OutputDirectory in OutputFiles.Select(x => x.Directory).Distinct())
			{
				if (DirectoryReference.Exists(OutputDirectory))
				{
					foreach (FileReference ExistingFile in DirectoryReference.EnumerateFiles(OutputDirectory))
					{
						FileReference OutputFile;
						if (OutputNameToLocation.TryGetValue(ExistingFile.GetFileName(), out OutputFile) && !OutputFiles.Contains(ExistingFile))
						{
							Log.TraceInformation("Deleting '{0}' to avoid ambiguity with '{1}'", ExistingFile, OutputFile);
							try
							{
								FileReference.Delete(ExistingFile);
							}
							catch (Exception Ex)
							{
								Log.TraceError("Unable to delete {0} ({1})", ExistingFile, Ex.Message.TrimEnd());
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Check whether a reference from an engine module to a plugin module is allowed. Temporary hack until these can be fixed up propertly.
		/// </summary>
		/// <param name="EngineModuleName">Name of the engine module.</param>
		/// <param name="PluginModuleName">Name of the plugin module.</param>
		/// <returns>True if the reference is whitelisted.</returns>
		static bool IsWhitelistedEnginePluginReference(string EngineModuleName, string PluginModuleName)
		{
			if(EngineModuleName == "AndroidDeviceDetection" && PluginModuleName == "TcpMessaging")
			{
				return true;
			}
			if(EngineModuleName == "Voice" && PluginModuleName == "AndroidPermission")
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Export the definition of this target to a JSON file
		/// </summary>
		/// <param name="OutputFile">File to write to</param>
		public void ExportJson(FileReference OutputFile)
		{
			DirectoryReference.CreateDirectory(OutputFile.Directory);
			using (JsonWriter Writer = new JsonWriter(OutputFile))
			{
				Writer.WriteObjectStart();

				Writer.WriteValue("Name", TargetName);
				Writer.WriteValue("Configuration", Configuration.ToString());
				Writer.WriteValue("Platform", Platform.ToString());
				if (ProjectFile != null)
				{
					Writer.WriteValue("ProjectFile", ProjectFile.FullName);
				}

				Writer.WriteArrayStart("Binaries");
				foreach (UEBuildBinary Binary in Binaries)
				{
					Writer.WriteObjectStart();
					Binary.ExportJson(Writer);
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();

				Writer.WriteObjectStart("Modules");
				foreach(UEBuildModule Module in Modules.Values)
				{
					Writer.WriteObjectStart(Module.Name);
					Module.ExportJson(Module.Binary?.OutputDir, GetExecutableDir(), Writer);
					Writer.WriteObjectEnd();
				}
				Writer.WriteObjectEnd();

				Writer.WriteObjectEnd();
			}
		}

		/// <summary>
		/// Check for EULA violation dependency issues.
		/// </summary>
		private void CheckForEULAViolation()
		{
			if (TargetType != TargetType.Editor && TargetType != TargetType.Program && Configuration == UnrealTargetConfiguration.Shipping &&
				Rules.bCheckLicenseViolations)
			{
				bool bLicenseViolation = false;
				foreach (UEBuildBinary Binary in Binaries)
				{
					List<UEBuildModule> AllDependencies = Binary.GetAllDependencyModules(true, false);
					IEnumerable<UEBuildModule> NonRedistModules = AllDependencies.Where((DependencyModule) =>
							!IsRedistributable(DependencyModule) && DependencyModule.Name != AppName
						);

					if (NonRedistModules.Count() != 0)
					{
						IEnumerable<UEBuildModule> NonRedistDeps = AllDependencies.Where((DependantModule) =>
							DependantModule.GetDirectDependencyModules().Intersect(NonRedistModules).Any()
						);
						string Message = string.Format("Non-editor build cannot depend on non-redistributable modules. {0} depends on '{1}'.", Binary.ToString(), string.Join("', '", NonRedistModules));
						if (NonRedistDeps.Any())
						{
							Message = string.Format("{0}\nDependant modules '{1}'", Message, string.Join("', '", NonRedistDeps));
						}
						if(Rules.bBreakBuildOnLicenseViolation)
						{
							Log.TraceError("ERROR: {0}", Message);
						}
						else
						{
							Log.TraceWarning("WARNING: {0}", Message);
						}
						bLicenseViolation = true;
					}
				}
				if (Rules.bBreakBuildOnLicenseViolation && bLicenseViolation)
				{
					throw new BuildException("Non-editor build cannot depend on non-redistributable modules.");
				}
			}
		}

		/// <summary>
		/// Tells if this module can be redistributed.
		/// </summary>
		public static bool IsRedistributable(UEBuildModule Module)
		{
			if(Module.Rules != null && Module.Rules.IsRedistributableOverride.HasValue)
			{
				return Module.Rules.IsRedistributableOverride.Value;
			}

			if(Module.RulesFile != null)
			{
				return !Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineSourceDeveloperDirectory) && !Module.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineSourceEditorDirectory);
			}

			return true;
		}

		/// <summary>
		/// Setup target before build. This method finds dependencies, sets up global environment etc.
		/// </summary>
		public void PreBuildSetup()
		{
			// Describe what's being built.
			Log.TraceVerbose("Building {0} - {1} - {2} - {3}", AppName, TargetName, Platform, Configuration);

			// Setup the target's binaries.
			SetupBinaries();

			// Setup the target's plugins
			SetupPlugins();

			// Add the plugin binaries to the build
			foreach (UEBuildPlugin Plugin in BuildPlugins)
			{
				foreach(UEBuildModuleCPP Module in Plugin.Modules)
				{
					AddModuleToBinary(Module);
				}
			}

			// Add all of the extra modules, including game modules, that need to be compiled along
			// with this app.  These modules are always statically linked in monolithic targets, but not necessarily linked to anything in modular targets,
			// and may still be required at runtime in order for the application to load and function properly!
			AddExtraModules();

			// Create all the modules referenced by the existing binaries
			foreach(UEBuildBinary Binary in Binaries)
			{
				Binary.CreateAllDependentModules(FindOrCreateModuleByName);
			}

			// Bind every referenced C++ module to a binary
			for (int Idx = 0; Idx < Binaries.Count; Idx++)
			{
				List<UEBuildModule> DependencyModules = Binaries[Idx].GetAllDependencyModules(true, true);
				foreach (UEBuildModuleCPP DependencyModule in DependencyModules.OfType<UEBuildModuleCPP>())
				{
					if(DependencyModule.Binary == null)
					{
						AddModuleToBinary(DependencyModule);
					}
				}
			}

			// Add all the modules to the target if necessary.
			if(Rules.bBuildAllModules)
			{
				AddAllValidModulesToTarget();
			}

			// Add the external and non-C++ referenced modules to the binaries that reference them.
			foreach (UEBuildModuleCPP Module in Modules.Values.OfType<UEBuildModuleCPP>())
			{
				if(Module.Binary != null)
				{
					foreach (UEBuildModule ReferencedModule in Module.GetUnboundReferences())
					{
						Module.Binary.AddModule(ReferencedModule);
					}
				}
			}

			if (!bCompileMonolithic)
			{
				if (Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Win32)
				{
					// On Windows create import libraries for all binaries ahead of time, since linking binaries often causes bottlenecks
					foreach (UEBuildBinary Binary in Binaries)
					{
						Binary.SetCreateImportLibrarySeparately(true);
					}
				}
				else
				{
					// On other platforms markup all the binaries containing modules with circular references
					foreach (UEBuildModule Module in Modules.Values.Where(x => x.Binary != null))
					{
						foreach (string CircularlyReferencedModuleName in Module.Rules.CircularlyReferencedDependentModules)
						{
							UEBuildModule CircularlyReferencedModule;
							if (Modules.TryGetValue(CircularlyReferencedModuleName, out CircularlyReferencedModule) && CircularlyReferencedModule.Binary != null)
							{
								CircularlyReferencedModule.Binary.SetCreateImportLibrarySeparately(true);
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Creates scripts for executing the pre-build scripts
		/// </summary>
		public FileReference[] CreatePreBuildScripts()
		{
			// Find all the pre-build steps
			List<Tuple<string[], UEBuildPlugin>> PreBuildCommandBatches = new List<Tuple<string[], UEBuildPlugin>>();
			if(ProjectDescriptor != null && ProjectDescriptor.PreBuildSteps != null)
			{
				AddCustomBuildSteps(ProjectDescriptor.PreBuildSteps, null, PreBuildCommandBatches);
			}
			if(Rules.PreBuildSteps.Count > 0)
			{
				PreBuildCommandBatches.Add(new Tuple<string[], UEBuildPlugin>(Rules.PreBuildSteps.ToArray(), null));
			}
			foreach(UEBuildPlugin BuildPlugin in BuildPlugins.Where(x => x.Descriptor.PreBuildSteps != null))
			{
				AddCustomBuildSteps(BuildPlugin.Descriptor.PreBuildSteps, BuildPlugin, PreBuildCommandBatches);
			}
			return WriteCustomBuildStepScripts(BuildHostPlatform.Current.Platform, ProjectIntermediateDirectory, "PreBuild", PreBuildCommandBatches);
		}

		/// <summary>
		/// Creates scripts for executing post-build steps
		/// </summary>
		/// <returns>Array of post-build scripts</returns>
		private FileReference[] CreatePostBuildScripts()
		{
			// Find all the post-build steps
			List<Tuple<string[], UEBuildPlugin>> PostBuildCommandBatches = new List<Tuple<string[], UEBuildPlugin>>();
			if(!Rules.bDisableLinking)
			{
				if(ProjectDescriptor != null && ProjectDescriptor.PostBuildSteps != null)
				{
					AddCustomBuildSteps(ProjectDescriptor.PostBuildSteps, null, PostBuildCommandBatches);
				}
				if(Rules.PostBuildSteps.Count > 0)
				{
					PostBuildCommandBatches.Add(new Tuple<string[], UEBuildPlugin>(Rules.PostBuildSteps.ToArray(), null));
				}
				foreach(UEBuildPlugin BuildPlugin in BuildPlugins.Where(x => x.Descriptor.PostBuildSteps != null))
				{
					AddCustomBuildSteps(BuildPlugin.Descriptor.PostBuildSteps, BuildPlugin, PostBuildCommandBatches);
				}
			}
			return WriteCustomBuildStepScripts(BuildHostPlatform.Current.Platform, ProjectIntermediateDirectory, "PostBuild", PostBuildCommandBatches);
		}

		/// <summary>
		/// Adds custom build steps from the given JSON object to the list of command batches
		/// </summary>
		/// <param name="BuildSteps">The custom build steps</param>
		/// <param name="Plugin">The plugin to associate with these commands</param>
		/// <param name="CommandBatches">List to receive the command batches</param>
		private void AddCustomBuildSteps(CustomBuildSteps BuildSteps, UEBuildPlugin Plugin, List<Tuple<string[], UEBuildPlugin>> CommandBatches)
		{
			string[] Commands;
			if(BuildSteps.TryGetCommands(BuildHostPlatform.Current.Platform, out Commands))
			{
				CommandBatches.Add(Tuple.Create(Commands, Plugin));
			}
		}

		/// <summary>
		/// Gets a list of variables that can be expanded in paths referenced by this target
		/// </summary>
		/// <param name="Plugin">The current plugin</param>
		/// <returns>Map of variable names to values</returns>
		private Dictionary<string, string> GetTargetVariables(UEBuildPlugin Plugin)
		{
			Dictionary<string, string> Variables = new Dictionary<string,string>();
			Variables.Add("RootDir", UnrealBuildTool.RootDirectory.FullName);
			Variables.Add("EngineDir", UnrealBuildTool.EngineDirectory.FullName);
			Variables.Add("EnterpriseDir", UnrealBuildTool.EnterpriseDirectory.FullName);
			Variables.Add("ProjectDir", ProjectDirectory.FullName);
			Variables.Add("TargetName", TargetName);
			Variables.Add("TargetPlatform", Platform.ToString());
			Variables.Add("TargetConfiguration", Configuration.ToString());
			Variables.Add("TargetType", TargetType.ToString());
			if(ProjectFile != null)
			{
				Variables.Add("ProjectFile", ProjectFile.FullName);
			}
			if(Plugin != null)
			{
				Variables.Add("PluginDir", Plugin.Directory.FullName);
			}
			return Variables;
		}

		/// <summary>
		/// Write scripts containing the custom build steps for the given host platform
		/// </summary>
		/// <param name="HostPlatform">The current host platform</param>
		/// <param name="Directory">The output directory for the scripts</param>
		/// <param name="FilePrefix">Bare prefix for all the created script files</param>
		/// <param name="CommandBatches">List of custom build steps, and their matching PluginInfo (if appropriate)</param>
		/// <returns>List of created script files</returns>
		private FileReference[] WriteCustomBuildStepScripts(UnrealTargetPlatform HostPlatform, DirectoryReference Directory, string FilePrefix, List<Tuple<string[], UEBuildPlugin>> CommandBatches)
		{
			List<FileReference> ScriptFiles = new List<FileReference>();
			foreach(Tuple<string[], UEBuildPlugin> CommandBatch in CommandBatches)
			{
				// Find all the standard variables
				Dictionary<string, string> Variables = GetTargetVariables(CommandBatch.Item2);

				// Get the output path to the script
				string ScriptExtension = (HostPlatform == UnrealTargetPlatform.Win64)? ".bat" : ".sh";
				FileReference ScriptFile = FileReference.Combine(Directory, String.Format("{0}-{1}{2}", FilePrefix, ScriptFiles.Count + 1, ScriptExtension));

				// Write it to disk
				List<string> Contents = new List<string>();
				if(HostPlatform == UnrealTargetPlatform.Win64)
				{
					Contents.Insert(0, "@echo off");
				}
				foreach(string Command in CommandBatch.Item1)
				{
					Contents.Add(Utils.ExpandVariables(Command, Variables));
				}
				if(!DirectoryReference.Exists(ScriptFile.Directory))
				{
					DirectoryReference.CreateDirectory(ScriptFile.Directory);
				}
				File.WriteAllLines(ScriptFile.FullName, Contents);

				// Add the output file to the list of generated scripts
				ScriptFiles.Add(ScriptFile);
			}
			return ScriptFiles.ToArray();
		}

		private static FileReference AddModuleFilenameSuffix(string ModuleName, FileReference FilePath, string Suffix)
		{
			int MatchPos = FilePath.FullName.LastIndexOf(ModuleName, StringComparison.InvariantCultureIgnoreCase);
			if (MatchPos < 0)
			{
				throw new BuildException("Failed to find module name \"{0}\" specified on the command line inside of the output filename \"{1}\" to add appendage.", ModuleName, FilePath);
			}
			string Appendage = "-" + Suffix;
			return new FileReference(FilePath.FullName.Insert(MatchPos + ModuleName.Length, Appendage));
		}

		/// <summary>
		/// Finds a list of module names which can be hot-reloaded
		/// </summary>
		/// <returns>Set of module names</returns>
		private HashSet<string> GetHotReloadModuleNames()
		{
			HashSet<string> HotReloadModuleNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			foreach (UEBuildBinary Binary in Binaries)
			{
				List<UEBuildModule> GameModules = Binary.FindGameModules();
				if (GameModules != null && GameModules.Count > 0)
				{
					if(!UnrealBuildTool.IsProjectInstalled() || EnabledPlugins.Where(x => x.Type == PluginType.Mod).Any(x => Binary.OutputFilePaths[0].IsUnderDirectory(x.Directory)))
					{
						HotReloadModuleNames.UnionWith(GameModules.OfType<UEBuildModuleCPP>().Select(x => x.Name));
					}
				}
			}
			return HotReloadModuleNames;
		}

		/// <summary>
		/// Determines which modules can be used to create shared PCHs
		/// </summary>
		/// <param name="OriginalBinaries">The list of binaries</param>
		/// <param name="GlobalCompileEnvironment">The compile environment. The shared PCHs will be added to the SharedPCHs list in this.</param>
		void FindSharedPCHs(List<UEBuildBinary> OriginalBinaries, CppCompileEnvironment GlobalCompileEnvironment)
		{
			// Find how many other shared PCH modules each module depends on, and use that to sort the shared PCHs by reverse order of size.
			HashSet<UEBuildModuleCPP> SharedPCHModules = new HashSet<UEBuildModuleCPP>();
			foreach(UEBuildBinary Binary in OriginalBinaries)
			{
				foreach(UEBuildModuleCPP Module in Binary.Modules.OfType<UEBuildModuleCPP>())
				{
					if(Module.Rules.SharedPCHHeaderFile != null)
					{
						SharedPCHModules.Add(Module);
					}
				}
			}

			// Shared PCHs are only supported for engine modules at the moment. Check there are no game modules in the list.
			List<UEBuildModuleCPP> NonEngineSharedPCHs = SharedPCHModules.Where(x => !x.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory)).ToList();
			if(NonEngineSharedPCHs.Count > 0)
			{
				throw new BuildException("Shared PCHs are only supported for engine modules (found {0}).", String.Join(", ", NonEngineSharedPCHs.Select(x => x.Name)));
			}

			// Find a priority for each shared PCH, determined as the number of other shared PCHs it includes.
			Dictionary<UEBuildModuleCPP, int> SharedPCHModuleToPriority = new Dictionary<UEBuildModuleCPP, int>();
			foreach(UEBuildModuleCPP SharedPCHModule in SharedPCHModules)
			{
				List<UEBuildModule> Dependencies = new List<UEBuildModule>();
				SharedPCHModule.GetAllDependencyModules(Dependencies, new HashSet<UEBuildModule>(), false, false, false);
				SharedPCHModuleToPriority.Add(SharedPCHModule, Dependencies.Count(x => SharedPCHModules.Contains(x)));
			}

			// Create the shared PCH modules, in order
			List<PrecompiledHeaderTemplate> OrderedSharedPCHModules = GlobalCompileEnvironment.SharedPCHs;
			foreach(UEBuildModuleCPP Module in SharedPCHModuleToPriority.OrderByDescending(x => x.Value).Select(x => x.Key))
			{
				OrderedSharedPCHModules.Add(Module.CreateSharedPCHTemplate(this, GlobalCompileEnvironment));
			}

			// Print the ordered list of shared PCHs
			if(OrderedSharedPCHModules.Count > 0)
			{
				Log.TraceVerbose("Found {0} shared PCH headers (listed in order of preference):", SharedPCHModules.Count);
				foreach (PrecompiledHeaderTemplate SharedPCHModule in OrderedSharedPCHModules)
				{
					Log.TraceVerbose("	" + SharedPCHModule.Module.Name);
				}
			}
		}

		/// <summary>
		/// When building a target, this is called to add any additional modules that should be compiled along
		/// with the main target.  If you override this in a derived class, remember to call the base implementation!
		/// </summary>
		protected void AddExtraModules()
		{
			// Find all the extra module names
			List<string> ExtraModuleNames = new List<string>();
			ExtraModuleNames.AddRange(Rules.ExtraModuleNames);
			UEBuildPlatform.GetBuildPlatform(Platform).AddExtraModules(Rules, ExtraModuleNames);

			// Add extra modules that will either link into the main binary (monolithic), or be linked into separate DLL files (modular)
			foreach (string ModuleName in ExtraModuleNames)
			{
				UEBuildModuleCPP Module = FindOrCreateCppModuleByName(ModuleName, TargetRulesFile.GetFileName());
				if (Module.Binary == null)
				{
					AddModuleToBinary(Module);
				}
			}
		}

		/// <summary>
		/// Adds all the precompiled modules into the target. Precompiled modules are compiled alongside the target, but not linked into it unless directly referenced.
		/// </summary>
		protected void AddAllValidModulesToTarget()
		{
			// Find all the modules that are part of the target
			HashSet<string> ValidModuleNames = new HashSet<string>();
			foreach (UEBuildModuleCPP Module in Modules.Values.OfType<UEBuildModuleCPP>())
			{
				if(Module.Binary != null)
				{
					ValidModuleNames.Add(Module.Name);
				}
			}

			// Whether to allow developer modules
			bool bAllowDeveloperModules = (Configuration != UnrealTargetConfiguration.Shipping);

			// Find all the platform folders to exclude from the list of valid modules
			ReadOnlyHashSet<string> ExcludeFolders = UEBuildPlatform.GetBuildPlatform(Platform).GetExcludedFolderNames();

			// Set of module names to build
			HashSet<string> FilteredModuleNames = new HashSet<string>();

			// Only add engine modules for non-program targets. Programs only compile whitelisted modules through plugins.
			if(TargetType != TargetType.Program)
			{
				// Find all the known module names in this assembly
				List<string> ModuleNames = new List<string>();
				RulesAssembly.GetAllModuleNames(ModuleNames);

				// Find all the directories containing engine modules that may be compatible with this target
				List<DirectoryReference> Directories = new List<DirectoryReference>();
				if (TargetType == TargetType.Editor)
				{
					Directories.Add(UnrealBuildTool.EngineSourceEditorDirectory);
				}
				Directories.Add(UnrealBuildTool.EngineSourceRuntimeDirectory);

				// Also allow anything in the developer directory in non-shipping configurations (though we blacklist by default unless the PrecompileForTargets
				// setting indicates that it's actually useful at runtime).
				if(bAllowDeveloperModules)
				{
					Directories.Add(UnrealBuildTool.EngineSourceDeveloperDirectory);
					Directories.Add(DirectoryReference.Combine(UnrealBuildTool.EnterpriseSourceDirectory, "Developer"));
				}

				// Find all the modules that are not part of the standard set
				foreach (string ModuleName in ModuleNames)
				{
					FileReference ModuleFileName = RulesAssembly.GetModuleFileName(ModuleName);
					foreach(DirectoryReference BaseDir in Directories)
					{
						if(ModuleFileName.IsUnderDirectory(BaseDir))
						{
							Type RulesType = RulesAssembly.GetModuleRulesType(ModuleName);

							SupportedPlatformsAttribute SupportedPlatforms = RulesType.GetCustomAttribute<SupportedPlatformsAttribute>();
							if(SupportedPlatforms != null)
							{
								if(SupportedPlatforms.Platforms.Contains(Platform))
								{
									FilteredModuleNames.Add(ModuleName);
								}
							}
							else
							{
								if(!ModuleFileName.ContainsAnyNames(ExcludeFolders, BaseDir))
								{
									FilteredModuleNames.Add(ModuleName);
								}
							}
						}
					}
				}
			}

			// Add all the plugin modules that need to be compiled
			List<PluginInfo> Plugins = RulesAssembly.EnumeratePlugins().ToList();
			foreach(PluginInfo Plugin in Plugins)
			{
				// Ignore plugins without any modules
				if(Plugin.Descriptor.Modules == null)
				{
					continue;
				}

				// Disable any plugin which does not support the target platform. The editor should update such references in the .uproject file on load.
				if (!Rules.bIncludePluginsForTargetPlatforms && !Plugin.Descriptor.SupportsTargetPlatform(Platform))
				{
					continue;
				}

				// Disable any plugin that requires the build platform
				if(Plugin.Descriptor.bRequiresBuildPlatform && ShouldExcludePlugin(Plugin, ExcludeFolders))
				{
					continue;
				}

				// Disable any plugins that aren't compatible with this program
				if (TargetType == TargetType.Program && (Plugin.Descriptor.SupportedPrograms == null || !Plugin.Descriptor.SupportedPrograms.Contains(AppName)))
				{
					continue;
				}

				// Add all the modules
				foreach (ModuleDescriptor ModuleDescriptor in Plugin.Descriptor.Modules)
				{
					if (ModuleDescriptor.IsCompiledInConfiguration(Platform, Configuration, TargetName, TargetType, bAllowDeveloperModules && Rules.bBuildDeveloperTools, Rules.bBuildEditor, Rules.bBuildRequiresCookedData))
					{
						FileReference ModuleFileName = RulesAssembly.GetModuleFileName(ModuleDescriptor.Name);
						if(ModuleFileName == null)
						{
							throw new BuildException("Unable to find module '{0}' referenced by {1}", ModuleDescriptor.Name, Plugin.File);
						}
						if(!ModuleFileName.ContainsAnyNames(ExcludeFolders, Plugin.Directory))
						{
							FilteredModuleNames.Add(ModuleDescriptor.Name);
						}
					}
				}
			}

			// Create rules for each remaining module, and check that it's set to be compiled
			foreach(string FilteredModuleName in FilteredModuleNames)
			{
				// Try to create the rules object, but catch any exceptions if it fails. Some modules (eg. SQLite) may determine that they are unavailable in the constructor.
				ModuleRules ModuleRules;
				try
				{
					ModuleRules = RulesAssembly.CreateModuleRules(FilteredModuleName, this.Rules, "all modules option");
				}
				catch (BuildException)
				{
					ModuleRules = null;
				}

				// Figure out if it can be precompiled
				if (ModuleRules != null && ModuleRules.IsValidForTarget(ModuleRules.File))
				{
					ValidModuleNames.Add(FilteredModuleName);
				}
			}

			// Now create all the precompiled modules, making sure they don't reference anything that's not in the precompiled set
			HashSet<UEBuildModuleCPP> ValidModules = new HashSet<UEBuildModuleCPP>();
			foreach(string ModuleName in ValidModuleNames)
			{
				const string PrecompileReferenceChain = "allmodules option";
				UEBuildModuleCPP Module = (UEBuildModuleCPP)FindOrCreateModuleByName(ModuleName, PrecompileReferenceChain);
				Module.RecursivelyCreateModules(FindOrCreateModuleByName, PrecompileReferenceChain);
				ValidModules.Add(Module);
			}

			// Make sure precompiled modules don't reference any non-precompiled modules
			foreach(UEBuildModuleCPP ValidModule in ValidModules)
			{
				foreach(UEBuildModuleCPP ReferencedModule in ValidModule.GetDependencies(false, true).OfType<UEBuildModuleCPP>())
				{
					if(!ValidModules.Contains(ReferencedModule))
					{
						Log.TraceWarning("Module '{0}' is not usable without module '{1}', which is not valid for this target.", ValidModule.Name, ReferencedModule.Name);
					}
				}
			}

			// Make sure every module is built
			foreach(UEBuildModuleCPP Module in ValidModules)
			{
				if(Module.Binary == null)
				{
					AddModuleToBinary(Module);
				}
			}
		}

		public void AddModuleToBinary(UEBuildModuleCPP Module)
		{
			if (ShouldCompileMonolithic())
			{
				// When linking monolithically, any unbound modules will be linked into the main executable
				Module.Binary = Binaries[0];
				Module.Binary.AddModule(Module);
			}
			else
			{
				// Otherwise create a new module for it
				Module.Binary = CreateDynamicLibraryForModule(Module);
				Binaries.Add(Module.Binary);
			}
		}

		/// <summary>
		/// Finds the base output directory for build products of the given module
		/// </summary>
		/// <param name="ModuleRules">The rules object created for this module</param>
		/// <returns>The base output directory for compiled object files for this module</returns>
		private DirectoryReference GetBaseOutputDirectory(ModuleRules ModuleRules)
		{
			// Get the root output directory and base name (target name/app name) for this binary
			DirectoryReference BaseOutputDirectory;
			if (ModuleRules.Plugin != null)
			{
				BaseOutputDirectory = ModuleRules.Plugin.Directory;
			}
			else if (RulesAssembly.IsGameModule(ModuleRules.Name) || !bUseSharedBuildEnvironment)
			{
				BaseOutputDirectory = ProjectDirectory;
			}
			else
			{
				if (RulesAssembly.GetModuleFileName(ModuleRules.Name).IsUnderDirectory(UnrealBuildTool.EnterpriseDirectory))
				{
					BaseOutputDirectory = UnrealBuildTool.EnterpriseDirectory;
				}
				else
				{
					BaseOutputDirectory = UnrealBuildTool.EngineDirectory;
				}
			}
			return BaseOutputDirectory;
		}

		/// <summary>
		/// Finds the base output directory for a module
		/// </summary>
		/// <param name="ModuleRules">The rules object created for this module</param>
		/// <returns>The output directory for compiled object files for this module</returns>
		private DirectoryReference GetModuleIntermediateDirectory(ModuleRules ModuleRules)
		{
			// Get the root output directory and base name (target name/app name) for this binary
			DirectoryReference BaseOutputDirectory = GetBaseOutputDirectory(ModuleRules);

			// Get the configuration that this module will be built in. Engine modules compiled in DebugGame will use Development.
			UnrealTargetConfiguration ModuleConfiguration = Configuration;
			if (Configuration == UnrealTargetConfiguration.DebugGame && !RulesAssembly.IsGameModule(ModuleRules.Name) && !ModuleRules.Name.Equals(Rules.LaunchModuleName, StringComparison.InvariantCultureIgnoreCase))
			{
				ModuleConfiguration = UnrealTargetConfiguration.Development;
			}

			// Get the output and intermediate directories for this module
			DirectoryReference IntermediateDirectory = DirectoryReference.Combine(BaseOutputDirectory, PlatformIntermediateFolder, AppName, ModuleConfiguration.ToString());

			// Append a subdirectory if the module rules specifies one
			if (ModuleRules != null && !String.IsNullOrEmpty(ModuleRules.BinariesSubFolder))
			{
				IntermediateDirectory = DirectoryReference.Combine(IntermediateDirectory, ModuleRules.BinariesSubFolder);
			}

			return DirectoryReference.Combine(IntermediateDirectory, ModuleRules.ShortName ?? ModuleRules.Name);
		}

		/// <summary>
		/// Adds a dynamic library for the given module. Does not check whether a binary already exists, or whether a binary should be created for this build configuration.
		/// </summary>
		/// <param name="Module">The module to create a binary for</param>
		/// <returns>The new binary. This has not been added to the target.</returns>
		private UEBuildBinary CreateDynamicLibraryForModule(UEBuildModuleCPP Module)
		{
			// Get the root output directory and base name (target name/app name) for this binary
			DirectoryReference BaseOutputDirectory = GetBaseOutputDirectory(Module.Rules);
			DirectoryReference OutputDirectory = DirectoryReference.Combine(BaseOutputDirectory, "Binaries", Platform.ToString());

			// Append a subdirectory if the module rules specifies one
			if (Module.Rules != null && !String.IsNullOrEmpty(Module.Rules.BinariesSubFolder))
			{
				OutputDirectory = DirectoryReference.Combine(OutputDirectory, Module.Rules.BinariesSubFolder);
			}

			// Get the configuration that this module will be built in. Engine modules compiled in DebugGame will use Development.
			UnrealTargetConfiguration ModuleConfiguration = Configuration;
			if (Configuration == UnrealTargetConfiguration.DebugGame && !RulesAssembly.IsGameModule(Module.Name))
			{
				ModuleConfiguration = UnrealTargetConfiguration.Development;
			}

            // Get the output filenames
            FileReference BaseBinaryPath = FileReference.Combine(OutputDirectory, MakeBinaryFileName(AppName + "-" + Module.Name, Platform, ModuleConfiguration, Architecture, Rules.UndecoratedConfiguration, UEBuildBinaryType.DynamicLinkLibrary));
			List<FileReference> OutputFilePaths = UEBuildPlatform.GetBuildPlatform(Platform).FinalizeBinaryPaths(BaseBinaryPath, ProjectFile, Rules);

			// Create the binary
			return new UEBuildBinary(
				Type: UEBuildBinaryType.DynamicLinkLibrary,
				OutputFilePaths: OutputFilePaths,
				IntermediateDirectory: Module.IntermediateDirectory,
				bAllowExports: true,
				PrimaryModule: Module,
				bUsePrecompiled: Module.Rules.bUsePrecompiled
			);
		}

        /// <summary>
        /// Makes a filename (without path) for a compiled binary (e.g. "Core-Win64-Debug.lib") */
        /// </summary>
        /// <param name="BinaryName">The name of this binary</param>
        /// <param name="Platform">The platform being built for</param>
        /// <param name="Configuration">The configuration being built</param>
		/// <param name="Architecture">The target architecture being built</param>
        /// <param name="UndecoratedConfiguration">The target configuration which doesn't require a platform and configuration suffix. Development by default.</param>
        /// <param name="BinaryType">Type of binary</param>
        /// <returns>Name of the binary</returns>
        public static string MakeBinaryFileName(string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, UnrealTargetConfiguration UndecoratedConfiguration, UEBuildBinaryType BinaryType)
		{
			StringBuilder Result = new StringBuilder();

			if (Platform == UnrealTargetPlatform.Linux && (BinaryType == UEBuildBinaryType.DynamicLinkLibrary || BinaryType == UEBuildBinaryType.StaticLibrary))
			{
				Result.Append("lib");
			}

			Result.Append(BinaryName);

			if (Configuration != UndecoratedConfiguration)
			{
				Result.AppendFormat("-{0}-{1}", Platform.ToString(), Configuration.ToString());
			}

			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);
			if(BuildPlatform.RequiresArchitectureSuffix())
			{
				Result.Append(Architecture);
			}

			Result.Append(BuildPlatform.GetBinaryExtension(BinaryType));

            return Result.ToString();
		}

        /// <summary>
        /// Determine the output path for a target's executable
        /// </summary>
        /// <param name="BaseDirectory">The base directory for the executable; typically either the engine directory or project directory.</param>
        /// <param name="BinaryName">Name of the binary</param>
        /// <param name="Platform">Target platform to build for</param>
        /// <param name="Configuration">Target configuration being built</param>
		/// <param name="Architecture">Architecture being built</param>
        /// <param name="BinaryType">The type of binary we're compiling</param>
        /// <param name="UndecoratedConfiguration">The configuration which doesn't have a "-{Platform}-{Configuration}" suffix added to the binary</param>
        /// <param name="bIncludesGameModules">Whether this executable contains game modules</param>
        /// <param name="ExeSubFolder">Subfolder for executables. May be null.</param>
		/// <param name="ProjectFile">The project file containing the target being built</param>
		/// <param name="Rules">Rules for the target being built</param>
        /// <returns>List of executable paths for this target</returns>
        public static List<FileReference> MakeBinaryPaths(DirectoryReference BaseDirectory, string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, UEBuildBinaryType BinaryType, string Architecture, UnrealTargetConfiguration UndecoratedConfiguration, bool bIncludesGameModules, string ExeSubFolder, FileReference ProjectFile, ReadOnlyTargetRules Rules)
		{
			// Get the configuration for the executable. If we're building DebugGame, and this executable only contains engine modules, use the same name as development.
			UnrealTargetConfiguration ExeConfiguration = Configuration;

			// Build the binary path
			DirectoryReference BinaryDirectory = DirectoryReference.Combine(BaseDirectory, "Binaries", Platform.ToString());
			if (!String.IsNullOrEmpty(ExeSubFolder))
			{
				BinaryDirectory = DirectoryReference.Combine(BinaryDirectory, ExeSubFolder);
			}
			FileReference BinaryFile = FileReference.Combine(BinaryDirectory, MakeBinaryFileName(BinaryName, Platform, ExeConfiguration, Architecture, UndecoratedConfiguration, BinaryType));

			// Allow the platform to customize the output path (and output several executables at once if necessary)
			return UEBuildPlatform.GetBuildPlatform(Platform).FinalizeBinaryPaths(BinaryFile, ProjectFile, Rules);
		}

		/// <summary>
		/// Sets up the plugins for this target
		/// </summary>
		protected virtual void SetupPlugins()
		{
			// Find all the valid plugins
			Dictionary<string, PluginInfo> NameToInfo = RulesAssembly.EnumeratePlugins().ToDictionary(x => x.Name, x => x, StringComparer.InvariantCultureIgnoreCase);

			// Remove any plugins for platforms we don't have
			List<UnrealTargetPlatform> MissingPlatforms = new List<UnrealTargetPlatform>();
			foreach (UnrealTargetPlatform TargetPlatform in Enum.GetValues(typeof(UnrealTargetPlatform)))
			{
				if (UEBuildPlatform.GetBuildPlatform(TargetPlatform, true) == null)
				{
					MissingPlatforms.Add(TargetPlatform);
				}
			}

			// Get an array of folders to filter out
			string[] ExcludeFolders = MissingPlatforms.Select(x => x.ToString()).ToArray();

			// Set of all the plugins that have been referenced
			HashSet<string> ReferencedNames = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);

			// Map of plugin names to instances of that plugin
			Dictionary<string, UEBuildPlugin> NameToInstance = new Dictionary<string, UEBuildPlugin>(StringComparer.InvariantCultureIgnoreCase);

			// Set up the foreign plugin
			if(ForeignPlugin != null)
			{
				PluginReferenceDescriptor PluginReference = new PluginReferenceDescriptor(ForeignPlugin.GetFileNameWithoutExtension(), null, true);
				AddPlugin(PluginReference, "command line", ExcludeFolders, NameToInstance, NameToInfo);
			}

			// Configure plugins explicitly enabled via target settings
			foreach(string PluginName in Rules.EnablePlugins)
			{
				if(ReferencedNames.Add(PluginName))
				{
					PluginReferenceDescriptor PluginReference = new PluginReferenceDescriptor(PluginName, null, true);
					AddPlugin(PluginReference, "target settings", ExcludeFolders, NameToInstance, NameToInfo);
				}
			}

			// Configure plugins explicitly disabled via target settings
			foreach(string PluginName in Rules.DisablePlugins)
			{
				if(ReferencedNames.Add(PluginName))
				{
					PluginReferenceDescriptor PluginReference = new PluginReferenceDescriptor(PluginName, null, false);
					AddPlugin(PluginReference, "target settings", ExcludeFolders, NameToInstance, NameToInfo);
				}
			}

			// Find a map of plugins which are explicitly referenced in the project file
			if(ProjectDescriptor != null && ProjectDescriptor.Plugins != null)
			{
				string ProjectReferenceChain = ProjectFile.GetFileName();
				foreach(PluginReferenceDescriptor PluginReference in ProjectDescriptor.Plugins)
				{
					if(!Rules.EnablePlugins.Contains(PluginReference.Name, StringComparer.InvariantCultureIgnoreCase) && !Rules.DisablePlugins.Contains(PluginReference.Name, StringComparer.InvariantCultureIgnoreCase))
					{
						// Make sure we don't have multiple references to the same plugin
						if(!ReferencedNames.Add(PluginReference.Name))
						{
							Log.TraceWarning("Plugin '{0}' is listed multiple times in project file '{1}'.", PluginReference.Name, ProjectFile);
						}
						else
						{
							AddPlugin(PluginReference, ProjectReferenceChain, ExcludeFolders, NameToInstance, NameToInfo);
						}
					}
				}
			}

			// Also synthesize references for plugins which are enabled by default
			if (Rules.bCompileAgainstEngine || Rules.bCompileWithPluginSupport)
			{
				foreach(PluginInfo Plugin in NameToInfo.Values)
				{
					if(Plugin.EnabledByDefault && !ReferencedNames.Contains(Plugin.Name))
					{
						ReferencedNames.Add(Plugin.Name);

						PluginReferenceDescriptor PluginReference = new PluginReferenceDescriptor(Plugin.Name, null, true);
						PluginReference.bOptional = true;

						AddPlugin(PluginReference, "default plugins", ExcludeFolders, NameToInstance, NameToInfo);
					}
				}
			}

			// If this is a program, synthesize references for plugins which are enabled via the config file
			if(TargetType == TargetType.Program)
			{
				ConfigHierarchy EngineConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, "Programs", TargetName), Platform);

				List<string> PluginNames;
				if(EngineConfig.GetArray("Plugins", "ProgramEnabledPlugins", out PluginNames))
				{
					foreach(string PluginName in PluginNames)
					{
						if(ReferencedNames.Add(PluginName))
						{
							PluginReferenceDescriptor PluginReference = new PluginReferenceDescriptor(PluginName, null, true);
							AddPlugin(PluginReference, "DefaultEngine.ini", ExcludeFolders, NameToInstance, NameToInfo);
						}
					}
				}
			}

			// Create the list of enabled plugins
			EnabledPlugins = new List<UEBuildPlugin>(NameToInstance.Values);

			// Set the list of plugins that should be built
			BuildPlugins = new List<UEBuildPlugin>(NameToInstance.Values);

			// Determine if the project has a script plugin. We will always build UHT if there is a script plugin in the game folder.
			bHasProjectScriptPlugin = EnabledPlugins.Any(x => x.Descriptor.SupportedPrograms != null && x.Descriptor.SupportedPrograms.Contains("UnrealHeaderTool"));
		}

		/// <summary>
		/// Creates a plugin instance from a reference to it
		/// </summary>
		/// <param name="Reference">Reference to the plugin</param>
		/// <param name="ReferenceChain">Textual representation of the chain of references, for error reporting</param>
		/// <param name="ExcludeFolders">Array of folder names to be excluded</param>
		/// <param name="NameToInstance">Map from plugin name to instance of it</param>
		/// <param name="NameToInfo">Map from plugin name to information</param>
		/// <returns>Instance of the plugin, or null if it should not be used</returns>
		private UEBuildPlugin AddPlugin(PluginReferenceDescriptor Reference, string ReferenceChain, string[] ExcludeFolders, Dictionary<string, UEBuildPlugin> NameToInstance, Dictionary<string, PluginInfo> NameToInfo)
		{
			// Ignore disabled references
			if(!Reference.bEnabled)
			{
				return null;
			}

			// Try to get an existing reference to this plugin
			UEBuildPlugin Instance;
			if(NameToInstance.TryGetValue(Reference.Name, out Instance))
			{
				// If this is a non-optional reference, make sure that and every referenced dependency is staged
				if(!Reference.bOptional && !Instance.bDescriptorReferencedExplicitly)
				{
					Instance.bDescriptorReferencedExplicitly = true;
					if(Instance.Descriptor.Plugins != null)
					{
						foreach(PluginReferenceDescriptor NextReference in Instance.Descriptor.Plugins)
						{
							string NextReferenceChain = String.Format("{0} -> {1}", ReferenceChain, Instance.File.GetFileName());
							AddPlugin(NextReference, NextReferenceChain, ExcludeFolders, NameToInstance, NameToInfo);
						}
					}
				}
			}
			else
			{
				// Check if the plugin is required for this platform
				if(!Reference.IsEnabledForPlatform(Platform) || !Reference.IsEnabledForTargetConfiguration(Configuration) || !Reference.IsEnabledForTarget(TargetType))
				{
					Log.TraceLog("Ignoring plugin '{0}' (referenced via {1}) for platform/configuration", Reference.Name, ReferenceChain);
					return null;
				}

				// Disable any plugin reference which does not support the target platform
				if (!Rules.bIncludePluginsForTargetPlatforms && !Reference.IsSupportedTargetPlatform(Platform))
				{
					Log.TraceLog("Ignoring plugin '{0}' (referenced via {1}) due to unsupported target platform.", Reference.Name, ReferenceChain);
					return null;
				}

				// Find the plugin being enabled
				PluginInfo Info;
				if(!NameToInfo.TryGetValue(Reference.Name, out Info))
				{
					if (Reference.bOptional)
					{
						return null;
					}
					else
					{
						throw new BuildException("Unable to find plugin '{0}' (referenced via {1}). Install it and try again, or remove it from the required plugin list.", Reference.Name, ReferenceChain);
					}
				}

				// Disable any plugin which does not support the target platform. The editor should update such references in the .uproject file on load.
				if (!Rules.bIncludePluginsForTargetPlatforms && !Info.Descriptor.SupportsTargetPlatform(Platform))
				{
					Log.TraceLog("Ignoring plugin '{0}' (referenced via {1}) due to target platform not supported by descriptor.", Reference.Name, ReferenceChain);
					return null;
				}

				// Disable any plugin that requires the build platform
				if(Info.Descriptor.bRequiresBuildPlatform && ShouldExcludePlugin(Info, ExcludeFolders))
				{
					Log.TraceLog("Ignoring plugin '{0}' (referenced via {1}) due to missing build platform", Reference.Name, ReferenceChain);
					return null;
				}

				// Disable any plugins that aren't compatible with this program
				if (Rules.Type == TargetType.Program && (Info.Descriptor.SupportedPrograms == null || !Info.Descriptor.SupportedPrograms.Contains(AppName)))
				{
					Log.TraceLog("Ignoring plugin '{0}' (referenced via {1}) due to absence from supported programs list.", Reference.Name, ReferenceChain);
					return null;
				}

				// Create the new instance and add it to the cache
				Log.TraceLog("Enabling plugin '{0}' (referenced via {1})", Reference.Name, ReferenceChain);
				Instance = new UEBuildPlugin(Info);
				Instance.bDescriptorReferencedExplicitly = !Reference.bOptional;
				NameToInstance.Add(Info.Name, Instance);

				// Get the reference chain for this plugin
				string PluginReferenceChain = String.Format("{0} -> {1}", ReferenceChain, Info.File.GetFileName());

				// Create modules for this plugin
				UEBuildBinaryType BinaryType = ShouldCompileMonolithic() ? UEBuildBinaryType.StaticLibrary : UEBuildBinaryType.DynamicLinkLibrary;
				if (Info.Descriptor.Modules != null)
				{
					foreach (ModuleDescriptor ModuleInfo in Info.Descriptor.Modules)
					{
						if (ModuleInfo.IsCompiledInConfiguration(Platform, Configuration, TargetName, TargetType, Rules.bBuildDeveloperTools, Rules.bBuildEditor, Rules.bBuildRequiresCookedData))
						{
							UEBuildModuleCPP Module = FindOrCreateCppModuleByName(ModuleInfo.Name, PluginReferenceChain);
							if(!Instance.Modules.Contains(Module))
							{
								if (!Module.RulesFile.IsUnderDirectory(Info.Directory))
								{
									throw new BuildException("Plugin '{0}' (referenced via {1}) does not contain the '{2}' module, but lists it in '{3}'.", Info.Name, ReferenceChain, ModuleInfo.Name, Info.File);
								}
								Instance.bDescriptorNeededAtRuntime = true;
								Instance.Modules.Add(Module);
							}
						}
					}
				}

				// Create the dependencies set
				HashSet<UEBuildPlugin> Dependencies = new HashSet<UEBuildPlugin>();
				if(Info.Descriptor.Plugins != null)
				{
					foreach(PluginReferenceDescriptor NextReference in Info.Descriptor.Plugins)
					{
						UEBuildPlugin NextInstance = AddPlugin(NextReference, PluginReferenceChain, ExcludeFolders, NameToInstance, NameToInfo);
						if(NextInstance != null)
						{
							Dependencies.Add(NextInstance);
							if(NextInstance.Dependencies == null)
							{
								throw new BuildException("Found circular dependency from plugin '{0}' onto itself.", NextReference.Name);
							}
							Dependencies.UnionWith(NextInstance.Dependencies);
						}
					}
				}
				Instance.Dependencies = Dependencies;

				// Stage the descriptor if the plugin contains content
				if (Info.Descriptor.bCanContainContent || Dependencies.Any(x => x.bDescriptorNeededAtRuntime))
				{
					Instance.bDescriptorNeededAtRuntime = true;
				}
			}
			return Instance;
		}

		/// <summary>
		/// Checks whether a plugin path contains a platform directory fragment
		/// </summary>
		private bool ShouldExcludePlugin(PluginInfo Plugin, IEnumerable<string> ExcludeFolders)
		{
			if (Plugin.LoadedFrom == PluginLoadedFrom.Engine)
			{
				return Plugin.File.ContainsAnyNames(ExcludeFolders, UnrealBuildTool.EngineDirectory);
			}
			else if(ProjectFile != null)
			{
				return Plugin.File.ContainsAnyNames(ExcludeFolders, ProjectFile.Directory);
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Sets up the binaries for the target.
		/// </summary>
		protected void SetupBinaries()
		{
			// If we're using the new method for specifying binaries, fill in the binary configurations now
			if(Rules.LaunchModuleName == null)
			{
				throw new BuildException("LaunchModuleName must be set for all targets.");
			}

			// Create the launch module
			UEBuildModuleCPP LaunchModule = FindOrCreateCppModuleByName(Rules.LaunchModuleName, TargetRulesFile.GetFileName());

			// Get the intermediate directory for the launch module directory. This can differ from the standard engine intermediate directory because it is always configuration-specific.
			DirectoryReference IntermediateDirectory;
			if(LaunchModule.RulesFile.IsUnderDirectory(UnrealBuildTool.EngineDirectory) && !ShouldCompileMonolithic())
			{
				IntermediateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, PlatformIntermediateFolder, AppName, Configuration.ToString());
			}
			else
			{
				IntermediateDirectory = ProjectIntermediateDirectory;
			}

			// Construct the output paths for this target's executable
			DirectoryReference OutputDirectory;
			if (bCompileMonolithic || !bUseSharedBuildEnvironment)
			{
				OutputDirectory = ProjectDirectory;
			}
			else
			{
				OutputDirectory = UnrealBuildTool.EngineDirectory;
			}

			bool bCompileAsDLL = Rules.bShouldCompileAsDLL && bCompileMonolithic;
			List<FileReference> OutputPaths = MakeBinaryPaths(OutputDirectory, bCompileMonolithic ? TargetName : AppName, Platform, Configuration, bCompileAsDLL ? UEBuildBinaryType.DynamicLinkLibrary : UEBuildBinaryType.Executable, Rules.Architecture, Rules.UndecoratedConfiguration, bCompileMonolithic && ProjectFile != null, Rules.ExeBinariesSubFolder, ProjectFile, Rules);

			// Create the binary
			UEBuildBinary Binary = new UEBuildBinary(
				Type: Rules.bShouldCompileAsDLL? UEBuildBinaryType.DynamicLinkLibrary : UEBuildBinaryType.Executable,
				OutputFilePaths: OutputPaths,
				IntermediateDirectory: IntermediateDirectory,
				bAllowExports: Rules.bHasExports,
				PrimaryModule: LaunchModule,
				bUsePrecompiled: LaunchModule.Rules.bUsePrecompiled && OutputPaths[0].IsUnderDirectory(UnrealBuildTool.EngineDirectory)
			);
			Binaries.Add(Binary);

			// Add the launch module to it
			LaunchModule.Binary = Binary;
			Binary.AddModule(LaunchModule);

			// Create an additional console app for the editor
			if ((Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Mac) && Configuration != UnrealTargetConfiguration.Shipping && TargetType == TargetType.Editor)
			{
				Binary.bBuildAdditionalConsoleApp = true;
			}
		}

		/// <summary>
		/// Sets up the global compile and link environment for the target.
		/// </summary>
		private void SetupGlobalEnvironment(UEToolChain ToolChain, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(Platform);

			ToolChain.SetUpGlobalEnvironment(Rules);

			// @Hack: This to prevent UHT from listing CoreUObject.init.gen.cpp as its dependency.
			// We flag the compile environment when we build UHT so that we don't need to check
			// this for each file when generating their dependencies.
			GlobalCompileEnvironment.bHackHeaderGenerator = (AppName == "UnrealHeaderTool");

			GlobalCompileEnvironment.bUseDebugCRT = GlobalCompileEnvironment.Configuration == CppConfiguration.Debug && Rules.bDebugBuildsActuallyUseDebugCRT;
			GlobalCompileEnvironment.bEnableOSX109Support = Rules.bEnableOSX109Support;
			GlobalCompileEnvironment.Definitions.Add(String.Format("IS_PROGRAM={0}", TargetType == TargetType.Program ? "1" : "0"));
			GlobalCompileEnvironment.Definitions.AddRange(Rules.GlobalDefinitions);
			GlobalCompileEnvironment.bUseSharedBuildEnvironment = (Rules.BuildEnvironment == TargetBuildEnvironment.Shared);
			GlobalCompileEnvironment.bEnableExceptions = Rules.bForceEnableExceptions || Rules.bBuildEditor;
			GlobalCompileEnvironment.bEnableObjCExceptions = Rules.bForceEnableObjCExceptions || Rules.bBuildEditor;
			GlobalCompileEnvironment.bShadowVariableWarningsAsErrors = Rules.bShadowVariableErrors;
			GlobalCompileEnvironment.bUndefinedIdentifierWarningsAsErrors = Rules.bUndefinedIdentifierErrors;
			GlobalCompileEnvironment.bOptimizeForSize = Rules.bCompileForSize;
			GlobalCompileEnvironment.bUseStaticCRT = Rules.bUseStaticCRT;
			GlobalCompileEnvironment.bOmitFramePointers = Rules.bOmitFramePointers;
			GlobalCompileEnvironment.bUsePDBFiles = Rules.bUsePDBFiles;
			GlobalCompileEnvironment.bSupportEditAndContinue = Rules.bSupportEditAndContinue;
			GlobalCompileEnvironment.bUseIncrementalLinking = Rules.bUseIncrementalLinking;
			GlobalCompileEnvironment.bAllowLTCG = Rules.bAllowLTCG;
			GlobalCompileEnvironment.bPGOOptimize = Rules.bPGOOptimize;
			GlobalCompileEnvironment.bPGOProfile = Rules.bPGOProfile;
			GlobalCompileEnvironment.bAllowRemotelyCompiledPCHs = Rules.bAllowRemotelyCompiledPCHs;
			GlobalCompileEnvironment.bCheckSystemHeadersForModification = Rules.bCheckSystemHeadersForModification;
			GlobalCompileEnvironment.bPrintTimingInfo = Rules.bPrintToolChainTimingInfo;
			GlobalCompileEnvironment.bUseRTTI = Rules.bForceEnableRTTI;
			GlobalCompileEnvironment.bUseInlining = Rules.bUseInlining;
			GlobalCompileEnvironment.bHideSymbolsByDefault = Rules.bHideSymbolsByDefault;
			GlobalCompileEnvironment.CppStandard = Rules.CppStandard;
			GlobalCompileEnvironment.AdditionalArguments = Rules.AdditionalCompilerArguments;

			GlobalLinkEnvironment.bIsBuildingConsoleApplication = Rules.bIsBuildingConsoleApplication;
			GlobalLinkEnvironment.bOptimizeForSize = Rules.bCompileForSize;
			GlobalLinkEnvironment.bOmitFramePointers = Rules.bOmitFramePointers;
			GlobalLinkEnvironment.bSupportEditAndContinue = Rules.bSupportEditAndContinue;
			GlobalLinkEnvironment.bCreateMapFile = Rules.bCreateMapFile;
			GlobalLinkEnvironment.bHasExports = Rules.bHasExports;
			GlobalLinkEnvironment.bAllowASLR = (GlobalCompileEnvironment.Configuration == CppConfiguration.Shipping && Rules.bAllowASLRInShipping);
			GlobalLinkEnvironment.bUsePDBFiles = Rules.bUsePDBFiles;
			GlobalLinkEnvironment.BundleDirectory = BuildPlatform.GetBundleDirectory(Rules, Binaries[0].OutputFilePaths);
			GlobalLinkEnvironment.BundleVersion = Rules.BundleVersion;
			GlobalLinkEnvironment.bAllowLTCG = Rules.bAllowLTCG;
            GlobalLinkEnvironment.bPGOOptimize = Rules.bPGOOptimize;
            GlobalLinkEnvironment.bPGOProfile = Rules.bPGOProfile;
			GlobalLinkEnvironment.bUseIncrementalLinking = Rules.bUseIncrementalLinking;
			GlobalLinkEnvironment.bUseFastPDBLinking = Rules.bUseFastPDBLinking ?? false;
			GlobalLinkEnvironment.bPrintTimingInfo = Rules.bPrintToolChainTimingInfo;
			GlobalLinkEnvironment.AdditionalArguments = Rules.AdditionalLinkerArguments;
		
            if (Rules.bPGOOptimize && Rules.bPGOProfile)
            {
                throw new BuildException("bPGOProfile and bPGOOptimize are mutually exclusive.");
            }

            if (Rules.bPGOProfile)
            {
                GlobalCompileEnvironment.Definitions.Add("ENABLE_PGO_PROFILE=1");
            }
            else
            {
                GlobalCompileEnvironment.Definitions.Add("ENABLE_PGO_PROFILE=0");
            }

			// Toggle to enable vorbis for audio streaming where available
			GlobalCompileEnvironment.Definitions.Add("USE_VORBIS_FOR_STREAMING=1");

			// Add the 'Engine/Source' path as a global include path for all modules
			GlobalCompileEnvironment.UserIncludePaths.Add(UnrealBuildTool.EngineSourceDirectory);

			//@todo.PLATFORM: Do any platform specific tool chain initialization here if required

			UnrealTargetConfiguration EngineTargetConfiguration = Configuration == UnrealTargetConfiguration.DebugGame ? UnrealTargetConfiguration.Development : Configuration;
			DirectoryReference LinkIntermediateDirectory = DirectoryReference.Combine(UnrealBuildTool.EngineDirectory, PlatformIntermediateFolder, AppName, EngineTargetConfiguration.ToString());

			// Installed Engine intermediates go to the project's intermediate folder. Installed Engine never writes to the engine intermediate folder. (Those files are immutable)
			// Also, when compiling in monolithic, all intermediates go to the project's folder.  This is because a project can change definitions that affects all engine translation
			// units too, so they can't be shared between different targets.  They are effectively project-specific engine intermediates.
			if (UnrealBuildTool.IsEngineInstalled() || (ProjectFile != null && ShouldCompileMonolithic()))
			{
				if (ProjectFile != null)
				{
					LinkIntermediateDirectory = DirectoryReference.Combine(ProjectFile.Directory, PlatformIntermediateFolder, AppName, Configuration.ToString());
				}
				else if (ForeignPlugin != null)
				{
					LinkIntermediateDirectory = DirectoryReference.Combine(ForeignPlugin.Directory, PlatformIntermediateFolder, AppName, Configuration.ToString());
				}
			}

			// Put the non-executable output files (PDB, import library, etc) in the intermediate directory.
			GlobalLinkEnvironment.IntermediateDirectory = LinkIntermediateDirectory;
			GlobalLinkEnvironment.OutputDirectory = GlobalLinkEnvironment.IntermediateDirectory;

			// By default, shadow source files for this target in the root OutputDirectory
			GlobalLinkEnvironment.LocalShadowDirectory = GlobalLinkEnvironment.OutputDirectory;

			if(!String.IsNullOrEmpty(Rules.ExeBinariesSubFolder))
			{
				GlobalCompileEnvironment.Definitions.Add(String.Format("ENGINE_BASE_DIR_ADJUST={0}", Rules.ExeBinariesSubFolder.Replace('\\', '/').Trim('/').Count(x => x == '/') + 1));
			}

			if (Rules.bForceCompileDevelopmentAutomationTests)
            {
                GlobalCompileEnvironment.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=1");
            }
            else
            {
                switch(Configuration)
                {
                    case UnrealTargetConfiguration.Test:
                    case UnrealTargetConfiguration.Shipping:
                        GlobalCompileEnvironment.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=0");
                        break;
                    default:
                        GlobalCompileEnvironment.Definitions.Add("WITH_DEV_AUTOMATION_TESTS=1");
                        break;
                }
            }

            if (Rules.bForceCompilePerformanceAutomationTests)
            {
                GlobalCompileEnvironment.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=1");
            }
            else
            {
                switch (Configuration)
                {
                    case UnrealTargetConfiguration.Shipping:
                        GlobalCompileEnvironment.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=0");
                        break;
                    default:
                        GlobalCompileEnvironment.Definitions.Add("WITH_PERF_AUTOMATION_TESTS=1");
                        break;
                }
            }

			GlobalCompileEnvironment.Definitions.Add("UNICODE");
			GlobalCompileEnvironment.Definitions.Add("_UNICODE");
			GlobalCompileEnvironment.Definitions.Add("__UNREAL__");

			GlobalCompileEnvironment.Definitions.Add(String.Format("IS_MONOLITHIC={0}", ShouldCompileMonolithic() ? "1" : "0"));

			GlobalCompileEnvironment.Definitions.Add(String.Format("WITH_ENGINE={0}", Rules.bCompileAgainstEngine ? "1" : "0"));
			GlobalCompileEnvironment.Definitions.Add(String.Format("WITH_UNREAL_DEVELOPER_TOOLS={0}", Rules.bBuildDeveloperTools ? "1" : "0"));

			// Set a macro to control whether to initialize ApplicationCore. Command line utilities should not generally need this.
			if (Rules.bCompileAgainstApplicationCore)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_APPLICATION_CORE=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_APPLICATION_CORE=0");
			}

			if (Rules.bCompileAgainstCoreUObject)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_COREUOBJECT=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_COREUOBJECT=0");
			}

			if (Rules.bCompileWithStatsWithoutEngine)
			{
				GlobalCompileEnvironment.Definitions.Add("USE_STATS_WITHOUT_ENGINE=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("USE_STATS_WITHOUT_ENGINE=0");
			}

			if (Rules.bCompileWithPluginSupport)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_PLUGIN_SUPPORT=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_PLUGIN_SUPPORT=0");
			}

            if (Rules.bWithPerfCounters)
            {
                GlobalCompileEnvironment.Definitions.Add("WITH_PERFCOUNTERS=1");
            }
            else
            {
                GlobalCompileEnvironment.Definitions.Add("WITH_PERFCOUNTERS=0");
            }

			if (Rules.bUseLoggingInShipping)
			{
				GlobalCompileEnvironment.Definitions.Add("USE_LOGGING_IN_SHIPPING=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("USE_LOGGING_IN_SHIPPING=0");
			}

			if (Rules.bLoggingToMemoryEnabled)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_LOGGING_TO_MEMORY=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_LOGGING_TO_MEMORY=0");
			}

            if (Rules.bUseCacheFreedOSAllocs)
            {
                GlobalCompileEnvironment.Definitions.Add("USE_CACHE_FREED_OS_ALLOCS=1");
            }
            else
            {
                GlobalCompileEnvironment.Definitions.Add("USE_CACHE_FREED_OS_ALLOCS=0");
            }

			if (Rules.bUseChecksInShipping)
			{
				GlobalCompileEnvironment.Definitions.Add("USE_CHECKS_IN_SHIPPING=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("USE_CHECKS_IN_SHIPPING=0");
			}

			// bBuildEditor has now been set appropriately for all platforms, so this is here to make sure the #define 
			if (Rules.bBuildEditor)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_EDITOR=1");
			}
			else if (!GlobalCompileEnvironment.Definitions.Contains("WITH_EDITOR=0"))
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_EDITOR=0");
			}

			if (Rules.bBuildWithEditorOnlyData == false)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_EDITORONLY_DATA=0");
			}

			// Check if server-only code should be compiled out.
			if (Rules.bWithServerCode == true)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_SERVER_CODE=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_SERVER_CODE=0");
			}

			// Set the define for whether we're compiling with CEF3
			if (Rules.bCompileCEF3 && (Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64 || Platform == UnrealTargetPlatform.Mac || Platform == UnrealTargetPlatform.Linux))
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_CEF3=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_CEF3=0");
			}

			// Set the define for enabling live coding
			if(Rules.bWithLiveCoding)
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_LIVE_CODING=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_LIVE_CODING=0");
			}

			if (Rules.bUseXGEController &&
				Rules.Type == TargetType.Editor &&
				(Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64))
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_XGE_CONTROLLER=1");
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("WITH_XGE_CONTROLLER=0");
			}

			// Compile in the names of the module manifests
			GlobalCompileEnvironment.Definitions.Add(String.Format("UBT_MODULE_MANIFEST=\"{0}\"", ModuleManifest.GetStandardFileName(AppName, Platform, Configuration, Architecture, false)));
			GlobalCompileEnvironment.Definitions.Add(String.Format("UBT_MODULE_MANIFEST_DEBUGGAME=\"{0}\"", ModuleManifest.GetStandardFileName(AppName, Platform, UnrealTargetConfiguration.DebugGame, Architecture, true)));

			// tell the compiled code the name of the UBT platform (this affects folder on disk, etc that the game may need to know)
			GlobalCompileEnvironment.Definitions.Add("UBT_COMPILED_PLATFORM=" + Platform.ToString());
			GlobalCompileEnvironment.Definitions.Add("UBT_COMPILED_TARGET=" + TargetType.ToString());

			// Set the global app name
			GlobalCompileEnvironment.Definitions.Add(String.Format("UE_APP_NAME=\"{0}\"", AppName));

			// Initialize the compile and link environments for the platform, configuration, and project.
			BuildPlatform.SetUpEnvironment(Rules, GlobalCompileEnvironment, GlobalLinkEnvironment);
			BuildPlatform.SetUpConfigurationEnvironment(Rules, GlobalCompileEnvironment, GlobalLinkEnvironment);
		}

		static CppConfiguration GetCppConfiguration(UnrealTargetConfiguration Configuration)
		{
			switch (Configuration)
			{
				case UnrealTargetConfiguration.Debug:
					return CppConfiguration.Debug;
				case UnrealTargetConfiguration.DebugGame:
				case UnrealTargetConfiguration.Development:
					return CppConfiguration.Development;
				case UnrealTargetConfiguration.Shipping:
					return CppConfiguration.Shipping;
				case UnrealTargetConfiguration.Test:
					return CppConfiguration.Shipping;
				default:
					throw new BuildException("Unhandled target configuration");
			}
		}

        /// <summary>
        /// Create a rules object for the given module, and set any default values for this target
        /// </summary>
        private ModuleRules CreateModuleRulesAndSetDefaults(string ModuleName, string ReferenceChain)
		{
			// Create the rules from the assembly
			ModuleRules RulesObject = RulesAssembly.CreateModuleRules(ModuleName, Rules, ReferenceChain);

			// Set whether the module requires an IMPLEMENT_MODULE macro
			if(!RulesObject.bRequiresImplementModule.HasValue)
			{
				RulesObject.bRequiresImplementModule = (RulesObject.Type == ModuleRules.ModuleType.CPlusPlus && RulesObject.Name != Rules.LaunchModuleName);
			}

			// Reads additional dependencies array for project module from project file and fills PrivateDependencyModuleNames. 
			if (ProjectDescriptor != null && ProjectDescriptor.Modules != null)
			{
				ModuleDescriptor Module = ProjectDescriptor.Modules.FirstOrDefault(x => x.Name.Equals(ModuleName, StringComparison.InvariantCultureIgnoreCase));
				if (Module != null && Module.AdditionalDependencies != null)
				{
					RulesObject.PrivateDependencyModuleNames.AddRange(Module.AdditionalDependencies);
				}
			}

			// Make sure include paths don't end in trailing slashes. This can result in enclosing quotes being escaped when passed to command line tools.
			RemoveTrailingSlashes(RulesObject.PublicIncludePaths);
			RemoveTrailingSlashes(RulesObject.PublicSystemIncludePaths);
			RemoveTrailingSlashes(RulesObject.PrivateIncludePaths);
			RemoveTrailingSlashes(RulesObject.PublicLibraryPaths);

			// Validate rules object
			if (RulesObject.Type == ModuleRules.ModuleType.CPlusPlus)
			{
				List<string> InvalidDependencies = RulesObject.DynamicallyLoadedModuleNames.Intersect(RulesObject.PublicDependencyModuleNames.Concat(RulesObject.PrivateDependencyModuleNames)).ToList();
				if (InvalidDependencies.Count != 0)
				{
					throw new BuildException("Module rules for '{0}' should not be dependent on modules which are also dynamically loaded: {1}", ModuleName, String.Join(", ", InvalidDependencies));
				}

				// Make sure that engine modules use shared PCHs or have an explicit private PCH
				if(RulesObject.PCHUsage == ModuleRules.PCHUsageMode.NoSharedPCHs && RulesObject.PrivatePCHHeaderFile == null)
				{
					if(ProjectFile == null || !RulesObject.File.IsUnderDirectory(ProjectFile.Directory))
					{
						Log.TraceWarning("{0} module has shared PCHs disabled, but does not have a private PCH set", ModuleName);
					}
				}

				// Disable shared PCHs for game modules by default (but not game plugins, since they won't depend on the game's PCH!)
				if (RulesObject.PCHUsage == ModuleRules.PCHUsageMode.Default)
				{
					if(RulesObject.bUseBackwardsCompatibleDefaults && !Rules.bIWYU)
					{
						if(RulesObject.Plugin != null)
						{
							// Game plugin.  Enable shared PCHs by default, since they aren't typically large enough to warrant their own PCH.
							RulesObject.PCHUsage = ModuleRules.PCHUsageMode.UseSharedPCHs;
						}
						else
						{
							// Game module.  Do not enable shared PCHs by default, because games usually have a large precompiled header of their own and compile times would suffer.
							RulesObject.PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
						}
					}
					else
					{
						// Engine module or plugin module -- allow shared PCHs
						RulesObject.PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
					}
				}

				// If we can't use a shared PCH, check there's a private PCH set
				if(RulesObject.PCHUsage != ModuleRules.PCHUsageMode.NoPCHs && RulesObject.PCHUsage != ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs && RulesObject.PrivatePCHHeaderFile == null)
				{
					// Try to figure out the legacy PCH file
					FileReference CppFile = DirectoryReference.EnumerateFiles(RulesObject.Directory, "*.cpp", SearchOption.AllDirectories).FirstOrDefault();
					if(CppFile != null)
					{
						string IncludeFile = MetadataCache.GetFirstInclude(FileItem.GetItemByFileReference(CppFile));
						if(IncludeFile != null)
						{
							FileReference PchIncludeFile = DirectoryReference.EnumerateFiles(RulesObject.Directory, Path.GetFileName(IncludeFile), SearchOption.AllDirectories).FirstOrDefault();
							if(PchIncludeFile != null)
							{
								RulesObject.PrivatePCHHeaderFile = PchIncludeFile.MakeRelativeTo(RulesObject.Directory).Replace(Path.DirectorySeparatorChar, '/');
							}
						}
					}

					// Print a suggestion for which file to include
					if(RulesObject.PrivatePCHHeaderFile == null)
					{
						Log.TraceWarningOnce(RulesObject.File, "Modules must specify an explicit precompiled header (eg. PrivatePCHHeaderFile = \"Private/{0}PrivatePCH.h\") from UE 4.21 onwards.", ModuleName);
					}
					else
					{
						Log.TraceWarningOnce(RulesObject.File, "Modules must specify an explicit precompiled header (eg. PrivatePCHHeaderFile = \"{0}\") from UE 4.21 onwards.", RulesObject.PrivatePCHHeaderFile);
					}
				}
			}
			return RulesObject;
		}

		/// <summary>
		/// Utility function to remove trailing slashes from a list of paths
		/// </summary>
		/// <param name="Paths">List of paths to process</param>
		private static void RemoveTrailingSlashes(List<string> Paths)
		{
			for(int Idx = 0; Idx < Paths.Count; Idx++)
			{
				Paths[Idx] = Paths[Idx].TrimEnd('\\');
			}
		}

		/// <summary>
		/// Finds a module given its name.  Throws an exception if the module couldn't be found.
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="ReferenceChain">Chain of references causing this module to be instantiated, for display in error messages</param>
		public UEBuildModule FindOrCreateModuleByName(string ModuleName, string ReferenceChain)
		{
			UEBuildModule Module;
			if (!Modules.TryGetValue(ModuleName, out Module))
			{
				// @todo projectfiles: Cross-platform modules can appear here during project generation, but they may have already
				//   been filtered out by the project generator.  This causes the projects to not be added to directories properly.
				ModuleRules RulesObject = CreateModuleRulesAndSetDefaults(ModuleName, ReferenceChain);
				DirectoryReference ModuleDirectory = RulesObject.File.Directory;

				// Clear the bUsePrecompiled flag if we're compiling a foreign plugin; since it's treated like an engine module, it will default to true in an installed build.
				if(RulesObject.Plugin != null && RulesObject.Plugin.File == ForeignPlugin)
				{
					RulesObject.bPrecompile = true;
					RulesObject.bUsePrecompiled = false;
				}

				// Get the base directory for paths referenced by the module. If the module's under the UProject source directory use that, otherwise leave it relative to the Engine source directory.
				if (ProjectFile != null)
				{
					DirectoryReference ProjectSourceDirectoryName = DirectoryReference.Combine(ProjectFile.Directory, "Source");
					if (RulesObject.File.IsUnderDirectory(ProjectSourceDirectoryName))
					{
						RulesObject.PublicIncludePaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PublicIncludePaths);
						RulesObject.PrivateIncludePaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PrivateIncludePaths);
						RulesObject.PublicLibraryPaths = CombinePathList(ProjectSourceDirectoryName, RulesObject.PublicLibraryPaths);
					}
				}

				// Get the generated code directory. Plugins always write to their own intermediate directory so they can be copied between projects, shared engine 
				// intermediates go in the engine intermediate folder, and anything else goes in the project folder.
				DirectoryReference GeneratedCodeDirectory = null;
				if (RulesObject.Type != ModuleRules.ModuleType.External)
				{
					// Get the base directory
					if (RulesObject.Plugin != null)
					{
						GeneratedCodeDirectory = RulesObject.Plugin.Directory;
					}
					else if (bUseSharedBuildEnvironment && RulesObject.File.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
					{
						GeneratedCodeDirectory = UnrealBuildTool.EngineDirectory;
					}
					else if (bUseSharedBuildEnvironment && UnrealBuildTool.IsUnderAnEngineDirectory(RulesObject.File.Directory))
					{
						GeneratedCodeDirectory = UnrealBuildTool.EnterpriseDirectory;
					}
					else
					{
						GeneratedCodeDirectory = ProjectDirectory;
					}

					// Get the subfolder containing generated code
					GeneratedCodeDirectory = DirectoryReference.Combine(GeneratedCodeDirectory, PlatformIntermediateFolder, AppName, "Inc");

					// Append the binaries subfolder, if present. We rely on this to ensure that build products can be filtered correctly.
					if(RulesObject.BinariesSubFolder != null)
					{
						GeneratedCodeDirectory = DirectoryReference.Combine(GeneratedCodeDirectory, RulesObject.BinariesSubFolder);
					}

					// Finally, append the module name.
					GeneratedCodeDirectory = DirectoryReference.Combine(GeneratedCodeDirectory, ModuleName);
				}

				// For legacy modules, add a bunch of default include paths.
				if (RulesObject.Type == ModuleRules.ModuleType.CPlusPlus && RulesObject.bAddDefaultIncludePaths && (RulesObject.Plugin != null || (ProjectFile != null && RulesObject.File.IsUnderDirectory(ProjectFile.Directory))))
				{
					// Add the module source directory 
					DirectoryReference BaseSourceDirectory;
					if (RulesObject.Plugin != null)
					{
						BaseSourceDirectory = DirectoryReference.Combine(RulesObject.Plugin.Directory, "Source");
					}
					else
					{
						BaseSourceDirectory = DirectoryReference.Combine(ProjectFile.Directory, "Source");
					}

					// If it's a game module (plugin or otherwise), add the root source directory to the include paths.
					if (RulesObject.File.IsUnderDirectory(TargetRulesFile.Directory) || (RulesObject.Plugin != null && RulesObject.Plugin.LoadedFrom == PluginLoadedFrom.Project))
					{
						if(DirectoryReference.Exists(BaseSourceDirectory))
						{
							RulesObject.PublicIncludePaths.Add(NormalizeIncludePath(BaseSourceDirectory));
						}
					}

					// Resolve private include paths against the project source root
					for (int Idx = 0; Idx < RulesObject.PrivateIncludePaths.Count; Idx++)
					{
						string PrivateIncludePath = RulesObject.PrivateIncludePaths[Idx];
						if (!Path.IsPathRooted(PrivateIncludePath))
						{
							PrivateIncludePath = DirectoryReference.Combine(BaseSourceDirectory, PrivateIncludePath).FullName;
						}
						RulesObject.PrivateIncludePaths[Idx] = PrivateIncludePath;
					}
				}

				// Override the default for whether the module requires nested include paths
				if(RulesObject.bLegacyPublicIncludePaths == null)
				{
					if(RulesObject.bUseBackwardsCompatibleDefaults)
					{
						RulesObject.bLegacyPublicIncludePaths = Rules.bLegacyPublicIncludePaths;
					}
					else
					{
						RulesObject.bLegacyPublicIncludePaths = false;
					}
				}

				// Allow the current platform to modify the module rules
				UEBuildPlatform.GetBuildPlatform(Platform).ModifyModuleRulesForActivePlatform(ModuleName, RulesObject, Rules);

				// Allow all build platforms to 'adjust' the module setting. 
				// This will allow undisclosed platforms to make changes without 
				// exposing information about the platform in publicly accessible 
				// locations.
				UEBuildPlatform.PlatformModifyHostModuleRules(ModuleName, RulesObject, Rules);

				// Now, go ahead and create the module builder instance
				Module = InstantiateModule(RulesObject, GeneratedCodeDirectory);
				Modules.Add(Module.Name, Module);
			}
			return Module;
		}

		/// <summary>
		/// Constructs a new C++ module
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="ReferenceChain">Chain of references causing this module to be instantiated, for display in error messages</param>
		/// <returns>New C++ module</returns>
		public UEBuildModuleCPP FindOrCreateCppModuleByName(string ModuleName, string ReferenceChain)
		{
			UEBuildModuleCPP CppModule = FindOrCreateModuleByName(ModuleName, ReferenceChain) as UEBuildModuleCPP;
			if(CppModule == null)
			{
				throw new BuildException("'{0}' is not a C++ module (referenced via {1})", ModuleName, ReferenceChain);
			}
			return CppModule;
		}

		protected UEBuildModule InstantiateModule(
			ModuleRules RulesObject,
			DirectoryReference GeneratedCodeDirectory)
		{
			switch (RulesObject.Type)
			{
				case ModuleRules.ModuleType.CPlusPlus:
					return new UEBuildModuleCPP(
							Rules: RulesObject,
							IntermediateDirectory: GetModuleIntermediateDirectory(RulesObject),
							GeneratedCodeDirectory: GeneratedCodeDirectory
						);

				case ModuleRules.ModuleType.External:
					return new UEBuildModuleExternal(RulesObject);

				default:
					throw new BuildException("Unrecognized module type specified by 'Rules' object {0}", RulesObject.ToString());
			}
		}

		/// <summary>
		/// Normalize an include path to be relative to the engine source directory
		/// </summary>
		public static string NormalizeIncludePath(DirectoryReference Directory)
		{
			return Utils.CleanDirectorySeparators(Directory.MakeRelativeTo(UnrealBuildTool.EngineSourceDirectory), '/');
		}

		/// <summary>
		/// Finds a module given its name.  Throws an exception if the module couldn't be found.
		/// </summary>
		public UEBuildModule GetModuleByName(string Name)
		{
			UEBuildModule Result;
			if (Modules.TryGetValue(Name, out Result))
			{
				return Result;
			}
			else
			{
				throw new BuildException("Couldn't find referenced module '{0}'.", Name);
			}
		}


		/// <summary>
		/// Combines a list of paths with a base path.
		/// </summary>
		/// <param name="BasePath">Base path to combine with. May be null or empty.</param>
		/// <param name="PathList">List of input paths to combine with. May be null.</param>
		/// <returns>List of paths relative The build module object for the specified build rules source file</returns>
		private static List<string> CombinePathList(DirectoryReference BasePath, List<string> PathList)
		{
			List<string> NewPathList = new List<string>();
			foreach (string Path in PathList)
			{
				NewPathList.Add(System.IO.Path.Combine(BasePath.FullName, Path));
			}
			return NewPathList;
		}
	}
}
