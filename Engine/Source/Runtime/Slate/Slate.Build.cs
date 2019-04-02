// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Slate : ModuleRules
{
	public Slate(ReadOnlyTargetRules Target) : base(Target)
	{
		SharedPCHHeaderFile = "Public/SlateSharedPCH.h";

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"InputCore",
				"Json",
				"SlateCore",
				"ImageWrapper"
			});

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/Slate/Private",
				"Runtime/Slate/Private/Framework",
				"Runtime/Slate/Private/Framework/Application",
				"Runtime/Slate/Private/Framework/Commands",
				"Runtime/Slate/Private/Framework/Docking",
				"Runtime/Slate/Private/Framework/Layout",
				"Runtime/Slate/Private/Framework/MultiBox",
				"Runtime/Slate/Private/Framework/Notifications",
				"Runtime/Slate/Private/Framework/Styling",
				"Runtime/Slate/Private/Framework/Text",
				"Runtime/Slate/Private/Framework/Text/IOS",
				"Runtime/Slate/Private/Widgets/Colors",
				"Runtime/Slate/Private/Widgets/Docking",
				"Runtime/Slate/Private/Widgets/Images",
				"Runtime/Slate/Private/Widgets/Input",
				"Runtime/Slate/Private/Widgets/Layout",
				"Runtime/Slate/Private/Widgets/Notifications",
				"Runtime/Slate/Private/Widgets/Text",
				"Runtime/Slate/Private/Widgets/Views",

				"Runtime/Toolbox/Public/"
			});

		if ((Target.Platform == UnrealTargetPlatform.Win64) ||
			(Target.Platform == UnrealTargetPlatform.Win32))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "XInput");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}

		// Add slate runtime dependencies
		if (Target.bUsesSlate)
		{
			RuntimeDependencies.Add("$(EngineDir)/Content/Slate/...", StagedFileType.UFS);
			RuntimeDependencies.Add("$(EngineDir)/Content/SlateDebug/...", StagedFileType.DebugNonUFS);

			if (Target.ProjectFile != null)
			{
				RuntimeDependencies.Add("$(ProjectDir)/Content/Slate/...", StagedFileType.UFS);
				RuntimeDependencies.Add("$(ProjectDir)/Content/SlateDebug/...", StagedFileType.DebugNonUFS);
			}
		}
	}
}
