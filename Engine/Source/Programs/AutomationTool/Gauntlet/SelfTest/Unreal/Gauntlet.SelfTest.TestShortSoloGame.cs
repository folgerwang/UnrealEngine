// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using Gauntlet;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

namespace Gauntlet.SelfTest
{
	[TestGroup("Unreal", 10)]
	class TestOrionShortSoloGame : TestUnrealInstallAndRunBase
	{

		class ShortSoloOptions : UnrealOptions
		{
			public ShortSoloOptions()
			{
			}

			public override void ApplyToConfig(UnrealAppConfig AppConfig)
			{
				base.ApplyToConfig(AppConfig);

				AppConfig.CommandLine += " -gauntlet=SoakTest -soak.matchcount=1 -soak.matchlength=3";

				if (AppConfig.ProcessType.IsClient() && AppConfig.Platform == UnrealTargetPlatform.Win64)
				{            
					AppConfig.CommandLine += " -windowed -ResX=1280 -ResY=720 -nullrhi";
				}
			}
		}

		void TestClientPlatform(UnrealTargetPlatform Platform)
		{
			string GameName = this.GameName;
			string BuildPath = this.BuildPath;
			string DevKit = this.PS4Name;

			if (GameName.Equals("OrionGame", StringComparison.OrdinalIgnoreCase) == false)
			{
				Log.Info("Skipping test {0} due to non-Orion project!", this);
				MarkComplete();
				return;
			}

			// create a new build
			UnrealBuildSource Build = new UnrealBuildSource(GameName, UsesSharedBuildType, Environment.CurrentDirectory, BuildPath);

			// check it's valid
			if (!CheckResult(Build.BuildCount > 0, "staged build was invalid"))
			{
				MarkComplete();
				return;
			}

			// Create devices to run the client and server
			ITargetDevice ServerDevice = new TargetDeviceWindows("PC Server", Gauntlet.Globals.TempDir);
			ITargetDevice ClientDevice;

			if (Platform == UnrealTargetPlatform.PS4)
			{
				ClientDevice = new TargetDevicePS4(this.PS4Name);
			}
			else
			{
				ClientDevice = new TargetDeviceWindows("PC Client", Gauntlet.Globals.TempDir);
			}

			UnrealAppConfig ServerConfig = Build.CreateConfiguration(new UnrealSessionRole(UnrealTargetRole.Server, ServerDevice.Platform, UnrealTargetConfiguration.Development));
			UnrealAppConfig ClientConfig = Build.CreateConfiguration(new UnrealSessionRole(UnrealTargetRole.Client, ClientDevice.Platform, UnrealTargetConfiguration.Development));

			if (!CheckResult(ServerConfig != null && ServerConfig != null, "Could not create configs!"))
			{
				MarkComplete();
				return;
			}

			ShortSoloOptions Options = new ShortSoloOptions();
			Options.ApplyToConfig(ClientConfig);
			Options.ApplyToConfig(ServerConfig);

			IAppInstall ClientInstall = ClientDevice.InstallApplication(ClientConfig);
			IAppInstall ServerInstall = ServerDevice.InstallApplication(ServerConfig);

			if (!CheckResult(ServerConfig != null && ServerConfig != null, "Could not create configs!"))
			{
				MarkComplete();
				return;
			}

			IAppInstance ClientInstance = ClientInstall.Run();
			IAppInstance ServerInstance = ServerInstall.Run();

			DateTime StartTime = DateTime.Now;
			bool RunWasSuccessful = true;

			while (ClientInstance.HasExited == false)
			{
				if ((DateTime.Now - StartTime).TotalSeconds > 800)
				{
					RunWasSuccessful = false;
					break;
				}
			}

			ClientInstance.Kill();
			ServerInstance.Kill();

			UnrealLogParser LogParser = new UnrealLogParser(ClientInstance.StdOut);

			UnrealLogParser.CallstackMessage ErrorInfo = LogParser.GetFatalError();
			if (ErrorInfo != null)
			{
				CheckResult(false, "FatalError - {0}", ErrorInfo.Message);
			}

			RunWasSuccessful = LogParser.HasRequestExit();

			CheckResult(RunWasSuccessful, "Failed to run for platform {0}", Platform);

		}

		public override void TickTest()
		{
			TestClientPlatform(UnrealTargetPlatform.PS4);
			// Win64 requires auth...
			///TestClientPlatform(UnrealTargetPlatform.Win64);

			MarkComplete();
		}
	}
}
