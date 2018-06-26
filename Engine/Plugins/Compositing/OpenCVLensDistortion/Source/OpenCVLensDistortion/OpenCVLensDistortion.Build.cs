// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenCVLensDistortion : ModuleRules
	{
		public OpenCVLensDistortion(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
					// ... add public include paths required here ...
				}
			);

			PrivateIncludePaths.AddRange(
				new string[] {
					// ... add other private include paths required here ...
				}
			);
            
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					
					// ... add other public dependencies that you statically link with here ...
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"OpenCVHelper",
					"OpenCV",
					"RenderCore",
					"RHI",
					"ShaderCore",
					// ... add private dependencies that you statically link with here ...
				}
			);
		}
	}
}
