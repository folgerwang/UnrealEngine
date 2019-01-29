// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using EpicGame;
using Gauntlet;
using System;
using System.Linq;
using System.Net;
using System.Text.RegularExpressions;

namespace EngineTest
{
	/// <summary>
	/// Runs automated tests on a platform
	/// </summary>
	public class RunAutomatedTests : UE4Game.DefaultTest
	{
		public RunAutomatedTests(Gauntlet.UnrealTestContext InContext)
			: base(InContext)
		{
		}

		public override UE4Game.UE4TestConfig GetConfiguration()
		{
			string ReportOutputPath = Globals.Params.ParseValue("ReportOutputPath", "");
			string DisplayReportOutputPath = Globals.Params.ParseValue("DisplayReportOutputPath", "");
			string SessionID = Guid.NewGuid().ToString();
			string HostIP = UnrealHelpers.GetHostIpAddress();

			UE4Game.UE4TestConfig Config = base.GetConfiguration();
			Config.MaxDuration = 60.0f * 60.0f * 2.0f; // set to 2 hours.

			var ClientRole = Config.RequireRole(UnrealTargetRole.Client);
			ClientRole.ExplicitClientCommandLine = string.Format("-sessionid={0} -messaging -log -TcpMessagingConnect={1}:6666", SessionID, HostIP);

			var EditorRole = Config.RequireRole(UnrealTargetRole.Editor);
			EditorRole.CommandLine = string.Format("-nullrhi -ExecCmds=\"Automation StartRemoteSession {0};RunTests Project+System; Quit\" -TcpMessagingListen={1}:6666 -log -ReportOutputPath=\"{2}\" -DisplayReportOutputPath=\"{3}\"", SessionID, HostIP, ReportOutputPath, DisplayReportOutputPath);

			return Config;
		}
		public override void StopTest(bool WasCancelled)
		{
			// Just echo the entire log output so errors/warnings get captured in context
			try
			{
				string[] EditorLines = TestInstance.EditorApp.StdOut.Split(new string[] { "\r\n", "\n" }, StringSplitOptions.None);
				Log.Info("Test coordinator output:");

				foreach (string Line in EditorLines)
				{
					// Strip timestamp, and bypass Gauntlet log to avoid sanitizing
					Console.WriteLine(Regex.Replace(Line, @"^\[.*?\]\[.*?\]", ""));
				}

				for (int i = 0; i < TestInstance.ClientApps.Length; i++)
				{
					string[] ClientLines = TestInstance.ClientApps[i].StdOut.Split(new string[] { "\r\n", "\n" }, StringSplitOptions.None);
					Log.Info("Client {0} output:", i);

					foreach (string Line in ClientLines)
					{
						Console.WriteLine(Regex.Replace(Line, @"^\[.*?\]\[.*?\]", ""));
					}
				}
			}
			catch (System.Exception ex)
			{
				Log.Info("Failed to process log: " + ex.ToString());
			}

			base.StopTest(WasCancelled);
		}
	}
}
