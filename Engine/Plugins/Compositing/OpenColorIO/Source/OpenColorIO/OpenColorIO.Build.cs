// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	using System.IO;

	public class OpenColorIO : ModuleRules
	{
		public OpenColorIO(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"RenderCore",
					"RHI",
					"RenderCore",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
				});

			PrivateIncludePaths.AddRange(
				new string[] {
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
				});

			if (Target.bBuildEditor == true)
			{
				if (Target.Platform == UnrealTargetPlatform.Win64 ||
						Target.Platform == UnrealTargetPlatform.Win32)
				{
					//There are some dynamic_cast in OCIO library.
					bUseRTTI = true;
				}

				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"DerivedDataCache",
						"OpenColorIOLib",
						"TargetPlatform",
						"UnrealEd"
					});
			}
		}
	}
}
