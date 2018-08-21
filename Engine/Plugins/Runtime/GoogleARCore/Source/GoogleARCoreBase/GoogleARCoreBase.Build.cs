// Copyright 2017 Google Inc.


using System.IO;


namespace UnrealBuildTool.Rules
{
	public class GoogleARCoreBase : ModuleRules
	{
		public GoogleARCoreBase(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[]
				{
					"GoogleARCoreBase/Private",
				}
			);
			PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"HeadMountedDisplay",
						"AugmentedReality",
					}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"EngineSettings",
					"Slate",
					"SlateCore",
					"RHI",
					"RenderCore",
					"ShaderCore",
					"AndroidPermission",
					"GoogleARCoreRendering",
					"GoogleARCoreSDK",
					"OpenGL",
					"UElibPNG",
					"zlib"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Settings" // For editor settings panel.
				}
			);

			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				// Additional dependencies on android...
				PrivateDependencyModuleNames.Add("Launch");

				// Register Plugin Language
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(ModuleDirectory, "GoogleARCoreBase_APL.xml"));
			}

			if (Target.bBuildEditor)
			{
				string ExecName = "";
				if (Target.Platform == UnrealTargetPlatform.Win64)
				{
					ExecName = "Windows/arcoreimg.exe";
				}
				else if (Target.Platform == UnrealTargetPlatform.Linux)
				{
					ExecName = "Linux/arcoreimg";
				}
				else if (Target.Platform == UnrealTargetPlatform.Mac)
				{
					ExecName = "Mac/ptdbtool_macos_lipobin";
				}
				
				if (ExecName.Length > 0)
				{
					RuntimeDependencies.Add("$(EngineDir)/Plugins/Runtime/GoogleARCore/Binaries/ThirdParty/Google/ARCoreImg/" + ExecName);
				}
			}
			
			bFasterWithoutUnity = false;
        }
    }
}
