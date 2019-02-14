// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class D3D12RHI : ModuleRules
{
	public D3D12RHI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("Runtime/D3D12RHI/Private");
        PrivateIncludePaths.Add("../Shaders/Private/RayTracing");

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"RHI",
				"RenderCore",
				"UtilityShaders",
			    }
			);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "TaskGraph" });
		}

		///////////////////////////////////////////////////////////////
        // Platform specific defines
        ///////////////////////////////////////////////////////////////

        if (Target.Platform != UnrealTargetPlatform.Win32 && Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.XboxOne)
        {
            PrecompileForTargets = PrecompileTargetsType.None;
        }

        if (Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Win32)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "DX11");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "NVAPI");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "AMD_AGS");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "IntelMetricsDiscovery");
		}

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string NVAftermathPath = Target.UEThirdPartySourceDirectory + "NVIDIA/NVaftermath/";
            PublicSystemIncludePaths.Add(NVAftermathPath);

            string NVAftermathLibPath = NVAftermathPath + "amd64/";
            PublicLibraryPaths.Add(NVAftermathLibPath);
            PublicAdditionalLibraries.Add("GFSDK_Aftermath_Lib.x64.lib");

            string AftermathDllName = "GFSDK_Aftermath_Lib.x64.dll";
            string nvDLLPath = "$(EngineDir)/Binaries/ThirdParty/NVIDIA/NVaftermath/Win64/" + AftermathDllName;
            PublicDelayLoadDLLs.Add(AftermathDllName);
            RuntimeDependencies.Add(nvDLLPath);

            PublicDefinitions.Add("NV_AFTERMATH=1");
        }
        else
        {
            PublicDefinitions.Add("NV_AFTERMATH=0");
        }
    }
}
