// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	// This enum has to be compatible with the one defined in the
	// UE4\Engine\Source\Runtime\Core\Public\Misc\ComplilationResult.h 
	// to keep communication between UHT, UBT and Editor compiling
	// processes valid.
	enum CompilationResult
	{
		/// <summary>
		/// Compilation succeeded
		/// </summary>
		Succeeded = 0,

		/// <summary>
		/// Build was canceled, this is used on the engine side only
		/// </summary>
		Canceled = 1,

		/// <summary>
		/// All targets were up to date, used only with -canskiplink
		/// </summary>
		UpToDate = 2,

		/// <summary>
		/// The process has most likely crashed. This is what UE returns in case of an assert
		/// </summary>
		CrashOrAssert = 3,

		/// <summary>
		/// Compilation failed because generated code changed which was not supported
		/// </summary>
		FailedDueToHeaderChange = 4,

		/// <summary>
		/// Compilation failed due to compilation errors
		/// </summary>
		OtherCompilationError = 5,

		/// <summary>
		/// Compilation is not supported in the current build
		/// </summary>
		Unsupported,

		/// <summary>
		/// Unknown error
		/// </summary>
		Unknown
	}

	static class CompilationResultExtensions
	{
		public static bool Succeeded(this CompilationResult Result)
		{
			return Result == CompilationResult.Succeeded || Result == CompilationResult.UpToDate;
		}
	}

	class CompilationResultException : BuildException
	{
		public readonly CompilationResult Result;

		public CompilationResultException(CompilationResult Result)
			: base("Error: {0}", Result)
		{
			this.Result = Result;
		}
	}

	/// <summary>
	/// Type of module. Mirrored in UHT as EBuildModuleType.
	/// This should be sorted by the order in which we expect modules to be built.
	/// </summary>
	enum UHTModuleType
	{
		Program,
		EngineRuntime,
		EngineDeveloper,
		EngineEditor,
		EngineThirdParty,
		GameRuntime,
		GameDeveloper,
		GameEditor,
		GameThirdParty,
	}
	static class UHTModuleTypeExtensions
	{
		public static bool IsProgramModule(this UHTModuleType ModuleType)
		{
			return ModuleType == UHTModuleType.Program;
		}
		public static bool IsEngineModule(this UHTModuleType ModuleType)
		{
			return ModuleType == UHTModuleType.EngineRuntime || ModuleType == UHTModuleType.EngineDeveloper || ModuleType == UHTModuleType.EngineEditor || ModuleType == UHTModuleType.EngineThirdParty;
		}
		public static bool IsGameModule(this UHTModuleType ModuleType)
		{
			return ModuleType == UHTModuleType.GameRuntime || ModuleType == UHTModuleType.GameDeveloper || ModuleType == UHTModuleType.GameEditor || ModuleType == UHTModuleType.GameThirdParty;
		}
		public static UHTModuleType? EngineModuleTypeFromHostType(ModuleHostType ModuleType)
		{
			switch (ModuleType)
			{
				case ModuleHostType.Runtime:
				case ModuleHostType.RuntimeNoCommandlet:
                case ModuleHostType.RuntimeAndProgram:
                case ModuleHostType.CookedOnly:
                case ModuleHostType.ServerOnly:
                case ModuleHostType.ClientOnly:
                    return UHTModuleType.EngineRuntime;
				case ModuleHostType.Developer:
					return UHTModuleType.EngineDeveloper;
				case ModuleHostType.Editor:
				case ModuleHostType.EditorNoCommandlet:
					return UHTModuleType.EngineEditor;
				default:
					return null;
			}
		}
		public static UHTModuleType? GameModuleTypeFromHostType(ModuleHostType ModuleType)
		{
			switch (ModuleType)
			{
				case ModuleHostType.Runtime:
				case ModuleHostType.RuntimeNoCommandlet:
                case ModuleHostType.RuntimeAndProgram:
                case ModuleHostType.CookedOnly:
                case ModuleHostType.ServerOnly:
                case ModuleHostType.ClientOnly:
                    return UHTModuleType.GameRuntime;
				case ModuleHostType.Developer:
					return UHTModuleType.GameDeveloper;
				case ModuleHostType.Editor:
				case ModuleHostType.EditorNoCommandlet:
					return UHTModuleType.GameEditor;
				default:
					return null;
			}
		}
	}

	/// <summary>
	/// Information about a module that needs to be passed to UnrealHeaderTool for code generation
	/// </summary>
	class UHTModuleInfo
	{
		/// <summary>
		/// Module name
		/// </summary>
		public string ModuleName;

		/// <summary>
		/// Path to the module rules file
		/// </summary>
		public FileReference ModuleRulesFile;

		/// <summary>
		/// Module base directory
		/// </summary>
		public DirectoryReference ModuleDirectory;

		/// <summary>
		/// Module type
		/// </summary>
		public string ModuleType;

		/// <summary>
		/// Public UObject headers found in the Classes directory (legacy)
		/// </summary>
		public List<FileItem> PublicUObjectClassesHeaders;

		/// <summary>
		/// Public headers with UObjects
		/// </summary>
		public List<FileItem> PublicUObjectHeaders;

		/// <summary>
		/// Private headers with UObjects
		/// </summary>
		public List<FileItem> PrivateUObjectHeaders;

		/// <summary>
		/// Directory containing generated code
		/// </summary>
		public DirectoryItem GeneratedCodeDirectory;

		/// <summary>
		/// Base (i.e. extensionless) path+filename of the .gen files
		/// </summary>
		public string GeneratedCPPFilenameBase;

		/// <summary>
		/// Version of code generated by UHT
		/// </summary>
		public EGeneratedCodeVersion GeneratedCodeVersion;

		/// <summary>
		/// Whether this module is read-only
		/// </summary>
		public bool bIsReadOnly;

		public UHTModuleInfo(string ModuleName, FileReference ModuleRulesFile, DirectoryReference ModuleDirectory, UHTModuleType ModuleType, DirectoryItem GeneratedCodeDirectory, EGeneratedCodeVersion GeneratedCodeVersion, bool bIsReadOnly)
		{
			this.ModuleName = ModuleName;
			this.ModuleRulesFile = ModuleRulesFile;
			this.ModuleDirectory = ModuleDirectory;
			this.ModuleType = ModuleType.ToString();
			this.PublicUObjectClassesHeaders = new List<FileItem>();
			this.PublicUObjectHeaders = new List<FileItem>();
			this.PrivateUObjectHeaders = new List<FileItem>();
			this.GeneratedCodeDirectory = GeneratedCodeDirectory;
			this.GeneratedCodeVersion = GeneratedCodeVersion;
			this.bIsReadOnly = bIsReadOnly;
		}

		public UHTModuleInfo(BinaryArchiveReader Reader)
		{
			ModuleName = Reader.ReadString();
			ModuleRulesFile = Reader.ReadFileReference();
			ModuleDirectory = Reader.ReadDirectoryReference();
			ModuleType = Reader.ReadString();
			PublicUObjectClassesHeaders = Reader.ReadList(() => Reader.ReadFileItem());
			PublicUObjectHeaders = Reader.ReadList(() => Reader.ReadFileItem());
			PrivateUObjectHeaders = Reader.ReadList(() => Reader.ReadFileItem());
			GeneratedCPPFilenameBase = Reader.ReadString();
			GeneratedCodeDirectory = Reader.ReadDirectoryItem();
			GeneratedCodeVersion = (EGeneratedCodeVersion)Reader.ReadInt();
			bIsReadOnly = Reader.ReadBool();
		}

		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteString(ModuleName);
			Writer.WriteFileReference(ModuleRulesFile);
			Writer.WriteDirectoryReference(ModuleDirectory);
			Writer.WriteString(ModuleType);
			Writer.WriteList(PublicUObjectClassesHeaders, Item => Writer.WriteFileItem(Item));
			Writer.WriteList(PublicUObjectHeaders, Item => Writer.WriteFileItem(Item));
			Writer.WriteList(PrivateUObjectHeaders, Item => Writer.WriteFileItem(Item));
			Writer.WriteString(GeneratedCPPFilenameBase);
			Writer.WriteDirectoryItem(GeneratedCodeDirectory);
			Writer.WriteInt((int)GeneratedCodeVersion);
			Writer.WriteBool(bIsReadOnly);
		}

		public override string ToString()
		{
			return ModuleName;
		}
	}

	/// <summary>
	/// This MUST be kept in sync with EGeneratedBodyVersion enum and 
	/// ToGeneratedBodyVersion function in UHT defined in GeneratedCodeVersion.h.
	/// </summary>
	public enum EGeneratedCodeVersion
	{
		/// <summary>
		/// 
		/// </summary>
		None,

		/// <summary>
		/// 
		/// </summary>
		V1,

		/// <summary>
		/// 
		/// </summary>
		V2,

		/// <summary>
		/// 
		/// </summary>
		VLatest = V2
	};

	struct UHTManifest
	{
		public class Module
		{
			public string Name;
			public string ModuleType;
			public string BaseDirectory;
			public string IncludeBase;     // The include path which all UHT-generated includes should be relative to
			public string OutputDirectory;
			public List<string> ClassesHeaders;
			public List<string> PublicHeaders;
			public List<string> PrivateHeaders;
			public string GeneratedCPPFilenameBase;
			public bool SaveExportedHeaders;
			public EGeneratedCodeVersion UHTGeneratedCodeVersion;

			public Module(UHTModuleInfo Info)
			{
				Name = Info.ModuleName;
				ModuleType = Info.ModuleType;
				BaseDirectory = Info.ModuleDirectory.FullName;
				IncludeBase = Info.ModuleDirectory.ParentDirectory.FullName;
				OutputDirectory = Path.GetDirectoryName(Info.GeneratedCPPFilenameBase);
				ClassesHeaders = Info.PublicUObjectClassesHeaders.Select((Header) => Header.AbsolutePath).ToList();
				PublicHeaders = Info.PublicUObjectHeaders.Select((Header) => Header.AbsolutePath).ToList();
				PrivateHeaders = Info.PrivateUObjectHeaders.Select((Header) => Header.AbsolutePath).ToList();
				GeneratedCPPFilenameBase = Info.GeneratedCPPFilenameBase;
				SaveExportedHeaders = !Info.bIsReadOnly;
				UHTGeneratedCodeVersion = Info.GeneratedCodeVersion;
			}

			public override string ToString()
			{
				return Name;
			}
		}

		public bool IsGameTarget;     // True if the current target is a game target
		public string RootLocalPath;    // The engine path on the local machine
		public string TargetName;       // Name of the target currently being compiled
		public string ExternalDependenciesFile; // File to contain additional dependencies that the generated code depends on
		public List<Module> Modules;

		public UHTManifest(string InTargetName, TargetType InTargetType, string InRootLocalPath, string InExternalDependenciesFile, List<Module> InModules)
		{
			IsGameTarget = (InTargetType != TargetType.Program);
			RootLocalPath = InRootLocalPath;
			TargetName = InTargetName;
			ExternalDependenciesFile = InExternalDependenciesFile;
			Modules = InModules;
		}
	}

	class UHTModuleHeaderInfo
	{
		public DirectoryItem SourceFolder;
		public List<FileItem> HeaderFiles;

		public UHTModuleHeaderInfo(DirectoryItem SourceFolder, List<FileItem> HeaderFiles)
		{
			this.SourceFolder = SourceFolder;
			this.HeaderFiles = HeaderFiles;
		}

		public UHTModuleHeaderInfo(BinaryArchiveReader Reader)
		{
			SourceFolder = Reader.ReadDirectoryItem();
			HeaderFiles = Reader.ReadList(() => Reader.ReadFileItem());
		}

		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteDirectoryItem(SourceFolder);
			Writer.WriteList(HeaderFiles, Item => Writer.WriteFileItem(Item));
		}
	}

	/// <summary>
	/// This handles all running of the UnrealHeaderTool
	/// </summary>
	class ExternalExecution
	{
		static UHTModuleType GetEngineModuleTypeFromDescriptor(ModuleDescriptor Module)
		{
            UHTModuleType? Type = UHTModuleTypeExtensions.EngineModuleTypeFromHostType(Module.Type);
            if (Type == null)
            {
                throw new BuildException("Unhandled engine module type {0}", Module.Type.ToString());
            }
            return Type.GetValueOrDefault();
        }

		static UHTModuleType GetGameModuleTypeFromDescriptor(ModuleDescriptor Module)
		{
            UHTModuleType? Type = UHTModuleTypeExtensions.GameModuleTypeFromHostType(Module.Type);
            if (Type == null)
            {
                throw new BuildException("Unhandled game module type {0}", Module.Type.ToString());
            }
            return Type.GetValueOrDefault();
        }

		static UHTModuleType? GetEngineModuleTypeBasedOnLocation(DirectoryReference SourceDirectory, FileReference ModuleFileName)
		{
			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "Runtime")))
			{
				return UHTModuleType.EngineRuntime;
			}

			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "Developer")))
			{
				return UHTModuleType.EngineDeveloper;
			}

			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "Editor")))
			{
				return UHTModuleType.EngineEditor;
			}

			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "Programs")))
			{
				return UHTModuleType.Program;
			}

			if (ModuleFileName.IsUnderDirectory(DirectoryReference.Combine(SourceDirectory, "ThirdParty")))
			{
				return UHTModuleType.EngineThirdParty;
			}

			return null;
		}

		/// <summary>
		/// Returns a copy of Nodes sorted by dependency.  Independent or circularly-dependent nodes should
		/// remain in their same relative order within the original Nodes sequence.
		/// </summary>
		/// <param name="NodeList">The list of nodes to sort.</param>
		static void StableTopologicalSort(List<UEBuildModuleCPP> NodeList)
		{
			int            NodeCount = NodeList.Count;

			Dictionary<UEBuildModule, HashSet<UEBuildModule>> Cache = new Dictionary<UEBuildModule, HashSet<UEBuildModule>>();

			for (int Index1 = 0; Index1 != NodeCount; ++Index1)
			{
				UEBuildModuleCPP Node1 = NodeList[Index1];

				for (int Index2 = 0; Index2 != Index1; ++Index2)
				{
					UEBuildModuleCPP Node2 = NodeList[Index2];

					if (IsDependency(Node2, Node1, Cache) && !IsDependency(Node1, Node2, Cache))
					{
						// Rotate element at Index1 into position at Index2
						for (int Index3 = Index1; Index3 != Index2; )
						{
							--Index3;
							NodeList[Index3 + 1] = NodeList[Index3];
						}
						NodeList[Index2] = Node1;

						// Break out of this loop, because this iteration must have covered all existing cases
						// involving the node formerly at position Index1
						break;
					}
				}
			}
		}

		/// <summary>
		/// Tests whether one module has a dependency on another
		/// </summary>
		/// <param name="FromModule">The module to test</param>
		/// <param name="ToModule">The module to look for a dependency</param>
		/// <param name="Cache">Cache mapping module to all its dependencies</param>
		/// <returns>True if ToModule is a dependency of FromModule, false otherwise</returns>
		static bool IsDependency(UEBuildModuleCPP FromModule, UEBuildModuleCPP ToModule, Dictionary<UEBuildModule, HashSet<UEBuildModule>> Cache)
		{
			HashSet<UEBuildModule> Dependencies;
			if(!Cache.TryGetValue(FromModule, out Dependencies))
			{
				Dependencies = new HashSet<UEBuildModule>();
				FromModule.GetAllDependencyModules(new List<UEBuildModule>(), Dependencies, true, true, false);
				Cache.Add(FromModule, Dependencies);
			}
			return Dependencies.Contains(ToModule);
		}

		/// <summary>
		/// Gets the module type for a given rules object
		/// </summary>
		/// <param name="RulesObject">The rules object</param>
		/// <param name="ProjectDescriptor">Descriptor for the project being built</param>
		/// <returns>The module type</returns>
		static UHTModuleType GetModuleType(ModuleRules RulesObject, ProjectDescriptor ProjectDescriptor)
		{
			// Get the type of module we're creating
			UHTModuleType? ModuleType = null;

			// Get the module descriptor for this module if it's a plugin
			ModuleDescriptor PluginModuleDesc = null;
			if (RulesObject.Plugin != null)
			{
				PluginModuleDesc = RulesObject.Plugin.Descriptor.Modules.FirstOrDefault(x => x.Name == RulesObject.Name);
				if (PluginModuleDesc != null && PluginModuleDesc.Type == ModuleHostType.Program)
				{
					ModuleType = UHTModuleType.Program;
				}
			}

			if (UnrealBuildTool.IsUnderAnEngineDirectory(RulesObject.File.Directory))
			{
				if (RulesObject.Type == ModuleRules.ModuleType.External)
				{
					ModuleType = UHTModuleType.EngineThirdParty;
				}
				else
				{
					if (!ModuleType.HasValue && PluginModuleDesc != null)
					{
						ModuleType = ExternalExecution.GetEngineModuleTypeFromDescriptor(PluginModuleDesc);
					}

					if (!ModuleType.HasValue)
					{
						if (RulesObject.File.IsUnderDirectory(UnrealBuildTool.EngineDirectory))
						{
							ModuleType = ExternalExecution.GetEngineModuleTypeBasedOnLocation(UnrealBuildTool.EngineSourceDirectory, RulesObject.File);
						}
						else if (RulesObject.File.IsUnderDirectory(UnrealBuildTool.EnterpriseSourceDirectory))
						{
							ModuleType = ExternalExecution.GetEngineModuleTypeBasedOnLocation(UnrealBuildTool.EnterpriseSourceDirectory, RulesObject.File);
						}
					}
				}
			}
			else
			{
				if (RulesObject.Type == ModuleRules.ModuleType.External)
				{
					ModuleType = UHTModuleType.GameThirdParty;
				}
				else
				{
					if (!ModuleType.HasValue && PluginModuleDesc != null)
					{
						ModuleType = ExternalExecution.GetGameModuleTypeFromDescriptor(PluginModuleDesc);
					}

					if (!ModuleType.HasValue)
					{
						if (ProjectDescriptor != null)
						{
							ModuleDescriptor ProjectModule = (ProjectDescriptor.Modules == null)? null : ProjectDescriptor.Modules.FirstOrDefault(x => x.Name == RulesObject.Name);
							if (ProjectModule != null)
							{
								ModuleType = UHTModuleTypeExtensions.GameModuleTypeFromHostType(ProjectModule.Type) ?? UHTModuleType.GameRuntime;
							}
							else
							{
								// No descriptor file or module was not on the list
								ModuleType = UHTModuleType.GameRuntime;
							}
						}
					}
				}
			}

			if (!ModuleType.HasValue)
			{
				throw new BuildException("Unable to determine UHT module type for {0}", RulesObject.File);
			}

			return ModuleType.Value;
		}

		/// <summary>
		/// Find all the headers under the given base directory, excluding any other platform folders.
		/// </summary>
		/// <param name="BaseDirectory">Base directory to search</param>
		/// <param name="ExcludeFolders">Array of folders to exclude</param>
		/// <param name="Headers">Receives the list of headers that was found</param>
		static void FindHeaders(DirectoryItem BaseDirectory, ReadOnlyHashSet<string> ExcludeFolders, List<FileItem> Headers)
		{
			foreach(DirectoryItem SubDirectory in BaseDirectory.EnumerateDirectories())
			{
				if(!ExcludeFolders.Contains(SubDirectory.Name))
				{
					FindHeaders(SubDirectory, ExcludeFolders, Headers);
				}
			}
			foreach(FileItem File in BaseDirectory.EnumerateFiles())
			{
				if(File.HasExtension(".h"))
				{
					Headers.Add(File);
				}
			}
		}

		public static void SetupUObjectModules(IEnumerable<UEBuildModuleCPP> ModulesToGenerateHeadersFor, UnrealTargetPlatform Platform, ProjectDescriptor ProjectDescriptor, List<UHTModuleInfo> UObjectModules, List<UHTModuleHeaderInfo> UObjectModuleHeaders, EGeneratedCodeVersion GeneratedCodeVersion, bool bIsAssemblingBuild, SourceFileMetadataCache MetadataCache)
		{
			// Find the type of each module
			Dictionary<UEBuildModuleCPP, UHTModuleType> ModuleToType = new Dictionary<UEBuildModuleCPP, UHTModuleType>();
			foreach(UEBuildModuleCPP Module in ModulesToGenerateHeadersFor)
			{
				ModuleToType[Module] = GetModuleType(Module.Rules, ProjectDescriptor);
			}

			// Sort modules by type, then by dependency
			List<UEBuildModuleCPP> ModulesSortedByType = ModulesToGenerateHeadersFor.OrderBy(c => ModuleToType[c]).ToList();
			StableTopologicalSort(ModulesSortedByType);

			// Create the info for each module in parallel
			UHTModuleInfo[] ModuleInfoArray = new UHTModuleInfo[ModulesSortedByType.Count];
			using(ThreadPoolWorkQueue Queue = new ThreadPoolWorkQueue())
			{
				ReadOnlyHashSet<string> ExcludedFolders = UEBuildPlatform.GetBuildPlatform(Platform, true).GetExcludedFolderNames();
				for(int Idx = 0; Idx < ModulesSortedByType.Count; Idx++)
				{
					UEBuildModuleCPP Module = ModulesSortedByType[Idx];

					UHTModuleInfo Info = new UHTModuleInfo(Module.Name, Module.RulesFile, Module.ModuleDirectory, ModuleToType[Module], DirectoryItem.GetItemByDirectoryReference(Module.GeneratedCodeDirectory), GeneratedCodeVersion, Module.Rules.bUsePrecompiled);
					ModuleInfoArray[Idx] = Info;

					Queue.Enqueue(() => SetupUObjectModule(Info, ExcludedFolders, MetadataCache, Queue));
				}
			}

			// Filter out all the modules with reflection data
			for(int Idx = 0; Idx < ModulesSortedByType.Count; Idx++)
			{
				UEBuildModuleCPP Module = ModulesSortedByType[Idx];
				UHTModuleInfo Info = ModuleInfoArray[Idx];
				if (Info.PublicUObjectClassesHeaders.Count > 0 || Info.PrivateUObjectHeaders.Count > 0 || Info.PublicUObjectHeaders.Count > 0)
				{
					// Set a flag indicating that we need to add the generated headers directory
					Module.bAddGeneratedCodeIncludePath = true;

					// If we've got this far and there are no source files then it's likely we're installed and ignoring
					// engine files, so we don't need a .gen.cpp either
					Info.GeneratedCPPFilenameBase = Path.Combine(Module.GeneratedCodeDirectory.FullName, Info.ModuleName) + ".gen";
					if (!Module.Rules.bUsePrecompiled)
					{
						Module.GeneratedCodeWildcard = Path.Combine(Module.GeneratedCodeDirectory.FullName, "*.gen.cpp");
					}

					UObjectModules.Add(Info);

					DirectoryItem ModuleDirectoryItem = DirectoryItem.GetItemByDirectoryReference(Module.ModuleDirectory);

					List<FileItem> ReflectedHeaderFiles = new List<FileItem>();
					ReflectedHeaderFiles.AddRange(Info.PublicUObjectHeaders);
					ReflectedHeaderFiles.AddRange(Info.PublicUObjectClassesHeaders);
					ReflectedHeaderFiles.AddRange(Info.PrivateUObjectHeaders);
					UObjectModuleHeaders.Add(new UHTModuleHeaderInfo(ModuleDirectoryItem, ReflectedHeaderFiles));
				}
				else
				{
					// Remove any stale generated code directory
					if(Module.GeneratedCodeDirectory != null && !Module.Rules.bUsePrecompiled)
					{
						if (DirectoryReference.Exists(Module.GeneratedCodeDirectory))
						{
							Directory.Delete(Module.GeneratedCodeDirectory.FullName, true);
						}
					}
				}
			}
		}

		static void SetupUObjectModule(UHTModuleInfo ModuleInfo, ReadOnlyHashSet<string> ExcludedFolders, SourceFileMetadataCache MetadataCache, ThreadPoolWorkQueue Queue)
		{
			DirectoryItem ModuleDirectoryItem = DirectoryItem.GetItemByDirectoryReference(ModuleInfo.ModuleDirectory);

			List<FileItem> HeaderFiles = new List<FileItem>();
			FindHeaders(ModuleDirectoryItem, ExcludedFolders, HeaderFiles);

			foreach (FileItem HeaderFile in HeaderFiles)
			{
				Queue.Enqueue(() => SetupUObjectModuleHeader(ModuleInfo, HeaderFile, MetadataCache));
			}
		}

		static void SetupUObjectModuleHeader(UHTModuleInfo ModuleInfo, FileItem HeaderFile, SourceFileMetadataCache MetadataCache)
		{
			// Check to see if we know anything about this file.  If we have up-to-date cached information about whether it has
			// UObjects or not, we can skip doing a test here.
			if (MetadataCache.ContainsReflectionMarkup(HeaderFile))
			{
				lock(ModuleInfo)
				{
					if (HeaderFile.Location.IsUnderDirectory(DirectoryReference.Combine(ModuleInfo.ModuleDirectory, "Classes")))
					{
						ModuleInfo.PublicUObjectClassesHeaders.Add(HeaderFile);
					}
					else if (HeaderFile.Location.IsUnderDirectory(DirectoryReference.Combine(ModuleInfo.ModuleDirectory, "Public")))
					{
						ModuleInfo.PublicUObjectHeaders.Add(HeaderFile);
					}
					else
					{
						ModuleInfo.PrivateUObjectHeaders.Add(HeaderFile);
					}
				}
			}
		}

		/// <summary>
		/// Gets the path to the receipt for UHT
		/// </summary>
		/// <returns>Path to the UHT receipt</returns>
		public static FileReference GetHeaderToolReceiptFile(FileReference ProjectFile, UnrealTargetConfiguration Configuration, bool bHasProjectScriptPlugin)
		{
			if(bHasProjectScriptPlugin && ProjectFile != null)
			{
				return TargetReceipt.GetDefaultPath(ProjectFile.Directory, "UnrealHeaderTool", BuildHostPlatform.Current.Platform, Configuration, "");
			}
			else
			{
				return TargetReceipt.GetDefaultPath(UnrealBuildTool.EngineDirectory, "UnrealHeaderTool", BuildHostPlatform.Current.Platform, Configuration, "");
			}
		}

		/// <summary>
		/// Gets UnrealHeaderTool.exe path. Does not care if UnrealheaderTool was build as a monolithic exe or not.
		/// </summary>
		static FileReference GetHeaderToolPath(FileReference ReceiptFile)
		{
			TargetReceipt Receipt = TargetReceipt.Read(ReceiptFile);
			return Receipt.Launch;
		}

		/// <summary>
		/// Gets the latest write time of any of the UnrealHeaderTool binaries (including DLLs and Plugins) or DateTime.MaxValue if UnrealHeaderTool does not exist
		/// </summary>
		/// <returns>Latest timestamp of UHT binaries or DateTime.MaxValue if UnrealHeaderTool is out of date and needs to be rebuilt.</returns>
		static bool GetHeaderToolTimestampUtc(FileReference ReceiptPath, out DateTime Timestamp)
		{
			using (ScopedTimer TimestampTimer = new ScopedTimer("GetHeaderToolTimestamp"))
			{
				// Try to read the receipt for UHT.
				FileItem ReceiptFile = FileItem.GetItemByFileReference(ReceiptPath);
				if (!ReceiptFile.Exists)
				{
					Timestamp = DateTime.MaxValue;
					return false;
				}

				TargetReceipt Receipt;
				if (!TargetReceipt.TryRead(ReceiptPath, out Receipt))
				{
					Timestamp = DateTime.MaxValue;
					return false;
				}

				// Make sure all the build products exist, and that the receipt is newer
				foreach (BuildProduct BuildProduct in Receipt.BuildProducts)
				{
					FileItem BuildProductItem = FileItem.GetItemByFileReference(BuildProduct.Path);
					if(!BuildProductItem.Exists || BuildProductItem.LastWriteTimeUtc > ReceiptFile.LastWriteTimeUtc)
					{
						Timestamp = DateTime.MaxValue;
						return false;
					}
				}

				// Return the timestamp for all the binaries
				Timestamp = ReceiptFile.LastWriteTimeUtc;
				return true;
			}
		}

		/// <summary>
		/// Gets the timestamp of CoreUObject.gen.cpp file.
		/// </summary>
		/// <returns>Last write time of CoreUObject.gen.cpp or DateTime.MaxValue if it doesn't exist.</returns>
		private static DateTime GetCoreGeneratedTimestampUtc(string ModuleName, string ModuleGeneratedCodeDirectory)
		{
			// In Installed Builds, we don't check the timestamps on engine headers.  Default to a very old date.
			if (UnrealBuildTool.IsEngineInstalled())
			{
				return DateTime.MinValue;
			}

			// Otherwise look for CoreUObject.init.gen.cpp
			FileInfo CoreGeneratedFileInfo = new FileInfo(Path.Combine(ModuleGeneratedCodeDirectory, ModuleName + ".init.gen.cpp"));
			if (CoreGeneratedFileInfo.Exists)
			{
				return CoreGeneratedFileInfo.LastWriteTimeUtc;
			}

			// Doesn't exist, so use a 'newer that everything' date to force rebuild headers.
			return DateTime.MaxValue;
		}

		/// <summary>
		/// Checks the class header files and determines if generated UObject code files are out of date in comparison.
		/// </summary>
		/// <param name="BuildConfiguration">Build configuration</param>
		/// <param name="UObjectModules">Modules that we generate headers for</param>
		/// <param name="HeaderToolTimestampUtc">Timestamp for UHT</param>
		/// <param name="bIsGatheringBuild"></param>
		/// <param name="bIsAssemblingBuild"></param>
		/// <returns>True if the code files are out of date</returns>
		private static bool AreGeneratedCodeFilesOutOfDate(BuildConfiguration BuildConfiguration, List<UHTModuleInfo> UObjectModules, DateTime HeaderToolTimestampUtc, bool bIsGatheringBuild, bool bIsAssemblingBuild)
		{
			// Get CoreUObject.init.gen.cpp timestamp.  If the source files are older than the CoreUObject generated code, we'll
			// need to regenerate code for the module
			DateTime? CoreGeneratedTimestampUtc = null;
			{
				// Find the CoreUObject module
				foreach (UHTModuleInfo Module in UObjectModules)
				{
					if (Module.ModuleName.Equals("CoreUObject", StringComparison.InvariantCultureIgnoreCase))
					{
						CoreGeneratedTimestampUtc = GetCoreGeneratedTimestampUtc(Module.ModuleName, Path.GetDirectoryName(Module.GeneratedCPPFilenameBase));
						break;
					}
				}
				if (CoreGeneratedTimestampUtc == null)
				{
					throw new BuildException("Could not find CoreUObject in list of all UObjectModules");
				}
			}


			foreach (UHTModuleInfo Module in UObjectModules)
			{
				// If we're using a precompiled engine, skip checking timestamps for modules that are under the engine directory
				if (Module.bIsReadOnly)
				{
					continue;
				}

				// Make sure we have an existing folder for generated code.  If not, then we definitely need to generate code!
				string GeneratedCodeDirectory = Path.GetDirectoryName(Module.GeneratedCPPFilenameBase);
				FileSystemInfo TestDirectory = (FileSystemInfo)new DirectoryInfo(GeneratedCodeDirectory);
				if (!TestDirectory.Exists)
				{
					// Generated code directory is missing entirely!
					Log.TraceLog("UnrealHeaderTool needs to run because no generated code directory was found for module {0}", Module.ModuleName);
					return true;
				}

				// Grab our special "Timestamp" file that we saved after the last set of headers were generated.  This file
				// actually contains the list of source files which contained UObjects, so that we can compare to see if any
				// UObject source files were deleted (or no longer contain UObjects), which means we need to run UHT even
				// if no other source files were outdated
				string TimestampFile = Path.Combine(GeneratedCodeDirectory, @"Timestamp");
				FileSystemInfo SavedTimestampFileInfo = (FileSystemInfo)new FileInfo(TimestampFile);
				if (!SavedTimestampFileInfo.Exists)
				{
					// Timestamp file was missing (possibly deleted/cleaned), so headers are out of date
					Log.TraceLog("UnrealHeaderTool needs to run because UHT Timestamp file did not exist for module {0}", Module.ModuleName);
					return true;
				}

				// Make sure the last UHT run completed after UnrealHeaderTool.exe was compiled last, and after the CoreUObject headers were touched last.
				DateTime SavedTimestampUtc = SavedTimestampFileInfo.LastWriteTimeUtc;
				if (HeaderToolTimestampUtc > SavedTimestampUtc)
				{
					// Generated code is older than UnrealHeaderTool.exe.  Out of date!
					Log.TraceLog("UnrealHeaderTool needs to run because UnrealHeaderTool timestamp ({0}) is later than timestamp for module {1} ({2})", HeaderToolTimestampUtc.ToLocalTime(), Module.ModuleName, SavedTimestampUtc.ToLocalTime());
					return true;
				}
				if (CoreGeneratedTimestampUtc > SavedTimestampUtc)
				{
					// Generated code is older than CoreUObject headers.  Out of date!
					Log.TraceLog("UnrealHeaderTool needs to run because CoreUObject timestamp ({0}) is newer than timestamp for module {1} ({2})", CoreGeneratedTimestampUtc.Value.ToLocalTime(), Module.ModuleName, SavedTimestampUtc.ToLocalTime());
					return true;
				}

				// Has the .build.cs file changed since we last generated headers successfully?
				FileInfo ModuleRulesFile = new FileInfo(Module.ModuleRulesFile.FullName);
				if (!ModuleRulesFile.Exists || ModuleRulesFile.LastWriteTimeUtc > SavedTimestampUtc)
				{
					Log.TraceLog("UnrealHeaderTool needs to run because SavedTimestamp is older than the rules file ({0}) for module {1}", Module.ModuleRulesFile, Module.ModuleName);
					return true;
				}

				// Iterate over our UObjects headers and figure out if any of them have changed
				List<FileItem> AllUObjectHeaders = new List<FileItem>();
				AllUObjectHeaders.AddRange(Module.PublicUObjectClassesHeaders);
				AllUObjectHeaders.AddRange(Module.PublicUObjectHeaders);
				AllUObjectHeaders.AddRange(Module.PrivateUObjectHeaders);

				// Load up the old timestamp file and check to see if anything has changed
				{
					string[] UObjectFilesFromPreviousRun = File.ReadAllLines(TimestampFile);
					if (AllUObjectHeaders.Count != UObjectFilesFromPreviousRun.Length)
					{
						Log.TraceLog("UnrealHeaderTool needs to run because there are a different number of UObject source files in module {0}", Module.ModuleName);
						return true;
					}

					// Iterate over our UObjects headers and figure out if any of them have changed
					HashSet<string> ObjectHeadersSet = new HashSet<string>(AllUObjectHeaders.Select(x => x.AbsolutePath), FileReference.Comparer);
					foreach (string FileName in UObjectFilesFromPreviousRun)
					{
						if(!ObjectHeadersSet.Contains(FileName))
						{
							Log.TraceLog("UnrealHeaderTool needs to run because the set of UObject source files in module {0} has changed ({1})", Module.ModuleName, FileName);
							return true;
						}
					}
				}

				foreach (FileItem HeaderFile in AllUObjectHeaders)
				{
					DateTime HeaderFileTimestampUtc = HeaderFile.LastWriteTimeUtc;

					// Has the source header changed since we last generated headers successfully?
					if (HeaderFileTimestampUtc > SavedTimestampUtc)
					{
						Log.TraceLog("UnrealHeaderTool needs to run because SavedTimestamp is older than HeaderFileTimestamp ({0}) for module {1}", HeaderFile.AbsolutePath, Module.ModuleName);
						return true;
					}

					// When we're running in assembler mode, outdatedness cannot be inferred by checking the directory timestamp
					// of the source headers.  We don't care if source files were added or removed in this mode, because we're only
					// able to process the known UObject headers that are in the Makefile.  If UObject header files are added/removed,
					// we expect the user to re-run GenerateProjectFiles which will force UBTMakefile outdatedness.
					// @todo ubtmake: Possibly, we should never be doing this check these days.
					//
					// We don't need to do this check if using hot reload makefiles, since makefile out-of-date checks already handle it.
					if (!BuildConfiguration.bUseUBTMakefiles && (bIsGatheringBuild || !bIsAssemblingBuild))
					{
						// Also check the timestamp on the directory the source file is in.  If the directory timestamp has
						// changed, new source files may have been added or deleted.  We don't know whether the new/deleted
						// files were actually UObject headers, but because we don't know all of the files we processed
						// in the previous run, we need to assume our generated code is out of date if the directory timestamp
						// is newer.
						DateTime HeaderDirectoryTimestampUtc = new DirectoryInfo(Path.GetDirectoryName(HeaderFile.AbsolutePath)).LastWriteTimeUtc;
						if (HeaderDirectoryTimestampUtc > SavedTimestampUtc)
						{
							Log.TraceLog("UnrealHeaderTool needs to run because the directory containing an existing header ({0}) has changed, and headers may have been added to or deleted from module {1}", HeaderFile.AbsolutePath, Module.ModuleName);
							return true;
						}
					}
				}
			}

			return false;
		}

		/// <summary>
		/// Determines if any external dependencies for generated code is out of date
		/// </summary>
		/// <param name="ExternalDependenciesFile">Path to the external dependencies file</param>
		/// <returns>True if any external dependencies are out of date</returns>
		private static bool AreExternalDependenciesOutOfDate(FileReference ExternalDependenciesFile)
		{
			if (!FileReference.Exists(ExternalDependenciesFile))
			{
				return true;
			}

			DateTime LastWriteTime = File.GetLastWriteTimeUtc(ExternalDependenciesFile.FullName);

			string[] Lines = File.ReadAllLines(ExternalDependenciesFile.FullName);
			foreach (string Line in Lines)
			{
				string ExternalDependencyFile = Line.Trim();
				if (ExternalDependencyFile.Length > 0)
				{
					if (!File.Exists(ExternalDependencyFile) || File.GetLastWriteTimeUtc(ExternalDependencyFile) > LastWriteTime)
					{
						return true;
					}
				}
			}

			return false;
		}

		/// <summary>
		/// Updates the intermediate include directory timestamps of all the passed in UObject modules
		/// </summary>
		private static void UpdateDirectoryTimestamps(List<UHTModuleInfo> UObjectModules)
		{
			foreach (UHTModuleInfo Module in UObjectModules)
			{
				if(!Module.bIsReadOnly)
				{
					string GeneratedCodeDirectory = Path.GetDirectoryName(Module.GeneratedCPPFilenameBase);
					DirectoryInfo GeneratedCodeDirectoryInfo = new DirectoryInfo(GeneratedCodeDirectory);

					try
					{
						if (GeneratedCodeDirectoryInfo.Exists)
						{
							// Touch the include directory since we have technically 'generated' the headers
							// However, the headers might not be touched at all since that would cause the compiler to recompile everything
							// We can't alter the directory timestamp directly, because this may throw exceptions when the directory is
							// open in visual studio or windows explorer, so instead we create a blank file that will change the timestamp for us
							FileReference TimestampFile = FileReference.Combine(new DirectoryReference(GeneratedCodeDirectoryInfo.FullName), "Timestamp");

							// Save all of the UObject files to a timestamp file.  We'll load these on the next run to see if any new
							// files with UObject classes were deleted, so that we'll know to run UHT even if the timestamps of all
							// of the other source files were unchanged
							{
								List<string> AllUObjectFiles = new List<string>();
								AllUObjectFiles.AddRange(Module.PublicUObjectClassesHeaders.ConvertAll(Item => Item.AbsolutePath));
								AllUObjectFiles.AddRange(Module.PublicUObjectHeaders.ConvertAll(Item => Item.AbsolutePath));
								AllUObjectFiles.AddRange(Module.PrivateUObjectHeaders.ConvertAll(Item => Item.AbsolutePath));
								FileReference.WriteAllLines(TimestampFile, AllUObjectFiles);
							}

							// Because new .cpp and .h files may have been generated by UHT, invalidate the DirectoryLookupCache
							DirectoryLookupCache.InvalidateCachedDirectory(new DirectoryReference(GeneratedCodeDirectoryInfo.FullName));
						}
					}
					catch (Exception Exception)
					{
						throw new BuildException(Exception, "Couldn't touch header directories: " + Exception.Message);
					}
				}
			}
		}

		/// <summary>
		/// Run an external exe (and capture the output), given the exe path and the commandline.
		/// </summary>
		public static int RunExternalDotNETExecutable(string ExePath, string Commandline)
		{
#if NET_CORE
			ProcessStartInfo ExeInfo = new ProcessStartInfo("dotnet", ExePath + " " + Commandline);
#else
			ProcessStartInfo ExeInfo = new ProcessStartInfo(ExePath, Commandline);
#endif
			Log.TraceVerbose("RunExternalExecutable {0} {1}", ExePath, Commandline);
			ExeInfo.UseShellExecute = false;
			ExeInfo.RedirectStandardOutput = true;
			using (Process GameProcess = Process.Start(ExeInfo))
			{
				GameProcess.BeginOutputReadLine();
				GameProcess.OutputDataReceived += PrintProcessOutputAsync;
				GameProcess.WaitForExit();

				return GameProcess.ExitCode;
			}
		}

		/// <summary>
		/// Run an external native executable (and capture the output), given the executable path and the commandline.
		/// </summary>
		public static int RunExternalNativeExecutable(FileReference ExePath, string Commandline)
		{
			ProcessStartInfo ExeInfo = new ProcessStartInfo(ExePath.FullName, Commandline);
			Log.TraceVerbose("RunExternalExecutable {0} {1}", ExePath.FullName, Commandline);
			ExeInfo.UseShellExecute = false;
			ExeInfo.RedirectStandardOutput = true;
			using (Process GameProcess = Process.Start(ExeInfo))
			{
				GameProcess.BeginOutputReadLine();
				GameProcess.OutputDataReceived += PrintProcessOutputAsync;
				GameProcess.WaitForExit();

				return GameProcess.ExitCode;
			}
		}

		/// <summary>
		/// Simple function to pipe output asynchronously
		/// </summary>
		private static void PrintProcessOutputAsync(object Sender, DataReceivedEventArgs Event)
		{
			// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
			// print anything for that event.
			if (!String.IsNullOrEmpty(Event.Data))
			{
				Log.TraceInformation(Event.Data);
			}
		}

		/// <summary>
		/// Builds and runs the header tool and touches the header directories.
		/// Performs any early outs if headers need no changes, given the UObject modules, tool path, game name, and configuration
		/// </summary>
		public static void ExecuteHeaderToolIfNecessary(BuildConfiguration BuildConfiguration, FileReference ProjectFile, string TargetName, TargetType TargetType, bool bHasProjectScriptPlugin, List<UHTModuleInfo> UObjectModules, FileReference ModuleInfoFileName, bool bIsGatheringBuild, bool bIsAssemblingBuild, ISourceFileWorkingSet WorkingSet)
		{
			if (ProgressWriter.bWriteMarkup)
			{
				Log.WriteLine(LogEventType.Console, "@progress push 5%");
			}
			using (ProgressWriter Progress = new ProgressWriter("Generating code...", false))
			{
				// We never want to try to execute the header tool when we're already trying to build it!
				bool bIsBuildingUHT = TargetName.Equals("UnrealHeaderTool", StringComparison.InvariantCultureIgnoreCase);

				string RootLocalPath = UnrealBuildTool.RootDirectory.FullName;

				UnrealTargetConfiguration UHTConfig = BuildConfiguration.bForceDebugUnrealHeaderTool ? UnrealTargetConfiguration.Debug : UnrealTargetConfiguration.Development;

				// Figure out the receipt path
				FileReference HeaderToolReceipt = GetHeaderToolReceiptFile(ProjectFile, UHTConfig, bHasProjectScriptPlugin);

				// check if UHT is out of date
				DateTime HeaderToolTimestampUtc = DateTime.MaxValue;
				bool bHaveHeaderTool = !bIsBuildingUHT && GetHeaderToolTimestampUtc(HeaderToolReceipt, out HeaderToolTimestampUtc);

				// ensure the headers are up to date
				bool bUHTNeedsToRun = false;
				if(!bHaveHeaderTool)
				{
					bUHTNeedsToRun = true;
				}
				else if(BuildConfiguration.bForceHeaderGeneration)
				{
					bUHTNeedsToRun = true;
				}
				else if(AreGeneratedCodeFilesOutOfDate(BuildConfiguration, UObjectModules, HeaderToolTimestampUtc, bIsGatheringBuild, bIsAssemblingBuild))
				{
					bUHTNeedsToRun = true;
				}

				// Get the file containing dependencies for the generated code
				FileReference ExternalDependenciesFile = ModuleInfoFileName.ChangeExtension(".deps");
				if (AreExternalDependenciesOutOfDate(ExternalDependenciesFile))
				{
					bUHTNeedsToRun = true;
					bHaveHeaderTool = false; // Force UHT to build until dependency checking is fast enough to run all the time
				}

				// @todo ubtmake: Optimization: Ideally we could avoid having to generate this data in the case where UHT doesn't even need to run!  Can't we use the existing copy?  (see below use of Manifest)

				List<UHTManifest.Module> Modules = new List<UHTManifest.Module>();
				foreach(UHTModuleInfo UObjectModule in UObjectModules)
				{
					Modules.Add(new UHTManifest.Module(UObjectModule));
				}
				UHTManifest Manifest = new UHTManifest(TargetName, TargetType, RootLocalPath, ExternalDependenciesFile.FullName, Modules);

				if (!bIsBuildingUHT && bUHTNeedsToRun)
				{
					// Always build UnrealHeaderTool if header regeneration is required, unless we're running within an installed ecosystem or hot-reloading
					if ((!UnrealBuildTool.IsEngineInstalled() || bHasProjectScriptPlugin) &&
						!BuildConfiguration.bDoNotBuildUHT &&
						!(bHaveHeaderTool && !bIsGatheringBuild && bIsAssemblingBuild))	// If running in "assembler only" mode, we assume UHT is already up to date for much faster iteration!
					{
						// If it is out of date or not there it will be built.
						// If it is there and up to date, it will add 0.8 seconds to the build time.

						// Which desktop platform do we need to compile UHT for?
						UnrealTargetPlatform Platform = BuildHostPlatform.Current.Platform;

						// NOTE: We force Development configuration for UHT so that it runs quickly, even when compiling debug, unless we say so explicitly
						UnrealTargetConfiguration Configuration;
						if (BuildConfiguration.bForceDebugUnrealHeaderTool)
						{
							Configuration = UnrealTargetConfiguration.Debug;
						}
						else
						{
							Configuration = UnrealTargetConfiguration.Development;
						}

						// Get the default architecture
						string Architecture = UEBuildPlatform.GetBuildPlatform(Platform).GetDefaultArchitecture(null);

						// Add UHT plugins to UBT command line as external plugins
						FileReference ScriptProjectFile = null;
						if(bHasProjectScriptPlugin && ProjectFile != null)
						{
							ScriptProjectFile = ProjectFile;
						}

						// Create the target descriptor
						List<TargetDescriptor> TargetDescriptors = new List<TargetDescriptor>();
						TargetDescriptors.Add(new TargetDescriptor(ScriptProjectFile, "UnrealHeaderTool", Platform, Configuration, Architecture, null));

						using(Timeline.ScopeEvent("Buildng UnrealHeaderTool"))
						{
							BuildMode.Build(TargetDescriptors, BuildConfiguration, WorkingSet, BuildOptions.Quiet);
						}
					}

					Progress.Write(1, 3);

					string ActualTargetName = String.IsNullOrEmpty(TargetName) ? "UE4" : TargetName;
					Log.TraceInformation("Parsing headers for {0}", ActualTargetName);

					FileReference HeaderToolPath = GetHeaderToolPath(HeaderToolReceipt);
					if (!FileReference.Exists(HeaderToolPath))
					{
						throw new BuildException("Unable to generate headers because UnrealHeaderTool binary was not found ({0}).", HeaderToolPath);
					}

					// Disable extensions when serializing to remove the $type fields
					Directory.CreateDirectory(ModuleInfoFileName.Directory.FullName);
					System.IO.File.WriteAllText(ModuleInfoFileName.FullName, fastJSON.JSON.Instance.ToJSON(Manifest, new fastJSON.JSONParameters { UseExtensions = false }));

					string CmdLine = (ProjectFile != null) ? "\"" + ProjectFile.FullName + "\"" : TargetName;
					CmdLine += " \"" + ModuleInfoFileName + "\" -LogCmds=\"loginit warning, logexit warning, logdatabase error\" -Unattended -WarningsAsErrors";
					if (UnrealBuildTool.IsEngineInstalled())
					{
						CmdLine += " -installed";
					}

					if (BuildConfiguration.bFailIfGeneratedCodeChanges)
					{
						CmdLine += " -FailIfGeneratedCodeChanges";
					}

					Log.TraceInformation("  Running UnrealHeaderTool {0}", CmdLine);

					Stopwatch s = new Stopwatch();
					s.Start();
					ITimelineEvent Timer = Timeline.ScopeEvent("Executing UnrealHeaderTool");
					CompilationResult UHTResult = (CompilationResult)RunExternalNativeExecutable(ExternalExecution.GetHeaderToolPath(HeaderToolReceipt), CmdLine);
					Timer.Finish();
					s.Stop();

					if (UHTResult != CompilationResult.Succeeded)
					{
						// On Linux and Mac, the shell will return 128+signal number exit codes if UHT gets a signal (e.g. crashes or is interrupted)
						if ((BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux ||
							BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac) &&
							(int)(UHTResult) >= 128
							)
						{
							// SIGINT is 2, so 128 + SIGINT is 130
							UHTResult = ((int)(UHTResult) == 130) ? CompilationResult.Canceled : CompilationResult.CrashOrAssert;
						}

						if ((BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32 || 
							BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64) && 
							(int)(UHTResult) < 0)
						{
							Log.TraceError(String.Format("UnrealHeaderTool failed with exit code 0x{0:X} - check that UE4 prerequisites are installed.", (int)UHTResult));
						}

						throw new CompilationResultException(UHTResult);
					}

					Log.TraceInformation("Reflection code generated for {0} in {1} seconds", ActualTargetName, s.Elapsed.TotalSeconds);

					// Now that UHT has successfully finished generating code, we need to update all cached FileItems in case their last write time has changed.
					// Otherwise UBT might not detect changes UHT made.
					using(Timeline.ScopeEvent("ExternalExecution.ResetCachedHeaderInfo()"))
					{
						ResetCachedHeaderInfo(UObjectModules);
					}
				}
				else
				{
					Log.TraceVerbose("Generated code is up to date.");
				}

				Progress.Write(2, 3);

				// touch the directories
				using(Timeline.ScopeEvent("ExternalExecution.UpdateDirectoryTimestamps()"))
				{
					UpdateDirectoryTimestamps(UObjectModules);
				}

				Progress.Write(3, 3);
			}
			if (ProgressWriter.bWriteMarkup)
			{
				Log.WriteLine(LogEventType.Console, "@progress pop");
			}
		}

		static void ResetCachedHeaderInfo(List<UHTModuleInfo> UObjectModules)
		{
			foreach(UHTModuleInfo ModuleInfo in UObjectModules)
			{
				ModuleInfo.GeneratedCodeDirectory.ResetCachedInfo();
			}
		}
	}
}
