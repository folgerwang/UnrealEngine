// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MagicLeapMediaCodec : ModuleRules
	{
		public MagicLeapMediaCodec(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));
			string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media"
			});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Engine",
					"RenderCore",
					"MediaUtils",
					"OpenGLDrv",
					"LuminRuntimeSettings",
					"MLSDK",
                    "MagicLeap",
                    "MagicLeapHelperVulkan",
            });

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media"
			});

			PrivateIncludePaths.AddRange(
				new string[] {
                    "MagicLeapMedia/Private",
					Path.Combine(EngineDir, "Source/Runtime/OpenGLDrv/Private")
            });

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
		}
	}
}
