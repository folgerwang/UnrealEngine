// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public abstract class MayaLiveLinkPluginTargetBase : TargetRules
{
	public MayaLiveLinkPluginTargetBase(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;

		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;

		// We only need minimal use of the engine for this plugin
		bCompileLeanAndMeanUE = true;
		bUseMallocProfiler = false;
		bBuildEditor = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = true;
        bCompileICU = false;
        bHasExports = true;

		bBuildInSolutionByDefault = false;

		// Add a post-build step that copies the output to a file with the .mll extension
		string OutputName = "MayaLiveLinkPlugin";
		if(Target.Configuration != UnrealTargetConfiguration.Development)
		{
			OutputName = string.Format("{0}-{1}-{2}", OutputName, Target.Platform, Target.Configuration);
		}
		PostBuildSteps.Add(string.Format("copy /Y \"$(EngineDir)\\Binaries\\Win64\\{0}.dll\" \"$(EngineDir)\\Binaries\\Win64\\{0}.mll\" >nul: & echo Copied output to $(EngineDir)\\Binaries\\Win64\\{0}.mll", OutputName));
	}
}

public class MayaLiveLinkPlugin2016Target : MayaLiveLinkPluginTargetBase
{
	public MayaLiveLinkPlugin2016Target(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "MayaLiveLinkPlugin2016";
	}
}