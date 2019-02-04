// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ShaderFormatD3D : ModuleRules
{
	public ShaderFormatD3D(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("TargetPlatform");
		PrivateIncludePathModuleNames.Add("D3D11RHI");

        PrivateIncludePaths.Add("../Shaders/Private/RayTracing");

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"RenderCore",
				"ShaderPreprocessor",
				"ShaderCompilerCommon",
			}
			);

		//DXC
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/dxil.dll");
            RuntimeDependencies.Add("$(EngineDir)/Binaries/Win64/dxcompiler.dll");
        }
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
	}
}
