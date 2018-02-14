// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OpenSubdiv : ModuleRules
{
	public OpenSubdiv(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// Compile and link with OpenSubDiv
        string OpenSubdivPath = Target.UEThirdPartySourceDirectory + "OpenSubdiv/3.2.0";

		PublicIncludePaths.Add( OpenSubdivPath + "/opensubdiv" );

		// @todo mesheditor subdiv: Support other platforms, 32-bit Windows, and older/newer compiler toolchains
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            string LibFolder = "";
            {
                switch (Target.WindowsPlatform.Compiler)
                {
                    case WindowsCompiler.VisualStudio2017:
                    case WindowsCompiler.VisualStudio2015:
						LibFolder = "/libVS2015";
						break;
                }
            }

            if (LibFolder != "")
            {
                bool bDebug = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT);
                string ConfigFolder = bDebug ? "/Debug" : "/RelWithDebInfo";

                PublicLibraryPaths.Add(OpenSubdivPath + LibFolder + ConfigFolder);

                PublicAdditionalLibraries.Add("osdCPU.lib");
            }
        }
	}
}