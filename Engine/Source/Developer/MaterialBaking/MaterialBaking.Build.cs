// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
public class MaterialBaking : ModuleRules
{
	public MaterialBaking(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string [] {
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
                "RHI",                
                "UnrealEd",
                "MainFrame",
                "SlateCore",
                "Slate",
                "InputCore",
                "PropertyEditor",
                "EditorStyle",
                "Renderer",
                "MeshDescription",
            }
		);
    }
}
