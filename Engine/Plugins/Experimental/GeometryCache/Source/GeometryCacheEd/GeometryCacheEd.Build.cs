// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryCacheEd : ModuleRules
{
	public GeometryCacheEd(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "InputCore",
                "RenderCore",
                "ShaderCore",
                "RHI",		
                "UnrealEd",
				"AssetTools",
                "GeometryCache"
			}
		);
	}
}
