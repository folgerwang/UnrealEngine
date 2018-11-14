// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IntelMetricsDiscovery : ModuleRules
{
	public IntelMetricsDiscovery(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string IntelMetricsDiscoveryPath = Target.UEThirdPartySourceDirectory + "IntelMetricsDiscovery/MetricsDiscoveryHelper/";
		bool bUseDebugBuild = false;

		if ( (Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Win32) )
		{
			string PlatformName = (Target.Platform == UnrealTargetPlatform.Win64) ? "x64" : "x86";
			string BuildType = bUseDebugBuild ? "-md-debug" : "-md-release";

			PublicSystemIncludePaths.Add(IntelMetricsDiscoveryPath + "build/include/metrics_discovery/");

			PublicLibraryPaths.Add(IntelMetricsDiscoveryPath + "build/lib/" + PlatformName + BuildType);

			PublicAdditionalLibraries.Add("metrics_discovery_helper.lib");

            PublicDefinitions.Add("INTEL_METRICSDISCOVERY=1");
        }
		else
        {
            PublicDefinitions.Add("INTEL_METRICSDISCOVERY=0");
        }
	}
}