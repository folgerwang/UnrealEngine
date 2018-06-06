// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using Microsoft.Win32;
using System.Diagnostics;
using Tools.DotNETCommon;

public abstract class BaseWinPlatform : Platform
{
	public BaseWinPlatform(UnrealTargetPlatform P)
		: base(P)
	{
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// Engine non-ufs (binaries)

		if (SC.bStageCrashReporter)
		{
			FileReference ReceiptFileName = TargetReceipt.GetDefaultPath(CommandUtils.EngineDirectory, "CrashReportClient", SC.StageTargetPlatform.PlatformType, UnrealTargetConfiguration.Shipping, null);
			if(FileReference.Exists(ReceiptFileName))
			{
				DirectoryReference EngineDir = CommandUtils.EngineDirectory;
				DirectoryReference ProjectDir = DirectoryReference.FromFile(Params.RawProjectPath);
				TargetReceipt Receipt = TargetReceipt.Read(ReceiptFileName, EngineDir, ProjectDir);
				SC.StageBuildProductsFromReceipt(Receipt, true, false);
			}
		}

		// Stage all the build products
		foreach(StageTarget Target in SC.StageTargets)
		{
			SC.StageBuildProductsFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.bTreatNonShippingBinariesAsDebugFiles);
		}

		// Copy the splash screen, windows specific
		FileReference SplashImage = FileReference.Combine(SC.ProjectRoot, "Content", "Splash", "Splash.bmp");
		if(FileReference.Exists(SplashImage))
		{
			SC.StageFile(StagedFileType.NonUFS, SplashImage);
		}

		// Stage the bootstrap executable
		if(!Params.NoBootstrapExe)
		{
			foreach(StageTarget Target in SC.StageTargets)
			{
				BuildProduct Executable = Target.Receipt.BuildProducts.FirstOrDefault(x => x.Type == BuildProductType.Executable);
				if(Executable != null)
				{
					// only create bootstraps for executables
					List<StagedFileReference> StagedFiles = SC.FilesToStage.NonUFSFiles.Where(x => x.Value == Executable.Path).Select(x => x.Key).ToList();
					if (StagedFiles.Count > 0 && Executable.Path.HasExtension(".exe"))
					{
						string BootstrapArguments = "";
						if (!ShouldStageCommandLine(Params, SC))
						{
							if (!SC.IsCodeBasedProject)
							{
								BootstrapArguments = String.Format("..\\..\\..\\{0}\\{0}.uproject", SC.ShortProjectName);
							}
							else
							{
								BootstrapArguments = SC.ShortProjectName;
							}
						}

						string BootstrapExeName;
						if(SC.StageTargetConfigurations.Count > 1)
						{
							BootstrapExeName = Executable.Path.GetFileName();
						}
						else if(Params.IsCodeBasedProject)
						{
							BootstrapExeName = Target.Receipt.TargetName + ".exe";
						}
						else
						{
							BootstrapExeName = SC.ShortProjectName + ".exe";
						}

						foreach (StagedFileReference StagePath in StagedFiles)
						{
							StageBootstrapExecutable(SC, BootstrapExeName, Executable.Path, StagePath, BootstrapArguments);
						}
					}
				}
			}
		}
	}

    public override void ExtractPackage(ProjectParams Params, string SourcePath, string DestinationPath)
    {
    }

	public override void GetTargetFile(string RemoteFilePath, string LocalFile, ProjectParams Params)
	{
		var SourceFile = FileReference.Combine(new DirectoryReference(Params.BaseStageDirectory), GetCookPlatform(Params.HasServerCookedTargets, Params.HasClientTargetDetected), RemoteFilePath);
		CommandUtils.CopyFile(SourceFile.FullName, LocalFile);
	}

	void StageBootstrapExecutable(DeploymentContext SC, string ExeName, FileReference TargetFile, StagedFileReference StagedRelativeTargetPath, string StagedArguments)
	{
		FileReference InputFile = FileReference.Combine(SC.LocalRoot, "Engine", "Binaries", SC.PlatformDir, String.Format("BootstrapPackagedGame-{0}-Shipping.exe", SC.PlatformDir));
		if(FileReference.Exists(InputFile))
		{
			// Create the new bootstrap program
			DirectoryReference IntermediateDir = DirectoryReference.Combine(SC.ProjectRoot, "Intermediate", "Staging");
			DirectoryReference.CreateDirectory(IntermediateDir);

			FileReference IntermediateFile = FileReference.Combine(IntermediateDir, ExeName);
			CommandUtils.CopyFile(InputFile.FullName, IntermediateFile.FullName);
			CommandUtils.SetFileAttributes(IntermediateFile.FullName, ReadOnly: false);
	
			// currently the icon updating doesn't run under mono
			if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 ||
				UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32)
			{
				// Get the icon from the build directory if possible
				GroupIconResource GroupIcon = null;
				if(FileReference.Exists(FileReference.Combine(SC.ProjectRoot, "Build/Windows/Application.ico")))
				{
					GroupIcon = GroupIconResource.FromIco(FileReference.Combine(SC.ProjectRoot, "Build/Windows/Application.ico").FullName);
				}
				if(GroupIcon == null)
				{
					GroupIcon = GroupIconResource.FromExe(TargetFile.FullName);
				}

				// Update the resources in the new file
				using(ModuleResourceUpdate Update = new ModuleResourceUpdate(IntermediateFile.FullName, false))
				{
					const int IconResourceId = 101;
					if(GroupIcon != null) Update.SetIcons(IconResourceId, GroupIcon);

					const int ExecFileResourceId = 201;
					Update.SetData(ExecFileResourceId, ResourceType.RawData, Encoding.Unicode.GetBytes(StagedRelativeTargetPath + "\0"));

					const int ExecArgsResourceId = 202;
					Update.SetData(ExecArgsResourceId, ResourceType.RawData, Encoding.Unicode.GetBytes(StagedArguments + "\0"));
				}
			}

			// Copy it to the staging directory
			SC.StageFile(StagedFileType.SystemNonUFS, IntermediateFile, new StagedFileReference(ExeName));
		}
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		const string NoEditorCookPlatform = "WindowsNoEditor";
		const string ServerCookPlatform = "WindowsServer";
		const string ClientCookPlatform = "WindowsClient";

		if (bDedicatedServer)
		{
			return ServerCookPlatform;
		}
		else if (bIsClientOnly)
		{
			return ClientCookPlatform;
		}
		else
		{
			return NoEditorCookPlatform;
		}
	}

	public override string GetEditorCookPlatform()
	{
		return "Windows";
	}

    public override string GetPlatformPakCommandLine(ProjectParams Params, DeploymentContext SC)
    {
        return " -patchpaddingalign=2048";
    }

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
        List<FileReference> ExeNames = GetExecutableNames(SC);

        // Select target configurations based on the exe list returned from GetExecutableNames
        List<UnrealTargetConfiguration> TargetConfigs = SC.StageTargetConfigurations.GetRange(0, ExeNames.Count);

		if (!Params.HasDLCName)
		{
			WindowsExports.SetApplicationIcon(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, TargetConfigs, ExeNames, SC.EngineRoot);
		}

		// package up the program, potentially with an installer for Windows
		PrintRunTime();
	}

	public override bool UseAbsLog
	{
		get { return BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win32; }
	}

	public override bool CanHostPlatform(UnrealTargetPlatform Platform)
	{
		if (Platform == UnrealTargetPlatform.Mac)
		{
			return false;
		}
		return true;
	}

    public override bool ShouldStageCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		return false; // !String.IsNullOrEmpty(Params.StageCommandline) || !String.IsNullOrEmpty(Params.RunCommandline) || (!Params.IsCodeBasedProject && Params.NoBootstrapExe);
	}

	public override List<string> GetDebugFileExtensions()
	{
		return new List<string> { ".pdb", ".map" };
	}

	public override bool SignExecutables(DeploymentContext SC, ProjectParams Params)
	{
		// Sign everything we built
		List<FileReference> FilesToSign = GetExecutableNames(SC);
		CodeSign.SignMultipleFilesIfEXEOrDLL(FilesToSign);

		return true;
	}

	public void StageAppLocalDependencies(ProjectParams Params, DeploymentContext SC, string PlatformDir)
	{
		Dictionary<string, string> PathVariables = new Dictionary<string, string>();
		PathVariables["EngineDir"] = SC.EngineRoot.FullName;
		PathVariables["ProjectDir"] = SC.ProjectRoot.FullName;

		// support multiple comma-separated paths
		string[] AppLocalDirectories = Params.AppLocalDirectory.Split(';');
		foreach (string AppLocalDirectory in AppLocalDirectories)
		{
			string ExpandedAppLocalDir = Utils.ExpandVariables(AppLocalDirectory, PathVariables);

			DirectoryReference BaseAppLocalDependenciesPath = Path.IsPathRooted(ExpandedAppLocalDir) ? new DirectoryReference(CombinePaths(ExpandedAppLocalDir, PlatformDir)) : DirectoryReference.Combine(SC.ProjectRoot, ExpandedAppLocalDir, PlatformDir);
			if (DirectoryReference.Exists(BaseAppLocalDependenciesPath))
			{
				StageAppLocalDependenciesToDir(SC, BaseAppLocalDependenciesPath, StagedDirectoryReference.Combine("Engine", "Binaries", PlatformDir));
				StageAppLocalDependenciesToDir(SC, BaseAppLocalDependenciesPath, StagedDirectoryReference.Combine(SC.RelativeProjectRootForStage, "Binaries", PlatformDir));
			}
			else
			{
				LogWarning("Unable to deploy AppLocalDirectory dependencies. No such path: {0}", BaseAppLocalDependenciesPath);
			}
		}
	}

	static void StageAppLocalDependenciesToDir(DeploymentContext SC, DirectoryReference BaseAppLocalDependenciesPath, StagedDirectoryReference StagedBinariesDir)
	{
		// Check if there are any executables being staged in this directory. Usually we only need to stage runtime dependencies next to the executable, but we may be staging
		// other engine executables too (eg. CEF)
		List<StagedFileReference> FilesInTargetDir = SC.FilesToStage.NonUFSFiles.Keys.Where(x => x.IsUnderDirectory(StagedBinariesDir) && (x.CanonicalName.EndsWith(".exe") || x.CanonicalName.EndsWith(".dll"))).ToList();
		if(FilesInTargetDir.Count > 0)
		{
			Log("Copying AppLocal dependencies from {0} to {1}", BaseAppLocalDependenciesPath, StagedBinariesDir);

			// Stage files in subdirs
			foreach (DirectoryReference DependencyDirectory in DirectoryReference.EnumerateDirectories(BaseAppLocalDependenciesPath))
			{	
				SC.StageFiles(StagedFileType.NonUFS, DependencyDirectory, StageFilesSearch.AllDirectories, StagedBinariesDir);
			}
		}
	}

    /// <summary>
    /// Try to get the SYMSTORE.EXE path from the given Windows SDK version
    /// </summary>
    /// <param name="SdkVersion">The SDK version string</param>
    /// <param name="SymStoreExe">Receives the path to symstore.exe if found</param>
    /// <returns>True if found, false otherwise</returns>
    private static bool TryGetSymStoreExe(string SdkVersion, out FileReference SymStoreExe)
    {
        // Try to get the SDK installation directory
        string SdkFolder = Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Microsoft\Microsoft SDKs\Windows\" + SdkVersion, "InstallationFolder", null) as String;
        if (SdkFolder == null)
        {
            SdkFolder = Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + SdkVersion, "InstallationFolder", null) as String;
            if (SdkFolder == null)
            {
                SdkFolder = Registry.GetValue(@"HKEY_CURRENT_USER\SOFTWARE\Wow6432Node\Microsoft\Microsoft SDKs\Windows\" + SdkVersion, "InstallationFolder", null) as String;
                if (SdkFolder == null)
                {
                    SymStoreExe = null;
                    return false;
                }
            }
        }

        // Check for the 64-bit toolchain first, then the 32-bit toolchain
        FileReference CheckSymStoreExe = FileReference.Combine(new DirectoryReference(SdkFolder), "Debuggers", "x64", "SymStore.exe");
        if (!FileReference.Exists(CheckSymStoreExe))
        {
            CheckSymStoreExe = FileReference.Combine(new DirectoryReference(SdkFolder), "Debuggers", "x86", "SymStore.exe");
            if (!FileReference.Exists(CheckSymStoreExe))
            {
                SymStoreExe = null;
                return false;
            }
        }

        SymStoreExe = CheckSymStoreExe;
        return true;
    }

	public static bool TryGetPdbCopyLocation(out FileReference OutLocation)
	{
		// Try to find an installation of the Windows 10 SDK
		string SdkInstallFolder =
			(Microsoft.Win32.Registry.GetValue("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", null) as string) ??
			(Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", null) as string) ??
			(Microsoft.Win32.Registry.GetValue("HKEY_CURRENT_USER\\Software\\Wow6432Node\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", null) as string) ??
			(Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Microsoft\\Windows Kits\\Installed Roots", "KitsRoot10", null) as string);

		if(!String.IsNullOrEmpty(SdkInstallFolder))
		{
			FileReference Location = FileReference.Combine(new DirectoryReference(SdkInstallFolder), "Debuggers", "x64", "PDBCopy.exe");
			if(FileReference.Exists(Location))
			{
				OutLocation = Location;
				return true;
			}
		}

		// Look for an installation of the MSBuild 14
		FileReference LocationMsBuild14 = FileReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.ProgramFilesX86), "MSBuild", "Microsoft", "VisualStudio", "v14.0", "AppxPackage", "PDBCopy.exe");
		if(FileReference.Exists(LocationMsBuild14))
		{
			OutLocation = LocationMsBuild14;
			return true;
		}

		// Look for an installation of the MSBuild 12
		FileReference LocationMsBuild12 = FileReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.ProgramFilesX86), "MSBuild", "Microsoft", "VisualStudio", "v12.0", "AppxPackage", "PDBCopy.exe");
		if(FileReference.Exists(LocationMsBuild12))
		{
			OutLocation = LocationMsBuild12;
			return true;
		}

		// Otherwise fail
		OutLocation = null;
		return false;
	}

	public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
	{
		bool bStripInPlace = false;

		if (SourceFile == TargetFile)
		{
			// PDBCopy only supports creation of a brand new stripped file so we have to create a temporary filename
			TargetFile = new FileReference(Path.Combine(TargetFile.Directory.FullName, Guid.NewGuid().ToString() + TargetFile.GetExtension()));
			bStripInPlace = true;
		}

		FileReference PdbCopyLocation;
		if(!TryGetPdbCopyLocation(out PdbCopyLocation))
		{
			throw new AutomationException("Unable to find installation of PDBCOPY.EXE, which is required to strip symbols. This tool is included as part of the 'Windows Debugging Tools' component of the Windows 10 SDK (https://developer.microsoft.com/en-us/windows/downloads/windows-10-sdk).");
		}

		ProcessStartInfo StartInfo = new ProcessStartInfo();
		StartInfo.FileName = PdbCopyLocation.FullName;
		StartInfo.Arguments = String.Format("\"{0}\" \"{1}\" -p", SourceFile.FullName, TargetFile.FullName);
		StartInfo.UseShellExecute = false;
		StartInfo.CreateNoWindow = true;
		Utils.RunLocalProcessAndLogOutput(StartInfo);

		if (bStripInPlace)
		{
			// Copy stripped file to original location and delete the temporary file
			File.Copy(TargetFile.FullName, SourceFile.FullName, true);
			FileReference.Delete(TargetFile);
		}
	}

    public override bool PublishSymbols(DirectoryReference SymbolStoreDirectory, List<FileReference> Files, string Product)
    {
        // Get the SYMSTORE.EXE path, using the latest SDK version we can find.
        FileReference SymStoreExe;
        if (!TryGetSymStoreExe("v10.0", out SymStoreExe) && !TryGetSymStoreExe("v8.1", out SymStoreExe) && !TryGetSymStoreExe("v8.0", out SymStoreExe))
        {
            CommandUtils.LogError("Couldn't find SYMSTORE.EXE in any Windows SDK installation");
            return false;
        }

		List<FileReference> FilesToAdd = Files.Where(x => x.HasExtension(".pdb") || x.HasExtension(".exe") || x.HasExtension(".dll")).ToList();
		if(FilesToAdd.Count > 0)
		{
			string TempFileName = Path.GetTempFileName();
			try
			{
				File.WriteAllLines(TempFileName, FilesToAdd.Select(x => x.FullName), Encoding.ASCII);

				ProcessStartInfo StartInfo = new ProcessStartInfo();
				StartInfo.FileName = SymStoreExe.FullName;
				StartInfo.Arguments = string.Format("add /f \"@{0}\" /s \"{1}\" /t \"{2}\" /compress", TempFileName, SymbolStoreDirectory.FullName, Product);
				StartInfo.UseShellExecute = false;
				StartInfo.CreateNoWindow = true;
				if (Utils.RunLocalProcessAndLogOutput(StartInfo) != 0)
				{
					return false;
				}
			}
			finally
			{
				File.Delete(TempFileName);
			}
        }

		return true;
    }

    public override string[] SymbolServerDirectoryStructure
    {
        get
        {
            return new string[]
            {
                "{0}*.pdb;{0}*.exe;{0}*.dll", // Binary File Directory (e.g. QAGameClient-Win64-Test.exe --- .pdb, .dll and .exe are allowed extensions)
                "*",                          // Hash Directory        (e.g. A92F5744D99F416EB0CCFD58CCE719CD1)
            };
        }
    }
}

public class Win64Platform : BaseWinPlatform
{
	public Win64Platform()
		: base(UnrealTargetPlatform.Win64)
	{
	}

	public override bool IsSupported { get { return true; } }

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		base.GetFilesToDeployOrStage(Params, SC);
		
		if(Params.Prereqs)
		{
			SC.StageFile(StagedFileType.NonUFS, FileReference.Combine(SC.EngineRoot, "Extras", "Redist", "en-us", "UE4PrereqSetup_x64.exe"));
		}

		if (!string.IsNullOrWhiteSpace(Params.AppLocalDirectory))
		{
			StageAppLocalDependencies(Params, SC, "Win64");
		}
	}
}

public class Win32Platform : BaseWinPlatform
{
	public Win32Platform()
		: base(UnrealTargetPlatform.Win32)
	{
	}

	public override bool IsSupported { get { return true; } }

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		base.GetFilesToDeployOrStage(Params, SC);

		if (Params.Prereqs)
		{
			SC.StageFile(StagedFileType.NonUFS, FileReference.Combine(SC.EngineRoot, "Extras", "Redist", "en-us", "UE4PrereqSetup_x86.exe"));
		}

		if (!string.IsNullOrWhiteSpace(Params.AppLocalDirectory))
		{
			StageAppLocalDependencies(Params, SC, "Win32");
		}
	}
}
