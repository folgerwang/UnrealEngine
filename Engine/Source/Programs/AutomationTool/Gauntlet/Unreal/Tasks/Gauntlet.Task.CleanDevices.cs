// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace Gauntlet
{
	/// <summary>
	/// Runs through every device provided (either singularly, a comma-separated list, or a device file) and removes any Gauntlet-installed
	/// builds older than MaxDays, and ay local crashdump info (if applicable) older than MaxDays
	/// </summary>
	public class CleanDevices : AutomationTool.BuildCommand
	{

		[AutoParamWithNames("", "device", "devices")]
		public string Devices;

		[AutoParam("")]
		public string TempDir;

		[AutoParam(7)]
		public int MaxDays;


		public override AutomationTool.ExitCode Execute()
		{
			AutoParam.ApplyParamsAndDefaults(this, Environment.GetCommandLineArgs());

			Gauntlet.Log.Level = Gauntlet.LogLevel.VeryVerbose;

			if (string.IsNullOrEmpty(TempDir) == false)
			{
				Globals.TempDir = TempDir;
			}

			// add devices. We're quick so can ignore constraints
			DevicePool.Instance.AddDevices(UnrealTargetPlatform.Win64, Devices, false);

			UnrealTargetPlatform[] SupportedPlatforms = { UnrealTargetPlatform.PS4, UnrealTargetPlatform.Win64 };

			foreach (UnrealTargetPlatform Platform in SupportedPlatforms)
			{
				DevicePool.Instance.EnumerateDevices(Platform, Device =>
				{
					try
					{
						CleanDevice(Device);
					}
					catch (Exception Ex)
					{
						Gauntlet.Log.Warning("Exception cleaning device: {0}", Ex);
					}

					return true;
				});
			}

			DevicePool.Instance.Dispose();


			return AutomationTool.ExitCode.Success;
		}

		protected void CleanDevice(ITargetDevice Device)
		{
			Gauntlet.Log.Info("Cleaning {0}", Device.Name);

			if (Device.IsOn == false)
			{
				Device.PowerOn();
			}

			if (Device.IsAvailable == false)
			{
				Gauntlet.Log.Info("{0} is not available, skipping", Device.Name);
				return;
			}

			Device.Connect();

			if (Device.IsConnected == false)
			{
				Gauntlet.Log.Warning("Failed to connect to {0}", Device.Name);
				return;
			}

			if (Device is TargetDevicePS4)
			{
				CleanPS4(Device as TargetDevicePS4);
			}

			// disconnect and power down
			Gauntlet.Log.Info("Powering down and disconnecting from {0}", Device.Name);
			Device.Disconnect();
			// turns out this may be a bad idea.. sorry environment.
			//Device.PowerOff();
		}

		protected void CleanPS4(TargetDevicePS4 PS4)
		{
			// TODO - it would be nice to make both enumeration of builds and their removal tasks that the targetdevice provides...
			DirectoryInfo Di = new DirectoryInfo(PS4.DataPath);

			if (Di.Exists == false)
			{
				Gauntlet.Log.Warning("Data path {0} not found for {1}", PS4.DataPath, PS4.DeviceName);
				return;
			}

			// first check each toplevel directory for a token that gauntlet creates
			foreach (DirectoryInfo SubDir in Di.GetDirectories())
			{
				try
				{
					FileInfo TokenFile = SubDir.GetFiles().Where(F => F.Name == "gauntlet.token" || F.Name == "testdata.token").FirstOrDefault();

					if (TokenFile != null)
					{
						double Age = (DateTime.Now - TokenFile.LastWriteTime).TotalDays;
						if (Age >= MaxDays)
						{
							Gauntlet.Log.Info("Build at {0} is {1:00} days old (max={2}). Removing", SubDir.Name, Age, MaxDays);

							try
							{
								SubDir.Delete(true);
							}
							catch (Exception Ex)
							{
								Gauntlet.Log.Error("Error deleting {0}: {1}", SubDir.FullName, Ex);
							}

						}
					}
					else
					{
						Gauntlet.Log.Verbose("Directory {0} was not created by Gauntlet. Ignoring", SubDir.Name);
					}
				}
				catch (Exception Ex)
				{
					// can occur when the data drive has characters considered invalid for a windows FS.
					Gauntlet.Log.Warning("Error getting directory tree for {0} on {1}. {2}", SubDir.FullName, PS4.Name, Ex);
				}
			}

			// now delete old Crashdumps
			DirectoryInfo DumpDir = new DirectoryInfo(Path.Combine(Di.FullName, "sce_coredumps"));

			if (DumpDir.Exists)
			{

				foreach (DirectoryInfo SubDir in DumpDir.GetDirectories())
				{
					// each crashdump has a report
					FileInfo DumpFile = SubDir.GetFiles("*.orbisdmp", SearchOption.TopDirectoryOnly).FirstOrDefault();

					if (DumpFile != null)
					{
						double Age = (DateTime.Now - DumpFile.LastWriteTime).TotalDays;
						if (Age >= MaxDays)
						{
							Gauntlet.Log.Info("Crashdump at {0} is {1:00} days old (max={2}). Removing", SubDir.Name, Age, MaxDays);

							try
							{
								SubDir.Delete(true);
							}
							catch (Exception Ex)
							{
								Gauntlet.Log.Error("Error deleting {0}: {1}", SubDir.FullName, Ex);
							}
						}
					}
					else
					{
						Gauntlet.Log.Info("No orbisdmp found in {0}", SubDir.Name);
					}
				}
			}
			PS4.SetSetting("NP Environment", "sp-int");
		}
	}
}
