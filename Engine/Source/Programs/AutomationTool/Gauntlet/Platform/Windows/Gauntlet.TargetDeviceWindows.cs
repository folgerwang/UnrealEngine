// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;

namespace Gauntlet
{

	public abstract class LocalAppProcess : IAppInstance
	{
		public IProcessResult ProcessResult { get; private set; }

		public bool HasExited { get { return ProcessResult.HasExited; } }

		public bool WasKilled { get; protected set; }

		public string StdOut { get { return ProcessResult.Output; } }

		public int ExitCode { get { return ProcessResult.ExitCode; } }

		public string CommandLine { get; private set; }

		public LocalAppProcess(IProcessResult InProcess, string InCommandLine)
		{
			this.CommandLine = InCommandLine;
			this.ProcessResult = InProcess;
		}
		public int WaitForExit()
		{
			if (!HasExited)
			{
				ProcessResult.WaitForExit();
			}

			return ExitCode;
		}

		public void Kill()
		{
			if (!HasExited)
			{
				WasKilled = true;
				ProcessResult.ProcessObject.Kill();
			}
		}

		public abstract string ArtifactPath { get; }

		public abstract ITargetDevice Device { get; }
		
	}

	class WindowsAppInstance : LocalAppProcess
	{
		protected WindowsAppInstall Install;

		public WindowsAppInstance(WindowsAppInstall InInstall, IProcessResult InProcess)
			: base(InProcess, InInstall.CommandArguments)
		{
			Install = InInstall;
		}

		public override string ArtifactPath
		{
			get
			{
				return Install.ArtifactPath;
			}
		}

		public override ITargetDevice Device
		{
			get
			{
				return Install.Device;
			}
		}
	}


	class WindowsAppInstall : IAppInstall
	{
		public string Name { get; private set; }

		public string WorkingDirectory;

		public string ExecutablePath;

		public string CommandArguments;

		public string ArtifactPath;

		public TargetDeviceWindows WinDevice { get; private set; }

		public ITargetDevice Device { get { return WinDevice; } }

		public CommandUtils.ERunOptions RunOptions { get; set; }

		public WindowsAppInstall(string InName, TargetDeviceWindows InDevice)
		{
			Name = InName;
			WinDevice = InDevice;
			CommandArguments = "";
			this.RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
		}

		public IAppInstance Run()
		{
			return Device.Run(this);
		}
	}

	public class WindowsDeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform Platform)
		{
			return Platform == UnrealTargetPlatform.Win64;
		}

		public ITargetDevice CreateDevice(string InRef, string InParam)
		{
			return new TargetDeviceWindows(InRef, InParam);
		}
	}

	/// <summary>
	/// Win32/64 implementation of a device to run applications
	/// </summary>
	public class TargetDeviceWindows : ITargetDevice
	{
		public string Name { get; protected set; }

		protected string UserDir { get; set; }

		/// <summary>
		/// Our mappings of Intended directories to where they actually represent on this platform.
		/// </summary>
		protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }

		public TargetDeviceWindows(string InName, string InTempDir)
		{
			Name = InName;
			TempDir = InTempDir;
			RunOptions = CommandUtils.ERunOptions.NoWaitForExit | CommandUtils.ERunOptions.NoLoggingOfRunCommand;

			UserDir = Path.Combine(TempDir, string.Format("{0}_UserDir", Name));
            LocalDirectoryMappings = new Dictionary<EIntendedBaseCopyDirectory, string>();
		}

		#region IDisposable Support
		private bool disposedValue = false; // To detect redundant calls

		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					// TODO: dispose managed state (managed objects).
				}

				// TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
				// TODO: set large fields to null.

				disposedValue = true;
			}
		}

		// This code added to correctly implement the disposable pattern.
		public void Dispose()
		{
			// Do not change this code. Put cleanup code in Dispose(bool disposing) above.
			Dispose(true);
			// TODO: uncomment the following line if the finalizer is overridden above.
			// GC.SuppressFinalize(this);
		}
		#endregion

		public CommandUtils.ERunOptions RunOptions { get; set; }

        // We care about UserDir in windows as some of the roles may require files going into user instead of build dir.
        public void PopulateDirectoryMappings(string BasePath, string UserDir)
        {
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Binaries, Path.Combine(BasePath, "Binaries"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Config, Path.Combine(BasePath, "Saved", "Config"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Content, Path.Combine(BasePath, "Content"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Demos, Path.Combine(UserDir, "Saved", "Demos"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Profiling, Path.Combine(BasePath, "Saved", "Profiling"));
            LocalDirectoryMappings.Add(EIntendedBaseCopyDirectory.Saved, Path.Combine(BasePath, "Saved"));
        }

        public IAppInstance Run(IAppInstall App)
		{
			WindowsAppInstall WinApp = App as WindowsAppInstall;

			if (WinApp == null)
			{
				throw new DeviceException("AppInstance is of incorrect type!");
			}

			if (File.Exists(WinApp.ExecutablePath) == false)
			{
				throw new DeviceException("Specified path {0} not found!", WinApp.ExecutablePath);
			}

			IProcessResult Result = null;

			lock (Globals.MainLock)
			{
				string ExePath = Path.GetDirectoryName(WinApp.ExecutablePath);
				string NewWorkingDir = string.IsNullOrEmpty(WinApp.WorkingDirectory) ? ExePath : WinApp.WorkingDirectory;
				string OldWD = Environment.CurrentDirectory;
				Environment.CurrentDirectory = NewWorkingDir;

				Log.Info("Launching {0} on {1}", App.Name, ToString());
				Log.Verbose("\t{0}", WinApp.CommandArguments);

				Result = CommandUtils.Run(WinApp.ExecutablePath, WinApp.CommandArguments, Options: WinApp.RunOptions);

				if (Result.HasExited && Result.ExitCode != 0)
				{
					throw new AutomationException("Failed to launch {0}. Error {1}", WinApp.ExecutablePath, Result.ExitCode);
				}

				Environment.CurrentDirectory = OldWD;
			}

			return new WindowsAppInstance(WinApp, Result);
		}

		protected IAppInstall InstallStagedBuild(UnrealAppConfig AppConfig, StagedBuild InBuild)
		{
			bool SkipDeploy = Globals.Params.ParseParam("SkipDeploy");

			string BuildPath = InBuild.BuildPath;

			if (CanRunFromPath(BuildPath) == false)
			{
				string SubDir = string.IsNullOrEmpty(AppConfig.Sandbox) ? AppConfig.ProjectName : AppConfig.Sandbox;
				string DestPath = Path.Combine(this.TempDir, SubDir, AppConfig.ProcessType.ToString());

				if (!SkipDeploy)
				{
					Log.Info("Installing {0} to {1}", AppConfig.Name, ToString());
					Log.Verbose("\tCopying {0} to {1}", BuildPath, DestPath);
					Gauntlet.Utils.SystemHelpers.CopyDirectory(BuildPath, DestPath, Utils.SystemHelpers.CopyOptions.Mirror);
				}
				else
				{
					Log.Info("Skipping install of {0} (-skipdeploy)", BuildPath);
				}

				// write a token, used to detect and old gauntlet-installedbuilds periodically
				string TokenPath = Path.Combine(DestPath, "gauntlet.token");
				File.WriteAllText(TokenPath, "Created by Gauntlet");

				BuildPath = DestPath;
			}

			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, this);
			WinApp.RunOptions = RunOptions;

			// Set commandline replace any InstallPath arguments with the path we use
			WinApp.CommandArguments = Regex.Replace(AppConfig.CommandLine, @"\$\(InstallPath\)", BuildPath, RegexOptions.IgnoreCase);

			if (string.IsNullOrEmpty(UserDir) == false)
			{
				WinApp.CommandArguments += string.Format(" -userdir={0}", UserDir);
				WinApp.ArtifactPath = Path.Combine(UserDir, @"Saved");
			}
			else
			{
				// e.g d:\Unreal\GameName\Saved
				WinApp.ArtifactPath = Path.Combine(BuildPath, AppConfig.ProjectName, @"Saved");

			}

			// clear artifact path
			if (Directory.Exists(WinApp.ArtifactPath))
			{
				try
				{
					Directory.Delete(WinApp.ArtifactPath, true);
				}
				catch (Exception Ex)
				{
					Log.Warning("Failed to delete {0}. {1}", WinApp.ArtifactPath, Ex.Message);
				}
			}

            if (LocalDirectoryMappings.Count == 0)
            {
                PopulateDirectoryMappings(Path.Combine(BuildPath, AppConfig.ProjectName), UserDir);
            }

            if (AppConfig.FilesToCopy != null)
            {
                foreach (UnrealFileToCopy FileToCopy in AppConfig.FilesToCopy)
                {
                    string PathToCopyTo = Path.Combine(LocalDirectoryMappings[FileToCopy.TargetBaseDirectory], FileToCopy.TargetRelativeLocation);
                    if (File.Exists(FileToCopy.SourceFileLocation))
                    {
                        FileInfo SrcInfo = new FileInfo(FileToCopy.SourceFileLocation);
                        SrcInfo.IsReadOnly = false;
                        string DirectoryToCopyTo = Path.GetDirectoryName(PathToCopyTo);
                        if (!Directory.Exists(DirectoryToCopyTo))
                        {
                            Directory.CreateDirectory(DirectoryToCopyTo);
                        }
                        if (File.Exists(PathToCopyTo))
                        {
                            FileInfo ExistingFile = new FileInfo(PathToCopyTo);
                            ExistingFile.IsReadOnly = false;
                        }
                        SrcInfo.CopyTo(PathToCopyTo, true);
                        Log.Info("Copying {0} to {1}", FileToCopy.SourceFileLocation, PathToCopyTo);
                    }
                    else
                    {
                        Log.Warning("File to copy {0} not found", FileToCopy);
                    }
                }
            }

            if (Path.IsPathRooted(InBuild.ExecutablePath))
			{
				WinApp.ExecutablePath = InBuild.ExecutablePath;
			}
			else
			{
				// TODO - this check should be at a higher level....
				string BinaryPath = Path.Combine(BuildPath, InBuild.ExecutablePath);

				// check for a local newer executable
				if (Globals.Params.ParseParam("dev") && AppConfig.ProcessType.UsesEditor() == false)
				{
					string LocalBinary = Path.Combine(Environment.CurrentDirectory, InBuild.ExecutablePath);

					bool LocalFileExists = File.Exists(LocalBinary);
					bool LocalFileNewer = LocalFileExists && File.GetLastWriteTime(LocalBinary) > File.GetLastWriteTime(BinaryPath);

					Log.Verbose("Checking for newer binary at {0}", LocalBinary);
					Log.Verbose("LocalFile exists: {0}. Newer: {1}", LocalFileExists, LocalFileNewer);

					if (LocalFileExists && LocalFileNewer)
					{
						// need to -basedir to have our exe load content from the path
						WinApp.CommandArguments += string.Format(" -basedir={0}", Path.GetDirectoryName(BinaryPath));

						BinaryPath = LocalBinary;
					}
				}

				WinApp.ExecutablePath = BinaryPath;
			}

			return WinApp;
		}


		public IAppInstall InstallApplication(UnrealAppConfig AppConfig)
		{
			if (AppConfig.Build is StagedBuild)
			{
				return InstallStagedBuild(AppConfig, AppConfig.Build as StagedBuild);
			}

			EditorBuild EditorBuild = AppConfig.Build as EditorBuild;

			if (EditorBuild == null)
			{
				throw new AutomationException("Invalid build type!");
			}

			WindowsAppInstall WinApp = new WindowsAppInstall(AppConfig.Name, this);

			WinApp.WorkingDirectory = Path.GetDirectoryName(EditorBuild.ExecutablePath);
			WinApp.RunOptions = RunOptions;
	
			// Force this to stop logs and other artifacts going to different places
			WinApp.CommandArguments = AppConfig.CommandLine + string.Format(" -userdir={0}", UserDir);
			WinApp.ArtifactPath = Path.Combine(UserDir, @"Saved");
			WinApp.ExecutablePath = EditorBuild.ExecutablePath;

			return WinApp;
		}
		
		public bool CanRunFromPath(string InPath)
		{
			// path must be under our mapped drive (e.g. w:\KitName);
			return string.Compare(Path.GetPathRoot(InPath), Path.GetPathRoot(this.TempDir), StringComparison.OrdinalIgnoreCase) == 0;
		}

		public UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.Win64; } }

		public string TempDir { get; private set; }
		public bool IsAvailable { get { return true; } }
		public bool IsConnected { get { return true; } }
		public bool IsOn { get { return true; } }
		public bool PowerOn() { return true; }
		public bool PowerOff() { return true; }
		public bool Reboot() { return true; }
		public bool Connect() { return true; }
		public bool Disconnect() { return true; }

		public override string ToString()
		{
			return Name;
		}

		public Dictionary<EIntendedBaseCopyDirectory, string> GetPlatformDirectoryMappings()
		{
			if (LocalDirectoryMappings.Count == 0)
			{
				Log.Warning("Platform directory mappings have not been populated for this platform! This should be done within InstallApplication()");
			}
			return LocalDirectoryMappings;
		}
	}
}