// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;
using Gauntlet;
using Newtonsoft.Json;
using System.Text.RegularExpressions;

namespace EngineTest
{
	/// <summary>
	/// Test runner for excuting EngineTest in Gauntlet
	/// </summary>
	public class RunEngineTests : Gauntlet.RunUnreal
	{
		public override ExitCode Execute()
		{
			Globals.Params = new Gauntlet.Params(this.Params);

			UnrealTestOptions ContextOptions = new UnrealTestOptions();

			AutoParam.ApplyDefaults(ContextOptions);

			ContextOptions.Project = "EngineTest";
			ContextOptions.Namespaces = "EngineTest,Gauntlet.UnrealTest";
			ContextOptions.UsesSharedBuildType = true;

			AutoParam.ApplyParams(ContextOptions, Globals.Params.AllArguments);

			return RunTests(ContextOptions);
		}
	}
}