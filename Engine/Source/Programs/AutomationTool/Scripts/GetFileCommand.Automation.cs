// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Reflection;
using AutomationTool;
using UnrealBuildTool;
using Tools.DotNETCommon;

public partial class Project : CommandUtils
{
	#region GetFile Command

	public static void GetFile(ProjectParams Params)
	{
		Params.ValidateAndLog();
		if (string.IsNullOrEmpty(Params.GetFile))
		{
			return;
		}

		LogInformation("********** GETFILE COMMAND STARTED **********");

		var FileName = Path.GetFileName(Params.GetFile);
		var LocalFile = CombinePaths(CmdEnv.EngineSavedFolder, FileName);

		var SC = CreateDeploymentContext(Params, false);
		if (SC.Count == 0)
		{
			throw new AutomationException("Failed to create deployment context");
		}

		SC[0].StageTargetPlatform.GetTargetFile(Params.GetFile, LocalFile, Params);

		LogInformation("********** GETFILE COMMAND COMPLETED **********");
	}

	#endregion
}
