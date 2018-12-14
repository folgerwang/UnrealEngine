// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool
{
	[Help("Rewrites include directives for headers in public include paths to make them relative to the 'Public' folder.")]
	[Help("-Project", "Specifies a project to include in the scan for source files. May be specified multiple times.")]
	[Help("-UpdateDir", "Specifies the directory containing files to update. This may be a project/engine directory, or a subfolder.")]
	[Help("-Write", "If set, causes the modified files to be written. Files will be checked out from Perforce if possible.")]
	class RebasePublicIncludePaths : BuildCommand
	{
		public override void ExecuteBuild()
		{
			string[] ProjectParams = ParseParamValues("Project");

			string UpdateDirParam = ParseParamValue("UpdateDir", null);
			if(UpdateDirParam == null)
			{
				throw new AutomationException("Missing -UpdateDir=... parameter");
			}
			DirectoryReference UpdateDir = new DirectoryReference(UpdateDirParam);

			bool bWrite = ParseParam("Write");

			// Get all the root dirs
			HashSet<DirectoryReference> RootDirs = new HashSet<DirectoryReference>();
			RootDirs.Add(EngineDirectory);

			// Add the enterprise edirectory
			DirectoryReference EnterpriseDirectory = DirectoryReference.Combine(RootDirectory, "Enterprise");
			if(DirectoryReference.Exists(EnterpriseDirectory))
			{
				RootDirs.Add(EnterpriseDirectory);
			}

			// Add the project directories
			foreach(string ProjectParam in ProjectParams)
			{
				FileReference ProjectLocation = new FileReference(ProjectParam);
				if(!FileReference.Exists(ProjectLocation))
				{
					throw new AutomationException("Unable to find project '{0}'", ProjectLocation);
				}
				RootDirs.Add(ProjectLocation.Directory);
			}

			// Find all the modules
			HashSet<DirectoryReference> ModuleDirs = new HashSet<DirectoryReference>();
			foreach(DirectoryReference RootDir in RootDirs)
			{
				// Find all the modules from the source folder
				DirectoryReference SourceDir = DirectoryReference.Combine(RootDir, "Source");
				if(DirectoryReference.Exists(SourceDir))
				{
					foreach(FileReference ModuleFile in DirectoryReference.EnumerateFiles(SourceDir, "*.Build.cs", SearchOption.AllDirectories))
					{
						ModuleDirs.Add(ModuleFile.Directory);
					}
				}

				// Find all the modules under the plugins folder
				DirectoryReference PluginsDir = DirectoryReference.Combine(RootDir, "Plugins");
				foreach(FileReference PluginFile in DirectoryReference.EnumerateFiles(PluginsDir, "*.uplugin", SearchOption.AllDirectories))
				{
					DirectoryReference PluginSourceDir = DirectoryReference.Combine(PluginFile.Directory, "Source");
					if(DirectoryReference.Exists(PluginSourceDir))
					{
						foreach(FileReference PluginModuleFile in DirectoryReference.EnumerateFiles(PluginSourceDir, "*.Build.cs", SearchOption.AllDirectories))
						{
							ModuleDirs.Add(PluginModuleFile.Directory);
						}
					}
				}
			}

			// Find a mapping from old to new include paths
			Dictionary<string, Tuple<string, FileReference>> RemapIncludePaths = new Dictionary<string, Tuple<string, FileReference>>(StringComparer.InvariantCultureIgnoreCase);
			foreach(DirectoryReference ModuleDir in ModuleDirs)
			{
				DirectoryReference ModulePublicDir = DirectoryReference.Combine(ModuleDir, "Public");
				if(DirectoryReference.Exists(ModulePublicDir))
				{
					foreach(FileReference HeaderFile in DirectoryReference.EnumerateFiles(ModulePublicDir, "*.h", SearchOption.AllDirectories))
					{
						string BaseIncludeFile = HeaderFile.GetFileName();

						Tuple<string, FileReference> ExistingIncludeName;
						if(RemapIncludePaths.TryGetValue(BaseIncludeFile, out ExistingIncludeName))
						{
							LogWarning("Multiple include paths for {0}: {1}, {2}", BaseIncludeFile, ExistingIncludeName.Item2, HeaderFile);
						}
						else
						{
							RemapIncludePaths.Add(BaseIncludeFile, Tuple.Create(HeaderFile.MakeRelativeTo(ModulePublicDir).Replace('\\', '/'), HeaderFile));
						}
					}
				}
			}

			// List of folders to exclude from updates
			string[] ExcludeFoldersFromUpdate =
			{
				"Intermediate",
				"ThirdParty"
			};
			
			// Enumerate all the files to update
			HashSet<FileReference> UpdateFiles = new HashSet<FileReference>();
			foreach(FileReference UpdateFile in DirectoryReference.EnumerateFiles(UpdateDir, "*", SearchOption.AllDirectories))
			{
				if(!UpdateFile.ContainsAnyNames(ExcludeFoldersFromUpdate, UpdateDir))
				{
					if(UpdateFile.HasExtension(".cpp") | UpdateFile.HasExtension(".h") || UpdateFile.HasExtension(".inl"))
					{
						UpdateFiles.Add(UpdateFile);
					}
				}
			}

			// Process all the source files
			Dictionary<FileReference, string[]> ModifiedFiles = new Dictionary<FileReference, string[]>();
			foreach(FileReference UpdateFile in UpdateFiles)
			{
				bool bModifiedFile = false;

				string[] Lines = FileReference.ReadAllLines(UpdateFile);
				for(int Idx = 0; Idx < Lines.Length; Idx++)
				{
					Match Match = Regex.Match(Lines[Idx], "^(\\s*#\\s*include\\s+\\\")([^\"]+)(\\\".*)$");
					if(Match.Success)
					{
						string IncludePath = Match.Groups[2].Value;

						Tuple<string, FileReference> NewIncludePath;
						if(RemapIncludePaths.TryGetValue(IncludePath, out NewIncludePath))
						{
							if(IncludePath != NewIncludePath.Item1)
							{
//								Log("{0}: Changing {1} -> {2}", UpdateFile, IncludePath, NewIncludePath.Item1);
								Lines[Idx] = String.Format("{0}{1}{2}", Match.Groups[1].Value, NewIncludePath.Item1, Match.Groups[3].Value);
								bModifiedFile = true;
							}
						}
					}
				}
				if(bModifiedFile)
				{
					ModifiedFiles.Add(UpdateFile, Lines);
				}
			}

			// Output them all to disk
			if(bWrite && ModifiedFiles.Count > 0)
			{
				LogInformation("Updating {0} files...", ModifiedFiles.Count);

				List<FileReference> FilesToCheckOut = new List<FileReference>();
				foreach(FileReference ModifiedFile in ModifiedFiles.Keys)
				{
					if((FileReference.GetAttributes(ModifiedFile) & FileAttributes.ReadOnly) != 0)
					{
						FilesToCheckOut.Add(ModifiedFile);
					}
				}

				if (FilesToCheckOut.Count > 0)
				{
					if(!P4Enabled)
					{
						throw new AutomationException("{0} files have been modified, but are read only. Run with -P4 to enable Perforce checkout.\n{1}", FilesToCheckOut.Count, String.Join("\n", FilesToCheckOut.Select(x => "  " + x)));
					}

					LogInformation("Checking out files from Perforce");

					int ChangeNumber = P4.CreateChange(Description: "Updating source files");
					P4.Edit(ChangeNumber, FilesToCheckOut.Select(x => x.FullName).ToList(), false);
				}

				foreach(KeyValuePair<FileReference, string[]> FileToWrite in ModifiedFiles)
				{
					LogInformation("Writing {0}", FileToWrite.Key);
					FileReference.WriteAllLines(FileToWrite.Key, FileToWrite.Value);
				}
			}
		}
	}
}
