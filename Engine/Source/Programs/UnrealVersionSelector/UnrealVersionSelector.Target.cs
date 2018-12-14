// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class UnrealVersionSelectorTarget : TargetRules
{
	public UnrealVersionSelectorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "UnrealVersionSelector";
		
		bBuildDeveloperTools = false;
		bUseMallocProfiler = false;

		bool bUsingSlate = (Target.Platform == UnrealTargetPlatform.Linux);

		if (bUsingSlate)
		{
			ExtraModuleNames.Add("EditorStyle");
		}

		bCompileICU = false;
		// Editor-only data, however, is needed
		bBuildWithEditorOnlyData = bUsingSlate;

		// Currently this app is not linking against the engine, so we'll compile out references from Core to the rest of the engine
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = bUsingSlate;
	}
}
