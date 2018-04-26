// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PhysXVehicles : ModuleRules
	{
        public PhysXVehicles(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "ShaderCore",
                    "AnimGraphRuntime",
                    "RHI",
                    "PhysXVehicleLib"
				}
				);

            SetupModulePhysXAPEXSupport(Target);
        }
    }
}
