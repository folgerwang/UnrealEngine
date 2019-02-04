// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class MikkTSpace : ModuleRules
{
	public MikkTSpace(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string MikkTSpacePath = Target.UEThirdPartySourceDirectory + "MikkTSpace/";

		PublicIncludePaths.Add(MikkTSpacePath + "inc/");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(MikkTSpacePath + "lib/Win64/VS2017/MikkTSpace.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win32)
		{
			PublicAdditionalLibraries.Add(MikkTSpacePath + "lib/Win32/VS2017/MikkTSpace.lib");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(MikkTSpacePath + "/lib/Linux/" + Target.Architecture + "/libMikkTSpace.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(MikkTSpacePath + "/lib/Mac/libMikkTSpace.a");
		}
	}
}
