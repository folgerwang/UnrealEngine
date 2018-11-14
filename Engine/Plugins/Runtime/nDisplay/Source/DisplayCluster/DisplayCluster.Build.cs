// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayCluster : ModuleRules
{
	private string ModulePath
	{
		get
		{
			//return Path.GetDirectoryName(RulesCompiler.GetModuleFilename(this.GetType().Name));
			string ModuleFilename = UnrealBuildTool.RulesCompiler.GetFileNameFromType(GetType());
			string ModuleBaseDirectory = Path.GetDirectoryName(ModuleFilename);
			return ModuleBaseDirectory;
		}
	}

	private string ThirdPartyPath
	{
		get
		{
			return Path.GetFullPath(Path.Combine(ModulePath, "../../ThirdParty/"));
		}
	}

	public DisplayCluster(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[] {
				"DisplayCluster/Private",
				"../../../../../Engine/Source/Runtime/Renderer/Private",
				"../../../../../Engine/Source/Runtime/Windows/D3D11RHI/Private",
				"../../../../../Engine/Source/Runtime/Windows/D3D11RHI/Private/Windows",
				"../../../../../Engine/Source/Runtime/D3D12RHI/Private",
				"../../../../../Engine/Source/Runtime/D3D12RHI/Private/Windows"
			});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"D3D11RHI",
				"D3D12RHI",
				"Engine",
				"HeadMountedDisplay",
				"InputCore",
				"Networking",
				"OpenGLDrv",
				"RHI",
				"RenderCore",
				"Slate",
				"SlateCore",
				"Sockets"
			});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		PublicAdditionalLibraries.Add("opengl32.lib");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAftermath");
        AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");

        // vrpn
        AddDependencyVrpn(ROTargetRules);
	}

	public bool AddDependencyVrpn(ReadOnlyTargetRules ROTargetRules)
	{
		if ((ROTargetRules.Platform == UnrealTargetPlatform.Win64) || (ROTargetRules.Platform == UnrealTargetPlatform.Win32))
		{
			string PlatformString = (ROTargetRules.Platform == UnrealTargetPlatform.Win64) ? "x64" : "x86";
			string LibrariesPath = Path.Combine(ThirdPartyPath, "VRPN", "Lib/" + PlatformString);

			//@todo: There are also debug versions: vrpnd.lib and quatd.lib
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "vrpn.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibrariesPath, "quat.lib"));

			PublicIncludePaths.Add(Path.Combine(ThirdPartyPath, "VRPN", "Include"));

			return true;
		}

		return false;
	}
}
