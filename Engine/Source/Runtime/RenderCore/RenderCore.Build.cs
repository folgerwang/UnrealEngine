// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RenderCore : ModuleRules
{
	public RenderCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] { "RHI" });
		
        if (Target.bBuildEditor == true)
        {
            PrivateDependencyModuleNames.Add("TargetPlatform");
        }
		else
        {

            PrivateIncludePathModuleNames.AddRange(new string[] { "TargetPlatform" });

        }

        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Projects", "RHI", "ApplicationCore" });

        PrivateIncludePathModuleNames.AddRange(new string[] { "DerivedDataCache" });
    }
}
