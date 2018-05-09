// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SequencerScripting : ModuleRules
{
	public SequencerScripting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"SequencerScripting/Private",
				"SequencerScripting/Private/ExtensionLibraries",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Kismet",
				"MovieScene",
				"PythonScriptPlugin",
				"Slate",
				"SlateCore",
				"TimeManagement",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			}
		);
	}
}
