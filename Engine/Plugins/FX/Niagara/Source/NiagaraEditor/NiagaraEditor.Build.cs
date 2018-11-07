// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NiagaraEditor : ModuleRules
{
	public NiagaraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
			"NiagaraEditor/Private",
			"NiagaraEditor/Private/Toolkits",
			"NiagaraEditor/Private/Widgets",
			"NiagaraEditor/Private/Sequencer/NiagaraSequence",
			"NiagaraEditor/Private/ViewModels",
			"NiagaraEditor/Private/TypeEditorUtilities"
		});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
                "RHI",
                "Core", 
				"CoreUObject", 
				"CurveEditor",
				"ApplicationCore",
                "InputCore",
				"RenderCore",
				"Slate", 
				"SlateCore",
				"Kismet",
                "EditorStyle",
				"UnrealEd", 
				"VectorVM",
                "NiagaraCore",
                "Niagara",
                "NiagaraShader",
                "MovieScene",
				"Sequencer",
				"TimeManagement",
				"PropertyEditor",
				"GraphEditor",
                "ShaderFormatVectorVM",
                "TargetPlatform",
                "AppFramework",
				"MovieSceneTools",
                "MovieSceneTracks",
                "AdvancedPreviewScene",
				"Projects",
                "MainFrame",
			}
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Engine",
				"Messaging",
				"LevelEditor",
				"AssetTools",
				"ContentBrowser",
                "DerivedDataCache",
                "ShaderCore",
            }
        );

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "NiagaraCore",
                "NiagaraShader",
                "Engine",
                "NiagaraCore",
                "Niagara",
                "UnrealEd",
            }
        );

        PublicIncludePathModuleNames.AddRange(
            new string[] {
				"Engine",
				"Niagara"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "WorkspaceMenuStructure",
                }
            );
	}
}
