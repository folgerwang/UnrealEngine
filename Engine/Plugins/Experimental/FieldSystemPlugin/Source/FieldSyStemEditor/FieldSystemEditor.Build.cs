// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class FieldSystemEditor : ModuleRules
	{
        public FieldSystemEditor(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePaths.Add(ModuleDirectory + "/Public");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Slate",
                    "SlateCore",
                    "Engine",
                    "UnrealEd",
                    "PropertyEditor",
                    "RenderCore",
                    "RHI",
                    "FieldSystemCore",
                    "FieldSystemEngine",
                    "RawMesh",
                    "AssetTools",
                    "AssetRegistry"
                }
				);
		}
	}
}
