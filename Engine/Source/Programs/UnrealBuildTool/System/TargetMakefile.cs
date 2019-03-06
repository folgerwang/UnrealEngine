// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Formatters.Binary;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Cached list of actions that need to be executed to build a target, along with the information needed to determine whether they are valid.
	/// </summary>
	class TargetMakefile
	{
		/// <summary>
		/// The version number to write
		/// </summary>
		public const int CurrentVersion = 11;

		/// <summary>
		/// The time at which the makefile was created
		/// </summary>
		public DateTime CreateTimeUtc;

		/// <summary>
		/// Information about the toolchain used to build. This string will be output before building.
		/// </summary>
		public string ToolchainInfo;

		/// <summary>
		/// Any additional information about the build environment which the platform can use to invalidate the makefile
		/// </summary>
		public string ExternalMetadata;

		/// <summary>
		/// Path to the receipt file for this target
		/// </summary>
		public FileReference ReceiptFile;

		/// <summary>
		/// The project intermediate directory
		/// </summary>
		public DirectoryReference ProjectIntermediateDirectory;

		/// <summary>
		/// Type of the target
		/// </summary>
		public TargetType TargetType;

		/// <summary>
		/// Whether the target should be deployed after being built
		/// </summary>
		public bool bDeployAfterCompile;

		/// <summary>
		/// Whether the project has a script plugin. UHT needs to know this to detect which manifest to use for checking out-of-datedness.
		/// </summary>
		public bool bHasProjectScriptPlugin;

		/// <summary>
		/// The array of command-line arguments. The makefile will be invalidated whenever these change.
		/// </summary>
		public string[] AdditionalArguments;

		/// <summary>
		/// Scripts which should be run before building anything
		/// </summary>
		public FileReference[] PreBuildScripts;

		/// <summary>
		/// Every action in the action graph
		/// </summary>
		public List<Action> Actions;

		/// <summary>
		/// Environment variables that we'll need in order to invoke the platform's compiler and linker
		/// </summary>
		// @todo ubtmake: Really we want to allow a different set of environment variables for every Action.  This would allow for targets on multiple platforms to be built in a single assembling phase.  We'd only have unique variables for each platform that has actions, so we'd want to make sure we only store the minimum set.
		public readonly List<Tuple<string, string>> EnvironmentVariables = new List<Tuple<string, string>>();

		/// <summary>
		/// The final output items for all target
		/// </summary>
		public List<FileItem> OutputItems;

		/// <summary>
		/// Maps module names to output items
		/// </summary>
		public Dictionary<string, FileItem[]> ModuleNameToOutputItems;

		/// <summary>
		/// List of game module names, for hot-reload
		/// </summary>
		public HashSet<string> HotReloadModuleNames;

		/// <summary>
		/// Set of all source directories. Any files being added or removed from these directories will invalidate the makefile.
		/// </summary>
		public Dictionary<DirectoryItem, FileItem[]> DirectoryToSourceFiles;

		/// <summary>
		/// The set of source files that UnrealBuildTool determined to be part of the programmer's "working set". Used for adaptive non-unity builds.
		/// </summary>
		public HashSet<FileItem> WorkingSet = new HashSet<FileItem>();

		/// <summary>
		/// Set of files which are currently not part of the working set, but could be.
		/// </summary>
		public HashSet<FileItem> CandidatesForWorkingSet = new HashSet<FileItem>();

		/// <summary>
		/// Maps each target to a list of UObject module info structures
		/// </summary>
		public List<UHTModuleInfo> UObjectModules;

		/// <summary>
		/// Used to map names of modules to their .Build.cs filename
		/// </summary>
		public List<UHTModuleHeaderInfo> UObjectModuleHeaders = new List<UHTModuleHeaderInfo>();

		/// <summary>
		/// List of all plugin names. The makefile will be considered invalid if any of these changes, or new plugins are added.
		/// </summary>
		public HashSet<FileItem> PluginFiles;

		/// <summary>
		/// Additional files which are required 
		/// </summary>
		public HashSet<FileItem> AdditionalDependencies = new HashSet<FileItem>(); 

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ToolchainInfo">String describing the toolchain used to build. This will be output before executing actions.</param>
		/// <param name="ExternalMetadata">External build metadata from the platform</param>
		/// <param name="ReceiptFile">Path to the receipt file</param>
		/// <param name="ProjectIntermediateDirectory">Path to the project intermediate directory</param>
		/// <param name="TargetType">The type of target</param>
		/// <param name="bDeployAfterCompile">Whether to deploy the target after compiling</param>
		/// <param name="bHasProjectScriptPlugin">Whether the target has a project script plugin</param>
		public TargetMakefile(string ToolchainInfo, string ExternalMetadata, FileReference ReceiptFile, DirectoryReference ProjectIntermediateDirectory, TargetType TargetType, bool bDeployAfterCompile, bool bHasProjectScriptPlugin)
		{
			this.CreateTimeUtc = DateTime.UtcNow;
			this.ToolchainInfo = ToolchainInfo;
			this.ExternalMetadata = ExternalMetadata;
			this.ReceiptFile = ReceiptFile;
			this.ProjectIntermediateDirectory = ProjectIntermediateDirectory;
			this.TargetType = TargetType;
			this.bDeployAfterCompile = bDeployAfterCompile;
			this.bHasProjectScriptPlugin = bHasProjectScriptPlugin;
			this.Actions = new List<Action>();
			this.OutputItems = new List<FileItem>();
			this.ModuleNameToOutputItems = new Dictionary<string, FileItem[]>(StringComparer.OrdinalIgnoreCase);
			this.HotReloadModuleNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			this.DirectoryToSourceFiles = new Dictionary<DirectoryItem, FileItem[]>();
			this.WorkingSet = new HashSet<FileItem>();
			this.CandidatesForWorkingSet = new HashSet<FileItem>();
			this.UObjectModules = new List<UHTModuleInfo>();
			this.UObjectModuleHeaders = new List<UHTModuleHeaderInfo>();
			this.PluginFiles = new HashSet<FileItem>();
			this.AdditionalDependencies = new HashSet<FileItem>();
		}

		/// <summary>
		/// Constructor. Reads a makefile from disk.
		/// </summary>
		/// <param name="Reader">The archive to read from</param>
		public TargetMakefile(BinaryArchiveReader Reader)
		{
			CreateTimeUtc = new DateTime(Reader.ReadLong(), DateTimeKind.Utc);
			ToolchainInfo = Reader.ReadString();
			ExternalMetadata = Reader.ReadString();
			ReceiptFile = Reader.ReadFileReference();
			ProjectIntermediateDirectory = Reader.ReadDirectoryReference();
			TargetType = (TargetType)Reader.ReadInt();
			bDeployAfterCompile = Reader.ReadBool();
			bHasProjectScriptPlugin = Reader.ReadBool();
			AdditionalArguments = Reader.ReadArray(() => Reader.ReadString());
			PreBuildScripts = Reader.ReadArray(() => Reader.ReadFileReference());
			Actions = Reader.ReadList(() => new Action(Reader));
			EnvironmentVariables = Reader.ReadList(() => Tuple.Create(Reader.ReadString(), Reader.ReadString()));
			OutputItems = Reader.ReadList(() => Reader.ReadFileItem());
			ModuleNameToOutputItems = Reader.ReadDictionary(() => Reader.ReadString(), () => Reader.ReadArray(() => Reader.ReadFileItem()), StringComparer.OrdinalIgnoreCase);
			HotReloadModuleNames = Reader.ReadHashSet(() => Reader.ReadString(), StringComparer.OrdinalIgnoreCase);
			DirectoryToSourceFiles = Reader.ReadDictionary(() => Reader.ReadDirectoryItem(), () => Reader.ReadArray(() => Reader.ReadFileItem()));
			WorkingSet = Reader.ReadHashSet(() => Reader.ReadFileItem());
			CandidatesForWorkingSet = Reader.ReadHashSet(() => Reader.ReadFileItem());
			UObjectModules = Reader.ReadList(() => new UHTModuleInfo(Reader));
			UObjectModuleHeaders = Reader.ReadList(() => new UHTModuleHeaderInfo(Reader));
			PluginFiles = Reader.ReadHashSet(() => Reader.ReadFileItem());
			AdditionalDependencies = Reader.ReadHashSet(() => Reader.ReadFileItem());
		}

		/// <summary>
		/// Write the makefile to the given archive
		/// </summary>
		/// <param name="Writer">The archive to write to</param>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteLong(CreateTimeUtc.Ticks);
			Writer.WriteString(ToolchainInfo);
			Writer.WriteString(ExternalMetadata);
			Writer.WriteFileReference(ReceiptFile);
			Writer.WriteDirectoryReference(ProjectIntermediateDirectory);
			Writer.WriteInt((int)TargetType);
			Writer.WriteBool(bDeployAfterCompile);
			Writer.WriteBool(bHasProjectScriptPlugin);
			Writer.WriteArray(AdditionalArguments, Item => Writer.WriteString(Item));
			Writer.WriteArray(PreBuildScripts, Item => Writer.WriteFileReference(Item));
			Writer.WriteList(Actions, Action => Action.Write(Writer));
			Writer.WriteList(EnvironmentVariables, x => { Writer.WriteString(x.Item1); Writer.WriteString(x.Item2); });
			Writer.WriteList(OutputItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteDictionary(ModuleNameToOutputItems, k => Writer.WriteString(k), v => Writer.WriteArray(v, e => Writer.WriteFileItem(e)));
			Writer.WriteHashSet(HotReloadModuleNames, x => Writer.WriteString(x));
			Writer.WriteDictionary(DirectoryToSourceFiles, k => Writer.WriteDirectoryItem(k), v => Writer.WriteArray(v, e => Writer.WriteFileItem(e)));
			Writer.WriteHashSet(WorkingSet, x => Writer.WriteFileItem(x));
			Writer.WriteHashSet(CandidatesForWorkingSet, x => Writer.WriteFileItem(x));
			Writer.WriteList(UObjectModules, e => e.Write(Writer));
			Writer.WriteList(UObjectModuleHeaders, x => x.Write(Writer));
			Writer.WriteHashSet(PluginFiles, x => Writer.WriteFileItem(x));
			Writer.WriteHashSet(AdditionalDependencies, x => Writer.WriteFileItem(x));
		}

		/// <summary>
		/// Saves a makefile to disk
		/// </summary>
		/// <param name="Location">Path to save the makefile to</param>
		public void Save(FileReference Location)
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using(BinaryArchiveWriter Writer = new BinaryArchiveWriter(Location))
			{
				Writer.WriteInt(CurrentVersion);
				Write(Writer);
			}
		}

		/// <summary>
		/// Loads a makefile  from disk
		/// </summary>
		/// <param name="MakefilePath">Path to the makefile to load</param>
		/// <param name="ProjectFile">Path to the project file</param>
		/// <param name="Platform">Platform for this makefile</param>
		/// <param name="Arguments">Command line arguments for this target</param>
		/// <param name="ReasonNotLoaded">If the function returns null, this string will contain the reason why</param>
		/// <returns>The loaded makefile, or null if it failed for some reason.  On failure, the 'ReasonNotLoaded' variable will contain information about why</returns>
		public static TargetMakefile Load(FileReference MakefilePath, FileReference ProjectFile, UnrealTargetPlatform Platform, string[] Arguments, out string ReasonNotLoaded)
		{
			using(Timeline.ScopeEvent("Checking dependent timestamps"))
			{
				// Check the directory timestamp on the project files directory.  If the user has generated project files more recently than the makefile, then we need to consider the file to be out of date
				FileInfo MakefileInfo = new FileInfo(MakefilePath.FullName);
				if (!MakefileInfo.Exists)
				{
					// Makefile doesn't even exist, so we won't bother loading it
					ReasonNotLoaded = "no existing makefile";
					return null;
				}

				// Check the build version
				FileInfo BuildVersionFileInfo = new FileInfo(BuildVersion.GetDefaultFileName().FullName);
				if (BuildVersionFileInfo.Exists && MakefileInfo.LastWriteTime.CompareTo(BuildVersionFileInfo.LastWriteTime) < 0)
				{
					Log.TraceLog("Existing makefile is older than Build.version, ignoring it");
					ReasonNotLoaded = "Build.version is newer";
					return null;
				}

				// @todo ubtmake: This will only work if the directory timestamp actually changes with every single GPF.  Force delete existing files before creating new ones?  Eh... really we probably just want to delete + create a file in that folder
				//			-> UPDATE: Seems to work OK right now though on Windows platform, maybe due to GUID changes
				// @todo ubtmake: Some platforms may not save any files into this folder.  We should delete + generate a "touch" file to force the directory timestamp to be updated (or just check the timestamp file itself.  We could put it ANYWHERE, actually)

				// Installed Build doesn't need to check engine projects for outdatedness
				if (!UnrealBuildTool.IsEngineInstalled())
				{
					if (DirectoryReference.Exists(ProjectFileGenerator.IntermediateProjectFilesPath))
					{
						DateTime EngineProjectFilesLastUpdateTime = new FileInfo(ProjectFileGenerator.ProjectTimestampFile).LastWriteTime;
						if (MakefileInfo.LastWriteTime.CompareTo(EngineProjectFilesLastUpdateTime) < 0)
						{
							// Engine project files are newer than makefile
							Log.TraceLog("Existing makefile is older than generated engine project files, ignoring it");
							ReasonNotLoaded = "project files are newer";
							return null;
						}
					}
				}

				// Check the game project directory too
				if (ProjectFile != null)
				{
					string ProjectFilename = ProjectFile.FullName;
					FileInfo ProjectFileInfo = new FileInfo(ProjectFilename);
					if (!ProjectFileInfo.Exists || MakefileInfo.LastWriteTime.CompareTo(ProjectFileInfo.LastWriteTime) < 0)
					{
						// .uproject file is newer than makefile
						Log.TraceLog("Makefile is older than .uproject file, ignoring it");
						ReasonNotLoaded = ".uproject file is newer";
						return null;
					}

					DirectoryReference MasterProjectRelativePath = ProjectFile.Directory;
					string GameIntermediateProjectFilesPath = Path.Combine(MasterProjectRelativePath.FullName, "Intermediate", "ProjectFiles");
					if (Directory.Exists(GameIntermediateProjectFilesPath))
					{
						DateTime GameProjectFilesLastUpdateTime = new DirectoryInfo(GameIntermediateProjectFilesPath).LastWriteTime;
						if (MakefileInfo.LastWriteTime.CompareTo(GameProjectFilesLastUpdateTime) < 0)
						{
							// Game project files are newer than makefile
							Log.TraceLog("Makefile is older than generated game project files, ignoring it");
							ReasonNotLoaded = "game project files are newer";
							return null;
						}
					}
				}

				// Check to see if UnrealBuildTool.exe was compiled more recently than the makefile
				DateTime UnrealBuildToolTimestamp = new FileInfo(Assembly.GetExecutingAssembly().Location).LastWriteTime;
				if (MakefileInfo.LastWriteTime.CompareTo(UnrealBuildToolTimestamp) < 0)
				{
					// UnrealBuildTool.exe was compiled more recently than the makefile
					Log.TraceLog("Makefile is older than UnrealBuildTool.exe, ignoring it");
					ReasonNotLoaded = "UnrealBuildTool.exe is newer";
					return null;
				}

				// Check to see if any BuildConfiguration files have changed since the last build
				List<XmlConfig.InputFile> InputFiles = XmlConfig.FindInputFiles();
				foreach (XmlConfig.InputFile InputFile in InputFiles)
				{
					FileInfo InputFileInfo = new FileInfo(InputFile.Location.FullName);
					if (InputFileInfo.LastWriteTime > MakefileInfo.LastWriteTime)
					{
						Log.TraceLog("Makefile is older than BuildConfiguration.xml, ignoring it");
						ReasonNotLoaded = "BuildConfiguration.xml is newer";
						return null;
					}
				}
			}

			TargetMakefile Makefile;
			using(Timeline.ScopeEvent("Loading makefile"))
			{
				try
				{
					using(BinaryArchiveReader Reader = new BinaryArchiveReader(MakefilePath))
					{
						int Version = Reader.ReadInt();
						if(Version != CurrentVersion)
						{
							ReasonNotLoaded = "makefile version does not match";
							return null;
						}
						Makefile = new TargetMakefile(Reader);
					}
				}
				catch (Exception Ex)
				{
					Log.TraceWarning("Failed to read makefile: {0}", Ex.Message);
					Log.TraceLog("Exception: {0}", Ex.ToString());
					ReasonNotLoaded = "couldn't read existing makefile";
					return null;
				}
			}

			using(Timeline.ScopeEvent("Checking makefile validity"))
			{
				// Check if the arguments are different
				if(!Enumerable.SequenceEqual(Makefile.AdditionalArguments, Arguments))
				{
					ReasonNotLoaded = "command line arguments changed";
					return null;
				}

				// Check if ini files are newer. Ini files contain build settings too.
				DirectoryReference ProjectDirectory = DirectoryReference.FromFile(ProjectFile);
				foreach (ConfigHierarchyType IniType in (ConfigHierarchyType[])Enum.GetValues(typeof(ConfigHierarchyType)))
				{
					foreach (FileReference IniFilename in ConfigHierarchy.EnumerateConfigFileLocations(IniType, ProjectDirectory, Platform))
					{
						FileInfo IniFileInfo = new FileInfo(IniFilename.FullName);
						if(IniFileInfo.LastWriteTimeUtc > Makefile.CreateTimeUtc)
						{
							// Ini files are newer than makefile
							ReasonNotLoaded = "ini files are newer than makefile";
							return null;
						}
					}
				}

				// Get the current build metadata from the platform
				string CurrentExternalMetadata = UEBuildPlatform.GetBuildPlatform(Platform).GetExternalBuildMetadata(ProjectFile);
				if(String.Compare(CurrentExternalMetadata, Makefile.ExternalMetadata, StringComparison.Ordinal) != 0)
				{
					Log.TraceLog("Old metadata:\n", Makefile.ExternalMetadata);
					Log.TraceLog("New metadata:\n", CurrentExternalMetadata);
					ReasonNotLoaded = "build metadata has changed";
					return null;
				}
			}

			// The makefile is ok
			ReasonNotLoaded = null;
			return Makefile;
		}

		/// <summary>
		/// Checks if the makefile is valid for the current set of source files. This is done separately to the Load() method to allow pre-build steps to modify source files.
		/// </summary>
		/// <param name="Makefile">The makefile that has been loaded</param>
		/// <param name="ProjectFile">Path to the project file</param>
		/// <param name="WorkingSet">The current working set of source files</param>
		/// <param name="ReasonNotLoaded">If the makefile is not valid, is set to a message describing why</param>
		/// <returns>True if the makefile is valid, false otherwise</returns>
		public static bool IsValidForSourceFiles(TargetMakefile Makefile, FileReference ProjectFile, ISourceFileWorkingSet WorkingSet, out string ReasonNotLoaded)
		{
			using(Timeline.ScopeEvent("TargetMakefile.IsValidForSourceFiles()"))
			{
				// Check if any source files have been added or removed
				foreach(KeyValuePair<DirectoryItem, FileItem[]> Pair in Makefile.DirectoryToSourceFiles)
				{
					DirectoryItem InputDirectory = Pair.Key;
					if(!InputDirectory.Exists || InputDirectory.LastWriteTimeUtc > Makefile.CreateTimeUtc)
					{
						FileItem[] SourceFiles = UEBuildModuleCPP.GetSourceFiles(InputDirectory);
						if(SourceFiles.Length < Pair.Value.Length)
						{
							ReasonNotLoaded = "source file removed";
							return false;
						}
						else if(SourceFiles.Length > Pair.Value.Length)
						{
							ReasonNotLoaded = "source file added";
							return false;
						}
						else if(SourceFiles.Intersect(Pair.Value).Count() != SourceFiles.Length)
						{
							ReasonNotLoaded = "source file modified";
							return false;
						}
					}
				}

				// Check if any of the additional dependencies has changed
				foreach(FileItem AdditionalDependency in Makefile.AdditionalDependencies)
				{
					if (!AdditionalDependency.Exists)
					{
						Log.TraceLog("{0} has been deleted since makefile was built.", AdditionalDependency.Location);
						ReasonNotLoaded = string.Format("{0} deleted", AdditionalDependency.Location.GetFileName());
						return false;
					}
					if(AdditionalDependency.LastWriteTimeUtc > Makefile.CreateTimeUtc)
					{
						Log.TraceLog("{0} has been modified since makefile was built.", AdditionalDependency.Location);
						ReasonNotLoaded = string.Format("{0} modified", AdditionalDependency.Location.GetFileName());
						return false;
					}
				}

				// Check that no new plugins have been added
				foreach(FileReference PluginFile in Plugins.EnumeratePlugins(ProjectFile))
				{
					FileItem PluginFileItem = FileItem.GetItemByFileReference(PluginFile);
					if(!Makefile.PluginFiles.Contains(PluginFileItem))
					{
						Log.TraceLog("{0} has been added", PluginFile.GetFileName());
						ReasonNotLoaded = string.Format("{0} has been added", PluginFile.GetFileName());
						return false;
					}
				}

				// We do a check to see if any modules' headers have changed which have
				// acquired or lost UHT types.  If so, which should be rare,
				// we'll just invalidate the entire makefile and force it to be rebuilt.

				// Get all H files in processed modules newer than the makefile itself
				HashSet<FileItem> HFilesNewerThanMakefile = new HashSet<FileItem>();
				foreach(UHTModuleHeaderInfo ModuleHeaderInfo in Makefile.UObjectModuleHeaders)
				{
					foreach(FileItem HeaderFile in ModuleHeaderInfo.SourceFolder.EnumerateFiles())
					{
						if(HeaderFile.HasExtension(".h") && HeaderFile.LastWriteTimeUtc > Makefile.CreateTimeUtc)
						{
							HFilesNewerThanMakefile.Add(HeaderFile);
						}
					}
				}

				// Get all H files in all modules processed in the last makefile build
				HashSet<FileItem> AllUHTHeaders = new HashSet<FileItem>(Makefile.UObjectModuleHeaders.SelectMany(x => x.HeaderFiles));

				// Check whether any headers have been deleted. If they have, we need to regenerate the makefile since the module might now be empty. If we don't,
				// and the file has been moved to a different module, we may include stale generated headers.
				foreach (FileItem HeaderFile in AllUHTHeaders)
				{
					if (!HeaderFile.Exists)
					{
						Log.TraceLog("File processed by UHT was deleted ({0}); invalidating makefile", HeaderFile);
						ReasonNotLoaded = string.Format("UHT file was deleted");
						return false;
					}
				}

				// Makefile is invalid if:
				// * There are any newer files which contain no UHT data, but were previously in the makefile
				// * There are any newer files contain data which needs processing by UHT, but weren't not previously in the makefile
				SourceFileMetadataCache MetadataCache = SourceFileMetadataCache.CreateHierarchy(ProjectFile);
				foreach (FileItem HeaderFile in HFilesNewerThanMakefile)
				{
					bool bContainsUHTData = MetadataCache.ContainsReflectionMarkup(HeaderFile);
					bool bWasProcessed = AllUHTHeaders.Contains(HeaderFile);
					if (bContainsUHTData != bWasProcessed)
					{
						Log.TraceLog("{0} {1} contain UHT types and now {2} , ignoring it", HeaderFile, bWasProcessed ? "used to" : "didn't", bWasProcessed ? "doesn't" : "does");
						ReasonNotLoaded = string.Format("new files with reflected types");
						return false;
					}
				}

				// If adaptive unity build is enabled, do a check to see if there are any source files that became part of the
				// working set since the Makefile was created (or, source files were removed from the working set.)  If anything
				// changed, then we'll force a new Makefile to be created so that we have fresh unity build blobs.  We always
				// want to make sure that source files in the working set are excluded from those unity blobs (for fastest possible
				// iteration times.)

				// Check if any source files in the working set no longer belong in it
				foreach (FileItem SourceFile in Makefile.WorkingSet)
				{
					if (!WorkingSet.Contains(SourceFile) && SourceFile.LastWriteTimeUtc > Makefile.CreateTimeUtc)
					{
						Log.TraceLog("{0} was part of source working set and now is not; invalidating makefile", SourceFile.AbsolutePath);
						ReasonNotLoaded = string.Format("working set of source files changed");
						return false;
					}
				}

				// Check if any source files that are eligible for being in the working set have been modified
				foreach (FileItem SourceFile in Makefile.CandidatesForWorkingSet)
				{
					if (WorkingSet.Contains(SourceFile) && SourceFile.LastWriteTimeUtc > Makefile.CreateTimeUtc)
					{
						Log.TraceLog("{0} was part of source working set and now is not", SourceFile.AbsolutePath);
						ReasonNotLoaded = string.Format("working set of source files changed");
						return false;
					}
				}
			}

			ReasonNotLoaded = null;
			return true;
		}

		/// <summary>
		/// Gets the location of the makefile for particular target
		/// </summary>
		/// <param name="ProjectFile">Project file for the build</param>
		/// <param name="TargetName">Name of the target being built</param>
		/// <param name="Platform">The platform that the target is being built for</param>
		/// <param name="Configuration">The configuration being built</param>
		/// <returns>Path to the makefile</returns>
		public static FileReference GetLocation(FileReference ProjectFile, string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration)
		{
			DirectoryReference BaseDirectory = DirectoryReference.FromFile(ProjectFile) ?? UnrealBuildTool.EngineDirectory;
			return FileReference.Combine(BaseDirectory, "Intermediate", "Build", Platform.ToString(), TargetName, Configuration.ToString(), "Makefile.bin");
		}
	}
}
