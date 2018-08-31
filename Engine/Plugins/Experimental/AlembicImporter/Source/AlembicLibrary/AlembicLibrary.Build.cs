// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AlembicLibrary : ModuleRules
{
    public AlembicLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
                "UnrealEd",
                "GeometryCache",
                "AlembicLib",
                "MeshUtilities",
                "MaterialUtilities",
                "PropertyEditor",
                "SlateCore",
                "Slate",
                "EditorStyle",
                "Eigen",
                "RenderCore",
                "RHI"
			}
		);

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "UnrealEd",
                "MeshDescription",
                "MeshDescriptionOperations",
            }
        );

    }
}
