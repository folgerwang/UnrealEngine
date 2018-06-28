// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// A module that is compiled from C++ code.
	/// </summary>
	class UEBuildModuleCPP : UEBuildModule
	{
		public class AutoGenerateCppInfoClass
		{
			public class BuildInfoClass
			{
				/// <summary>
				/// The wildcard of the *.gen.cpp file which was generated for the module
				/// </summary>
				public readonly string FileWildcard;

				public BuildInfoClass(string InWildcard)
				{
					Debug.Assert(InWildcard != null);

					FileWildcard = InWildcard;
				}
			}

			/// <summary>
			/// Information about how to build the .gen.cpp files. If this is null, then we're not building .gen.cpp files for this module.
			/// </summary>
			public BuildInfoClass BuildInfo;

			public AutoGenerateCppInfoClass(BuildInfoClass InBuildInfo)
			{
				BuildInfo = InBuildInfo;
			}
		}

		/// <summary>
		/// Information about the .gen.cpp file.  If this is null then this module doesn't have any UHT-produced code.
		/// </summary>
		public AutoGenerateCppInfoClass AutoGenerateCppInfo = null;

		public class SourceFilesClass
		{
			public readonly List<FileItem> MissingFiles = new List<FileItem>();
			public readonly List<FileItem> CPPFiles = new List<FileItem>();
			public readonly List<FileItem> CFiles = new List<FileItem>();
			public readonly List<FileItem> CCFiles = new List<FileItem>();
			public readonly List<FileItem> MMFiles = new List<FileItem>();
			public readonly List<FileItem> RCFiles = new List<FileItem>();
			public readonly List<FileItem> OtherFiles = new List<FileItem>();

			public int Count
			{
				get
				{
					return MissingFiles.Count +
						   CPPFiles.Count +
						   CFiles.Count +
						   CCFiles.Count +
						   MMFiles.Count +
						   RCFiles.Count +
						   OtherFiles.Count;
				}
			}

			/// <summary>
			/// Copy from list to list helper.
			/// </summary>
			/// <param name="From">Source list.</param>
			/// <param name="To">Destination list.</param>
			private static void CopyFromListToList(List<FileItem> From, List<FileItem> To)
			{
				To.Clear();
				To.AddRange(From);
			}

			/// <summary>
			/// Copies file lists from other SourceFilesClass to this.
			/// </summary>
			/// <param name="Other">Source object.</param>
			public void CopyFrom(SourceFilesClass Other)
			{
				CopyFromListToList(Other.MissingFiles, MissingFiles);
				CopyFromListToList(Other.CPPFiles, CPPFiles);
				CopyFromListToList(Other.CFiles, CFiles);
				CopyFromListToList(Other.CCFiles, CCFiles);
				CopyFromListToList(Other.MMFiles, MMFiles);
				CopyFromListToList(Other.RCFiles, RCFiles);
				CopyFromListToList(Other.OtherFiles, OtherFiles);
			}
		}

		/// <summary>
		/// Adds additional source cpp files for this module.
		/// </summary>
		/// <param name="Files">Files to add.</param>
		public void AddAdditionalCPPFiles(IEnumerable<FileItem> Files)
		{
			SourceFiles.AddRange(Files);
			SourceFilesToBuild.CPPFiles.AddRange(Files);
		}

		/// <summary>
		/// All the source files for this module
		/// </summary>
		public readonly List<FileItem> SourceFiles = new List<FileItem>();

		/// <summary>
		/// A list of the absolute paths of source files to be built in this module.
		/// </summary>
		public readonly SourceFilesClass SourceFilesToBuild = new SourceFilesClass();

		/// <summary>
		/// A list of the source files that were found for the module.
		/// </summary>
		public readonly SourceFilesClass SourceFilesFound = new SourceFilesClass();

		/// <summary>
		/// The directory for this module's object files
		/// </summary>
		public readonly DirectoryReference IntermediateDirectory;

		/// <summary>
		/// The directory for this module's generated code
		/// </summary>
		public readonly DirectoryReference GeneratedCodeDirectory;

		public List<DirectoryReference> IncludeSearchPaths = new List<DirectoryReference>();

		public class ProcessedDependenciesClass
		{
			/// <summary>
			/// The file, if any, which is used as the unique PCH for this module
			/// </summary>
			public FileItem UniquePCHHeaderFile = null;
		}

		/// <summary>
		/// The processed dependencies for the class
		/// </summary>
		public ProcessedDependenciesClass ProcessedDependencies = null;

		/// <summary>
		/// List of invalid include directives. These are buffered up and output before we start compiling.
		/// </summary>
		public List<string> InvalidIncludeDirectiveMessages;

		public IEnumerable<string> FindGeneratedCppFiles()
		{
			return ((null == GeneratedCodeDirectory) || !DirectoryReference.Exists(GeneratedCodeDirectory))
				? Enumerable.Empty<string>()
				: DirectoryReference.EnumerateFiles(GeneratedCodeDirectory, "*.gen.cpp", SearchOption.TopDirectoryOnly).Select((Dir) => Dir.FullName);
		}

		protected override void GetReferencedDirectories(HashSet<DirectoryReference> Directories)
		{
			base.GetReferencedDirectories(Directories);

			foreach(FileItem SourceFile in SourceFiles)
			{
				Directories.Add(SourceFile.Location.Directory);
			}
		}

		/// <summary>
		/// Categorizes source files into per-extension buckets
		/// </summary>
		private static void CategorizeSourceFiles(IEnumerable<FileItem> InSourceFiles, SourceFilesClass OutSourceFiles)
		{
			foreach (FileItem SourceFile in InSourceFiles)
			{
				string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();
				if (!SourceFile.bExists)
				{
					OutSourceFiles.MissingFiles.Add(SourceFile);
				}
				else if (Extension == ".CPP")
				{
					OutSourceFiles.CPPFiles.Add(SourceFile);
				}
				else if (Extension == ".C")
				{
					OutSourceFiles.CFiles.Add(SourceFile);
				}
				else if (Extension == ".CC")
				{
					OutSourceFiles.CCFiles.Add(SourceFile);
				}
				else if (Extension == ".MM" || Extension == ".M")
				{
					OutSourceFiles.MMFiles.Add(SourceFile);
				}
				else if (Extension == ".RC")
				{
					OutSourceFiles.RCFiles.Add(SourceFile);
				}
				else
				{
					OutSourceFiles.OtherFiles.Add(SourceFile);
				}
			}
		}

		/// <summary>
		/// List of whitelisted circular dependencies. Please do NOT add new modules here; refactor to allow the modules to be decoupled instead.
		/// </summary>
		static readonly KeyValuePair<string, string>[] WhitelistedCircularDependencies =
		{
			new KeyValuePair<string, string>("Engine", "Landscape"),
			new KeyValuePair<string, string>("Engine", "UMG"),
			new KeyValuePair<string, string>("Engine", "GameplayTags"),
			new KeyValuePair<string, string>("Engine", "MaterialShaderQualitySettings"),
			new KeyValuePair<string, string>("Engine", "UnrealEd"),
			new KeyValuePair<string, string>("PacketHandler", "ReliabilityHandlerComponent"),
			new KeyValuePair<string, string>("GameplayDebugger", "AIModule"),
			new KeyValuePair<string, string>("GameplayDebugger", "GameplayTasks"),
			new KeyValuePair<string, string>("Engine", "CinematicCamera"),
			new KeyValuePair<string, string>("Engine", "CollisionAnalyzer"),
			new KeyValuePair<string, string>("Engine", "LogVisualizer"),
			new KeyValuePair<string, string>("Engine", "Kismet"),
			new KeyValuePair<string, string>("Landscape", "UnrealEd"),
			new KeyValuePair<string, string>("Landscape", "MaterialUtilities"),
			new KeyValuePair<string, string>("LocalizationDashboard", "LocalizationService"),
			new KeyValuePair<string, string>("LocalizationDashboard", "MainFrame"),
			new KeyValuePair<string, string>("LocalizationDashboard", "TranslationEditor"),
			new KeyValuePair<string, string>("Documentation", "SourceControl"),
			new KeyValuePair<string, string>("UnrealEd", "GraphEditor"),
			new KeyValuePair<string, string>("UnrealEd", "Kismet"),
			new KeyValuePair<string, string>("UnrealEd", "AudioEditor"),
			new KeyValuePair<string, string>("BlueprintGraph", "KismetCompiler"),
			new KeyValuePair<string, string>("BlueprintGraph", "UnrealEd"),
			new KeyValuePair<string, string>("BlueprintGraph", "GraphEditor"),
			new KeyValuePair<string, string>("BlueprintGraph", "Kismet"),
			new KeyValuePair<string, string>("BlueprintGraph", "CinematicCamera"),
			new KeyValuePair<string, string>("ConfigEditor", "PropertyEditor"),
			new KeyValuePair<string, string>("SourceControl", "UnrealEd"),
			new KeyValuePair<string, string>("Kismet", "BlueprintGraph"),
			new KeyValuePair<string, string>("Kismet", "UMGEditor"),
			new KeyValuePair<string, string>("MovieSceneTools", "Sequencer"),
			new KeyValuePair<string, string>("Sequencer", "MovieSceneTools"),
			new KeyValuePair<string, string>("AIModule", "AITestSuite"),
			new KeyValuePair<string, string>("GameplayTasks", "UnrealEd"),
			new KeyValuePair<string, string>("AnimGraph", "UnrealEd"),
			new KeyValuePair<string, string>("AnimGraph", "GraphEditor"),
			new KeyValuePair<string, string>("MaterialUtilities", "Landscape"),
			new KeyValuePair<string, string>("HierarchicalLODOutliner", "UnrealEd"),
			new KeyValuePair<string, string>("PixelInspectorModule", "UnrealEd"),
			new KeyValuePair<string, string>("GameplayAbilitiesEditor", "BlueprintGraph"),
            new KeyValuePair<string, string>("UnrealEd", "ViewportInteraction"),
            new KeyValuePair<string, string>("UnrealEd", "VREditor"),
            new KeyValuePair<string, string>("LandscapeEditor", "ViewportInteraction"),
            new KeyValuePair<string, string>("LandscapeEditor", "VREditor"),
            new KeyValuePair<string, string>("FoliageEdit", "ViewportInteraction"),
            new KeyValuePair<string, string>("FoliageEdit", "VREditor"),
            new KeyValuePair<string, string>("MeshPaint", "ViewportInteraction"),
            new KeyValuePair<string, string>("MeshPaint", "VREditor"),
            new KeyValuePair<string, string>("MeshPaintMode", "ViewportInteraction"),
            new KeyValuePair<string, string>("MeshPaintMode", "VREditor"),
            new KeyValuePair<string, string>("Sequencer", "ViewportInteraction"),
            new KeyValuePair<string, string>("NavigationSystem", "UnrealEd"),
        };


		public UEBuildModuleCPP(
			string InName,
			UHTModuleType InType,
			DirectoryReference InModuleDirectory,
			DirectoryReference InIntermediateDirectory,
			DirectoryReference InGeneratedCodeDirectory,
			IEnumerable<FileItem> InSourceFiles,
			ModuleRules InRules,
			bool bInBuildSourceFiles,
			FileReference InRulesFile,
			List<RuntimeDependency> InRuntimeDependencies
			)
			: base(
					InName,
					InType,
					InModuleDirectory,
					InRules,
					InRulesFile,
					InRuntimeDependencies
				)
		{
			IntermediateDirectory = InIntermediateDirectory;
			GeneratedCodeDirectory = InGeneratedCodeDirectory;

			SourceFiles = InSourceFiles.ToList();

			CategorizeSourceFiles(InSourceFiles, SourceFilesFound);
			if (bInBuildSourceFiles)
			{
				SourceFilesToBuild.CopyFrom(SourceFilesFound);
			}

			foreach (string Def in PublicDefinitions)
			{
				Log.TraceVerbose("Compile Env {0}: {1}", Name, Def);
			}

			foreach (string Def in Rules.PrivateDefinitions)
			{
				Log.TraceVerbose("Compile Env {0}: {1}", Name, Def);
			}

			foreach(string CircularlyReferencedModuleName in Rules.CircularlyReferencedDependentModules)
			{
				if(CircularlyReferencedModuleName != "BlueprintContext" && !WhitelistedCircularDependencies.Any(x => x.Key == Name && x.Value == CircularlyReferencedModuleName))
				{
					Log.TraceWarning("Found reference between '{0}' and '{1}'. Support for circular references is being phased out; please do not introduce new ones.", Name, CircularlyReferencedModuleName);
				}
			}

			AddDefaultIncludePaths();
		}

		/// <summary>
		/// Add the default include paths for this module to its settings
		/// </summary>
		private void AddDefaultIncludePaths()
		{
			// Add the module's parent directory to the public include paths, so other modules may include headers from it explicitly.
			PublicIncludePaths.Add(ModuleDirectory.ParentDirectory);

			// Add the base directory to the legacy include paths.
			LegacyPublicIncludePaths.Add(ModuleDirectory);

			// Add the 'classes' directory, if it exists
			DirectoryReference ClassesDirectory = DirectoryReference.Combine(ModuleDirectory, "Classes");
			if (DirectoryLookupCache.DirectoryExists(ClassesDirectory))
			{
				PublicIncludePaths.Add(ClassesDirectory);
			}

			// Add all the public directories
			DirectoryReference PublicDirectory = DirectoryReference.Combine(ModuleDirectory, "Public");
			if (DirectoryLookupCache.DirectoryExists(PublicDirectory))
			{
				PublicIncludePaths.Add(PublicDirectory);

				FileSystemName[] ExcludedFolderNames = UEBuildPlatform.GetBuildPlatform(Rules.Target.Platform).GetExcludedFolderNames();
				foreach (DirectoryReference PublicSubDirectory in DirectoryLookupCache.EnumerateDirectoriesRecursively(PublicDirectory))
				{
					if(!PublicSubDirectory.ContainsAnyNames(ExcludedFolderNames, PublicDirectory))
					{
						LegacyPublicIncludePaths.Add(PublicSubDirectory);
					}
				}
			}

			// Add the base private directory for this module
			DirectoryReference PrivateDirectory = DirectoryReference.Combine(ModuleDirectory, "Private");
			if(DirectoryLookupCache.DirectoryExists(PrivateDirectory))
			{
				PrivateIncludePaths.Add(PrivateDirectory);
			}
		}

		/// <summary>
		/// Path to the precompiled manifest location
		/// </summary>
		public virtual FileReference PrecompiledManifestLocation
		{
			get { return FileReference.Combine(IntermediateDirectory, String.Format("{0}.precompiled", Name)); }
		}

		/// <summary>
		/// Gathers intellisense data for the project file containing this module
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="BinaryCompileEnvironment">The inherited compile environment for this module</param>
		/// <param name="ProjectFile">The project file containing this module</param>
		public void GatherDataForProjectFile(ReadOnlyTargetRules Target, CppCompileEnvironment BinaryCompileEnvironment, ProjectFile ProjectFile)
		{
			CppCompileEnvironment ModuleCompileEnvironment = CreateModuleCompileEnvironment(Target, BinaryCompileEnvironment);
			ProjectFile.AddIntelliSensePreprocessorDefinitions(ModuleCompileEnvironment.Definitions);
			ProjectFile.AddInteliiSenseIncludePaths(ModuleCompileEnvironment.IncludePaths.SystemIncludePaths, true);
			ProjectFile.AddInteliiSenseIncludePaths(ModuleCompileEnvironment.IncludePaths.UserIncludePaths, false);
		}

		/// <summary>
		/// Sets up the environment for compiling any module that includes the public interface of this module.
		/// </summary>
		public override void AddModuleToCompileEnvironment(
			UEBuildBinary SourceBinary,
			HashSet<DirectoryReference> IncludePaths,
			HashSet<DirectoryReference> SystemIncludePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks,
			bool bLegacyPublicIncludePaths
			)
		{
			if(AutoGenerateCppInfo != null)
			{
				IncludePaths.Add(GeneratedCodeDirectory);
			}

			base.AddModuleToCompileEnvironment(SourceBinary, IncludePaths, SystemIncludePaths, Definitions, AdditionalFrameworks, bLegacyPublicIncludePaths);
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(ReadOnlyTargetRules Target, UEToolChain ToolChain, CppCompileEnvironment BinaryCompileEnvironment, List<PrecompiledHeaderTemplate> SharedPCHs, ISourceFileWorkingSet WorkingSet, ActionGraph ActionGraph)
		{
			UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatformForCPPTargetPlatform(BinaryCompileEnvironment.Platform);

			List<FileItem> LinkInputFiles = new List<FileItem>();

			CppCompileEnvironment ModuleCompileEnvironment = CreateModuleCompileEnvironment(Target, BinaryCompileEnvironment);
			IncludeSearchPaths = ModuleCompileEnvironment.IncludePaths.UserIncludePaths.ToList();
			IncludeSearchPaths.AddRange(ModuleCompileEnvironment.IncludePaths.SystemIncludePaths.ToList());

			// If the module is precompiled, read the object files from the manifest
			if(Rules.bUsePrecompiled)
			{
				PrecompiledManifest Manifest = PrecompiledManifest.Read(PrecompiledManifestLocation);
				foreach(FileReference OutputFile in Manifest.OutputFiles)
				{
					FileItem ObjectFile = FileItem.GetExistingItemByFileReference(OutputFile);
					ToolChain.DoLocalToRemoteFileItem(ObjectFile);
					LinkInputFiles.Add(ObjectFile);
				}
				return LinkInputFiles;
			}

			// Throw an error if the module's source file list referenced any non-existent files.
			if (SourceFilesToBuild.MissingFiles.Count > 0)
			{
				throw new BuildException(
					"UBT ERROR: Module \"{0}\" references non-existent files:\n{1} (perhaps a file was added to the project but not checked in)",
					Name,
					string.Join("\n", SourceFilesToBuild.MissingFiles.Select(M => M.AbsolutePath))
				);
			}

			{
				// Process all of the header file dependencies for this module
				this.CachePCHUsageForModuleSourceFiles(Target, ModuleCompileEnvironment);

				// Make sure our RC files have cached includes.  
				foreach (FileItem RCFile in SourceFilesToBuild.RCFiles)
				{
					// The default resource file (PCLaunch.rc) is created in a module-agnostic way, so we want to avoid overriding the include paths for it
					if(RCFile.CachedIncludePaths == null)
					{
						RCFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;
					}
				}
			}

			// Should we force a precompiled header to be generated for this module?  Usually, we only bother with a
			// precompiled header if there are at least several source files in the module (after combining them for unity
			// builds.)  But for game modules, it can be convenient to always have a precompiled header to single-file
			// changes to code is really quick to compile.
			int MinFilesUsingPrecompiledHeader = Target.MinFilesUsingPrecompiledHeader;
			if (Rules.MinFilesUsingPrecompiledHeaderOverride != 0)
			{
				MinFilesUsingPrecompiledHeader = Rules.MinFilesUsingPrecompiledHeaderOverride;
			}
			else if (!Rules.bTreatAsEngineModule && Target.bForcePrecompiledHeaderForGameModules)
			{
				// This is a game module with only a small number of source files, so go ahead and force a precompiled header
				// to be generated to make incremental changes to source files as fast as possible for small projects.
				MinFilesUsingPrecompiledHeader = 1;
			}

			// Engine modules will always use unity build mode unless MinSourceFilesForUnityBuildOverride is specified in
			// the module rules file.  By default, game modules only use unity of they have enough source files for that
			// to be worthwhile.  If you have a lot of small game modules, consider specifying MinSourceFilesForUnityBuildOverride=0
			// in the modules that you don't typically iterate on source files in very frequently.
			int MinSourceFilesForUnityBuild = 0;
			if (Rules.MinSourceFilesForUnityBuildOverride != 0)
			{
				MinSourceFilesForUnityBuild = Rules.MinSourceFilesForUnityBuildOverride;
			}
			else if (Target.ProjectFile != null && RulesFile.IsUnderDirectory(DirectoryReference.Combine(Target.ProjectFile.Directory, "Source")))
			{
				// Game modules with only a small number of source files are usually better off having faster iteration times
				// on single source file changes, so we forcibly disable unity build for those modules
				MinSourceFilesForUnityBuild = Target.MinGameModuleSourceFilesForUnityBuild;
			}

			// Should we use unity build mode for this module?
			bool bModuleUsesUnityBuild = false;
			if (Target.bUseUnityBuild || Target.bForceUnityBuild)
			{
				if (Target.bForceUnityBuild)
				{
					Log.TraceVerbose("Module '{0}' using unity build mode (bForceUnityBuild enabled for this module)", this.Name);
					bModuleUsesUnityBuild = true;
				}
				else if (Rules.bFasterWithoutUnity)
				{
					Log.TraceVerbose("Module '{0}' not using unity build mode (bFasterWithoutUnity enabled for this module)", this.Name);
					bModuleUsesUnityBuild = false;
				}
				else if (SourceFilesToBuild.CPPFiles.Count < MinSourceFilesForUnityBuild)
				{
					Log.TraceVerbose("Module '{0}' not using unity build mode (module with fewer than {1} source files)", this.Name, MinSourceFilesForUnityBuild);
					bModuleUsesUnityBuild = false;
				}
				else
				{
					Log.TraceVerbose("Module '{0}' using unity build mode", this.Name);
					bModuleUsesUnityBuild = true;
				}
			}
			else
			{
				Log.TraceVerbose("Module '{0}' not using unity build mode", this.Name);
			}

			// Set up the environment with which to compile the CPP files
			CppCompileEnvironment CompileEnvironment = ModuleCompileEnvironment;
			if (Target.bUsePCHFiles)
			{
				// If this module has an explicit PCH, use that
				if(Rules.PrivatePCHHeaderFile != null)
				{
					PrecompiledHeaderInstance Instance = CreatePrivatePCH(ToolChain, FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile)), CompileEnvironment, ActionGraph);

					CompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
					CompileEnvironment.Definitions.Clear();
					CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
					CompileEnvironment.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Location;
					CompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

					LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
				}

				// Try to find a suitable shared PCH for this module
				if (CompileEnvironment.PrecompiledHeaderFile == null && SharedPCHs.Count > 0 && !CompileEnvironment.bIsBuildingLibrary && Rules.PCHUsage != ModuleRules.PCHUsageMode.NoSharedPCHs)
				{
					// Find all the dependencies of this module
					HashSet<UEBuildModule> ReferencedModules = new HashSet<UEBuildModule>();
					GetAllDependencyModules(new List<UEBuildModule>(), ReferencedModules, bIncludeDynamicallyLoaded: false, bForceCircular: false, bOnlyDirectDependencies: true);

					// Find the first shared PCH module we can use
					PrecompiledHeaderTemplate Template = SharedPCHs.FirstOrDefault(x => ReferencedModules.Contains(x.Module));
					if(Template != null && Template.IsValidFor(CompileEnvironment))
					{
						PrecompiledHeaderInstance Instance = FindOrCreateSharedPCH(ToolChain, Template, ModuleCompileEnvironment.bOptimizeCode, ModuleCompileEnvironment.bUseRTTI, ActionGraph);

						FileReference PrivateDefinitionsFile = FileReference.Combine(IntermediateDirectory, String.Format("Definitions.{0}.h", Name));

						FileItem PrivateDefinitionsFileItem;
						using (StringWriter Writer = new StringWriter())
						{
							// Remove the module _API definition for cases where there are circular dependencies between the shared PCH module and modules using it
							Writer.WriteLine("#undef {0}", ModuleApiDefine);

							// Games may choose to use shared PCHs from the engine, so allow them to change the value of these macros
							if(!Rules.bTreatAsEngineModule)
							{
								Writer.WriteLine("#undef UE_IS_ENGINE_MODULE");
								Writer.WriteLine("#undef DEPRECATED_FORGAME");
								Writer.WriteLine("#define DEPRECATED_FORGAME DEPRECATED");
							}

							WriteDefinitions(CompileEnvironment.Definitions, Writer);
							PrivateDefinitionsFileItem = FileItem.CreateIntermediateTextFile(PrivateDefinitionsFile, Writer.ToString());
						}

						CompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
						CompileEnvironment.Definitions.Clear();
						CompileEnvironment.ForceIncludeFiles.Add(PrivateDefinitionsFileItem);
						CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
						CompileEnvironment.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Location;
						CompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

						LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
					}
				}

				// If there was one header that was included first by enough C++ files, use it as the precompiled header. Only use precompiled headers for projects with enough files to make the PCH creation worthwhile.
				if (CompileEnvironment.PrecompiledHeaderFile == null && SourceFilesToBuild.CPPFiles.Count >= MinFilesUsingPrecompiledHeader && ProcessedDependencies != null)
				{
					PrecompiledHeaderInstance Instance = CreatePrivatePCH(ToolChain, ProcessedDependencies.UniquePCHHeaderFile, CompileEnvironment, ActionGraph);

					CompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
					CompileEnvironment.Definitions.Clear();
					CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
					CompileEnvironment.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Location;
					CompileEnvironment.PrecompiledHeaderFile = Instance.Output.PrecompiledHeaderFile;

					LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
				}
			}

			// Write all the definitions to a separate file
			CreateHeaderForDefinitions(CompileEnvironment, IntermediateDirectory, null);

			// Compile CPP files
			List<FileItem> CPPFilesToCompile = SourceFilesToBuild.CPPFiles;
			if (bModuleUsesUnityBuild)
			{
				CPPFilesToCompile = Unity.GenerateUnityCPPs(Target, CPPFilesToCompile, CompileEnvironment, WorkingSet, Rules.ShortName ?? Name, IntermediateDirectory);
				LinkInputFiles.AddRange(CompileUnityFilesWithToolChain(Target, ToolChain, CompileEnvironment, ModuleCompileEnvironment, CPPFilesToCompile, ActionGraph).ObjectFiles);
			}
			else
			{
				LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CompileEnvironment, CPPFilesToCompile, IntermediateDirectory, Name, ActionGraph).ObjectFiles);
			}

			// Compile all the generated CPP files
			if (AutoGenerateCppInfo != null && AutoGenerateCppInfo.BuildInfo != null && !CompileEnvironment.bHackHeaderGenerator)
			{
				string[] GeneratedFiles = Directory.GetFiles(Path.GetDirectoryName(AutoGenerateCppInfo.BuildInfo.FileWildcard), Path.GetFileName(AutoGenerateCppInfo.BuildInfo.FileWildcard));
				if(GeneratedFiles.Length > 0)
				{
					// Create a compile environment for the generated files. We can disable creating debug info here to improve link times.
					CppCompileEnvironment GeneratedCPPCompileEnvironment = CompileEnvironment;
					if(GeneratedCPPCompileEnvironment.bCreateDebugInfo && Target.bDisableDebugInfoForGeneratedCode)
					{
						GeneratedCPPCompileEnvironment = new CppCompileEnvironment(GeneratedCPPCompileEnvironment);
						GeneratedCPPCompileEnvironment.bCreateDebugInfo = false;
					}

					// Compile all the generated files
					List<FileItem> GeneratedFileItems = new List<FileItem>();
					foreach (string GeneratedFilename in GeneratedFiles)
					{
						FileItem GeneratedCppFileItem = FileItem.GetItemByPath(GeneratedFilename);

						CachePCHUsageForModuleSourceFile(CompileEnvironment, GeneratedCppFileItem);

						// @todo ubtmake: Check for ALL other places where we might be injecting .cpp or .rc files for compiling without caching CachedCPPIncludeInfo first (anything platform specific?)
						GeneratedFileItems.Add(GeneratedCppFileItem);
					}

					if (bModuleUsesUnityBuild)
					{
						GeneratedFileItems = Unity.GenerateUnityCPPs(Target, GeneratedFileItems, GeneratedCPPCompileEnvironment, WorkingSet, (Rules.ShortName ?? Name) + ".gen", IntermediateDirectory);
						LinkInputFiles.AddRange(CompileUnityFilesWithToolChain(Target, ToolChain, GeneratedCPPCompileEnvironment, ModuleCompileEnvironment, GeneratedFileItems, ActionGraph).ObjectFiles);
					}
					else
					{
						LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(GeneratedCPPCompileEnvironment, GeneratedFileItems, IntermediateDirectory, Name, ActionGraph).ObjectFiles);
					}
				}
			}

			// Compile C files directly. Do not use a PCH here, because a C++ PCH is not compatible with C source files.
			if(SourceFilesToBuild.CFiles.Count > 0)
			{
				LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(ModuleCompileEnvironment, SourceFilesToBuild.CFiles, IntermediateDirectory, Name, ActionGraph).ObjectFiles);
			}

			// Compile CC files directly.
			if(SourceFilesToBuild.CCFiles.Count > 0)
			{
				LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CompileEnvironment, SourceFilesToBuild.CCFiles, IntermediateDirectory, Name, ActionGraph).ObjectFiles);
			}

			// Compile MM files directly.
			if(SourceFilesToBuild.MMFiles.Count > 0)
			{
				LinkInputFiles.AddRange(ToolChain.CompileCPPFiles(CompileEnvironment, SourceFilesToBuild.MMFiles, IntermediateDirectory, Name, ActionGraph).ObjectFiles);
			}

			// Compile RC files. The resource compiler does not work with response files, and using the regular compile environment can easily result in the 
			// command line length exceeding the OS limit. Use the binary compile environment to keep the size down, and require that all include paths
			// must be specified relative to the resource file itself or Engine/Source.
			if(SourceFilesToBuild.RCFiles.Count > 0)
			{
				CppCompileEnvironment ResourceCompileEnvironment = new CppCompileEnvironment(BinaryCompileEnvironment);
				LinkInputFiles.AddRange(ToolChain.CompileRCFiles(ResourceCompileEnvironment, SourceFilesToBuild.RCFiles, IntermediateDirectory, ActionGraph).ObjectFiles);
			}

			// Write the compiled manifest
			if(Rules.bPrecompile)
			{
				DirectoryReference.CreateDirectory(PrecompiledManifestLocation.Directory);

				PrecompiledManifest Manifest = new PrecompiledManifest();
				Manifest.OutputFiles.AddRange(LinkInputFiles.Select(x => x.Location));
				Manifest.Write(PrecompiledManifestLocation);
			}

			return LinkInputFiles;
		}

		/// <summary>
		/// Create a shared PCH template for this module, which allows constructing shared PCH instances in the future
		/// </summary>
		/// <param name="Target">The target which owns this module</param>
		/// <param name="BaseCompileEnvironment">Base compile environment for this target</param>
		/// <returns>Template for shared PCHs</returns>
		public PrecompiledHeaderTemplate CreateSharedPCHTemplate(UEBuildTarget Target, CppCompileEnvironment BaseCompileEnvironment)
		{
			CppCompileEnvironment CompileEnvironment = CreateSharedPCHCompileEnvironment(Target, BaseCompileEnvironment);
			FileItem HeaderFile = FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.SharedPCHHeaderFile));
			HeaderFile.CachedIncludePaths = CompileEnvironment.IncludePaths;

			DirectoryReference PrecompiledHeaderDir;
			if(Target.Rules.bUsePrecompiled)
			{
				PrecompiledHeaderDir = DirectoryReference.Combine(Target.ProjectIntermediateDirectory, Name);
			}
			else
			{
				PrecompiledHeaderDir = IntermediateDirectory;
			}

			return new PrecompiledHeaderTemplate(this, CompileEnvironment, HeaderFile, PrecompiledHeaderDir);
		}

		/// <summary>
		/// Creates a precompiled header action to generate a new pch file 
		/// </summary>
		/// <param name="ToolChain">The toolchain to generate the PCH</param>
		/// <param name="HeaderFile"></param>
		/// <param name="ModuleCompileEnvironment"></param>
		/// <param name="ActionGraph">Graph containing build actions</param>
		/// <returns>The created PCH instance.</returns>
		private PrecompiledHeaderInstance CreatePrivatePCH(UEToolChain ToolChain, FileItem HeaderFile, CppCompileEnvironment ModuleCompileEnvironment, ActionGraph ActionGraph)
		{
			// Cache the header file include paths. This file could have been a shared PCH too, so ignore if the include paths are already set.
			if(HeaderFile.CachedIncludePaths == null)
			{
				HeaderFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;
			}

			// Create the wrapper file, which sets all the definitions needed to compile it
			FileReference WrapperLocation = FileReference.Combine(IntermediateDirectory, String.Format("PCH.{0}.h", Name));
			FileItem WrapperFile = CreatePCHWrapperFile(WrapperLocation, ModuleCompileEnvironment.Definitions, HeaderFile);

			// Create a new C++ environment that is used to create the PCH.
			CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(ModuleCompileEnvironment);
			CompileEnvironment.Definitions.Clear();
			CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
			CompileEnvironment.PrecompiledHeaderIncludeFilename = WrapperFile.Location;
			CompileEnvironment.bOptimizeCode = ModuleCompileEnvironment.bOptimizeCode;

			// Create the action to compile the PCH file.
			CPPOutput Output = ToolChain.CompileCPPFiles(CompileEnvironment, new List<FileItem>() { WrapperFile }, IntermediateDirectory, Name, ActionGraph);
			return new PrecompiledHeaderInstance(WrapperFile, CompileEnvironment.bOptimizeCode, CompileEnvironment.bUseRTTI, Output);
		}

		/// <summary>
		/// Generates a precompiled header instance from the given template, or returns an existing one if it already exists
		/// </summary>
		/// <param name="ToolChain">The toolchain being used to build this module</param>
		/// <param name="Template">The PCH template</param>
		/// <param name="bOptimizeCode">Whether optimization should be enabled for this PCH</param>
		/// <param name="bUseRTTI">Whether to enable RTTI for this PCH</param>
		/// <param name="ActionGraph">Graph containing build actions</param>
		/// <returns>Instance of a PCH</returns>
		public PrecompiledHeaderInstance FindOrCreateSharedPCH(UEToolChain ToolChain, PrecompiledHeaderTemplate Template, bool bOptimizeCode, bool bUseRTTI, ActionGraph ActionGraph)
		{
			PrecompiledHeaderInstance Instance = Template.Instances.Find(x => x.bOptimizeCode == bOptimizeCode && x.bUseRTTI == bUseRTTI);
			if(Instance == null)
			{
				// Create a suffix to distinguish this shared PCH variant from any others. Currently only optimized and non-optimized shared PCHs are supported.
				string Variant = "";
				if(bOptimizeCode != Template.BaseCompileEnvironment.bOptimizeCode)
				{
					if(bOptimizeCode)
					{
						Variant += ".Optimized";
					}
					else
					{
						Variant += ".NonOptimized";
					}
				}
				if(bUseRTTI != Template.BaseCompileEnvironment.bUseRTTI)
				{
					if (bUseRTTI)
					{
						Variant += ".RTTI";
					}
					else
					{
						Variant += ".NonRTTI";
					}
				}

				// Create the wrapper file, which sets all the definitions needed to compile it
				FileReference WrapperLocation = FileReference.Combine(Template.OutputDir, String.Format("SharedPCH.{0}{1}.h", Template.Module.Name, Variant));
				FileItem WrapperFile = CreatePCHWrapperFile(WrapperLocation, Template.BaseCompileEnvironment.Definitions, Template.HeaderFile);

				// Create the compile environment for this PCH
				CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(Template.BaseCompileEnvironment);
				CompileEnvironment.Definitions.Clear();
				CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
				CompileEnvironment.PrecompiledHeaderIncludeFilename = WrapperFile.Location;
				CompileEnvironment.bOptimizeCode = bOptimizeCode;
				CompileEnvironment.bUseRTTI = bUseRTTI;

				// Create the PCH
				CPPOutput Output = ToolChain.CompileCPPFiles(CompileEnvironment, new List<FileItem>() { WrapperFile }, Template.OutputDir, "Shared", ActionGraph);
				Instance = new PrecompiledHeaderInstance(WrapperFile, bOptimizeCode, bUseRTTI, Output);
				Template.Instances.Add(Instance);
			}
			return Instance;
		}

		/// <summary>
		/// Compiles the provided CPP unity files. Will
		/// </summary>
		private CPPOutput CompileUnityFilesWithToolChain(ReadOnlyTargetRules Target, UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, CppCompileEnvironment ModuleCompileEnvironment, List<FileItem> SourceFiles, ActionGraph ActionGraph)
		{
			List<FileItem> NormalFiles = new List<FileItem>();
			List<FileItem> AdaptiveFiles = new List<FileItem>();

			bool bAdaptiveUnityDisablesPCH = (Target.bAdaptiveUnityDisablesPCH && Rules.PCHUsage == ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs);

			if ((Target.bAdaptiveUnityDisablesOptimizations || bAdaptiveUnityDisablesPCH) && !Target.bStressTestUnity)
			{
				foreach (FileItem File in SourceFiles)
				{
					// Basic check as to whether something in this module is/isn't a unity file...
					if (File.Location.GetFileName().StartsWith(Unity.ModulePrefix))
					{
						NormalFiles.Add(File);
					}
					else
					{
						AdaptiveFiles.Add(File);
					}
				}
			}
			else
			{
				NormalFiles.AddRange(SourceFiles);
			}

			CPPOutput OutputFiles = new CPPOutput();

			if (NormalFiles.Count > 0)
			{
				OutputFiles = ToolChain.CompileCPPFiles(CompileEnvironment, NormalFiles, IntermediateDirectory, Name, ActionGraph);
			}

			if (AdaptiveFiles.Count > 0)
			{
				// Create the new compile environment. Always turn off PCH due to different compiler settings.
				CppCompileEnvironment AdaptiveUnityEnvironment = new CppCompileEnvironment(ModuleCompileEnvironment);
				if(Target.bAdaptiveUnityDisablesOptimizations)
				{
					AdaptiveUnityEnvironment.bOptimizeCode = false;
				}
				AdaptiveUnityEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.None;

				// Write all the definitions out to a separate file
				CreateHeaderForDefinitions(AdaptiveUnityEnvironment, IntermediateDirectory, "Adaptive");

				// Compile the files
				CPPOutput AdaptiveOutput = ToolChain.CompileCPPFiles(AdaptiveUnityEnvironment, AdaptiveFiles, IntermediateDirectory, Name, ActionGraph);

				// Merge output
				OutputFiles.ObjectFiles.AddRange(AdaptiveOutput.ObjectFiles);
				OutputFiles.DebugDataFiles.AddRange(AdaptiveOutput.DebugDataFiles);
			}

			return OutputFiles;
		}

		/// <summary>
		/// Creates a header file containing all the preprocessor definitions for a compile environment, and force-include it. We allow a more flexible syntax for preprocessor definitions than
		/// is typically allowed on the command line (allowing function macros or double-quote characters, for example). Ensuring all definitions are specified in a header files ensures consistent
		/// behavior.
		/// </summary>
		/// <param name="CompileEnvironment">The compile environment</param>
		/// <param name="IntermediateDirectory">Directory to create the intermediate file</param>
		/// <param name="HeaderSuffix">Suffix for the included file</param>
		static void CreateHeaderForDefinitions(CppCompileEnvironment CompileEnvironment, DirectoryReference IntermediateDirectory, string HeaderSuffix)
		{
			if(CompileEnvironment.Definitions.Count > 0)
			{
				StringBuilder PrivateDefinitionsName = new StringBuilder("Definitions");
				if(!String.IsNullOrEmpty(HeaderSuffix))
				{
					PrivateDefinitionsName.Append('.');
					PrivateDefinitionsName.Append(HeaderSuffix);
				}
				PrivateDefinitionsName.Append(".h");

				FileReference PrivateDefinitionsFile = FileReference.Combine(IntermediateDirectory, PrivateDefinitionsName.ToString());
				using (StringWriter Writer = new StringWriter())
				{
					WriteDefinitions(CompileEnvironment.Definitions, Writer);
					CompileEnvironment.Definitions.Clear();

					FileItem PrivateDefinitionsFileItem = FileItem.CreateIntermediateTextFile(PrivateDefinitionsFile, Writer.ToString());
					CompileEnvironment.ForceIncludeFiles.Add(PrivateDefinitionsFileItem);
				}
			}
		}

		/// <summary>
		/// Create a header file containing the module definitions, which also includes the PCH itself. Including through another file is necessary on 
		/// Clang, since we get warnings about #pragma once otherwise, but it also allows us to consistently define the preprocessor state on all 
		/// platforms.
		/// </summary>
		/// <param name="OutputFile">The output file to create</param>
		/// <param name="Definitions">Definitions required by the PCH</param>
		/// <param name="IncludedFile">The PCH file to include</param>
		/// <returns>FileItem for the created file</returns>
		static FileItem CreatePCHWrapperFile(FileReference OutputFile, IEnumerable<string> Definitions, FileItem IncludedFile)
		{
			// Build the contents of the wrapper file
			StringBuilder WrapperContents = new StringBuilder();
			using (StringWriter Writer = new StringWriter(WrapperContents))
			{
				Writer.WriteLine("// PCH for {0}", IncludedFile.AbsolutePath);
				WriteDefinitions(Definitions, Writer);
				Writer.WriteLine("#include \"{0}\"", IncludedFile.AbsolutePath);
			}

			// Create the item
			FileItem WrapperFile = FileItem.CreateIntermediateTextFile(OutputFile, WrapperContents.ToString());
			WrapperFile.CachedIncludePaths = IncludedFile.CachedIncludePaths;

			// Touch it if the included file is newer, to make sure our timestamp dependency checking is accurate.
			if (IncludedFile.LastWriteTime > WrapperFile.LastWriteTime)
			{
				File.SetLastWriteTimeUtc(WrapperFile.AbsolutePath, DateTime.UtcNow);
				WrapperFile.ResetFileInfo();
			}
			return WrapperFile;
		}

		/// <summary>
		/// Write a list of macro definitions to an output file
		/// </summary>
		/// <param name="Definitions">List of definitions</param>
		/// <param name="Writer">Writer to receive output</param>
		static void WriteDefinitions(IEnumerable<string> Definitions, TextWriter Writer)
		{
			foreach(string Definition in Definitions)
			{
				int EqualsIdx = Definition.IndexOf('=');
				if(EqualsIdx == -1)
				{
					Writer.WriteLine("#define {0} 1", Definition);
				}
				else
				{
					Writer.WriteLine("#define {0} {1}", Definition.Substring(0, EqualsIdx), Definition.Substring(EqualsIdx + 1));
				}
			}
		}

		public static FileItem CachePCHUsageForModuleSourceFile(CppCompileEnvironment ModuleCompileEnvironment, FileItem CPPFile)
		{
			if (!CPPFile.bExists)
			{
				throw new BuildException("Required source file not found: " + CPPFile.AbsolutePath);
			}

			DateTime PCHCacheTimerStart = DateTime.UtcNow;

			// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
			// for header dependencies
			CPPFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;

			FileItem PCHFile = ModuleCompileEnvironment.Headers.CachePCHUsageForCPPFile(CPPFile, ModuleCompileEnvironment.IncludePaths, ModuleCompileEnvironment.Platform);

			if (UnrealBuildTool.bPrintPerformanceInfo)
			{
				double PCHCacheTime = (DateTime.UtcNow - PCHCacheTimerStart).TotalSeconds;
				TotalPCHCacheTime += PCHCacheTime;
			}

			return PCHFile;
		}


		public void CachePCHUsageForModuleSourceFiles(ReadOnlyTargetRules Target, CppCompileEnvironment ModuleCompileEnvironment)
		{
			if(Rules == null || Rules.PCHUsage == ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs || Rules.PrivatePCHHeaderFile != null)
			{
				if(InvalidIncludeDirectiveMessages == null)
				{
					// Find all the source files in this module
					List<FileReference> ModuleFiles = SourceFileSearch.FindModuleSourceFiles(RulesFile);

					// Find headers used by the source file.
					Dictionary<string, FileReference> NameToHeaderFile = new Dictionary<string, FileReference>();
					foreach(FileReference ModuleFile in ModuleFiles)
					{
						if(ModuleFile.HasExtension(".h"))
						{
							NameToHeaderFile[ModuleFile.GetFileNameWithoutExtension()] = ModuleFile;
						}
					}

					// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking for header dependencies
					foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)
					{
						CPPFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;
					}

					// Find the directly included files for each source file, and make sure it includes the matching header if possible
					InvalidIncludeDirectiveMessages = new List<string>();
					if (Rules != null && Rules.bEnforceIWYU && Target.bEnforceIWYU)
					{
						foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)
						{
							List<DependencyInclude> DirectIncludeFilenames = ModuleCompileEnvironment.Headers.GetDirectIncludeDependencies(CPPFile, bOnlyCachedDependencies: false);
							if (DirectIncludeFilenames.Count > 0)
							{
								string IncludeName = Path.GetFileNameWithoutExtension(DirectIncludeFilenames[0].IncludeName);
								string ExpectedName = CPPFile.Location.GetFileNameWithoutExtension();
								if (String.Compare(IncludeName, ExpectedName, StringComparison.InvariantCultureIgnoreCase) != 0)
								{
									FileReference HeaderFile;
									if (NameToHeaderFile.TryGetValue(ExpectedName, out HeaderFile) && !IgnoreMismatchedHeader(ExpectedName))
									{
										InvalidIncludeDirectiveMessages.Add(String.Format("{0}(1): error: Expected {1} to be first header included.", CPPFile.Location, HeaderFile.GetFileName()));
									}
								}
							}
						}
					}
				}
			}
			else
			{
				if (ProcessedDependencies == null)
				{
					DateTime PCHCacheTimerStart = DateTime.UtcNow;

					bool bFoundAProblemWithPCHs = false;

					FileItem UniquePCH = null;
					foreach (FileItem CPPFile in SourceFilesFound.CPPFiles)	// @todo ubtmake: We're not caching CPPEnvironments for .c/.mm files, etc.  Even though they don't use PCHs, they still have #includes!  This can break dependency checking!
					{
						// Store the module compile environment along with the .cpp file.  This is so that we can use it later on when looking
						// for header dependencies
						CPPFile.CachedIncludePaths = ModuleCompileEnvironment.IncludePaths;

						// Find headers used by the source file.
						FileItem PCH = ModuleCompileEnvironment.Headers.CachePCHUsageForCPPFile(CPPFile, ModuleCompileEnvironment.IncludePaths, ModuleCompileEnvironment.Platform);
						if (PCH == null)
						{
							throw new BuildException("Source file \"{0}\" is not including any headers.  We expect all modules to include a header file for precompiled header generation.  Please add an #include statement.", CPPFile.AbsolutePath);
						}

						if (UniquePCH == null)
						{
							UniquePCH = PCH;
						}
						else if (!UniquePCH.Info.Name.Equals(PCH.Info.Name, StringComparison.InvariantCultureIgnoreCase))		// @todo ubtmake: We do a string compare on the file name (not path) here, because sometimes the include resolver will pick an Intermediate copy of a PCH header file and throw off our comparisons
						{
							// OK, looks like we have multiple source files including a different header file first.  We'll keep track of this and print out
							// helpful information afterwards.
							bFoundAProblemWithPCHs = true;
						}
					}

					ProcessedDependencies = new ProcessedDependenciesClass { UniquePCHHeaderFile = UniquePCH };


					if (bFoundAProblemWithPCHs)
					{
						// Map from pch header string to the source files that use that PCH
						Dictionary<FileReference, List<FileItem>> UsageMapPCH = new Dictionary<FileReference, List<FileItem>>();
						foreach (FileItem CPPFile in SourceFilesToBuild.CPPFiles)
						{
							// Create a new entry if not in the pch usage map
							List<FileItem> Files;
							if (!UsageMapPCH.TryGetValue(CPPFile.PrecompiledHeaderIncludeFilename, out Files))
							{
								Files = new List<FileItem>();
								UsageMapPCH.Add(CPPFile.PrecompiledHeaderIncludeFilename, Files);
							}
							Files.Add(CPPFile);
						}

						if (UnrealBuildTool.bPrintDebugInfo)
						{
							Log.TraceVerbose("{0} PCH files for module {1}:", UsageMapPCH.Count, Name);
							int MostFilesIncluded = 0;
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH)
							{
								if (CurPCH.Value.Count > MostFilesIncluded)
								{
									MostFilesIncluded = CurPCH.Value.Count;
								}

								Log.TraceVerbose("   {0}  ({1} files including it: {2}, ...)", CurPCH.Key, CurPCH.Value.Count, CurPCH.Value[0].AbsolutePath);
							}
						}

						if (UsageMapPCH.Count > 1)
						{
							// Keep track of the PCH file that is most used within this module
							FileReference MostFilesAreIncludingPCH = null;
							int MostFilesIncluded = 0;
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH.Where(PCH => PCH.Value.Count > MostFilesIncluded))
							{
								MostFilesAreIncludingPCH = CurPCH.Key;
								MostFilesIncluded = CurPCH.Value.Count;
							}

							// Find all of the files that are not including our "best" PCH header
							StringBuilder FilesNotIncludingBestPCH = new StringBuilder();
							foreach (KeyValuePair<FileReference, List<FileItem>> CurPCH in UsageMapPCH.Where(PCH => PCH.Key != MostFilesAreIncludingPCH))
							{
								foreach (FileItem SourceFile in CurPCH.Value)
								{
									FilesNotIncludingBestPCH.AppendFormat("{0} (including {1})\n", SourceFile.AbsolutePath, CurPCH.Key);
								}
							}

							// Bail out and let the user know which source files may need to be fixed up
							throw new BuildException(
								"All source files in module \"{0}\" must include the same precompiled header first.  Currently \"{1}\" is included by most of the source files.  The following source files are not including \"{1}\" as their first include:\n\n{2}\n\nTo compile this module without implicit precompiled headers, add \"PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;\" to {0}.build.cs.",
								Name,
								MostFilesAreIncludingPCH,
								FilesNotIncludingBestPCH);
						}
					}

					if (UnrealBuildTool.bPrintPerformanceInfo)
					{
						double PCHCacheTime = (DateTime.UtcNow - PCHCacheTimerStart).TotalSeconds;
						TotalPCHCacheTime += PCHCacheTime;
					}
				}
			}
		}

		private bool IgnoreMismatchedHeader(string ExpectedName)
		{
			switch(ExpectedName)
			{
				case "Stats2":
				case "DynamicRHI":
				case "RHICommandList":
				case "RHIUtilities":
					return true;
			}
			switch(Name)
			{
				case "D3D11RHI":
				case "D3D12RHI":
				case "VulkanRHI":
				case "OpenGLDrv":
				case "MetalRHI":
				case "PS4RHI":
                case "Gnmx":
				case "OnlineSubsystemIOS":
				case "OnlineSubsystemLive":
					return true;
			}
			return false;
		}

		/// <summary>
		/// Determine whether optimization should be enabled for a given target
		/// </summary>
		/// <param name="Setting">The optimization setting from the rules file</param>
		/// <param name="Configuration">The active target configuration</param>
		/// <param name="bIsEngineModule">Whether the current module is an engine module</param>
		/// <returns>True if optimization should be enabled</returns>
		public static bool ShouldEnableOptimization(ModuleRules.CodeOptimization Setting, UnrealTargetConfiguration Configuration, bool bIsEngineModule)
		{
			switch(Setting)
			{
				case ModuleRules.CodeOptimization.Never:
					return false;
				case ModuleRules.CodeOptimization.Default:
				case ModuleRules.CodeOptimization.InNonDebugBuilds:
					return (Configuration == UnrealTargetConfiguration.Debug)? false : (Configuration != UnrealTargetConfiguration.DebugGame || bIsEngineModule);
				case ModuleRules.CodeOptimization.InShippingBuildsOnly:
					return (Configuration == UnrealTargetConfiguration.Shipping);
				default:
					return true;
			}
		}

		/// <summary>
		/// Creates a compile environment from a base environment based on the module settings.
		/// </summary>
		/// <param name="Target">Rules for the target being built</param>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <returns>The new module compile environment.</returns>
		public CppCompileEnvironment CreateModuleCompileEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment BaseCompileEnvironment)
		{
			CppCompileEnvironment Result = new CppCompileEnvironment(BaseCompileEnvironment);

			// Override compile environment
			Result.bFasterWithoutUnity = Rules.bFasterWithoutUnity;
			Result.bOptimizeCode = ShouldEnableOptimization(Rules.OptimizeCode, Target.Configuration, Rules.bTreatAsEngineModule);
			Result.bUseRTTI |= Rules.bUseRTTI;
			Result.bUseAVX = Rules.bUseAVX;
			Result.bEnableBufferSecurityChecks = Rules.bEnableBufferSecurityChecks;
			Result.MinSourceFilesForUnityBuildOverride = Rules.MinSourceFilesForUnityBuildOverride;
			Result.MinFilesUsingPrecompiledHeaderOverride = Rules.MinFilesUsingPrecompiledHeaderOverride;
			Result.bBuildLocallyWithSNDBS = Rules.bBuildLocallyWithSNDBS;
			Result.bEnableExceptions |= Rules.bEnableExceptions;
			Result.bEnableObjCExceptions |= Rules.bEnableObjCExceptions;
			Result.bEnableShadowVariableWarnings = Rules.bEnableShadowVariableWarnings;
			Result.bEnableUndefinedIdentifierWarnings = Rules.bEnableUndefinedIdentifierWarnings;

			// Set the macro used to check whether monolithic headers can be used
			if (Rules.bTreatAsEngineModule && (!Rules.bEnforceIWYU || !Target.bEnforceIWYU))
			{
				Result.Definitions.Add("SUPPRESS_MONOLITHIC_HEADER_WARNINGS=1");
			}

			// Add a macro for when we're compiling an engine module, to enable additional compiler diagnostics through code.
			if (Rules.bTreatAsEngineModule)
			{
				Result.Definitions.Add("UE_IS_ENGINE_MODULE=1");
			}
			else
			{
				Result.Definitions.Add("UE_IS_ENGINE_MODULE=0");
			}

			// Switch the optimization flag if we're building a game module. Also pass the definition for building in DebugGame along (see ModuleManager.h for notes).
			if (!Rules.bTreatAsEngineModule)
			{
				if (Target.Configuration == UnrealTargetConfiguration.DebugGame)
				{
					Result.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=1");
				}
				else
				{
					Result.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=0");
				}
			}

			// For game modules, set the define for the project name. This will be used by the IMPLEMENT_PRIMARY_GAME_MODULE macro.
			if (!Rules.bTreatAsEngineModule)
			{
				// Make sure we don't set any define for a non-engine module that's under the engine directory (eg. UE4Game)
				if (Target.ProjectFile != null && RulesFile.IsUnderDirectory(Target.ProjectFile.Directory))
				{
					string ProjectName = Target.ProjectFile.GetFileNameWithoutExtension();
					Result.Definitions.Add(String.Format("UE_PROJECT_NAME={0}", ProjectName));
				}
			}

			// Add the module's public and private definitions.
			Result.Definitions.AddRange(PublicDefinitions);
			Result.Definitions.AddRange(Rules.PrivateDefinitions);

			// Add the project definitions
			if(!Rules.bTreatAsEngineModule)
			{
				Result.Definitions.AddRange(Rules.Target.ProjectDefinitions);
			}

			// Setup the compile environment for the module.
			SetupPrivateCompileEnvironment(Result.IncludePaths.UserIncludePaths, Result.IncludePaths.SystemIncludePaths, Result.Definitions, Result.AdditionalFrameworks, (Rules != null)? Rules.bLegacyPublicIncludePaths.Value : true);

			return Result;
		}

		/// <summary>
		/// Creates a compile environment for a shared PCH from a base environment based on the module settings.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <returns>The new shared PCH compile environment.</returns>
		public CppCompileEnvironment CreateSharedPCHCompileEnvironment(UEBuildTarget Target, CppCompileEnvironment BaseCompileEnvironment)
		{
			CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(BaseCompileEnvironment);

			// Use the default optimization setting for 
			CompileEnvironment.bOptimizeCode = ShouldEnableOptimization(ModuleRules.CodeOptimization.Default, Target.Configuration, Rules.bTreatAsEngineModule);

			// Override compile environment
			CompileEnvironment.bIsBuildingDLL = !Target.ShouldCompileMonolithic();
			CompileEnvironment.bIsBuildingLibrary = false;

			// Add a macro for when we're compiling an engine module, to enable additional compiler diagnostics through code.
			if (Rules.bTreatAsEngineModule)
			{
				CompileEnvironment.Definitions.Add("UE_IS_ENGINE_MODULE=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("UE_IS_ENGINE_MODULE=0");
			}

			// Switch the optimization flag if we're building a game module. Also pass the definition for building in DebugGame along (see ModuleManager.h for notes).
			if (!Rules.bTreatAsEngineModule)
			{
				if (Target.Configuration == UnrealTargetConfiguration.DebugGame)
				{
					CompileEnvironment.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=1");
				}
				else
				{
					CompileEnvironment.Definitions.Add("UE_BUILD_DEVELOPMENT_WITH_DEBUGGAME=0");
				}
			}

			// Add the module's private definitions.
			CompileEnvironment.Definitions.AddRange(PublicDefinitions);

			// Find all the modules that are part of the public compile environment for this module.
			Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
			FindModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);

			// Now set up the compile environment for the modules in the original order that we encountered them
			foreach (UEBuildModule Module in ModuleToIncludePathsOnlyFlag.Keys)
			{
				Module.AddModuleToCompileEnvironment(null, CompileEnvironment.IncludePaths.UserIncludePaths, CompileEnvironment.IncludePaths.SystemIncludePaths, CompileEnvironment.Definitions, CompileEnvironment.AdditionalFrameworks, (Rules != null)? Rules.bLegacyPublicIncludePaths.Value : true);
			}
			return CompileEnvironment;
		}

		public class UHTModuleInfoCacheType
		{
			public UHTModuleInfoCacheType(IEnumerable<string> InHeaderFilenames, UHTModuleInfo InInfo)
			{
				HeaderFilenames = InHeaderFilenames;
				Info = InInfo;
			}

			public IEnumerable<string> HeaderFilenames = null;
			public UHTModuleInfo Info = null;
		}

		private UHTModuleInfoCacheType UHTModuleInfoCache = null;

		/// Total time spent generating PCHs for modules (not actually compiling, but generating the PCH's input data)
		public static double TotalPCHGenTime = 0.0;

		/// Time spent caching which PCH header is included by each module and source file
		public static double TotalPCHCacheTime = 0.0;


		/// <summary>
		/// If any of this module's source files contain UObject definitions, this will return those header files back to the caller
		/// </summary>
		/// <returns></returns>
		public UHTModuleInfoCacheType GetCachedUHTModuleInfo(EGeneratedCodeVersion GeneratedCodeVersion)
		{
			if (UHTModuleInfoCache == null)
			{
				List<FileReference> HeaderFiles = new List<FileReference>();
				FileSystemName[] ExcludedFolders = UEBuildPlatform.GetBuildPlatform(Rules.Target.Platform, true).GetExcludedFolderNames();
				FindHeaders(new DirectoryInfo(ModuleDirectory.FullName), ExcludedFolders, HeaderFiles);
				UHTModuleInfo Info = ExternalExecution.CreateUHTModuleInfo(HeaderFiles, Name, RulesFile, ModuleDirectory, Type, GeneratedCodeVersion);
				UHTModuleInfoCache = new UHTModuleInfoCacheType(Info.PublicUObjectHeaders.Concat(Info.PublicUObjectClassesHeaders).Concat(Info.PrivateUObjectHeaders).Select(x => x.AbsolutePath).ToList(), Info);
			}

			return UHTModuleInfoCache;
		}

		/// <summary>
		/// Find all the headers under the given base directory, excluding any other platform folders.
		/// </summary>
		/// <param name="BaseDir">Base directory to search</param>
		/// <param name="ExcludeFolders">Array of folders to exclude</param>
		/// <param name="Headers">Receives the list of headers that was found</param>
		static void FindHeaders(DirectoryInfo BaseDir, FileSystemName[] ExcludeFolders, List<FileReference> Headers)
		{
			if (!ExcludeFolders.Any(x => x.DisplayName.Equals(BaseDir.Name, StringComparison.InvariantCultureIgnoreCase)))
			{
				foreach (DirectoryInfo SubDir in BaseDir.EnumerateDirectories())
				{
					FindHeaders(SubDir, ExcludeFolders, Headers);
				}
				foreach (FileInfo File in BaseDir.EnumerateFiles("*.h"))
				{
					Headers.Add(new FileReference(File));
				}
			}
		}

		public override void GetAllDependencyModules(List<UEBuildModule> ReferencedModules, HashSet<UEBuildModule> IgnoreReferencedModules, bool bIncludeDynamicallyLoaded, bool bForceCircular, bool bOnlyDirectDependencies)
		{
			List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
			AllDependencyModules.AddRange(PrivateDependencyModules);
			AllDependencyModules.AddRange(PublicDependencyModules);
			if (bIncludeDynamicallyLoaded)
			{
				AllDependencyModules.AddRange(DynamicallyLoadedModules);
			}

			foreach (UEBuildModule DependencyModule in AllDependencyModules)
			{
				if (!IgnoreReferencedModules.Contains(DependencyModule))
				{
					// Don't follow circular back-references!
					bool bIsCircular = HasCircularDependencyOn(DependencyModule.Name);
					if (bForceCircular || !bIsCircular)
					{
						IgnoreReferencedModules.Add(DependencyModule);

						if (!bOnlyDirectDependencies)
						{
							// Recurse into dependent modules first
							DependencyModule.GetAllDependencyModules(ReferencedModules, IgnoreReferencedModules, bIncludeDynamicallyLoaded, bForceCircular, bOnlyDirectDependencies);
						}

						ReferencedModules.Add(DependencyModule);
					}
				}
			}
		}

		public override void RecursivelyAddPrecompiledModules(List<UEBuildModule> Modules)
		{
			if (!Modules.Contains(this))
			{
				Modules.Add(this);

				// Get the dependent modules
				List<UEBuildModule> DependentModules = new List<UEBuildModule>();
				if (PrivateDependencyModules != null)
				{
					DependentModules.AddRange(PrivateDependencyModules);
				}
				if (PublicDependencyModules != null)
				{
					DependentModules.AddRange(PublicDependencyModules);
				}
				if (DynamicallyLoadedModules != null)
				{
					DependentModules.AddRange(DynamicallyLoadedModules);
				}

				// Find modules for each of them, and add their dependencies too
				foreach (UEBuildModule DependentModule in DependentModules)
				{
					DependentModule.RecursivelyAddPrecompiledModules(Modules);
				}
			}
		}
	}
}
