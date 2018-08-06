// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GeometryCollectionComponentEditor : ModuleRules
	{
        public GeometryCollectionComponentEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("GeometryCollectionComponentEditor/Private");
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
                    "ShaderCore",
                    "RHI",
                    "GeometryCollectionComponent",
                    "RawMesh",
                    "AssetTools",
                    "AssetRegistry"
                }
				);
		}
	}
}
