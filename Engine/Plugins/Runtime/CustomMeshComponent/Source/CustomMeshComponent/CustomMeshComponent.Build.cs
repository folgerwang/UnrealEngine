// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CustomMeshComponent : ModuleRules
	{
        public CustomMeshComponent(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("CustomMeshComponent/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "RHI"
				}
				);
		}
	}
}
