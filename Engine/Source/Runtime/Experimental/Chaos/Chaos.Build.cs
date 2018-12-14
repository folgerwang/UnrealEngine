// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class Chaos : ModuleRules
    {
        public Chaos(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicIncludePaths.Add("Runtime/Experimental/Chaos/Public");

            PublicDependencyModuleNames.AddRange(
                new string[] {
                "Core",
                "CoreUObject",
				"FieldSystemCore"
                }
            );

            PublicDefinitions.Add("COMPILE_WITHOUT_UNREAL_SUPPORT=0");

            SetupModulePhysicsSupport(Target);
        }
    }
}
