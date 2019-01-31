// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Text;
using System.IO;
using System.Security.Cryptography;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.Localization;
using Tools.DotNETCommon;

[Help("Updates the external localization data using the arguments provided.")]
[Help("UEProjectRoot", "Optional root-path to the project we're gathering for (defaults to CmdEnv.LocalRoot if unset).")]
[Help("UEProjectDirectory", "Sub-path to the project we're gathering for (relative to UEProjectRoot).")]
[Help("UEProjectName", "Optional name of the project we're gathering for (should match its .uproject file, eg QAGame).")]
[Help("LocalizationProjectNames", "Comma separated list of the projects to gather text from.")]
[Help("LocalizationBranch", "Optional suffix to use when uploading the new data to the localization provider.")]
[Help("LocalizationProvider", "Optional localization provide override.")]
[Help("LocalizationSteps", "Optional comma separated list of localization steps to perform [Download, Gather, Import, Export, Compile, GenerateReports, Upload] (default is all). Only valid for projects using a modular config.")]
[Help("IncludePlugins", "Optional flag to include plugins from within the given UEProjectDirectory as part of the gather. This may optionally specify a comma separated list of the specific plugins to gather (otherwise all plugins will be gathered).")]
[Help("ExcludePlugins", "Optional comma separated list of plugins to exclude from the gather.")]
[Help("AdditionalCommandletArguments", "Optional arguments to pass to the gather process.")]
class Localize : BuildCommand
{
	private class LocalizationBatch
	{
		public LocalizationBatch(string InUEProjectDirectory, string InLocalizationTargetDirectory, string InRemoteFilenamePrefix, IReadOnlyList<string> InLocalizationProjectNames)
		{
			UEProjectDirectory = InUEProjectDirectory;
			LocalizationTargetDirectory = InLocalizationTargetDirectory;
			RemoteFilenamePrefix = InRemoteFilenamePrefix;
			LocalizationProjectNames = InLocalizationProjectNames;
		}

		public string UEProjectDirectory { get; private set; }
		public string LocalizationTargetDirectory { get; private set; }
		public string RemoteFilenamePrefix { get; private set; }
		public IReadOnlyList<string> LocalizationProjectNames { get; private set; }
	};

	public override void ExecuteBuild()
	{
		var UEProjectRoot = ParseParamValue("UEProjectRoot");
		if (UEProjectRoot == null)
		{
			UEProjectRoot = CmdEnv.LocalRoot;
		}

		var UEProjectDirectory = ParseParamValue("UEProjectDirectory");
		if (UEProjectDirectory == null)
		{
			throw new AutomationException("Missing required command line argument: 'UEProjectDirectory'");
		}

		var UEProjectName = ParseParamValue("UEProjectName");
		if (UEProjectName == null)
		{
			UEProjectName = "";
		}

		var LocalizationProjectNames = new List<string>();
		{
			var LocalizationProjectNamesStr = ParseParamValue("LocalizationProjectNames");
			if (LocalizationProjectNamesStr != null)
			{
				foreach (var ProjectName in LocalizationProjectNamesStr.Split(','))
				{
					LocalizationProjectNames.Add(ProjectName.Trim());
				}
			}
		}

		var LocalizationProviderName = ParseParamValue("LocalizationProvider");
		if (LocalizationProviderName == null)
		{
			LocalizationProviderName = "";
		}

		var LocalizationStepNames = new List<string>();
		{
			var LocalizationStepNamesStr = ParseParamValue("LocalizationSteps");
			if (LocalizationStepNamesStr == null)
			{
				LocalizationStepNames.AddRange(new string[] { "Download", "Gather", "Import", "Export", "Compile", "GenerateReports", "Upload" });
			}
			else
			{
				foreach (var StepName in LocalizationStepNamesStr.Split(','))
				{
					LocalizationStepNames.Add(StepName.Trim());
				}
			}
			LocalizationStepNames.Add("Monolithic"); // Always allow the monolithic scripts to run as we don't know which steps they do
		}

		var ShouldGatherPlugins = ParseParam("IncludePlugins");
		var IncludePlugins = new List<string>();
		var ExcludePlugins = new List<string>();
		if (ShouldGatherPlugins)
		{
			var IncludePluginsStr = ParseParamValue("IncludePlugins");
			if (IncludePluginsStr != null)
			{
				foreach (var PluginName in IncludePluginsStr.Split(','))
				{
					IncludePlugins.Add(PluginName.Trim());
				}
			}

			var ExcludePluginsStr = ParseParamValue("ExcludePlugins");
			if (ExcludePluginsStr != null)
			{
				foreach (var PluginName in ExcludePluginsStr.Split(','))
				{
					ExcludePlugins.Add(PluginName.Trim());
				}
			}
		}

		var AdditionalCommandletArguments = ParseParamValue("AdditionalCommandletArguments");
		if (AdditionalCommandletArguments == null)
		{
			AdditionalCommandletArguments = "";
		}

		var LocalizationBatches = new List<LocalizationBatch>();

		// Add the static set of localization projects as a batch
		if (LocalizationProjectNames.Count > 0)
		{
			LocalizationBatches.Add(new LocalizationBatch(UEProjectDirectory, UEProjectDirectory, "", LocalizationProjectNames));
		}

		// Build up any additional batches needed for plugins
		if (ShouldGatherPlugins)
		{
			var PluginsRootDirectory = CombinePaths(UEProjectRoot, UEProjectDirectory, "Plugins");
			IReadOnlyList<PluginInfo> AllPlugins = Plugins.ReadPluginsFromDirectory(new DirectoryReference(PluginsRootDirectory), UEProjectName.Length == 0 ? PluginType.Engine : PluginType.Project);

			// Add a batch for each plugin that meets our criteria
			var AvailablePluginNames = new HashSet<string>();
			foreach (var PluginInfo in AllPlugins)
			{
				AvailablePluginNames.Add(PluginInfo.Name);

				bool ShouldIncludePlugin = (IncludePlugins.Count == 0 || IncludePlugins.Contains(PluginInfo.Name)) && !ExcludePlugins.Contains(PluginInfo.Name);
				if (ShouldIncludePlugin && PluginInfo.Descriptor.LocalizationTargets != null && PluginInfo.Descriptor.LocalizationTargets.Length > 0)
				{
					var RootRelativePluginPath = PluginInfo.Directory.MakeRelativeTo(new DirectoryReference(UEProjectRoot));
					RootRelativePluginPath = RootRelativePluginPath.Replace('\\', '/'); // Make sure we use / as these paths are used with P4

					var PluginTargetNames = new List<string>();
					foreach (var LocalizationTarget in PluginInfo.Descriptor.LocalizationTargets)
					{
						PluginTargetNames.Add(LocalizationTarget.Name);
					}

					LocalizationBatches.Add(new LocalizationBatch(UEProjectDirectory, RootRelativePluginPath, PluginInfo.Name, PluginTargetNames));
				}
			}

			// If we had an explicit list of plugins to include, warn if any were missing
			foreach (string PluginName in IncludePlugins)
			{
				if (!AvailablePluginNames.Contains(PluginName))
				{
					LogWarning("The plugin '{0}' specified by -IncludePlugins wasn't found and will be skipped.", PluginName);
				}
			}
		}

		// Create a single changelist to use for all changes, and hash the current PO files on disk so we can work out whether they actually change
		int PendingChangeList = 0;
		Dictionary<string, byte[]> InitalPOFileHashes = null;
		if (P4Enabled)
		{
			var ChangeListCommitMessage = "Localization Automation";
			if (File.Exists(CombinePaths(CmdEnv.LocalRoot, @"Engine/Build/NotForLicensees/EpicInternal.txt")))
			{
				ChangeListCommitMessage += "\n#okforgithub ignore";
			}

			PendingChangeList = P4.CreateChange(P4Env.Client, ChangeListCommitMessage);
			InitalPOFileHashes = GetPOFileHashes(LocalizationBatches, UEProjectRoot);
		}

		// Process each localization batch
		foreach (var LocalizationBatch in LocalizationBatches)
		{
			ProcessLocalizationProjects(LocalizationBatch, PendingChangeList, UEProjectRoot, UEProjectName, LocalizationProviderName, LocalizationStepNames, AdditionalCommandletArguments);
		}

		// Clean-up the changelist so it only contains the changed files, and then submit it (if we were asked to)
		if (P4Enabled)
		{
			// Revert any PO files that haven't changed aside from their header
			{
				var POFilesToRevert = new List<string>();

				var CurrentPOFileHashes = GetPOFileHashes(LocalizationBatches, UEProjectRoot);
				foreach (var CurrentPOFileHashPair in CurrentPOFileHashes)
				{
					byte[] InitialPOFileHash;
					if (InitalPOFileHashes.TryGetValue(CurrentPOFileHashPair.Key, out InitialPOFileHash) && InitialPOFileHash.SequenceEqual(CurrentPOFileHashPair.Value))
					{
						POFilesToRevert.Add(CurrentPOFileHashPair.Key);
					}
				}

				if (POFilesToRevert.Count > 0)
				{
					var P4RevertArgsFilename = CombinePaths(CmdEnv.LocalRoot, "Engine", "Intermediate", String.Format("LocalizationP4RevertArgs-{0}.txt", Guid.NewGuid().ToString()));

					using (StreamWriter P4RevertArgsWriter = File.CreateText(P4RevertArgsFilename))
					{
						foreach (var POFileToRevert in POFilesToRevert)
						{
							P4RevertArgsWriter.WriteLine(POFileToRevert);
						}
					}

					P4.LogP4(String.Format("-x{0} revert", P4RevertArgsFilename));
					DeleteFile_NoExceptions(P4RevertArgsFilename);
				}
			}

			// Revert any other unchanged files
			P4.RevertUnchanged(PendingChangeList);

			// Submit that single changelist now
			if (AllowSubmit)
			{
				int SubmittedChangeList;
				P4.Submit(PendingChangeList, out SubmittedChangeList);
			}
		}
	}

	private void ProcessLocalizationProjects(LocalizationBatch LocalizationBatch, int PendingChangeList, string UEProjectRoot, string UEProjectName, string LocalizationProviderName, IReadOnlyList<string> LocalizationSteps, string AdditionalCommandletArguments)
	{
		var EditorExe = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/UE4Editor-Cmd.exe");
		var RootWorkingDirectory = CombinePaths(UEProjectRoot, LocalizationBatch.UEProjectDirectory);
		var RootLocalizationTargetDirectory = CombinePaths(UEProjectRoot, LocalizationBatch.LocalizationTargetDirectory);

		// Try and find our localization provider
		LocalizationProvider LocProvider = null;
		{
			LocalizationProvider.LocalizationProviderArgs LocProviderArgs;
			LocProviderArgs.RootWorkingDirectory = RootWorkingDirectory;
			LocProviderArgs.RootLocalizationTargetDirectory = RootLocalizationTargetDirectory;
			LocProviderArgs.RemoteFilenamePrefix = LocalizationBatch.RemoteFilenamePrefix;
			LocProviderArgs.Command = this;
			LocProviderArgs.PendingChangeList = PendingChangeList;
			LocProvider = LocalizationProvider.GetLocalizationProvider(LocalizationProviderName, LocProviderArgs);
		}

		// Make sure the Localization configs and content is up-to-date to ensure we don't get errors later on
		if (P4Enabled)
		{
			LogInformation("Sync necessary content to head revision");
			P4.Sync(P4Env.Branch + "/" + LocalizationBatch.LocalizationTargetDirectory + "/Config/Localization/...");
			P4.Sync(P4Env.Branch + "/" + LocalizationBatch.LocalizationTargetDirectory + "/Content/Localization/...");
		}

		// Generate the info we need to gather for each project
		var ProjectInfos = new List<ProjectInfo>();
		foreach (var ProjectName in LocalizationBatch.LocalizationProjectNames)
		{
			ProjectInfos.Add(GenerateProjectInfo(RootLocalizationTargetDirectory, ProjectName, LocalizationSteps));
		}

		if (LocalizationSteps.Contains("Download") && LocProvider != null)
		{
			// Export all text from our localization provider
			foreach (var ProjectInfo in ProjectInfos)
			{
				LocProvider.DownloadProjectFromLocalizationProvider(ProjectInfo.ProjectName, ProjectInfo.ImportInfo);
			}
		}

		// Setup editor arguments for SCC.
		string EditorArguments = String.Empty;
		if (P4Enabled)
		{
			EditorArguments = String.Format("-SCCProvider={0} -P4Port={1} -P4User={2} -P4Client={3} -P4Passwd={4} -P4Changelist={5} -EnableSCC -DisableSCCSubmit", "Perforce", P4Env.ServerAndPort, P4Env.User, P4Env.Client, P4.GetAuthenticationToken(), PendingChangeList);
		}
		else
		{
			EditorArguments = String.Format("-SCCProvider={0}", "None");
		}
		if (IsBuildMachine)
		{
			EditorArguments += " -BuildMachine";
		}
		EditorArguments += " -Unattended -LogLocalizationConflicts";

		// Execute commandlet for each config in each project.
		bool bLocCommandletFailed = false;
		foreach (var ProjectInfo in ProjectInfos)
		{
			var LocalizationConfigFiles = new List<string>();
			foreach (var LocalizationStep in ProjectInfo.LocalizationSteps)
			{
				if (LocalizationSteps.Contains(LocalizationStep.Name))
				{
					LocalizationConfigFiles.Add(LocalizationStep.LocalizationConfigFile);
				}
			}

			if (LocalizationConfigFiles.Count > 0)
			{
				var ProjectArgument = String.IsNullOrEmpty(UEProjectName) ? "" : String.Format("\"{0}\"", Path.Combine(RootWorkingDirectory, String.Format("{0}.uproject", UEProjectName)));
				var CommandletArguments = String.Format("-config=\"{0}\"", String.Join(";", LocalizationConfigFiles));

				if (!String.IsNullOrEmpty(AdditionalCommandletArguments))
				{
					CommandletArguments += " " + AdditionalCommandletArguments;
				}

				string Arguments = String.Format("{0} -run=GatherText {1} {2}", ProjectArgument, EditorArguments, CommandletArguments);
				LogInformation("Running localization commandlet: {0}", Arguments);
				var StartTime = DateTime.UtcNow;
				var RunResult = Run(EditorExe, Arguments, null, ERunOptions.Default | ERunOptions.NoLoggingOfRunCommand); // Disable logging of the run command as it will print the exit code which GUBP can pick up as an error (we do that ourselves below)
				var RunDuration = (DateTime.UtcNow - StartTime).TotalMilliseconds;
				LogInformation("Localization commandlet finished in {0}s", RunDuration / 1000);

				if (RunResult.ExitCode != 0)
				{
					LogWarning("The localization commandlet exited with code {0} which likely indicates a crash. It ran with the following arguments: '{1}'", RunResult.ExitCode, Arguments);
					bLocCommandletFailed = true;
					break; // We failed a step, so don't process any other steps in this config chain
				}
			}
		}

		if (LocalizationSteps.Contains("Upload") && LocProvider != null)
		{
			if (bLocCommandletFailed)
			{
				LogWarning("Skipping upload to the localization provider due to an earlier commandlet failure.");
			}
			else
			{
				// Upload all text to our localization provider
				foreach (var ProjectInfo in ProjectInfos)
				{
					// Recalculate the split platform paths before doing the upload, as the export may have changed them
					ProjectInfo.ExportInfo.CalculateSplitPlatformNames(RootLocalizationTargetDirectory);
					LocProvider.UploadProjectToLocalizationProvider(ProjectInfo.ProjectName, ProjectInfo.ExportInfo);
				}
			}
		}
	}

	private ProjectInfo GenerateProjectInfo(string RootWorkingDirectory, string ProjectName, IReadOnlyList<string> LocalizationStepNames)
	{
		var LocalizationSteps = new List<ProjectStepInfo>();
		ProjectImportExportInfo ImportInfo = null;
		ProjectImportExportInfo ExportInfo = null;

		// Projects generated by the localization dashboard will use multiple config files that must be run in a specific order
		// Older projects (such as the Engine) would use a single config file containing all the steps
		// Work out which kind of project we're dealing with...
		var MonolithicConfigFile = CombinePaths(RootWorkingDirectory, String.Format(@"Config/Localization/{0}.ini", ProjectName));
		if (File.Exists(MonolithicConfigFile))
		{
			LocalizationSteps.Add(new ProjectStepInfo("Monolithic", MonolithicConfigFile));

			ImportInfo = GenerateProjectImportExportInfo(RootWorkingDirectory, MonolithicConfigFile);
			ExportInfo = ImportInfo;
		}
		else
		{
			var FileSuffixes = new[] { 
				new { Suffix = "Gather", Required = LocalizationStepNames.Contains("Gather") }, 
				new { Suffix = "Import", Required = LocalizationStepNames.Contains("Import") || LocalizationStepNames.Contains("Download") },	// Downloading needs the parsed ImportInfo
				new { Suffix = "Export", Required = LocalizationStepNames.Contains("Gather") || LocalizationStepNames.Contains("Upload")},		// Uploading needs the parsed ExportInfo
				new { Suffix = "Compile", Required = LocalizationStepNames.Contains("Compile") }, 
				new { Suffix = "GenerateReports", Required = false } 
			};

			foreach (var FileSuffix in FileSuffixes)
			{
				var ModularConfigFile = CombinePaths(RootWorkingDirectory, String.Format(@"Config/Localization/{0}_{1}.ini", ProjectName, FileSuffix.Suffix));

				if (File.Exists(ModularConfigFile))
				{
					LocalizationSteps.Add(new ProjectStepInfo(FileSuffix.Suffix, ModularConfigFile));

					if (FileSuffix.Suffix == "Import")
					{
						ImportInfo = GenerateProjectImportExportInfo(RootWorkingDirectory, ModularConfigFile);
					}
					else if (FileSuffix.Suffix == "Export")
					{
						ExportInfo = GenerateProjectImportExportInfo(RootWorkingDirectory, ModularConfigFile);
					}
				}
				else if (FileSuffix.Required)
				{
					throw new AutomationException("Failed to find a required config file! '{0}'", ModularConfigFile);
				}
			}
		}

		return new ProjectInfo(ProjectName, LocalizationSteps, ImportInfo, ExportInfo);
	}

	private ProjectImportExportInfo GenerateProjectImportExportInfo(string RootWorkingDirectory, string LocalizationConfigFile)
	{
		ConfigFile File = new ConfigFile(new FileReference(LocalizationConfigFile), ConfigLineAction.Add);
		var LocalizationConfig = new ConfigHierarchy(new ConfigFile[] { File });

		string DestinationPath;
		if (!LocalizationConfig.GetString("CommonSettings", "DestinationPath", out DestinationPath))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'DestinationPath', File: '{0}'", LocalizationConfigFile);
		}

		string ManifestName;
		if (!LocalizationConfig.GetString("CommonSettings", "ManifestName", out ManifestName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'ManifestName', File: '{0}'", LocalizationConfigFile);
		}

		string ArchiveName;
		if (!LocalizationConfig.GetString("CommonSettings", "ArchiveName", out ArchiveName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'ArchiveName', File: '{0}'", LocalizationConfigFile);
		}

		string PortableObjectName;
		if (!LocalizationConfig.GetString("CommonSettings", "PortableObjectName", out PortableObjectName))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'PortableObjectName', File: '{0}'", LocalizationConfigFile);
		}

		string NativeCulture;
		if (!LocalizationConfig.GetString("CommonSettings", "NativeCulture", out NativeCulture))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'NativeCulture', File: '{0}'", LocalizationConfigFile);
		}

		List<string> CulturesToGenerate;
		if (!LocalizationConfig.GetArray("CommonSettings", "CulturesToGenerate", out CulturesToGenerate))
		{
			throw new AutomationException("Failed to find a required config key! Section: 'CommonSettings', Key: 'CulturesToGenerate', File: '{0}'", LocalizationConfigFile);
		}

		bool bUseCultureDirectory;
		if (!LocalizationConfig.GetBool("CommonSettings", "bUseCultureDirectory", out bUseCultureDirectory))
		{
			// bUseCultureDirectory is optional, default is true
			bUseCultureDirectory = true;
		}

		var ProjectImportExportInfo = new ProjectImportExportInfo(DestinationPath, ManifestName, ArchiveName, PortableObjectName, NativeCulture, CulturesToGenerate, bUseCultureDirectory);
		ProjectImportExportInfo.CalculateSplitPlatformNames(RootWorkingDirectory);
		return ProjectImportExportInfo;
	}

	private Dictionary<string, byte[]> GetPOFileHashes(IReadOnlyList<LocalizationBatch> LocalizationBatches, string UEProjectRoot)
	{
		var AllFiles = new Dictionary<string, byte[]>();

		foreach (var LocalizationBatch in LocalizationBatches)
		{
			var LocalizationPath = CombinePaths(UEProjectRoot, LocalizationBatch.LocalizationTargetDirectory, "Content", "Localization");
			if (!Directory.Exists(LocalizationPath))
			{
				continue;
			}

			string[] POFileNames = Directory.GetFiles(LocalizationPath, "*.po", SearchOption.AllDirectories);
			foreach (var POFileName in POFileNames)
			{
				using (StreamReader POFileReader = File.OpenText(POFileName))
				{
					// Don't include the PO header (everything up to the first empty line) in the hash as it contains transient information (like timestamps) that we don't care about
					bool bHasParsedHeader = false;
					var POFileHash = MD5.Create();

					string POFileLine;
					while ((POFileLine = POFileReader.ReadLine()) != null)
					{
						if (!bHasParsedHeader)
						{
							bHasParsedHeader = POFileLine.Length == 0;
							continue;
						}

						var POFileLineBytes = Encoding.UTF8.GetBytes(POFileLine);
						POFileHash.TransformBlock(POFileLineBytes, 0, POFileLineBytes.Length, null, 0);
					}

					POFileHash.TransformFinalBlock(new byte[0], 0, 0);

					AllFiles.Add(POFileName, POFileHash.Hash);
				}
			}
		}

		return AllFiles;
	}
}

// Legacy alias
class Localise : Localize
{
};
