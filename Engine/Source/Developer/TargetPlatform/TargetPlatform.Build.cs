// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class TargetPlatform : ModuleRules
{
	public TargetPlatform(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("SlateCore");
		PrivateDependencyModuleNames.Add("Slate");
		PrivateDependencyModuleNames.Add("EditorStyle");
		PublicDependencyModuleNames.Add("AudioPlatformConfiguration");
		PublicDependencyModuleNames.Add("DesktopPlatform");
		PublicDependencyModuleNames.Add("LauncherPlatform");

		PrivateIncludePathModuleNames.Add("Engine");

		// no need for all these modules if the program doesn't want developer tools at all (like UnrealFileServer)
		if (!Target.bBuildRequiresCookedData && Target.bBuildDeveloperTools)
		{
			// these are needed by multiple platform specific target platforms, so we make sure they are built with the base editor
			DynamicallyLoadedModuleNames.Add("ShaderPreprocessor");
			DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
			DynamicallyLoadedModuleNames.Add("ImageWrapper");

			if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32))
			{
				DynamicallyLoadedModuleNames.Add("TextureFormatIntelISPCTexComp");
			}

			if (Target.Platform == UnrealTargetPlatform.Win32 ||
				Target.Platform == UnrealTargetPlatform.Win64)
			{

				// these are needed by multiple platform specific target platforms, so we make sure they are built with the base editor
				DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
				DynamicallyLoadedModuleNames.Add("MetalShaderFormat");

				DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
				DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
				DynamicallyLoadedModuleNames.Add("TextureFormatASTC");

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatADPCM"); // For IOS cooking
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("HTML5TargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacNoEditorTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacServerTargetPlatform");
					DynamicallyLoadedModuleNames.Add("MacClientTargetPlatform");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
				DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
				DynamicallyLoadedModuleNames.Add("TextureFormatASTC");

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
					DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					DynamicallyLoadedModuleNames.Add("HTML5TargetPlatform");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				DynamicallyLoadedModuleNames.Add("TextureFormatDXT");
				DynamicallyLoadedModuleNames.Add("TextureFormatPVR");
				DynamicallyLoadedModuleNames.Add("TextureFormatASTC");

				DynamicallyLoadedModuleNames.Add("TextureFormatUncompressed");

				if (Target.bCompileAgainstEngine)
				{
					DynamicallyLoadedModuleNames.Add("AudioFormatOgg");
					DynamicallyLoadedModuleNames.Add("AudioFormatOpus");
					DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
				}

				if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
				{
					DynamicallyLoadedModuleNames.Add("AndroidTargetPlatform");
					DynamicallyLoadedModuleNames.Add("HTML5TargetPlatform");
				}
			}
		}

		if (Target.bBuildDeveloperTools == true && Target.bBuildRequiresCookedData && Target.bCompileAgainstEngine && Target.bCompilePhysX)
		{
			DynamicallyLoadedModuleNames.Add("PhysXCooking");
		}
	}
}
