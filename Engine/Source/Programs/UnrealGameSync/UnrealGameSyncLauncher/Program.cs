// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using UnrealGameSync;

namespace UnrealGameSyncLauncher
{
	static partial class Program
	{
		/// <summary>
		/// Specifies the depot path to sync down the stable version of UGS from, without a trailing slash (eg. //depot/UnrealGameSync/bin). This is a site-specific setting. 
		/// The UnrealGameSync executable should be located at Release/UnrealGameSync.exe under this path, with any dependent DLLs.
		/// </summary>
		static readonly string DefaultDepotPath;

		[STAThread]
		static int Main(string[] Args)
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			bool bFirstInstance;
			using(Mutex InstanceMutex = new Mutex(true, "UnrealGameSyncRunning", out bFirstInstance))
			{
				if(!bFirstInstance)
				{
					using(EventWaitHandle ActivateEvent = new EventWaitHandle(false, EventResetMode.AutoReset, "ActivateUnrealGameSync"))
					{
						ActivateEvent.Set();
					}
					return 0;
				}

				// Try to find Perforce in the path
				string PerforceFileName = null;
				foreach(string PathDirectoryName in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(new char[]{ Path.PathSeparator }, StringSplitOptions.RemoveEmptyEntries))
				{
					try
					{
						string PossibleFileName = Path.Combine(PathDirectoryName, "p4.exe");
						if(File.Exists(PossibleFileName))
						{
							PerforceFileName = PossibleFileName;
							break;
						}
					}
					catch { }
				}

				// If it doesn't exist, don't continue
				if(PerforceFileName == null)
				{
					MessageBox.Show("UnrealGameSync requires the Perforce command-line tools. Please download and install from http://www.perforce.com/.");
					return 1;
				}

				// Figure out if we should sync the unstable build by default
				bool bUnstable = Args.Contains("-unstable", StringComparer.InvariantCultureIgnoreCase);

				// Read the settings
				string ServerAndPort = null;
				string UserName = null;
				string DepotPath = DefaultDepotPath;
				ReadSettings(ref ServerAndPort, ref UserName, ref DepotPath);

				// If the shift key is held down, immediately show the settings window
				if((Control.ModifierKeys & Keys.Shift) != 0)
				{
					// Show the settings window immediately
					SettingsWindow UpdateError = new SettingsWindow(null, null, ServerAndPort, UserName, DepotPath, bUnstable, (Perforce, DepotParam, bUnstableParam, LogWriter) => SyncAndRun(Perforce, DepotParam, bUnstableParam, Args, InstanceMutex, LogWriter));
					if(UpdateError.ShowDialog() == DialogResult.OK)
					{
						return 0;
					}
				}
				else
				{
					// Try to do a sync with the current settings first
					SyncAndRunModalTask SyncApplication = new SyncAndRunModalTask(ServerAndPort, UserName, (Perforce, LogWriter) => SyncAndRun(Perforce, DepotPath, bUnstable, Args, InstanceMutex, LogWriter));

					string ErrorMessage;
					if(ModalTask.Execute(null, SyncApplication, "Updating", "Checking for updates, please wait...", out ErrorMessage) == ModalTaskResult.Succeeded)
					{
						return 0;
					}

					SettingsWindow UpdateError = new SettingsWindow("Unable to update UnrealGameSync from Perforce. Verify that your connection settings are correct.", SyncApplication.LogWriter.ToString(), SyncApplication.ServerAndPort, SyncApplication.UserName, DepotPath, bUnstable, (Perforce, DepotParam, bUnstableParam, LogWriter) => SyncAndRun(Perforce, DepotParam, bUnstableParam, Args, InstanceMutex, LogWriter));
					if(UpdateError.ShowDialog() == DialogResult.OK)
					{
						return 0;
					}
				}
			}
			return 1;
		}

		public static void ReadSettings(ref string ServerAndPort, ref string UserName, ref string DepotPath)
		{
			using (RegistryKey Key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Epic Games\\UnrealGameSync", false))
			{
				if(Key != null)
				{
					ServerAndPort = Key.GetValue("Server", ServerAndPort) as string;
					UserName = Key.GetValue("UserName", UserName) as string;
					DepotPath = Key.GetValue("DepotPath", DepotPath) as string;
				}
			}
		}

		public static void SaveSettings(string ServerAndPort, string UserName, string DepotPath)
		{
			using (RegistryKey Key = Registry.CurrentUser.OpenSubKey("SOFTWARE\\Epic Games\\UnrealGameSync", true))
			{
				if(String.IsNullOrEmpty(ServerAndPort))
				{
					try { Key.DeleteValue("Server"); } catch(Exception) { }
				}
				else
				{
					Key.SetValue("Server", ServerAndPort);
				}

				if(String.IsNullOrEmpty(UserName))
				{
					try { Key.DeleteValue("UserName"); } catch(Exception) { }
				}
				else
				{
					Key.SetValue("UserName", UserName);
				}

				if(String.IsNullOrEmpty(DepotPath) || (DefaultDepotPath != null && String.Equals(DepotPath, DefaultDepotPath, StringComparison.InvariantCultureIgnoreCase)))
				{
					try { Key.DeleteValue("DepotPath"); } catch(Exception) { }
				}
				else
				{
					Key.SetValue("DepotPath", DepotPath);
				}
			}
		}

		public static bool SyncAndRun(PerforceConnection Perforce, string BaseDepotPath, bool bUnstable, string[] Args, Mutex InstanceMutex, TextWriter LogWriter)
		{
			try
			{
				string SyncPath = BaseDepotPath.TrimEnd('/') + (bUnstable? "/UnstableRelease/..." : "/Release/...");
				LogWriter.WriteLine("Syncing from {0}", SyncPath);

				// Create the target folder
				string ApplicationFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnrealGameSync", "Latest");
				if(!SafeCreateDirectory(ApplicationFolder))
				{
					LogWriter.WriteLine("Couldn't create directory: {0}", ApplicationFolder);
					return false;
				}

				// Find the most recent changelist
				List<PerforceChangeSummary> Changes;
				if(!Perforce.FindChanges(SyncPath, 1, out Changes, LogWriter) || Changes.Count < 1)
				{
					LogWriter.WriteLine("Couldn't find last changelist");
					return false;
				}

				// Take the first changelist number
				int RequiredChangeNumber = Changes[0].Number;

				// Read the current version
				string SyncVersionFile = Path.Combine(ApplicationFolder, "SyncVersion.txt");
				string RequiredSyncText = String.Format("{0}\n{1}@{2}", Perforce.ServerAndPort ?? "", SyncPath, RequiredChangeNumber);

				// Check the application exists
				string ApplicationExe = Path.Combine(ApplicationFolder, "UnrealGameSync.exe");

				// Check if the version has changed
				string SyncText;
				if(!File.Exists(SyncVersionFile) || !File.Exists(ApplicationExe) || !TryReadAllText(SyncVersionFile, out SyncText) || SyncText != RequiredSyncText)
				{
					// Try to delete the directory contents. Retry for a while, in case we've been spawned by an application in this folder to do an update.
					for(int NumRetries = 0; !SafeDeleteDirectoryContents(ApplicationFolder); NumRetries++)
					{
						if(NumRetries > 20)
						{
							LogWriter.WriteLine("Couldn't delete contents of {0} (retried {1} times).", ApplicationFolder, NumRetries);
							return false;
						}
						Thread.Sleep(500);
					}
				
					// Find all the files in the sync path at this changelist
					List<PerforceFileRecord> FileRecords;
					if(!Perforce.Stat(String.Format("{0}@{1}", SyncPath, RequiredChangeNumber), out FileRecords, LogWriter))
					{
						LogWriter.WriteLine("Couldn't find matching files.");
						return false;
					}

					// Sync all the files in this list to the same directory structure under the application folder
					string DepotPathPrefix = SyncPath.Substring(0, SyncPath.LastIndexOf('/') + 1);
					foreach(PerforceFileRecord FileRecord in FileRecords)
					{
						string LocalPath = Path.Combine(ApplicationFolder, FileRecord.DepotPath.Substring(DepotPathPrefix.Length).Replace('/', Path.DirectorySeparatorChar));
						if(!SafeCreateDirectory(Path.GetDirectoryName(LocalPath)))
						{
							LogWriter.WriteLine("Couldn't create folder {0}", Path.GetDirectoryName(LocalPath));
							return false;
						}
						if(!Perforce.PrintToFile(FileRecord.DepotPath, LocalPath, LogWriter))
						{
							LogWriter.WriteLine("Couldn't sync {0} to {1}", FileRecord.DepotPath, LocalPath);
							return false;
						}
					}

					// Check the application exists
					if(!File.Exists(ApplicationExe))
					{
						LogWriter.WriteLine("Application was not synced from Perforce. Check that UnrealGameSync exists at {0}/UnrealGameSync.exe, and you have access to it.", SyncPath);
						return false;
					}

					// Update the version
					if(!TryWriteAllText(SyncVersionFile, RequiredSyncText))
					{
						LogWriter.WriteLine("Couldn't write sync text to {0}", SyncVersionFile);
						return false;
					}
				}
				LogWriter.WriteLine();

				// Build the command line for the synced application, including the sync path to monitor for updates
				StringBuilder NewCommandLine = new StringBuilder(String.Format("-updatepath=\"{0}@>{1}\" -updatespawn=\"{2}\"{3}", SyncPath, RequiredChangeNumber, Assembly.GetEntryAssembly().Location, bUnstable? " -unstable" : ""));
				if(!String.IsNullOrEmpty(Perforce.ServerAndPort))
				{
					NewCommandLine.AppendFormat(" -p4port={0}", QuoteArgument(Perforce.ServerAndPort));
				}
				if(!String.IsNullOrEmpty(Perforce.UserName))
				{
					NewCommandLine.AppendFormat(" -p4user={0}", QuoteArgument(Perforce.UserName));
				}
				foreach(string Arg in Args)
				{
					NewCommandLine.AppendFormat(" {0}", QuoteArgument(Arg));
				}

				// Release the mutex now so that the new application can start up
				InstanceMutex.Close();

				// Spawn the application
				LogWriter.WriteLine("Spawning {0} with command line: {1}", ApplicationExe, NewCommandLine.ToString());
				using(Process ChildProcess = new Process())
				{
					ChildProcess.StartInfo.FileName = ApplicationExe;
					ChildProcess.StartInfo.Arguments = NewCommandLine.ToString();
					ChildProcess.StartInfo.UseShellExecute = false;
					ChildProcess.StartInfo.CreateNoWindow = false;
					if(!ChildProcess.Start())
					{
						LogWriter.WriteLine("Failed to start process");
						return false;
					}
				}

				return true;
			}
			catch(Exception Ex)
			{
				LogWriter.WriteLine(Ex.ToString());
				return false;
			}
		}

		static string QuoteArgument(string Arg)
		{
			if(Arg.IndexOf(' ') != -1 && !Arg.StartsWith("\""))
			{
				return String.Format("\"{0}\"", Arg);
			}
			else
			{
				return Arg;
			}
		}

		static bool TryReadAllText(string FileName, out string Text)
		{
			try
			{
				Text = File.ReadAllText(FileName);
				return true;
			}
			catch(Exception)
			{
				Text = null;
				return false;
			}
		}

		static bool TryWriteAllText(string FileName, string Text)
		{
			try
			{
				File.WriteAllText(FileName, Text);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeCreateDirectory(string DirectoryName)
		{
			try
			{
				Directory.CreateDirectory(DirectoryName);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeDeleteDirectory(string DirectoryName)
		{
			try
			{
				Directory.Delete(DirectoryName, true);
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}

		static bool SafeDeleteDirectoryContents(string DirectoryName)
		{
			try
			{
				DirectoryInfo Directory = new DirectoryInfo(DirectoryName);
				foreach(FileInfo ChildFile in Directory.EnumerateFiles("*", SearchOption.AllDirectories))
				{
					ChildFile.Attributes = ChildFile.Attributes & ~FileAttributes.ReadOnly;
					ChildFile.Delete();
				}
				foreach(DirectoryInfo ChildDirectory in Directory.EnumerateDirectories())
				{
					SafeDeleteDirectory(ChildDirectory.FullName);
				}
				return true;
			}
			catch(Exception)
			{
				return false;
			}
		}
	}
}
