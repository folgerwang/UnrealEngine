// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class elftoolchain : ModuleRules
{
	public elftoolchain(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "elftoolchain/include/" + Target.Architecture);

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
			string LibDir = Target.UEThirdPartySourceDirectory + "elftoolchain/lib/Linux/" + Target.Architecture;
			PublicAdditionalLibraries.Add(LibDir + "/libelf.a");
			PublicAdditionalLibraries.Add(LibDir + "/libdwarf.a");
        }
	}
}
