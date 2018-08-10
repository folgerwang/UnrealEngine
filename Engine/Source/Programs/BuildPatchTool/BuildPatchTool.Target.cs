// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

[SupportedPlatforms(UnrealTargetPlatform.Win32, UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac, UnrealTargetPlatform.Linux)]
[SupportedConfigurations(UnrealTargetConfiguration.Debug, UnrealTargetConfiguration.Development, UnrealTargetConfiguration.Shipping)]
public class BuildPatchToolTarget : TargetRules
{
	public BuildPatchToolTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "BuildPatchTool";
		bOutputPubliclyDistributable = true;
		UndecoratedConfiguration = UnrealTargetConfiguration.Shipping;

		bBuildEditor = false;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;
		if(Target.Configuration == UnrealTargetConfiguration.Shipping)
		{
			// AutomationController is referenced by BuildPatchTool.build.cs, and references a ton of editor things that it probably shouldn't. Need ApplicationCore for this.
			bCompileAgainstApplicationCore = false;
		}
		bCompileLeanAndMeanUE = true;
		bUseLoggingInShipping = true;
		bUseChecksInShipping = true;
		bIsBuildingConsoleApplication = true;
		bHasExports = false;
		bWithServerCode = false;
	}
}
