// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class UnrealUSDWrapper : ModuleRules
	{
		public UnrealUSDWrapper(ReadOnlyTargetRules Target) : base(Target)
		{
			bEnableExceptions = true;

			PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
			PrivatePCHHeaderFile = "Public/UnrealUSDWrapper.h";

			bUseRTTI = true;

			PublicDependencyModuleNames.AddRange(
				new string[] {
				"Core"
				});

			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
			var PythonSourceTPSDir = Path.Combine(EngineDir, "Source", "ThirdParty", "Python");

			if (Target.WindowsPlatform.Compiler != WindowsCompiler.Clang && Target.WindowsPlatform.StaticAnalyzer == WindowsStaticAnalyzer.None)
			{
				PublicDefinitions.Add("USE_USD_SDK=1");

				var USDLibsDir = Path.Combine(ModuleDirectory, "../ThirdParty/USD/lib/");
				var USDLibs = new string[]
				{
					"arch",
					"gf",
					"tf",
					"kind",
					"sdf",
					"plug",
					"js",
					"work",
					"vt",
					"pcp",
					"usd",
					"usdShade",
					"usdGeom",
					"usdLux"
				};

				PrivateIncludePaths.AddRange(
					new string[] {
					ModuleDirectory + "/../ThirdParty/USD/include",
					PythonSourceTPSDir + "/Win64/include",
					});

				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					foreach (string UsdLib in USDLibs)
					{
						PublicAdditionalLibraries.Add(Path.Combine(USDLibsDir, UsdLib + ".lib"));
					}

					PublicLibraryPaths.Add(Path.Combine(EngineDir, "Source/ThirdParty/Python/Win64/libs"));
					PublicLibraryPaths.Add(USDLibsDir);
				}
				else
				{
					System.Console.WriteLine("UnrealUSDWrapper does not supported this platform");
				}
			}
			else
			{
				PublicDefinitions.Add("USE_USD_SDK=0");
			}

			PrivateIncludePaths.Add("UnrealUSDWrapper/Private");
        }
	}
}
