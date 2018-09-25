// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;

namespace Gauntlet
{
	
	public class MacDeviceFactory : IDeviceFactory
	{
		public bool CanSupportPlatform(UnrealTargetPlatform Platform)
		{
			return Platform == UnrealTargetPlatform.Mac;
		}

		public ITargetDevice CreateDevice(string InRef, string InParam)
		{
			return new TargetDeviceMac(InRef, InParam);
		}
	}

	class MacAppInstall : IAppInstall
	{
		public string Name { get; private set; }

		public string LocalPath;

		public string WorkingDirectory;

		public string ExecutablePath;

		public string CommandArguments;

		public string ArtifactPath;

		public ITargetDevice Device { get; protected set; }

		public CommandUtils.ERunOptions RunOptions { get; set; }

		public MacAppInstall(string InName, TargetDeviceMac InDevice)
		{
			Name = InName;
			Device = InDevice;
			CommandArguments = "";
			this.RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
		}

		public IAppInstance Run()
		{
			return Device.Run(this);
		}
	}

	class MacAppInstance : LocalAppProcess
	{
		protected MacAppInstall Install;

		public MacAppInstance(MacAppInstall InInstall, IProcessResult InProcess)
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

	public class TargetDeviceMac : ITargetDevice
	{
		public string Name { get; protected set; }

		public UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.Mac; } }

		public bool IsAvailable { get { return true; } }
		public bool IsConnected { get { return true; } }
		public bool IsOn { get { return true; } }
		public bool PowerOn() { return true; }
		public bool PowerOff() { return true; }
		public bool Reboot() { return true; }
		public bool Connect() { return true; }
		public bool Disconnect() { return true; }

		protected string UserDir { get; set; }

		protected string TempDir { get; set; }

		public CommandUtils.ERunOptions RunOptions { get; set; }

		protected Dictionary<EIntendedBaseCopyDirectory, string> LocalDirectoryMappings { get; set; }

		public TargetDeviceMac(string InName, string InTempDir)
		{
			Name = InName;
			TempDir = InTempDir;
			UserDir = Path.Combine(TempDir, string.Format("{0}_UserDir", Name));
			this.RunOptions = CommandUtils.ERunOptions.NoWaitForExit;
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

		public override string ToString()
		{
			return Name;
		}

		protected string GetVolumeName(string InPath)
		{
			Match M = Regex.Match(InPath, @"/Volumes/(.+?)/");

			if (M.Success)
			{
				return M.Groups[1].ToString();
			}

			return "";
		}

		protected IAppInstall InstallStagedBuild(UnrealAppConfig AppConfig, StagedBuild InBuild)
		{
			bool SkipDeploy = Globals.Params.ParseParam("SkipDeploy");

			string BuildPath = InBuild.BuildPath;

			// Must be on our volume to run
			string BuildVolume = GetVolumeName(BuildPath);
			string LocalRoot = GetVolumeName(Environment.CurrentDirectory);

			if (BuildVolume.Equals(LocalRoot, StringComparison.OrdinalIgnoreCase) == false)
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

			MacAppInstall MacApp = new MacAppInstall(AppConfig.Name, this);
			MacApp.LocalPath = BuildPath;
			MacApp.WorkingDirectory = MacApp.LocalPath;
			MacApp.RunOptions = RunOptions;

			// Set commandline replace any InstallPath arguments with the path we use
			MacApp.CommandArguments = Regex.Replace(AppConfig.CommandLine, @"\$\(InstallPath\)", BuildPath, RegexOptions.IgnoreCase);

			// Mac always forces this to stop logs and other artifacts going to different places
			// Mac always forces this to stop logs and other artifacts going to different places
			MacApp.CommandArguments += string.Format(" -userdir={0}", UserDir);
			MacApp.ArtifactPath = Path.Combine(UserDir, @"Saved");

			// temp - Mac doesn't support -userdir?
			//MacApp.ArtifactPath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Library/Logs", AppConfig.ProjectName);

			// clear artifact path
			if (Directory.Exists(MacApp.ArtifactPath))
			{
				try
				{
					Directory.Delete(MacApp.ArtifactPath, true);
				}
				catch (Exception Ex)
				{
					Log.Warning("Failed to delete {0}. {1}", MacApp.ArtifactPath, Ex.Message);
				}
			}

			if (Path.IsPathRooted(InBuild.ExecutablePath))
			{
				MacApp.ExecutablePath = InBuild.ExecutablePath;
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
						MacApp.CommandArguments += string.Format(" -basedir={0}", Path.GetDirectoryName(BinaryPath));

						BinaryPath = LocalBinary;
					}
				}

				MacApp.ExecutablePath = BinaryPath;
			}

			// now turn the Foo.app into Foo/Content/MacOS/Foo
			string AppPath = Path.GetDirectoryName(MacApp.ExecutablePath);
			string FileName = Path.GetFileNameWithoutExtension(MacApp.ExecutablePath);
			MacApp.ExecutablePath = Path.Combine(MacApp.ExecutablePath, "Contents", "MacOS", FileName);

			return MacApp;
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

			MacAppInstall MacApp = new MacAppInstall(AppConfig.Name, this);

			MacApp.WorkingDirectory = Path.GetFullPath(EditorBuild.ExecutablePath);
			MacApp.CommandArguments = AppConfig.CommandLine;
			MacApp.RunOptions = RunOptions;

			// Mac always forces this to stop logs and other artifacts going to different places
			MacApp.CommandArguments += string.Format(" -userdir={0}", UserDir);
			MacApp.ArtifactPath = Path.Combine(UserDir, @"Saved");

			// now turn the Foo.app into Foo/Content/MacOS/Foo
			string AppPath = Path.GetDirectoryName(EditorBuild.ExecutablePath);
			string FileName = Path.GetFileNameWithoutExtension(EditorBuild.ExecutablePath);
			MacApp.ExecutablePath = Path.Combine(EditorBuild.ExecutablePath, "Contents", "MacOS", FileName);

			return MacApp;
		}

		public IAppInstance Run(IAppInstall App)
		{
			MacAppInstall MacInstall = App as MacAppInstall;

			if (MacInstall == null)
			{
				throw new AutomationException("Invalid install type!");
			}

			IProcessResult Result = null;

			lock (Globals.MainLock)
			{
				string NewWorkingDir = string.IsNullOrEmpty(MacInstall.WorkingDirectory) ? MacInstall.LocalPath : MacInstall.WorkingDirectory;
				string OldWD = Environment.CurrentDirectory;
				Environment.CurrentDirectory = NewWorkingDir;

				Log.Info("Launching {0} on {1}", App.Name, ToString());
				Log.Verbose("\t{0}", MacInstall.CommandArguments);

				Result = CommandUtils.Run(MacInstall.ExecutablePath, MacInstall.CommandArguments, Options: MacInstall.RunOptions);

				if (Result.HasExited && Result.ExitCode != 0)
				{
					throw new AutomationException("Failed to launch {0}. Error {1}", MacInstall.ExecutablePath, Result.ExitCode);
				}

				Environment.CurrentDirectory = OldWD;
			}

			return new MacAppInstance(MacInstall, Result);
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
