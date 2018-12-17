// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
	/// <summary>
	/// Validates that we can install and run all Windows based targets that are supported. P8 because this is the longest
	/// running Unreal-based test
	/// </summary>
	[TestGroup("Unreal", 8)]
	class TestUnrealInstallThenRunWindows : TestUnrealInstallAndRunBase
	{
		public override void TickTest()
		{
			// create a new build
			UnrealBuildSource Build = new UnrealBuildSource(this.GameName, this.UsesSharedBuildType, Environment.CurrentDirectory, this.BuildPath, new string[] { "" });

			// check it's valid
			if (!CheckResult(Build.BuildCount > 0, "staged build was invalid"))
			{
				MarkComplete();
				return;
			}

			// Create devices to run the client and server
			ITargetDevice PCDevice = new TargetDeviceWindows("Local PC", Globals.TempDir);

			// Monolithic servers
			TestInstallThenRun(Build, UnrealTargetRole.Server, PCDevice, UnrealTargetConfiguration.Development);
			TestInstallThenRun(Build, UnrealTargetRole.Server, PCDevice, UnrealTargetConfiguration.Test);

			// Monolithic clients
			TestInstallThenRun(Build, UnrealTargetRole.Client, PCDevice, UnrealTargetConfiguration.Development);
			TestInstallThenRun(Build, UnrealTargetRole.Client, PCDevice, UnrealTargetConfiguration.Test);

			// editor configs
			TestInstallThenRun(Build, UnrealTargetRole.EditorGame, PCDevice, UnrealTargetConfiguration.Development);
			TestInstallThenRun(Build, UnrealTargetRole.EditorServer, PCDevice, UnrealTargetConfiguration.Development);
			TestInstallThenRun(Build, UnrealTargetRole.Editor, PCDevice, UnrealTargetConfiguration.Development);

			MarkComplete();
		}
	}
}
