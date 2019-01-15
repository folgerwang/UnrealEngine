// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	/// Cleans build products and intermediates for the target. This deletes files which are named consistently with the target being built
	/// (e.g. UE4Editor-Foo-Win64-Debug.dll) rather than an actual record of previous build products.
	/// </summary>
	[ToolMode("Clean", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance)]
	class CleanMode : ToolMode
	{
		/// <summary>
		/// Whether to avoid cleaning targets
		/// </summary>
		[CommandLine("-SkipRulesCompile")]
		bool bSkipRulesCompile = false;

		/// <summary>
		/// Main entry point
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <returns>One of the values of ECompilationResult</returns>
		public override int Execute(CommandLineArguments Arguments)
		{
			Arguments.ApplyTo(this);

			// Create the build configuration object, and read the settings
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			XmlConfig.ApplyTo(BuildConfiguration);
			Arguments.ApplyTo(BuildConfiguration);

			// Parse all the targets being built
			List<TargetDescriptor> TargetDescriptors = TargetDescriptor.ParseCommandLine(Arguments, BuildConfiguration.bUsePrecompiled, bSkipRulesCompile);
			if(TargetDescriptors.Count == 0)
			{
				throw new BuildException("No targets specified to clean");
			}

			// Also add implicit descriptors for cleaning UnrealBuildTool
			if(!BuildConfiguration.bDoNotBuildUHT)
			{
				const string UnrealHeaderToolTarget = "UnrealHeaderTool";

				// Get a list of project files to clean UHT for
				List<FileReference> ProjectFiles = new List<FileReference>();
				foreach(TargetDescriptor TargetDesc in TargetDescriptors)
				{
					if(TargetDesc.Name != UnrealHeaderToolTarget && !RemoteMac.HandlesTargetPlatform(TargetDesc.Platform))
					{
						if(ProjectFiles.Count == 0)
						{
							ProjectFiles.Add(null);
						}
						if(TargetDesc.ProjectFile != null && !ProjectFiles.Contains(TargetDesc.ProjectFile))
						{
							ProjectFiles.Add(TargetDesc.ProjectFile);
						}
					}
				}

				// Add descriptors for cleaning UHT with all these projects
				if(ProjectFiles.Count > 0)
				{
					UnrealTargetConfiguration Configuration = BuildConfiguration.bForceDebugUnrealHeaderTool ? UnrealTargetConfiguration.Debug : UnrealTargetConfiguration.Development;
					string Architecture = UEBuildPlatform.GetBuildPlatform(BuildHostPlatform.Current.Platform).GetDefaultArchitecture(null);
					foreach(FileReference ProjectFile in ProjectFiles)
					{
						TargetDescriptors.Add(new TargetDescriptor(ProjectFile, UnrealHeaderToolTarget, BuildHostPlatform.Current.Platform, Configuration, Architecture, null));
					}
				}
			}

			// Output the list of targets that we're cleaning
			Log.TraceInformation("Cleaning {0} binaries...", StringUtils.FormatList(TargetDescriptors.Select(x => x.Name).Distinct()));

			// Loop through all the targets, and clean them all
			HashSet<FileReference> FilesToDelete = new HashSet<FileReference>();
			HashSet<DirectoryReference> DirectoriesToDelete = new HashSet<DirectoryReference>();
			foreach(TargetDescriptor TargetDescriptor in TargetDescriptors)
			{
				// Create the rules assembly
				RulesAssembly RulesAssembly = RulesCompiler.CreateTargetRulesAssembly(TargetDescriptor.ProjectFile, TargetDescriptor.Name, bSkipRulesCompile, BuildConfiguration.bUsePrecompiled, TargetDescriptor.ForeignPlugin);

				// Create the rules object
				ReadOnlyTargetRules Target = new ReadOnlyTargetRules(RulesAssembly.CreateTargetRules(TargetDescriptor.Name, TargetDescriptor.Platform, TargetDescriptor.Configuration, TargetDescriptor.Architecture, TargetDescriptor.ProjectFile, TargetDescriptor.AdditionalArguments));

				// Find the base folders that can contain binaries
				List<DirectoryReference> BaseDirs = new List<DirectoryReference>();
				BaseDirs.Add(UnrealBuildTool.EngineDirectory);
				BaseDirs.Add(UnrealBuildTool.EnterpriseDirectory);
				foreach (FileReference Plugin in Plugins.EnumeratePlugins(Target.ProjectFile))
				{
					BaseDirs.Add(Plugin.Directory);
				}
				if (Target.ProjectFile != null)
				{
					BaseDirs.Add(Target.ProjectFile.Directory);
				}

				// If we're running a precompiled build, remove anything under the engine folder
				BaseDirs.RemoveAll(x => RulesAssembly.IsReadOnly(x));

				// Get all the names which can prefix build products
				List<string> NamePrefixes = new List<string>();
				if (Target.Type != TargetType.Program)
				{
					NamePrefixes.Add(UEBuildTarget.GetAppNameForTargetType(Target.Type));
				}
				NamePrefixes.Add(Target.Name);

				// Get the suffixes for this configuration
				List<string> NameSuffixes = new List<string>();
				if (Target.Configuration == Target.UndecoratedConfiguration)
				{
					NameSuffixes.Add("");
				}
				NameSuffixes.Add(String.Format("-{0}-{1}", Target.Platform.ToString(), Target.Configuration.ToString()));
				if (!String.IsNullOrEmpty(Target.Architecture))
				{
					NameSuffixes.AddRange(NameSuffixes.ToArray().Select(x => x + Target.Architecture));
				}

				// Add all the makefiles and caches to be deleted
				FilesToDelete.Add(TargetMakefile.GetLocation(Target.ProjectFile, Target.Name, Target.Platform, Target.Configuration));
				FilesToDelete.UnionWith(SourceFileMetadataCache.GetFilesToClean(Target.ProjectFile));
				FilesToDelete.UnionWith(ActionHistory.GetFilesToClean(Target.ProjectFile, Target.Name, Target.Platform, Target.Type));

				// Add all the intermediate folders to be deleted
				foreach (DirectoryReference BaseDir in BaseDirs)
				{
					foreach (string NamePrefix in NamePrefixes)
					{
						DirectoryReference GeneratedCodeDir = DirectoryReference.Combine(BaseDir, "Intermediate", "Build", Target.Platform.ToString(), NamePrefix, "Inc");
						if (DirectoryReference.Exists(GeneratedCodeDir))
						{
							DirectoriesToDelete.Add(GeneratedCodeDir);
						}

						DirectoryReference IntermediateDir = DirectoryReference.Combine(BaseDir, "Intermediate", "Build", Target.Platform.ToString(), NamePrefix, Target.Configuration.ToString());
						if (DirectoryReference.Exists(IntermediateDir))
						{
							DirectoriesToDelete.Add(IntermediateDir);
						}
					}
				}

				// List of additional files and directories to clean, specified by the target platform
				List<FileReference> AdditionalFilesToDelete = new List<FileReference>();
				List<DirectoryReference> AdditionalDirectoriesToDelete = new List<DirectoryReference>();

				// Add all the build products from this target
				string[] NamePrefixesArray = NamePrefixes.Distinct().ToArray();
				string[] NameSuffixesArray = NameSuffixes.Distinct().ToArray();
				foreach (DirectoryReference BaseDir in BaseDirs)
				{
					DirectoryReference BinariesDir = DirectoryReference.Combine(BaseDir, "Binaries", Target.Platform.ToString());
					if(DirectoryReference.Exists(BinariesDir))
					{
						UEBuildPlatform.GetBuildPlatform(Target.Platform).FindBuildProductsToClean(BinariesDir, NamePrefixesArray, NameSuffixesArray, AdditionalFilesToDelete, AdditionalDirectoriesToDelete);
					}
				}

				// Get all the additional intermediate folders created by this platform
				UEBuildPlatform.GetBuildPlatform(Target.Platform).FindAdditionalBuildProductsToClean(Target, AdditionalFilesToDelete, AdditionalDirectoriesToDelete);

				// Add the platform's files and directories to the main list
				FilesToDelete.UnionWith(AdditionalFilesToDelete);
				DirectoriesToDelete.UnionWith(AdditionalDirectoriesToDelete);
			}

			// Delete all the directories, then all the files. By sorting the list of directories before we delete them, we avoid spamming the log if a parent directory is deleted first.
			foreach (DirectoryReference DirectoryToDelete in DirectoriesToDelete.OrderBy(x => x.FullName))
			{
				if (DirectoryReference.Exists(DirectoryToDelete))
				{
					Log.TraceVerbose("    Deleting {0}{1}...", DirectoryToDelete, Path.DirectorySeparatorChar);
					try
					{
						DirectoryReference.Delete(DirectoryToDelete, true);
					}
					catch (Exception Ex)
					{
						throw new BuildException(Ex, "Unable to delete {0} ({1})", DirectoryToDelete, Ex.Message.TrimEnd());
					}
				}
			}

			foreach (FileReference FileToDelete in FilesToDelete.OrderBy(x => x.FullName))
			{
				if (FileReference.Exists(FileToDelete))
				{
					Log.TraceVerbose("    Deleting " + FileToDelete);
					try
					{
						FileReference.Delete(FileToDelete);
					}
					catch (Exception Ex)
					{
						throw new BuildException(Ex, "Unable to delete {0} ({1})", FileToDelete, Ex.Message.TrimEnd());
					}
				}
			}

			// Also clean all the remote targets
			for(int Idx = 0; Idx < TargetDescriptors.Count; Idx++)
			{
				TargetDescriptor TargetDescriptor = TargetDescriptors[Idx];
				if(RemoteMac.HandlesTargetPlatform(TargetDescriptor.Platform))
				{
					RemoteMac RemoteMac = new RemoteMac(TargetDescriptor.ProjectFile);
					RemoteMac.Clean(TargetDescriptor);
				}
			}

			return 0;
		}
	}
}

