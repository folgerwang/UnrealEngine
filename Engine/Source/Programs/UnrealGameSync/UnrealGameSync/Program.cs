// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data.SqlClient;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	static partial class Program
	{
		/// <summary>
		/// SQL connection string used to connect to the database for telemetry and review data. The 'Program' class is a partial class, to allow an
		/// opportunistically included C# source file in NotForLicensees/ProgramSettings.cs to override this value in a static constructor.
		/// </summary>
		public static readonly string ApiUrl = null;

		public static string SyncVersion = null;

		[STAThread]
		static void Main(string[] Args)
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			bool bFirstInstance;
			using(Mutex InstanceMutex = new Mutex(true, "UnrealGameSyncRunning", out bFirstInstance))
			{
				using(EventWaitHandle ActivateEvent = new EventWaitHandle(false, EventResetMode.AutoReset, "ActivateUnrealGameSync"))
				{
					if(bFirstInstance)
					{
						InnerMain(InstanceMutex, ActivateEvent, Args);
					}
					else
					{
						ActivateEvent.Set();
					}
				}
			}
		}

		static void InnerMain(Mutex InstanceMutex, EventWaitHandle ActivateEvent, string[] Args)
		{
			List<string> RemainingArgs = new List<string>(Args);

			string UpdatePath;
			ParseArgument(RemainingArgs, "-updatepath=", out UpdatePath);

			string UpdateSpawn;
			ParseArgument(RemainingArgs, "-updatespawn=", out UpdateSpawn);

			string ServerAndPort;
			ParseArgument(RemainingArgs, "-p4port=", out ServerAndPort);

			string UserName;
			ParseArgument(RemainingArgs, "-p4user=", out UserName);

			bool bRestoreState;
			ParseOption(RemainingArgs, "-restorestate", out bRestoreState);

			bool bUnstable;
			ParseOption(RemainingArgs, "-unstable", out bUnstable);

            string ProjectFileName;
            ParseArgument(RemainingArgs, "-project=", out ProjectFileName);

			string UpdateConfigFile = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "AutoUpdate.ini");
			MergeUpdateSettings(UpdateConfigFile, ref UpdatePath, ref UpdateSpawn);

			string SyncVersionFile = Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "SyncVersion.txt");
			if(File.Exists(SyncVersionFile))
			{
				try
				{
					SyncVersion = File.ReadAllText(SyncVersionFile).Trim();
				}
				catch(Exception)
				{
					SyncVersion = null;
				}
			}

			string DataFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UnrealGameSync");
			Directory.CreateDirectory(DataFolder);

			using(TelemetryWriter Telemetry = new TelemetryWriter(ApiUrl, Path.Combine(DataFolder, "Telemetry.log")))
			{
				AppDomain.CurrentDomain.UnhandledException += CurrentDomain_UnhandledException;
				using(UpdateMonitor UpdateMonitor = new UpdateMonitor(new PerforceConnection(UserName, null, ServerAndPort), UpdatePath))
				{
					ProgramApplicationContext Context = new ProgramApplicationContext(UpdateMonitor, ApiUrl, DataFolder, ActivateEvent, bRestoreState, UpdateSpawn, ProjectFileName, bUnstable);
					Application.Run(Context);

					if(UpdateMonitor.IsUpdateAvailable && UpdateSpawn != null)
					{
						InstanceMutex.Close();
						Utility.SpawnProcess(UpdateSpawn, "-restorestate" + (bUnstable? " -unstable" : ""));
					}
				}
			}
		}

		private static void CurrentDomain_UnhandledException(object Sender, UnhandledExceptionEventArgs Args)
		{
			Exception Ex = Args.ExceptionObject as Exception;
			if(Ex != null)
			{
				StringBuilder ExceptionTrace = new StringBuilder(Ex.ToString());
				for(Exception InnerEx = Ex.InnerException; InnerEx != null; InnerEx = InnerEx.InnerException)
				{
					ExceptionTrace.Append("\nInner Exception:\n");
					ExceptionTrace.Append(InnerEx.ToString());
				}
				TelemetryWriter.Enqueue(TelemetryErrorType.Crash, ExceptionTrace.ToString(), null, DateTime.Now);
			}
		}

		static void MergeUpdateSettings(string UpdateConfigFile, ref string UpdatePath, ref string UpdateSpawn)
		{
			try
			{
				ConfigFile UpdateConfig = new ConfigFile();
				if(File.Exists(UpdateConfigFile))
				{
					UpdateConfig.Load(UpdateConfigFile);
				}

				if(UpdatePath == null)
				{
					UpdatePath = UpdateConfig.GetValue("Update.Path", null);
				}
				else
				{
					UpdateConfig.SetValue("Update.Path", UpdatePath);
				}

				if(UpdateSpawn == null)
				{
					UpdateSpawn = UpdateConfig.GetValue("Update.Spawn", null);
				}
				else
				{
					UpdateConfig.SetValue("Update.Spawn", UpdateSpawn);
				}

				UpdateConfig.Save(UpdateConfigFile);
			}
			catch(Exception)
			{
			}
		}

		static bool ParseOption(List<string> RemainingArgs, string Option, out bool Value)
		{
			for(int Idx = 0; Idx < RemainingArgs.Count; Idx++)
			{
				if(RemainingArgs[Idx].Equals(Option, StringComparison.InvariantCultureIgnoreCase))
				{
					Value = true;
					RemainingArgs.RemoveAt(Idx);
					return true;
				}
			}

			Value = false;
			return false;
		}

		static bool ParseArgument(List<string> RemainingArgs, string Prefix, out string Value)
		{
			for(int Idx = 0; Idx < RemainingArgs.Count; Idx++)
			{
				if(RemainingArgs[Idx].StartsWith(Prefix, StringComparison.InvariantCultureIgnoreCase))
				{
					Value = RemainingArgs[Idx].Substring(Prefix.Length);
					RemainingArgs.RemoveAt(Idx);
					return true;
				}
			}

			Value = null;
			return false;
		}
	}
}
