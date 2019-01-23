// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class GLTFImporter : ModuleRules
    {
        public GLTFImporter(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicIncludePaths.AddRange(
                new string[] {
                }
                );

            PrivateIncludePaths.AddRange(
                new string[] {
                }
                );

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "UnrealEd",
                    "MeshDescription",
                    "MeshUtilities",
                    "MessageLog",
                    "Json",
                    "MaterialEditor",
                    "Slate",
                    "SlateCore",
                    "Mainframe",
                    "InputCore",
                    "EditorStyle",
                }
                );

            DynamicallyLoadedModuleNames.AddRange(
                new string[]
                {
                }
                );
        }
    }
}
