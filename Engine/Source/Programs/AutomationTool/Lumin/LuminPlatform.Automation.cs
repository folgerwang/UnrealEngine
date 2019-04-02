// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Threading;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

/// Lumin platform behaves more like Linux than Android as there's no Android
/// system running on it. It's just the bare OS with non-Java additions.
public class LuminPlatform : Platform
{
	#region Public

	// TODO Try to get these at runtime from the Lumin lifecycle service instead of hardcoding them here.
	//private static string PackageInstallPath = "/package";
	private static string PackageWritePath = "/documents/c2";

	private List<FileReference> RuntimeDependenciesForMabu;

	public LuminPlatform()
		: base(UnrealTargetPlatform.Lumin)
	{
		TargetIniPlatformType = UnrealTargetPlatform.Lumin;
		RuntimeDependenciesForMabu = new List<FileReference>();
	}

	private static string GetElfNameWithoutArchitecture(ProjectParams Params, string DecoratedExeName)
	{
		return Path.Combine(Path.GetDirectoryName(Params.GetProjectExeForPlatform(UnrealTargetPlatform.Lumin).ToString()), DecoratedExeName);
	}

	// 	public override List<string> GetExecutableNames(DeploymentContext SC, bool bIsRun = false)
	// 	{
	// 		List<string> Exes = base.GetExecutableNames(SC, bIsRun);
	// 		// replace the binary name to match what was staged
	// 		if (bIsRun)
	// 		{
	// 			Exes[0] = CommandUtils.CombinePaths(SC.StageProjectRoot, "Binaries", SC.PlatformDir, SC.ShortProjectName);
	// 		}
	// 		return Exes;
	// 	}

	private string CleanFilePath(string FilePath)
	{
		// Removes the extra characters from a FFilePath parameter.
		// This functionality is required in the automation file to avoid having duplicate variables stored in the settings file.
		// Potentially this could be replaced with FParse::Value("IconModelPath="(Path="", Value).
		int startIndex = FilePath.IndexOf('"') + 1;
		int length = FilePath.LastIndexOf('"') - startIndex;
		if (length > 0)
		{
			FilePath = FilePath.Substring(startIndex, length);
		}
		FilePath = FilePath.Replace('\\', '/').Replace("//", "/");
		return FilePath;
	}

	private string StageIconFileToMabu(string ConfigPropertyName, string IconStagePath, DeploymentContext SC)
	{
		// Read in any extra assets required to correctly install the application.
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, SC.RawProjectPath.Directory, UnrealTargetPlatform.Lumin);
		// ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, SC.RawProjectPath.Directory, SC.StageTargetPlatform.PlatformType);
		string Value;
		Ini.GetString("/Script/LuminRuntimeSettings.LuminRuntimeSettings", ConfigPropertyName, out Value);
		Value = CleanFilePath(Value);
		// We can have two different kinds of paths in the ini for icons: engine exec relative or project root relative.
		DirectoryReference IconDir = GetFullPathFromRelativePath(Value, SC);
		List<FileReference> IconFiles = SC.FindFilesToStage(IconDir, "*", StageFilesSearch.AllDirectories);
		StringBuilder Builder = new StringBuilder();
		foreach(FileReference IconFile in IconFiles)
		{
			string StagePath = IconFile.FullName.Replace(IconDir.FullName, IconStagePath);
			Builder.AppendLine(String.Format("\"{0}\" : \"{1}\"\\", IconFile.FullName.Replace('\\', '/'), StagePath.Replace('\\', '/')));
		}
		return Builder.ToString();
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// 		if (SC.StageTargetConfigurations.Count != 1)
		// 		{
		// 			throw new AutomationException(ExitCode.Error_OnlyOneTargetConfigurationSupported, "Lumin is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		// 		}
		// 		if (SC.StageExecutables.Count != 1 && Params.Package)
		// 		{
		// 			throw new AutomationException("Exactly one executable expected when staging Lumin. Had " + SC.StageExecutables.Count.ToString());
		// 		}
		// 		string Configuration = GetConfigurations(SC)[0];
		// 		// TODO: Consider if we need to deal with devices at this point.
		// 		string DeviceArchitecture = "-arm64";
		// 		string GPUArchitecture = GetBestGPUArchitecture(Params, "");
		// 		string ExeVariation = DeviceArchitecture + GPUArchitecture;
		// 		string ProjectGameExeFilename = Params.ProjectGameExeFilename + Configuration + ExeVariation;
		// 
		// 		List<string> Exes = GetExecutableNames(SC);
		// 
		// 		foreach (var Exe in Exes)
		// 		{
		// 			string ExeFilename = Path.GetFileNameWithoutExtension(Exe) + ExeVariation;
		// 			if (Exe.StartsWith(CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir)))
		// 			{
		// 				// remap the project root. Rename the executable to the project name to strip arch/gl verison
		// 				string ExeDir = CombinePaths(SC.ProjectRoot, "Binaries", SC.PlatformDir);
		// 				if (Exe == Exes[0])
		// 				{
		// 					SC.StageFiles(
		// 						StagedFileType.NonUFS,
		// 						ExeDir,
		// 						ExeFilename,
		// 						true,
		// 						null,
		// 						"/bin",
		// 						false,
		// 						true,
		// 						SC.ShortProjectName);
		// 					StageNvTegraGfxDebugger(SC, "/bin");
		// 				}
		// 				else
		// 				{
		// 					SC.StageFiles(
		// 						StagedFileType.NonUFS,
		// 						ExeDir,
		// 						ExeFilename,
		// 						true,
		// 						null,
		// 						"/bin",
		// 						false);
		// 				}
		// 			}
		// 			else if (Exe.StartsWith(CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir)))
		// 			{
		// 				// Move the executable for content-only projects into the project directory, using the project name, so it can figure out the UProject to look for and is consistent with code projects.
		// 				string ExeDir = CombinePaths(SC.LocalRoot, "Engine/Binaries", SC.PlatformDir);
		// 				if (!Params.IsCodeBasedProject && Exe == Exes[0])
		// 				{
		// 					// ensure the ue4game binary exists, if applicable
		// 					if (!SC.IsCodeBasedProject && !FileExists_NoExceptions(ProjectGameExeFilename) && !SC.bIsCombiningMultiplePlatforms)
		// 					{
		// 						Log("Failed to find game binary " + ProjectGameExeFilename);
		// 						throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Could not find game binary {0}. You may need to build the UE4 project with your target configuration and platform.", ProjectGameExeFilename);
		// 					}
		// 
		// 					SC.StageFiles(
		// 						StagedFileType.NonUFS,
		// 						ExeDir,
		// 						ExeFilename,
		// 						true,
		// 						null,
		// 						"/bin",
		// 						false,
		// 						true,
		// 						SC.ShortProjectName);
		// 					StageNvTegraGfxDebugger(SC, "/bin");
		// 				}
		// 				else
		// 				{
		// 					SC.StageFiles(
		// 						StagedFileType.NonUFS,
		// 						ExeDir,
		// 						ExeFilename,
		// 						true, null, null, false);
		// 				}
		// 			}
		// 			else
		// 			{
		// 				throw new AutomationException("Can't stage the exe {0} because it doesn't start with {1} or {2}", Exe, CombinePaths(SC.RuntimeProjectRootDir, "Binaries", SC.PlatformDir), CombinePaths(SC.RuntimeRootDir, "Engine/Binaries", SC.PlatformDir));
		// 			}
		// 		}
		// 
		// Patterns to exclude from wildcard searches. Any maps and assets must be cooked. 
		List<string> ExcludePatterns = new List<string>();
		ExcludePatterns.Add(".../*.umap");
		ExcludePatterns.Add(".../*.uasset");
		ExcludePatterns.Add(".../*.uplugin");
		foreach (StageTarget Target in SC.StageTargets)
		{
			HashSet<RuntimeDependency> DependenciesToRemove = new HashSet<RuntimeDependency>();
			foreach (RuntimeDependency RuntimeDependency in Target.Receipt.RuntimeDependencies)
			{
				foreach (FileReference File in CommandUtils.ResolveFilespec(CommandUtils.RootDirectory, RuntimeDependency.Path.FullName, ExcludePatterns))
				{
					// Stage all libraries described as Runtime Dependencies in the bin folder.
					if (File.GetExtension() == ".so")
					{
						// third party lbs should be packaged in the bin folder, without changing their filename casing. Staging via Unreal's system changes their case.
						RuntimeDependenciesForMabu.Add(File);
						DependenciesToRemove.Add(RuntimeDependency);
					}
				}
			}

			Target.Receipt.RuntimeDependencies.RemoveAll(x => DependenciesToRemove.Contains(x));
		}
	}

	private DirectoryReference GetFullPathFromRelativePath(string RelativePath, DeploymentContext SC)
	{
		string fullPath = RelativePath;
		if (!string.IsNullOrEmpty(fullPath) && !(Path.IsPathRooted(fullPath)))
		{
			if (Path.IsPathRooted(fullPath))
			{
				// We where handed an absolute path. So just use that.
				fullPath = Path.GetFullPath(fullPath);
			}
			else
			{
				// For relative paths we need to figure out if they are in the engine or project tree.
				string FromProjectFullPath = Path.GetFullPath(CombinePaths(SC.ProjectRoot.ToString(), CleanFilePath(RelativePath)));
				string FromEngineFullPath = Path.GetFullPath(CombinePaths(SC.EngineRoot.ToString(), CleanFilePath(RelativePath)));
				if (Directory.Exists(FromProjectFullPath) || File.Exists(FromProjectFullPath))
				{
					// Works as a project relative path.. We'll go with that.
					fullPath = FromProjectFullPath;
				}
				else
				{
					// Not in the project tree.. Assume it's in the engine tree.
					fullPath = FromEngineFullPath;
				}
			}
		}
		return new DirectoryReference(Path.GetFullPath(fullPath));
	}

	private void StageNvTegraGfxDebugger(DeploymentContext SC, string Dir)
	{
		// @todo Lumin nv debugger
		// 		if (LuminPlatformContext.UseTegraGraphicsDebugger(SC.RawProjectPath))
		// 		{
		// 			List<string> NvFiles = new List<string>();
		// 			NvFiles.Add(CommandUtils.CombinePaths(LuminPlatformContext.TegraDebuggerDir, "target", "android-L-egl-t132-a64", "Stripped_libNvPmApi.Core.so"));
		// 			NvFiles.Add(CommandUtils.CombinePaths(LuminPlatformContext.TegraDebuggerDir, "target", "android-L-egl-t132-a64", "Stripped_libNvidia_gfx_debugger.so"));
		// 			foreach (string NvFile in NvFiles)
		// 			{
		// 				string TargetFile = Path.GetFileName(NvFile).Replace("Stripped_","");
		// 				SC.StageFile(StagedFileType.NonUFS, NvFile, CommandUtils.CombinePaths(Dir, TargetFile));
		// 			}
		// 		}
	}

	/// <summary>
	/// Gets cook platform name for this platform.
	/// </summary>
	/// <returns>Cook platform string.</returns>
	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return bIsClientOnly ? "LuminClient" : "Lumin";
	}

	/// <summary>
	/// return true if we need to change the case of filenames outside of pak files
	/// </summary>
	/// <returns></returns>
	public override bool DeployLowerCaseFilenames()
	{
		// @todo quail hack: we don't actually want to lower case some of the OS files, but I HACKED the
		// CopyBuildToStagingDirectory code to not apply lowercase to root files 
		return true;
	}

	private static string GetFinalPackageDirectory(ProjectParams Params)
	{
		string ProjectDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(Params.RawProjectPath.FullName)), "Binaries/Lumin");

		if (Params.Prebuilt)
		{
			ProjectDir = Path.Combine(Params.BaseStageDirectory, "Lumin");
		}
		return ProjectDir;
	}

	// @todo Lumin: maybe move this into UEDeployLumin.cs to share with MakeMabuPackage
	private static string GetFinalMpkName(ProjectParams Params, DeploymentContext SC)
	{
		string ProjectDir = GetFinalPackageDirectory(Params);
		string MpkName = SC.StageExecutables[0] + ".mpk";
		if (MpkName.StartsWith("UE4Game"))
		{
			MpkName = MpkName.Replace("UE4Game", Params.ShortProjectName);
		}
		// Package's go to project location, not necessarily where the elf is (content only packages need to output to their directory)
		return Path.Combine(ProjectDir, MpkName);
	}

	private static string GetFinalBatchName(ProjectParams Params, DeploymentContext SC, bool bUninstall)
	{
		string Extension = ".bat";
		switch (HostPlatform.Current.HostEditorPlatform)
		{
			default:
			case UnrealTargetPlatform.Win64:
				Extension = ".bat";
				break;

			case UnrealTargetPlatform.Linux:
				Extension = ".sh";
				break;

			case UnrealTargetPlatform.Mac:
				Extension = ".command";
				break;
		}
		return Path.Combine(GetFinalPackageDirectory(Params), (bUninstall ? "Uninstall_" : "Install_") + Path.GetFileNameWithoutExtension(GetFinalMpkName(Params, SC)) + Extension);
	}

	private List<string> CollectPluginDataPaths(DeploymentContext SC)
	{
		// collect plugin extra data paths from target receipts
		List<string> PluginExtras = new List<string>();
		foreach (StageTarget Target in SC.StageTargets)
		{
			TargetReceipt Receipt = Target.Receipt;
			var Results = Receipt.AdditionalProperties.Where(x => x.Name == "LuminPlugin");
			foreach (var Property in Results)
			{
				// Keep only unique paths
				string PluginPath = Property.Value;
				if (PluginExtras.FirstOrDefault(x => x == PluginPath) == null)
				{
					PluginExtras.Add(PluginPath);
					LogInformation("LuminPlugin: {0}", PluginPath);
				}
			}
		}
		return PluginExtras;
	}

	private string[] GenerateInstallBatchFile(string MpkName, string PackageName, ProjectParams Params)
	{
		string[] BatchLines = null;
		if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
		{
			LogInformation("Writing bat for install");
			BatchLines = new string[] {
				"@echo off",
				"setlocal",
				"set MLSDK=%MLSDK%",
				"if \"%MLSDK%\"==\"\" set MLSDK="+Environment.GetEnvironmentVariable("MLSDK"),
				"set MLDB=%MLSDK%\\tools\\mldb\\mldb.exe",
				"@echo.",
				"@echo Installing existing application. Failures here indicate a problem with the device (connection or storage permissions) and are fatal.",
				"%MLDB% %DEVICE% install -u \"%~dp0\\" + Path.GetFileName(MpkName) + "\"",
				"@if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
				"@echo.",
				"@echo Installation successful",
				"%MLDB% %DEVICE% ps > nul",
				"@if \"%ERRORLEVEL%\" NEQ \"0\" goto OobeError",
				"goto:eof",
				":OobeError",
				"@echo Device is not ready for use. Run \"%MLDB% ps\" from a command prompt for details.",
				"goto Pause",
				":Error",
				"@echo.",
				"@echo There was an error installing the game. Look above for more info.",
				"@echo.",
				"@echo Things to try:",
				"@echo Check that the device (and only the device) is listed with \"%MLDB% devices\" from a command prompt.",
				"@echo Check if the device is ready for use with \"%MLDB% ps\" from a command prompt.",
				":Pause",
				"@pause"
			};
		}
		else
		{
			LogInformation("Writing shell for install");
			BatchLines = new string[] {
				"#!/bin/sh",
				"cd \"`dirname \"$0\"`\"",
				"MLSDK_ROOT=$MLSDK",
				"if [ -z \"$MLSDK_ROOT\" ]; then",
					"\tMLSDK_ROOT=\"" + Environment.GetEnvironmentVariable("MLSDK") + " \"",
				"fi",
				"MLDB=$MLSDK_ROOT/tools/mldb/mldb",
				"echo",
				"echo \"Installing existing application. Failures here indicate a problem with the device (connection or storage permissions) and are fatal.\"",
				"$MLDB $DEVICE install -u " + Path.GetFileName(MpkName),
				"if [ $? -ne 0 ]; then",
					"\techo",
					"\techo \"There was an error installing the game. Look above for more info.\"",
					"\techo",
					"\techo \"Things to try:\"",
					"\techo \"Check that the device (and only the device) is listed with \"$MLDB devices\" from a command prompt.\"",
					"\techo \"Check if the device is ready for use with \"%MLDB% ps\" from a command prompt.\"",
					"\techo",
					"\texit 1",
				"fi",
				"echo",
				"echo \"Installation successful\"",
				"$MLDB $DEVICE ps > /dev/null",
				"if [ $? -ne 0 ]; then",
					"\techo \"Device is not ready for use. Run \"%MLDB% ps\" from a command prompt for details.\"",				
				"fi",
				"exit 0",
			};
		}
		return BatchLines;
	}

	private string[] GenerateUninstallBatchFile(string PackageName, ProjectParams Params)
	{
		string[] BatchLines = null;
		if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
		{
			LogInformation("Writing bat for uninstall");
			BatchLines = new string[] {
				"@echo off",
				"setlocal",
				"set MLSDK=%MLSDK%",
				"if \"%MLSDK%\"==\"\" set MLSDK="+Environment.GetEnvironmentVariable("MLSDK"),
				"set MLDB=%MLSDK%\\tools\\mldb\\mldb.exe",
				"@echo.",
				"@echo Uninstalling existing application. Failures here can almost always be ignored.",
				"%MLDB% %DEVICE% uninstall " + PackageName,
				"@echo.",
				"@echo.",
				"@echo Uninstall completed",
			};
		}
		else
		{
			LogInformation("Writing shell for uninstall");
			BatchLines = new string[] {
				"#!/bin/sh",
				"MLSDK_ROOT=$MLSDK",
				"if [ -z \"$MLSDK_ROOT\" ]; then",
					"\tMLSDK_ROOT=\"" + Environment.GetEnvironmentVariable("MLSDK") + " \"",
				"fi",
				"MLDB=$MLSDK_ROOT/tools/mldb/mldb",
				"echo",
				"echo \"Uninstalling existing application. Failures here can almost always be ignored.\"",
				"$MLDB $DEVICE uninstall " + PackageName,
				"echo",
				"echo",
				"echo \"Uninstall completed\"",
				"exit 0",
			};
		}
		return BatchLines;
	}

	// 	private LuminToolChain ValidateConfig(ProjectParams Params, DeploymentContext SC, string ActionName)
	// 	{
	// 		if (SC.StageTargetConfigurations.Count != 1)
	// 		{
	// 			throw new AutomationException(ExitCode.Error_OnlyOneTargetConfigurationSupported, "Lumin is currently only able to {0} one target configuration at a time, but StageTargetConfigurations contained {1} configurations", ActionName, SC.StageTargetConfigurations.Count);
	// 		}
	// 
	// 		LuminToolChain ToolChain = new LuminToolChain(Params.RawProjectPath);
	// 
	// 		var GPUArchitectures = ToolChain.GetAllGPUArchitectures();
	// 
	// 		if (GPUArchitectures.Count != 1)
	// 		{
	// 			throw new AutomationException("Lumin is currently only able to {0} one GPU architecture at a time, but there are {1} active GPU architectures in project configuration.", ActionName, GPUArchitectures.Count);
	// 		}
	// 
	// 		return ToolChain;
	// 	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		// 		ValidateConfig(Params, SC, "archive");

		//ILuminDeploy Deploy = LuminExports.CreateDeploymentHandler(Params.RawProjectPath);

		string MpkName = GetFinalMpkName(Params, SC);

		// verify the files exist
		if (!FileExists(MpkName))
		{
			throw new AutomationException(ExitCode.Error_AppNotFound, "ARCHIVE FAILED - {0} was not found", MpkName);
		}

		SC.ArchiveFiles(Path.GetDirectoryName(MpkName), Path.GetFileName(MpkName));

		string BatchName = GetFinalBatchName(Params, SC, false);
		string UninstallBatchName = GetFinalBatchName(Params, SC, true);

		SC.ArchiveFiles(Path.GetDirectoryName(BatchName), Path.GetFileName(BatchName));
		SC.ArchiveFiles(Path.GetDirectoryName(UninstallBatchName), Path.GetFileName(UninstallBatchName));
	}

	private string GetExePath(ProjectParams Params, DeploymentContext SC)
	{
		// @todo Lumin: bleh - we should just remove GPU arch from the name entirely maybe, except we want to be compatible with Android still
		return Path.Combine(Path.GetDirectoryName(Params.GetProjectExeForPlatform(UnrealTargetPlatform.Lumin).ToString()), SC.StageExecutables[0] + "-arm64" + GetBestGPUArchitecture(Params) + ".so");
	}

	private bool IsPackageUpToDate(ProjectParams Params, DeploymentContext SC)
	{
		if (Params.IterativeDeploy)
		{
			if (Params.Devices.Count != 1)
			{
				throw new AutomationException("Can only interatively deploy to a single device, but {0} were specified", Params.Devices.Count);
			}

			string NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(Params.DeviceNames[0]);
			// check to determine if we need to update the IPA
			if (File.Exists(NonUFSManifestPath))
			{
				string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
				string[] Lines = NonUFSFiles.Split('\n');

				// if we don't need to deploy any NonUFS files, and the exe is up to date, then there's no need to waste time packaging!
				if (Lines.Length > 0 && !string.IsNullOrWhiteSpace(Lines[0]))
				{
					LogInformation("Need minimal package because {0} NonUFS files needed to be staged", Lines.Length);
					return false;
				}
			}
			else
			{
				LogInformation("Need minimal package because delta staging file {0} wasn't pulled from device", NonUFSManifestPath);
				return false;
			}

			string ExeTimestampFileName = CombinePaths(Path.Combine(Params.RawProjectPath.Directory.FullName, "Intermediate", "ExeTimestamp.txt"));
			if (File.Exists(ExeTimestampFileName))
			{
				string TimestampString = File.ReadAllText(ExeTimestampFileName);
				DateTime DeployedTimestamp = DateTime.Parse(TimestampString);
				DateTime ExeTimestamp = File.GetLastWriteTime(GetExePath(Params, SC));

				if (ExeTimestamp > DeployedTimestamp)
				{
					LogInformation("Need minimal package because exe timestamp {0:O} is newer than deployed timestamp {1:O}", ExeTimestamp, DeployedTimestamp);
					return false;
				}
				else
				{
					LogInformation("Exe up to date: Exe is {0:O} / {1}, Deployed is {2:O}", ExeTimestamp, SC.StageExecutables[0], DeployedTimestamp);
				}
			}
			else
			{
				LogInformation("Need minimal package because exe timestamp file {0} wasn't pulled from device", ExeTimestampFileName);
				return false;
			}

			return true;
		}

		return false;
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		ILuminDeploy Deploy = LuminExports.CreateDeploymentHandler(Params.RawProjectPath);

		// don't bother making the package, if we don't need it!
		if (IsPackageUpToDate(Params, SC))
		{
			return;
		}

		//requires target name instead of just the project name
		string TargetName = Params.ClientCookedTargets[0];
		Deploy.InitUPL(SC.StageTargets[0].Receipt);

		string MpkName = GetFinalMpkName(Params, SC);
		string PackageName = GetPackageName(Params);

		if (!Params.Prebuilt)
		{
			string additionalFiles = Deploy.StageFiles();

			// note this must match MakeMabuPackage!
			string UE4BuildPath = Path.Combine(Params.RawProjectPath.Directory.FullName, "Intermediate/Lumin/Mabu");
			string MabuFile = Path.Combine(UE4BuildPath, GetPackageName(Params) + ".package");

			// generate data file list file thing
			StringBuilder Builder = new StringBuilder();

			Builder.AppendLine("OPTIONS=\\");
			Builder.AppendLine("stl/libgnustl\\");
			Builder.AppendLine(string.Format("package/debuggable/{0}", Params.Distribution ? "off" : "on"));
			Builder.AppendLine("DATAS=\\");
			if (additionalFiles.Length > 0)
			{
				//intentionally no newline because it should already be in the additionalFiles
				Builder.Append(additionalFiles);
			}

			string StagedEngineDir = Path.Combine(SC.StageDirectory.FullName, "engine").ToLowerInvariant();
			string StagedProjectDir = Path.Combine(SC.StageDirectory.FullName, Params.ShortProjectName).ToLowerInvariant();
			string StagedMoviesDir = Path.Combine(StagedProjectDir, "content", "movies");

			foreach (var FilePath in Directory.EnumerateFiles(SC.StageDirectory.FullName, "*", SearchOption.AllDirectories))
			{
				if (Path.GetFileName(MabuFile) == Path.GetFileName(FilePath))
				{
					continue;
				}

				string LowercaseFilePath = FilePath.ToLowerInvariant();

				// when doing iterative deploying, we only add the root files and Icon files
				// @todo Lumin: Can we change iterative deploying to be at staging time, so that we have the list of NonUFS files to package
				// @todo Lumin: Why aren't the manifest files around in the Staged dir? This would allow us to know what is Staged
				if (Params.IterativeDeploy)
				{
					// skip project files (except movies)
					if (LowercaseFilePath.StartsWith(StagedEngineDir) || (LowercaseFilePath.StartsWith(StagedProjectDir) && !LowercaseFilePath.StartsWith(StagedMoviesDir)))
					{
						continue;
					}
				}

				{
					Builder.AppendLine(String.Format("\"{0}\" : \"{1}\"\\", FilePath, Utils.MakePathRelativeTo(FilePath, SC.StageDirectory.FullName, true).Replace('\\', '/')));
				}
			}

			// third party lbs should be packaged in the bin folder, without changing their filename casing.
			foreach (FileReference LibFile in RuntimeDependenciesForMabu)
			{
				Builder.AppendLine(String.Format("\"{0}\" : \"bin/{1}\"\\", LibFile.FullName, LibFile.GetFileName()));
			}

			// Stage icon files directly to mabu instead of via Unreal to avoid the file casing issues.
			// Icon files must be staged as is, without changing the case as the fbx, obj etc could have 
			// embedded references to texture files.
			Builder.Append(StageIconFileToMabu("IconModelPath", Deploy.GetIconModelStagingPath(), SC));
			Builder.Append(StageIconFileToMabu("IconPortalPath", Deploy.GetIconPortalStagingPath(), SC));

			// find executable
			if (GetExecutableNames(SC).Count > 1)
			{
				throw new AutomationException("Multiple executables are not expected for a Lumin build");
			}

			// now put the exe into the data list (coming from original location)
			string ExePath = GetExePath(Params, SC);
			Builder.AppendLine(String.Format("\"{0}/Binaries/{1}\" : \"bin/{2}\"", Path.GetDirectoryName(MabuFile), Path.GetFileName(ExePath), Params.ShortProjectName));

			Directory.CreateDirectory(Path.GetDirectoryName(MabuFile));
			WriteAllText(MabuFile, Builder.ToString());

			string ElfName = GetElfNameWithoutArchitecture(Params, SC.StageExecutables[0]);
			Deploy.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, ExePath, SC.LocalRoot + "/Engine", Params.Distribution, "", false, MpkName);
		}

		// Write install batch file(s).
		string BatchName = GetFinalBatchName(Params, SC, false);
		// make a batch file that can be used to install the .mpk and .obb files
		string[] BatchLines = GenerateInstallBatchFile(MpkName, PackageName, Params);
		Directory.CreateDirectory(Path.GetDirectoryName(MpkName));
		File.WriteAllLines(BatchName, BatchLines);
		// make a batch file that can be used to uninstall the .mpk and .obb files
		string UninstallBatchName = GetFinalBatchName(Params, SC, true);
		BatchLines = GenerateUninstallBatchFile(PackageName, Params);
		File.WriteAllLines(UninstallBatchName, BatchLines);

		// If needed, make the batch files able to execute
		if (Utils.IsRunningOnMono)
		{
			CommandUtils.FixUnixFilePermissions(BatchName);
			CommandUtils.FixUnixFilePermissions(UninstallBatchName);
		}

		PrintRunTime();
	}

	public override bool RequiresPackageToDeploy
	{
		get { return true; }
	}

	public override bool IsSupported { get { return true; } }

	public override void GetConnectedDevices(ProjectParams Params, out List<string> Devices)
	{
		Devices = new List<string>();
		IProcessResult Result = RunDeviceCommand(Params, "", "devices");

		if (Result.Output.Length > 0)
		{
			string[] LogLines = Result.Output.Split(new char[] { '\n', '\r' });
			bool FoundList = false;
			for (int i = 0; i < LogLines.Length; ++i)
			{
				if (FoundList == false)
				{
					if (LogLines[i].StartsWith("List of devices attached"))
					{
						FoundList = true;
					}
					continue;
				}

				string[] DeviceLine = LogLines[i].Split(new char[] { '\t' });

				if (DeviceLine.Length == 2)
				{
					// the second param should be "device"
					// if it's not setup correctly it might be "unattached" or "powered off" or something like that
					// warning in that case
					if (DeviceLine[1] == "device")
					{
						Devices.Add("@" + DeviceLine[0]);
					}
					else
					{
						CommandUtils.LogWarning("Device attached but in bad state {0}:{1}", DeviceLine[0], DeviceLine[1]);
					}
				}
			}
		}
	}

	private string GetPackageName(ProjectParams Params)
	{
		ILuminDeploy Deploy = LuminExports.CreateDeploymentHandler(Params.RawProjectPath);
		return Deploy.GetPackageName(Params.ShortProjectName);
	}

	public override bool RetrieveDeployedManifests(ProjectParams Params, DeploymentContext SC, string DeviceName, out List<string> UFSManifests, out List<string> NonUFSManifests)
	{
		UFSManifests = null;
		NonUFSManifests = null;

		string PackageName = GetPackageName(Params);

		string UFSManifestFileName = CombinePaths(SC.StageDirectory.FullName, SC.GetUFSDeployedManifestFileName(DeviceName));
		string NonUFSManifestFileName = CombinePaths(SC.StageDirectory.FullName, SC.GetNonUFSDeployedManifestFileName(DeviceName));
		string ExeTimestampFileName = CombinePaths(Path.Combine(Params.RawProjectPath.Directory.FullName, "Intermediate", "ExeTimestamp.txt"));


		// Try retrieving the UFS files manifest files from the device
		IProcessResult UFSResult = RunDeviceCommand(Params, DeviceName, " pull -p " + PackageName + " " + PackageWritePath + "/" + SC.GetUFSDeployedManifestFileName(null) + " \"" + UFSManifestFileName + "\"", null, ERunOptions.AppMustExist);
		if (!(UFSResult.Output.Contains("bytes") || UFSResult.Output.Contains("[100%]")))
		{
			File.Delete(UFSManifestFileName);
			File.Delete(NonUFSManifestFileName);
			return false;
		}

		// Try retrieving the non UFS files manifest files from the device
		IProcessResult NonUFSResult = RunDeviceCommand(Params, DeviceName, " pull -p " + PackageName + " " + PackageWritePath + "/" + SC.GetNonUFSDeployedManifestFileName(null) + " \"" + NonUFSManifestFileName + "\"", null, ERunOptions.AppMustExist);
		if (!(NonUFSResult.Output.Contains("bytes") || NonUFSResult.Output.Contains("[100%]")))
		{
			// Did not retrieve both so delete one we did retrieve
			File.Delete(UFSManifestFileName);
			File.Delete(NonUFSManifestFileName);
			return false;
		}

		// Try retrieving the non UFS files manifest files from the device
		IProcessResult ExeTimestampResult = RunDeviceCommand(Params, DeviceName, " pull -p " + PackageName + " " + PackageWritePath + "/" + "ExeTimestamp.txt" + " \"" + ExeTimestampFileName + "\"", null, ERunOptions.AppMustExist);
		if (!(ExeTimestampResult.Output.Contains("bytes") || ExeTimestampResult.Output.Contains("[100%]")))
		{
			File.Delete(ExeTimestampFileName);
		}

		// Return the manifest files
		UFSManifests = new List<string>();
		UFSManifests.Add(UFSManifestFileName);
		NonUFSManifests = new List<string>();
		NonUFSManifests.Add(NonUFSManifestFileName);

		return true;
	}

	/// <summary>
	/// Deploy, aka adb push, files to connected device.
	/// </summary>
	/// <param name="Params"></param>
	/// <param name="SC"></param>
	public override void Deploy(ProjectParams Params, DeploymentContext SC)
	{
		if (Params.DeviceNames.Count > 1)
		{
			throw new AutomationException("Deploying to multiple devices is not supported on Lumin. Had " + Params.DeviceNames.Count.ToString());
		}
		Deploy(Params, SC, Params.DeviceNames[0]);
	}

	private void Deploy(ProjectParams Params, DeploymentContext SC, String DeviceName)
	{
		// update the ue4commandline.txt
		// update and deploy ue4commandline.txt
		// always delete the existing commandline text file, so it doesn't reuse an old one
		string IntermediateCmdLineFile = CombinePaths(SC.StageDirectory.FullName, "UE4CommandLine.txt");
		Project.WriteStageCommandline(new FileReference(IntermediateCmdLineFile), Params, SC);

		// Where we put the files on device.
		string PackageName = GetPackageName(Params);

		// copy files to device if we were staging
		if (SC.Stage)
		{
			HashSet<string> EntriesToDeploy = new HashSet<string>();

			// @todo Lumin support iterative deploy! and packaging for iterative deploy
			if (Params.IterativeDeploy)
			{
				LogInformation("ITERATIVE DEPLOY..");
				// always send UE4CommandLine.txt (it was written above after delta checks applied)
				EntriesToDeploy.Add(IntermediateCmdLineFile);

				// Add non UFS files if any to deploy
				String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(DeviceName);
				if (File.Exists(NonUFSManifestPath))
				{
					string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
					foreach (string Filename in NonUFSFiles.Split('\n'))
					{
						if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
						{
							// Log("NonUFS: {0}", Filename);
							EntriesToDeploy.Add(CombinePaths(SC.StageDirectory.FullName, Filename.Trim()));
						}
					}
				}
				else
				{
					LogInformation("Unable to read delta file {0}", NonUFSManifestPath);
				}

				// Add UFS files if any to deploy
				String UFSManifestPath = SC.GetUFSDeploymentDeltaPath(DeviceName);
				if (File.Exists(UFSManifestPath))
				{
					string UFSFiles = File.ReadAllText(UFSManifestPath);
					foreach (string Filename in UFSFiles.Split('\n'))
					{
						if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
						{
							// Log("UFS: {0}", Filename);
							EntriesToDeploy.Add(CombinePaths(SC.StageDirectory.FullName, Filename.Trim()));
						}
					}
				}
				else
				{
					LogInformation("Unable to read delta file {0}", UFSManifestPath);
				}

				//// For now, if too many files may be better to just push them all
				//if (EntriesToDeploy.Count > 50000)
				//{
				//	Log("ITERATIVE DEPLOY: Abandoned!");

				//	CleanInstall(Params, SC, DeviceName);
				//}
				//else
				{
					// we may need to install the package so that push can deploy to that package's documents directory
					// don't do a clean install, because that would delete the documents directory!
					RunDeviceCommand(Params, DeviceName, string.Format("terminate \"{0}\"", GetPackageName(Params)), null);

					// install the package only if was (re-)created during the package step
					if (!IsPackageUpToDate(Params, SC))
					{
						IProcessResult InstallResult = RunDeviceCommand(Params, DeviceName, string.Format("install -u \"{0}\"", GetFinalMpkName(Params, SC)), null);
						if (InstallResult.ExitCode != 0)
						{
							throw new AutomationException((ExitCode)InstallResult.ExitCode, "Failed to install {0}", GetFinalMpkName(Params, SC));
						}
					}

					// cache the timestamp of the exe
					string ExePath = Params.GetProjectExeForPlatform(UnrealTargetPlatform.Lumin).ToString() + "-arm64" + GetBestGPUArchitecture(Params) + ".so";
					string ExeTimestampFileName = CombinePaths(SC.StageDirectory.FullName, "ExeTimestamp.txt");
					File.WriteAllText(ExeTimestampFileName, File.GetLastWriteTime(ExePath).ToString("O"));
					EntriesToDeploy.Add(ExeTimestampFileName);

					// mono is bugging out making stderr pipes
					bool bGoSlow = HostPlatform.Current.HostEditorPlatform != UnrealTargetPlatform.Win64;

					// We now have a minimal set of file & dir entries we need
					// to deploy. Files we deploy will get individually copied
					// and dirs will get the tree copies by default (that's
					// what MLDB does).
					HashSet<IProcessResult> DeployCommands = new HashSet<IProcessResult>();
					foreach (string Entry in EntriesToDeploy)
					{
						string RemotePath = Entry.Replace(SC.StageDirectory.FullName, PackageWritePath).Replace("\\", "/");
						string Commandline = string.Format("{0} \"{1}\" -p {2} \"{3}\"", "push", Entry, PackageName, RemotePath);
						// We run deploy commands in parallel to maximize the connection
						// throughput.
						ERunOptions Options = bGoSlow ? ERunOptions.Default : (ERunOptions.Default | ERunOptions.NoWaitForExit);
						DeployCommands.Add(
						   RunDeviceCommand(Params, DeviceName, Commandline, null, Options));
						// But we limit the parallel commands to avoid overwhelming
						// memory resources.
						if (!bGoSlow && DeployCommands.Count == DeployMaxParallelCommands)
						{
							while (DeployCommands.Count > DeployMaxParallelCommands / 2)
							{
								Thread.Sleep(1);
								DeployCommands.RemoveWhere(
									delegate (IProcessResult r)
									{
										return r.HasExited;
									});
							}
						}
					}
					foreach (ProcessResult deploy_result in DeployCommands)
					{
						deploy_result.WaitForExit();
					}
				}
			}
			else
			{
				LogInformation("CLEAN DEPLOY..");

				CleanInstall(Params, SC, DeviceName);
			}
		}
		else if (SC.Archive)
		{
			// Nothing?
		}
		else
		{
			string RemoteFilename = IntermediateCmdLineFile.Replace(SC.StageDirectory.FullName, PackageWritePath).Replace("\\", "/");
			string Commandline = string.Format("{0} \"{1}\" -p {2} \"{3}\"", "push", IntermediateCmdLineFile, PackageName, RemoteFilename);
			RunDeviceCommand(Params, DeviceName, Commandline);
		}
	}

	private void CleanInstall(ProjectParams Params, DeploymentContext SC, string DeviceName)
	{
		//ILuminDeploy Deploy = LuminExports.CreateDeploymentHandler(Params.RawProjectPath);
		string MpkName = GetFinalMpkName(Params, SC);
		string PackageName = GetPackageName(Params);

		string UninstallCmd = string.Format("uninstall {0}", PackageName);
		RunDeviceCommand(Params, DeviceName, UninstallCmd, null);

		string InstallCmd = string.Format("install \"{0}\"", MpkName);
		IProcessResult InstallResult = RunDeviceCommand(Params, DeviceName, InstallCmd, null);
		if (InstallResult.ExitCode != 0)
		{
			throw new AutomationException((ExitCode)InstallResult.ExitCode, "Failed to install {0}", GetFinalMpkName(Params, SC));
		}
	}

	// adb shell quoted arguments need to be wrapped twice - once for Windows, and once for /bin/sh, otherwise sh can get confused
	// by special characters like parenthesis
	private static string WrapQuotes(string InputString)
	{
		StringBuilder WrappedString = new StringBuilder(InputString.Length);
		bool bInQuotes = false;

		for (int i = 0; i < InputString.Length; ++i)
		{
			if (InputString[i] == '\"')
			{
				if (bInQuotes)
				{
					// if we're in quotes, escaped quote should go first
					WrappedString.Append("\\\"\"");
				}
				else
				{
					// if we're outside of quotes, escaped quote should go second
					WrappedString.Append("\"\\\"");
				}

				bInQuotes = !bInQuotes;
			}
			else
			{
				WrappedString.Append(InputString[i]);
			}
		}

		return WrappedString.ToString();
	}

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (Params.DeviceNames.Count > 1)
		{
			throw new AutomationException("Running multiple devices is not supported on Lumin. Had " + Params.DeviceNames.Count.ToString());
		}
		return RunClient(ClientRunFlags, ClientApp, ClientCmdLine, Params, Params.DeviceNames[0]);
	}

	private IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params, string DeviceName)
	{
		// If the client needs to talk to the host for UFS or cook server we need to map some ports.
		if (Params.CookOnTheFly || Params.CookOnTheFlyStreaming || Params.FileServer)
		{
			RunDeviceCommand(Params, DeviceName, "reverse tcp:41898 tcp:41898");
			RunDeviceCommand(Params, DeviceName, "reverse tcp:41899 tcp:41899");
		}

		RunDeviceCommand(Params, DeviceName, "log -c");
		RunDeviceCommand(Params, DeviceName, "log *:S UE4:D", null, ERunOptions.AppMustExist | ERunOptions.NoWaitForExit | ERunOptions.AllowSpew);

		// @todo Lumin graphics debugger
		// 		bool UseTegraGraphicsDebugger =
		// 			(LuminPlatformContext.UseTegraGraphicsDebugger(Params.RawProjectPath) &&
		// 			!LuminPlatformContext.UseTegraDebuggerStub(Params.RawProjectPath));
		bool UseTegraGraphicsDebugger = false;

		if (UseTegraGraphicsDebugger)
		{
			return RunClientWithGraphicsDebugger(ClientRunFlags, ClientApp, ClientCmdLine, Params, DeviceName);
		}
		else
		{
			//ILuminDeploy Deploy = LuminExports.CreateDeploymentHandler(Params.RawProjectPath);
			string PackageName = GetPackageName(Params);
			string Argument = "-w";
			if (!Params.CookOnTheFly && !Params.CookOnTheFlyStreaming && !Params.FileServer)
			{
				Argument = "-x";
			}
			if (Params.CookOnTheFlyStreaming || Params.CookOnTheFly)
			{
				// 'LocalAreaNetwork' privilege is required for CookOnTheFly. Being a sensitive privilege, it needs to be requested at runtime.
				// We use the --auto-net-privs launch option to bypass this requirement.
				Argument += " --auto-net-privs";
			}
			string LaunchArgs = string.Format("launch {0} {1}", Argument, PackageName);

			// HACK
			// Until we get "-f -w" support, we need to manually terminate any existing process, wait, and then launch a new one.
			string TerminateArgs = string.Format("terminate -f {0}", PackageName);
			RunDeviceCommand(Params, DeviceName, TerminateArgs, null, ERunOptions.AppMustExist);
			Thread.Sleep(1000);

			return RunDeviceCommand(Params, DeviceName, LaunchArgs, null, ERunOptions.NoWaitForExit);
		}
	}

	private IProcessResult RunClientWithGraphicsDebugger(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params, string DeviceName)
	{
		// clear the log
		RunDeviceCommand(Params, DeviceName, "log -c");

		// Where we put the files on device.
		string PackageName = GetPackageName(Params);

		// start the app on device!
		string CommandLine = "shell ";
		string RemoteBinDir = "/data/app/" + PackageName + "/bin";
		CommandLine += "LD_PRELOAD=" + RemoteBinDir + "/libNvidia_gfx_debugger.so ";
		CommandLine += RemoteBinDir + "/" + Params.ShortProjectName;
		ClientRunFlags |= ERunOptions.NoWaitForExit;
		IProcessResult ClientProcess = RunDeviceCommand(Params, DeviceName, CommandLine, null, ClientRunFlags);

		// check if the game is running
		// time out if it takes to long to start
		DateTime StartTime = DateTime.Now;
		int TimeOutSeconds = Params.RunTimeoutSeconds;

		while (true)
		{
			IProcessResult ProcessesResult = RunDeviceCommand(Params, DeviceName, "shell pgrep " + Params.ShortProjectName, null, ERunOptions.SpewIsVerbose);

			string RunningProcessID = ProcessesResult.Output;
			if (RunningProcessID.Length > 0)
			{
				break;
			}
			Thread.Sleep(10);

			TimeSpan DeltaRunTime = DateTime.Now - StartTime;
			if ((DeltaRunTime.TotalSeconds > TimeOutSeconds) && (TimeOutSeconds != 0))
			{
				LogInformation("Device: " + DeviceName + " timed out while waiting for run to finish");
				break;
			}
		}

		return ClientProcess;
	}

	public override bool UseAbsLog
	{
		get
		{
			return false;
		}
	}
	public override void PlatformSetupParams(ref ProjectParams ProjParams)
	{
		if (ProjParams.Stage && !ProjParams.SkipStage)
		{
			// Add "Installed" indication to command line when we use "mldb install"
			// as that will prevent UE4 from trying to write to the engine install location.
			// Which is not permitted by the sandbox.
			// @todo Lumin graphics debugger
			bool UseTegraGraphicsDebugger = false;
			// 			bool UseTegraGraphicsDebugger =
			// 				(LuminPlatformContext.UseTegraGraphicsDebugger(ProjParams.RawProjectPath) &&
			// 				!LuminPlatformContext.UseTegraDebuggerStub(ProjParams.RawProjectPath));
			if (!UseTegraGraphicsDebugger)
			{
				ProjParams.RunCommandline += " -Installed";
			}
		}
	}

	public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
	{
		LuminExports.StripSymbols(SourceFile, TargetFile);
	}

	#endregion

	#region Implementation Details

	private string GetDeviceCommandLine(ProjectParams Params, string SerialNumber, string Args)
	{
		if (SerialNumber != "")
		{
			SerialNumber = "-s " + SerialNumber;
		}

		return string.Format("{0} {1}", SerialNumber, Args);
	}

	static string LastSpewFilename = "";

	public string ADBSpewFilter(string Message)
	{
		if (Message.StartsWith("[") && Message.Contains("%]"))
		{
			int LastIndex = Message.IndexOf(":");
			LastIndex = LastIndex == -1 ? Message.Length : LastIndex;

			if (Message.Length > 7)
			{
				string Filename = Message.Substring(7, LastIndex - 7);
				if (Filename == LastSpewFilename)
				{
					return null;
				}
				LastSpewFilename = Filename;
			}
			return Message;
		}
		return Message;
	}

	private string DeviceCommand
	{
		get
		{
			var envValue = Environment.GetEnvironmentVariable("MLSDK");

			if (String.IsNullOrEmpty(envValue))
			{
				throw new AutomationException(ExitCode.Error_AndroidBuildToolsPathNotFound, "Failed to find a %MLSDK% directory. Please set MLSDK environment variable to point to ML SDK.");
			}

			switch (Environment.OSVersion.Platform)
			{
				case PlatformID.MacOSX:
					return Path.Combine(
						envValue,
						"tools", "mldb", "mldb");

				case PlatformID.Unix:
					return Path.Combine(
						envValue,
						"tools", "mldb", "mldb");

				default:
					return Path.Combine(
						envValue,
						"tools", "mldb", "mldb.exe");
			}
		}
	}

	private IProcessResult RunDeviceCommand(ProjectParams Params, string SerialNumber, string Args, string Input = null, ERunOptions Options = ERunOptions.Default)
	{
		if (Options.HasFlag(ERunOptions.AllowSpew) || Options.HasFlag(ERunOptions.SpewIsVerbose))
		{
			LastSpewFilename = "";
			return Run(DeviceCommand, GetDeviceCommandLine(Params, SerialNumber, Args), Input, Options, SpewFilterCallback: new ProcessResult.SpewFilterCallbackType(ADBSpewFilter));
		}
		return Run(DeviceCommand, GetDeviceCommandLine(Params, SerialNumber, Args), Input, Options);
	}

	private string RunAndLogDeviceCommand(ProjectParams Params, string SerialNumber, string Args, out int SuccessCode)
	{
		LastSpewFilename = "";
		return RunAndLog(CmdEnv, DeviceCommand, GetDeviceCommandLine(Params, SerialNumber, Args), out SuccessCode, SpewFilterCallback: new ProcessResult.SpewFilterCallbackType(ADBSpewFilter));
	}

	private const int DeployMaxParallelCommands = 6;

	private string GetBestGPUArchitecture(ProjectParams Params)
	{
		var AppGPUArchitectures = LuminExports.CreateToolChain(Params.RawProjectPath).GetAllGPUArchitectures();

		if (AppGPUArchitectures.Contains("-lumingl4"))
		{
			return "-lumingl4";
		}

		return "-lumin";
	}

	private List<string> GetConfigurations(DeploymentContext SC)
	{
		List<string> Configurations = new List<string>();
		foreach (UnrealTargetConfiguration C in SC.StageTargetConfigurations)
		{
			switch (C)
			{
				case UnrealTargetConfiguration.Debug:
				case UnrealTargetConfiguration.DebugGame:
					Configurations.Add("-" + SC.PlatformDir + "-Debug");
					break;

				case UnrealTargetConfiguration.Shipping:
					Configurations.Add("-" + SC.PlatformDir + "-Shipping");
					break;

				case UnrealTargetConfiguration.Test:
					Configurations.Add("-" + SC.PlatformDir + "-Test");
					break;

				default:
					Configurations.Add("");
					break;
			}
		}
		return Configurations;
	}


	#endregion
}