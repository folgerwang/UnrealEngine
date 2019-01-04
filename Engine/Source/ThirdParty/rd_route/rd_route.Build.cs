// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class rd_route : ModuleRules
{
	public rd_route(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string RDRoutePath = Target.UEThirdPartySourceDirectory + "rd_route/";

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicSystemIncludePaths.Add(RDRoutePath + "src");
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(RDRoutePath + "lib/Mac/librd_routed.a");
			}
			else
			{
				PublicAdditionalLibraries.Add(RDRoutePath + "lib/Mac/librd_route.a");
			}
		}
    }
}
