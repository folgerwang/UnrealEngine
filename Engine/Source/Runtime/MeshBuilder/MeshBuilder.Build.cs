// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MeshBuilder : ModuleRules
	{
		public MeshBuilder(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateIncludePaths.Add("Runtime/MeshBuilder/Private");
            PublicIncludePaths.Add("Runtime/MeshBuilder/Public");

            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "MeshDescription",
                }
			);
		}
	}
}
